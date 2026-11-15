#pragma once

#include <QJsonObject>
#include <QObject>

class ClientSession;

// 朋友圈：发布 / 分页列表 / 按需取图 / 点赞 / 评论 / 删除
class MomentHandler : public QObject
{
    Q_OBJECT

public:
    explicit MomentHandler(QObject *parent = nullptr);

    bool handle(ClientSession *session, const QJsonObject &frame);

signals:
    void frameToUser(quint64 userId, const QJsonObject &frame);

private:
    bool handlePublish(ClientSession *session, const QJsonObject &frame);
    bool handleList(ClientSession *session, const QJsonObject &frame);
    bool handleImage(ClientSession *session, const QJsonObject &frame);
    bool handleLike(ClientSession *session, const QJsonObject &frame);
    bool handleComment(ClientSession *session, const QJsonObject &frame);
    bool handleDelete(ClientSession *session, const QJsonObject &frame);

    void sendError(ClientSession *session, int code, const QString &message);
    void notifyOwner(quint64 ownerId, const QString &event, const QString &message, qint64 momentId);
};