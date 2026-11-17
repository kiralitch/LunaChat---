#pragma once

#include <QHash>
#include <QSet>
#include <QJsonArray>
#include <QJsonObject>
#include <QWidget>

class LunaClient;
class MomentCard;
class QLabel;
class QPushButton;
class QScrollArea;
class QVBoxLayout;

// 朋友圈页面（内嵌在 MainWindow 的 QStackedWidget 里，不再是弹窗）
class MomentPage : public QWidget
{
    Q_OBJECT

public:
    MomentPage(LunaClient *client, quint64 userId, const QString &nickname,
               QWidget *parent = nullptr);

    void refresh();

private slots:
    void onPublish();
    void onFrameReceived(const QJsonObject &frame);

private:
    void openUserMoments(quint64 userId, const QString &nickname);
    void setBannerAvatar(const QByteArray &bytes);
    void applyCachedAvatars();   // 把已缓存的头像重新套到当前所有卡片上
    void onPickBackground();
    void applyBannerBackground(const QString &imagePath);
    void rebuild(const QJsonArray &moments);
    void requestImage(qint64 momentId, int index);

    LunaClient *m_client = nullptr;
    quint64 m_userId = 0;
    QString m_nickname;

    QVBoxLayout *m_cardLayout = nullptr;
    QLabel *m_statusLabel = nullptr;
    QWidget *m_banner = nullptr;        // 顶部横幅（背景 + 头像 + 昵称）
    QLabel *m_bannerAvatar = nullptr;
    QLabel *m_bannerName = nullptr;
    QLabel *m_bannerHint = nullptr;
    QPushButton *m_bannerBack = nullptr;   // 看他人朋友圈时的"返回我的"
    QHash<qint64, MomentCard *> m_cards;
    QSet<quint64> m_avatarAsked;
    QHash<quint64, QByteArray> m_userAvatars;   // 头像缓存，切回来能立刻显示
    quint64 m_viewUserId = 0;      // 0 = 我的朋友圈
    QString m_viewNickname;
};