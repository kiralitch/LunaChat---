#include "Handlers/MatchHandler.h"

#include <QDateTime>
#include <QJsonObject>
#include <QTimer>
#include <QUuid>
#include <QtDebug>

#include "LunaDb/DbManager.h"
#include "LunaProtocol/MessageTypes.h"
#include "Network/ClientSession.h"

namespace {
double nowMs()
{
    return static_cast<double>(QDateTime::currentMSecsSinceEpoch());
}
const int kMatchTimeoutMs = 15000;
}

MatchHandler::MatchHandler(QObject *parent)
    : QObject(parent)
{
}

void MatchHandler::sendError(ClientSession *session, int code, const QString &message)
{
    QJsonObject data;
    data.insert(QStringLiteral("code"), code);
    data.insert(QStringLiteral("msg"), message);

    QJsonObject reply;
    reply.insert(QStringLiteral("type"), MsgType::kError);
    reply.insert(QStringLiteral("ts"), nowMs());
    reply.insert(QStringLiteral("data"), data);
    session->sendFrame(reply);
}

bool MatchHandler::handle(ClientSession *session, const QJsonObject &frame)
{
    const QString type = frame.value(QStringLiteral("type")).toString();
    if (type != MsgType::kMatchStart && type != MsgType::kMatchCancel
        && type != MsgType::kMatchMessage && type != MsgType::kMatchEnd) {
        return false;
    }
    if (session->userId() == 0) {
        sendError(session, 401, QStringLiteral("请先登录"));
        return true;
    }

    if (type == MsgType::kMatchStart)
        return handleStart(session, frame);
    if (type == MsgType::kMatchCancel)
        return handleCancel(session, frame);
    if (type == MsgType::kMatchMessage)
        return handleMessage(session, frame);
    return handleEnd(session, frame);
}

void MatchHandler::removeFromQueues(quint64 userId)
{
    m_textQueue.removeAll(QString::number(userId));
    m_voiceQueue.removeAll(QString::number(userId));

    if (QTimer *timer = m_timeouts.take(userId)) {
        timer->stop();
        timer->deleteLater();
    }
}

bool MatchHandler::handleStart(ClientSession *session, const QJsonObject &frame)
{
    const QJsonObject data = frame.value(QStringLiteral("data")).toObject();
    const QString mode = data.value(QStringLiteral("mode")).toString(QStringLiteral("text"));

    if (m_sessionOf.contains(session->userId())) {
        sendError(session, 409, QStringLiteral("你已经在匹配会话中"));
        return true;
    }
    if (m_textQueue.contains(QString::number(session->userId()))
        || m_voiceQueue.contains(QString::number(session->userId()))) {
        sendError(session, 409, QStringLiteral("正在匹配中，请稍候"));
        return true;
    }

    m_names.insert(session->userId(), session->nickname());
    if (tryMatch(session, mode))
        return true;

    // 没配上：入队并起超时定时器
    (mode == QStringLiteral("voice") ? m_voiceQueue : m_textQueue)
        .append(QString::number(session->userId()));

    auto *timer = new QTimer(this);
    timer->setSingleShot(true);
    connect(timer, &QTimer::timeout, this, [this, session]() { notifyTimeout(session->userId()); });
    m_timeouts.insert(session->userId(), timer);
    timer->start(kMatchTimeoutMs);

    qInfo().noquote() << QStringLiteral("[match] user %1 wait in %2 queue")
                             .arg(session->userId()).arg(mode);
    return true;
}

bool MatchHandler::tryMatch(ClientSession *session, const QString &mode)
{
    QStringList &queue = (mode == QStringLiteral("voice") ? m_voiceQueue : m_textQueue);
    if (queue.isEmpty())
        return false;

    const quint64 peerId = queue.takeFirst().toULongLong();
    if (peerId == session->userId() || peerId == 0)
        return false;

    // 对方可能刚好掉线：递归再试一次
    if (!m_names.contains(peerId))
        return tryMatch(session, mode);

    const QString sessionId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    m_sessionOf.insert(session->userId(), sessionId);
    m_sessionOf.insert(peerId, sessionId);
    m_sessions.insert(sessionId, qMakePair(session->userId(), peerId));
    m_modes.insert(sessionId, mode);
    DbManager::instance().createMatchSession(sessionId, session->userId(), peerId, mode);

    if (QTimer *timer = m_timeouts.take(peerId)) {
        timer->stop();
        timer->deleteLater();
    }
    if (QTimer *timer = m_timeouts.take(session->userId())) {
        timer->stop();
        timer->deleteLater();
    }

    const QString selfName = m_names.value(session->userId());
    const QString peerName = m_names.value(peerId);

    QJsonObject dataForSelf;
    dataForSelf.insert(QStringLiteral("sessionId"), sessionId);
    dataForSelf.insert(QStringLiteral("peerUserId"), static_cast<double>(peerId));
    dataForSelf.insert(QStringLiteral("peerNickname"), peerName);
    dataForSelf.insert(QStringLiteral("mode"), mode);

    QJsonObject frameForSelf;
    frameForSelf.insert(QStringLiteral("type"), MsgType::kMatchResult);
    frameForSelf.insert(QStringLiteral("ts"), nowMs());
    frameForSelf.insert(QStringLiteral("data"), dataForSelf);
    session->sendFrame(frameForSelf);

    QJsonObject dataForPeer;
    dataForPeer.insert(QStringLiteral("sessionId"), sessionId);
    dataForPeer.insert(QStringLiteral("peerUserId"), static_cast<double>(session->userId()));
    dataForPeer.insert(QStringLiteral("peerNickname"), selfName);
    dataForPeer.insert(QStringLiteral("mode"), mode);

    QJsonObject frameForPeer;
    frameForPeer.insert(QStringLiteral("type"), MsgType::kMatchResult);
    frameForPeer.insert(QStringLiteral("ts"), nowMs());
    frameForPeer.insert(QStringLiteral("data"), dataForPeer);
    emit frameToUser(peerId, frameForPeer);

    qInfo().noquote() << QStringLiteral("[match] %1 <-> %2 (%3) session=%4")
                             .arg(selfName, peerName, mode, sessionId);
    return true;
}

void MatchHandler::notifyTimeout(quint64 userId)
{
    removeFromQueues(userId);

    QJsonObject frame;
    frame.insert(QStringLiteral("type"), MsgType::kMatchTimeout);
    frame.insert(QStringLiteral("ts"), nowMs());
    frame.insert(QStringLiteral("data"), QJsonObject());
    emit frameToUser(userId, frame);

    qInfo().noquote() << QStringLiteral("[match] user %1 timeout").arg(userId);
}

bool MatchHandler::handleCancel(ClientSession *session, const QJsonObject &frame)
{
    Q_UNUSED(frame)
    removeFromQueues(session->userId());

    QJsonObject reply;
    reply.insert(QStringLiteral("type"), MsgType::kMatchTimeout);
    reply.insert(QStringLiteral("ts"), nowMs());
    reply.insert(QStringLiteral("data"), QJsonObject());
    session->sendFrame(reply);
    return true;
}

bool MatchHandler::handleMessage(ClientSession *session, const QJsonObject &frame)
{
    const QJsonObject data = frame.value(QStringLiteral("data")).toObject();
    const QString sessionId = data.value(QStringLiteral("sessionId")).toString();
    const QString content = data.value(QStringLiteral("content")).toString().trimmed();

    const QPair<quint64, quint64> pair = m_sessions.value(sessionId, qMakePair(0ULL, 0ULL));
    if (pair.first == 0 || content.isEmpty()) {
        sendError(session, 400, QStringLiteral("匹配会话不存在"));
        return true;
    }
    const quint64 peerId = (pair.first == session->userId() ? pair.second : pair.first);

    QJsonObject out;
    out.insert(QStringLiteral("sessionId"), sessionId);
    out.insert(QStringLiteral("fromUser"), static_cast<double>(session->userId()));
    out.insert(QStringLiteral("nickname"), session->nickname());
    out.insert(QStringLiteral("content"), content);
    out.insert(QStringLiteral("ts"), nowMs());

    QJsonObject relay;
    relay.insert(QStringLiteral("type"), MsgType::kMatchMessage);
    relay.insert(QStringLiteral("ts"), nowMs());
    relay.insert(QStringLiteral("data"), out);
    emit frameToUser(peerId, relay);
    // 回送发送者一份，客户端统一以服务端为准，避免本地回显造成重复
    session->sendFrame(relay);
    return true;
}

void MatchHandler::endSessionInternal(const QString &sessionId, quint64 notifyUser, const QString &reason)
{
    const QPair<quint64, quint64> pair = m_sessions.take(sessionId);
    m_modes.remove(sessionId);
    DbManager::instance().endMatchSession(sessionId);

    if (pair.first)
        m_sessionOf.remove(pair.first);
    if (pair.second)
        m_sessionOf.remove(pair.second);

    if (notifyUser != 0) {
        QJsonObject data;
        data.insert(QStringLiteral("sessionId"), sessionId);
        data.insert(QStringLiteral("reason"), reason);

        QJsonObject frame;
        frame.insert(QStringLiteral("type"), MsgType::kMatchEnd);
        frame.insert(QStringLiteral("ts"), nowMs());
        frame.insert(QStringLiteral("data"), data);
        emit frameToUser(notifyUser, frame);
    }
    qInfo().noquote() << QStringLiteral("[match] session %1 ended (%2)").arg(sessionId, reason);
}

bool MatchHandler::handleEnd(ClientSession *session, const QJsonObject &frame)
{
    Q_UNUSED(frame)
    const QString sessionId = m_sessionOf.value(session->userId());
    if (sessionId.isEmpty()) {
        sendError(session, 400, QStringLiteral("你不在匹配会话中"));
        return true;
    }

    const QPair<quint64, quint64> pair = m_sessions.value(sessionId);
    const quint64 peerId = (pair.first == session->userId() ? pair.second : pair.first);
    endSessionInternal(sessionId, peerId, QStringLiteral("对方结束了会话"));
    return true;
}

void MatchHandler::onUserOffline(quint64 userId)
{
    removeFromQueues(userId);

    const QString sessionId = m_sessionOf.value(userId);
    if (!sessionId.isEmpty()) {
        const QPair<quint64, quint64> pair = m_sessions.value(sessionId);
        const quint64 peerId = (pair.first == userId ? pair.second : pair.first);
        endSessionInternal(sessionId, peerId, QStringLiteral("对方已掉线"));
    }
    m_names.remove(userId);
}