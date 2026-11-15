#pragma once

#include <QJsonObject>
#include <QObject>

class ClientSession;

// 好友：申请 / 接受 / 好友列表（申请与接受都会主动推送最新列表）
class FriendHandler : public QObject
{
    Q_OBJECT

public:
    explicit FriendHandler(QObject *parent = nullptr);

    bool handle(ClientSession *session, const QJsonObject &frame);

    // 供其它模块调用：主动给对方推一份最新好友列表
    void pushFriendList(quint64 userId);

signals:
    void frameToUser(quint64 userId, const QJsonObject &frame);

private:
    bool handleRequest(ClientSession *session, const QJsonObject &frame);
    bool handleAccept(ClientSession *session, const QJsonObject &frame);
    bool handleList(ClientSession *session, const QJsonObject &frame);

    void sendError(ClientSession *session, int code, const QString &message);
    void sendResp(ClientSession *session, const QString &type, bool ok, const QString &message);
};