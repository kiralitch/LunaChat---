#pragma once

#include <QByteArray>
#include <QJsonObject>
#include <QWidget>

class QLabel;
class QLineEdit;
class QPushButton;
class QTextEdit;
class LunaClient;

// 新建群聊页（内嵌整页）：直接编辑群头像、群名称、群公告，创建后自动保存资料
class CreateGroupPage : public QWidget
{
    Q_OBJECT

public:
    explicit CreateGroupPage(LunaClient *client, QWidget *parent = nullptr);
    void reset();

signals:
    void backRequested();
    void groupCreated(qint64 groupId, const QString &name);

private slots:
    void onPickAvatar();
    void onCreate();
    void onFrameReceived(const QJsonObject &frame);

private:
    void setStatus(const QString &text, int tone = 0);
    void updateAvatarPreview();
    void sendCreate();

    LunaClient *m_client = nullptr;
    QLabel *m_avatar = nullptr;
    QByteArray m_avatarBytes;
    QLineEdit *m_name = nullptr;
    QTextEdit *m_announcement = nullptr;
    QLabel *m_status = nullptr;
    QPushButton *m_createButton = nullptr;
    qint64 m_pendingGroupId = 0;
    bool m_creating = false;
};
