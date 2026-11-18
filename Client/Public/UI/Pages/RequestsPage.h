#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QWidget>

class LunaClient;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QStackedWidget;

// 消息与申请中心：好友申请 / 群聊申请 / 搜索群聊
class RequestsPage : public QWidget
{
    Q_OBJECT

public:
    explicit RequestsPage(LunaClient *client, quint64 selfId, QWidget *parent = nullptr);

    void refreshAll();

signals:
    void backRequested();
    void countsChanged(int friendCount, int groupCount);

private:
    void requestFriendList();
    void requestGroupList();
    void rebuildFriends(const QJsonArray &requests);
    void rebuildGroupRequests(const QJsonArray &requests);
    void setStatus(const QString &text, int tone = 0);
    void switchTab(int index);

    void acceptFriend(quint64 fromUser, QPushButton *button);
    void rejectFriend(quint64 fromUser, QPushButton *button);
    void handleGroupJoin(qint64 requestId, bool accept);

    void onFrameReceived(const QJsonObject &frame);

    LunaClient *m_client = nullptr;
    quint64 m_selfId = 0;

    QPushButton *m_tabFriend = nullptr;
    QPushButton *m_tabGroup = nullptr;
    QStackedWidget *m_pages = nullptr;

    QListWidget *m_friendList = nullptr;
    QListWidget *m_groupList = nullptr;
    QWidget *m_friendEmpty = nullptr;
    QWidget *m_groupEmpty = nullptr;
    QLabel *m_status = nullptr;

    int m_friendCount = 0;
    int m_groupCount = 0;
};
