#pragma once

#include <QJsonObject>
#include <QObject>

class ClientSession;
class ChatHandler;
class GroupHandler;
class FileHandler;
class ActivityHandler;
class MomentHandler;
class MatchHandler;
class VoiceHandler;
class ProfileHandler;
class FriendHandler;

#include <functional>

// 业务分发层：只看 type 决定谁来处理，网络收发细节留在 LunaServer
class MessageDispatcher : public QObject
{
    Q_OBJECT

public:
    explicit MessageDispatcher(QObject *parent = nullptr);

    // 返回 true 表示已处理；false 表示没有对应处理器（调用方回 error）
    bool dispatch(ClientSession *session, const QJsonObject &frame);

    // 注入"在线判定"，供文件传输拒绝离线接收方
    void setOnlineChecker(std::function<bool(quint64)> checker);

    // 连接断开时调用：退出匹配队列并结束其匹配会话
    void onUserOffline(quint64 userId);
    void pushFriendList(quint64 userId);

signals:
    void userLoggedIn(ClientSession *session);   // 登录成功：由 LunaServer 处理重复登录与广播
    void userListChanged();

    // 聊天的投递意图：由 LunaServer 决定发给哪些在线会话 / 写离线消息
    void frameToGroup(qint64 groupId, const QJsonObject &frame);
    void frameToUser(quint64 userId, const QJsonObject &frame);

private:
    bool handleRegister(ClientSession *session, const QJsonObject &frame);
    bool handleLogin(ClientSession *session, const QJsonObject &frame);

    ChatHandler *m_chat = nullptr;
    GroupHandler *m_group = nullptr;
    FileHandler *m_file = nullptr;
    ActivityHandler *m_activity = nullptr;
    MomentHandler *m_moment = nullptr;
    MatchHandler *m_match = nullptr;
    VoiceHandler *m_voice = nullptr;
    ProfileHandler *m_profile = nullptr;
    FriendHandler *m_friend = nullptr;

    void replyLoginResp(ClientSession *session, bool ok, const QString &msg,
                        quint64 userId = 0, const QString &nickname = QString());
};