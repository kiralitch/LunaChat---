#include "Network/ClientSession.h"

#include <QDateTime>
#include <QHostAddress>
#include <QTcpSocket>
#include <QtDebug>

#include "LunaProtocol/FrameCodec.h"

ClientSession::ClientSession(QTcpSocket *socket, QObject *parent)
    : QObject(parent), m_socket(socket), m_codec(new FrameCodec),
      m_lastActiveMs(QDateTime::currentMSecsSinceEpoch())
{
    m_socket->setParent(this);
    connect(m_socket, &QTcpSocket::readyRead, this, &ClientSession::onReadyRead);

    // 对端断开只上报一次，由 LunaServer 统一从会话表移除
    connect(m_socket, &QTcpSocket::disconnected, this, [this]() { emit closed(this); });
}

ClientSession::~ClientSession()
{
    delete m_codec;
}

QString ClientSession::peerAddress() const
{
    return m_socket ? m_socket->peerAddress().toString() : QString();
}

quint16 ClientSession::peerPort() const
{
    return m_socket ? m_socket->peerPort() : 0;
}

QString ClientSession::nickname() const
{
    return m_nickname;
}

void ClientSession::touch()
{
    m_lastActiveMs = QDateTime::currentMSecsSinceEpoch();
}

void ClientSession::onReadyRead()
{
    m_codec->append(m_socket->readAll());

    QJsonObject frame;
    for (;;) {
        const FrameCodec::Result result = m_codec->nextFrame(frame);
        if (result == FrameCodec::NeedMoreData)
            break;
        if (result != FrameCodec::FrameDecoded) {
            closeSession(QStringLiteral("protocol error"));
            return;
        }
        touch();   // 任何一帧都算活跃（心跳帧也算），超时判定以此为准
        emit frameReceived(this, frame);
    }
}

void ClientSession::sendFrame(const QJsonObject &obj)
{
    if (!m_socket)
        return;
    m_socket->write(FrameCodec::encode(obj));
}

void ClientSession::closeSession(const QString &reason)
{
    qInfo().noquote() << QStringLiteral("[session] %1 closed: %2").arg(peerPort()).arg(reason);
    if (m_socket)
        m_socket->disconnectFromHost();
}