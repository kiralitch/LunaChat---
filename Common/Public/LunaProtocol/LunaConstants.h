#pragma once

#include <QtGlobal>

// 协议与网络常量集中定义：Server / Client 共用同一份，避免两端魔数不一致
namespace LunaConst {

// 服务端监听端口（局域网演示默认 8888，避开 MySQL 3306）
extern const quint16 kServerPort;

// 单帧 JSON 负载上限 1MB：超限直接断开，防止恶意大包撑爆内存
extern const quint32 kMaxFrameBytes;

// 文件/语音分块的原始数据上限 64KB（base64 后约 87KB，仍在 1MB 帧内）
extern const quint32 kChunkRawBytes;

// 心跳：客户端 15s 发一次；服务端 45s 收不到判定掉线
extern const int kHeartbeatIntervalMs;
extern const int kHeartbeatTimeoutMs;

// 断线重连退避序列（毫秒），用完最后一项后保持该间隔，上限 30s 另由逻辑控制
extern const int kReconnectBackoffMs[4];

}