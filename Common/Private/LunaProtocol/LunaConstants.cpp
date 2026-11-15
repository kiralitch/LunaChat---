#include "LunaProtocol/LunaConstants.h"

namespace LunaConst {

const quint16 kServerPort = 8888;
const quint32 kMaxFrameBytes = 1024u * 1024u;
const quint32 kChunkRawBytes = 64u * 1024u;
const int kHeartbeatIntervalMs = 15000;
const int kHeartbeatTimeoutMs = 45000;
const int kReconnectBackoffMs[4] = { 1000, 2000, 4000, 8000 };

}