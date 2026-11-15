#include "Handlers/ActivityHandler.h"

#include <QDateTime>
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

ActivityHandler::ActivityHandler(QObject *parent)
    : QObject(parent)
{
}

bool ActivityHandler::handle(ClientSession *session, const QJsonObject &frame)
{
    const QString type = frame.value(QStringLiteral("type")).toString();
    if (type != MsgType::kActivityCreate && type != MsgType::kActivityList
        && type != MsgType::kActivityDetail && type != MsgType::kActivitySignup
        && type != MsgType::kActivitySignupCancel && type != MsgType::kActivityCancel) {
        return false;
    }
    if (session->userId() == 0) {
        sendError(session, 401, QStringLiteral("请先登录"));
        return true;
    }

    if (type == MsgType::kActivityCreate)
        return handleCreate(session, frame);
    if (type == MsgType::kActivityList)
        return handleList(session, frame);
    if (type == MsgType::kActivityDetail)
        return handleDetail(session, frame);
    if (type == MsgType::kActivitySignup)
        return handleSignup(session, frame);
    if (type == MsgType::kActivitySignupCancel)
        return handleCancelSignup(session, frame);
    return handleCancel(session, frame);
}

void ActivityHandler::sendError(ClientSession *session, int code, const QString &message)
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

void ActivityHandler::notifyGroup(qint64 groupId, qint64 activityId,
                                  const QString &event, const QString &message)
{
    QJsonObject data;
    data.insert(QStringLiteral("groupId"), static_cast<double>(groupId));
    data.insert(QStringLiteral("activityId"), static_cast<double>(activityId));
    data.insert(QStringLiteral("event"), event);
    data.insert(QStringLiteral("message"), message);

    QJsonObject frame;
    frame.insert(QStringLiteral("type"), MsgType::kActivityNotify);
    frame.insert(QStringLiteral("ts"), nowMs());
    frame.insert(QStringLiteral("data"), data);
    emit frameToGroup(groupId, frame);
}

bool ActivityHandler::handleCreate(ClientSession *session, const QJsonObject &frame)
{
    const QJsonObject data = frame.value(QStringLiteral("data")).toObject();
    const qint64 groupId = static_cast<qint64>(data.value(QStringLiteral("groupId")).toDouble());
    const QString title = data.value(QStringLiteral("title")).toString().trimmed();
    const QString description = data.value(QStringLiteral("description")).toString();
    const QString location = data.value(QStringLiteral("location")).toString().trimmed();
    const QString startTime = data.value(QStringLiteral("startTime")).toString();
    const QString endTime = data.value(QStringLiteral("endTime")).toString();
    const QString deadline = data.value(QStringLiteral("signupDeadline")).toString();
    const int maxParticipants = data.value(QStringLiteral("maxParticipants")).toInt();

    // 权限校验：只有群主/管理员能发活动
    if (!DbManager::instance().isGroupAdmin(groupId, session->userId())) {
        sendError(session, 403, QStringLiteral("只有群主或管理员可以发布活动"));
        return true;
    }
    if (title.isEmpty() || startTime.isEmpty() || endTime.isEmpty() || deadline.isEmpty()) {
        sendError(session, 400, QStringLiteral("活动信息不完整"));
        return true;
    }
    if (startTime >= endTime) {
        sendError(session, 400, QStringLiteral("结束时间必须晚于开始时间"));
        return true;
    }

    const qint64 activityId = DbManager::instance().createActivity(
        groupId, session->userId(), title, description, location,
        startTime, endTime, deadline, maxParticipants);
    if (activityId <= 0) {
        sendError(session, 500, QStringLiteral("活动创建失败"));
        return true;
    }

    qInfo().noquote() << QStringLiteral("[activity] created %1 (id=%2) group=%3 by %4")
                             .arg(title).arg(activityId).arg(groupId).arg(session->nickname());
    notifyGroup(groupId, activityId, QStringLiteral("created"),
                QStringLiteral("%1 发布了活动《%2》").arg(session->nickname(), title));
    return true;
}

bool ActivityHandler::handleList(ClientSession *session, const QJsonObject &frame)
{
    const QJsonObject data = frame.value(QStringLiteral("data")).toObject();
    const qint64 groupId = static_cast<qint64>(data.value(QStringLiteral("groupId")).toDouble(1));

    QJsonObject replyData;
    replyData.insert(QStringLiteral("groupId"), static_cast<double>(groupId));
    replyData.insert(QStringLiteral("activities"),
                     DbManager::instance().listActivities(groupId, session->userId()));

    QJsonObject reply;
    reply.insert(QStringLiteral("type"), MsgType::kActivityListResp);
    reply.insert(QStringLiteral("ts"), nowMs());
    reply.insert(QStringLiteral("data"), replyData);
    session->sendFrame(reply);
    return true;
}

bool ActivityHandler::handleDetail(ClientSession *session, const QJsonObject &frame)
{
    const QJsonObject data = frame.value(QStringLiteral("data")).toObject();
    const qint64 activityId = static_cast<qint64>(data.value(QStringLiteral("activityId")).toDouble());

    QJsonObject detail = DbManager::instance().activityDetail(activityId, session->userId());
    if (detail.isEmpty()) {
        sendError(session, 404, QStringLiteral("活动不存在"));
        return true;
    }

    QJsonObject reply;
    reply.insert(QStringLiteral("type"), MsgType::kActivityDetailResp);
    reply.insert(QStringLiteral("ts"), nowMs());
    reply.insert(QStringLiteral("data"), detail);
    session->sendFrame(reply);
    return true;
}

bool ActivityHandler::handleSignup(ClientSession *session, const QJsonObject &frame)
{
    const QJsonObject data = frame.value(QStringLiteral("data")).toObject();
    const qint64 activityId = static_cast<qint64>(data.value(QStringLiteral("activityId")).toDouble());

    const QString error = DbManager::instance().signupActivity(activityId, session->userId());
    if (!error.isEmpty()) {
        sendError(session, 409, error);
        return true;
    }

    qint64 groupId = 0;
    QString title;
    DbManager::instance().activityBrief(activityId, &groupId, &title);
    notifyGroup(groupId, activityId, QStringLiteral("signup"),
                QStringLiteral("%1 报名了《%2》").arg(session->nickname(), title));
    return true;
}

bool ActivityHandler::handleCancelSignup(ClientSession *session, const QJsonObject &frame)
{
    const QJsonObject data = frame.value(QStringLiteral("data")).toObject();
    const qint64 activityId = static_cast<qint64>(data.value(QStringLiteral("activityId")).toDouble());

    const QString error = DbManager::instance().cancelSignup(activityId, session->userId());
    if (!error.isEmpty()) {
        sendError(session, 409, error);
        return true;
    }

    qint64 groupId = 0;
    QString title;
    DbManager::instance().activityBrief(activityId, &groupId, &title);
    notifyGroup(groupId, activityId, QStringLiteral("cancel_signup"),
                QStringLiteral("%1 取消报名《%2》").arg(session->nickname(), title));
    return true;
}

bool ActivityHandler::handleCancel(ClientSession *session, const QJsonObject &frame)
{
    const QJsonObject data = frame.value(QStringLiteral("data")).toObject();
    const qint64 activityId = static_cast<qint64>(data.value(QStringLiteral("activityId")).toDouble());
    const QString reason = data.value(QStringLiteral("reason")).toString();

    qint64 groupId = 0;
    QString title;
    if (!DbManager::instance().activityBrief(activityId, &groupId, &title)) {
        sendError(session, 404, QStringLiteral("活动不存在"));
        return true;
    }
    if (!DbManager::instance().isGroupAdmin(groupId, session->userId())) {
        sendError(session, 403, QStringLiteral("只有群主或管理员可以取消活动"));
        return true;
    }
    if (!DbManager::instance().cancelActivity(activityId)) {
        sendError(session, 400, QStringLiteral("活动已结束，无法取消"));
        return true;
    }

    qInfo().noquote() << QStringLiteral("[activity] cancelled %1 (id=%2) by %3")
                             .arg(title).arg(activityId).arg(session->nickname());
    const QString suffix = reason.isEmpty() ? QString() : QStringLiteral("，原因：") + reason;
    notifyGroup(groupId, activityId, QStringLiteral("cancelled"),
                QStringLiteral("活动《%1》已取消%2").arg(title, suffix));
    return true;
}