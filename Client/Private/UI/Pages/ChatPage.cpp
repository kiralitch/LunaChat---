#include "UI/Pages/ChatPage.h"

#include <QContextMenuEvent>
#include <QCursor>
#include <QMouseEvent>
#include <QEvent>
#include <QHBoxLayout>
#include <QImage>
#include <QLabel>
#include <QIcon>
#include <QLineEdit>
#include <QToolButton>
#include <QPainter>
#include <QPainterPath>
#include <QProgressBar>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QVBoxLayout>
#include <QWidget>

#include "UI/Widgets/VoiceMessageWidget.h"

namespace {

// 把图片裁成圆形：QSS 的 border-radius 不会裁剪 pixmap，必须自己画
QPixmap circularPixmap(const QByteArray &bytes, int side)
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

ChatPage::ChatPage(QWidget *parent)
    : QWidget(parent)
{
    m_titleLabel = new QLabel(QStringLiteral("未选择会话"), this);
    m_titleLabel->setObjectName(QStringLiteral("ChatTitle"));

    // 群聊专属入口：图标放在标题栏右侧（图标文件见 Client/resources/icons/README.md）
    auto makeIconButton = [this](const QString &iconPath, const QString &tip) {
        auto *button = new QToolButton(this);
        button->setObjectName(QStringLiteral("ChatIconButton"));
        button->setToolTip(tip);
        button->setIcon(QIcon(iconPath));
        button->setIconSize(QSize(26, 26));
        button->setFixedSize(40, 40);
        button->setAutoRaise(true);
        button->setCursor(Qt::PointingHandCursor);
        button->setVisible(false);
        return button;
    };
    m_groupInfoButton = makeIconButton(QStringLiteral(":/icons/group_info.png"), QStringLiteral("群资料"));
    m_groupActivityButton = makeIconButton(QStringLiteral(":/icons/group_activity.png"), QStringLiteral("群活动"));
    m_groupInviteButton = makeIconButton(QStringLiteral(":/icons/group_invite.png"), QStringLiteral("邀请成员"));
    m_groupFilesButton = makeIconButton(QStringLiteral(":/icons/folder.png"), QStringLiteral("共享文件夹"));
    connect(m_groupInfoButton, &QToolButton::clicked, this, &ChatPage::groupInfoRequested);
    connect(m_groupActivityButton, &QToolButton::clicked, this, &ChatPage::groupActivityRequested);
    connect(m_groupInviteButton, &QToolButton::clicked, this, &ChatPage::groupInviteRequested);
    connect(m_groupFilesButton, &QToolButton::clicked, this, &ChatPage::groupFilesRequested);

    // 消息区用「滚动区 + 纵向布局」而不是 QListWidget：
    // 气泡高度随内容变化，用 setItemWidget 很容易被 item 高度截断
    m_scroll = new QScrollArea(this);
    m_scroll->setObjectName(QStringLiteral("MessageList"));
    m_scroll->setWidgetResizable(true);
    m_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    m_msgContainer = new QWidget(m_scroll);
    m_msgContainer->setObjectName(QStringLiteral("MessageContainer"));
    m_msgLayout = new QVBoxLayout(m_msgContainer);
    m_msgLayout->setContentsMargins(8, 8, 8, 8);
    m_msgLayout->setSpacing(2);
    m_msgLayout->addStretch();
    m_scroll->setWidget(m_msgContainer);

    m_input = new QLineEdit(this);
    m_input->setObjectName(QStringLiteral("MessageInput"));
    m_input->setPlaceholderText(QStringLiteral("输入消息，回车发送"));
    m_sendButton = new QPushButton(QStringLiteral("发送"), this);
    m_fileButton = new QPushButton(QStringLiteral("发送文件"), this);
    m_voiceButton = new QPushButton(QStringLiteral("语音"), this);

    m_progress = new QProgressBar(this);
    m_progress->setVisible(false);

    m_statusLabel = new QLabel(this);
    m_statusLabel->setStyleSheet(QStringLiteral("color:#8b8f98;"));

    auto *inputRow = new QHBoxLayout;
    inputRow->addWidget(m_input, 1);
    inputRow->addWidget(m_voiceButton);
    inputRow->addWidget(m_fileButton);
    inputRow->addWidget(m_sendButton);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(6);
    // 标题栏：群头像 + 群名/N 人 + 右侧两个图标
    m_groupAvatarLabel = new QLabel(this);
    m_groupAvatarLabel->setFixedSize(30, 30);
    m_groupAvatarLabel->setObjectName(QStringLiteral("ChatHeaderAvatar"));
    m_groupAvatarLabel->setAlignment(Qt::AlignCenter);

    m_subtitleLabel = new QLabel(this);
    m_subtitleLabel->setObjectName(QStringLiteral("ChatSubtitle"));

    auto *titleBox = new QVBoxLayout;
    titleBox->setContentsMargins(0, 0, 0, 0);
    titleBox->setSpacing(0);
    titleBox->addWidget(m_titleLabel);
    titleBox->addWidget(m_subtitleLabel);

    auto *headerRow = new QHBoxLayout;
    headerRow->setContentsMargins(0, 0, 0, 0);
    headerRow->setSpacing(8);
    headerRow->addWidget(m_groupAvatarLabel, 0, Qt::AlignVCenter);
    headerRow->addLayout(titleBox, 1);
    headerRow->addWidget(m_groupInfoButton);
    headerRow->addWidget(m_groupActivityButton);
    headerRow->addWidget(m_groupInviteButton);
    headerRow->addWidget(m_groupFilesButton);
    layout->addLayout(headerRow);
    layout->addWidget(m_scroll, 1);
    layout->addWidget(m_progress);
    layout->addWidget(m_statusLabel);
    layout->addLayout(inputRow);

    connect(m_sendButton, &QPushButton::clicked, this, &ChatPage::onSendClicked);
    connect(m_fileButton, &QPushButton::clicked, this, &ChatPage::fileSendRequested);
    connect(m_voiceButton, &QPushButton::clicked, this, &ChatPage::voiceRecordRequested);
    connect(m_input, &QLineEdit::returnPressed, this, &ChatPage::onSendClicked);
}

void ChatPage::addRow(QWidget *row)
{
    // 弹簧永远在最后，新消息插在它前面
    m_msgLayout->insertWidget(m_msgLayout->count() - 1, row);
    scrollToBottom();
}

void ChatPage::scrollToBottom()
{
    QScrollBar *bar = m_scroll->verticalScrollBar();
    bar->setValue(bar->maximum());
}

void ChatPage::openGroup(qint64 groupId, const QString &title)
{
    m_groupId = groupId;
    m_peerId = 0;
    m_titleLabel->setText(title);
    m_groupInfoButton->setVisible(true);
    m_groupActivityButton->setVisible(true);
    m_groupInviteButton->setVisible(true);
    m_groupFilesButton->setVisible(true);
    m_groupAvatarLabel->setVisible(true);
    m_subtitleLabel->setVisible(true);
    m_groupAvatarLabel->setText(title.left(1));
    m_subtitleLabel->setText(QStringLiteral("群聊"));
    m_input->setEnabled(true);
    m_sendButton->setEnabled(true);
    m_input->setFocus();
}

void ChatPage::openPrivate(quint64 peerId, const QString &nickname)
{
    m_groupId = 0;
    m_peerId = peerId;
    m_titleLabel->setText(QStringLiteral("与 %1 的私聊").arg(nickname));
    m_groupInfoButton->setVisible(false);
    m_groupActivityButton->setVisible(false);
    m_groupInviteButton->setVisible(false);
    m_groupFilesButton->setVisible(false);
    m_groupAvatarLabel->setVisible(false);
    m_subtitleLabel->setVisible(false);
    m_input->setEnabled(true);
    m_sendButton->setEnabled(true);
    m_input->setFocus();
}

void ChatPage::clearMessages()
{
    while (m_msgLayout->count() > 1) {
        QLayoutItem *item = m_msgLayout->takeAt(0);
        if (QWidget *widget = item->widget())
            widget->deleteLater();
        delete item;
    }
    m_avatarLabels.clear();
    m_avatarOwners.clear();
}

bool ChatPage::eventFilter(QObject *watched, QEvent *event)
{
    // 右键头像：把"点的是谁"抛给 MainWindow，由它决定加好友 / 邀请入群
    if (event->type() == QEvent::MouseButtonRelease) {
        auto *mouseEvent = static_cast<QMouseEvent *>(event);
        if (mouseEvent->button() == Qt::RightButton && m_avatarOwners.contains(watched)) {
            emit avatarContextMenuRequested(m_avatarOwners.value(watched), QCursor::pos());
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}

void ChatPage::clearAvatarCache()
{
    m_avatars.clear();
}

void ChatPage::setAvatar(quint64 userId, const QByteArray &imageBytes)
{
    if (imageBytes.isEmpty())
        return;

    m_avatars.insert(userId, imageBytes);
    const QPixmap pixmap = circularPixmap(imageBytes, 40);
    for (QLabel *label : m_avatarLabels.value(userId)) {
        label->setPixmap(pixmap);
        label->setText(QString());
        label->setStyleSheet(QStringLiteral("background:transparent;"));
    }
}

void ChatPage::appendMessage(quint64 userId, const QString &nickname, const QString &content,
                             bool isSelf, const QString &timeText)
{
    auto *row = new QWidget(m_msgContainer);
    row->setObjectName(QStringLiteral("MessageRow"));

    auto *layout = new QHBoxLayout(row);
    layout->setContentsMargins(8, 6, 8, 6);
    layout->setSpacing(10);

    auto *avatar = new QLabel(nickname.left(1), row);
    avatar->setObjectName(isSelf ? QStringLiteral("AvatarSelf") : QStringLiteral("Avatar"));
    avatar->setFixedSize(40, 40);
    avatar->setAlignment(Qt::AlignCenter);
    m_avatarLabels[userId].append(avatar);
    avatar->installEventFilter(this);
    m_avatarOwners.insert(avatar, userId);

    if (m_avatars.contains(userId)) {
        const QPixmap pixmap = circularPixmap(m_avatars.value(userId), 40);
        if (!pixmap.isNull()) {
            avatar->setPixmap(pixmap);
            avatar->setText(QString());
            avatar->setStyleSheet(QStringLiteral("background:transparent;"));
        }
    }

    auto *textLayout = new QVBoxLayout;
    textLayout->setSpacing(3);

    auto *head = new QLabel(QStringLiteral("%1    %2").arg(nickname, timeText), row);
    head->setObjectName(QStringLiteral("MessageHead"));

    // 轻气泡：很淡的圆角底，按文字宽度收缩（靠尾部弹簧）
    auto *bubble = new QLabel(content, row);
    bubble->setObjectName(QStringLiteral("MessageBubble"));
    bubble->setWordWrap(true);
    bubble->setTextInteractionFlags(Qt::TextSelectableByMouse);
    bubble->setMaximumWidth(560);
    bubble->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Minimum);

    auto *bubbleRow = new QHBoxLayout;
    bubbleRow->setContentsMargins(0, 0, 0, 0);
    if (isSelf) {
        bubbleRow->addStretch();
        bubbleRow->addWidget(bubble, 0, Qt::AlignRight);
        head->setAlignment(Qt::AlignRight);
    } else {
        bubbleRow->addWidget(bubble, 0, Qt::AlignLeft);
        bubbleRow->addStretch();
    }

    textLayout->addWidget(head);
    textLayout->addLayout(bubbleRow);

    // 自己发的消息头像和气泡都靠右，别人的靠左
    if (isSelf) {
        layout->addStretch();
        layout->addLayout(textLayout, 0);
        layout->addWidget(avatar, 0, Qt::AlignTop);
    } else {
        layout->addWidget(avatar, 0, Qt::AlignTop);
        layout->addLayout(textLayout, 1);
    }

    addRow(row);
}

void ChatPage::appendVoiceMessage(const QString &nickname, const QString &timeText,
                                  const QString &filePath, int durationMs, bool self)
{
    auto *row = new QWidget(m_msgContainer);
    row->setObjectName(QStringLiteral("MessageRow"));

    auto *layout = new QVBoxLayout(row);
    layout->setContentsMargins(58, 4, 8, 4);

    auto *header = new QLabel(QStringLiteral("%1  %2").arg(nickname, timeText), row);
    header->setStyleSheet(QStringLiteral("color:#8b8f98;"));

    auto *voice = new VoiceMessageWidget(filePath, durationMs, self, row);
    layout->addWidget(header);
    layout->addWidget(voice);

    addRow(row);
}

void ChatPage::setGroupAvatar(const QByteArray &imageBytes)
{
    const QPixmap pixmap = circularPixmap(imageBytes, 30);
    if (pixmap.isNull())
        return;
    m_groupAvatarLabel->setPixmap(pixmap);
    m_groupAvatarLabel->setText(QString());
}

void ChatPage::setMemberCount(int count)
{
    m_subtitleLabel->setText(QStringLiteral("%1 人").arg(count));
}

void ChatPage::setStatusText(const QString &text)
{
    m_statusLabel->setText(text);
}

void ChatPage::setVoiceButtonText(const QString &text)
{
    m_voiceButton->setText(text);
}

void ChatPage::showProgress(qint64 sent, qint64 total)
{
    m_progress->setVisible(true);
    m_progress->setRange(0, static_cast<int>(total > 0 ? total : 1));
    m_progress->setValue(static_cast<int>(sent));
}

void ChatPage::hideProgress()
{
    m_progress->setVisible(false);
}

void ChatPage::onSendClicked()
{
    const QString text = m_input->text().trimmed();
    if (text.isEmpty())
        return;

    m_input->clear();
    emit sendRequested(text);
}
