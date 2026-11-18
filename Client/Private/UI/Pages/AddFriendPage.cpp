#include "UI/Pages/AddFriendPage.h"

#include <QDateTime>
#include <QFrame>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
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
}

AddFriendPage::AddFriendPage(LunaClient *client, quint64 selfId, QWidget *parent)
    : QWidget(parent), m_client(client), m_selfId(selfId)
{
    setObjectName(QStringLiteral("AddFriendPage"));

    auto *back = new QToolButton(this);
    back->setObjectName(QStringLiteral("PageBackButton"));
    back->setIcon(QIcon(QStringLiteral(":/icons/back.png")));
    back->setIconSize(QSize(26, 26));
    back->setFixedSize(40, 40);
    back->setAutoRaise(true);
    back->setCursor(Qt::PointingHandCursor);
    back->setToolTip(QStringLiteral("返回聊天"));
    connect(back, &QToolButton::clicked, this, &AddFriendPage::backRequested);

    m_title = new QLabel(QStringLiteral("添加好友"), this);
    m_title->setObjectName(QStringLiteral("ChatTitle"));
    m_hint = new QLabel(QStringLiteral("按用户名或昵称搜索，发送好友申请"), this);
    m_hint->setObjectName(QStringLiteral("ChatSubtitle"));

    auto *titleBox = new QVBoxLayout;
    titleBox->setContentsMargins(0, 0, 0, 0);
    titleBox->setSpacing(0);
    titleBox->addWidget(m_title);
    titleBox->addWidget(m_hint);

    auto *header = new QHBoxLayout;
    header->setContentsMargins(0, 0, 0, 0);
    header->setSpacing(8);
    header->addWidget(back);
    header->addLayout(titleBox, 1);

    m_search = new QLineEdit(this);
    m_search->setPlaceholderText(QStringLiteral("输入用户名或昵称关键词，例如：星"));
    m_search->setClearButtonEnabled(true);

    m_searchButton = new QPushButton(QStringLiteral("搜索"), this);
    m_searchButton->setObjectName(QStringLiteral("PrimaryButton"));
    m_searchButton->setFixedWidth(96);
    connect(m_searchButton, &QPushButton::clicked, this, &AddFriendPage::onSearch);
    connect(m_search, &QLineEdit::returnPressed, this, &AddFriendPage::onSearch);

    auto *searchRow = new QHBoxLayout;
    searchRow->setSpacing(10);
    searchRow->addWidget(m_search, 1);
    searchRow->addWidget(m_searchButton);

    m_status = new QLabel(this);
    m_status->setObjectName(QStringLiteral("ChatSubtitle"));
    m_status->setWordWrap(true);

    m_empty = new QLabel(QStringLiteral("输入关键词开始搜索，可以搜到多个相关用户"), this);
    m_empty->setObjectName(QStringLiteral("ChatSubtitle"));
    m_empty->setAlignment(Qt::AlignCenter);
    m_empty->setWordWrap(true);

    m_list = new QListWidget(this);
    m_list->setObjectName(QStringLiteral("FriendResultList"));
    m_list->setSpacing(2);
    m_list->hide();

    auto *card = new QFrame(this);
    card->setObjectName(QStringLiteral("SettingsCard"));
    auto *cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(20, 18, 20, 18);
    cardLayout->setSpacing(10);
    cardLayout->addLayout(searchRow);
    cardLayout->addWidget(m_status);
    cardLayout->addWidget(m_empty, 1);
    cardLayout->addWidget(m_list, 1);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(18, 8, 18, 14);
    layout->setSpacing(12);
    layout->addLayout(header);
    layout->addWidget(card, 1);

    connect(m_client, &LunaClient::frameReceived, this, &AddFriendPage::onFrameReceived);
}

void AddFriendPage::setInviteGroup(qint64 groupId, const QString &groupName)
{
    m_inviteGroupId = groupId;
    m_inviteGroupName = groupName;

    if (m_inviteGroupId > 0) {
        m_title->setText(QStringLiteral("邀请成员"));
        m_hint->setText(QStringLiteral("邀请好友加入「%1」").arg(groupName));
        m_empty->setText(QStringLiteral("输入用户名或昵称，搜索要邀请的人"));
    } else {
        m_title->setText(QStringLiteral("添加好友"));
        m_hint->setText(QStringLiteral("按用户名或昵称搜索，发送好友申请"));
        m_empty->setText(QStringLiteral("输入关键词开始搜索，可以搜到多个相关用户"));
    }
}

void AddFriendPage::reset()
{
    m_search->clear();
    m_list->clear();
    m_list->hide();
    setInviteGroup(m_inviteGroupId, m_inviteGroupName);
    m_empty->show();
    setStatus(QString());

    if (m_inviteGroupId <= 0) {
        m_hint->setText(QStringLiteral("为你推荐的同学，也可以直接搜索"));
        QJsonObject data;
        data.insert(QStringLiteral("keyword"), QString());
        m_client->sendFrame(makeFrame(MsgType::kUserSearch, data));
        setStatus(QStringLiteral("正在加载推荐好友…"));
    }
}

void AddFriendPage::onSearch()
{
    const QString keyword = m_search->text().trimmed();
    if (keyword.isEmpty()) {
        m_list->hide();
        m_empty->setText(QStringLiteral("请输入用户名或昵称关键词"));
        m_empty->show();
        return;
    }

    QJsonObject data;
    data.insert(QStringLiteral("keyword"), keyword);
    m_client->sendFrame(makeFrame(MsgType::kUserSearch, data));
    if (m_inviteGroupId <= 0)
        m_hint->setText(QStringLiteral("按用户名或昵称搜索，发送好友申请"));
    setStatus(QStringLiteral("正在搜索 “%1” …").arg(keyword));
}

void AddFriendPage::onFrameReceived(const QJsonObject &frame)
{
    const QString type = frame.value(QStringLiteral("type")).toString();
    if (type == MsgType::kUserSearchResp) {
        const QJsonObject data = frame.value(QStringLiteral("data")).toObject();
        rebuildResults(data.value(QStringLiteral("users")).toArray(),
                       data.value(QStringLiteral("keyword")).toString());
        return;
    }

    // 好友申请的结果：服务端复用 friend_list 回包（ok/msg）
    if (type == MsgType::kFriendList) {
        const QJsonObject data = frame.value(QStringLiteral("data")).toObject();
        setStatus(data.value(QStringLiteral("msg")).toString(),
                  data.value(QStringLiteral("ok")).toBool() ? 1 : 2);
        return;
    }

    if (type == MsgType::kGroupInviteResp) {
        const QJsonObject data = frame.value(QStringLiteral("data")).toObject();
        setStatus(data.value(QStringLiteral("msg")).toString(),
                  data.value(QStringLiteral("ok")).toBool() ? 1 : 2);
        return;
    }
}

void AddFriendPage::rebuildResults(const QJsonArray &users, const QString &keyword)
{
    m_list->clear();

    if (users.isEmpty()) {
        m_list->hide();
        if (keyword.isEmpty()) {
            m_empty->setText(QStringLiteral("暂时没有推荐，试试搜索用户名或昵称"));
            setStatus(QStringLiteral("暂时没有推荐好友"));
        } else {
            m_empty->setText(QStringLiteral("没有找到与 “%1” 相关的用户").arg(keyword));
            setStatus(QStringLiteral("共 0 个结果"));
        }
        m_empty->show();
        return;
    }

    m_empty->hide();
    m_list->show();

    for (const QJsonValue &value : users) {
        const QJsonObject user = value.toObject();
        const QString nickname = user.value(QStringLiteral("nickname")).toString();
        const QString username = user.value(QStringLiteral("username")).toString();
        const quint64 userId = static_cast<quint64>(user.value(QStringLiteral("userId")).toDouble());
        const bool alreadyFriend = user.value(QStringLiteral("friend")).toBool();
        const bool inviteMode = (m_inviteGroupId > 0);

        auto *row = new QWidget(m_list);
        row->setObjectName(QStringLiteral("FriendResultRow"));
        auto *h = new QHBoxLayout(row);
        h->setContentsMargins(10, 8, 10, 8);
        h->setSpacing(12);

        auto *avatar = new QLabel(nickname.left(1), row);
        avatar->setObjectName(QStringLiteral("Avatar"));
        avatar->setFixedSize(40, 40);
        avatar->setAlignment(Qt::AlignCenter);

        auto *name = new QLabel(nickname, row);
        name->setObjectName(QStringLiteral("MessageHead"));
        auto *account = new QLabel(QStringLiteral("@%1").arg(username), row);
        account->setObjectName(QStringLiteral("ChatSubtitle"));

        auto *textCol = new QVBoxLayout;
        textCol->setContentsMargins(0, 0, 0, 0);
        textCol->setSpacing(2);
        textCol->addWidget(name);
        textCol->addWidget(account);

        h->addWidget(avatar);
        h->addLayout(textCol, 1);

        if (alreadyFriend && !inviteMode) {
            auto *tag = new QLabel(QStringLiteral("已是好友"), row);
            tag->setObjectName(QStringLiteral("ChatSubtitle"));
            h->addWidget(tag);
        } else {
            auto *button = new QPushButton(inviteMode ? QStringLiteral("邀请")
                                                      : QStringLiteral("加好友"), row);
            button->setFixedWidth(96);
            button->setCursor(Qt::PointingHandCursor);
            connect(button, &QPushButton::clicked, this, [this, userId, username, button]() {
                sendRequest(userId, username, button);
            });
            h->addWidget(button);
        }

        auto *item = new QListWidgetItem();
        item->setSizeHint(row->sizeHint());
        m_list->addItem(item);
        m_list->setItemWidget(item, row);
    }

    setStatus(keyword.isEmpty() ? QStringLiteral("为你推荐 %1 位同学").arg(users.size())
                                : QStringLiteral("找到 %1 个用户").arg(users.size()));
}

void AddFriendPage::sendRequest(quint64 userId, const QString &username, QPushButton *button)
{
    if (m_inviteGroupId > 0) {
        QJsonObject invite;
        invite.insert(QStringLiteral("groupId"), static_cast<double>(m_inviteGroupId));
        invite.insert(QStringLiteral("userId"), static_cast<double>(userId));
        m_client->sendFrame(makeFrame(MsgType::kGroupInvite, invite));

        button->setText(QStringLiteral("已邀请"));
        button->setEnabled(false);
        setStatus(QStringLiteral("已邀请 @%1 加入群聊").arg(username), 1);
        return;
    }

    QJsonObject data;
    data.insert(QStringLiteral("username"), username);
    m_client->sendFrame(makeFrame(MsgType::kFriendRequest, data));

    button->setText(QStringLiteral("已申请"));
    button->setEnabled(false);
    setStatus(QStringLiteral("好友申请已发送给 @%1").arg(username), 1);
}

void AddFriendPage::setStatus(const QString &text, int tone)
{
    static const char *const colors[] = { "#9C93C4", "#6FE3B4", "#FF8E8E" };
    m_status->setText(text);
    m_status->setStyleSheet(QStringLiteral("color: %1; background: transparent;")
                                .arg(QString::fromLatin1(colors[qBound(0, tone, 2)])));
}
