#include "Handlers/MomentHandler.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QtDebug>

#include "LunaDb/DbManager.h"
#include "LunaProtocol/MessageTypes.h"
#include "Network/ClientSession.h"

namespace {
double nowMs()
{
    return static_cast<double>(QDateTime::currentMSecsSinceEpoch());
}
const int kMaxImages = 3;
const int kMaxImageBytes = 200 * 1024;    // 单张压缩后 ≤200KB
}

MomentHandler::MomentHandler(QObject *parent)
    : QObject(parent)
{
}

void MomentHandler::sendError(ClientSession *session, int code, const QString &message)
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

void MomentHandler::notifyOwner(quint64 ownerId, const QString &event,
                                const QString &message, qint64 momentId)
{
    if (ownerId == 0)
        return;

    QJsonObject data;
    data.insert(QStringLiteral("event"), event);
    data.insert(QStringLiteral("momentId"), static_cast<double>(momentId));
    data.insert(QStringLiteral("message"), message);

    QJsonObject frame;
    frame.insert(QStringLiteral("type"), MsgType::kMomentNotify);
    frame.insert(QStringLiteral("ts"), nowMs());
    frame.insert(QStringLiteral("data"), data);
    emit frameToUser(ownerId, frame);
}

bool MomentHandler::handle(ClientSession *session, const QJsonObject &frame)
{
    const QString type = frame.value(QStringLiteral("type")).toString();
    const bool mine = (type == MsgType::kMomentPublish || type == MsgType::kMomentList
                       || type == MsgType::kMomentImage || type == MsgType::kMomentLike
                       || type == MsgType::kMomentComment || type == MsgType::kMomentDelete);
    if (!mine)
        return false;

    if (session->userId() == 0) {
        sendError(session, 401, QStringLiteral("请先登录"));
        return true;
    }

    if (type == MsgType::kMomentPublish)
        return handlePublish(session, frame);
    if (type == MsgType::kMomentList)
        return handleList(session, frame);
    if (type == MsgType::kMomentImage)
        return handleImage(session, frame);
    if (type == MsgType::kMomentLike)
        return handleLike(session, frame);
    if (type == MsgType::kMomentComment)
        return handleComment(session, frame);
    return handleDelete(session, frame);
}

bool MomentHandler::handlePublish(ClientSession *session, const QJsonObject &frame)
{
    const QJsonObject data = frame.value(QStringLiteral("data")).toObject();
    const QString content = data.value(QStringLiteral("content")).toString().trimmed();
    const QJsonArray images = data.value(QStringLiteral("images")).toArray();

    if (content.isEmpty() && images.isEmpty()) {
        sendError(session, 400, QStringLiteral("内容不能为空"));
        return true;
    }
    if (images.size() > kMaxImages) {
        sendError(session, 400, QStringLiteral("最多只能发 3 张图片"));
        return true;
    }

    const qint64 momentId = DbManager::instance().publishMoment(session->userId(), content);
    if (momentId <= 0) {
        sendError(session, 500, QStringLiteral("发布失败"));
        return true;
    }

    // 图片按 moment_id 命名落盘，客户端按需逐张拉取，避免一帧塞几百 KB
    const QString dirPath = QCoreApplication::applicationDirPath() + QStringLiteral("/moments");
    QDir().mkpath(dirPath);

    for (int i = 0; i < images.size(); ++i) {
        const QByteArray raw = QByteArray::fromBase64(
            images.at(i).toString().toLatin1());
        if (raw.isEmpty() || raw.size() > kMaxImageBytes) {
            qWarning() << "[moment] image" << i << "rejected, size =" << raw.size();
            continue;
        }
        const QString path = QDir(dirPath).filePath(
            QStringLiteral("%1_%2.jpg").arg(momentId).arg(i));
        QFile file(path);
        if (file.open(QIODevice::WriteOnly) && file.write(raw) > 0)
            DbManager::instance().addMomentImage(momentId, path, i);
    }

    qInfo().noquote() << QStringLiteral("[moment] user %1 published moment %2 (%3 image(s))")
                             .arg(session->userId()).arg(momentId).arg(images.size());

    QJsonObject replyData;
    replyData.insert(QStringLiteral("momentId"), static_cast<double>(momentId));
    replyData.insert(QStringLiteral("ok"), true);

    QJsonObject reply;
    reply.insert(QStringLiteral("type"), MsgType::kMomentList);
    reply.insert(QStringLiteral("ts"), nowMs());
    reply.insert(QStringLiteral("data"), replyData);
    session->sendFrame(reply);   // 发布成功让客户端自己刷新列表（复用 list 请求）
    return true;
}

bool MomentHandler::handleList(ClientSession *session, const QJsonObject &frame)
{
    const QJsonObject data = frame.value(QStringLiteral("data")).toObject();
    const int page = qMax(1, data.value(QStringLiteral("page")).toInt(1));
    const int pageSize = qBound(1, data.value(QStringLiteral("pageSize")).toInt(5), 20);

    QJsonObject replyData;
    replyData.insert(QStringLiteral("page"), page);
    replyData.insert(QStringLiteral("pageSize"), pageSize);
    replyData.insert(QStringLiteral("moments"),
                     DbManager::instance().listMoments(page, pageSize, session->userId()));

    QJsonObject reply;
    reply.insert(QStringLiteral("type"), MsgType::kMomentListResp);
    reply.insert(QStringLiteral("ts"), nowMs());
    reply.insert(QStringLiteral("data"), replyData);
    session->sendFrame(reply);
    return true;
}

bool MomentHandler::handleImage(ClientSession *session, const QJsonObject &frame)
{
    const QJsonObject data = frame.value(QStringLiteral("data")).toObject();
    const qint64 momentId = static_cast<qint64>(data.value(QStringLiteral("momentId")).toDouble());
    const int index = data.value(QStringLiteral("index")).toInt();

    const QString path = DbManager::instance().momentImagePath(momentId, index);
    QFile file(path);
    if (path.isEmpty() || !file.open(QIODevice::ReadOnly)) {
        sendError(session, 404, QStringLiteral("图片不存在"));
        return true;
    }
    const QByteArray bytes = file.readAll();
    file.close();

    QJsonObject replyData;
    replyData.insert(QStringLiteral("momentId"), static_cast<double>(momentId));
    replyData.insert(QStringLiteral("index"), index);
    replyData.insert(QStringLiteral("data"), QString::fromLatin1(bytes.toBase64()));

    QJsonObject reply;
    reply.insert(QStringLiteral("type"), MsgType::kMomentImageResp);
    reply.insert(QStringLiteral("ts"), nowMs());
    reply.insert(QStringLiteral("data"), replyData);
    session->sendFrame(reply);
    return true;
}

bool MomentHandler::handleLike(ClientSession *session, const QJsonObject &frame)
{
    const QJsonObject data = frame.value(QStringLiteral("data")).toObject();
    const qint64 momentId = static_cast<qint64>(data.value(QStringLiteral("momentId")).toDouble());

    const quint64 owner = DbManager::instance().momentOwner(momentId);
    if (owner == 0) {
        sendError(session, 404, QStringLiteral("动态不存在"));
        return true;
    }

    const QJsonObject likeResult = DbManager::instance().toggleMomentLike(momentId, session->userId());

    QJsonObject replyData;
    replyData.insert(QStringLiteral("momentId"), static_cast<double>(momentId));
    replyData.insert(QStringLiteral("liked"), likeResult.value(QStringLiteral("liked")));
    replyData.insert(QStringLiteral("likeCount"), likeResult.value(QStringLiteral("likeCount")));

    QJsonObject reply;
    reply.insert(QStringLiteral("type"), MsgType::kMomentLikeResp);
    reply.insert(QStringLiteral("ts"), nowMs());
    reply.insert(QStringLiteral("data"), replyData);
    session->sendFrame(reply);

    if (likeResult.value(QStringLiteral("liked")).toBool() && owner != session->userId())
        notifyOwner(owner, QStringLiteral("like"),
                    QStringLiteral("%1 赞了你的动态").arg(session->nickname()), momentId);
    return true;
}

bool MomentHandler::handleComment(ClientSession *session, const QJsonObject &frame)
{
    const QJsonObject data = frame.value(QStringLiteral("data")).toObject();
    const qint64 momentId = static_cast<qint64>(data.value(QStringLiteral("momentId")).toDouble());
    const QString content = data.value(QStringLiteral("content")).toString().trimmed();

    if (content.isEmpty()) {
        sendError(session, 400, QStringLiteral("评论内容不能为空"));
        return true;
    }

    const quint64 owner = DbManager::instance().momentOwner(momentId);
    if (owner == 0) {
        sendError(session, 404, QStringLiteral("动态不存在"));
        return true;
    }

    const int commentCount = DbManager::instance().addMomentComment(momentId, session->userId(), content);
    if (commentCount < 0) {
        sendError(session, 500, QStringLiteral("评论失败"));
        return true;
    }

    QJsonObject replyData;
    replyData.insert(QStringLiteral("momentId"), static_cast<double>(momentId));
    replyData.insert(QStringLiteral("ok"), true);
    replyData.insert(QStringLiteral("commentCount"), commentCount);

    QJsonObject reply;
    reply.insert(QStringLiteral("type"), MsgType::kMomentCommentResp);
    reply.insert(QStringLiteral("ts"), nowMs());
    reply.insert(QStringLiteral("data"), replyData);
    session->sendFrame(reply);

    if (owner != session->userId())
        notifyOwner(owner, QStringLiteral("comment"),
                    QStringLiteral("%1 评论了你的动态：%2").arg(session->nickname(), content.left(30)),
                    momentId);
    return true;
}

bool MomentHandler::handleDelete(ClientSession *session, const QJsonObject &frame)
{
    const QJsonObject data = frame.value(QStringLiteral("data")).toObject();
    const qint64 momentId = static_cast<qint64>(data.value(QStringLiteral("momentId")).toDouble());

    // 只能删自己的：SQL 里带 user_id 条件，删别人的会 0 行
    const bool ok = DbManager::instance().deleteMoment(momentId, session->userId());

    QJsonObject replyData;
    replyData.insert(QStringLiteral("momentId"), static_cast<double>(momentId));
    replyData.insert(QStringLiteral("ok"), ok);

    QJsonObject reply;
    reply.insert(QStringLiteral("type"), MsgType::kMomentDeleteResp);
    reply.insert(QStringLiteral("ts"), nowMs());
    reply.insert(QStringLiteral("data"), replyData);
    session->sendFrame(reply);
    return true;
}