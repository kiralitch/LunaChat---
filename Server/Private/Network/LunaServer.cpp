#include "Network/LunaServer.h"

#include <QDateTime>
#include <QHostAddress>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTimer>
#include <QTcpSocket>
#include <QtAlgorithms>
#include <QtDebug>

#include "Handlers/MessageDispatcher.h"
#include "LunaDb/DbManager.h"
#include "LunaProtocol/LunaConstants.h"
#include "LunaProtocol/MessageTypes.h"
#include "Network/ClientSession.h"

LunaServer::LunaServer(QObject *parent)
    : QObject(parent), m_server(new QTcpServer(this)), m_dispatcher(new MessageDispatcher(this))
{
    connect(m_server, &QTcpServer::newConnection, this, &LunaServer::onNewConnection);
    connect(m_dispatcher, &MessageDispatcher::userListChanged, this, &LunaServer::broadcastUserList);
    connect(m_dispatcher, &MessageDispatcher::userLoggedIn, this, &LunaServer::onUserLoggedIn);
    connect(m_dispatcher, &MessageDispatcher::frameToGroup, this, &LunaServer::onFrameToGroup);
    connect(m_dispatcher, &MessageDispatcher::frameToUser, this, &LunaServer::onFrameToUser);

    // 文件传输依赖在线判定：离线接收方直接拒绝，不写离线队列
    m_dispatcher->setOnlineChecker([this](quint64 userId) {
        return findSessionByUserId(userId) != nullptr;
    });

    // 定时器：每 5s 扫一遍会话，心跳超时的直接踢（不依赖用户在线状态）
    m_sweepTimer = new QTimer(this);
    m_sweepTimer->setInterval(5000);
    connect(m_sweepTimer, &QTimer::timeout, this, &LunaServer::sweepIdleSessions);
}

LunaServer::~LunaServer()
{
    qDeleteAll(m_sessions);
}

bool LunaServer::start(quint16 port)
{
    if (!m_server->listen(QHostAddress::Any, port)) {
        qCritical() << "[server] listen failed:" << m_server->errorString();
        return false;
    }
    m_sweepTimer->start();

    // 活动状态机：每分钟按 signup_deadline/start_time/end_time 推进，不依赖用户在线
    auto *activityTimer = new QTimer(this);
    activityTimer->setInterval(60000);
    connect(activityTimer, &QTimer::timeout, this, []() {
        const int changed = DbManager::instance().refreshActivityStatuses();
        if (changed > 0)
            qInfo() << "[activity] status advanced for" << changed << "activity(ies)";
    });
    activityTimer->start();
    qInfo() << "[server] listening on 0.0.0.0:" << m_server->serverPort()
            << "| heartbeat" << LunaConst::kHeartbeatIntervalMs << "ms / timeout"
            << LunaConst::kHeartbeatTimeoutMs << "ms";
    return true;
}

void LunaServer::onNewConnection()
{
    while (m_server->hasPendingConnections()) {
        QTcpSocket *socket = m_server->nextPendingConnection();
        ClientSession *session = new ClientSession(socket, this);
        m_sessions.append(session);

        connect(session, &ClientSession::frameReceived, this, &LunaServer::onSessionFrame);
        connect(session, &ClientSession::closed, this, &LunaServer::onSessionClosed);

        qInfo().noquote() << QStringLiteral("[server] client connected %1:%2")
                                 .arg(session->peerAddress()).arg(session->peerPort());
    }
}

void LunaServer::onSessionFrame(ClientSession *session, const QJsonObject &frame)
{
    const QString type = frame.value(QStringLiteral("type")).toString();

    // 心跳在所有业务之下，鉴权之前也要能回
    if (type == MsgType::kHeartbeat) {
        QJsonObject reply;
        reply.insert(QStringLiteral("type"), MsgType::kHeartbeatResp);
        reply.insert(QStringLiteral("ts"), static_cast<double>(QDateTime::currentMSecsSinceEpoch()));
        reply.insert(QStringLiteral("data"), QJsonObject());
        session->sendFrame(reply);
        return;
    }

    if (m_dispatcher->dispatch(session, frame))
        return;

    // 没登记的 type（或已登记但本阶段没实现）统一回 error，避免客户端干等
    QJsonObject data;
    data.insert(QStringLiteral("code"), 1001);
    data.insert(QStringLiteral("msg"), QStringLiteral("unsupported message type: ") + type);

    QJsonObject reply;
    reply.insert(QStringLiteral("type"), MsgType::kError);
    reply.insert(QStringLiteral("ts"), static_cast<double>(QDateTime::currentMSecsSinceEpoch()));
    reply.insert(QStringLiteral("data"), data);
    session->sendFrame(reply);
    qWarning().noquote() << "[server] unsupported type:" << type;
}

void LunaServer::onUserLoggedIn(ClientSession *session)
{
    // 同一账号重复登录：把旧会话踢下线，保证 user_list 里一个账号只有一条
    for (ClientSession *other : m_sessions) {
        if (other != session && other->userId() != 0 && other->userId() == session->userId()) {
            qWarning().noquote() << QStringLiteral("[server] duplicate login, kick %1")
                                        .arg(other->peerPort());
            QJsonObject data;
            data.insert(QStringLiteral("reason"), QStringLiteral("账号在其他地方登录"));
            QJsonObject kick;
            kick.insert(QStringLiteral("type"), MsgType::kKick);
            kick.insert(QStringLiteral("ts"), static_cast<double>(QDateTime::currentMSecsSinceEpoch()));
            kick.insert(QStringLiteral("data"), data);
            other->sendFrame(kick);
            other->closeSession(QStringLiteral("duplicate login"));
        }
    }
    // 广播统一由 userListChanged 触发，这里不再重复播一次；管理界面靠这个信号刷新
    emit onlineChanged();
}

bool LunaServer::kickUser(quint64 userId, const QString &reason)
{
    ClientSession *target = findSessionByUserId(userId);
    if (!target)
        return false;

    QJsonObject data;
    data.insert(QStringLiteral("reason"), reason);

    QJsonObject kick;
    kick.insert(QStringLiteral("type"), MsgType::kKick);
    kick.insert(QStringLiteral("ts"), static_cast<double>(QDateTime::currentMSecsSinceEpoch()));
    kick.insert(QStringLiteral("data"), data);
    target->sendFrame(kick);

    qWarning().noquote() << QStringLiteral("[server] admin kick: %1 (%2)")
                                .arg(target->nickname()).arg(reason);
    target->closeSession(QStringLiteral("admin kick"));
    return true;
}

void LunaServer::onFrameToGroup(qint64 groupId, const QJsonObject &frame)
{
    // 只投给该群成员中在线的会话；离线群成员不补发（群聊离线补发留给阶段5）
    const QList<quint64> members = DbManager::instance().groupMemberIds(groupId);
    for (ClientSession *session : m_sessions) {
        if (session->userId() != 0 && members.contains(session->userId()))
            session->sendFrame(frame);
    }
}

void LunaServer::onFrameToUser(quint64 userId, const QJsonObject &frame)
{
    ClientSession *target = findSessionByUserId(userId);
    if (target) {
        target->sendFrame(frame);
        return;
    }

    // 只有聊天消息进离线队列；点赞/活动通知这类到点就过期的帧直接丢弃
    if (frame.value(QStringLiteral("type")).toString() != MsgType::kChatBroadcast) {
        qInfo().noquote() << QStringLiteral("[server] drop non-chat frame for offline user %1").arg(userId);
        return;
    }

    // 对方不在线：整帧 JSON 存 offline_message，阶段5 上线时按顺序补推
    const QString payload = QString::fromUtf8(QJsonDocument(frame).toJson(QJsonDocument::Compact));
    if (DbManager::instance().storeOfflineMessage(userId, payload))
        qInfo().noquote() << QStringLiteral("[server] user %1 offline, message queued").arg(userId);
}

void LunaServer::sweepIdleSessions()
{
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    const QList<ClientSession *> snapshot = m_sessions;   // 断开可能触发 closed，先拷贝再遍历

    for (ClientSession *session : snapshot) {
        if (now - session->lastActiveMs() > LunaConst::kHeartbeatTimeoutMs) {
            qWarning().noquote() << QStringLiteral("[server] heartbeat timeout, kick %1")
                                        .arg(session->peerPort());
            session->closeSession(QStringLiteral("heartbeat timeout"));
        }
    }
}

void LunaServer::onSessionClosed(ClientSession *session)
{
    if (!m_sessions.removeOne(session))
        return;

    // 掉线要把数据库里的状态写回 offline，活动/朋友圈不受影响
    // 掉线先退出匹配队列/结束匹配会话，避免对端干等
    m_dispatcher->onUserOffline(session->userId());

    // 同账号可能已有新会话接管（被顶号），只有确实没有在线会话时才写 offline
    const quint64 userId = session->userId();
    if (userId != 0 && findSessionByUserId(userId) == nullptr) {
        DbManager::instance().updateUserStatus(userId, QStringLiteral("offline"));
        qInfo().noquote() << QStringLiteral("[server] %1 offline, online=%2")
                                 .arg(session->nickname()).arg(m_sessions.size());
    }

    broadcastUserList();
    emit onlineChanged();
    session->deleteLater();
}

void LunaServer::broadcastUserList()
{
    QJsonArray users;
    for (ClientSession *session : m_sessions) {
        if (session->userId() == 0)
            continue;      // 未登录的连接不对外展示
        QJsonObject item;
        item.insert(QStringLiteral("userId"), static_cast<double>(session->userId()));
        item.insert(QStringLiteral("nickname"), session->nickname());
        item.insert(QStringLiteral("status"), QStringLiteral("online"));
        users.append(item);
    }

    QJsonObject data;
    data.insert(QStringLiteral("users"), users);

    QJsonObject frame;
    frame.insert(QStringLiteral("type"), MsgType::kUserList);
    frame.insert(QStringLiteral("ts"), static_cast<double>(QDateTime::currentMSecsSinceEpoch()));
    frame.insert(QStringLiteral("data"), data);

    // 只发给已登录会话：未登录连接不该看到在线名单
    for (ClientSession *session : m_sessions) {
        if (session->userId() != 0)
            session->sendFrame(frame);
    }
}

ClientSession *LunaServer::findSessionByUserId(quint64 userId) const
{
    for (ClientSession *session : m_sessions) {
        if (session->userId() == userId)
            return session;
    }
    return nullptr;
}