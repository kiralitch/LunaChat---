#include "LunaProtocol/FrameCodec.h"

#include <QJsonDocument>
#include <QJsonParseError>

#include "LunaProtocol/LunaConstants.h"

QByteArray FrameCodec::encode(const QJsonObject &obj)
{
    const QByteArray payload = QJsonDocument(obj).toJson(QJsonDocument::Compact);

    // 长度前缀只表示 JSON 负载字节数，不含前缀自身；超限直接拒绝
    if (static_cast<quint32>(payload.size()) > LunaConst::kMaxFrameBytes)
        return QByteArray();

    const quint32 len = static_cast<quint32>(payload.size());

    QByteArray frame;
    frame.reserve(4 + payload.size());
    // 4 字节大端写入长度：高字节在前，与协议文档一致
    frame.append(static_cast<char>((len >> 24) & 0xFF));
    frame.append(static_cast<char>((len >> 16) & 0xFF));
    frame.append(static_cast<char>((len >> 8) & 0xFF));
    frame.append(static_cast<char>(len & 0xFF));
    frame.append(payload);
    return frame;
}

void FrameCodec::append(const QByteArray &bytes)
{
    m_buffer.append(bytes);
}

FrameCodec::Result FrameCodec::nextFrame(QJsonObject &out)
{
    // 不足 4 字节连长度都读不到，属于半包
    if (m_buffer.size() < 4)
        return NeedMoreData;

    const quint32 len = (static_cast<quint32>(static_cast<quint8>(m_buffer.at(0))) << 24)
                      | (static_cast<quint32>(static_cast<quint8>(m_buffer.at(1))) << 16)
                      | (static_cast<quint32>(static_cast<quint8>(m_buffer.at(2))) << 8)
                      |  static_cast<quint32>(static_cast<quint8>(m_buffer.at(3)));

    if (len == 0) {
        m_buffer.remove(0, 4);   // 空负载视为坏帧，丢掉前缀继续
        return InvalidJson;
    }
    if (len > LunaConst::kMaxFrameBytes) {
        m_buffer.clear();
        return FrameTooLarge;
    }

    // 长度已知但负载还没到齐：保留缓冲，等下一次 append（半包的核心分支）
    if (m_buffer.size() < static_cast<int>(4 + len))
        return NeedMoreData;

    const QByteArray payload = m_buffer.mid(4, static_cast<int>(len));
    m_buffer.remove(0, static_cast<int>(4 + len));   // 只消费这一帧，后面粘着的帧留在缓冲区

    QJsonParseError error;
    const QJsonDocument doc = QJsonDocument::fromJson(payload, &error);
    if (error.error != QJsonParseError::NoError || !doc.isObject())
        return InvalidJson;

    out = doc.object();
    return FrameDecoded;
}