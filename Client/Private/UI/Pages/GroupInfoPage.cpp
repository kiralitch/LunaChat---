#include "UI/Pages/GroupInfoPage.h"

#include <QBuffer>
#include <QDateTime>
#include <QDialog>
#include <QFileDialog>
#include <QFrame>
#include <QHBoxLayout>
#include <QImage>
#include <QJsonArray>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPainter>
#include <QPainterPath>
#include <QIcon>
#include <QPushButton>
#include <QToolButton>
#include <QTextEdit>
#include <QVBoxLayout>

#include "LunaProtocol/MessageTypes.h"
#include "Network/LunaClient.h"
#include "UI/Widgets/AvatarCropper.h"

namespace {

QJsonObject makeFrame(const QString &type, const QJsonObject &data)
{
    QJsonObject frame;
    frame.insert(QStringLiteral("type"), type);
    frame.insert(QStringLiteral("ts"), static_cast<double>(QDateTime::currentMSecsSinceEpoch()));
    frame.insert(QStringLiteral("data"), data);
    return frame;
}

QPixmap circlePixmap(const QByteArray &bytes, int side)
{
    QImage image;
    if (!image.loadFromData(bytes))
        return QPixmap();

    const int edge = qMin(image.width(), image.height());
    image = image.copy((image.width() - edge) / 2, (image.height() - edge) / 2, edge, edge)
                    .scaled(side, side, Qt::KeepAspectRatio, Qt::SmoothTransformation);

    QPixmap result(side, side);
    result.fill(Qt::transparent);
    QPainter painter(&result);
    painter.setRenderHint(QPainter::Antialiasing, true);
    QPainterPath path;
    path.addEllipse(0, 0, side, side);
    painter.setClipPath(path);
    painter.drawImage(0, 0, image);
    return result;
}

}

GroupInfoPage::GroupInfoPage(LunaClient *client, quint64 selfId, QWidget *parent)
    : QWidget(parent), m_client(client), m_selfId(selfId)
{
    m_avatarLabel = new QLabel(this);
    m_avatarLabel->setObjectName(QStringLiteral("GroupAvatarBig"));
    m_avatarLabel->setFixedSize(84, 84);
    m_avatarLabel->setAlignment(Qt::AlignCenter);

    m_nameEdit = new QLineEdit(this);

    auto *pickAvatar = new QPushButton(QStringLiteral("更换群头像"), this);
    auto *avatarRow = new QHBoxLayout;
    avatarRow->setSpacing(16);
    avatarRow->addWidget(m_avatarLabel, 0, Qt::AlignTop);

    auto *nameCol = new QVBoxLayout;
    nameCol->setSpacing(6);
    auto *nameCaption = new QLabel(QStringLiteral("群名称"), this);
    nameCaption->setObjectName(QStringLiteral("LoginCaption"));
    nameCol->addWidget(nameCaption);
    nameCol->addWidget(m_nameEdit);
    nameCol->addWidget(pickAvatar, 0, Qt::AlignLeft);
    avatarRow->addLayout(nameCol, 1);

    m_announcementEdit = new QTextEdit(this);
    m_announcementEdit->setMaximumHeight(90);
    m_folderEdit = new QLineEdit(this);
    m_folderEdit->setPlaceholderText(QStringLiteral("服务器端共享文件夹路径"));

    m_memberList = new QListWidget(this);
    m_fileList = new QListWidget(this);

    m_statusLabel = new QLabel(this);
    m_statusLabel->setObjectName(QStringLiteral("ChatSubtitle"));

    auto *saveButton = new QPushButton(QStringLiteral("保存资料"), this);
    saveButton->setObjectName(QStringLiteral("PrimaryButton"));
    auto *transferButton = new QPushButton(QStringLiteral("转让群主给选中成员"), this);
    auto *leaveButton = new QPushButton(QStringLiteral("退出该群"), this);
    auto *dissolveButton = new QPushButton(QStringLiteral("解散该群"), this);

    m_saveButton = saveButton;
    m_transferButton = transferButton;
    m_leaveButton = leaveButton;
    m_dissolveButton = dissolveButton;

    auto *buttonRow = new QHBoxLayout;
    buttonRow->addWidget(saveButton);
    buttonRow->addStretch();
    buttonRow->addWidget(transferButton);
    buttonRow->addWidget(leaveButton);
    buttonRow->addWidget(dissolveButton);

    auto *pageTitle = new QLabel(QStringLiteral("群资料"), this);
    pageTitle->setObjectName(QStringLiteral("ChatTitle"));
    auto *pageHint = new QLabel(QStringLiteral("群头像、公告、成员与共享文件"), this);
    pageHint->setObjectName(QStringLiteral("ChatSubtitle"));

    m_backButton = new QToolButton(this);
    m_backButton->setObjectName(QStringLiteral("PageBackButton"));
    m_backButton->setIcon(QIcon(QStringLiteral(":/icons/back.png")));
    m_backButton->setIconSize(QSize(26, 26));
    m_backButton->setFixedSize(40, 40);
    m_backButton->setAutoRaise(true);
    m_backButton->setCursor(Qt::PointingHandCursor);
    m_backButton->setToolTip(QStringLiteral("返回聊天"));
    connect(m_backButton, &QToolButton::clicked, this, &GroupInfoPage::backRequested);

    auto *titleBox = new QVBoxLayout;
    titleBox->setContentsMargins(0, 0, 0, 0);
    titleBox->setSpacing(0);
    titleBox->addWidget(pageTitle);
    titleBox->addWidget(pageHint);

    auto *pageHeader = new QHBoxLayout;
    pageHeader->setContentsMargins(0, 0, 0, 4);
    pageHeader->addWidget(m_backButton);
    pageHeader->addLayout(titleBox, 1);

    auto caption = [this](const QString &text) {
        auto *label = new QLabel(text, this);
        label->setObjectName(QStringLiteral("LoginCaption"));
        return label;
    };

    auto *identityCard = new QFrame(this);
    identityCard->setObjectName(QStringLiteral("SettingsCard"));
    auto *identityLayout = new QVBoxLayout(identityCard);
    identityLayout->setContentsMargins(20, 18, 20, 18);
    identityLayout->setSpacing(8);
    identityLayout->addLayout(avatarRow);
    identityLayout->addWidget(caption(QStringLiteral("群公告")));
    identityLayout->addWidget(m_announcementEdit);
    identityLayout->addWidget(caption(QStringLiteral("共享文件夹")));
    identityLayout->addWidget(m_folderEdit);

    auto *listsCard = new QFrame(this);
    listsCard->setObjectName(QStringLiteral("SettingsCard"));
    auto *lists = new QHBoxLayout(listsCard);
    lists->setContentsMargins(20, 16, 20, 16);
    lists->setSpacing(20);

    auto *memberBox = new QVBoxLayout;
    memberBox->setSpacing(6);
    memberBox->addWidget(caption(QStringLiteral("群成员")));
    memberBox->addWidget(m_memberList, 1);

    auto *fileBox = new QVBoxLayout;
    fileBox->setSpacing(6);
    fileBox->addWidget(caption(QStringLiteral("已共享文件")));
    fileBox->addWidget(m_fileList, 1);

    lists->addLayout(memberBox, 1);
    lists->addLayout(fileBox, 1);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(18, 8, 18, 14);
    layout->setSpacing(12);
    layout->addLayout(pageHeader);
    layout->addWidget(identityCard);
    layout->addWidget(listsCard, 1);
    layout->addWidget(m_statusLabel);
    layout->addLayout(buttonRow);

    connect(pickAvatar, &QPushButton::clicked, this, &GroupInfoPage::onPickAvatar);
    connect(saveButton, &QPushButton::clicked, this, &GroupInfoPage::onSave);
    connect(transferButton, &QPushButton::clicked, this, &GroupInfoPage::onTransfer);
    connect(leaveButton, &QPushButton::clicked, this, &GroupInfoPage::onLeave);
    connect(dissolveButton, &QPushButton::clicked, this, &GroupInfoPage::onDissolve);
    connect(m_client, &LunaClient::frameReceived, this, &GroupInfoPage::onFrameReceived);

    // 注意：构造阶段不发任何请求，等 MainWindow 调 setGroup() 之后再发
    m_saveButton->setEnabled(false);
    m_transferButton->setEnabled(false);
    m_leaveButton->setEnabled(false);
    m_dissolveButton->setEnabled(false);
}

void GroupInfoPage::setGroup(qint64 groupId, const QString &role)
{
    m_groupId = groupId;
    m_role = role;
    m_pendingAvatar.clear();

    const bool manage = canManage();
    m_nameEdit->setEnabled(manage);
    m_announcementEdit->setEnabled(manage);
    m_folderEdit->setEnabled(manage);
    m_saveButton->setEnabled(manage);
    m_transferButton->setEnabled(isOwner());
    m_dissolveButton->setEnabled(isOwner());
    m_leaveButton->setEnabled(!isOwner());

    refresh();
}

void GroupInfoPage::refresh()
{
    if (m_groupId <= 0 || !m_client)
        return;

    QJsonObject data;
    data.insert(QStringLiteral("groupId"), static_cast<double>(m_groupId));
    m_client->sendFrame(makeFrame(MsgType::kGroupInfoGet, data));
    m_client->sendFrame(makeFrame(MsgType::kGroupAvatarGet, data));
}

void GroupInfoPage::onPickAvatar()
{
    if (m_groupId <= 0)
        return;

    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("选择群头像"), QString(), QStringLiteral("图片 (*.png *.jpg *.jpeg *.bmp)"));
    if (path.isEmpty())
        return;

    QImage image(path);
    if (image.isNull())
        return;

    AvatarCropper cropper(image, QSize(256, 256), this);
    if (cropper.exec() != QDialog::Accepted)
        return;

    image = cropper.result();
    if (image.isNull())
        return;

    QByteArray bytes;
    QBuffer buffer(&bytes);
    buffer.open(QIODevice::WriteOnly);
    image.save(&buffer, "JPG", 85);
    m_pendingAvatar = bytes;

    m_avatarLabel->setPixmap(circlePixmap(bytes, 84));
    m_avatarLabel->setText(QString());
    m_statusLabel->setText(QStringLiteral("已选择群头像，点「保存资料」生效"));
}

void GroupInfoPage::onSave()
{
    if (m_groupId <= 0)
        return;

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

void GroupInfoPage::onTransfer()
{
    QListWidgetItem *item = m_memberList->currentItem();
    if (m_groupId <= 0 || !item) {
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

void GroupInfoPage::onLeave()
{
    if (m_groupId <= 0)
        return;
    if (QMessageBox::question(this, QStringLiteral("退出群聊"), QStringLiteral("确定退出该群吗？"))
        != QMessageBox::Yes)
        return;

    QJsonObject data;
    data.insert(QStringLiteral("groupId"), static_cast<double>(m_groupId));
    m_client->sendFrame(makeFrame(MsgType::kGroupLeave, data));
}

void GroupInfoPage::onDissolve()
{
    if (m_groupId <= 0)
        return;
    if (QMessageBox::warning(this, QStringLiteral("解散群聊"),
                             QStringLiteral("解散后所有成员都会被移出，确定吗？"))
        != QMessageBox::Yes)
        return;

    QJsonObject data;
    data.insert(QStringLiteral("groupId"), static_cast<double>(m_groupId));
    m_client->sendFrame(makeFrame(MsgType::kGroupDissolve, data));
}

void GroupInfoPage::onFrameReceived(const QJsonObject &frame)
{
    const QString type = frame.value(QStringLiteral("type")).toString();
    const QJsonObject data = frame.value(QStringLiteral("data")).toObject();

    if (type == MsgType::kGroupAvatarResp) {
        const qint64 groupId = static_cast<qint64>(data.value(QStringLiteral("groupId")).toDouble());
        if (groupId != m_groupId)
            return;
        const QByteArray bytes = QByteArray::fromBase64(
            data.value(QStringLiteral("data")).toString().toLatin1());
        if (bytes.isEmpty())
            return;
        m_avatarLabel->setPixmap(circlePixmap(bytes, 84));
        m_avatarLabel->setText(QString());
        return;
    }

    if (type == MsgType::kError) {
        m_statusLabel->setText(QStringLiteral("错误：%1").arg(data.value(QStringLiteral("msg")).toString()));
        return;
    }

    if (type != MsgType::kGroupInfoResp)
        return;

    // 带 ok 的是"操作回执"，不带的才是查询结果
    if (data.contains(QStringLiteral("ok"))) {
        const bool ok = data.value(QStringLiteral("ok")).toBool();
        const QString msg = data.value(QStringLiteral("msg")).toString();
        m_statusLabel->setText(msg);
        if (ok && (msg.contains(QStringLiteral("解散")) || msg.contains(QStringLiteral("退出")))) {
            emit closed();
            return;
        }
        if (ok)
            refresh();
        return;
    }

    const QJsonObject info = data.value(QStringLiteral("info")).toObject();
    if (static_cast<qint64>(info.value(QStringLiteral("groupId")).toDouble()) != m_groupId)
        return;

    m_nameEdit->setText(info.value(QStringLiteral("name")).toString());
    m_announcementEdit->setPlainText(info.value(QStringLiteral("announcement")).toString());
    m_folderEdit->setText(info.value(QStringLiteral("folderPath")).toString());
    if (m_avatarLabel->pixmap(Qt::ReturnByValue).isNull())
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
            QStringLiteral("%1（%2）%3").arg(member.value(QStringLiteral("nickname")).toString(), roleText,
                                             userId == m_selfId ? QStringLiteral(" - 我") : QString()),
            m_memberList);
        item->setData(Qt::UserRole, static_cast<qlonglong>(userId));
    }

    m_fileList->clear();
    for (const QJsonValue &value : data.value(QStringLiteral("files")).toArray()) {
        const QJsonObject fileItem = value.toObject();
        m_fileList->addItem(QStringLiteral("%1  (%2 KB)  %3")
                                .arg(fileItem.value(QStringLiteral("fileName")).toString())
                                .arg(fileItem.value(QStringLiteral("fileSize")).toDouble() / 1024.0, 0, 'f', 0)
                                .arg(fileItem.value(QStringLiteral("fromNickname")).toString()));
    }

    m_statusLabel->setText(QStringLiteral("成员 %1 人，共享文件 %2 个")
                               .arg(m_memberList->count())
                               .arg(m_fileList->count()));
}
