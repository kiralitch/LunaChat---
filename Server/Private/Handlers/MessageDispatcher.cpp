#include "Handlers/MessageDispatcher.h"
#include "Handlers/ActivityHandler.h"
#include "Handlers/ChatHandler.h"
#include "Handlers/FileHandler.h"
#include "Handlers/GroupHandler.h"
#include "Handlers/MatchHandler.h"
#include "Handlers/MomentHandler.h"
#include "Handlers/FriendHandler.h"
#include "Handlers/ProfileHandler.h"
#include "Handlers/VoiceHandler.h"

#include <QDateTime>
#include <QJsonArray>
#include <QtDebug>

#include "LunaDb/DbManager.h"
#include "LunaProtocol/MessageTypes.h"
#include "LunaUtil/Sha256.h"
#include "Network/ClientSession.h"

MessageDispatcher::MessageDispatcher(QObject *parent)
    : QObject(parent), m_chat(new ChatHandler(this)), m_group(new GroupHandler(this)),
      m_file(new FileHandler(this)), m_activity(new ActivityHandler(this)),
      m_moment(new MomentHandler(this)), m_match(new MatchHandler(this)), m_voice(new VoiceHandler(this)), m_profile(new ProfileHandler(this)), m_friend(new FriendHandler(this))
{
    // 转投递意图：聊天层不认识 socket，只发信号
    connect(m_chat, &ChatHandler::frameToGroup, this, &MessageDispatcher::frameToGroup);
    connect(m_chat, &ChatHandler::frameToUser, this, &MessageDispatcher::frameToUser);
    connect(m_file, &FileHandler::frameToUser, this, &MessageDispatcher::frameToUser);
    connect(m_activity, &ActivityHandler::frameToGroup, this, &MessageDispatcher::frameToGroup);
    connect(m_moment, &MomentHandler::frameToUser, this, &MessageDispatcher::frameToUser);
    connect(m_match, &MatchHandler::frameToUser, this, &MessageDispatcher::frameToUser);
    connect(m_voice, &VoiceHandler::frameToUser, this, &MessageDispatcher::frameToUser);
    connect(m_profile, &ProfileHandler::userListChanged, this, &MessageDispatcher::userListChanged);
    connect(m_friend, &FriendHandler::frameToUser, this, &MessageDispatcher::frameToUser);
    connect(m_group, &GroupHandler::frameToUser, this, &MessageDispatcher::frameToUser);
}

void MessageDispatcher::setOnlineChecker(std::function<bool(quint64)> checker)
{
    m_file->setOnlineChecker(checker);
}

void MessageDispatcher::pushFriendList(quint64 userId)
{
    m_friend->pushFriendList(userId);
}

void MessageDispatcher::onUserOffline(quint64 userId)
{
    if (userId != 0)
        m_match->onUserOffline(userId);
}

bool MessageDispatcher::dispatch(ClientSession *session, const QJsonObject &frame)
{
    const QString type = frame.value(QStringLiteral("type")).toString();

    if (type == MsgType::kRegister)
        return handleRegister(session, frame);
    if (type == MsgType::kLogin)
        return handleLogin(session, frame);

    if (m_chat->handle(session, frame))
        return true;

    if (m_group->handle(session, frame))
        return true;

    if (m_file->handle(session, frame))
        return true;

    if (m_activity->handle(session, frame))
        return true;

    if (m_moment->handle(session, frame))
        return true;

    if (m_match->handle(session, frame))
        return true;

    if (m_voice->handle(session, frame))
        return true;

    if (m_profile->handle(session, frame))
        return true;

    if (m_friend->handle(session, frame))
        return true;

    return false;
}

void MessageDispatcher::replyLoginResp(ClientSession *session, bool ok, const QString &msg,
                                       quint64 userId, const QString &nickname)
{
    QJsonObject data;
    data.insert(QStringLiteral("ok"), ok);
    data.insert(QStringLiteral("msg"), msg);
    data.insert(QStringLiteral("userId"), static_cast<double>(userId));
    data.insert(QStringLiteral("nickname"), nickname);

    QJsonObject reply;
    reply.insert(QStringLiteral("type"), MsgType::kLoginResp);
    reply.insert(QStringLiteral("ts"), static_cast<double>(QDateTime::currentMSecsSinceEpoch()));
    reply.insert(QStringLiteral("data"), data);
    session->sendFrame(reply);
}

bool MessageDispatcher::handleRegister(ClientSession *session, const QJsonObject &frame)
{
    const QJsonObject data = frame.value(QStringLiteral("data")).toObject();
    const QString username = data.value(QStringLiteral("username")).toString().trimmed();
    const QString password = data.value(QStringLiteral("password")).toString();
    QString nickname = data.value(QStringLiteral("nickname")).toString().trimmed();

    if (username.isEmpty() || password.isEmpty()) {
        replyLoginResp(session, false, QStringLiteral("用户名和密码不能为空"));
        return true;
    }
    if (nickname.isEmpty())
        nickname = username;

    quint64 newUserId = 0;
    QString error;
    // 密码只在服务端做一次 SHA-256，入库存哈希
    if (!DbManager::instance().registerUser(username, LunaCrypto::sha256Hex(password),
                                            nickname, &newUserId, &error)) {
        replyLoginResp(session, false, error);
        return true;
    }

    qInfo().noquote() << QStringLiteral("[dispatcher] register ok: %1 (id=%2)")
                             .arg(username).arg(newUserId);
    replyLoginResp(session, true, QStringLiteral("注册成功"), newUserId, nickname);
    return true;
}

bool MessageDispatcher::handleLogin(ClientSession *session, const QJsonObject &frame)
{
    if (session->userId() != 0) {
        replyLoginResp(session, false, QStringLiteral("该连接已登录"));
        return true;
    }

    const QJsonObject data = frame.value(QStringLiteral("data")).toObject();
    const QString username = data.value(QStringLiteral("username")).toString().trimmed();
    const QString password = data.value(QStringLiteral("password")).toString();

    if (username.isEmpty() || password.isEmpty()) {
        replyLoginResp(session, false, QStringLiteral("用户名和密码不能为空"));
        return true;
    }

    DbManager::UserInfo info;
    QString error;
    if (!DbManager::instance().authenticate(username, LunaCrypto::sha256Hex(password),
                                            session->peerAddress(), &info, &error)) {
        replyLoginResp(session, false, error);
        return true;
    }

    // 登录成功后把身份绑定到会话上，后续消息都靠这个 userId 鉴权
    session->setUserId(info.userId);
    session->setNickname(info.nickname);

    qInfo().noquote() << QStringLiteral("[dispatcher] login ok: %1 (id=%2) from %3")
                             .arg(info.username).arg(info.userId).arg(session->peerAddress());

    replyLoginResp(session, true, QStringLiteral("登录成功"), info.userId, info.nickname);
    emit userLoggedIn(session);
    emit userListChanged();

    // 登录成功后补推离线消息：整批打包成 offline_msg，客户端按会话归位
    const QJsonArray pending = DbManager::instance().takeOfflineMessages(info.userId);
    if (!pending.isEmpty()) {
        QJsonObject data;
        data.insert(QStringLiteral("messages"), pending);

        QJsonObject frame;
        frame.insert(QStringLiteral("type"), MsgType::kOfflineMsg);
        frame.insert(QStringLiteral("ts"), static_cast<double>(QDateTime::currentMSecsSinceEpoch()));
        frame.insert(QStringLiteral("data"), data);
        session->sendFrame(frame);

        qInfo().noquote() << QStringLiteral("[dispatcher] push %1 offline message(s) to %2")
                                 .arg(pending.size()).arg(info.nickname);
    }
    return true;
}