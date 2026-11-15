#include "UI/Widgets/MomentDialog.h"

#include <QBuffer>
#include <QDateTime>
#include <QDialog>
#include <QFileDialog>
#include <QFrame>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QIcon>
#include <QImage>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScreen>
#include <QScrollArea>
#include <QToolButton>
#include <QVBoxLayout>
#include <QtDebug>

#include "LunaProtocol/MessageTypes.h"
#include "Network/LunaClient.h"

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
        setFrameShape(QFrame::StyledPanel);
        auto *layout = new QVBoxLayout(this);

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
        m_commentsLabel->setStyleSheet(QStringLiteral("color:#666;"));
        buttonRow->addWidget(m_likeButton);
        buttonRow->addWidget(m_commentButton);
        buttonRow->addWidget(m_commentsLabel, 1);
        buttonRow->addWidget(m_deleteButton);

        layout->addWidget(m_header);
        layout->addWidget(m_content);
        layout->addLayout(imageRow);
        layout->addLayout(buttonRow);

        connect(m_likeButton, &QPushButton::clicked, this, [this]() { if (onLike) onLike(); });
        connect(m_commentButton, &QPushButton::clicked, this, [this]() { if (onComment) onComment(); });
        connect(m_deleteButton, &QPushButton::clicked, this, [this]() { if (onDelete) onDelete(); });
    }

    void setMoment(const QJsonObject &moment, bool mine)
    {
        momentId = static_cast<qint64>(moment.value(QStringLiteral("momentId")).toDouble());
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
        m_commentsLabel->setText(commentLines.join(QStringLiteral("    ")));
        m_deleteButton->setVisible(mine);

        const int imageCount = moment.value(QStringLiteral("imageCount")).toInt();
        for (int i = 0; i < m_images.size(); ++i)
            m_images.at(i)->setVisible(i < imageCount);
    }

    void setImage(int index, const QImage &image)
    {
        if (index < 0 || index >= m_images.size() || image.isNull())
            return;

        QToolButton *button = m_images.at(index);
        button->setIcon(QIcon(QPixmap::fromImage(
            image.scaled(button->iconSize(), Qt::KeepAspectRatio, Qt::SmoothTransformation))));

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

private:
    QLabel *m_header = nullptr;
    QLabel *m_content = nullptr;
    QLabel *m_commentsLabel = nullptr;
    QPushButton *m_likeButton = nullptr;
    QPushButton *m_commentButton = nullptr;
    QPushButton *m_deleteButton = nullptr;
    QVector<QToolButton *> m_images;
};

MomentDialog::MomentDialog(LunaClient *client, quint64 userId, const QString &nickname, QWidget *parent)
    : QDialog(parent), m_client(client), m_userId(userId), m_nickname(nickname)
{
    setWindowTitle(QStringLiteral("朋友圈"));
    resize(760, 620);

    auto *scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    auto *container = new QWidget(scroll);
    m_cardLayout = new QVBoxLayout(container);
    m_cardLayout->addStretch();
    scroll->setWidget(container);

    m_statusLabel = new QLabel(QStringLiteral("加载中..."), this);

    auto *publishButton = new QPushButton(QStringLiteral("发布动态"), this);
    auto *refreshButton = new QPushButton(QStringLiteral("刷新"), this);
    auto *closeButton = new QPushButton(QStringLiteral("关闭"), this);

    auto *buttonRow = new QHBoxLayout;
    buttonRow->addWidget(publishButton);
    buttonRow->addWidget(refreshButton);
    buttonRow->addStretch();
    buttonRow->addWidget(closeButton);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(scroll, 1);
    layout->addWidget(m_statusLabel);
    layout->addLayout(buttonRow);

    connect(publishButton, &QPushButton::clicked, this, &MomentDialog::onPublish);
    connect(refreshButton, &QPushButton::clicked, this, &MomentDialog::onRefresh);
    connect(closeButton, &QPushButton::clicked, this, &QDialog::accept);
    connect(m_client, &LunaClient::frameReceived, this, &MomentDialog::onFrameReceived);

    onRefresh();
}

void MomentDialog::onRefresh()
{
    QJsonObject data;
    data.insert(QStringLiteral("page"), 1);
    data.insert(QStringLiteral("pageSize"), 10);
    m_client->sendFrame(makeFrame(MsgType::kMomentList, data));
}

void MomentDialog::requestImage(qint64 momentId, int index)
{
    QJsonObject data;
    data.insert(QStringLiteral("momentId"), static_cast<double>(momentId));
    data.insert(QStringLiteral("index"), index);
    m_client->sendFrame(makeFrame(MsgType::kMomentImage, data));
}

void MomentDialog::rebuild(const QJsonArray &moments)
{
    for (MomentCard *card : m_cards)
        card->deleteLater();
    m_cards.clear();

    while (m_cardLayout->count() > 0) {
        QLayoutItem *item = m_cardLayout->takeAt(0);
        delete item;
    }

    for (const QJsonValue &value : moments) {
        const QJsonObject moment = value.toObject();
        auto *card = new MomentCard;
        const bool mine = (static_cast<quint64>(moment.value(QStringLiteral("userId")).toDouble()) == m_userId);
        card->setMoment(moment, mine);

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
}

void MomentDialog::onPublish()
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

void MomentDialog::onFrameReceived(const QJsonObject &frame)
{
    const QString type = frame.value(QStringLiteral("type")).toString();
    const QJsonObject data = frame.value(QStringLiteral("data")).toObject();

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
        onRefresh();
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