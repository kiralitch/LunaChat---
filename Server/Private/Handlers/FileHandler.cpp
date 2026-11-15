#include "Handlers/FileHandler.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QtDebug>

#include "LunaDb/DbManager.h"
#include "LunaProtocol/LunaConstants.h"
#include "LunaProtocol/MessageTypes.h"
#include "Network/ClientSession.h"

namespace {
double nowMs()
{
    return static_cast<double>(QDateTime::currentMSecsSinceEpoch());
}
const qint64 kMaxFileBytes = 10LL * 1024 * 1024;   // 单文件上限 10MB
}

FileHandler::FileHandler(QObject *parent)
    : QObject(parent)
{
}

void FileHandler::sendError(ClientSession *session, int code, const QString &message)
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

bool FileHandler::handle(ClientSession *session, const QJsonObject &frame)
{
    const QString type = frame.value(QStringLiteral("type")).toString();
    if (type != MsgType::kFileMeta && type != MsgType::kFileChunk
        && type != MsgType::kFileDone && type != MsgType::kFileResp) {
        return false;
    }
    if (session->userId() == 0) {
        sendError(session, 401, QStringLiteral("请先登录"));
        return true;
    }

    if (type == MsgType::kFileMeta)
        return handleMeta(session, frame);
    if (type == MsgType::kFileChunk)
        return handleChunk(session, frame);
    if (type == MsgType::kFileDone)
        return handleDone(session, frame);
    return handleResp(session, frame);
}

bool FileHandler::handleMeta(ClientSession *session, const QJsonObject &frame)
{
    const QJsonObject data = frame.value(QStringLiteral("data")).toObject();
    const quint64 toUser = static_cast<quint64>(data.value(QStringLiteral("toUser")).toDouble());
    const QString fileName = data.value(QStringLiteral("fileName")).toString();
    const qint64 fileSize = static_cast<qint64>(data.value(QStringLiteral("fileSize")).toDouble());
    const QString fileId = data.value(QStringLiteral("fileId")).toString();
    const qint64 groupId = static_cast<qint64>(data.value(QStringLiteral("groupId")).toDouble());

    if (fileId.isEmpty() || fileName.isEmpty() || fileSize <= 0) {
        sendError(session, 400, QStringLiteral("文件元信息不完整"));
        return true;
    }
    if (fileSize > kMaxFileBytes) {
        sendError(session, 413, QStringLiteral("文件超过 10MB 上限"));
        return true;
    }
    // 群文件：服务端直接存进该群共享文件夹，并广播给群成员
    if (groupId > 0) {
        if (!DbManager::instance().isGroupMember(groupId, session->userId())) {
            sendError(session, 403, QStringLiteral("你不在该群中"));
            return true;
        }

        QString folder = DbManager::instance().groupInfo(groupId, session->userId())
                             .value(QStringLiteral("folderPath")).toString();
        if (folder.isEmpty())
            folder = QCoreApplication::applicationDirPath() + QStringLiteral("/group_files/%1").arg(groupId);
        QDir().mkpath(folder);

        const QString path = QDir(folder).filePath(fileName);
        auto *file = new QFile(path);
        if (!file->open(QIODevice::WriteOnly)) {
            delete file;
            sendError(session, 500, QStringLiteral("无法写入群共享文件夹"));
            return true;
        }

        DbManager::instance().saveFileRecord(fileId, fileName, fileSize, session->userId(), 0);
        m_groupFiles.insert(fileId, file);
        m_groupOf.insert(fileId, groupId);
        m_groupPath.insert(fileId, path);
        m_groupName.insert(fileId, fileName);
        m_groupSender.insert(fileId, session->userId());

        qInfo().noquote() << QStringLiteral("[file] group %1 upload %2 -> %3")
                                 .arg(groupId).arg(fileName, path);
        return true;
    }

    if (!m_isOnline || !m_isOnline(toUser)) {
        // 文件不进离线队列：几十万条 base64 块塞数据库没有意义
        sendError(session, 409, QStringLiteral("对方不在线，暂时无法发送文件"));
        return true;
    }

    if (!DbManager::instance().saveFileRecord(fileId, fileName, fileSize, session->userId(), toUser)) {
        sendError(session, 500, QStringLiteral("文件记录写入失败"));
        return true;
    }

    m_targets.insert(fileId, toUser);
    m_senders.insert(fileId, session->userId());

    qInfo().noquote() << QStringLiteral("[file] %1 (%2 bytes) -> user %3, id=%4")
                             .arg(fileName).arg(fileSize).arg(toUser).arg(fileId);
    emit frameToUser(toUser, frame);
    return true;
}

bool FileHandler::handleChunk(ClientSession *session, const QJsonObject &frame)
{
    const QJsonObject data = frame.value(QStringLiteral("data")).toObject();
    const QString fileId = data.value(QStringLiteral("fileId")).toString();

    // 群文件：落盘
    if (m_groupFiles.contains(fileId)) {
        QFile *file = m_groupFiles.value(fileId);
        const QByteArray chunk = QByteArray::fromBase64(
            data.value(QStringLiteral("chunkBase64")).toString().toLatin1());
        file->write(chunk);
        return true;
    }

    const quint64 target = m_targets.value(fileId, 0);
    if (target == 0) {
        sendError(session, 400, QStringLiteral("未知的 fileId，请先发 file_meta"));
        return true;
    }
    emit frameToUser(target, frame);
    return true;
}

bool FileHandler::handleDone(ClientSession *session, const QJsonObject &frame)
{
    const QJsonObject data = frame.value(QStringLiteral("data")).toObject();
    const QString fileId = data.value(QStringLiteral("fileId")).toString();

    // 群文件收尾：关闭文件、记路径、广播给群成员
    if (m_groupFiles.contains(fileId)) {
        QFile *file = m_groupFiles.take(fileId);
        file->close();
        file->deleteLater();

        const qint64 groupId = m_groupOf.take(fileId);
        const QString path = m_groupPath.take(fileId);
        const QString name = m_groupName.take(fileId);
        m_groupSender.remove(fileId);

        DbManager::instance().updateFileSavedPath(fileId, path);

        QJsonObject share;
        share.insert(QStringLiteral("groupId"), static_cast<double>(groupId));
        share.insert(QStringLiteral("fileId"), fileId);
        share.insert(QStringLiteral("fileName"), name);
        share.insert(QStringLiteral("savedPath"), path);
        share.insert(QStringLiteral("fromUser"), static_cast<double>(session->userId()));
        share.insert(QStringLiteral("nickname"), session->nickname());

        QJsonObject shareFrame;
        shareFrame.insert(QStringLiteral("type"), MsgType::kFileShared);
        shareFrame.insert(QStringLiteral("ts"), nowMs());
        shareFrame.insert(QStringLiteral("data"), share);

        for (quint64 memberId : DbManager::instance().groupMemberIds(groupId))
            emit frameToUser(memberId, shareFrame);
        qInfo().noquote() << QStringLiteral("[file] group %1 shared %2").arg(groupId).arg(name);
        return true;
    }

    const quint64 target = m_targets.value(fileId, 0);
    if (target == 0) {
        sendError(session, 400, QStringLiteral("未知的 fileId"));
        return true;
    }
    emit frameToUser(target, frame);
    return true;
}

bool FileHandler::handleResp(ClientSession *session, const QJsonObject &frame)
{
    const QJsonObject data = frame.value(QStringLiteral("data")).toObject();
    const QString fileId = data.value(QStringLiteral("fileId")).toString();
    const quint64 sender = m_senders.value(fileId, 0);
    if (sender == 0) {
        sendError(session, 400, QStringLiteral("未知的 fileId"));
        return true;
    }

    // 接收方回执转给发送方，发送方据此结束进度条
    emit frameToUser(sender, frame);
    m_targets.remove(fileId);
    m_senders.remove(fileId);
    return true;
}