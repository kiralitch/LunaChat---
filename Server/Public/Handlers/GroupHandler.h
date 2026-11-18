#pragma once

#include <QJsonObject>
#include <QObject>

class ClientSession;

// 群组处理：建群 / 我的群列表 / 邀请成员（仅群主或管理员）
class GroupHandler : public QObject
{
    Q_OBJECT

public:
    explicit GroupHandler(QObject *parent = nullptr);

    bool handle(ClientSession *session, const QJsonObject &frame);

signals:
    void frameToUser(quint64 userId, const QJsonObject &frame);

private:
    bool handleCreate(ClientSession *session, const QJsonObject &frame);
    bool handleList(ClientSession *session, const QJsonObject &frame);
    bool handleSearch(ClientSession *session, const QJsonObject &frame);
    bool handleJoinRequest(ClientSession *session, const QJsonObject &frame);
    bool handleJoinRequests(ClientSession *session, const QJsonObject &frame);
    bool handleJoinHandle(ClientSession *session, const QJsonObject &frame, bool accept);
    bool handleInvite(ClientSession *session, const QJsonObject &frame);
    bool handleInfo(ClientSession *session, const QJsonObject &frame);
    bool handleInfoUpdate(ClientSession *session, const QJsonObject &frame);
    bool handleLeave(ClientSession *session, const QJsonObject &frame);
    bool handleTransfer(ClientSession *session, const QJsonObject &frame);
    bool handleDissolve(ClientSession *session, const QJsonObject &frame);
    bool handleAvatarGet(ClientSession *session, const QJsonObject &frame);
    bool handleFilesList(ClientSession *session, const QJsonObject &frame);
    bool handleMkdir(ClientSession *session, const QJsonObject &frame);
    bool handleDeleteFile(ClientSession *session, const QJsonObject &frame);
    bool handleDownload(ClientSession *session, const QJsonObject &frame);

    void sendGroupInfo(ClientSession *session, qint64 groupId, bool ok, const QString &msg);
    void pushGroupListTo(quint64 userId);

    void sendError(ClientSession *session, int code, const QString &message);
};