#pragma once

#include <QHash>
#include <QJsonObject>
#include <QObject>

class ClientSession;

// 语音：voice_meta 登记 -> voice_chunk 转发 -> voice_done 收尾 -> voice_resp 回执
class VoiceHandler : public QObject
{
    Q_OBJECT

public:
    explicit VoiceHandler(QObject *parent = nullptr);

    bool handle(ClientSession *session, const QJsonObject &frame);

signals:
    void frameToUser(quint64 userId, const QJsonObject &frame);

private:
    bool handleMeta(ClientSession *session, const QJsonObject &frame);
    bool handleChunk(ClientSession *session, const QJsonObject &frame);
    bool handleDone(ClientSession *session, const QJsonObject &frame);
    bool handleResp(ClientSession *session, const QJsonObject &frame);
    void sendError(ClientSession *session, int code, const QString &message);

    QHash<QString, quint64> m_targets;   // voiceId -> 接收者
    QHash<QString, quint64> m_senders;   // voiceId -> 发送者
};