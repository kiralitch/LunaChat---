#pragma once

#include <QJsonObject>
#include <QList>
#include <QObject>
#include <QTcpServer>

class ClientSession;
class MessageDispatcher;
class QTimer;

// 阶段2 服务端：会话管理 + 心跳超时清理 + 登录态 + user_list 广播
class LunaServer : public QObject
{
    Q_OBJECT

public:
    explicit LunaServer(QObject *parent = nullptr);
    ~LunaServer() override;

    bool start(quint16 port);
    int onlineCount() const { return m_sessions.size(); }

    // 管理端接口：会话快照 + 踢人
    QList<ClientSession *> sessions() const { return m_sessions; }
    bool kickUser(quint64 userId, const QString &reason);

signals:
    void onlineChanged();   // 在线列表变化，管理界面据此刷新

private slots:
    void onNewConnection();
    void sweepIdleSessions();
    void onSessionFrame(ClientSession *session, const QJsonObject &frame);
    void onSessionClosed(ClientSession *session);
    void onUserLoggedIn(ClientSession *session);
    void onFrameToGroup(qint64 groupId, const QJsonObject &frame);
    void onFrameToUser(quint64 userId, const QJsonObject &frame);

private:
    void broadcastUserList();
    ClientSession *findSessionByUserId(quint64 userId) const;

    QTcpServer *m_server = nullptr;
    QTimer *m_sweepTimer = nullptr;
    MessageDispatcher *m_dispatcher = nullptr;
    QList<ClientSession *> m_sessions;
};