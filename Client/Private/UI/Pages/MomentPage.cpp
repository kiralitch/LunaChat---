#include "UI/Pages/MomentPage.h"

#include <QBuffer>
#include <QDateTime>
#include <QDialog>
#include <QFileDialog>
#include <QFrame>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QIcon>
#include <QImage>
#include <QPainter>
#include <QPainterPath>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScreen>
#include <QScrollArea>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>
#include <QtDebug>

#include "LunaProtocol/MessageTypes.h"
#include "Network/LunaClient.h"
#include "UI/Widgets/AvatarCropper.h"

namespace {

const int kMaxImages = 3;
const int kMaxImageBytes = 200 * 1024;

QJsonObject makeFrame(const QString &type, const QJsonObject &data)
{
    QJsonObject frame;
    frame.insert(QStringLiteral("type"), type);
    frame.insert(QStringLiteral("ts"), static_cast<double>(QDateTime::currentMSecsSinceEpoch()));
    frame.insert(QStringLiteral("data"), data);
    return frame;
}

// 客户端压缩：最长边 720，JPEG 质量逐级下调直到 ≤200KB（不引任何图像库，全靠 QImage）
QByteArray compressImage(const QString &path)
{
    QImage image(path);
    if (image.isNull())
        return QByteArray();
    if (image.width() > 720)
        image = image.scaledToWidth(720, Qt::SmoothTransformation);

    for (int quality = 75; quality >= 35; quality -= 10) {
        QByteArray bytes;
        QBuffer buffer(&bytes);
        buffer.open(QIODevice::WriteOnly);
        if (!image.save(&buffer, "JPG", quality))
            return QByteArray();
        if (bytes.size() <= kMaxImageBytes)
            return bytes;
    }
    return QByteArray();
}

}

// 单条动态卡片：不继承 Q_OBJECT，用 std::function 回调把事件抛给对话框
class MomentCard : public QFrame
{
public:
    explicit MomentCard(QWidget *parent = nullptr) : QFrame(parent)
    {
        setObjectName(QStringLiteral("MomentCard"));
        setAttribute(Qt::WA_StyledBackground, true);
        setFrameShape(QFrame::NoFrame);
        auto *layout = new QVBoxLayout(this);

        // 作者头像：点一下进入他的朋友圈
        m_avatar = new QToolButton(this);
        m_avatar->setObjectName(QStringLiteral("MomentAvatar"));
        m_avatar->setFixedSize(44, 44);
        m_avatar->setIconSize(QSize(44, 44));
        m_avatar->setAutoRaise(true);
        m_avatar->setCursor(Qt::PointingHandCursor);
        connect(m_avatar, &QToolButton::clicked, this, [this]() {
            if (onOpenAuthor && m_userId != 0)
                onOpenAuthor(m_userId);
        });

        m_header = new QLabel(this);
        m_header->setStyleSheet(QStringLiteral("font-weight:bold;"));
        m_content = new QLabel(this);
        m_content->setWordWrap(true);

        auto *imageRow = new QHBoxLayout;
        for (int i = 0; i < kMaxImages; ++i) {
            // 用 QToolButton 而不是 QLabel：按钮一定能收到点击，缩略图点一下看大图
            auto *slot = new QToolButton(this);
            slot->setFixedSize(110, 110);
            slot->setIconSize(QSize(110, 110));
            slot->setAutoRaise(true);
            slot->setVisible(false);
            m_images.append(slot);
            imageRow->addWidget(slot);
        }
        imageRow->addStretch();

        auto *buttonRow = new QHBoxLayout;
        m_likeButton = new QPushButton(QStringLiteral("赞"), this);
        m_commentButton = new QPushButton(QStringLiteral("评论"), this);
        m_deleteButton = new QPushButton(QStringLiteral("删除"), this);
        m_commentsLabel = new QLabel(this);
        m_commentsLabel->setWordWrap(true);
        m_commentsLabel->setObjectName(QStringLiteral("MomentComments"));
        m_commentsLabel->setTextFormat(Qt::PlainText);
        buttonRow->addWidget(m_likeButton);
        buttonRow->addWidget(m_commentButton);
        buttonRow->addStretch();
        buttonRow->addWidget(m_deleteButton);

        auto *headerRow = new QHBoxLayout;
        headerRow->setSpacing(8);
        headerRow->addWidget(m_avatar, 0, Qt::AlignTop);
        headerRow->addWidget(m_header, 1);
        layout->addLayout(headerRow);
        layout->addWidget(m_content);
        layout->addLayout(imageRow);
        layout->addWidget(m_commentsLabel);
        layout->addLayout(buttonRow);

        connect(m_likeButton, &QPushButton::clicked, this, [this]() { if (onLike) onLike(); });
        connect(m_commentButton, &QPushButton::clicked, this, [this]() { if (onComment) onComment(); });
        connect(m_deleteButton, &QPushButton::clicked, this, [this]() { if (onDelete) onDelete(); });
    }

    void setMoment(const QJsonObject &moment, bool mine)
    {
        momentId = static_cast<qint64>(moment.value(QStringLiteral("momentId")).toDouble());
        m_userId = static_cast<quint64>(moment.value(QStringLiteral("userId")).toDouble());
        if (m_userId == 0)
            m_userId = static_cast<quint64>(moment.value(QStringLiteral("user_id")).toDouble());
        m_header->setText(QStringLiteral("%1   %2")
                              .arg(moment.value(QStringLiteral("nickname")).toString(),
                                   moment.value(QStringLiteral("createdAt")).toString()));
        m_content->setText(moment.value(QStringLiteral("content")).toString());

        const int likeCount = moment.value(QStringLiteral("likeCount")).toInt();
        const bool liked = moment.value(QStringLiteral("liked")).toBool();
        m_likeButton->setText(QStringLiteral("赞 %1").arg(likeCount));
        m_likeButton->setStyleSheet(liked ? QStringLiteral("color:#c33;") : QString());

        QStringList commentLines;
        for (const QJsonValue &value : moment.value(QStringLiteral("comments")).toArray()) {
            const QJsonObject comment = value.toObject();
            commentLines << QStringLiteral("%1：%2")
                                .arg(comment.value(QStringLiteral("nickname")).toString(),
                                     comment.value(QStringLiteral("content")).toString());
        }
        if (commentLines.size() > 3) {
            const int more = commentLines.size() - 3;
            commentLines = commentLines.mid(0, 3);
            commentLines << QStringLiteral("查看全部 %1 条评论").arg(more + 3);
        }
        m_commentsLabel->setText(commentLines.join(QLatin1Char('\n')));
        m_commentsLabel->setVisible(!commentLines.isEmpty());
        m_deleteButton->setVisible(mine);
        setLetterAvatar(moment.value(QStringLiteral("nickname")).toString());

        const int imageCount = moment.value(QStringLiteral("imageCount")).toInt();
        for (int i = 0; i < m_images.size(); ++i)
            m_images.at(i)->setVisible(i < imageCount);
    }

    quint64 userId() const { return m_userId; }

    // 没有头像时的占位：蓝底圆形 + 昵称首字
    void setLetterAvatar(const QString &letter)
    {
        QPixmap pixmap(44, 44);
        pixmap.fill(Qt::transparent);
        QPainter painter(&pixmap);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(QStringLiteral("#5865f2")));
        painter.drawEllipse(0, 0, 44, 44);
        painter.setPen(Qt::white);
        QFont font = painter.font();
        font.setBold(true);
        font.setPixelSize(18);
        painter.setFont(font);
        painter.drawText(QRect(0, 0, 44, 44), Qt::AlignCenter,
                         letter.isEmpty() ? QStringLiteral("?") : letter.left(1));
        m_avatar->setIcon(QIcon(pixmap));
        m_avatar->setIconSize(QSize(44, 44));
    }

    void setAvatar(const QByteArray &bytes)
    {
        QImage image;
        if (!image.loadFromData(bytes))
            return;
        const int edge = qMin(image.width(), image.height());
        image = image.copy((image.width() - edge) / 2, (image.height() - edge) / 2, edge, edge)
                        .scaled(44, 44, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        QPixmap rounded(44, 44);
        rounded.fill(Qt::transparent);
        QPainter painter(&rounded);
        painter.setRenderHint(QPainter::Antialiasing, true);
        QPainterPath clip;
        clip.addEllipse(0, 0, 44, 44);
        painter.setClipPath(clip);
        painter.drawImage(0, 0, image);
        m_avatar->setIcon(QIcon(rounded));
        m_avatar->setIconSize(QSize(44, 44));
    }

    void setImage(int index, const QImage &image)
    {
        if (index < 0 || index >= m_images.size() || image.isNull())
            return;

        QToolButton *button = m_images.at(index);
        const QSize target = button->iconSize();
        QPixmap rounded(target);
        rounded.fill(Qt::transparent);
        QPainter painter(&rounded);
        painter.setRenderHint(QPainter::Antialiasing, true);
        QPainterPath clip;
        clip.addRoundedRect(QRectF(0, 0, target.width(), target.height()), 8, 8);
        painter.setClipPath(clip);
        painter.drawImage(0, 0, image.scaled(target, Qt::KeepAspectRatioByExpanding,
                                             Qt::SmoothTransformation));
        button->setIcon(QIcon(rounded));

        // 点缩略图弹窗看大图：按屏幕可用区域 85% 缩放，原图数据不变
        disconnect(button, nullptr, this, nullptr);
        connect(button, &QToolButton::clicked, this, [this, image]() {
            QDialog viewer(this);
            viewer.setWindowTitle(QStringLiteral("查看图片"));
            auto *view = new QLabel(&viewer);
            const QSize limit = QGuiApplication::primaryScreen()->availableSize() * 0.85;
            view->setPixmap(QPixmap::fromImage(
                image.scaled(limit, Qt::KeepAspectRatio, Qt::SmoothTransformation)));
            auto *layout = new QVBoxLayout(&viewer);
            layout->addWidget(view);
            viewer.exec();
        });
    }

    qint64 momentId = 0;
    std::function<void()> onLike;
    std::function<void()> onComment;
    std::function<void()> onDelete;
    std::function<void(quint64)> onOpenAuthor;   // 点头像看他的朋友圈

private:
    QToolButton *m_avatar = nullptr;
    quint64 m_userId = 0;
    QLabel *m_header = nullptr;
    QLabel *m_content = nullptr;
    QLabel *m_commentsLabel = nullptr;
    QPushButton *m_likeButton = nullptr;
    QPushButton *m_commentButton = nullptr;
    QPushButton *m_deleteButton = nullptr;
    QVector<QToolButton *> m_images;
};

MomentPage::MomentPage(LunaClient *client, quint64 userId, const QString &nickname, QWidget *parent)
    : QWidget(parent), m_client(client), m_userId(userId), m_nickname(nickname)
{
    setObjectName(QStringLiteral("MomentPage"));
    resize(760, 620);

    // ===== 顶部个人横幅：渐变背景 + 圆形头像 + 昵称 =====
    m_banner = new QWidget(this);
    m_banner->setObjectName(QStringLiteral("MomentBanner"));
    m_banner->setFixedHeight(130);

    m_bannerAvatar = new QLabel(m_banner);
    m_bannerAvatar->setObjectName(QStringLiteral("MomentBannerAvatar"));
    m_bannerAvatar->setFixedSize(64, 64);
    m_bannerAvatar->setAlignment(Qt::AlignCenter);
    m_bannerAvatar->setText(m_nickname.left(1));

    m_bannerName = new QLabel(m_nickname, m_banner);
    m_bannerName->setObjectName(QStringLiteral("MomentBannerName"));
    m_bannerHint = new QLabel(this);
    m_bannerHint->setVisible(false);   // 这行文字按需求去掉
    m_bannerHint->setObjectName(QStringLiteral("MomentBannerHint"));

    m_bannerBack = new QPushButton(m_banner);
    m_bannerBack->setObjectName(QStringLiteral("MomentBackButton"));
    m_bannerBack->setIcon(QIcon(QStringLiteral(":/icons/back.png")));
    m_bannerBack->setIconSize(QSize(24, 24));
    m_bannerBack->setFixedSize(40, 40);
    m_bannerBack->setToolTip(QStringLiteral("返回我的朋友圈"));
    m_bannerBack->setCursor(Qt::PointingHandCursor);
    m_bannerBack->setVisible(false);

    auto *bgButton = new QPushButton(QStringLiteral("更换背景"), m_banner);
    bgButton->setObjectName(QStringLiteral("MomentBgButton"));
    connect(bgButton, &QPushButton::clicked, this, &MomentPage::onPickBackground);

    auto *bannerLayout = new QVBoxLayout(m_banner);
    bannerLayout->setContentsMargins(12, 8, 12, 12);
    bannerLayout->setSpacing(4);

    // 第一行：左上角返回按钮，右上角"更换背景"
    auto *bannerTop = new QHBoxLayout;
    bannerTop->addWidget(m_bannerBack, 0, Qt::AlignLeft);
    bannerTop->addStretch();
    bannerTop->addWidget(bgButton, 0, Qt::AlignRight);
    bannerLayout->addLayout(bannerTop);

    // 昵称放在第二行
    auto *nameBox = new QVBoxLayout;
    nameBox->setSpacing(2);
    nameBox->addWidget(m_bannerName);
    nameBox->addWidget(m_bannerHint);

    // 第二行：头像 + 昵称
    auto *bannerBottom = new QHBoxLayout;
    bannerBottom->setSpacing(10);
    bannerBottom->addWidget(m_bannerAvatar, 0, Qt::AlignBottom);
    bannerBottom->addLayout(nameBox);
    bannerBottom->addStretch();
    bannerLayout->addLayout(bannerBottom);


    auto *scroll = new QScrollArea(this);
    scroll->setObjectName(QStringLiteral("MomentScroll"));
    scroll->viewport()->setObjectName(QStringLiteral("MomentViewport"));
    scroll->setWidgetResizable(true);
    auto *container = new QWidget(scroll);
    container->setObjectName(QStringLiteral("MomentContainer"));
    m_cardLayout = new QVBoxLayout(container);
    m_cardLayout->addWidget(m_banner);   // 横幅在滚动区内，往下滚会一起滚走
    m_cardLayout->addStretch();
    scroll->setWidget(container);

    m_statusLabel = new QLabel(QStringLiteral("加载中..."), this);

    auto *publishButton = new QPushButton(QStringLiteral("发布动态"), this);
    auto *refreshButton = new QPushButton(QStringLiteral("刷新"), this);

    auto *buttonRow = new QHBoxLayout;
    buttonRow->addWidget(publishButton);
    buttonRow->addWidget(refreshButton);
    buttonRow->addStretch();

    auto *layout = new QVBoxLayout(this);

    layout->addWidget(scroll, 1);
    layout->addWidget(m_statusLabel);
    layout->addLayout(buttonRow);

    connect(publishButton, &QPushButton::clicked, this, &MomentPage::onPublish);
    connect(refreshButton, &QPushButton::clicked, this, &MomentPage::refresh);
    connect(m_bannerBack, &QPushButton::clicked, this, [this]() { openUserMoments(m_userId, QString()); });
    connect(m_client, &LunaClient::frameReceived, this, &MomentPage::onFrameReceived);

    refresh();
}

void MomentPage::openUserMoments(quint64 userId, const QString &nickname)
{
    if (userId == 0 || userId == m_userId) {
        m_viewUserId = 0;
        m_viewNickname.clear();
        m_bannerName->setText(m_nickname);
        m_bannerHint->setText(QStringLiteral("我的朋友圈"));
        m_bannerBack->setVisible(false);
        m_bannerAvatar->setText(m_nickname.left(1));
        m_bannerAvatar->setPixmap(QPixmap());
        if (m_userAvatars.contains(m_userId)) {
            setBannerAvatar(m_userAvatars.value(m_userId));   // 缓存里有，立刻恢复
        } else {
            QJsonObject selfAvatar;
            selfAvatar.insert(QStringLiteral("userId"), static_cast<double>(m_userId));
            m_client->sendFrame(makeFrame(MsgType::kAvatarGet, selfAvatar));
        }
        m_cards.clear();
        m_avatarAsked.clear();
        refresh();
        return;
    }

    m_viewUserId = userId;
    m_viewNickname = nickname;
    m_bannerName->setText(nickname.isEmpty() ? QStringLiteral("TA 的朋友圈") : nickname);
    m_bannerHint->setText(QStringLiteral("TA 的朋友圈"));
    m_bannerBack->setVisible(true);
    m_bannerAvatar->setText(nickname.isEmpty() ? QStringLiteral("友") : nickname.left(1));
    m_bannerAvatar->setPixmap(QPixmap());

    QJsonObject avatarReq;
    avatarReq.insert(QStringLiteral("userId"), static_cast<double>(userId));
    m_client->sendFrame(makeFrame(MsgType::kAvatarGet, avatarReq));

    m_cards.clear();
    m_avatarAsked.clear();
    refresh();
}

void MomentPage::applyCachedAvatars()
{
    // 统一入口：只要缓存里有这个作者的头像，就刷到卡片上
    for (MomentCard *card : m_cards) {
        if (!card)
            continue;
        const quint64 uid = card->userId();
        if (uid != 0 && m_userAvatars.contains(uid))
            card->setAvatar(m_userAvatars.value(uid));
    }
}

void MomentPage::onPickBackground()
{
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("选择朋友圈背景"), QString(), QStringLiteral("图片 (*.png *.jpg *.jpeg *.bmp)"));
    if (path.isEmpty())
        return;

    QImage image(path);
    if (image.isNull())
        return;

    const QString dir = QCoreApplication::applicationDirPath() + QStringLiteral("/backgrounds");
    QDir().mkpath(dir);
    const QString target = dir + QStringLiteral("/moment_%1.jpg").arg(m_userId);
    // 用裁剪器按 10:3 取景，输出 1200×360
    AvatarCropper cropper(image, QSize(1200, 360), this);
    if (cropper.exec() != QDialog::Accepted)
        return;
    QImage cropped = cropper.result();
    if (cropped.isNull())
        return;
    cropped.save(target, "JPG", 90);

    applyBannerBackground(target);

    // 上传到服务端，别人点进我的朋友圈也能看到这张背景
    QFile uploaded(target);
    if (uploaded.open(QIODevice::ReadOnly)) {
        QJsonObject bgData;
        bgData.insert(QStringLiteral("data"),
                      QString::fromLatin1(uploaded.readAll().toBase64()));
        uploaded.close();
        m_client->sendFrame(makeFrame(MsgType::kMomentBgUpdate, bgData));
    }
}

void MomentPage::applyBannerBackground(const QString &imagePath)
{
    if (imagePath.isEmpty() || !m_banner)
        return;

    // 用 QSS 的 background-image 铺满横幅（裁剪后的正方形会拉伸铺满）
    m_banner->setStyleSheet(
        QStringLiteral("QWidget#MomentBanner { border-image: url(%1) 0 0 0 0 stretch stretch; }")
            .arg(imagePath));
}

void MomentPage::setBannerAvatar(const QByteArray &bytes)
{
    QImage image;
    if (!image.loadFromData(bytes))
        return;
    const int edge = qMin(image.width(), image.height());
    image = image.copy((image.width() - edge) / 2, (image.height() - edge) / 2, edge, edge)
                    .scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    QPixmap rounded(64, 64);
    rounded.fill(Qt::transparent);
    QPainter painter(&rounded);
    painter.setRenderHint(QPainter::Antialiasing, true);
    QPainterPath clip;
    clip.addEllipse(0, 0, 64, 64);
    painter.setClipPath(clip);
    painter.drawImage(0, 0, image);
    m_bannerAvatar->setPixmap(rounded);
    m_bannerAvatar->setText(QString());
}

void MomentPage::refresh()
{
    QJsonObject data;
    data.insert(QStringLiteral("page"), 1);
    data.insert(QStringLiteral("pageSize"), 10);
    if (m_viewUserId > 0)
        data.insert(QStringLiteral("userId"), static_cast<double>(m_viewUserId));
    m_client->sendFrame(makeFrame(MsgType::kMomentList, data));

    // 连带拉取"正在看的人"的背景
    QJsonObject bgReq;
    bgReq.insert(QStringLiteral("userId"),
                  static_cast<double>(m_viewUserId > 0 ? m_viewUserId : m_userId));
    m_client->sendFrame(makeFrame(MsgType::kMomentBgGet, bgReq));
}

void MomentPage::requestImage(qint64 momentId, int index)
{
    QJsonObject data;
    data.insert(QStringLiteral("momentId"), static_cast<double>(momentId));
    data.insert(QStringLiteral("index"), index);
    m_client->sendFrame(makeFrame(MsgType::kMomentImage, data));
}

void MomentPage::rebuild(const QJsonArray &moments)
{
    m_avatarAsked.clear();   // 重建后允许重新请求（否则刷新后头像会被清掉）
    // 清掉旧卡片：进过别人的朋友圈再回来时，避免新旧卡片叠在一起
    while (m_cardLayout->count() > 0) {
        QLayoutItem *oldItem = m_cardLayout->takeAt(0);
        if (!oldItem)
            break;
        if (QWidget *widget = oldItem->widget()) {
            if (widget != m_banner)
                widget->deleteLater();
        }
        delete oldItem;
    }
    m_cards.clear();
    m_cardLayout->addWidget(m_banner);
    for (MomentCard *card : m_cards)
        card->deleteLater();
    m_cards.clear();


    for (const QJsonValue &value : moments) {
        const QJsonObject moment = value.toObject();
        auto *card = new MomentCard;
        const bool mine = (static_cast<quint64>(moment.value(QStringLiteral("userId")).toDouble()) == m_userId);        card->setMoment(moment, mine);
        card->onOpenAuthor = [this](quint64 authorId) {
            openUserMoments(authorId, QString());
        };

        // 每个作者的头像只请求一次
        m_cards.insert(card->momentId, card);   // 先登记，头像响应才能找到这张卡
        const quint64 authorId = card->userId();
        if (authorId != 0 && m_userAvatars.contains(authorId))
            card->setAvatar(m_userAvatars.value(authorId));
        else if (authorId != 0 && !m_avatarAsked.contains(authorId)) {
            m_avatarAsked.insert(authorId);
            QJsonObject avatarReq;
            avatarReq.insert(QStringLiteral("userId"), static_cast<double>(authorId));
            m_client->sendFrame(makeFrame(MsgType::kAvatarGet, avatarReq));
        }

        const qint64 momentId = card->momentId;
        card->onLike = [this, momentId]() {
            QJsonObject data;
            data.insert(QStringLiteral("momentId"), static_cast<double>(momentId));
            m_client->sendFrame(makeFrame(MsgType::kMomentLike, data));
        };
        card->onComment = [this, momentId]() {
            bool ok = false;
            const QString text = QInputDialog::getText(this, QStringLiteral("评论"),
                                                       QStringLiteral("写点什么："),
                                                       QLineEdit::Normal, QString(), &ok);
            if (!ok || text.trimmed().isEmpty())
                return;
            QJsonObject data;
            data.insert(QStringLiteral("momentId"), static_cast<double>(momentId));
            data.insert(QStringLiteral("content"), text.trimmed());
            m_client->sendFrame(makeFrame(MsgType::kMomentComment, data));
        };
        card->onDelete = [this, momentId]() {
            QJsonObject data;
            data.insert(QStringLiteral("momentId"), static_cast<double>(momentId));
            m_client->sendFrame(makeFrame(MsgType::kMomentDelete, data));
        };

        m_cards.insert(momentId, card);
        m_cardLayout->addWidget(card);

        const int imageCount = moment.value(QStringLiteral("imageCount")).toInt();
        for (int i = 0; i < imageCount; ++i)
            requestImage(momentId, i);
    }
    m_cardLayout->addStretch();
    m_statusLabel->setText(QStringLiteral("共 %1 条动态").arg(moments.size()));
    QTimer::singleShot(0, this, [this]() { applyCachedAvatars(); });
}

void MomentPage::onPublish()
{
    QDialog form(this);
    form.setWindowTitle(QStringLiteral("发布动态"));
    auto *text = new QPlainTextEdit(&form);
    text->setPlaceholderText(QStringLiteral("这一刻的想法..."));

    QStringList picked;
    auto *previewLabel = new QLabel(&form);
    previewLabel->setStyleSheet(QStringLiteral("color:#666;"));

    auto *pickButton = new QPushButton(QStringLiteral("添加图片（最多 3 张）"), &form);
    connect(pickButton, &QPushButton::clicked, &form, [&]() {
        const QStringList files = QFileDialog::getOpenFileNames(
            &form, QStringLiteral("选择图片"), QString(),
            QStringLiteral("图片 (*.png *.jpg *.jpeg *.bmp)"));
        for (const QString &file : files) {
            if (picked.size() >= kMaxImages)
                break;
            picked << file;
        }
        previewLabel->setText(picked.isEmpty() ? QString()
                                               : QStringLiteral("已选 %1 张").arg(picked.size()));
    });

    auto *okButton = new QPushButton(QStringLiteral("发布"), &form);
    auto *cancelButton = new QPushButton(QStringLiteral("取消"), &form);
    auto *buttonRow = new QHBoxLayout;
    buttonRow->addStretch();
    buttonRow->addWidget(okButton);
    buttonRow->addWidget(cancelButton);

    auto *layout = new QVBoxLayout(&form);
    layout->addWidget(text, 1);
    layout->addWidget(pickButton);
    layout->addWidget(previewLabel);
    layout->addLayout(buttonRow);

    connect(okButton, &QPushButton::clicked, &form, &QDialog::accept);
    connect(cancelButton, &QPushButton::clicked, &form, &QDialog::reject);

    if (form.exec() != QDialog::Accepted)
        return;
    if (text->toPlainText().trimmed().isEmpty() && picked.isEmpty()) {
        m_statusLabel->setText(QStringLiteral("内容不能为空"));
        return;
    }

    QJsonArray images;
    for (const QString &file : picked) {
        const QByteArray bytes = compressImage(file);
        if (bytes.isEmpty()) {
            m_statusLabel->setText(QStringLiteral("图片压缩失败或超过 200KB：%1").arg(file));
            return;
        }
        images.append(QString::fromLatin1(bytes.toBase64()));
    }

    QJsonObject data;
    data.insert(QStringLiteral("content"), text->toPlainText().trimmed());
    data.insert(QStringLiteral("imageCount"), images.size());
    data.insert(QStringLiteral("images"), images);
    m_client->sendFrame(makeFrame(MsgType::kMomentPublish, data));
    m_statusLabel->setText(QStringLiteral("发布中..."));
}

void MomentPage::onFrameReceived(const QJsonObject &frame)
{
    const QString type = frame.value(QStringLiteral("type")).toString();
    const QJsonObject data = frame.value(QStringLiteral("data")).toObject();

    if (type == MsgType::kAvatarResp) {
        const quint64 avatarUser = static_cast<quint64>(data.value(QStringLiteral("userId")).toDouble());
        const QByteArray bytes = QByteArray::fromBase64(
            data.value(QStringLiteral("data")).toString().toLatin1());
        if (bytes.isEmpty())
            return;

        m_userAvatars.insert(avatarUser, bytes);   // 缓存头像，重建列表时直接复用

        // 卡片上的作者头像
        for (MomentCard *card : m_cards) {
            if (card && card->userId() == avatarUser)
                card->setAvatar(bytes);
        }

        // 顶部横幅：我的头像 / 正在看的那个人
        const quint64 bannerUser = m_viewUserId > 0 ? m_viewUserId : m_userId;
        if (avatarUser == bannerUser)
            setBannerAvatar(bytes);
        return;
    }

    if (type == MsgType::kMomentBgResp) {
        const QJsonObject bgData = frame.value(QStringLiteral("data")).toObject();
        if (bgData.contains(QStringLiteral("ok")))
            return;   // 这是"上传成功"回执，不清背景
        const QByteArray bytes = QByteArray::fromBase64(
            bgData.value(QStringLiteral("data")).toString().toLatin1());
        if (bytes.isEmpty()) {
            m_banner->setStyleSheet(QString());   // 没设置背景 → 回退默认渐变
            return;
        }

        // 落成本地临时文件，再用 border-image 铺满横幅
        const QString dir = QCoreApplication::applicationDirPath() + QStringLiteral("/backgrounds");
        QDir().mkpath(dir);
        const QString path = dir + QStringLiteral("/view_bg_%1.jpg")
                                        .arg(static_cast<quint64>(bgData.value(QStringLiteral("userId")).toDouble()));
        QFile file(path);
        if (file.open(QIODevice::WriteOnly)) {
            file.write(bytes);
            file.close();
            applyBannerBackground(path);
        }
        return;
    }

    if (type == MsgType::kMomentListResp) {
        rebuild(data.value(QStringLiteral("moments")).toArray());
        return;
    }

    if (type == MsgType::kMomentImageResp) {
        const qint64 momentId = static_cast<qint64>(data.value(QStringLiteral("momentId")).toDouble());
        MomentCard *card = m_cards.value(momentId, nullptr);
        if (!card)
            return;
        QImage image;
        image.loadFromData(QByteArray::fromBase64(
            data.value(QStringLiteral("data")).toString().toLatin1()));
        card->setImage(data.value(QStringLiteral("index")).toInt(), image);
        return;
    }

    if (type == MsgType::kMomentLikeResp || type == MsgType::kMomentCommentResp
        || type == MsgType::kMomentDeleteResp) {
        refresh();
        return;
    }

    if (type == MsgType::kMomentNotify) {
        m_statusLabel->setText(data.value(QStringLiteral("message")).toString());
        return;
    }

    if (type == MsgType::kError) {
        m_statusLabel->setText(QStringLiteral("错误：%1").arg(data.value(QStringLiteral("msg")).toString()));
        return;
    }
}
