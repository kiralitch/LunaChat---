#include "UI/Pages/GroupFilesPage.h"

#include <QDateTime>
#include <QFile>
#include <QDir>
#include <QCoreApplication>
#include <QDesktopServices>
#include <QHBoxLayout>
#include <QIcon>
#include <QInputDialog>
#include <QJsonArray>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QAction>
#include <QMessageBox>
#include <QMenu>
#include <QToolButton>
#include <QUrl>
#include <QVBoxLayout>

#include "LunaProtocol/MessageTypes.h"
#include "Network/LunaClient.h"

namespace {
QJsonObject makeFrame(const QString &type, const QJsonObject &data)
{
    QJsonObject frame;
    frame.insert(QStringLiteral("type"), type);
    frame.insert(QStringLiteral("ts"), static_cast<double>(QDateTime::currentMSecsSinceEpoch()));
    frame.insert(QStringLiteral("data"), data);
    return frame;
}

QString sizeText(qint64 bytes)
{
    if (bytes >= 1024 * 1024)
        return QStringLiteral("%1 MB").arg(bytes / 1024.0 / 1024.0, 0, 'f', 1);
    if (bytes >= 1024)
        return QStringLiteral("%1 KB").arg(bytes / 1024.0, 0, 'f', 0);
    return QStringLiteral("%1 B").arg(bytes);
}
}

GroupFilesPage::GroupFilesPage(LunaClient *client, quint64 userId, QWidget *parent)
    : QWidget(parent), m_client(client), m_userId(userId)
{
    setObjectName(QStringLiteral("GroupFilesPage"));

    m_back = new QToolButton(this);
    m_back->setObjectName(QStringLiteral("PageBackButton"));
    m_back->setIcon(QIcon(QStringLiteral(":/icons/back.png")));
    m_back->setIconSize(QSize(26, 26));
    m_back->setFixedSize(40, 40);
    m_back->setAutoRaise(true);
    m_back->setToolTip(QStringLiteral("返回群聊"));
    connect(m_back, &QToolButton::clicked, this, [this]() {
        if (!m_currentPath.isEmpty()) {
            // 返回上一级目录
            const int slash = m_currentPath.lastIndexOf(QLatin1Char('/'));
            m_currentPath = slash > 0 ? m_currentPath.left(slash) : QString();
            refresh();
            return;
        }
        emit backRequested();
    });

    m_title = new QLabel(QStringLiteral("共享文件夹"), this);
    m_title->setObjectName(QStringLiteral("ChatTitle"));

    m_search = new QLineEdit(this);
    m_search->setPlaceholderText(QStringLiteral("搜索文件"));
    m_search->setClearButtonEnabled(true);
    m_search->addAction(QIcon(QStringLiteral(":/icons/search.png")), QLineEdit::LeadingPosition);
    m_search->setFixedWidth(260);
    connect(m_search, &QLineEdit::textChanged, this, &GroupFilesPage::applyFilter);

    m_mkdir = new QToolButton(this);
    m_mkdir->setObjectName(QStringLiteral("ChatIconButton"));
    m_mkdir->setIcon(QIcon(QStringLiteral(":/icons/folder_add.png")));
    m_mkdir->setIconSize(QSize(24, 24));
    m_mkdir->setFixedSize(40, 40);
    m_mkdir->setAutoRaise(true);
    m_mkdir->setToolTip(QStringLiteral("新建文件夹"));
    connect(m_mkdir, &QToolButton::clicked, this, &GroupFilesPage::onMkdir);

    m_upload = new QToolButton(this);
    m_upload->setObjectName(QStringLiteral("ChatIconButton"));
    m_upload->setIcon(QIcon(QStringLiteral(":/icons/file.png")));
    m_upload->setIconSize(QSize(24, 24));
    m_upload->setFixedSize(40, 40);
    m_upload->setAutoRaise(true);
    m_upload->setToolTip(QStringLiteral("上传文件到当前目录"));
    if (m_upload->icon().isNull())
        m_upload->setText(QStringLiteral("↑"));
    connect(m_upload, &QToolButton::clicked, this, &GroupFilesPage::uploadRequested);

    auto *header = new QHBoxLayout;
    header->setContentsMargins(10, 8, 10, 6);
    header->addWidget(m_back);
    header->addWidget(m_title);
    header->addStretch();
    header->addWidget(m_search);
    header->addWidget(m_upload);
    header->addWidget(m_mkdir);

    m_list = new QListWidget(this);
    m_list->setObjectName(QStringLiteral("GroupFilesList"));
    m_list->setIconSize(QSize(26, 26));
    m_list->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_list, &QListWidget::customContextMenuRequested, this, [this](const QPoint &pos) {
        if (!m_canManage)
            return;
        QListWidgetItem *item = m_list->itemAt(pos);
        if (!item)
            return;

        QMenu menu(this);
        QAction *remove = menu.addAction(QStringLiteral("删除"));
        QAction *chosen = menu.exec(m_list->mapToGlobal(pos));
        if (chosen != remove)
            return;

        if (QMessageBox::question(this, QStringLiteral("删除"),
                                  QStringLiteral("确定删除「%1」吗？").arg(item->text()))
            != QMessageBox::Yes)
            return;

        const QString path = m_currentPath.isEmpty() ? item->text()
                                                     : m_currentPath + QLatin1Char('/') + item->text();
        QJsonObject data;
        data.insert(QStringLiteral("groupId"), static_cast<double>(m_groupId));
        data.insert(QStringLiteral("path"), path);
        m_client->sendFrame(makeFrame(MsgType::kGroupFileDelete, data));
    });
    connect(m_list, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem *item) {
        const QString path = item->data(Qt::UserRole + 1).toString();
        if (item->data(Qt::UserRole).toBool()) {
            // 双击目录：进入子目录
            const QString name = item->text();
            m_currentPath = m_currentPath.isEmpty() ? name : m_currentPath + QLatin1Char('/') + name;
            refresh();
        } else {
            // 双击文件：从服务端下载到本地 received/
            const QString rel = m_currentPath.isEmpty()
                                    ? item->text()
                                    : m_currentPath + QLatin1Char('/') + item->text();
            QJsonObject data;
            data.insert(QStringLiteral("groupId"), static_cast<double>(m_groupId));
            data.insert(QStringLiteral("path"), rel);
            m_client->sendFrame(makeFrame(MsgType::kGroupFileDownload, data));
            m_status->setText(QStringLiteral("正在下载 %1...").arg(item->text()));
        }
    });

    m_status = new QLabel(this);
    m_status->setObjectName(QStringLiteral("ChatSubtitle"));

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addLayout(header);
    layout->addWidget(m_list, 1);
    layout->addWidget(m_status);

    connect(m_client, &LunaClient::frameReceived, this, &GroupFilesPage::onFrameReceived);
}

void GroupFilesPage::setGroup(qint64 groupId, bool canManage)
{
    m_groupId = groupId;
    m_currentPath.clear();
    m_canManage = canManage;
    m_mkdir->setEnabled(canManage);
    m_upload->setEnabled(groupId > 0);
    m_title->setText(QStringLiteral("共享文件夹"));
    m_search->clear();
    refresh();
}

void GroupFilesPage::refresh()
{
    if (m_groupId <= 0 || !m_client)
        return;
    QJsonObject data;
    data.insert(QStringLiteral("groupId"), static_cast<double>(m_groupId));
    data.insert(QStringLiteral("path"), m_currentPath);
    m_client->sendFrame(makeFrame(MsgType::kGroupFilesList, data));
}

void GroupFilesPage::onMkdir()
{
    if (m_groupId <= 0)
        return;
    bool ok = false;
    const QString name = QInputDialog::getText(this, QStringLiteral("新建文件夹"),
                                               QStringLiteral("文件夹名称："), QLineEdit::Normal,
                                               QString(), &ok);
    if (!ok || name.trimmed().isEmpty())
        return;

    QJsonObject data;
    data.insert(QStringLiteral("groupId"), static_cast<double>(m_groupId));
    data.insert(QStringLiteral("name"), name.trimmed());
    data.insert(QStringLiteral("path"), m_currentPath);
    m_client->sendFrame(makeFrame(MsgType::kGroupFileMkdir, data));
}

void GroupFilesPage::applyFilter(const QString &text)
{
    for (int i = 0; i < m_list->count(); ++i) {
        QListWidgetItem *item = m_list->item(i);
        item->setHidden(!text.isEmpty() && !item->text().contains(text, Qt::CaseInsensitive));
    }
}

void GroupFilesPage::onFrameReceived(const QJsonObject &frame)
{
    const QString type = frame.value(QStringLiteral("type")).toString();
    const QJsonObject data = frame.value(QStringLiteral("data")).toObject();

    if (type == MsgType::kError) {
        m_status->setText(QStringLiteral("操作失败：%1").arg(data.value(QStringLiteral("msg")).toString()));
        return;
    }

    if (type == MsgType::kGroupFileChunk) {
        const QString fileId = data.value(QStringLiteral("fileId")).toString();
        m_downloads[fileId].append(QByteArray::fromBase64(
            data.value(QStringLiteral("chunkBase64")).toString().toLatin1()));
        return;
    }

    if (type == MsgType::kGroupFileDownloadDone) {
        const QString fileId = data.value(QStringLiteral("fileId")).toString();
        const QString fileName = data.value(QStringLiteral("fileName")).toString();
        const QByteArray bytes = m_downloads.take(fileId);

        const QString dir = QCoreApplication::applicationDirPath() + QStringLiteral("/received");
        QDir().mkpath(dir);
        const QString path = QDir(dir).filePath(fileName);
        QFile file(path);
        if (file.open(QIODevice::WriteOnly)) {
            file.write(bytes);
            file.close();
            m_status->setText(QStringLiteral("已保存到 %1").arg(path));
        } else {
            m_status->setText(QStringLiteral("保存失败：%1").arg(path));
        }
        return;
    }

    if (type == MsgType::kGroupFileDeleteResp) {
        m_status->setText(data.value(QStringLiteral("msg")).toString());
        if (data.value(QStringLiteral("ok")).toBool())
            refresh();
        return;
    }

    if (type == MsgType::kGroupFileMkdir && data.value(QStringLiteral("ok")).toBool()) {
        m_status->setText(QStringLiteral("文件夹已创建"));
        refresh();
        return;
    }

    if (type != MsgType::kGroupFilesResp)
        return;

    if (static_cast<qint64>(data.value(QStringLiteral("groupId")).toDouble()) != m_groupId)
        return;

    m_folderPath = data.value(QStringLiteral("folderPath")).toString();
    m_currentPath = data.value(QStringLiteral("path")).toString();
    m_title->setText(m_currentPath.isEmpty() ? QStringLiteral("共享文件夹")
                                              : QStringLiteral("共享文件夹 / %1").arg(m_currentPath));
    m_list->clear();

    int dirCount = 0;
    int fileCount = 0;
    for (const QJsonValue &value : data.value(QStringLiteral("entries")).toArray()) {
        const QJsonObject entry = value.toObject();
        const bool isDir = entry.value(QStringLiteral("dir")).toBool();
        const QString name = entry.value(QStringLiteral("name")).toString();
        const qint64 size = static_cast<qint64>(entry.value(QStringLiteral("size")).toDouble());

        auto *item = new QListWidgetItem(name, m_list);
        item->setIcon(QIcon(QStringLiteral(":/icons/%1.png")
                                .arg(isDir ? QStringLiteral("folder") : QStringLiteral("file"))));
        item->setData(Qt::UserRole, isDir);
        item->setData(Qt::UserRole + 1, m_folderPath + QLatin1Char('/') + name);
        item->setToolTip(QStringLiteral("%1  %2  %3")
                             .arg(name, isDir ? QString() : sizeText(size),
                                  entry.value(QStringLiteral("modified")).toString()));
        if (isDir)
            ++dirCount;
        else
            ++fileCount;
    }

    m_status->setText(QStringLiteral("共 %1 个文件夹、%2 个文件").arg(dirCount).arg(fileCount));
}