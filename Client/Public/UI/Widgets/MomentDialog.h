#pragma once

#include <QDialog>
#include <QHash>
#include <QJsonArray>
#include <QJsonObject>

class LunaClient;
class MomentCard;
class QLabel;
class QScrollArea;
class QVBoxLayout;

// 朋友圈：滚动卡片列表 + 发布（文字 + ≤3 图）+ 点赞/评论/删除
class MomentDialog : public QDialog
{
    Q_OBJECT

public:
    MomentDialog(LunaClient *client, quint64 userId, const QString &nickname,
                 QWidget *parent = nullptr);

private slots:
    void onPublish();
    void onRefresh();
    void onFrameReceived(const QJsonObject &frame);

private:
    void rebuild(const QJsonArray &moments);
    void requestImage(qint64 momentId, int index);

    LunaClient *m_client = nullptr;
    quint64 m_userId = 0;
    QString m_nickname;

    QVBoxLayout *m_cardLayout = nullptr;
    QLabel *m_statusLabel = nullptr;
    QHash<qint64, MomentCard *> m_cards;
};