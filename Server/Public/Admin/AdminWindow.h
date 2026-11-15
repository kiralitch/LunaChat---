#pragma once

#include <QWidget>

class QPlainTextEdit;
class QPushButton;
class QTableWidget;
class LunaServer;

// 服务端管理界面：在线用户表 + 踢下线 + 实时日志
class AdminWindow : public QWidget
{
    Q_OBJECT

public:
    explicit AdminWindow(LunaServer *server, QWidget *parent = nullptr);

public slots:
    void refresh();
    void appendLog(const QString &text);

private slots:
    void kickSelected();

private:
    LunaServer *m_server = nullptr;
    QTableWidget *m_table = nullptr;
    QPlainTextEdit *m_logView = nullptr;
    QPushButton *m_kickButton = nullptr;
    QPushButton *m_refreshButton = nullptr;
};