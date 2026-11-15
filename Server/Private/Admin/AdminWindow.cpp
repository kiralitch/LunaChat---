#include "Admin/AdminWindow.h"

#include <QDateTime>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QShortcut>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QtDebug>

#include "Network/ClientSession.h"
#include "Network/LunaServer.h"

AdminWindow::AdminWindow(LunaServer *server, QWidget *parent)
    : QWidget(parent), m_server(server)
{
    setWindowTitle(QStringLiteral("LunaChat 服务端管理"));
    resize(760, 560);

    auto *title = new QLabel(QStringLiteral("在线用户"), this);
    title->setStyleSheet(QStringLiteral("font-size:15px; font-weight:bold;"));

    m_table = new QTableWidget(this);
    m_table->setColumnCount(4);
    m_table->setHorizontalHeaderLabels(QStringList()
                                       << QStringLiteral("userId")
                                       << QStringLiteral("昵称")
                                       << QStringLiteral("IP")
                                       << QStringLiteral("端口"));
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->verticalHeader()->setVisible(false);

    m_kickButton = new QPushButton(QStringLiteral("踢下线"), this);
    m_refreshButton = new QPushButton(QStringLiteral("刷新"), this);

    auto *buttonRow = new QHBoxLayout;
    buttonRow->addWidget(m_refreshButton);
    buttonRow->addWidget(m_kickButton);
    buttonRow->addStretch();

    auto *logTitle = new QLabel(QStringLiteral("运行日志"), this);
    logTitle->setStyleSheet(QStringLiteral("font-size:15px; font-weight:bold;"));

    m_logView = new QPlainTextEdit(this);
    m_logView->setReadOnly(true);
    m_logView->setMaximumBlockCount(500);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(title);
    layout->addWidget(m_table, 3);
    layout->addLayout(buttonRow);
    layout->addWidget(logTitle);
    layout->addWidget(m_logView, 4);

    // 快捷键：选中一行后按 Delete 踢下线（演示时比找按钮快）
    auto *deleteShortcut = new QShortcut(QKeySequence::Delete, m_table);
    connect(deleteShortcut, &QShortcut::activated, this, &AdminWindow::kickSelected);

    connect(m_refreshButton, &QPushButton::clicked, this, &AdminWindow::refresh);
    connect(m_kickButton, &QPushButton::clicked, this, &AdminWindow::kickSelected);

    refresh();
}

void AdminWindow::refresh()
{
    if (!m_server)
        return;

    const QList<ClientSession *> sessions = m_server->sessions();
    m_table->setRowCount(0);

    for (ClientSession *session : sessions) {
        if (session->userId() == 0)
            continue;      // 未登录连接不展示

        const int row = m_table->rowCount();
        m_table->insertRow(row);
        m_table->setItem(row, 0, new QTableWidgetItem(QString::number(session->userId())));
        m_table->setItem(row, 1, new QTableWidgetItem(session->nickname()));
        m_table->setItem(row, 2, new QTableWidgetItem(session->peerAddress()));
        m_table->setItem(row, 3, new QTableWidgetItem(QString::number(session->peerPort())));
    }
    m_table->resizeColumnsToContents();
}

void AdminWindow::appendLog(const QString &text)
{
    m_logView->appendPlainText(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss"))
                               + QLatin1Char(' ') + text);
}

void AdminWindow::kickSelected()
{
    const int row = m_table->currentRow();
    if (row < 0 || row >= m_table->rowCount()) {
        appendLog(QStringLiteral("请先选中一行在线用户"));
        return;
    }

    const quint64 userId = m_table->item(row, 0)->text().toULongLong();
    const QString nickname = m_table->item(row, 1)->text();
    if (!m_server->kickUser(userId, QStringLiteral("管理员将其踢下线"))) {
        appendLog(QStringLiteral("踢下线失败：用户 %1 已不在线").arg(nickname));
        return;
    }
    appendLog(QStringLiteral("已踢下线：%1 (userId=%2)").arg(nickname).arg(userId));
    refresh();
}