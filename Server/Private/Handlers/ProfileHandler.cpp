#include "Handlers/ProfileHandler.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QtDebug>

#include "LunaDb/DbManager.h"
#include "LunaProtocol/MessageTypes.h"
#include "Network/ClientSession.h"

namespace {
double nowMs()
{
    return static_cast<double>(QDateTime::currentMSecsSinceEpoch());
}
const int kMaxAvatarBytes = 200 * 1024;
}

ProfileHandler::ProfileHandler(QObject *parent)
    : QObject(parent)
{
}

QString ProfileHandler::avatarFilePath(quint64 userId) const
{
    const QString dirPath = QCoreApplication::applicationDirPath() + QStringLiteral("/avatars");
    return QDir(dirPath).filePath(QStringLiteral("%1.jpg").arg(userId));
}

void ProfileHandler::sendError(ClientSession *session, int code, const QString &message)
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

bool ProfileHandler::handle(ClientSession *session, const QJsonObject &frame)
{
    const QString type = frame.value(QStringLiteral("type")).toString();
    if (type != MsgType::kProfileGet && type != MsgType::kProfileUpdate
        && type != MsgType::kAvatarGet)
        return false;

    if (session->userId() == 0) {
        sendError(session, 401, QStringLiteral("请先登录"));
        return true;
    }
    if (type == MsgType::kAvatarGet)
        return handleAvatarGet(session, frame);
    return (type == MsgType::kProfileGet) ? handleGet(session, frame)
                                          : handleUpdate(session, frame);
}

bool ProfileHandler::handleGet(ClientSession *session, const QJsonObject &frame)
{
    Q_UNUSED(frame)
    QJsonObject data = DbManager::instance().userProfile(session->userId());
    if (data.isEmpty()) {
        sendError(session, 404, QStringLiteral("用户不存在"));
        return true;
    }

    // 头像单独读文件并按需回 base64，避免资料帧过大
    QFile avatar(avatarFilePath(session->userId()));
    if (avatar.open(QIODevice::ReadOnly)) {
        const QByteArray bytes = avatar.readAll();
        if (!bytes.isEmpty())
            data.insert(QStringLiteral("avatarBase64"), QString::fromLatin1(bytes.toBase64()));
        avatar.close();
    }

    QJsonObject reply;
    reply.insert(QStringLiteral("type"), MsgType::kProfileResp);
    reply.insert(QStringLiteral("ts"), nowMs());
    reply.insert(QStringLiteral("data"), data);
    session->sendFrame(reply);
    return true;
}

bool ProfileHandler::handleUpdate(ClientSession *session, const QJsonObject &frame)
{
    const QJsonObject data = frame.value(QStringLiteral("data")).toObject();
    const QString nickname = data.value(QStringLiteral("nickname")).toString().trimmed();
    const QString avatarBase64 = data.value(QStringLiteral("avatarBase64")).toString();

    QJsonObject replyData;
    if (!nickname.isEmpty()) {
        if (nickname.size() > 32) {
            sendError(session, 400, QStringLiteral("昵称最多 32 个字符"));
            return true;
        }
        if (!DbManager::instance().updateUserNickname(session->userId(), nickname)) {
            sendError(session, 500, QStringLiteral("昵称修改失败"));
            return true;
        }
        session->setNickname(nickname);
    }

    if (!avatarBase64.isEmpty()) {
        const QByteArray bytes = QByteArray::fromBase64(avatarBase64.toLatin1());
        if (bytes.size() > kMaxAvatarBytes) {
            sendError(session, 413, QStringLiteral("头像文件过大"));
            return true;
        }
        const QString path = avatarFilePath(session->userId());
        QDir().mkpath(QFileInfo(path).absolutePath());
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly) || file.write(bytes) <= 0) {
            sendError(session, 500, QStringLiteral("头像保存失败"));
            return true;
        }
        file.close();
        DbManager::instance().updateUserAvatarPath(session->userId(), path);
    }

    replyData.insert(QStringLiteral("ok"), true);
    replyData.insert(QStringLiteral("msg"), QStringLiteral("资料已更新"));
    replyData.insert(QStringLiteral("nickname"), session->nickname());

    QJsonObject reply;
    reply.insert(QStringLiteral("type"), MsgType::kProfileResp);
    reply.insert(QStringLiteral("ts"), nowMs());
    reply.insert(QStringLiteral("data"), replyData);
    session->sendFrame(reply);

    qInfo().noquote() << QStringLiteral("[profile] user %1 updated (nickname=%2)")
                             .arg(session->userId()).arg(session->nickname());
    emit userListChanged();
    return true;
}
bool ProfileHandler::handleAvatarGet(ClientSession *session, const QJsonObject &frame)
{
    const QJsonObject data = frame.value(QStringLiteral("data")).toObject();
    const quint64 userId = static_cast<quint64>(data.value(QStringLiteral("userId")).toDouble());

    // 没有头像时回空串而不是错误：客户端据此停止重复请求
    QByteArray bytes;
    QFile file(avatarFilePath(userId));
    if (file.open(QIODevice::ReadOnly)) {
        bytes = file.readAll();
        file.close();
    }

    QJsonObject replyData;
    replyData.insert(QStringLiteral("userId"), static_cast<double>(userId));
    replyData.insert(QStringLiteral("data"), QString::fromLatin1(bytes.toBase64()));

    QJsonObject reply;
    reply.insert(QStringLiteral("type"), MsgType::kAvatarResp);
    reply.insert(QStringLiteral("ts"), nowMs());
    reply.insert(QStringLiteral("data"), replyData);
    session->sendFrame(reply);
    return true;
}