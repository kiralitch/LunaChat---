#include "UI/Pages/CreateGroupPage.h"

#include <QBuffer>
#include <QDateTime>
#include <QFileDialog>
#include <QFrame>
#include <QHBoxLayout>
#include <QIcon>
#include <QImage>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QTextEdit>
#include <QToolButton>
#include <QVBoxLayout>
#include <QtDebug>

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

QPixmap circularPixmap(const QImage &image, int side)
{
    if (image.isNull())
        return QPixmap();

    const int edge = qMin(image.width(), image.height());
    const QImage cropped = image.copy((image.width() - edge) / 2, (image.height() - edge) / 2, edge, edge)
                               .scaled(side, side, Qt::KeepAspectRatio, Qt::SmoothTransformation);

    QPixmap result(side, side);
    result.fill(Qt::transparent);
    QPainter painter(&result);
    painter.setRenderHint(QPainter::Antialiasing, true);
    QPainterPath path;
    path.addEllipse(0, 0, side, side);
    painter.setClipPath(path);
    painter.drawImage(0, 0, cropped);
    return result;
}

}   // namespace

CreateGroupPage::CreateGroupPage(LunaClient *client, QWidget *parent)
    : QWidget(parent), m_client(client)
{
    setObjectName(QStringLiteral("CreateGroupPage"));

    auto *back = new QToolButton(this);
    back->setObjectName(QStringLiteral("PageBackButton"));
    back->setIcon(QIcon(QStringLiteral(":/icons/back.png")));
    back->setIconSize(QSize(26, 26));
    back->setFixedSize(40, 40);
    back->setAutoRaise(true);
    back->setCursor(Qt::PointingHandCursor);
    back->setToolTip(QStringLiteral("返回聊天"));
    connect(back, &QToolButton::clicked, this, &CreateGroupPage::backRequested);

    auto *title = new QLabel(QStringLiteral("新建群聊"), this);
    title->setObjectName(QStringLiteral("ChatTitle"));
    auto *hint = new QLabel(QStringLiteral("设置群头像与群资料，创建后你就是群主"), this);
    hint->setObjectName(QStringLiteral("ChatSubtitle"));

    auto *titleBox = new QVBoxLayout;
    titleBox->setContentsMargins(0, 0, 0, 0);
    titleBox->setSpacing(0);
    titleBox->addWidget(title);
    titleBox->addWidget(hint);

    auto *header = new QHBoxLayout;
    header->setContentsMargins(0, 0, 0, 0);
    header->setSpacing(8);
    header->addWidget(back);
    header->addLayout(titleBox, 1);

    m_avatar = new QLabel(this);
    m_avatar->setObjectName(QStringLiteral("GroupAvatarBig"));
    m_avatar->setFixedSize(84, 84);
    m_avatar->setAlignment(Qt::AlignCenter);
    m_avatar->setText(QStringLiteral("群"));

    auto *pickButton = new QPushButton(QStringLiteral("选择群头像"), this);
    pickButton->setCursor(Qt::PointingHandCursor);
    connect(pickButton, &QPushButton::clicked, this, &CreateGroupPage::onPickAvatar);

    auto *avatarCol = new QVBoxLayout;
    avatarCol->setSpacing(8);
    avatarCol->addWidget(m_avatar, 0, Qt::AlignHCenter);
    avatarCol->addWidget(pickButton, 0, Qt::AlignHCenter);

    m_name = new QLineEdit(this);
    m_name->setPlaceholderText(QStringLiteral("群名称（1-32 字）"));
    m_name->setMaxLength(32);

    m_announcement = new QTextEdit(this);
    m_announcement->setPlaceholderText(QStringLiteral("群公告（可选）：上课地点、活动规则等"));
    m_announcement->setFixedHeight(110);

    m_status = new QLabel(this);
    m_status->setObjectName(QStringLiteral("ChatSubtitle"));
    m_status->setWordWrap(true);

    m_createButton = new QPushButton(QStringLiteral("创建群聊"), this);
    m_createButton->setObjectName(QStringLiteral("PrimaryButton"));
    m_createButton->setFixedWidth(140);
    m_createButton->setCursor(Qt::PointingHandCursor);
    connect(m_createButton, &QPushButton::clicked, this, &CreateGroupPage::onCreate);

    auto caption = [this](const QString &text) {
        auto *label = new QLabel(text, this);
        label->setObjectName(QStringLiteral("LoginCaption"));
        return label;
    };

    auto *form = new QVBoxLayout;
    form->setSpacing(8);
    form->addWidget(caption(QStringLiteral("群名称")));
    form->addWidget(m_name);
    form->addWidget(caption(QStringLiteral("群公告")));
    form->addWidget(m_announcement);

    auto *card = new QFrame(this);
    card->setObjectName(QStringLiteral("SettingsCard"));
    auto *cardLayout = new QHBoxLayout(card);
    cardLayout->setContentsMargins(24, 22, 24, 22);
    cardLayout->setSpacing(24);
    cardLayout->addLayout(avatarCol, 0);
    cardLayout->addLayout(form, 1);

    auto *actions = new QHBoxLayout;
    actions->setContentsMargins(0, 0, 0, 0);
    actions->addWidget(m_status, 1);
    actions->addWidget(m_createButton);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(18, 8, 18, 14);
    layout->setSpacing(12);
    layout->addLayout(header);
    layout->addWidget(card);
    layout->addLayout(actions);
    layout->addStretch();

    connect(m_client, &LunaClient::frameReceived, this, &CreateGroupPage::onFrameReceived);
}

void CreateGroupPage::reset()
{
    m_name->clear();
    m_announcement->clear();
    m_avatarBytes.clear();
    m_pendingGroupId = 0;
    m_creating = false;
    m_createButton->setEnabled(true);
    m_createButton->setText(QStringLiteral("创建群聊"));
    m_avatar->setPixmap(QPixmap());
    m_avatar->setText(QStringLiteral("群"));
    setStatus(QString());
}

void CreateGroupPage::onPickAvatar()
{
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("选择群头像"), QString(),
        QStringLiteral("图片 (*.png *.jpg *.jpeg *.bmp *.webp)"));
    if (path.isEmpty())
        return;

    QImage image(path);
    if (image.isNull()) {
        setStatus(QStringLiteral("图片加载失败"), 2);
        return;
    }
    image = image.scaled(256, 256, Qt::KeepAspectRatio, Qt::SmoothTransformation);

    // 服务端限制 200KB：按质量递减压到限制内
    QByteArray bytes;
    for (int quality = 88; quality >= 40; quality -= 8) {
        QBuffer buffer(&bytes);
        buffer.open(QIODevice::WriteOnly);
        if (!image.save(&buffer, "JPG", quality))
            continue;
        if (bytes.size() <= 200 * 1024)
            break;
    }

    if (bytes.isEmpty() || bytes.size() > 200 * 1024) {
        setStatus(QStringLiteral("头像压缩后仍过大，请换一张"), 2);
        return;
    }

    m_avatarBytes = bytes;
    updateAvatarPreview();
    setStatus(QStringLiteral("已选择群头像，创建时会一并保存"), 1);
}

void CreateGroupPage::updateAvatarPreview()
{
    QImage image;
    image.loadFromData(m_avatarBytes);
    const QPixmap pixmap = circularPixmap(image, 84);
    if (!pixmap.isNull()) {
        m_avatar->setPixmap(pixmap);
        m_avatar->setText(QString());
    }
}

void CreateGroupPage::onCreate()
{
    if (m_creating)
        return;

    const QString name = m_name->text().trimmed();
    if (name.isEmpty()) {
        setStatus(QStringLiteral("请先填写群名称"), 2);
        return;
    }
    if (name.size() > 32) {
        setStatus(QStringLiteral("群名称需为 1-32 个字符"), 2);
        return;
    }

    m_creating = true;
    m_createButton->setEnabled(false);
    m_createButton->setText(QStringLiteral("创建中…"));
    setStatus(QStringLiteral("正在创建群聊…"));
    sendCreate();
}

void CreateGroupPage::sendCreate()
{
    QJsonObject data;
    data.insert(QStringLiteral("name"), m_name->text().trimmed());
    m_client->sendFrame(makeFrame(MsgType::kGroupCreate, data));
}

void CreateGroupPage::onFrameReceived(const QJsonObject &frame)
{
    const QString type = frame.value(QStringLiteral("type")).toString();
    const QJsonObject data = frame.value(QStringLiteral("data")).toObject();

    if (type == MsgType::kGroupCreateResp) {
        if (!data.value(QStringLiteral("ok")).toBool()) {
            m_creating = false;
            m_createButton->setEnabled(true);
            m_createButton->setText(QStringLiteral("创建群聊"));
            setStatus(data.value(QStringLiteral("msg")).toString(), 2);
            return;
        }

        m_pendingGroupId = static_cast<qint64>(data.value(QStringLiteral("groupId")).toDouble());
        if (m_pendingGroupId <= 0) {
            m_creating = false;
            m_createButton->setEnabled(true);
            m_createButton->setText(QStringLiteral("创建群聊"));
            setStatus(QStringLiteral("建群失败：没有拿到群号"), 2);
            return;
        }

        // 群已建好，接着把头像/公告写入群资料
        QJsonObject update;
        update.insert(QStringLiteral("groupId"), static_cast<double>(m_pendingGroupId));
        update.insert(QStringLiteral("name"), m_name->text().trimmed());
        update.insert(QStringLiteral("announcement"), m_announcement->toPlainText());
        update.insert(QStringLiteral("folderPath"), QString());
        if (!m_avatarBytes.isEmpty())
            update.insert(QStringLiteral("avatarBase64"), QString::fromLatin1(m_avatarBytes.toBase64()));
        m_client->sendFrame(makeFrame(MsgType::kGroupInfoUpdate, update));
        setStatus(QStringLiteral("群创建成功，正在保存群资料…"));
        return;
    }

    if (type == MsgType::kGroupInfoResp) {
        if (!data.contains(QStringLiteral("ok")))
            return;   // 其它群资料回包不归本页处理

        if (!data.value(QStringLiteral("ok")).toBool()) {
            m_creating = false;
            m_createButton->setEnabled(true);
            m_createButton->setText(QStringLiteral("创建群聊"));
            setStatus(data.value(QStringLiteral("msg")).toString(), 2);
            return;
        }

        const qint64 groupId = m_pendingGroupId;
        const QString createdName = m_name->text().trimmed();
        reset();
        if (groupId > 0)
            emit groupCreated(groupId, createdName);
        return;
    }

    if (type == MsgType::kError) {
        m_creating = false;
        m_createButton->setEnabled(true);
        m_createButton->setText(QStringLiteral("创建群聊"));
        setStatus(QStringLiteral("创建失败：%1").arg(data.value(QStringLiteral("msg")).toString()), 2);
    }
}

void CreateGroupPage::setStatus(const QString &text, int tone)
{
    static const char *const colors[] = { "#9C93C4", "#6FE3B4", "#FF8E8E" };
    m_status->setText(text);
    m_status->setStyleSheet(QStringLiteral("color: %1; background: transparent;")
                                .arg(QString::fromLatin1(colors[qBound(0, tone, 2)])));
}
