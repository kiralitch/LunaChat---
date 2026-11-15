#include "Handlers/VoiceHandler.h"

#include <QDateTime>
#include <QtDebug>

#include "LunaDb/DbManager.h"
#include "LunaProtocol/MessageTypes.h"
#include "Network/ClientSession.h"

namespace {
double nowMs()
{
    return static_cast<double>(QDateTime::currentMSecsSinceEpoch());
}
const int kMaxDurationMs = 60 * 1000;        // 单条 ≤60 秒
const qint64 kMaxVoiceBytes = 4LL * 1024 * 1024;   // 60s 的 16kHz WAV 约 1.9MB，留足余量
}

VoiceHandler::VoiceHandler(QObject *parent)
    : QObject(parent)
{
}

void VoiceHandler::sendError(ClientSession *session, int code, const QString &message)
{
    QJsonObject data;
    data.insert(QStringLiteral("code"), code);
    data.insert(QStringLiteral("msg"), message);

    QJsonObject reply;
    reply.insert(QStringLiteral("type"), MsgType::kError);
    reply.insert(QStringLiteral("ts"), nowMs());
    reply.insert(QStringLiteral("data"), data);
    session->sendFrame(reply);
}

bool VoiceHandler::handle(ClientSession *session, const QJsonObject &frame)
{
    const QString type = frame.value(QStringLiteral("type")).toString();
    if (type != MsgType::kVoiceMeta && type != MsgType::kVoiceChunk
        && type != MsgType::kVoiceDone && type != MsgType::kVoiceResp) {
        return false;
    }
    if (session->userId() == 0) {
        sendError(session, 401, QStringLiteral("请先登录"));
        return true;
    }

    if (type == MsgType::kVoiceMeta)
        return handleMeta(session, frame);
    if (type == MsgType::kVoiceChunk)
        return handleChunk(session, frame);
    if (type == MsgType::kVoiceDone)
        return handleDone(session, frame);
    return handleResp(session, frame);
}

bool VoiceHandler::handleMeta(ClientSession *session, const QJsonObject &frame)
{
    const QJsonObject data = frame.value(QStringLiteral("data")).toObject();
    const quint64 toUser = static_cast<quint64>(data.value(QStringLiteral("toUser")).toDouble());
    const QString voiceId = data.value(QStringLiteral("voiceId")).toString();
    const QString fileName = data.value(QStringLiteral("fileName")).toString();
    const int duration = data.value(QStringLiteral("duration")).toInt();
    const qint64 fileSize = static_cast<qint64>(data.value(QStringLiteral("fileSize")).toDouble());
    const QString format = data.value(QStringLiteral("format")).toString(QStringLiteral("wav"));
    const int bitrate = data.value(QStringLiteral("bitrate")).toInt();

    if (voiceId.isEmpty() || fileName.isEmpty() || duration <= 0) {
        sendError(session, 400, QStringLiteral("语音元信息不完整"));
        return true;
    }
    if (duration > kMaxDurationMs) {
        sendError(session, 400, QStringLiteral("单条语音不能超过 60 秒"));
        return true;
    }
    if (fileSize <= 0 || fileSize > kMaxVoiceBytes) {
        sendError(session, 400, QStringLiteral("语音文件大小异常"));
        return true;
    }
    if (toUser == 0 || toUser == session->userId()) {
        sendError(session, 400, QStringLiteral("语音接收方无效"));
        return true;
    }

    DbManager::instance().saveVoiceRecord(voiceId, session->userId(), toUser, fileName,
                                          duration, fileSize, format, bitrate);
    m_targets.insert(voiceId, toUser);
    m_senders.insert(voiceId, session->userId());

    qInfo().noquote() << QStringLiteral("[voice] %1 (%2 ms, %3 bytes, %4) -> user %5")
                             .arg(fileName).arg(duration).arg(fileSize).arg(format).arg(toUser);
    emit frameToUser(toUser, frame);
    return true;
}

bool VoiceHandler::handleChunk(ClientSession *session, const QJsonObject &frame)
{
    const QJsonObject data = frame.value(QStringLiteral("data")).toObject();
    const QString voiceId = data.value(QStringLiteral("voiceId")).toString();
    const quint64 target = m_targets.value(voiceId, 0);
    if (target == 0) {
        sendError(session, 400, QStringLiteral("未知的 voiceId，请先发 voice_meta"));
        return true;
    }
    emit frameToUser(target, frame);
    return true;
}

bool VoiceHandler::handleDone(ClientSession *session, const QJsonObject &frame)
{
    const QJsonObject data = frame.value(QStringLiteral("data")).toObject();
    const QString voiceId = data.value(QStringLiteral("voiceId")).toString();
    const quint64 target = m_targets.value(voiceId, 0);
    if (target == 0) {
        sendError(session, 400, QStringLiteral("未知的 voiceId"));
        return true;
    }
    emit frameToUser(target, frame);
    return true;
}

bool VoiceHandler::handleResp(ClientSession *session, const QJsonObject &frame)
{
    const QJsonObject data = frame.value(QStringLiteral("data")).toObject();
    const QString voiceId = data.value(QStringLiteral("voiceId")).toString();
    const quint64 sender = m_senders.value(voiceId, 0);
    if (sender == 0) {
        sendError(session, 400, QStringLiteral("未知的 voiceId"));
        return true;
    }

    // 接收方回执转给发送方
    emit frameToUser(sender, frame);
    m_targets.remove(voiceId);
    m_senders.remove(voiceId);
    return true;
}