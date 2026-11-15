#pragma once

#include <QByteArray>
#include <QJsonObject>

// 帧编解码：[4 字节大端 uint32 长度 L][L 字节 UTF-8 JSON]
// 收发两端共用：encode() 打包；append()+nextFrame() 负责拆包（半包、粘包都在这里处理）
class FrameCodec
{
public:
    enum Result {
        NeedMoreData = 0,   // 半包：数据没到齐，继续等下一次 readyRead
        FrameDecoded,       // 成功取出一帧
        FrameTooLarge,      // 长度前缀超过 1MB 上限，按协议错误处理
        InvalidJson         // 长度够但负载不是合法 JSON 对象
    };

    // 对象 -> 完整帧（含 4 字节长度前缀）；负载超过上限时返回空 QByteArray
    static QByteArray encode(const QJsonObject &obj);

    // 把 socket 上读到的裸字节塞进来，内部累积（调用方不再自己拼包）
    void append(const QByteArray &bytes);

    // 从缓冲区头部切出一帧；要循环调用直到返回 NeedMoreData
    Result nextFrame(QJsonObject &out);

    void clear() { m_buffer.clear(); }
    int bufferedBytes() const { return m_buffer.size(); }

private:
    QByteArray m_buffer;   // 尚未消费的字节：半包时就是上一帧的残留
};