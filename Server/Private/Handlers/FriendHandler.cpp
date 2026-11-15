#include "Handlers/FriendHandler.h"

#include <QDateTime>
#include <QJsonArray>
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

FriendHandler::FriendHandler(QObject *parent)
    : QObject(parent)
{
}

void FriendHandler::sendError(ClientSession *session, int code, const QString &message)
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

void FriendHandler::sendResp(ClientSession *session, const QString &type, bool ok, const QString &message)
{
    QJsonObject data;
    data.insert(QStringLiteral("ok"), ok);
    data.insert(QStringLiteral("msg"), message);

    QJsonObject reply;
    reply.insert(QStringLiteral("type"), type);
    reply.insert(QStringLiteral("ts"), nowMs());
    reply.insert(QStringLiteral("data"), data);
    session->sendFrame(reply);
}

void FriendHandler::pushFriendList(quint64 userId)
{
    if (userId == 0)
        return;

    QJsonObject data;
    data.insert(QStringLiteral("friends"), DbManager::instance().friendsOf(userId));

    QJsonObject frame;
    frame.insert(QStringLiteral("type"), MsgType::kFriendListResp);
    frame.insert(QStringLiteral("ts"), nowMs());
    frame.insert(QStringLiteral("data"), data);
    emit frameToUser(userId, frame);
}

bool FriendHandler::handle(ClientSession *session, const QJsonObject &frame)
{
    const QString type = frame.value(QStringLiteral("type")).toString();
    if (type != MsgType::kFriendRequest && type != MsgType::kFriendAccept
        && type != MsgType::kFriendList) {
        return false;
    }
    if (session->userId() == 0) {
        sendError(session, 401, QStringLiteral("请先登录"));
        return true;
    }

    if (type == MsgType::kFriendRequest)
        return handleRequest(session, frame);
    if (type == MsgType::kFriendAccept)
        return handleAccept(session, frame);
    return handleList(session, frame);
}

bool FriendHandler::handleRequest(ClientSession *session, const QJsonObject &frame)
{
    const QJsonObject data = frame.value(QStringLiteral("data")).toObject();
    const QString keyword = data.value(QStringLiteral("username")).toString().trimmed();
    // 先按用户名找，找不到再按昵称找（界面上提示"输入用户名或昵称"）
    quint64 toUser = DbManager::instance().userIdByName(keyword);
    if (toUser == 0)
        toUser = DbManager::instance().userIdByNickname(keyword);

    if (toUser == 0) {
        sendResp(session, MsgType::kFriendList, false, QStringLiteral("找不到该用户（用户名或昵称）"));
        return true;
    }
    if (toUser == session->userId()) {
        sendResp(session, MsgType::kFriendList, false, QStringLiteral("不能加自己为好友"));
        return true;
    }
    if (DbManager::instance().areFriends(session->userId(), toUser)) {
        sendResp(session, MsgType::kFriendList, false, QStringLiteral("你们已经是好友了"));
        return true;
    }

    if (!DbManager::instance().friendRequest(session->userId(), toUser)) {
        sendResp(session, MsgType::kFriendList, false, QStringLiteral("申请发送失败"));
        return true;
    }

    // 通知对方（对方在线就能立刻看到申请弹窗）
    QJsonObject notifyData;
    notifyData.insert(QStringLiteral("event"), QStringLiteral("request"));
    notifyData.insert(QStringLiteral("fromUser"), static_cast<double>(session->userId()));
    notifyData.insert(QStringLiteral("nickname"), session->nickname());
    notifyData.insert(QStringLiteral("message"),
                      QStringLiteral("%1 请求加你为好友").arg(session->nickname()));

    QJsonObject notify;
    notify.insert(QStringLiteral("type"), MsgType::kFriendNotify);
    notify.insert(QStringLiteral("ts"), nowMs());
    notify.insert(QStringLiteral("data"), notifyData);
    emit frameToUser(toUser, notify);

    qInfo().noquote() << QStringLiteral("[friend] %1 -> %2 request").arg(session->nickname(), keyword);
    sendResp(session, MsgType::kFriendList, true, QStringLiteral("好友申请已发送"));
    return true;
}

bool FriendHandler::handleAccept(ClientSession *session, const QJsonObject &frame)
{
    const QJsonObject data = frame.value(QStringLiteral("data")).toObject();
    const quint64 fromUser = static_cast<quint64>(data.value(QStringLiteral("fromUser")).toDouble());

    if (fromUser == 0 || !DbManager::instance().acceptFriend(fromUser, session->userId())) {
        sendResp(session, MsgType::kFriendAccept, false, QStringLiteral("接受好友失败"));
        return true;
    }

    // 双方都推最新列表，谁都不用重启客户端
    pushFriendList(session->userId());
    pushFriendList(fromUser);

    QJsonObject notifyData;
    notifyData.insert(QStringLiteral("event"), QStringLiteral("accepted"));
    notifyData.insert(QStringLiteral("fromUser"), static_cast<double>(session->userId()));
    notifyData.insert(QStringLiteral("nickname"), session->nickname());
    notifyData.insert(QStringLiteral("message"),
                      QStringLiteral("%1 同意了你的好友申请").arg(session->nickname()));

    QJsonObject notify;
    notify.insert(QStringLiteral("type"), MsgType::kFriendNotify);
    notify.insert(QStringLiteral("ts"), nowMs());
    notify.insert(QStringLiteral("data"), notifyData);
    emit frameToUser(fromUser, notify);

    qInfo().noquote() << QStringLiteral("[friend] %1 accepted %2").arg(session->nickname()).arg(fromUser);
    sendResp(session, MsgType::kFriendAccept, true, QStringLiteral("已添加好友"));
    return true;
}

bool FriendHandler::handleList(ClientSession *session, const QJsonObject &frame)
{
    Q_UNUSED(frame)
    pushFriendList(session->userId());
    return true;
}