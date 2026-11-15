#pragma once

#include <QJsonObject>
#include <QObject>

class ClientSession;

// 群活动：发布/列表/详情/报名/取消报名/取消活动 + 群内通知
class ActivityHandler : public QObject
{
    Q_OBJECT

public:
    explicit ActivityHandler(QObject *parent = nullptr);

    bool handle(ClientSession *session, const QJsonObject &frame);

signals:
    void frameToGroup(qint64 groupId, const QJsonObject &frame);

private:
    bool handleCreate(ClientSession *session, const QJsonObject &frame);
    bool handleList(ClientSession *session, const QJsonObject &frame);
    bool handleDetail(ClientSession *session, const QJsonObject &frame);
    bool handleSignup(ClientSession *session, const QJsonObject &frame);
    bool handleCancelSignup(ClientSession *session, const QJsonObject &frame);
    bool handleCancel(ClientSession *session, const QJsonObject &frame);

    void sendError(ClientSession *session, int code, const QString &message);
    void notifyGroup(qint64 groupId, qint64 activityId, const QString &event, const QString &message);
};