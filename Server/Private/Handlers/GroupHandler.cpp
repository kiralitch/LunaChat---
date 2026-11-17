#include "Handlers/GroupHandler.h"

#include <QDateTime>
#include <QCoreApplication>
#include <QDir>
#include <QRegularExpression>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QtDebug>

#include "LunaDb/DbManager.h"
#include "LunaProtocol/MessageTypes.h"
#include "Network/ClientSession.h"

namespace {
double nowMs()
{
    return static_cast<double>(QDateTime::currentMSecsSinceEpoch());
}
}

GroupHandler::GroupHandler(QObject *parent)
    : QObject(parent)
{
}

bool GroupHandler::handle(ClientSession *session, const QJsonObject &frame)
{
    const QString type = frame.value(QStringLiteral("type")).toString();
    const bool known = (type == MsgType::kGroupCreate || type == MsgType::kGroupList
                        || type == MsgType::kGroupInvite || type == MsgType::kGroupInfoGet
                        || type == MsgType::kGroupInfoUpdate || type == MsgType::kGroupLeave
                        || type == MsgType::kGroupTransfer || type == MsgType::kGroupDissolve || type == MsgType::kGroupAvatarGet
                        || type == MsgType::kGroupFilesList || type == MsgType::kGroupFileMkdir
                        || type == MsgType::kGroupFileDelete || type == MsgType::kGroupFileDownload);
    if (!known)
        return false;

    if (session->userId() == 0) {
        sendError(session, 401, QStringLiteral("请先登录"));
        return true;
    }

    if (type == MsgType::kGroupCreate)
        return handleCreate(session, frame);
    if (type == MsgType::kGroupList)
        return handleList(session, frame);
    if (type == MsgType::kGroupInvite)
        return handleInvite(session, frame);
    if (type == MsgType::kGroupInfoGet)
        return handleInfo(session, frame);
    if (type == MsgType::kGroupInfoUpdate)
        return handleInfoUpdate(session, frame);
    if (type == MsgType::kGroupLeave)
        return handleLeave(session, frame);
    if (type == MsgType::kGroupTransfer)
        return handleTransfer(session, frame);
    if (type == MsgType::kGroupAvatarGet)
        return handleAvatarGet(session, frame);
    if (type == MsgType::kGroupFilesList)
        return handleFilesList(session, frame);
    if (type == MsgType::kGroupFileMkdir)
        return handleMkdir(session, frame);
    if (type == MsgType::kGroupFileDelete)
        return handleDeleteFile(session, frame);
    if (type == MsgType::kGroupFileDownload)
        return handleDownload(session, frame);
    return handleDissolve(session, frame);
}

void GroupHandler::sendError(ClientSession *session, int code, const QString &message)
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

bool GroupHandler::handleCreate(ClientSession *session, const QJsonObject &frame)
{
    const QJsonObject data = frame.value(QStringLiteral("data")).toObject();
    const QString name = data.value(QStringLiteral("name")).toString().trimmed();

    QJsonObject replyData;
    QJsonObject reply;
    reply.insert(QStringLiteral("type"), MsgType::kGroupCreateResp);
    reply.insert(QStringLiteral("ts"), nowMs());

    if (name.isEmpty() || name.size() > 32) {
        replyData.insert(QStringLiteral("ok"), false);
        replyData.insert(QStringLiteral("msg"), QStringLiteral("群名称需为 1-32 个字符"));
        reply.insert(QStringLiteral("data"), replyData);
        session->sendFrame(reply);
        return true;
    }

    const qint64 groupId = DbManager::instance().createGroup(name, session->userId());
    if (groupId <= 0) {
        replyData.insert(QStringLiteral("ok"), false);
        replyData.insert(QStringLiteral("msg"), QStringLiteral("建群失败"));
        reply.insert(QStringLiteral("data"), replyData);
        session->sendFrame(reply);
        return true;
    }

    qInfo().noquote() << QStringLiteral("[group] created %1 (id=%2) by %3")
                             .arg(name).arg(groupId).arg(session->nickname());

    replyData.insert(QStringLiteral("ok"), true);
    replyData.insert(QStringLiteral("msg"), QStringLiteral("建群成功"));
    replyData.insert(QStringLiteral("groupId"), static_cast<double>(groupId));
    replyData.insert(QStringLiteral("name"), name);
    reply.insert(QStringLiteral("data"), replyData);
    session->sendFrame(reply);
    return true;
}

bool GroupHandler::handleList(ClientSession *session, const QJsonObject &frame)
{
    Q_UNUSED(frame)
    QJsonObject data;
    data.insert(QStringLiteral("groups"), DbManager::instance().groupsOf(session->userId()));

    QJsonObject reply;
    reply.insert(QStringLiteral("type"), MsgType::kGroupListResp);
    reply.insert(QStringLiteral("ts"), nowMs());
    reply.insert(QStringLiteral("data"), data);
    session->sendFrame(reply);
    return true;
}

bool GroupHandler::handleInvite(ClientSession *session, const QJsonObject &frame)
{
    const QJsonObject data = frame.value(QStringLiteral("data")).toObject();
    const qint64 groupId = static_cast<qint64>(data.value(QStringLiteral("groupId")).toDouble());
    const quint64 targetId = static_cast<quint64>(data.value(QStringLiteral("userId")).toDouble());

    QJsonObject replyData;
    QJsonObject reply;
    reply.insert(QStringLiteral("type"), MsgType::kGroupInviteResp);
    reply.insert(QStringLiteral("ts"), nowMs());

    // 权限校验：只有群主/管理员可以拉人
    if (!DbManager::instance().isGroupAdmin(groupId, session->userId())) {
        replyData.insert(QStringLiteral("ok"), false);
        replyData.insert(QStringLiteral("msg"), QStringLiteral("只有群主或管理员可以邀请成员"));
        reply.insert(QStringLiteral("data"), replyData);
        session->sendFrame(reply);
        return true;
    }

    if (targetId == 0 || !DbManager::instance().addGroupMember(groupId, targetId)) {
        replyData.insert(QStringLiteral("ok"), false);
        replyData.insert(QStringLiteral("msg"), QStringLiteral("邀请失败"));
        reply.insert(QStringLiteral("data"), replyData);
        session->sendFrame(reply);
        return true;
    }

    qInfo().noquote() << QStringLiteral("[group] invite user %1 -> group %2 by %3")
                             .arg(targetId).arg(groupId).arg(session->nickname());
    replyData.insert(QStringLiteral("ok"), true);
    replyData.insert(QStringLiteral("msg"), QStringLiteral("邀请成功"));
    replyData.insert(QStringLiteral("groupId"), static_cast<double>(groupId));
    replyData.insert(QStringLiteral("userId"), static_cast<double>(targetId));
    reply.insert(QStringLiteral("data"), replyData);
    session->sendFrame(reply);

    // 主动把最新群列表推给被邀请者：他那边不用重登也能看到新群
    QJsonObject pushData;
    pushData.insert(QStringLiteral("groups"), DbManager::instance().groupsOf(targetId));
    QJsonObject pushFrame;
    pushFrame.insert(QStringLiteral("type"), MsgType::kGroupListResp);
    pushFrame.insert(QStringLiteral("ts"), nowMs());
    pushFrame.insert(QStringLiteral("data"), pushData);
    emit frameToUser(targetId, pushFrame);
    return true;
}
// ================= 群资料与群生命周期 =================

void GroupHandler::pushGroupListTo(quint64 userId)
{
    QJsonObject data;
    data.insert(QStringLiteral("groups"), DbManager::instance().groupsOf(userId));

    QJsonObject frame;
    frame.insert(QStringLiteral("type"), MsgType::kGroupListResp);
    frame.insert(QStringLiteral("ts"), nowMs());
    frame.insert(QStringLiteral("data"), data);
    emit frameToUser(userId, frame);
}

void GroupHandler::sendGroupInfo(ClientSession *session, qint64 groupId, bool ok, const QString &msg)
{
    QJsonObject data;
    data.insert(QStringLiteral("ok"), ok);
    data.insert(QStringLiteral("msg"), msg);
    if (groupId > 0) {
        data.insert(QStringLiteral("info"),
                    DbManager::instance().groupInfo(groupId, session->userId()));
        data.insert(QStringLiteral("members"),
                    DbManager::instance().groupMembers(groupId));
        data.insert(QStringLiteral("files"),
                    DbManager::instance().filesOfGroup(groupId));
    }

    QJsonObject reply;
    reply.insert(QStringLiteral("type"), MsgType::kGroupInfoResp);
    reply.insert(QStringLiteral("ts"), nowMs());
    reply.insert(QStringLiteral("data"), data);
    session->sendFrame(reply);
}

bool GroupHandler::handleInfo(ClientSession *session, const QJsonObject &frame)
{
    const qint64 groupId = static_cast<qint64>(frame.value(QStringLiteral("data")).toObject()
                                                   .value(QStringLiteral("groupId")).toDouble());
    const QJsonObject info = DbManager::instance().groupInfo(groupId, session->userId());
    if (info.isEmpty() || info.value(QStringLiteral("role")).toString().isEmpty()) {
        sendError(session, 403, QStringLiteral("你不在该群组中"));
        return true;
    }
    // 查询结果只回 info + members，不带 ok：客户端靠 "有没有 ok" 区分"回执"与"查询结果"
    QJsonObject replyData;
    replyData.insert(QStringLiteral("info"), DbManager::instance().groupInfo(groupId, session->userId()));
    replyData.insert(QStringLiteral("members"), DbManager::instance().groupMembers(groupId));
    replyData.insert(QStringLiteral("files"), DbManager::instance().filesOfGroup(groupId));

    QJsonObject reply;
    reply.insert(QStringLiteral("type"), MsgType::kGroupInfoResp);
    reply.insert(QStringLiteral("ts"), nowMs());
    reply.insert(QStringLiteral("data"), replyData);
    session->sendFrame(reply);
    return true;
}

bool GroupHandler::handleInfoUpdate(ClientSession *session, const QJsonObject &frame)
{
    const QJsonObject data = frame.value(QStringLiteral("data")).toObject();
    const qint64 groupId = static_cast<qint64>(data.value(QStringLiteral("groupId")).toDouble());
    const QString name = data.value(QStringLiteral("name")).toString().trimmed();
    const QString announcement = data.value(QStringLiteral("announcement")).toString();
    QString folderPath = data.value(QStringLiteral("folderPath")).toString().trimmed();

    if (!DbManager::instance().isGroupAdmin(groupId, session->userId())) {
        sendError(session, 403, QStringLiteral("只有群主或管理员可以修改群资料"));
        return true;
    }
    if (name.isEmpty() || name.size() > 32) {
        sendError(session, 400, QStringLiteral("群名称需为 1-32 个字符"));
        return true;
    }
    if (folderPath.isEmpty())
        folderPath = QCoreApplication::applicationDirPath()
                     + QStringLiteral("/group_files/%1").arg(groupId);

    const QJsonObject oldInfo = DbManager::instance().groupInfo(groupId, session->userId());
    QString avatarPath = oldInfo.value(QStringLiteral("avatarPath")).toString();

    const QString avatarBase64 = data.value(QStringLiteral("avatarBase64")).toString();
    if (!avatarBase64.isEmpty()) {
        const QByteArray bytes = QByteArray::fromBase64(avatarBase64.toLatin1());
        if (bytes.size() > 200 * 1024) {
            sendError(session, 413, QStringLiteral("群头像文件过大"));
            return true;
        }
        const QString path = QCoreApplication::applicationDirPath()
                             + QStringLiteral("/group_avatars/%1.jpg").arg(groupId);
        QDir().mkpath(QFileInfo(path).absolutePath());
        QFile file(path);
        if (file.open(QIODevice::WriteOnly)) {
            file.write(bytes);
            file.close();
            avatarPath = path;
        }
    }

    QDir().mkpath(folderPath);
    if (!DbManager::instance().updateGroupInfo(groupId, name, announcement, folderPath, avatarPath)) {
        sendError(session, 500, QStringLiteral("群资料保存失败"));
        return true;
    }

    qInfo().noquote() << QStringLiteral("[group] info updated %1 (id=%2)").arg(name).arg(groupId);
    sendGroupInfo(session, groupId, true, QStringLiteral("群资料已更新"));
    for (quint64 memberId : DbManager::instance().groupMemberIds(groupId))
        pushGroupListTo(memberId);
    return true;
}

bool GroupHandler::handleLeave(ClientSession *session, const QJsonObject &frame)
{
    const qint64 groupId = static_cast<qint64>(frame.value(QStringLiteral("data")).toObject()
                                                   .value(QStringLiteral("groupId")).toDouble());

    if (DbManager::instance().groupOwnerId(groupId) == static_cast<qint64>(session->userId())) {
        sendError(session, 409, QStringLiteral("群主请先把群主转让给其他成员再退群"));
        return true;
    }
    if (!DbManager::instance().removeGroupMember(groupId, session->userId())) {
        sendError(session, 400, QStringLiteral("退群失败（你不在该群）"));
        return true;
    }

    qInfo().noquote() << QStringLiteral("[group] user %1 left group %2").arg(session->userId()).arg(groupId);
    sendGroupInfo(session, 0, true, QStringLiteral("已退出群聊"));
    pushGroupListTo(session->userId());
    return true;
}

bool GroupHandler::handleTransfer(ClientSession *session, const QJsonObject &frame)
{
    const QJsonObject data = frame.value(QStringLiteral("data")).toObject();
    const qint64 groupId = static_cast<qint64>(data.value(QStringLiteral("groupId")).toDouble());
    const quint64 newOwner = static_cast<quint64>(data.value(QStringLiteral("userId")).toDouble());

    if (DbManager::instance().groupOwnerId(groupId) != static_cast<qint64>(session->userId())) {
        sendError(session, 403, QStringLiteral("只有群主可以转让群主"));
        return true;
    }
    if (!DbManager::instance().isGroupMember(groupId, newOwner)) {
        sendError(session, 400, QStringLiteral("对方不在该群中"));
        return true;
    }
    if (!DbManager::instance().transferGroupOwner(groupId, newOwner)) {
        sendError(session, 500, QStringLiteral("转让失败"));
        return true;
    }

    qInfo().noquote() << QStringLiteral("[group] owner of %1 transferred to %2").arg(groupId).arg(newOwner);
    sendGroupInfo(session, groupId, true, QStringLiteral("群主已转让"));
    pushGroupListTo(session->userId());
    pushGroupListTo(newOwner);
    return true;
}

bool GroupHandler::handleDissolve(ClientSession *session, const QJsonObject &frame)
{
    const qint64 groupId = static_cast<qint64>(frame.value(QStringLiteral("data")).toObject()
                                                   .value(QStringLiteral("groupId")).toDouble());

    if (DbManager::instance().groupOwnerId(groupId) != static_cast<qint64>(session->userId())) {
        sendError(session, 403, QStringLiteral("只有群主可以解散群聊"));
        return true;
    }

    const QList<quint64> members = DbManager::instance().groupMemberIds(groupId);
    if (!DbManager::instance().dissolveGroup(groupId)) {
        sendError(session, 500, QStringLiteral("解散失败"));
        return true;
    }

    qInfo().noquote() << QStringLiteral("[group] dissolved %1 by %2").arg(groupId).arg(session->userId());
    sendGroupInfo(session, 0, true, QStringLiteral("群聊已解散"));
    for (quint64 memberId : members)
        pushGroupListTo(memberId);
    return true;
}
bool GroupHandler::handleAvatarGet(ClientSession *session, const QJsonObject &frame)
{
    const qint64 groupId = static_cast<qint64>(frame.value(QStringLiteral("data")).toObject()
                                                   .value(QStringLiteral("groupId")).toDouble());
    QByteArray bytes;
    QFile file(QCoreApplication::applicationDirPath()
               + QStringLiteral("/group_avatars/%1.jpg").arg(groupId));
    if (file.open(QIODevice::ReadOnly)) {
        bytes = file.readAll();
        file.close();
    }

    QJsonObject data;
    data.insert(QStringLiteral("groupId"), static_cast<double>(groupId));
    data.insert(QStringLiteral("data"), QString::fromLatin1(bytes.toBase64()));

    QJsonObject reply;
    reply.insert(QStringLiteral("type"), MsgType::kGroupAvatarResp);
    reply.insert(QStringLiteral("ts"), nowMs());
    reply.insert(QStringLiteral("data"), data);
    session->sendFrame(reply);
    return true;
}
// ================= 群共享文件夹 =================

namespace {
QString groupFolder(qint64 groupId, quint64 userId)
{
    QString folder = DbManager::instance().groupInfo(groupId, userId)
                         .value(QStringLiteral("folderPath")).toString();
    if (folder.isEmpty())
        folder = QCoreApplication::applicationDirPath()
                 + QStringLiteral("/group_files/%1").arg(groupId);
    QDir().mkpath(folder);
    return folder;
}
}


bool GroupHandler::handleFilesList(ClientSession *session, const QJsonObject &frame)
{
    const QJsonObject req = frame.value(QStringLiteral("data")).toObject();
    const qint64 groupId = static_cast<qint64>(req.value(QStringLiteral("groupId")).toDouble());
    if (!DbManager::instance().isGroupMember(groupId, session->userId())) {
        sendError(session, 403, QStringLiteral("你不在该群中"));
        return true;
    }

    const QString base = groupFolder(groupId, session->userId());
    QString rel = req.value(QStringLiteral("path")).toString();
    if (rel.contains(QStringLiteral("..")))
        rel.clear();
    QString folder = rel.isEmpty() ? base : QDir(base).filePath(rel);
    if (!QDir(folder).exists()) {
        rel.clear();
        folder = base;
    }

    QJsonArray entries;
    const QFileInfoList infos = QDir(folder).entryInfoList(QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot,
                                                           QDir::DirsFirst | QDir::Name);
    for (const QFileInfo &info : infos) {
        QJsonObject item;
        item.insert(QStringLiteral("name"), info.fileName());
        item.insert(QStringLiteral("dir"), info.isDir());
        item.insert(QStringLiteral("size"), static_cast<double>(info.size()));
        item.insert(QStringLiteral("modified"),
                    info.lastModified().toString(QStringLiteral("yyyy-MM-dd HH:mm")));
        entries.append(item);
    }

    QJsonObject data;
    data.insert(QStringLiteral("groupId"), static_cast<double>(groupId));
    data.insert(QStringLiteral("folderPath"), folder);
    data.insert(QStringLiteral("path"), rel);
    data.insert(QStringLiteral("entries"), entries);

    QJsonObject reply;
    reply.insert(QStringLiteral("type"), MsgType::kGroupFilesResp);
    reply.insert(QStringLiteral("ts"), nowMs());
    reply.insert(QStringLiteral("data"), data);
    session->sendFrame(reply);
    return true;
}

bool GroupHandler::handleMkdir(ClientSession *session, const QJsonObject &frame)
{
    const QJsonObject req = frame.value(QStringLiteral("data")).toObject();
    const qint64 groupId = static_cast<qint64>(req.value(QStringLiteral("groupId")).toDouble());
    const QString name = req.value(QStringLiteral("name")).toString().trimmed();

    if (!DbManager::instance().isGroupAdmin(groupId, session->userId())) {
        sendError(session, 403, QStringLiteral("只有群主或管理员可以新建文件夹"));
        return true;
    }
    if (name.isEmpty() || name.contains(QRegExp(QStringLiteral("[\\\\\\\\/:*?<>|]")))) {
        sendError(session, 400, QStringLiteral("文件夹名不合法"));
        return true;
    }

    QString rel = req.value(QStringLiteral("path")).toString();
    if (rel.contains(QStringLiteral("..")))
        rel.clear();
    const QString base = groupFolder(groupId, session->userId());
    const QString folder = rel.isEmpty() ? base : QDir(base).filePath(rel);
    if (!QDir(folder).mkpath(name)) {
        sendError(session, 500, QStringLiteral("创建文件夹失败"));
        return true;
    }

    QJsonObject data;
    data.insert(QStringLiteral("ok"), true);
    QJsonObject reply;
    reply.insert(QStringLiteral("type"), MsgType::kGroupFileMkdir);
    reply.insert(QStringLiteral("ts"), nowMs());
    reply.insert(QStringLiteral("data"), data);
    session->sendFrame(reply);
    return true;
}

bool GroupHandler::handleDeleteFile(ClientSession *session, const QJsonObject &frame)
{
    const QJsonObject req = frame.value(QStringLiteral("data")).toObject();
    const qint64 groupId = static_cast<qint64>(req.value(QStringLiteral("groupId")).toDouble());
    const QString rel = req.value(QStringLiteral("path")).toString().trimmed();

    if (!DbManager::instance().isGroupAdmin(groupId, session->userId())) {
        sendError(session, 403, QStringLiteral("只有群主或管理员可以删除"));
        return true;
    }
    if (rel.isEmpty() || rel.contains(QStringLiteral(".."))) {
        sendError(session, 400, QStringLiteral("路径不合法"));
        return true;
    }

    const QString base = groupFolder(groupId, session->userId());
    const QString target = QDir(base).filePath(rel);

    bool ok = false;
    const QFileInfo info(target);
    if (info.isDir())
        ok = QDir(target).removeRecursively();
    else if (info.exists())
        ok = QFile::remove(target);

    QJsonObject data;
    data.insert(QStringLiteral("ok"), ok);
    data.insert(QStringLiteral("msg"), ok ? QStringLiteral("已删除")
                                          : QStringLiteral("删除失败（文件不存在？）"));
    QJsonObject reply;
    reply.insert(QStringLiteral("type"), MsgType::kGroupFileDeleteResp);
    reply.insert(QStringLiteral("ts"), nowMs());
    reply.insert(QStringLiteral("data"), data);
    session->sendFrame(reply);
    return true;
}
bool GroupHandler::handleDownload(ClientSession *session, const QJsonObject &frame)
{
    const QJsonObject req = frame.value(QStringLiteral("data")).toObject();
    const qint64 groupId = static_cast<qint64>(req.value(QStringLiteral("groupId")).toDouble());
    const QString rel = req.value(QStringLiteral("path")).toString().trimmed();

    if (!DbManager::instance().isGroupMember(groupId, session->userId())) {
        sendError(session, 403, QStringLiteral("你不在该群中"));
        return true;
    }
    if (rel.isEmpty() || rel.contains(QStringLiteral(".."))) {
        sendError(session, 400, QStringLiteral("路径不合法"));
        return true;
    }

    const QString base = groupFolder(groupId, session->userId());
    const QString target = QDir(base).filePath(rel);
    QFile file(target);
    if (!file.open(QIODevice::ReadOnly)) {
        sendError(session, 404, QStringLiteral("文件不存在"));
        return true;
    }

    qInfo().noquote() << QStringLiteral("[files] download start %1 (%2 bytes)").arg(rel).arg(file.size());
    const QString fileId = QString::number(QDateTime::currentMSecsSinceEpoch());
    const qint64 total = file.size();
    const QByteArray all = file.readAll();
    file.close();

    const int chunkSize = 64 * 1024;
    const int totalChunks = qMax(1, static_cast<int>((all.size() + chunkSize - 1) / chunkSize));
    for (int i = 0; i < totalChunks; ++i) {
        QByteArray piece = all.mid(i * chunkSize, chunkSize);
        QJsonObject data;
        data.insert(QStringLiteral("fileId"), fileId);
        data.insert(QStringLiteral("seq"), i);
        data.insert(QStringLiteral("total"), totalChunks);
        data.insert(QStringLiteral("chunkBase64"), QString::fromLatin1(piece.toBase64()));

        QJsonObject reply;
        reply.insert(QStringLiteral("type"), MsgType::kGroupFileChunk);
        reply.insert(QStringLiteral("ts"), nowMs());
        reply.insert(QStringLiteral("data"), data);
        session->sendFrame(reply);
    }

    QJsonObject doneData;
    doneData.insert(QStringLiteral("fileId"), fileId);
    doneData.insert(QStringLiteral("fileName"), QFileInfo(target).fileName());
    doneData.insert(QStringLiteral("size"), static_cast<double>(total));

    QJsonObject doneFrame;
    doneFrame.insert(QStringLiteral("type"), MsgType::kGroupFileDownloadDone);
    doneFrame.insert(QStringLiteral("ts"), nowMs());
    doneFrame.insert(QStringLiteral("data"), doneData);
    session->sendFrame(doneFrame);
    return true;
}
