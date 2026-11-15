#include "Handlers/ChatHandler.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonObject>
#include <QtDebug>

#include "LunaDb/DbManager.h"
#include "LunaProtocol/MessageTypes.h"
#include "Network/ClientSession.h"

namespace {

double nowMs()
{
    return static_cast<double>(QDateTime::currentMSecsSinceEpoch());
}

}

ChatHandler::ChatHandler(QObject *parent)
    : QObject(parent)
{
}

bool ChatHandler::handle(ClientSession *session, const QJsonObject &frame)
{
    const QString type = frame.value(QStringLiteral("type")).toString();

    // 权限校验：未登录连接不允许发业务帧（心跳由 LunaServer 先处理）
    if (type == MsgType::kChatGroup || type == MsgType::kChatPrivate
        || type == MsgType::kChatHistory) {
        if (session->userId() == 0) {
            sendError(session, 401, QStringLiteral("请先登录"));
            return true;
        }
    }

    if (type == MsgType::kChatGroup)
        return handleGroup(session, frame);
    if (type == MsgType::kChatPrivate)
        return handlePrivate(session, frame);
    if (type == MsgType::kChatHistory)
        return handleHistory(session, frame);
    return false;
}

void ChatHandler::sendError(ClientSession *session, int code, const QString &message)
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

bool ChatHandler::handleGroup(ClientSession *session, const QJsonObject &frame)
{
    const QJsonObject data = frame.value(QStringLiteral("data")).toObject();
    const qint64 groupId = static_cast<qint64>(data.value(QStringLiteral("groupId")).toDouble(1));
    const QString content = data.value(QStringLiteral("content")).toString().trimmed();

    if (content.isEmpty()) {
        sendError(session, 400, QStringLiteral("消息内容不能为空"));
        return true;
    }
    if (!DbManager::instance().isGroupMember(groupId, session->userId())) {
        sendError(session, 403, QStringLiteral("你不在该群组中"));
        return true;
    }

    const qint64 msgId = DbManager::instance().saveMessage(
        session->userId(), 0, groupId, QStringLiteral("text"), content);
    if (msgId <= 0) {
        sendError(session, 500, QStringLiteral("消息保存失败"));
        return true;
    }

    QJsonObject out;
    out.insert(QStringLiteral("fromUser"), static_cast<double>(session->userId()));
    out.insert(QStringLiteral("nickname"), session->nickname());
    out.insert(QStringLiteral("groupId"), static_cast<double>(groupId));
    out.insert(QStringLiteral("content"), content);
    out.insert(QStringLiteral("msgId"), static_cast<double>(msgId));
    out.insert(QStringLiteral("msgType"), QStringLiteral("text"));
    out.insert(QStringLiteral("ts"), nowMs());

    QJsonObject broadcast;
    broadcast.insert(QStringLiteral("type"), MsgType::kChatBroadcast);
    broadcast.insert(QStringLiteral("from"), session->nickname());
    broadcast.insert(QStringLiteral("ts"), nowMs());
    broadcast.insert(QStringLiteral("data"), out);

    qInfo().noquote() << QStringLiteral("[chat] group %1 <- %2: %3")
                             .arg(groupId).arg(session->nickname(), content.left(40));
    emit frameToGroup(groupId, broadcast);
    return true;
}

bool ChatHandler::handlePrivate(ClientSession *session, const QJsonObject &frame)
{
    const QJsonObject data = frame.value(QStringLiteral("data")).toObject();
    const quint64 toUser = static_cast<quint64>(data.value(QStringLiteral("toUser")).toDouble());
    const QString content = data.value(QStringLiteral("content")).toString().trimmed();

    if (toUser == 0 || toUser == session->userId()) {
        sendError(session, 400, QStringLiteral("私聊对象无效"));
        return true;
    }
    if (content.isEmpty()) {
        sendError(session, 400, QStringLiteral("消息内容不能为空"));
        return true;
    }

    const qint64 msgId = DbManager::instance().saveMessage(
        session->userId(), toUser, 0, QStringLiteral("text"), content);
    if (msgId <= 0) {
        sendError(session, 500, QStringLiteral("消息保存失败"));
        return true;
    }

    QJsonObject out;
    out.insert(QStringLiteral("fromUser"), static_cast<double>(session->userId()));
    out.insert(QStringLiteral("nickname"), session->nickname());
    out.insert(QStringLiteral("toUser"), static_cast<double>(toUser));
    out.insert(QStringLiteral("content"), content);
    out.insert(QStringLiteral("msgId"), static_cast<double>(msgId));
    out.insert(QStringLiteral("msgType"), QStringLiteral("text"));
    out.insert(QStringLiteral("ts"), nowMs());

    QJsonObject payload;
    payload.insert(QStringLiteral("type"), MsgType::kChatBroadcast);
    payload.insert(QStringLiteral("from"), session->nickname());
    payload.insert(QStringLiteral("ts"), nowMs());
    payload.insert(QStringLiteral("data"), out);

    qInfo().noquote() << QStringLiteral("[chat] private %1 -> %2: %3")
                             .arg(session->nickname()).arg(toUser).arg(content.left(40));
    emit frameToUser(toUser, payload);
    // 同时回送给发送者：客户端不再本地回显，统一以服务端这一份为准，避免"一条消息显示两次"
    emit frameToUser(session->userId(), payload);
    return true;
}

bool ChatHandler::handleHistory(ClientSession *session, const QJsonObject &frame)
{
    const QJsonObject data = frame.value(QStringLiteral("data")).toObject();
    const QString scope = data.value(QStringLiteral("scope")).toString(QStringLiteral("group"));
    int limit = data.value(QStringLiteral("limit")).toInt(50);
    limit = qBound(1, limit, 200);

    QJsonObject replyData;
    replyData.insert(QStringLiteral("scope"), scope);

    if (scope == QStringLiteral("group")) {
        const qint64 groupId = static_cast<qint64>(data.value(QStringLiteral("groupId")).toDouble(1));
        if (!DbManager::instance().isGroupMember(groupId, session->userId())) {
            sendError(session, 403, QStringLiteral("你不在该群组中"));
            return true;
        }
        replyData.insert(QStringLiteral("groupId"), static_cast<double>(groupId));
        replyData.insert(QStringLiteral("messages"),
                         DbManager::instance().recentGroupMessages(groupId, limit));
    } else {
        const quint64 peerId = static_cast<quint64>(data.value(QStringLiteral("toUser")).toDouble());
        if (peerId == 0) {
            sendError(session, 400, QStringLiteral("缺少私聊对象"));
            return true;
        }
        replyData.insert(QStringLiteral("toUser"), static_cast<double>(peerId));
        replyData.insert(QStringLiteral("messages"),
                         DbManager::instance().recentPrivateMessages(session->userId(), peerId, limit));
    }

    QJsonObject reply;
    reply.insert(QStringLiteral("type"), MsgType::kChatHistoryResp);
    reply.insert(QStringLiteral("ts"), nowMs());
    reply.insert(QStringLiteral("data"), replyData);
    session->sendFrame(reply);
    return true;
}