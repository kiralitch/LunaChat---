#pragma once

#include <QHash>
#include <QJsonObject>
#include <QObject>
#include <QStringList>

class ClientSession;
class QTimer;

// 随机匹配：文字/语音两条队列，配对成功建临时会话，支持消息转发、取消、超时、结束
class MatchHandler : public QObject
{
    Q_OBJECT

public:
    explicit MatchHandler(QObject *parent = nullptr);

    bool handle(ClientSession *session, const QJsonObject &frame);

    // 会话断开时由 LunaServer 调用：出队 + 结束其匹配会话
    void onUserOffline(quint64 userId);

signals:
    void frameToUser(quint64 userId, const QJsonObject &frame);

private:
    bool handleStart(ClientSession *session, const QJsonObject &frame);
    bool handleCancel(ClientSession *session, const QJsonObject &frame);
    bool handleMessage(ClientSession *session, const QJsonObject &frame);
    bool handleEnd(ClientSession *session, const QJsonObject &frame);

    bool tryMatch(ClientSession *session, const QString &mode);
    void notifyTimeout(quint64 userId);
    void endSessionInternal(const QString &sessionId, quint64 notifyUser, const QString &reason);
    void removeFromQueues(quint64 userId);
    void sendError(ClientSession *session, int code, const QString &message);

    QStringList m_textQueue;      // 用 userId 字符串保存，顺序即排队顺序
    QStringList m_voiceQueue;
    QHash<quint64, QString> m_names;
    QHash<quint64, QString> m_sessionOf;                 // userId -> sessionId
    QHash<QString, QPair<quint64, quint64>> m_sessions;  // sessionId -> (a, b)
    QHash<QString, QString> m_modes;                     // sessionId -> text/voice
    QHash<quint64, QTimer *> m_timeouts;
};