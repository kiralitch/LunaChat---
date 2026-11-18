#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QWidget>

class LunaClient;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QStackedLayout;

// 群聊搜索页：未输入关键词时展示数据库随机推荐的群
class GroupSearchPage : public QWidget
{
    Q_OBJECT

public:
    explicit GroupSearchPage(LunaClient *client, quint64 selfId, QWidget *parent = nullptr);

    void refresh();

signals:
    void backRequested();

private slots:
    void onSearch();
    void onFrameReceived(const QJsonObject &frame);

private:
    void rebuildResults(const QJsonArray &groups, const QString &keyword);
    void requestJoin(qint64 groupId, QPushButton *button);
    void setStatus(const QString &text, int tone = 0);

    LunaClient *m_client = nullptr;
    quint64 m_selfId = 0;
    QLineEdit *m_search = nullptr;
    QLabel *m_section = nullptr;
    QLabel *m_status = nullptr;
    QLabel *m_emptyTitle = nullptr;
    QWidget *m_empty = nullptr;
    QListWidget *m_list = nullptr;
    QStackedLayout *m_stack = nullptr;
};
