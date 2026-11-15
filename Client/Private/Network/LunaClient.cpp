#include "Network/LunaClient.h"

#include <QDateTime>
#include <QJsonDocument>
#include <QTcpSocket>
#include <QTimer>
#include <QtDebug>

#include "LunaProtocol/FrameCodec.h"
#include "LunaProtocol/LunaConstants.h"
#include "LunaProtocol/MessageTypes.h"

namespace {

// 重连退避序列：1s,2s,4s,8s,16s,30s，之后固定 30s（上限）
int backoffDelayMs(int attempt)
{
    static const int kSequence[] = { 1000, 2000, 4000, 8000, 16000, 30000 };
    const int index = qBound(0, attempt, 5);
    return kSequence[index];
}

}

LunaClient::LunaClient(QObject *parent)
    : QObject(parent), m_socket(new QTcpSocket(this)), m_codec(new FrameCodec)
{
    m_heartbeatTimer = new QTimer(this);
    m_heartbeatTimer->setInterval(LunaConst::kHeartbeatIntervalMs);
    connect(m_heartbeatTimer, &QTimer::timeout, this, &LunaClient::onHeartbeatTick);

    m_reconnectTimer = new QTimer(this);
    m_reconnectTimer->setSingleShot(true);
    connect(m_reconnectTimer, &QTimer::timeout, this, &LunaClient::tryReconnect);

    connect(m_socket, &QTcpSocket::connected, this, &LunaClient::onConnected);
    connect(m_socket, &QTcpSocket::disconnected, this, &LunaClient::onDisconnected);
    connect(m_socket, &QTcpSocket::readyRead, this, &LunaClient::onReadyRead);
    connect(m_socket, &QTcpSocket::errorOccurred, this, &LunaClient::onSocketError);
}

LunaClient::~LunaClient()
{
    delete m_codec;
}

void LunaClient::connectToServer(const QString &host, quint16 port)
{
    m_host = host;
    m_port = port;
    m_manualClose = false;
    m_attempt = 0;
    m_codec->clear();
    setState(Connecting);
    m_socket->connectToHost(host, port);
}

void LunaClient::disconnectFromServer()
{
    m_manualClose = true;
    m_reconnectTimer->stop();
    m_heartbeatTimer->stop();
    m_socket->disconnectFromHost();
}

void LunaClient::setState(State state)
{
    m_state = state;
}

void LunaClient::onConnected()
{
    m_attempt = 0;                 // 连上就清零退避，下次掉线重新从 1s 起
    m_lastRecvMs = QDateTime::currentMSecsSinceEpoch();
    m_codec->clear();              // 新连接丢弃旧半包，避免串帧
    setState(Connected);
    m_heartbeatTimer->start();
    emit connected();
}

void LunaClient::onDisconnected()
{
    m_heartbeatTimer->stop();
    setState(Disconnected);
    emit disconnected();

    if (m_manualClose)
        return;
    scheduleReconnect();
}

void LunaClient::onSocketError()
{
    emit errorOccurred(m_socket->errorString());

    // 连接阶段失败不会触发 disconnected，这里补一次重连调度
    if (!m_manualClose && m_state != Connected) {
        setState(Disconnected);
        scheduleReconnect();
    }
}

void LunaClient::scheduleReconnect()
{
    if (m_reconnectTimer->isActive())
        return;

    const int delayMs = backoffDelayMs(m_attempt);
    ++m_attempt;
    qWarning().noquote() << QStringLiteral("[client] reconnect attempt %1 in %2 ms")
                                .arg(m_attempt).arg(delayMs);
    emit reconnecting(m_attempt, delayMs);
    m_reconnectTimer->start(delayMs);
}

void LunaClient::tryReconnect()
{
    if (m_manualClose)
        return;

    qInfo() << "[client] trying" << m_host << ":" << m_port;
    m_codec->clear();
    setState(Connecting);
    m_socket->abort();
    m_socket->connectToHost(m_host, m_port);
}

void LunaClient::onReadyRead()
{
    m_codec->append(m_socket->readAll());

    QJsonObject frame;
    for (;;) {
        const FrameCodec::Result result = m_codec->nextFrame(frame);
        if (result == FrameCodec::NeedMoreData)
            break;
        if (result != FrameCodec::FrameDecoded) {
            qWarning() << "[client] protocol error, drop connection";
            m_socket->abort();
            return;
        }
        m_lastRecvMs = QDateTime::currentMSecsSinceEpoch();
        emit frameReceived(frame);
    }
}

void LunaClient::onHeartbeatTick()
{
    if (m_state != Connected)
        return;

    // 45s 没有任何回包 -> 认为链路已死（对端进程被杀/网线拔掉），主动断开触发重连
    if (QDateTime::currentMSecsSinceEpoch() - m_lastRecvMs > LunaConst::kHeartbeatTimeoutMs) {
        qWarning() << "[client] no response for 45s, abort socket";
        m_socket->abort();
        if (m_state == Connected)
            onDisconnected();
        return;
    }

    QJsonObject frame;
    frame.insert(QStringLiteral("type"), MsgType::kHeartbeat);
    frame.insert(QStringLiteral("ts"), static_cast<double>(QDateTime::currentMSecsSinceEpoch()));
    frame.insert(QStringLiteral("data"), QJsonObject());
    sendFrame(frame);
    qDebug() << "[client] heartbeat sent";
}

void LunaClient::sendFrame(const QJsonObject &obj)
{
    if (m_state != Connected) {
        qWarning() << "[client] sendFrame ignored, not connected";
        return;
    }
    m_socket->write(FrameCodec::encode(obj));
}