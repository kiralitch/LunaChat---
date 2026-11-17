#pragma once

#include <QJsonObject>
#include <QWidget>

class LunaClient;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QToolButton;
class QTextEdit;

// 群资料页面（内嵌在右栏，和群活动一样）
class GroupInfoPage : public QWidget
{
    Q_OBJECT

public:
    GroupInfoPage(LunaClient *client, quint64 selfId, QWidget *parent = nullptr);

    void setGroup(qint64 groupId, const QString &role);
    void refresh();

signals:
    void closed();
    void backRequested();   // 顶部返回箭头      // 退群/解散成功，界面回到聊天

private slots:
    void onPickAvatar();
    void onSave();
    void onLeave();
    void onTransfer();
    void onDissolve();
    void onFrameReceived(const QJsonObject &frame);

private:
    bool isOwner() const { return m_role == QStringLiteral("owner"); }
    bool canManage() const { return isOwner() || m_role == QStringLiteral("admin"); }

    LunaClient *m_client = nullptr;
    qint64 m_groupId = 0;
    quint64 m_selfId = 0;
    QString m_role;
    QByteArray m_pendingAvatar;

    QLabel *m_avatarLabel = nullptr;
    QLineEdit *m_nameEdit = nullptr;
    QTextEdit *m_announcementEdit = nullptr;
    QLineEdit *m_folderEdit = nullptr;
    QListWidget *m_memberList = nullptr;
    QListWidget *m_fileList = nullptr;
    QLabel *m_statusLabel = nullptr;
    QToolButton *m_backButton = nullptr;
    QPushButton *m_saveButton = nullptr;
    QPushButton *m_transferButton = nullptr;
    QPushButton *m_leaveButton = nullptr;
    QPushButton *m_dissolveButton = nullptr;
};