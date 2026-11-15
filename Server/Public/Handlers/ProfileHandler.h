#pragma once

#include <QJsonObject>
#include <QObject>

class ClientSession;

// 个人资料：查询 / 改昵称 / 换头像（头像存服务端 avatars/，按需回 base64）
class ProfileHandler : public QObject
{
    Q_OBJECT

public:
    explicit ProfileHandler(QObject *parent = nullptr);

    bool handle(ClientSession *session, const QJsonObject &frame);

signals:
    void userListChanged();

private:
    bool handleGet(ClientSession *session, const QJsonObject &frame);
    bool handleUpdate(ClientSession *session, const QJsonObject &frame);
    bool handleAvatarGet(ClientSession *session, const QJsonObject &frame);
    void sendError(ClientSession *session, int code, const QString &message);

    QString avatarFilePath(quint64 userId) const;
};