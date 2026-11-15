#pragma once

#include <QJsonObject>
#include <QObject>

class QTcpSocket;
class FrameCodec;

// 一条客户端连接的服务端句柄：socket + 独立解码器 + 心跳时间戳 + 登录态
class ClientSession : public QObject
{
    Q_OBJECT

public:
    explicit ClientSession(QTcpSocket *socket, QObject *parent = nullptr);
    ~ClientSession() override;

    QTcpSocket *socket() const { return m_socket; }
    QString peerAddress() const;
    quint16 peerPort() const;

    // 阶段2 由 login 处理器写入；0 表示尚未登录
    void setUserId(quint64 id) { m_userId = id; }
    quint64 userId() const { return m_userId; }

    void setNickname(const QString &nickname) { m_nickname = nickname; }
    QString nickname() const;

    qint64 lastActiveMs() const { return m_lastActiveMs; }
    void touch();

    void sendFrame(const QJsonObject &obj);
    void closeSession(const QString &reason);

signals:
    void frameReceived(ClientSession *session, const QJsonObject &frame);
    void closed(ClientSession *session);

private slots:
    void onReadyRead();

private:
    QTcpSocket *m_socket = nullptr;
    FrameCodec *m_codec = nullptr;
    qint64 m_lastActiveMs = 0;   // 最近一次收到帧的时间，心跳超时判定只看它
    quint64 m_userId = 0;
    QString m_nickname;
};