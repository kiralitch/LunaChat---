#include "UI/Pages/RequestsPage.h"

#include <QButtonGroup>
#include <QDateTime>
#include <QFrame>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QStackedLayout>
#include <QStackedWidget>
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

QWidget *makeEmptyState(const QString &title, const QString &hint, QWidget *parent)
{
    auto *widget = new QWidget(parent);
    auto *layout = new QVBoxLayout(widget);
    layout->setSpacing(6);
    layout->addStretch();
    auto *icon = new QLabel(widget);
    icon->setPixmap(QIcon(QStringLiteral(":/icons/empty_folder.png")).pixmap(72, 72));
    icon->setAlignment(Qt::AlignCenter);
    auto *titleLabel = new QLabel(title, widget);
    titleLabel->setObjectName(QStringLiteral("ChatTitle"));
    titleLabel->setAlignment(Qt::AlignCenter);
    auto *hintLabel = new QLabel(hint, widget);
    hintLabel->setObjectName(QStringLiteral("ChatSubtitle"));
    hintLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(icon, 0, Qt::AlignHCenter);
    layout->addWidget(titleLabel);
    layout->addWidget(hintLabel);
    layout->addStretch();
    return widget;
}

}   // namespace

RequestsPage::RequestsPage(LunaClient *client, quint64 selfId, QWidget *parent)
    : QWidget(parent), m_client(client), m_selfId(selfId)
{
    setObjectName(QStringLiteral("RequestsPage"));

    auto *back = new QToolButton(this);
    back->setObjectName(QStringLiteral("PageBackButton"));
    back->setIcon(QIcon(QStringLiteral(":/icons/back.png")));
    back->setIconSize(QSize(26, 26));
    back->setFixedSize(40, 40);
    back->setAutoRaise(true);
    back->setCursor(Qt::PointingHandCursor);
    back->setToolTip(QStringLiteral("返回聊天"));
    connect(back, &QToolButton::clicked, this, &RequestsPage::backRequested);

    auto *title = new QLabel(QStringLiteral("消息与申请"), this);
    title->setObjectName(QStringLiteral("ChatTitle"));
    auto *hint = new QLabel(QStringLiteral("好友申请与群聊申请都在这里处理"), this);
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

    m_tabFriend = new QPushButton(QStringLiteral("好友申请"), this);
    m_tabGroup = new QPushButton(QStringLiteral("群聊申请"), this);
    auto *tabGroup = new QButtonGroup(this);
    tabGroup->setExclusive(true);
    int index = 0;
    for (QPushButton *tab : { m_tabFriend, m_tabGroup }) {
        tab->setObjectName(QStringLiteral("SegmentButton"));
        tab->setCheckable(true);
        tab->setCursor(Qt::PointingHandCursor);
        tabGroup->addButton(tab, index++);
    }
    m_tabFriend->setChecked(true);

    auto *tabs = new QHBoxLayout;
    tabs->setSpacing(8);
    tabs->addWidget(m_tabFriend);
    tabs->addWidget(m_tabGroup);
    tabs->addStretch();

    m_pages = new QStackedWidget(this);

    // ===== 好友申请 =====
    auto *friendPage = new QFrame(m_pages);
    friendPage->setObjectName(QStringLiteral("SettingsCard"));
    auto *friendStack = new QStackedLayout;
    m_friendList = new QListWidget(friendPage);
    m_friendList->setObjectName(QStringLiteral("RequestList"));
    m_friendEmpty = makeEmptyState(QStringLiteral("暂无好友申请"),
                                   QStringLiteral("别人通过搜索加你时，会出现在这里"),
                                   friendPage);
    friendStack->addWidget(m_friendList);
    friendStack->addWidget(m_friendEmpty);
    auto *friendLayout = new QVBoxLayout(friendPage);
    friendLayout->setContentsMargins(10, 10, 10, 10);
    friendLayout->addLayout(friendStack);

    // ===== 群聊申请 =====
    auto *groupPage = new QFrame(m_pages);
    groupPage->setObjectName(QStringLiteral("SettingsCard"));
    auto *groupStack = new QStackedLayout;
    m_groupList = new QListWidget(groupPage);
    m_groupList->setObjectName(QStringLiteral("RequestList"));
    m_groupEmpty = makeEmptyState(QStringLiteral("暂无群聊申请"),
                                  QStringLiteral("别人申请加入你的群时，会出现在这里"),
                                  groupPage);
    groupStack->addWidget(m_groupList);
    groupStack->addWidget(m_groupEmpty);
    auto *groupLayout = new QVBoxLayout(groupPage);
    groupLayout->setContentsMargins(10, 10, 10, 10);
    groupLayout->addLayout(groupStack);


    m_pages->addWidget(friendPage);
    m_pages->addWidget(groupPage);

    m_status = new QLabel(this);
    m_status->setObjectName(QStringLiteral("ChatSubtitle"));

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(18, 8, 18, 14);
    layout->setSpacing(10);
    layout->addLayout(header);
    layout->addLayout(tabs);
    layout->addWidget(m_pages, 1);
    layout->addWidget(m_status);

    connect(tabGroup, &QButtonGroup::idClicked, this, &RequestsPage::switchTab);

    connect(m_client, &LunaClient::frameReceived, this, &RequestsPage::onFrameReceived);

    refreshAll();
}

void RequestsPage::refreshAll()
{
    requestFriendList();
    requestGroupList();
}

void RequestsPage::requestFriendList()
{
    m_client->sendFrame(makeFrame(MsgType::kFriendRequests, QJsonObject()));
}

void RequestsPage::requestGroupList()
{
    m_client->sendFrame(makeFrame(MsgType::kGroupJoinRequests, QJsonObject()));
}

void RequestsPage::switchTab(int index)
{
    m_pages->setCurrentIndex(index);
}

void RequestsPage::onFrameReceived(const QJsonObject &frame)
{
    const QString type = frame.value(QStringLiteral("type")).toString();
    const QJsonObject data = frame.value(QStringLiteral("data")).toObject();

    if (type == MsgType::kFriendRequestsResp) {
        rebuildFriends(data.value(QStringLiteral("requests")).toArray());
        return;
    }
    if (type == MsgType::kGroupJoinRequestsResp) {
        rebuildGroupRequests(data.value(QStringLiteral("requests")).toArray());
        return;
    }

    if (type == MsgType::kFriendAccept) {
        setStatus(data.value(QStringLiteral("msg")).toString(),
                  data.value(QStringLiteral("ok")).toBool() ? 1 : 2);
        requestFriendList();
        return;
    }
    if (type == MsgType::kFriendRequests) {
        setStatus(data.value(QStringLiteral("msg")).toString(),
                  data.value(QStringLiteral("ok")).toBool() ? 1 : 2);
        requestFriendList();
        return;
    }
    if (type == MsgType::kGroupJoinHandleResp) {
        setStatus(data.value(QStringLiteral("msg")).toString(),
                  data.value(QStringLiteral("ok")).toBool() ? 1 : 2);
        requestGroupList();
        return;
    }
    if (type == MsgType::kGroupJoinRequestResp) {
        setStatus(data.value(QStringLiteral("msg")).toString(),
                  data.value(QStringLiteral("ok")).toBool() ? 1 : 2);
        return;
    }
    if (type == MsgType::kGroupJoinResult) {
        setStatus(data.value(QStringLiteral("message")).toString(),
                  data.value(QStringLiteral("accepted")).toBool() ? 1 : 2);
        return;
    }
    if (type == MsgType::kFriendNotify) {
        if (data.value(QStringLiteral("event")).toString() == QStringLiteral("request")) {
            setStatus(data.value(QStringLiteral("message")).toString());
            requestFriendList();
        }
        return;
    }
    if (type == MsgType::kGroupJoinNotify) {
        setStatus(data.value(QStringLiteral("message")).toString());
        requestGroupList();
        return;
    }
}

void RequestsPage::rebuildFriends(const QJsonArray &requests)
{
    m_friendList->clear();
    m_friendCount = requests.size();
    m_tabFriend->setText(m_friendCount > 0 ? QStringLiteral("好友申请 %1").arg(m_friendCount)
                                           : QStringLiteral("好友申请"));

    auto *stack = qobject_cast<QStackedLayout *>(m_friendEmpty->parentWidget()->layout()->itemAt(0)->layout());
    if (stack)
        stack->setCurrentWidget(requests.isEmpty() ? m_friendEmpty : static_cast<QWidget *>(m_friendList));

    for (const QJsonValue &value : requests) {
        const QJsonObject request = value.toObject();
        const quint64 fromUser = static_cast<quint64>(request.value(QStringLiteral("fromUser")).toDouble());
        const QString nickname = request.value(QStringLiteral("nickname")).toString();
        const QString username = request.value(QStringLiteral("username")).toString();

        auto *row = new QWidget(m_friendList);
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
        auto *account = new QLabel(QStringLiteral("@%1 请求加你为好友").arg(username), row);
        account->setObjectName(QStringLiteral("ChatSubtitle"));
        auto *textCol = new QVBoxLayout;
        textCol->setSpacing(2);
        textCol->addWidget(name);
        textCol->addWidget(account);

        auto *accept = new QPushButton(QStringLiteral("同意"), row);
        accept->setObjectName(QStringLiteral("PrimaryButton"));
        accept->setFixedWidth(76);
        auto *reject = new QPushButton(QStringLiteral("拒绝"), row);
        reject->setFixedWidth(64);
        connect(accept, &QPushButton::clicked, this, [this, fromUser, accept, reject]() {
            acceptFriend(fromUser, accept);
            reject->setEnabled(false);
        });
        connect(reject, &QPushButton::clicked, this, [this, fromUser, accept, reject]() {
            rejectFriend(fromUser, reject);
            accept->setEnabled(false);
        });

        h->addWidget(avatar);
        h->addLayout(textCol, 1);
        h->addWidget(accept);
        h->addWidget(reject);

        auto *item = new QListWidgetItem();
        item->setSizeHint(row->sizeHint());
        m_friendList->addItem(item);
        m_friendList->setItemWidget(item, row);
    }

    emit countsChanged(m_friendCount, m_groupCount);
}

void RequestsPage::rebuildGroupRequests(const QJsonArray &requests)
{
    m_groupList->clear();
    m_groupCount = requests.size();
    m_tabGroup->setText(m_groupCount > 0 ? QStringLiteral("群聊申请 %1").arg(m_groupCount)
                                         : QStringLiteral("群聊申请"));

    auto *stack = qobject_cast<QStackedLayout *>(m_groupEmpty->parentWidget()->layout()->itemAt(0)->layout());
    if (stack)
        stack->setCurrentWidget(requests.isEmpty() ? m_groupEmpty : static_cast<QWidget *>(m_groupList));

    for (const QJsonValue &value : requests) {
        const QJsonObject request = value.toObject();
        const qint64 requestId = static_cast<qint64>(request.value(QStringLiteral("requestId")).toDouble());
        const QString nickname = request.value(QStringLiteral("nickname")).toString();
        const QString groupName = request.value(QStringLiteral("groupName")).toString();

        auto *row = new QWidget(m_groupList);
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
        auto *detail = new QLabel(QStringLiteral("申请加入「%1」").arg(groupName), row);
        detail->setObjectName(QStringLiteral("ChatSubtitle"));
        auto *textCol = new QVBoxLayout;
        textCol->setSpacing(2);
        textCol->addWidget(name);
        textCol->addWidget(detail);

        auto *accept = new QPushButton(QStringLiteral("同意"), row);
        accept->setObjectName(QStringLiteral("PrimaryButton"));
        accept->setFixedWidth(76);
        auto *reject = new QPushButton(QStringLiteral("拒绝"), row);
        reject->setFixedWidth(64);
        connect(accept, &QPushButton::clicked, this, [this, requestId, accept, reject]() {
            handleGroupJoin(requestId, true);
            accept->setEnabled(false);
            reject->setEnabled(false);
        });
        connect(reject, &QPushButton::clicked, this, [this, requestId, accept, reject]() {
            handleGroupJoin(requestId, false);
            accept->setEnabled(false);
            reject->setEnabled(false);
        });

        h->addWidget(avatar);
        h->addLayout(textCol, 1);
        h->addWidget(accept);
        h->addWidget(reject);

        auto *item = new QListWidgetItem();
        item->setSizeHint(row->sizeHint());
        m_groupList->addItem(item);
        m_groupList->setItemWidget(item, row);
    }

    emit countsChanged(m_friendCount, m_groupCount);
}

void RequestsPage::acceptFriend(quint64 fromUser, QPushButton *button)
{
    QJsonObject data;
    data.insert(QStringLiteral("fromUser"), static_cast<double>(fromUser));
    m_client->sendFrame(makeFrame(MsgType::kFriendAccept, data));
    button->setText(QStringLiteral("已同意"));
    button->setEnabled(false);
    setStatus(QStringLiteral("已同意好友申请，正在刷新…"), 1);
}

void RequestsPage::rejectFriend(quint64 fromUser, QPushButton *button)
{
    QJsonObject data;
    data.insert(QStringLiteral("fromUser"), static_cast<double>(fromUser));
    m_client->sendFrame(makeFrame(MsgType::kFriendReject, data));
    button->setText(QStringLiteral("已拒绝"));
    button->setEnabled(false);
    setStatus(QStringLiteral("已拒绝好友申请"), 2);
}

void RequestsPage::handleGroupJoin(qint64 requestId, bool accept)
{
    QJsonObject data;
    data.insert(QStringLiteral("requestId"), static_cast<double>(requestId));
    m_client->sendFrame(makeFrame(accept ? MsgType::kGroupJoinAccept : MsgType::kGroupJoinReject,
                                  data));
    setStatus(accept ? QStringLiteral("正在同意入群…") : QStringLiteral("正在拒绝入群…"));
}

void RequestsPage::setStatus(const QString &text, int tone)
{
    static const char *const colors[] = { "#9C93C4", "#6FE3B4", "#FF8E8E" };
    m_status->setText(text);
    m_status->setStyleSheet(QStringLiteral("color: %1; background: transparent;")
                                .arg(QString::fromLatin1(colors[qBound(0, tone, 2)])));
}
