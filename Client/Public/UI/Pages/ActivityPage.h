#pragma once

#include <QJsonObject>
#include <QWidget>

class QLabel;
class QToolButton;
class QPushButton;
class QTableWidget;
class QStackedLayout;
class LunaClient;

// 群活动页面（内嵌在 MainWindow 的 QStackedWidget 里）
class ActivityPage : public QWidget
{
    Q_OBJECT

public:
    ActivityPage(LunaClient *client, quint64 userId, QWidget *parent = nullptr);

    // 切到本页前调用：指定群 + 是否有管理权限
    void setGroup(qint64 groupId, bool canManage);
    void refresh();

signals:
    void backRequested();   // 顶部返回箭头

private slots:
    void onCreate();
    void onSignup();
    void onCancelSignup();
    void onCancelActivity();
    void onDetail();
    void onFrameReceived(const QJsonObject &frame);

private:
    void requestList();
    void requestDetail(qint64 activityId);
    qint64 selectedActivityId() const;

    LunaClient *m_client = nullptr;
    qint64 m_groupId = 0;
    quint64 m_userId = 0;
    bool m_canManage = false;

    QTableWidget *m_table = nullptr;
    QLabel *m_statusLabel = nullptr;
    QPushButton *m_createButton = nullptr;
    QPushButton *m_cancelButton = nullptr;
    QToolButton *m_backButton = nullptr;
    QStackedLayout *m_tableStack = nullptr;
    QWidget *m_empty = nullptr;
};