#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QWidget>

class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class LunaClient;

// 加好友页（内嵌整页）：按用户名/昵称关键词搜索；也复用为群内“邀请成员”
class AddFriendPage : public QWidget
{
    Q_OBJECT

public:
    explicit AddFriendPage(LunaClient *client, quint64 selfId, QWidget *parent = nullptr);
    void reset();
    void setInviteGroup(qint64 groupId, const QString &groupName);

signals:
    void backRequested();

private slots:
    void onSearch();
    void onFrameReceived(const QJsonObject &frame);

private:
    void rebuildResults(const QJsonArray &users, const QString &keyword);
    void setStatus(const QString &text, int tone = 0);   // 0 普通 / 1 成功 / 2 错误
    void sendRequest(quint64 userId, const QString &username, QPushButton *button);

    LunaClient *m_client = nullptr;
    quint64 m_selfId = 0;
    QLineEdit *m_search = nullptr;
    QPushButton *m_searchButton = nullptr;
    QLabel *m_title = nullptr;
    QLabel *m_hint = nullptr;
    QLabel *m_status = nullptr;
    QLabel *m_empty = nullptr;
    QListWidget *m_list = nullptr;
    qint64 m_inviteGroupId = 0;
    QString m_inviteGroupName;
};
