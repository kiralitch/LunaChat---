#pragma once

#include <QJsonObject>
#include <QObject>

class ClientSession;

// 聊天处理：群聊 / 私聊 / 历史查询；只负责落库与组帧，投递交给 LunaServer
class ChatHandler : public QObject
{
    Q_OBJECT

public:
    explicit ChatHandler(QObject *parent = nullptr);

    // 返回 true 表示已处理（含权限拒绝与参数错误）
    bool handle(ClientSession *session, const QJsonObject &frame);

signals:
    void frameToGroup(qint64 groupId, const QJsonObject &frame);
    void frameToUser(quint64 userId, const QJsonObject &frame);

private:
    bool handleGroup(ClientSession *session, const QJsonObject &frame);
    bool handlePrivate(ClientSession *session, const QJsonObject &frame);
    bool handleHistory(ClientSession *session, const QJsonObject &frame);
    bool handleRecall(ClientSession *session, const QJsonObject &frame);

    void sendError(ClientSession *session, int code, const QString &message);
};