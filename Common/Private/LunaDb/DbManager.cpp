#include "LunaDb/DbManager.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>
#include <QtDebug>

namespace {
const char *const kConnectionName = "luna_main";
}

DbManager &DbManager::instance()
{
    static DbManager manager;
    return manager;
}

QSqlDatabase DbManager::db() const
{
    return QSqlDatabase::database(QString::fromLatin1(kConnectionName));
}

QStringList DbManager::expectedTables()
{
    return QStringList()
        << "user" << "friend" << "chat_group" << "group_member"
        << "message" << "offline_message" << "file_record" << "login_log"
        << "group_activity" << "activity_signup"
        << "moment" << "moment_image" << "moment_like" << "moment_comment"
        << "match_session" << "voice_record";
}

bool DbManager::initialize(const QString &host, quint16 port, const QString &userName,
                           const QString &password, const QString &dbName)
{
    QSqlDatabase database = QSqlDatabase::contains(QString::fromLatin1(kConnectionName))
        ? QSqlDatabase::database(QString::fromLatin1(kConnectionName))
        : QSqlDatabase::addDatabase(QStringLiteral("QMYSQL"), QString::fromLatin1(kConnectionName));

    database.setHostName(host);
    database.setPort(port);
    database.setUserName(userName);
    database.setPassword(password);
    database.setDatabaseName(dbName);

    if (!database.open()) {
        m_lastError = database.lastError().text();
        m_connected = false;
        qCritical().noquote() << "[db] open failed:" << m_lastError;
        return false;
    }

    m_connected = true;
    m_lastError.clear();
    qInfo().noquote() << "[db] connected" << host << ":" << port << "/" << dbName;
    return true;
}

void DbManager::shutdown()
{
    if (QSqlDatabase::contains(QString::fromLatin1(kConnectionName))) {
        {
            QSqlDatabase database = QSqlDatabase::database(QString::fromLatin1(kConnectionName));
            if (database.isOpen())
                database.close();
        }
        QSqlDatabase::removeDatabase(QString::fromLatin1(kConnectionName));
    }
    m_connected = false;
}

bool DbManager::isOpen() const
{
    return m_connected && db().isOpen();
}

QStringList DbManager::missingTables() const
{
    QStringList missing;
    if (!isOpen()) {
        missing << QStringLiteral("<数据库未连接>");
        return missing;
    }

    // 只查 information_schema，不做任何建表动作
    QSqlQuery query(db());
    query.prepare(QStringLiteral(
        "SELECT COUNT(*) FROM information_schema.tables "
        "WHERE table_schema = ? AND table_name = ?"));

    const QStringList expected = expectedTables();
    for (const QString &table : expected) {
        query.bindValue(0, db().databaseName());
        query.bindValue(1, table);
        if (!query.exec() || !query.next() || query.value(0).toInt() == 0)
            missing << table;
    }
    return missing;
}

bool DbManager::registerUser(const QString &username, const QString &passwordHash,
                             const QString &nickname, quint64 *newUserId, QString *errorMessage)
{
    if (!isOpen()) {
        *errorMessage = QStringLiteral("数据库未连接");
        return false;
    }

    QSqlQuery exists(db());
    exists.prepare(QStringLiteral("SELECT id FROM `user` WHERE username = ?"));
    exists.addBindValue(username);
    if (exists.exec() && exists.next()) {
        *errorMessage = QStringLiteral("用户名已存在");
        return false;
    }

    QSqlQuery insert(db());
    insert.prepare(QStringLiteral(
        "INSERT INTO `user` (username, password, nickname, status) VALUES (?, ?, ?, 'offline')"));
    insert.addBindValue(username);
    insert.addBindValue(passwordHash);
    insert.addBindValue(nickname);
    if (!insert.exec()) {
        *errorMessage = insert.lastError().text();
        return false;
    }

    if (newUserId)
        *newUserId = insert.lastInsertId().toULongLong();
    return true;
}

bool DbManager::authenticate(const QString &username, const QString &passwordHash,
                             const QString &ip, UserInfo *out, QString *errorMessage)
{
    if (!isOpen()) {
        *errorMessage = QStringLiteral("数据库未连接");
        return false;
    }

    QSqlQuery query(db());
    query.prepare(QStringLiteral(
        "SELECT id, username, nickname, status FROM `user` WHERE username = ? AND password = ?"));
    query.addBindValue(username);
    query.addBindValue(passwordHash);

    if (!query.exec()) {
        *errorMessage = query.lastError().text();
        return false;
    }

    if (!query.next()) {
        // 区分"用户不存在"和"密码错误"，便于客户端提示；不泄露更多信息
        QSqlQuery probe(db());
        probe.prepare(QStringLiteral("SELECT id FROM `user` WHERE username = ?"));
        probe.addBindValue(username);
        const bool userExists = probe.exec() && probe.next();
        *errorMessage = userExists ? QStringLiteral("密码错误") : QStringLiteral("用户不存在");
        return false;
    }

    if (out) {
        out->userId = query.value(0).toULongLong();
        out->username = query.value(1).toString();
        out->nickname = query.value(2).toString();
        out->status = query.value(3).toString();
    }

    // 登录成功：状态置 online 并留痕（管理端日志与论文测试用例都要用到）
    updateUserStatus(out ? out->userId : 0, QStringLiteral("online"));

    QSqlQuery log(db());
    log.prepare(QStringLiteral(
        "INSERT INTO login_log (user_id, login_time, ip) VALUES (?, NOW(), ?)"));
    log.addBindValue(out ? static_cast<qlonglong>(out->userId) : 0);
    log.addBindValue(ip);
    if (!log.exec())
        qWarning().noquote() << "[db] write login_log failed:" << log.lastError().text();

    return true;
}

bool DbManager::updateUserStatus(quint64 userId, const QString &status)
{
    if (!isOpen() || userId == 0)
        return false;

    QSqlQuery query(db());
    query.prepare(QStringLiteral("UPDATE `user` SET status = ? WHERE id = ?"));
    query.addBindValue(status);
    query.addBindValue(static_cast<qlonglong>(userId));
    if (!query.exec()) {
        qWarning().noquote() << "[db] update status failed:" << query.lastError().text();
        return false;
    }
    return true;
}
// ================= 阶段3：群组 / 消息 / 历史 =================

bool DbManager::isGroupMember(qint64 groupId, quint64 userId)
{
    if (!isOpen() || userId == 0)
        return false;

    QSqlQuery query(db());
    query.prepare(QStringLiteral(
        "SELECT 1 FROM group_member WHERE group_id = ? AND user_id = ? LIMIT 1"));
    query.addBindValue(groupId);
    query.addBindValue(static_cast<qlonglong>(userId));
    return query.exec() && query.next();
}

QList<quint64> DbManager::groupMemberIds(qint64 groupId)
{
    QList<quint64> ids;
    if (!isOpen())
        return ids;

    QSqlQuery query(db());
    query.prepare(QStringLiteral("SELECT user_id FROM group_member WHERE group_id = ?"));
    query.addBindValue(groupId);
    if (query.exec()) {
        while (query.next())
            ids.append(query.value(0).toULongLong());
    }
    return ids;
}

qint64 DbManager::saveMessage(quint64 fromUser, quint64 toUser, qint64 groupId,
                              const QString &msgType, const QString &content)
{
    if (!isOpen())
        return -1;

    QSqlQuery query(db());
    query.prepare(QStringLiteral(
        "INSERT INTO message (from_user_id, to_user_id, group_id, msg_type, content) "
        "VALUES (?, ?, ?, ?, ?)"));
    query.addBindValue(static_cast<qlonglong>(fromUser));
    // 群消息写 group_id、to_user_id 留 NULL；私聊反之
    if (toUser == 0)
        query.addBindValue(QVariant());
    else
        query.addBindValue(static_cast<qlonglong>(toUser));
    if (groupId == 0)
        query.addBindValue(QVariant());
    else
        query.addBindValue(static_cast<qlonglong>(groupId));
    query.addBindValue(msgType);
    query.addBindValue(content);

    if (!query.exec()) {
        qWarning().noquote() << "[db] saveMessage failed:" << query.lastError().text();
        return -1;
    }
    return query.lastInsertId().toLongLong();
}

QJsonArray DbManager::recentGroupMessages(qint64 groupId, int limit)
{
    QJsonArray result;
    if (!isOpen())
        return result;

    QSqlQuery query(db());
    query.prepare(QStringLiteral(
        "SELECT m.id, m.from_user_id, u.nickname, m.content, "
        "DATE_FORMAT(m.created_at, '%Y-%m-%d %H:%i:%s'), m.msg_type "
        "FROM message m JOIN `user` u ON u.id = m.from_user_id "
        "WHERE m.group_id = ? ORDER BY m.id DESC LIMIT ?"));
    query.addBindValue(groupId);
    query.addBindValue(limit);
    if (!query.exec()) {
        qWarning().noquote() << "[db] recentGroupMessages failed:" << query.lastError().text();
        return result;
    }

    // 查询按 id 倒序取最近 N 条，这里再逆序拼成时间升序
    QList<QJsonObject> rows;
    while (query.next()) {
        QJsonObject item;
        item.insert(QStringLiteral("msgId"), static_cast<double>(query.value(0).toLongLong()));
        item.insert(QStringLiteral("fromUser"), static_cast<double>(query.value(1).toULongLong()));
        item.insert(QStringLiteral("nickname"), query.value(2).toString());
        item.insert(QStringLiteral("content"), query.value(3).toString());
        item.insert(QStringLiteral("createdAt"), query.value(4).toString());
        item.insert(QStringLiteral("msgType"), query.value(5).toString());
        rows.prepend(item);
    }
    for (const QJsonObject &row : rows)
        result.append(row);
    return result;
}

QJsonArray DbManager::recentPrivateMessages(quint64 selfId, quint64 peerId, int limit)
{
    QJsonArray result;
    if (!isOpen() || selfId == 0 || peerId == 0)
        return result;

    QSqlQuery query(db());
    query.prepare(QStringLiteral(
        "SELECT m.id, m.from_user_id, u.nickname, m.content, "
        "DATE_FORMAT(m.created_at, '%Y-%m-%d %H:%i:%s'), m.msg_type "
        "FROM message m JOIN `user` u ON u.id = m.from_user_id "
        "WHERE (m.from_user_id = ? AND m.to_user_id = ?) "
        "   OR (m.from_user_id = ? AND m.to_user_id = ?) "
        "ORDER BY m.id DESC LIMIT ?"));
    query.addBindValue(static_cast<qlonglong>(selfId));
    query.addBindValue(static_cast<qlonglong>(peerId));
    query.addBindValue(static_cast<qlonglong>(peerId));
    query.addBindValue(static_cast<qlonglong>(selfId));
    query.addBindValue(limit);
    if (!query.exec()) {
        qWarning().noquote() << "[db] recentPrivateMessages failed:" << query.lastError().text();
        return result;
    }

    QList<QJsonObject> rows;
    while (query.next()) {
        QJsonObject item;
        item.insert(QStringLiteral("msgId"), static_cast<double>(query.value(0).toLongLong()));
        item.insert(QStringLiteral("fromUser"), static_cast<double>(query.value(1).toULongLong()));
        item.insert(QStringLiteral("nickname"), query.value(2).toString());
        item.insert(QStringLiteral("content"), query.value(3).toString());
        item.insert(QStringLiteral("createdAt"), query.value(4).toString());
        item.insert(QStringLiteral("msgType"), query.value(5).toString());
        rows.prepend(item);
    }
    for (const QJsonObject &row : rows)
        result.append(row);
    return result;
}

bool DbManager::storeOfflineMessage(quint64 toUser, const QString &payloadJson)
{
    if (!isOpen() || toUser == 0)
        return false;

    QSqlQuery query(db());
    query.prepare(QStringLiteral(
        "INSERT INTO offline_message (to_user_id, payload_json) VALUES (?, ?)"));
    query.addBindValue(static_cast<qlonglong>(toUser));
    query.addBindValue(payloadJson);
    if (!query.exec()) {
        qWarning().noquote() << "[db] storeOfflineMessage failed:" << query.lastError().text();
        return false;
    }
    return true;
}
// ================= 阶段5：离线消息 / 群组 / 角色 =================

QJsonArray DbManager::takeOfflineMessages(quint64 userId)
{
    QJsonArray result;
    if (!isOpen() || userId == 0)
        return result;

    QSqlQuery query(db());
    query.prepare(QStringLiteral(
        "SELECT id, payload_json FROM offline_message "
        "WHERE to_user_id = ? AND delivered = 0 ORDER BY id"));
    query.addBindValue(static_cast<qlonglong>(userId));
    if (!query.exec()) {
        qWarning().noquote() << "[db] takeOfflineMessages failed:" << query.lastError().text();
        return result;
    }

    qlonglong maxId = 0;
    while (query.next()) {
        const qlonglong id = query.value(0).toLongLong();
        const QJsonDocument doc = QJsonDocument::fromJson(query.value(1).toString().toUtf8());
        if (doc.isObject())
            result.append(doc.object());
        if (id > maxId)
            maxId = id;
    }

    if (maxId > 0) {
        // 只标记本次取出的那一段，避免并发登录重复投递
        QSqlQuery update(db());
        update.prepare(QStringLiteral(
            "UPDATE offline_message SET delivered = 1 WHERE to_user_id = ? AND id <= ?"));
        update.addBindValue(static_cast<qlonglong>(userId));
        update.addBindValue(maxId);
        if (!update.exec())
            qWarning().noquote() << "[db] mark offline delivered failed:" << update.lastError().text();
    }
    return result;
}

qint64 DbManager::createGroup(const QString &groupName, quint64 ownerId)
{
    if (!isOpen() || groupName.isEmpty() || ownerId == 0)
        return -1;

    QSqlDatabase database = db();
    if (!database.transaction())
        return -1;

    QSqlQuery groupQuery(database);
    groupQuery.prepare(QStringLiteral("INSERT INTO chat_group (group_name, owner_id) VALUES (?, ?)"));
    groupQuery.addBindValue(groupName);
    groupQuery.addBindValue(static_cast<qlonglong>(ownerId));
    if (!groupQuery.exec()) {
        database.rollback();
        qWarning().noquote() << "[db] createGroup failed:" << groupQuery.lastError().text();
        return -1;
    }
    const qint64 groupId = groupQuery.lastInsertId().toLongLong();

    // 建群时群主立刻写入 group_member.role='owner'，否则群主自己没有成员身份
    QSqlQuery memberQuery(database);
    memberQuery.prepare(QStringLiteral(
        "INSERT INTO group_member (group_id, user_id, role) VALUES (?, ?, 'owner')"));
    memberQuery.addBindValue(groupId);
    memberQuery.addBindValue(static_cast<qlonglong>(ownerId));
    if (!memberQuery.exec()) {
        database.rollback();
        qWarning().noquote() << "[db] createGroup member failed:" << memberQuery.lastError().text();
        return -1;
    }

    if (!database.commit()) {
        database.rollback();
        return -1;
    }
    return groupId;
}

bool DbManager::addGroupMember(qint64 groupId, quint64 userId)
{
    if (!isOpen() || userId == 0)
        return false;

    QSqlQuery query(db());
    query.prepare(QStringLiteral(
        "INSERT INTO group_member (group_id, user_id, role) VALUES (?, ?, 'member') "
        "ON DUPLICATE KEY UPDATE role = role"));
    query.addBindValue(groupId);
    query.addBindValue(static_cast<qlonglong>(userId));
    if (!query.exec()) {
        qWarning().noquote() << "[db] addGroupMember failed:" << query.lastError().text();
        return false;
    }
    return true;
}

bool DbManager::isGroupAdmin(qint64 groupId, quint64 userId)
{
    if (!isOpen() || userId == 0)
        return false;

    QSqlQuery query(db());
    query.prepare(QStringLiteral(
        "SELECT role FROM group_member WHERE group_id = ? AND user_id = ? LIMIT 1"));
    query.addBindValue(groupId);
    query.addBindValue(static_cast<qlonglong>(userId));
    if (!query.exec() || !query.next())
        return false;

    const QString role = query.value(0).toString();
    return role == QStringLiteral("owner") || role == QStringLiteral("admin");
}

QJsonArray DbManager::groupsOf(quint64 userId)
{
    QJsonArray result;
    if (!isOpen() || userId == 0)
        return result;

    QSqlQuery query(db());
    query.prepare(QStringLiteral(
        "SELECT g.id, g.group_name, gm.role, "
        "  (SELECT COUNT(*) FROM group_member m2 WHERE m2.group_id = g.id) AS member_count "
        "FROM chat_group g JOIN group_member gm ON gm.group_id = g.id "
        "WHERE gm.user_id = ? ORDER BY g.id"));
    query.addBindValue(static_cast<qlonglong>(userId));
    if (!query.exec()) {
        qWarning().noquote() << "[db] groupsOf failed:" << query.lastError().text();
        return result;
    }

    while (query.next()) {
        QJsonObject item;
        item.insert(QStringLiteral("groupId"), static_cast<double>(query.value(0).toLongLong()));
        item.insert(QStringLiteral("name"), query.value(1).toString());
        item.insert(QStringLiteral("role"), query.value(2).toString());
        item.insert(QStringLiteral("memberCount"), query.value(3).toInt());
        result.append(item);
    }
    return result;
}
// ================= 阶段6：文件记录 =================

bool DbManager::saveFileRecord(const QString &fileId, const QString &fileName, qint64 fileSize,
                               quint64 fromUser, quint64 toUser)
{
    if (!isOpen())
        return false;

    QSqlQuery query(db());
    query.prepare(QStringLiteral(
        "INSERT INTO file_record (file_id, file_name, file_size, from_user_id, to_user_id) "
        "VALUES (?, ?, ?, ?, ?)"));
    query.addBindValue(fileId);
    query.addBindValue(fileName);
    query.addBindValue(static_cast<qlonglong>(fileSize));
    query.addBindValue(static_cast<qlonglong>(fromUser));
    query.addBindValue(static_cast<qlonglong>(toUser));
    if (!query.exec()) {
        qWarning().noquote() << "[db] saveFileRecord failed:" << query.lastError().text();
        return false;
    }
    return true;
}

bool DbManager::updateFileSavedPath(const QString &fileId, const QString &savedPath)
{
    if (!isOpen())
        return false;

    QSqlQuery query(db());
    query.prepare(QStringLiteral("UPDATE file_record SET saved_path = ? WHERE file_id = ?"));
    query.addBindValue(savedPath);
    query.addBindValue(fileId);
    if (!query.exec()) {
        qWarning().noquote() << "[db] updateFileSavedPath failed:" << query.lastError().text();
        return false;
    }
    return true;
}
// ================= 阶段7：群活动报名 =================

qint64 DbManager::createActivity(qint64 groupId, quint64 creatorId, const QString &title,
                                 const QString &description, const QString &location,
                                 const QString &startTime, const QString &endTime,
                                 const QString &signupDeadline, int maxParticipants)
{
    if (!isOpen())
        return -1;

    QSqlQuery query(db());
    query.prepare(QStringLiteral(
        "INSERT INTO group_activity (group_id, creator_id, title, description, location, "
        "start_time, end_time, signup_deadline, max_participants, status) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, 'published')"));
    query.addBindValue(groupId);
    query.addBindValue(static_cast<qlonglong>(creatorId));
    query.addBindValue(title);
    query.addBindValue(description);
    query.addBindValue(location);
    query.addBindValue(startTime);
    query.addBindValue(endTime);
    query.addBindValue(signupDeadline);
    query.addBindValue(maxParticipants);
    if (!query.exec()) {
        qWarning().noquote() << "[db] createActivity failed:" << query.lastError().text();
        return -1;
    }
    return query.lastInsertId().toLongLong();
}

QJsonArray DbManager::listActivities(qint64 groupId, quint64 userId)
{
    QJsonArray result;
    if (!isOpen())
        return result;

    QSqlQuery query(db());
    query.prepare(QStringLiteral(
        "SELECT a.id, a.title, a.location, "
        "  DATE_FORMAT(a.start_time, '%Y-%m-%d %H:%i'), "
        "  DATE_FORMAT(a.end_time, '%Y-%m-%d %H:%i'), "
        "  DATE_FORMAT(a.signup_deadline, '%Y-%m-%d %H:%i'), "
        "  a.max_participants, a.status, u.nickname, "
        "  (SELECT COUNT(*) FROM activity_signup s WHERE s.activity_id = a.id AND s.status = 'signed'), "
        "  EXISTS(SELECT 1 FROM activity_signup s2 WHERE s2.activity_id = a.id "
        "         AND s2.user_id = ? AND s2.status = 'signed') "
        "FROM group_activity a JOIN `user` u ON u.id = a.creator_id "
        "WHERE a.group_id = ? ORDER BY a.start_time DESC"));
    query.addBindValue(static_cast<qlonglong>(userId));
    query.addBindValue(groupId);
    if (!query.exec()) {
        qWarning().noquote() << "[db] listActivities failed:" << query.lastError().text();
        return result;
    }

    while (query.next()) {
        QJsonObject item;
        item.insert(QStringLiteral("activityId"), static_cast<double>(query.value(0).toLongLong()));
        item.insert(QStringLiteral("title"), query.value(1).toString());
        item.insert(QStringLiteral("location"), query.value(2).toString());
        item.insert(QStringLiteral("startTime"), query.value(3).toString());
        item.insert(QStringLiteral("endTime"), query.value(4).toString());
        item.insert(QStringLiteral("signupDeadline"), query.value(5).toString());
        item.insert(QStringLiteral("maxParticipants"), query.value(6).toInt());
        item.insert(QStringLiteral("status"), query.value(7).toString());
        item.insert(QStringLiteral("creatorNickname"), query.value(8).toString());
        item.insert(QStringLiteral("signedCount"), query.value(9).toInt());
        item.insert(QStringLiteral("signed"), query.value(10).toInt() > 0);
        result.append(item);
    }
    return result;
}

QJsonObject DbManager::activityDetail(qint64 activityId, quint64 userId)
{
    QJsonObject result;
    if (!isOpen())
        return result;

    QSqlQuery query(db());
    query.prepare(QStringLiteral(
        "SELECT a.id, a.title, a.description, a.location, "
        "  DATE_FORMAT(a.start_time, '%Y-%m-%d %H:%i'), "
        "  DATE_FORMAT(a.end_time, '%Y-%m-%d %H:%i'), "
        "  DATE_FORMAT(a.signup_deadline, '%Y-%m-%d %H:%i'), "
        "  a.max_participants, a.status, u.nickname, "
        "  (SELECT COUNT(*) FROM activity_signup s WHERE s.activity_id = a.id AND s.status = 'signed'), "
        "  EXISTS(SELECT 1 FROM activity_signup s2 WHERE s2.activity_id = a.id "
        "         AND s2.user_id = ? AND s2.status = 'signed') "
        "FROM group_activity a JOIN `user` u ON u.id = a.creator_id WHERE a.id = ?"));
    query.addBindValue(static_cast<qlonglong>(userId));
    query.addBindValue(activityId);
    if (!query.exec() || !query.next())
        return result;

    result.insert(QStringLiteral("activityId"), static_cast<double>(query.value(0).toLongLong()));
    result.insert(QStringLiteral("title"), query.value(1).toString());
    result.insert(QStringLiteral("description"), query.value(2).toString());
    result.insert(QStringLiteral("location"), query.value(3).toString());
    result.insert(QStringLiteral("startTime"), query.value(4).toString());
    result.insert(QStringLiteral("endTime"), query.value(5).toString());
    result.insert(QStringLiteral("signupDeadline"), query.value(6).toString());
    result.insert(QStringLiteral("maxParticipants"), query.value(7).toInt());
    result.insert(QStringLiteral("status"), query.value(8).toString());
    result.insert(QStringLiteral("creatorNickname"), query.value(9).toString());
    result.insert(QStringLiteral("signedCount"), query.value(10).toInt());
    result.insert(QStringLiteral("signed"), query.value(11).toInt() > 0);

    QJsonArray participants;
    QSqlQuery people(db());
    people.prepare(QStringLiteral(
        "SELECT s.user_id, u.nickname, DATE_FORMAT(s.signed_at, '%Y-%m-%d %H:%i') "
        "FROM activity_signup s JOIN `user` u ON u.id = s.user_id "
        "WHERE s.activity_id = ? AND s.status = 'signed' ORDER BY s.signed_at"));
    people.addBindValue(activityId);
    if (people.exec()) {
        while (people.next()) {
            QJsonObject person;
            person.insert(QStringLiteral("userId"), static_cast<double>(people.value(0).toULongLong()));
            person.insert(QStringLiteral("nickname"), people.value(1).toString());
            person.insert(QStringLiteral("signedAt"), people.value(2).toString());
            participants.append(person);
        }
    }
    result.insert(QStringLiteral("participants"), participants);
    return result;
}

QString DbManager::signupActivity(qint64 activityId, quint64 userId)
{
    if (!isOpen())
        return QStringLiteral("数据库未连接");

    QSqlQuery query(db());
    query.prepare(QStringLiteral(
        "SELECT a.status, a.max_participants, "
        "  (a.signup_deadline < NOW()), "
        "  (SELECT COUNT(*) FROM activity_signup s WHERE s.activity_id = a.id AND s.status = 'signed'), "
        "  (SELECT s2.status FROM activity_signup s2 WHERE s2.activity_id = a.id AND s2.user_id = ?) "
        "FROM group_activity a WHERE a.id = ?"));
    query.addBindValue(static_cast<qlonglong>(userId));
    query.addBindValue(activityId);
    if (!query.exec() || !query.next())
        return QStringLiteral("活动不存在");

    const QString status = query.value(0).toString();
    const int maxParticipants = query.value(1).toInt();
    const bool deadlinePassed = query.value(2).toInt() != 0;
    const int signedCount = query.value(3).toInt();
    const QString mine = query.value(4).toString();

    if (status != QStringLiteral("published"))
        return QStringLiteral("活动已结束或已取消");
    if (deadlinePassed)
        return QStringLiteral("报名已截止");
    if (mine == QStringLiteral("signed"))
        return QStringLiteral("你已经报名了");
    if (maxParticipants > 0 && signedCount >= maxParticipants)
        return QStringLiteral("名额已满");

    QSqlQuery upsert(db());
    upsert.prepare(QStringLiteral(
        "INSERT INTO activity_signup (activity_id, user_id, status, signed_at) "
        "VALUES (?, ?, 'signed', NOW()) "
        "ON DUPLICATE KEY UPDATE status = 'signed', signed_at = NOW(), cancelled_at = NULL"));
    upsert.addBindValue(activityId);
    upsert.addBindValue(static_cast<qlonglong>(userId));
    if (!upsert.exec())
        return QStringLiteral("报名失败");
    return QString();
}

QString DbManager::cancelSignup(qint64 activityId, quint64 userId)
{
    if (!isOpen())
        return QStringLiteral("数据库未连接");

    QSqlQuery query(db());
    query.prepare(QStringLiteral(
        "UPDATE activity_signup SET status = 'cancelled', cancelled_at = NOW() "
        "WHERE activity_id = ? AND user_id = ? AND status = 'signed'"));
    query.addBindValue(activityId);
    query.addBindValue(static_cast<qlonglong>(userId));
    if (!query.exec())
        return QStringLiteral("取消报名失败");
    if (query.numRowsAffected() == 0)
        return QStringLiteral("你还没有报名");
    return QString();
}

bool DbManager::cancelActivity(qint64 activityId)
{
    if (!isOpen())
        return false;

    QSqlQuery query(db());
    query.prepare(QStringLiteral(
        "UPDATE group_activity SET status = 'cancelled' "
        "WHERE id = ? AND status IN ('published', 'ongoing')"));
    query.addBindValue(activityId);
    return query.exec() && query.numRowsAffected() > 0;
}

int DbManager::refreshActivityStatuses()
{
    if (!isOpen())
        return 0;

    int changed = 0;

    // 到开始时间且未结束：published -> ongoing
    QSqlQuery startQuery(db());
    startQuery.prepare(QStringLiteral(
        "UPDATE group_activity SET status = 'ongoing' "
        "WHERE status = 'published' AND start_time <= NOW() AND end_time > NOW()"));
    if (startQuery.exec())
        changed += startQuery.numRowsAffected();

    // 过结束时间：published/ongoing -> ended
    QSqlQuery endQuery(db());
    endQuery.prepare(QStringLiteral(
        "UPDATE group_activity SET status = 'ended' "
        "WHERE status IN ('published', 'ongoing') AND end_time <= NOW()"));
    if (endQuery.exec())
        changed += endQuery.numRowsAffected();

    return changed;
}
bool DbManager::activityBrief(qint64 activityId, qint64 *groupId, QString *title)
{
    if (!isOpen())
        return false;

    QSqlQuery query(db());
    query.prepare(QStringLiteral("SELECT group_id, title FROM group_activity WHERE id = ?"));
    query.addBindValue(activityId);
    if (!query.exec() || !query.next())
        return false;

    if (groupId)
        *groupId = query.value(0).toLongLong();
    if (title)
        *title = query.value(1).toString();
    return true;
}
// ================= 阶段8：朋友圈 =================

qint64 DbManager::publishMoment(quint64 userId, const QString &content)
{
    if (!isOpen())
        return -1;

    QSqlQuery query(db());
    query.prepare(QStringLiteral("INSERT INTO moment (user_id, content) VALUES (?, ?)"));
    query.addBindValue(static_cast<qlonglong>(userId));
    query.addBindValue(content);
    if (!query.exec()) {
        qWarning().noquote() << "[db] publishMoment failed:" << query.lastError().text();
        return -1;
    }
    return query.lastInsertId().toLongLong();
}

bool DbManager::addMomentImage(qint64 momentId, const QString &filePath, int sortOrder)
{
    if (!isOpen())
        return false;

    QSqlQuery query(db());
    query.prepare(QStringLiteral(
        "INSERT INTO moment_image (moment_id, image_path, sort_order) VALUES (?, ?, ?)"));
    query.addBindValue(momentId);
    query.addBindValue(filePath);
    query.addBindValue(sortOrder);
    if (!query.exec()) {
        qWarning().noquote() << "[db] addMomentImage failed:" << query.lastError().text();
        return false;
    }
    return true;
}

QJsonArray DbManager::listMoments(int page, int pageSize, quint64 selfId)
{
    QJsonArray result;
    if (!isOpen())
        return result;

    const int offset = qMax(0, (page - 1) * pageSize);

    QSqlQuery query(db());
    query.prepare(QStringLiteral(
        "SELECT m.id, m.user_id, u.nickname, m.content, "
        "  DATE_FORMAT(m.created_at, '%Y-%m-%d %H:%i'), "
        "  (SELECT COUNT(*) FROM moment_like l WHERE l.moment_id = m.id), "
        "  (SELECT COUNT(*) FROM moment_comment c WHERE c.moment_id = m.id), "
        "  EXISTS(SELECT 1 FROM moment_like l2 WHERE l2.moment_id = m.id AND l2.user_id = ?), "
        "  (SELECT COUNT(*) FROM moment_image i WHERE i.moment_id = m.id) "
        "FROM moment m JOIN `user` u ON u.id = m.user_id "
        "WHERE m.deleted = 0 ORDER BY m.id DESC LIMIT ? OFFSET ?"));
    query.addBindValue(static_cast<qlonglong>(selfId));
    query.addBindValue(pageSize);
    query.addBindValue(offset);
    if (!query.exec()) {
        qWarning().noquote() << "[db] listMoments failed:" << query.lastError().text();
        return result;
    }

    while (query.next()) {
        QJsonObject item;
        const qint64 momentId = query.value(0).toLongLong();
        item.insert(QStringLiteral("momentId"), static_cast<double>(momentId));
        item.insert(QStringLiteral("userId"), static_cast<double>(query.value(1).toULongLong()));
        item.insert(QStringLiteral("nickname"), query.value(2).toString());
        item.insert(QStringLiteral("content"), query.value(3).toString());
        item.insert(QStringLiteral("createdAt"), query.value(4).toString());
        item.insert(QStringLiteral("likeCount"), query.value(5).toInt());
        item.insert(QStringLiteral("commentCount"), query.value(6).toInt());
        item.insert(QStringLiteral("liked"), query.value(7).toInt() > 0);
        item.insert(QStringLiteral("imageCount"), query.value(8).toInt());

        // 最近 3 条评论，够卡片展示
        QJsonArray comments;
        QSqlQuery commentQuery(db());
        commentQuery.prepare(QStringLiteral(
            "SELECT u.nickname, c.content FROM moment_comment c "
            "JOIN `user` u ON u.id = c.user_id "
            "WHERE c.moment_id = ? ORDER BY c.id DESC LIMIT 3"));
        commentQuery.addBindValue(momentId);
        if (commentQuery.exec()) {
            QList<QJsonObject> rows;
            while (commentQuery.next()) {
                QJsonObject comment;
                comment.insert(QStringLiteral("nickname"), commentQuery.value(0).toString());
                comment.insert(QStringLiteral("content"), commentQuery.value(1).toString());
                rows.prepend(comment);
            }
            for (const QJsonObject &row : rows)
                comments.append(row);
        }
        item.insert(QStringLiteral("comments"), comments);
        result.append(item);
    }
    return result;
}

QString DbManager::momentImagePath(qint64 momentId, int index)
{
    if (!isOpen())
        return QString();

    QSqlQuery query(db());
    query.prepare(QStringLiteral(
        "SELECT image_path FROM moment_image WHERE moment_id = ? "
        "ORDER BY sort_order LIMIT 1 OFFSET ?"));
    query.addBindValue(momentId);
    query.addBindValue(index);
    if (!query.exec() || !query.next())
        return QString();
    return query.value(0).toString();
}

QJsonObject DbManager::toggleMomentLike(qint64 momentId, quint64 userId)
{
    QJsonObject result;
    result.insert(QStringLiteral("liked"), false);
    result.insert(QStringLiteral("likeCount"), 0);
    if (!isOpen())
        return result;

    QSqlQuery check(db());
    check.prepare(QStringLiteral(
        "SELECT id FROM moment_like WHERE moment_id = ? AND user_id = ? LIMIT 1"));
    check.addBindValue(momentId);
    check.addBindValue(static_cast<qlonglong>(userId));
    const bool likedBefore = check.exec() && check.next();

    bool likedNow = false;
    if (likedBefore) {
        QSqlQuery remove(db());
        remove.prepare(QStringLiteral("DELETE FROM moment_like WHERE moment_id = ? AND user_id = ?"));
        remove.addBindValue(momentId);
        remove.addBindValue(static_cast<qlonglong>(userId));
        remove.exec();
        likedNow = false;
    } else {
        QSqlQuery insert(db());
        insert.prepare(QStringLiteral(
            "INSERT INTO moment_like (moment_id, user_id) VALUES (?, ?)"));
        insert.addBindValue(momentId);
        insert.addBindValue(static_cast<qlonglong>(userId));
        insert.exec();
        likedNow = true;
    }

    QSqlQuery count(db());
    count.prepare(QStringLiteral("SELECT COUNT(*) FROM moment_like WHERE moment_id = ?"));
    count.addBindValue(momentId);
    int likeCount = 0;
    if (count.exec() && count.next())
        likeCount = count.value(0).toInt();

    result.insert(QStringLiteral("liked"), likedNow);
    result.insert(QStringLiteral("likeCount"), likeCount);
    return result;
}

int DbManager::addMomentComment(qint64 momentId, quint64 userId, const QString &content)
{
    if (!isOpen())
        return -1;

    QSqlQuery insert(db());
    insert.prepare(QStringLiteral(
        "INSERT INTO moment_comment (moment_id, user_id, content) VALUES (?, ?, ?)"));
    insert.addBindValue(momentId);
    insert.addBindValue(static_cast<qlonglong>(userId));
    insert.addBindValue(content);
    if (!insert.exec())
        return -1;

    QSqlQuery count(db());
    count.prepare(QStringLiteral("SELECT COUNT(*) FROM moment_comment WHERE moment_id = ?"));
    count.addBindValue(momentId);
    if (count.exec() && count.next())
        return count.value(0).toInt();
    return 0;
}

bool DbManager::deleteMoment(qint64 momentId, quint64 userId)
{
    if (!isOpen())
        return false;

    QSqlQuery query(db());
    query.prepare(QStringLiteral(
        "UPDATE moment SET deleted = 1 WHERE id = ? AND user_id = ? AND deleted = 0"));
    query.addBindValue(momentId);
    query.addBindValue(static_cast<qlonglong>(userId));
    return query.exec() && query.numRowsAffected() > 0;
}

quint64 DbManager::momentOwner(qint64 momentId)
{
    if (!isOpen())
        return 0;

    QSqlQuery query(db());
    query.prepare(QStringLiteral("SELECT user_id FROM moment WHERE id = ?"));
    query.addBindValue(momentId);
    if (query.exec() && query.next())
        return query.value(0).toULongLong();
    return 0;
}
// ================= 阶段9：随机匹配会话 =================

bool DbManager::createMatchSession(const QString &sessionId, quint64 userA, quint64 userB, const QString &mode)
{
    if (!isOpen())
        return false;

    QSqlQuery query(db());
    query.prepare(QStringLiteral(
        "INSERT INTO match_session (session_id, user_a, user_b, mode, status) "
        "VALUES (?, ?, ?, ?, 'active')"));
    query.addBindValue(sessionId);
    query.addBindValue(static_cast<qlonglong>(userA));
    query.addBindValue(static_cast<qlonglong>(userB));
    query.addBindValue(mode);
    if (!query.exec()) {
        qWarning().noquote() << "[db] createMatchSession failed:" << query.lastError().text();
        return false;
    }
    return true;
}

bool DbManager::endMatchSession(const QString &sessionId)
{
    if (!isOpen())
        return false;

    QSqlQuery query(db());
    query.prepare(QStringLiteral(
        "UPDATE match_session SET status = 'ended', ended_at = NOW() "
        "WHERE session_id = ? AND status = 'active'"));
    query.addBindValue(sessionId);
    return query.exec();
}
// ================= 阶段10：语音记录 =================

bool DbManager::saveVoiceRecord(const QString &voiceId, quint64 fromUser, quint64 toUser,
                                const QString &fileName, int durationMs, qint64 fileSize,
                                const QString &format, int bitrate)
{
    if (!isOpen())
        return false;

    QSqlQuery query(db());
    query.prepare(QStringLiteral(
        "INSERT INTO voice_record (voice_id, from_user_id, to_user_id, file_name, "
        "duration_ms, file_size, format, bitrate) VALUES (?, ?, ?, ?, ?, ?, ?, ?)"));
    query.addBindValue(voiceId);
    query.addBindValue(static_cast<qlonglong>(fromUser));
    query.addBindValue(static_cast<qlonglong>(toUser));
    query.addBindValue(fileName);
    query.addBindValue(durationMs);
    query.addBindValue(static_cast<qlonglong>(fileSize));
    query.addBindValue(format);
    query.addBindValue(bitrate);
    if (!query.exec()) {
        qWarning().noquote() << "[db] saveVoiceRecord failed:" << query.lastError().text();
        return false;
    }
    return true;
}

bool DbManager::updateVoiceSavedPath(const QString &voiceId, const QString &savedPath)
{
    if (!isOpen())
        return false;

    QSqlQuery query(db());
    query.prepare(QStringLiteral("UPDATE voice_record SET saved_path = ? WHERE voice_id = ?"));
    query.addBindValue(savedPath);
    query.addBindValue(voiceId);
    return query.exec();
}
// ================= 阶段11：个人资料 =================

QJsonObject DbManager::userProfile(quint64 userId)
{
    QJsonObject profile;
    if (!isOpen() || userId == 0)
        return profile;

    QSqlQuery query(db());
    query.prepare(QStringLiteral(
        "SELECT id, username, nickname, avatar_path, "
        "DATE_FORMAT(created_at, '%Y-%m-%d') FROM `user` WHERE id = ?"));
    query.addBindValue(static_cast<qlonglong>(userId));
    if (!query.exec() || !query.next())
        return profile;

    profile.insert(QStringLiteral("userId"), static_cast<double>(query.value(0).toULongLong()));
    profile.insert(QStringLiteral("username"), query.value(1).toString());
    profile.insert(QStringLiteral("nickname"), query.value(2).toString());
    profile.insert(QStringLiteral("avatarPath"), query.value(3).toString());
    profile.insert(QStringLiteral("createdAt"), query.value(4).toString());
    return profile;
}

bool DbManager::updateUserNickname(quint64 userId, const QString &nickname)
{
    if (!isOpen() || nickname.isEmpty())
        return false;

    QSqlQuery query(db());
    query.prepare(QStringLiteral("UPDATE `user` SET nickname = ? WHERE id = ?"));
    query.addBindValue(nickname);
    query.addBindValue(static_cast<qlonglong>(userId));
    // 昵称没变时 MySQL 返回 0 行受影响，这不算失败（只改头像的场合会走到这里）
    return query.exec();
}

bool DbManager::updateUserAvatarPath(quint64 userId, const QString &avatarPath)
{
    if (!isOpen())
        return false;

    QSqlQuery query(db());
    query.prepare(QStringLiteral("UPDATE `user` SET avatar_path = ? WHERE id = ?"));
    query.addBindValue(avatarPath);
    query.addBindValue(static_cast<qlonglong>(userId));
    return query.exec();
}
// ================= 阶段11：好友 =================

quint64 DbManager::userIdByName(const QString &username)
{
    if (!isOpen())
        return 0;

    QSqlQuery query(db());
    query.prepare(QStringLiteral("SELECT id FROM `user` WHERE username = ?"));
    query.addBindValue(username);
    if (query.exec() && query.next())
        return query.value(0).toULongLong();
    return 0;
}

quint64 DbManager::userIdByNickname(const QString &nickname)
{
    if (!isOpen() || nickname.isEmpty())
        return 0;

    QSqlQuery query(db());
    query.prepare(QStringLiteral("SELECT id FROM `user` WHERE nickname = ? LIMIT 1"));
    query.addBindValue(nickname);
    if (query.exec() && query.next())
        return query.value(0).toULongLong();
    return 0;
}

bool DbManager::friendRequest(quint64 fromUser, quint64 toUser)
{
    if (!isOpen() || fromUser == 0 || toUser == 0 || fromUser == toUser)
        return false;

    QSqlQuery query(db());
    query.prepare(QStringLiteral(
        "INSERT INTO friend (user_id, friend_id, status) VALUES (?, ?, 'pending') "
        "ON DUPLICATE KEY UPDATE status = status"));
    query.addBindValue(static_cast<qlonglong>(fromUser));
    query.addBindValue(static_cast<qlonglong>(toUser));
    return query.exec();
}

bool DbManager::acceptFriend(quint64 fromUser, quint64 toUser)
{
    if (!isOpen())
        return false;

    QSqlDatabase database = db();
    if (!database.transaction())
        return false;

    QSqlQuery accept(database);
    accept.prepare(QStringLiteral(
        "UPDATE friend SET status = 'accepted' WHERE user_id = ? AND friend_id = ?"));
    accept.addBindValue(static_cast<qlonglong>(fromUser));
    accept.addBindValue(static_cast<qlonglong>(toUser));
    accept.exec();

    // 双向都插一条 accepted，查询时不用再判方向
    QSqlQuery reverse(database);
    reverse.prepare(QStringLiteral(
        "INSERT INTO friend (user_id, friend_id, status) VALUES (?, ?, 'accepted') "
        "ON DUPLICATE KEY UPDATE status = 'accepted'"));
    reverse.addBindValue(static_cast<qlonglong>(toUser));
    reverse.addBindValue(static_cast<qlonglong>(fromUser));
    reverse.exec();

    if (!database.commit()) {
        database.rollback();
        return false;
    }
    return true;
}

bool DbManager::areFriends(quint64 a, quint64 b)
{
    if (!isOpen())
        return false;

    QSqlQuery query(db());
    query.prepare(QStringLiteral(
        "SELECT 1 FROM friend WHERE user_id = ? AND friend_id = ? AND status = 'accepted' LIMIT 1"));
    query.addBindValue(static_cast<qlonglong>(a));
    query.addBindValue(static_cast<qlonglong>(b));
    return query.exec() && query.next();
}

QJsonArray DbManager::friendsOf(quint64 userId)
{
    QJsonArray result;
    if (!isOpen() || userId == 0)
        return result;

    QSqlQuery query(db());
    query.prepare(QStringLiteral(
        "SELECT u.id, u.nickname, u.username FROM friend f "
        "JOIN `user` u ON u.id = f.friend_id "
        "WHERE f.user_id = ? AND f.status = 'accepted' ORDER BY u.nickname"));
    query.addBindValue(static_cast<qlonglong>(userId));
    if (!query.exec())
        return result;

    while (query.next()) {
        QJsonObject item;
        item.insert(QStringLiteral("userId"), static_cast<double>(query.value(0).toULongLong()));
        item.insert(QStringLiteral("nickname"), query.value(1).toString());
        item.insert(QStringLiteral("username"), query.value(2).toString());
        result.append(item);
    }
    return result;
}
// ================= 阶段11：群资料与群生命周期 =================

QJsonObject DbManager::groupInfo(qint64 groupId, quint64 userId)
{
    QJsonObject info;
    if (!isOpen())
        return info;

    QSqlQuery query(db());
    query.prepare(QStringLiteral(
        "SELECT g.id, g.group_name, g.owner_id, g.announcement, g.avatar_path, g.folder_path, gm.role, "
        "  (SELECT COUNT(*) FROM group_member m2 WHERE m2.group_id = g.id) "
        "FROM chat_group g LEFT JOIN group_member gm ON gm.group_id = g.id AND gm.user_id = ? "
        "WHERE g.id = ?"));
    query.addBindValue(static_cast<qlonglong>(userId));
    query.addBindValue(groupId);
    if (!query.exec() || !query.next())
        return info;

    info.insert(QStringLiteral("groupId"), static_cast<double>(query.value(0).toLongLong()));
    info.insert(QStringLiteral("name"), query.value(1).toString());
    info.insert(QStringLiteral("ownerId"), static_cast<double>(query.value(2).toULongLong()));
    info.insert(QStringLiteral("announcement"), query.value(3).toString());
    info.insert(QStringLiteral("avatarPath"), query.value(4).toString());
    info.insert(QStringLiteral("folderPath"), query.value(5).toString());
    info.insert(QStringLiteral("role"), query.value(6).toString());
    info.insert(QStringLiteral("memberCount"), query.value(7).toInt());
    return info;
}

bool DbManager::updateGroupInfo(qint64 groupId, const QString &name, const QString &announcement,
                               const QString &folderPath, const QString &avatarPath)
{
    if (!isOpen())
        return false;

    QSqlQuery query(db());
    query.prepare(QStringLiteral(
        "UPDATE chat_group SET group_name = ?, announcement = ?, folder_path = ?, avatar_path = ? WHERE id = ?"));
    query.addBindValue(name);
    query.addBindValue(announcement);
    query.addBindValue(folderPath);
    query.addBindValue(avatarPath);
    query.addBindValue(groupId);
    return query.exec();
}

bool DbManager::transferGroupOwner(qint64 groupId, quint64 newOwnerId)
{
    if (!isOpen())
        return false;

    QSqlDatabase database = db();
    if (!database.transaction())
        return false;

    QSqlQuery promote(database);
    promote.prepare(QStringLiteral("UPDATE group_member SET role = 'owner' WHERE group_id = ? AND user_id = ?"));
    promote.addBindValue(groupId);
    promote.addBindValue(static_cast<qlonglong>(newOwnerId));
    promote.exec();

    QSqlQuery demote(database);
    demote.prepare(QStringLiteral(
        "UPDATE group_member SET role = 'member' WHERE group_id = ? AND user_id <> ? AND role IN ('owner','admin')"));
    demote.addBindValue(groupId);
    demote.addBindValue(static_cast<qlonglong>(newOwnerId));
    demote.exec();

    QSqlQuery owner(database);
    owner.prepare(QStringLiteral("UPDATE chat_group SET owner_id = ? WHERE id = ?"));
    owner.addBindValue(static_cast<qlonglong>(newOwnerId));
    owner.addBindValue(groupId);
    owner.exec();

    if (!database.commit()) {
        database.rollback();
        return false;
    }
    return true;
}

bool DbManager::removeGroupMember(qint64 groupId, quint64 userId)
{
    if (!isOpen())
        return false;

    QSqlQuery query(db());
    query.prepare(QStringLiteral("DELETE FROM group_member WHERE group_id = ? AND user_id = ?"));
    query.addBindValue(groupId);
    query.addBindValue(static_cast<qlonglong>(userId));
    return query.exec() && query.numRowsAffected() > 0;
}

bool DbManager::dissolveGroup(qint64 groupId)
{
    if (!isOpen())
        return false;

    QSqlDatabase database = db();
    if (!database.transaction())
        return false;

    QSqlQuery members(database);
    members.prepare(QStringLiteral("DELETE FROM group_member WHERE group_id = ?"));
    members.addBindValue(groupId);
    members.exec();

    QSqlQuery group(database);
    group.prepare(QStringLiteral("DELETE FROM chat_group WHERE id = ?"));
    group.addBindValue(groupId);
    group.exec();

    if (!database.commit()) {
        database.rollback();
        return false;
    }
    return true;
}

qint64 DbManager::groupOwnerId(qint64 groupId)
{
    if (!isOpen())
        return 0;

    QSqlQuery query(db());
    query.prepare(QStringLiteral("SELECT owner_id FROM chat_group WHERE id = ?"));
    query.addBindValue(groupId);
    if (query.exec() && query.next())
        return query.value(0).toLongLong();
    return 0;
}
QJsonArray DbManager::groupMembers(qint64 groupId)
{
    QJsonArray result;
    if (!isOpen())
        return result;

    QSqlQuery query(db());
    query.prepare(QStringLiteral(
        "SELECT m.user_id, u.nickname, m.role FROM group_member m "
        "JOIN `user` u ON u.id = m.user_id WHERE m.group_id = ? "
        "ORDER BY FIELD(m.role,'owner','admin','member'), u.nickname"));
    query.addBindValue(groupId);
    if (!query.exec())
        return result;

    while (query.next()) {
        QJsonObject item;
        item.insert(QStringLiteral("userId"), static_cast<double>(query.value(0).toULongLong()));
        item.insert(QStringLiteral("nickname"), query.value(1).toString());
        item.insert(QStringLiteral("role"), query.value(2).toString());
        result.append(item);
    }
    return result;
}
QJsonArray DbManager::filesOfGroup(qint64 groupId)
{
    QJsonArray result;
    if (!isOpen())
        return result;

    QSqlQuery query(db());
    query.prepare(QStringLiteral(
        "SELECT f.file_id, f.file_name, f.file_size, u.nickname, f.saved_path, "
        "  DATE_FORMAT(f.created_at, '%Y-%m-%d %H:%i') "
        "FROM file_record f LEFT JOIN `user` u ON u.id = f.from_user_id "
        "WHERE f.group_id = ? ORDER BY f.id DESC LIMIT 100"));
    query.addBindValue(groupId);
    if (!query.exec())
        return result;

    while (query.next()) {
        QJsonObject item;
        item.insert(QStringLiteral("fileId"), query.value(0).toString());
        item.insert(QStringLiteral("fileName"), query.value(1).toString());
        item.insert(QStringLiteral("fileSize"), static_cast<double>(query.value(2).toLongLong()));
        item.insert(QStringLiteral("fromNickname"), query.value(3).toString());
        item.insert(QStringLiteral("savedPath"), query.value(4).toString());
        item.insert(QStringLiteral("createdAt"), query.value(5).toString());
        result.append(item);
    }
    return result;
}