#pragma once

#include <QDialog>
#include <QJsonObject>

class QLabel;
class QTableWidget;
class LunaClient;

// 群活动面板：列表 + 报名/取消报名 + 群主发布/取消 + 详情（含名单）
class ActivityDialog : public QDialog
{
    Q_OBJECT

public:
    ActivityDialog(LunaClient *client, qint64 groupId, quint64 userId,
                   bool canManage, QWidget *parent = nullptr);

private slots:
    void onRefresh();
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
};