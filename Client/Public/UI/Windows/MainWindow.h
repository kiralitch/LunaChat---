#pragma once

#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QList>
#include <QMainWindow>
#include <QMap>
#include <QIcon>
#include <QSet>

class ChatPage;
class AddFriendPage;
class CreateGroupPage;
class GroupSearchPage;
class LunaClient;
class QFile;
class QListWidget;
class QPushButton;
class QTimer;

// 主窗口：左侧群组/在线列表，右侧聊天页；负责组帧、消息归位与文件收发
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(LunaClient *client, quint64 userId, const QString &nickname,
               QWidget *parent = nullptr);
    ~MainWindow() override;

protected:
    bool nativeEvent(const QByteArray &eventType, void *message, long *result) override;

private slots:
    void onSessionItemChanged();
    void onSendRequested(const QString &text);
    void onFrameReceived(const QJsonObject &frame);
    void onCreateGroup();
    void onOpenActivities();
    void onOpenMoments();
    void onOpenMatch();
    void onOpenSettings();
    void onOpenProfile();
    void onAddFriend();
    void onOpenRequests();
    void onOpenGroupSearch();
    void onOpenGroupInfo();
    void onOpenGroupFiles();
    void playSound(const QString &fileName);
    void onUploadToFolder();
    qint64 currentGroupId() const;
    void onInviteMember();
    void onLeaveGroup();
    void onContextMenu(const QPoint &pos);
    void onFileSendRequested();
    void onVoiceRecordRequested();
    void onRecorderFinished(const QByteArray &wavData, int durationMs);
    void sendNextChunk();

private:
    struct MessageLine {
        quint64 userId = 0;
        QString nickname;
        QString content;
        bool self = false;
        QString timeText;
        QString dateKey;   // yyyy-MM-dd，聊天列表按日期分组用
        qint64 msgId = 0;
    };

    void rebuildConversationList();
    void refreshListIcons();
    void selectConversation(const QString &key);
    void requestHistory(const QString &key);
    void appendToConversation(const QString &key, const MessageLine &line);
    void handleChatBroadcast(const QJsonObject &frame);
    void appendSystemLine(const QString &text);
    void inviteUser(qint64 groupId, quint64 targetId);
    void refreshGroupList();
    void handleFileFrame(const QJsonObject &frame);
    void handleVoiceFrame(const QJsonObject &frame);
    void requestAvatar(quint64 userId);
    void sendVoice(const QByteArray &wavData, int durationMs);
    QString keyOfUser(quint64 userId) const;
    void setSection(const QString &name);
    void requestDeleteFriend(quint64 userId, const QString &nickname);
    QString keyOfGroup(qint64 groupId) const;

    LunaClient *m_client = nullptr;
    quint64 m_userId = 0;
    QString m_nickname;

    QListWidget *m_sessionList = nullptr;
    ChatPage *m_chatPage = nullptr;
    QPushButton *m_newGroupButton = nullptr;
    QPushButton *m_activityButton = nullptr;
    QPushButton *m_momentButton = nullptr;
    QPushButton *m_matchButton = nullptr;
    QPushButton *m_settingsButton = nullptr;
    QPushButton *m_profileButton = nullptr;
    QPushButton *m_addFriendButton = nullptr;
    QPushButton *m_groupInfoButton = nullptr;
    QPushButton *m_inviteButton = nullptr;
    QPushButton *m_leaveButton = nullptr;
    class QStackedWidget *m_stack = nullptr;
    class ProfilePage *m_profilePage = nullptr;
    class MomentPage *m_momentPage = nullptr;
    class ActivityPage *m_activityPage = nullptr;
    class MatchPage *m_matchPage = nullptr;
    class GroupInfoPage *m_groupInfoPage = nullptr;
    class GroupFilesPage *m_groupFilesPage = nullptr;
    class AddFriendPage *m_addFriendPage = nullptr;
    class CreateGroupPage *m_createGroupPage = nullptr;
    class RequestsPage *m_requestsPage = nullptr;
    class GroupSearchPage *m_groupSearchPage = nullptr;
    class BadgeButton *m_requestsButton = nullptr;
    class QToolButton *m_groupSearchButton = nullptr;
    class IconRail *m_rail = nullptr;
    class TitleBar *m_titleBar = nullptr;
    class SettingsPage *m_settingsPage = nullptr;
    class QStackedWidget *m_mainStack = nullptr;

    QString m_currentKey;                              // "g1" 群聊 / "u2" 私聊
    QHash<QString, QList<MessageLine>> m_messages;
    QSet<QString> m_historyLoaded;
    QHash<quint64, QString> m_users;                   // userId -> 昵称
    QMap<qint64, QString> m_groups;                    // groupId -> 群名（有序）
    QHash<qint64, QString> m_groupRole;                // groupId -> owner/admin/member
    QSet<quint64> m_avatarRequested;
    QSet<quint64> m_friendIds;                        // 只有好友才出现在左侧列表
    QHash<quint64, QString> m_friendNames;
    QHash<quint64, QIcon> m_friendIcons;               // 好友头像（列表用）
    QHash<qint64, QIcon> m_groupIcons;                 // 群头像（列表用）
    QSet<quint64> m_friendIconAsked;
    QSet<qint64> m_groupIconAsked;                   // 已请求过头像的用户，避免重复拉

    class Recorder *m_recorder = nullptr;
    QHash<QString, QByteArray> m_inVoice;             // voiceId -> 已收到的数据
    QHash<QString, QString> m_inVoiceName;
    QHash<QString, int> m_inVoiceDuration;
    QHash<QString, QString> m_inVoiceNick;
    QHash<QString, qint64> m_inVoiceSize;

    // 发送端状态（一次只发一个文件，够用且简单）
    QFile *m_outFile = nullptr;
    QTimer *m_chunkTimer = nullptr;
    QString m_outFileId;
    QString m_outName;
    quint64 m_outTarget = 0;
    qint64 m_outGroup = 0;    // 非 0 表示发到群共享文件夹
    QString m_outPath;        // 群共享文件夹内的子目录（相对路径）
    qint64 m_outTotal = 0;
    qint64 m_outSent = 0;
    int m_outSeq = 0;

    // 接收端状态：按 fileId 暂存内存，file_done 时落盘
    QHash<QString, QByteArray> m_inData;
    QHash<QString, QString> m_inName;
    QHash<QString, qint64> m_inSize;
};