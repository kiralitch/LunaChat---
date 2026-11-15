#pragma once

#include <QJsonObject>
#include <QObject>
#include <QString>

class QTcpSocket;
class FrameCodec;
class QTimer;

// 阶段1 客户端网络层：连接 + 心跳(15s) + 无回包判定(45s) + 指数退避重连(1/2/4/8/16/30s)
class LunaClient : public QObject
{
    Q_OBJECT

public:
    enum State { Disconnected, Connecting, Connected };

    explicit LunaClient(QObject *parent = nullptr);
    ~LunaClient() override;

    void connectToServer(const QString &host, quint16 port);
    void disconnectFromServer();   // 主动断开：不触发自动重连

    State state() const { return m_state; }
    int reconnectAttempt() const { return m_attempt; }

signals:
    void connected();
    void disconnected();
    void frameReceived(const QJsonObject &frame);
    void reconnecting(int attempt, int delayMs);
    void errorOccurred(const QString &message);

public slots:
    void sendFrame(const QJsonObject &obj);

private slots:
    void onConnected();
    void onDisconnected();
    void onSocketError();
    void onReadyRead();
    void onHeartbeatTick();
    void tryReconnect();

private:
    void setState(State state);
    void scheduleReconnect();

    QTcpSocket *m_socket = nullptr;
    FrameCodec *m_codec = nullptr;
    QTimer *m_heartbeatTimer = nullptr;
    QTimer *m_reconnectTimer = nullptr;

    QString m_host;
    quint16 m_port = 0;
    State m_state = Disconnected;
    int m_attempt = 0;            // 已连续重连次数，连上即清零
    bool m_manualClose = false;   // 用户主动断开时置 true，抑制重连
    qint64 m_lastRecvMs = 0;
};