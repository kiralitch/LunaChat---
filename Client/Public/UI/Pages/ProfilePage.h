#pragma once

#include <QWidget>
#include <QJsonObject>

class LunaClient;
class QLabel;
class QLineEdit;

// 个人资料页（内嵌页面，不是弹窗）：头像、昵称、账号信息
class ProfilePage : public QWidget
{
    Q_OBJECT

public:
    ProfilePage(LunaClient *client, quint64 userId, QWidget *parent = nullptr);

    void refresh();

signals:
    void nicknameChanged(const QString &nickname);

private slots:
    void onPickAvatar();
    void onSave();
    void onFrameReceived(const QJsonObject &frame);

private:
    void applyAvatar(const QByteArray &bytes);

    LunaClient *m_client = nullptr;
    quint64 m_userId = 0;

    QLabel *m_avatarLabel = nullptr;
    QLabel *m_usernameLabel = nullptr;
    QLabel *m_statusLabel = nullptr;
    QLineEdit *m_nicknameEdit = nullptr;
    QByteArray m_pendingAvatar;
    QString m_loadedNickname;   // 服务端当前昵称，用来判断是否真的改了
};