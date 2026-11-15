#include "UI/Widgets/GroupInfoDialog.h"

#include <QBuffer>
#include <QComboBox>
#include <QDateTime>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QImage>
#include <QPainter>
#include <QPainterPath>
#include <QJsonArray>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QTextEdit>
#include <QVBoxLayout>

#include "UI/Widgets/AvatarCropper.h"
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
}

GroupInfoDialog::GroupInfoDialog(LunaClient *client, qint64 groupId, quint64 selfId,
                                 const QString &role, QWidget *parent)
    : QDialog(parent), m_client(client), m_groupId(groupId), m_selfId(selfId), m_role(role)
{
    setWindowTitle(QStringLiteral("群资料"));
    resize(560, 560);

    m_avatarLabel = new QLabel(this);
    m_avatarLabel->setFixedSize(88, 88);
    m_avatarLabel->setAlignment(Qt::AlignCenter);
    m_avatarLabel->setStyleSheet(QStringLiteral(
        "border-radius:44px; background:#5865f2; color:white; font-size:32px; font-weight:700;"));

    m_nameEdit = new QLineEdit(this);
    m_announcementEdit = new QTextEdit(this);
    m_announcementEdit->setPlaceholderText(QStringLiteral("群公告"));
    m_announcementEdit->setMaximumHeight(90);
    m_folderEdit = new QLineEdit(this);
    m_folderEdit->setPlaceholderText(QStringLiteral("共享文件夹路径（服务器端）"));

    m_nameEdit->setEnabled(canManage());
    m_announcementEdit->setEnabled(canManage());
    m_folderEdit->setEnabled(canManage());

    m_memberList = new QListWidget(this);
    m_fileList = new QListWidget(this);

    auto *pickAvatar = new QPushButton(QStringLiteral("更换群头像"), this);
    auto *saveButton = new QPushButton(QStringLiteral("保存资料"), this);
    auto *transferButton = new QPushButton(QStringLiteral("转让群主给选中成员"), this);
    auto *leaveButton = new QPushButton(QStringLiteral("退出该群"), this);
    auto *dissolveButton = new QPushButton(QStringLiteral("解散该群"), this);
    auto *closeButton = new QPushButton(QStringLiteral("关闭"), this);

    pickAvatar->setEnabled(canManage());
    saveButton->setEnabled(canManage());
    transferButton->setEnabled(isOwner());
    dissolveButton->setEnabled(isOwner());
    leaveButton->setEnabled(!isOwner());

    m_statusLabel = new QLabel(this);

    auto *row1 = new QHBoxLayout;
    row1->addWidget(m_avatarLabel);
    row1->addWidget(pickAvatar);
    row1->addStretch();

    auto *row2 = new QHBoxLayout;
    row2->addWidget(transferButton);
    row2->addWidget(leaveButton);
    row2->addWidget(dissolveButton);
    row2->addStretch();
    row2->addWidget(closeButton);

    auto *layout = new QVBoxLayout(this);
    layout->addLayout(row1);
    layout->addWidget(new QLabel(QStringLiteral("群名称"), this));
    layout->addWidget(m_nameEdit);
    layout->addWidget(new QLabel(QStringLiteral("群公告"), this));
    layout->addWidget(m_announcementEdit);
    layout->addWidget(new QLabel(QStringLiteral("共享文件夹"), this));
    layout->addWidget(m_folderEdit);
    layout->addWidget(new QLabel(QStringLiteral("群成员（选中后可转让群主）"), this));
    layout->addWidget(m_memberList, 1);
    layout->addWidget(new QLabel(QStringLiteral("已共享文件"), this));
    layout->addWidget(m_fileList, 1);
    layout->addWidget(m_statusLabel);
    layout->addWidget(saveButton);
    layout->addLayout(row2);

    connect(pickAvatar, &QPushButton::clicked, this, &GroupInfoDialog::onPickAvatar);
    connect(saveButton, &QPushButton::clicked, this, &GroupInfoDialog::onSave);
    connect(transferButton, &QPushButton::clicked, this, &GroupInfoDialog::onTransfer);
    connect(leaveButton, &QPushButton::clicked, this, &GroupInfoDialog::onLeave);
    connect(dissolveButton, &QPushButton::clicked, this, &GroupInfoDialog::onDissolve);
    connect(closeButton, &QPushButton::clicked, this, &QDialog::accept);
    connect(m_client, &LunaClient::frameReceived, this, &GroupInfoDialog::onFrameReceived);

    refresh();
}

void GroupInfoDialog::refresh()
{
    QJsonObject data;
    data.insert(QStringLiteral("groupId"), static_cast<double>(m_groupId));
    m_client->sendFrame(makeFrame(MsgType::kGroupInfoGet, data));
    m_client->sendFrame(makeFrame(MsgType::kGroupAvatarGet, data));
}

void GroupInfoDialog::onPickAvatar()
{
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("选择群头像"), QString(), QStringLiteral("图片 (*.png *.jpg *.jpeg *.bmp)"));
    if (path.isEmpty())
        return;

    QImage image(path);
    if (image.isNull())
        return;

    // 和修改个人头像一样：先裁剪（缩放 + 拖动），再压缩上传
    AvatarCropper cropper(image, this);
    if (cropper.exec() != QDialog::Accepted)
        return;
    image = cropper.result();
    if (image.isNull()) {
        m_statusLabel->setText(QStringLiteral("裁剪失败"));
        return;
    }

    QByteArray bytes;
    QBuffer buffer(&bytes);
    buffer.open(QIODevice::WriteOnly);
    image.save(&buffer, "JPG", 85);
    m_pendingAvatar = bytes;

    m_avatarLabel->setPixmap(QPixmap::fromImage(
        image.scaled(88, 88, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation)));
    m_statusLabel->setText(QStringLiteral("已选择群头像，点保存生效"));
}

void GroupInfoDialog::onSave()
{
    QJsonObject data;
    data.insert(QStringLiteral("groupId"), static_cast<double>(m_groupId));
    data.insert(QStringLiteral("name"), m_nameEdit->text().trimmed());
    data.insert(QStringLiteral("announcement"), m_announcementEdit->toPlainText());
    data.insert(QStringLiteral("folderPath"), m_folderEdit->text().trimmed());
    if (!m_pendingAvatar.isEmpty())
        data.insert(QStringLiteral("avatarBase64"), QString::fromLatin1(m_pendingAvatar.toBase64()));
    m_client->sendFrame(makeFrame(MsgType::kGroupInfoUpdate, data));
    m_statusLabel->setText(QStringLiteral("保存中..."));
}

void GroupInfoDialog::onLeave()
{
    if (QMessageBox::question(this, QStringLiteral("退出群聊"),
                              QStringLiteral("确定退出该群吗？")) != QMessageBox::Yes)
        return;
    QJsonObject data;
    data.insert(QStringLiteral("groupId"), static_cast<double>(m_groupId));
    m_client->sendFrame(makeFrame(MsgType::kGroupLeave, data));
}

void GroupInfoDialog::onTransfer()
{
    QListWidgetItem *item = m_memberList->currentItem();
    if (!item) {
        m_statusLabel->setText(QStringLiteral("请先在成员列表里选中一个人"));
        return;
    }
    const quint64 targetId = static_cast<quint64>(item->data(Qt::UserRole).toLongLong());
    if (targetId == m_selfId) {
        m_statusLabel->setText(QStringLiteral("不能转让给自己"));
        return;
    }
    if (QMessageBox::question(this, QStringLiteral("转让群主"),
                              QStringLiteral("确定把群主转让给 %1 吗？").arg(item->text()))
        != QMessageBox::Yes)
        return;

    QJsonObject data;
    data.insert(QStringLiteral("groupId"), static_cast<double>(m_groupId));
    data.insert(QStringLiteral("userId"), static_cast<double>(targetId));
    m_client->sendFrame(makeFrame(MsgType::kGroupTransfer, data));
}

void GroupInfoDialog::onDissolve()
{
    if (QMessageBox::warning(this, QStringLiteral("解散群聊"),
                             QStringLiteral("解散后所有成员都会被移出，确定吗？"),
                             QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
        return;

    QJsonObject data;
    data.insert(QStringLiteral("groupId"), static_cast<double>(m_groupId));
    m_client->sendFrame(makeFrame(MsgType::kGroupDissolve, data));
}

void GroupInfoDialog::onFrameReceived(const QJsonObject &frame)
{
    const QString type = frame.value(QStringLiteral("type")).toString();
    const QJsonObject data = frame.value(QStringLiteral("data")).toObject();

    if (type == MsgType::kGroupInfoResp) {
        if (data.contains(QStringLiteral("ok"))) {
            const bool ok = data.value(QStringLiteral("ok")).toBool();
            m_statusLabel->setText(data.value(QStringLiteral("msg")).toString());
            if (ok) {
                const QString msg = data.value(QStringLiteral("msg")).toString();
                if (msg.contains(QStringLiteral("解散")) || msg.contains(QStringLiteral("退出")))
                    accept();
                else
                    refresh();
            }
            return;
        }

        const QJsonObject info = data.value(QStringLiteral("info")).toObject();
        m_nameEdit->setText(info.value(QStringLiteral("name")).toString());
        m_announcementEdit->setPlainText(info.value(QStringLiteral("announcement")).toString());
        m_folderEdit->setText(info.value(QStringLiteral("folderPath")).toString());
        m_avatarLabel->setText(info.value(QStringLiteral("name")).toString().left(1));

        m_memberList->clear();
        for (const QJsonValue &value : data.value(QStringLiteral("members")).toArray()) {
            const QJsonObject member = value.toObject();
            const quint64 userId = static_cast<quint64>(member.value(QStringLiteral("userId")).toDouble());
            QString roleText = member.value(QStringLiteral("role")).toString();
            if (roleText == QStringLiteral("owner"))
                roleText = QStringLiteral("群主");
            else if (roleText == QStringLiteral("admin"))
                roleText = QStringLiteral("管理员");
            else
                roleText = QStringLiteral("成员");

            auto *item = new QListWidgetItem(
                QStringLiteral("%1（%2）%3").arg(member.value(QStringLiteral("nickname")).toString(),
                                                roleText,
                                                userId == m_selfId ? QStringLiteral(" - 我") : QString()),
                m_memberList);
            item->setData(Qt::UserRole, static_cast<qlonglong>(userId));
        }
        m_fileList->clear();
        for (const QJsonValue &value : data.value(QStringLiteral("files")).toArray()) {
            const QJsonObject fileItem = value.toObject();
            const double sizeKB = fileItem.value(QStringLiteral("fileSize")).toDouble() / 1024.0;
            m_fileList->addItem(QStringLiteral("%1  (%2 KB)  %3  %4")
                                    .arg(fileItem.value(QStringLiteral("fileName")).toString())
                                    .arg(QString::number(sizeKB, 'f', 0),
                                         fileItem.value(QStringLiteral("fromNickname")).toString(),
                                         fileItem.value(QStringLiteral("createdAt")).toString()));
        }
        m_statusLabel->setText(QStringLiteral("成员 %1 人，共享文件 %2 个")
                                   .arg(m_memberList->count())
                                   .arg(m_fileList->count()));
        return;
    }

    if (type == MsgType::kGroupAvatarResp) {
        const QByteArray bytes = QByteArray::fromBase64(
            data.value(QStringLiteral("data")).toString().toLatin1());
        QImage image;
        if (image.loadFromData(bytes)) {
            const int edge = qMin(image.width(), image.height());
            image = image.copy((image.width() - edge) / 2, (image.height() - edge) / 2, edge, edge)
                            .scaled(88, 88, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            QPixmap rounded(88, 88);
            rounded.fill(Qt::transparent);
            QPainter painter(&rounded);
            painter.setRenderHint(QPainter::Antialiasing, true);
            QPainterPath clipPath;
            clipPath.addEllipse(0, 0, 88, 88);
            painter.setClipPath(clipPath);
            painter.drawImage(0, 0, image);
            m_avatarLabel->setPixmap(rounded);
            m_avatarLabel->setText(QString());
        }
        return;
    }

    if (type == MsgType::kError) {
        m_statusLabel->setText(QStringLiteral("错误：%1").arg(data.value(QStringLiteral("msg")).toString()));
    }
}