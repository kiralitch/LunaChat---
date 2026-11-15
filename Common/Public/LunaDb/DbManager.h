#pragma once

#include <QJsonArray>
#include <QList>
#include <QSqlDatabase>
#include <QString>
#include <QStringList>

// 数据库访问唯一入口：连接管理 + 启动自检 + 用户查询
// 约定：所有 SQL 一律 QSqlQuery 预处理绑定（addBindValue），禁止拼接字符串
class DbManager
{
public:
    struct UserInfo {
        quint64 userId = 0;
        QString username;
        QString nickname;
        QString status;
    };

    static DbManager &instance();

    bool initialize(const QString &host, quint16 port, const QString &userName,
                    const QString &password, const QString &dbName);
    void shutdown();

    bool isOpen() const;
    QString lastError() const { return m_lastError; }

    // 启动自检：返回缺失的表名；不自动建表，提示先执行 sql/schema.sql
    QStringList missingTables() const;
    static QStringList expectedTables();

    // 注册：username 唯一；passwordHash 由调用方先做 SHA-256
    bool registerUser(const QString &username, const QString &passwordHash,
                      const QString &nickname, quint64 *newUserId, QString *errorMessage);

    // 登录：命中则把状态改为 online 并写 login_log；失败时 errorMessage 区分"用户不存在/密码错误"
    bool authenticate(const QString &username, const QString &passwordHash,
                      const QString &ip, UserInfo *out, QString *errorMessage);

    bool updateUserStatus(quint64 userId, const QString &status);

    // ---- 阶段3：群组 / 消息 / 历史 ----
    bool isGroupMember(qint64 groupId, quint64 userId);
    QList<quint64> groupMemberIds(qint64 groupId);

    // toUser=0 表示群消息（写 group_id），groupId=0 表示私聊（写 to_user_id）
    qint64 saveMessage(quint64 fromUser, quint64 toUser, qint64 groupId,
                       const QString &msgType, const QString &content);

    // 返回按时间升序排列的 JSON 数组（每项含 msgId/fromUser/nickname/content/createdAt/msgType）
    QJsonArray recentGroupMessages(qint64 groupId, int limit);
    QJsonArray recentPrivateMessages(quint64 selfId, quint64 peerId, int limit);

    bool storeOfflineMessage(quint64 toUser, const QString &payloadJson);
    // 取出并标记已投递（登录成功时调用）
    QJsonArray takeOfflineMessages(quint64 userId);

    // ---- 阶段5：自建群组与角色 ----
    qint64 createGroup(const QString &groupName, quint64 ownerId);   // 群主同时写 group_member.role='owner'
    bool addGroupMember(qint64 groupId, quint64 userId);
    bool isGroupAdmin(qint64 groupId, quint64 userId);               // owner/admin 都算管理员
    QJsonArray groupsOf(quint64 userId);

    // ---- 阶段6：文件记录 ----
    bool saveFileRecord(const QString &fileId, const QString &fileName, qint64 fileSize,
                        quint64 fromUser, quint64 toUser);
    bool updateFileSavedPath(const QString &fileId, const QString &savedPath);

    // ---- 阶段7：群活动报名 ----
    qint64 createActivity(qint64 groupId, quint64 creatorId, const QString &title,
                          const QString &description, const QString &location,
                          const QString &startTime, const QString &endTime,
                          const QString &signupDeadline, int maxParticipants);
    QJsonArray listActivities(qint64 groupId, quint64 userId);
    QJsonObject activityDetail(qint64 activityId, quint64 userId);
    // 返回空字符串表示成功，否则是给用户看的失败原因
    QString signupActivity(qint64 activityId, quint64 userId);
    QString cancelSignup(qint64 activityId, quint64 userId);
    bool cancelActivity(qint64 activityId);
    int refreshActivityStatuses();
    // 取活动所属群与标题（用于广播通知）
    bool activityBrief(qint64 activityId, qint64 *groupId, QString *title);

    // ---- 阶段8：朋友圈 ----
    qint64 publishMoment(quint64 userId, const QString &content);
    bool addMomentImage(qint64 momentId, const QString &filePath, int sortOrder);
    QJsonArray listMoments(int page, int pageSize, quint64 selfId);
    QString momentImagePath(qint64 momentId, int index);
    QJsonObject toggleMomentLike(qint64 momentId, quint64 userId);   // {liked, likeCount}
    int addMomentComment(qint64 momentId, quint64 userId, const QString &content);  // 返回评论数
    bool deleteMoment(qint64 momentId, quint64 userId);
    quint64 momentOwner(qint64 momentId);

    // ---- 阶段9：随机匹配会话 ----
    bool createMatchSession(const QString &sessionId, quint64 userA, quint64 userB, const QString &mode);
    bool endMatchSession(const QString &sessionId);

    // ---- 阶段10：语音记录 ----
    bool saveVoiceRecord(const QString &voiceId, quint64 fromUser, quint64 toUser,
                         const QString &fileName, int durationMs, qint64 fileSize,
                         const QString &format, int bitrate);
    bool updateVoiceSavedPath(const QString &voiceId, const QString &savedPath);

    // ---- 阶段11：个人资料 ----
    QJsonObject userProfile(quint64 userId);
    bool updateUserNickname(quint64 userId, const QString &nickname);
    bool updateUserAvatarPath(quint64 userId, const QString &avatarPath);

    // ---- 阶段11：好友 ----
    quint64 userIdByName(const QString &username);
    quint64 userIdByNickname(const QString &nickname);
    bool friendRequest(quint64 fromUser, quint64 toUser);
    bool acceptFriend(quint64 fromUser, quint64 toUser);
    bool areFriends(quint64 a, quint64 b);
    QJsonArray friendsOf(quint64 userId);   // published -> ongoing -> ended，由服务端定时器驱动                             // [{groupId,name,role,memberCount}]

    // ---- 阶段11：群资料与群生命周期 ----
    QJsonObject groupInfo(qint64 groupId, quint64 userId);
    bool updateGroupInfo(qint64 groupId, const QString &name, const QString &announcement,
                         const QString &folderPath, const QString &avatarPath);
    bool transferGroupOwner(qint64 groupId, quint64 newOwnerId);
    bool removeGroupMember(qint64 groupId, quint64 userId);
    bool dissolveGroup(qint64 groupId);
    qint64 groupOwnerId(qint64 groupId);
    QJsonArray groupMembers(qint64 groupId);
    QJsonArray filesOfGroup(qint64 groupId);

private:
    DbManager() = default;
    QSqlDatabase db() const;

    bool m_connected = false;
    QString m_lastError;
};
