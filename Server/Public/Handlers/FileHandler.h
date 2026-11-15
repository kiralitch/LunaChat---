#pragma once

#include <QHash>
#include <QJsonObject>
#include <QObject>
#include <functional>

class ClientSession;
class QFile;

// 文件传输：file_meta 登记 -> file_chunk 转发 -> file_done 收尾；文件内容不落服务端磁盘
class FileHandler : public QObject
{
    Q_OBJECT

public:
    explicit FileHandler(QObject *parent = nullptr);

    // 由 LunaServer 注入"对方是否在线"的判定，避免把大块数据塞进离线队列
    void setOnlineChecker(std::function<bool(quint64)> checker) { m_isOnline = checker; }

    // 群文件：直接写进群共享文件夹，不再逐块转发
    QHash<QString, QFile *> m_groupFiles;
    QHash<QString, qint64> m_groupOf;
    QHash<QString, QString> m_groupPath;
    QHash<QString, QString> m_groupName;
    QHash<QString, quint64> m_groupSender;

    bool handle(ClientSession *session, const QJsonObject &frame);

signals:
    void frameToUser(quint64 userId, const QJsonObject &frame);

private:
    bool handleMeta(ClientSession *session, const QJsonObject &frame);
    bool handleChunk(ClientSession *session, const QJsonObject &frame);
    bool handleDone(ClientSession *session, const QJsonObject &frame);
    bool handleResp(ClientSession *session, const QJsonObject &frame);
    void sendError(ClientSession *session, int code, const QString &message);

    std::function<bool(quint64)> m_isOnline;
    QHash<QString, quint64> m_targets;   // fileId -> 接收者
    QHash<QString, quint64> m_senders;   // fileId -> 发送者（file_resp 回传用）
};