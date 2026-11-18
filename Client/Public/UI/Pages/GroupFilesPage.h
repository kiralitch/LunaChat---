#pragma once

#include <QByteArray>
#include <QHash>
#include <QJsonObject>
#include <QWidget>

class LunaClient;
class QLabel;
class QLineEdit;
class QListWidget;
class QStackedLayout;
class QToolButton;

// 群共享文件夹页面（覆盖左栏，网盘式布局：搜索 + 新建文件夹 + 列表）
class GroupFilesPage : public QWidget
{
    Q_OBJECT

public:
    GroupFilesPage(LunaClient *client, quint64 userId, QWidget *parent = nullptr);

    void setGroup(qint64 groupId, bool canManage);
    void refresh();
    qint64 groupId() const { return m_groupId; }
    QString currentPath() const { return m_currentPath; }

signals:
    void backRequested();
    void uploadRequested();

private slots:
    void onMkdir();
    void onFrameReceived(const QJsonObject &frame);
    void applyFilter(const QString &text);

private:
    LunaClient *m_client = nullptr;
    quint64 m_userId = 0;
    qint64 m_groupId = 0;
    bool m_canManage = false;
    QString m_folderPath;
    QString m_currentPath;   // 相对群根目录的子路径，空表示根
    QHash<QString, QByteArray> m_downloads;   // 下载中的文件缓存（fileId -> 数据）

    QLabel *m_title = nullptr;
    QLineEdit *m_search = nullptr;
    QToolButton *m_back = nullptr;
    QToolButton *m_mkdir = nullptr;
    QToolButton *m_upload = nullptr;
    QListWidget *m_list = nullptr;
    QLabel *m_status = nullptr;
    QStackedLayout *m_listStack = nullptr;
    QWidget *m_empty = nullptr;
};