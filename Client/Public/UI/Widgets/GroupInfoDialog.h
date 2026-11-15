#pragma once

#include <QDialog>
#include <QJsonObject>

class LunaClient;
class QComboBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QTextEdit;

// 群资料面板：群名/群公告/共享文件夹/群头像，含退群、转让群主、解散群
class GroupInfoDialog : public QDialog
{
    Q_OBJECT

public:
    GroupInfoDialog(LunaClient *client, qint64 groupId, quint64 selfId,
                    const QString &role, QWidget *parent = nullptr);

private slots:
    void onPickAvatar();
    void onSave();
    void onLeave();
    void onTransfer();
    void onDissolve();
    void onFrameReceived(const QJsonObject &frame);

private:
    void refresh();
    bool isOwner() const { return m_role == QStringLiteral("owner"); }
    bool canManage() const { return m_role == QStringLiteral("owner") || m_role == QStringLiteral("admin"); }

    LunaClient *m_client = nullptr;
    qint64 m_groupId = 0;
    quint64 m_selfId = 0;
    QString m_role;
    QByteArray m_pendingAvatar;

    QLineEdit *m_nameEdit = nullptr;
    QTextEdit *m_announcementEdit = nullptr;
    QLineEdit *m_folderEdit = nullptr;
    QListWidget *m_memberList = nullptr;
    QListWidget *m_fileList = nullptr;
    QLabel *m_avatarLabel = nullptr;
    QLabel *m_statusLabel = nullptr;
};