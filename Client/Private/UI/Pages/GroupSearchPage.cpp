#include "UI/Pages/GroupSearchPage.h"

#include <QDateTime>
#include <QFrame>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QStackedLayout>
#include <QToolButton>
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

}   // namespace

GroupSearchPage::GroupSearchPage(LunaClient *client, quint64 selfId, QWidget *parent)
    : QWidget(parent), m_client(client), m_selfId(selfId)
{
    setObjectName(QStringLiteral("GroupSearchPage"));

    auto *back = new QToolButton(this);
    back->setObjectName(QStringLiteral("PageBackButton"));
    back->setIcon(QIcon(QStringLiteral(":/icons/back.png")));
    back->setIconSize(QSize(26, 26));
    back->setFixedSize(40, 40);
    back->setAutoRaise(true);
    back->setCursor(Qt::PointingHandCursor);
    back->setToolTip(QStringLiteral("返回聊天"));
    connect(back, &QToolButton::clicked, this, &GroupSearchPage::backRequested);

    auto *title = new QLabel(QStringLiteral("搜索群聊"), this);
    title->setObjectName(QStringLiteral("ChatTitle"));
    auto *hint = new QLabel(QStringLiteral("按群名搜索，或从随机推荐里挑一个加入"), this);
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

    auto *card = new QFrame(this);
    card->setObjectName(QStringLiteral("SettingsCard"));

    m_search = new QLineEdit(card);
    m_search->setPlaceholderText(QStringLiteral("输入群名称关键词，例如：校园"));
    m_search->setClearButtonEnabled(true);
    auto *searchButton = new QPushButton(QStringLiteral("搜索"), card);
    searchButton->setObjectName(QStringLiteral("PrimaryButton"));
    searchButton->setFixedWidth(96);

    auto *searchRow = new QHBoxLayout;
    searchRow->setSpacing(10);
    searchRow->addWidget(m_search, 1);
    searchRow->addWidget(searchButton);

    m_section = new QLabel(QStringLiteral("推荐群聊"), card);
    m_section->setObjectName(QStringLiteral("SectionLabel"));

    m_empty = new QWidget(card);
    auto *emptyLayout = new QVBoxLayout(m_empty);
    emptyLayout->setSpacing(6);
    emptyLayout->addStretch();
    auto *emptyIcon = new QLabel(m_empty);
    emptyIcon->setPixmap(QIcon(QStringLiteral(":/icons/empty_folder.png")).pixmap(80, 80));
    emptyIcon->setAlignment(Qt::AlignCenter);
    m_emptyTitle = new QLabel(QStringLiteral("正在加载推荐群聊…"), m_empty); auto *emptyTitle = m_emptyTitle;
    emptyTitle->setObjectName(QStringLiteral("ChatTitle"));
    emptyTitle->setAlignment(Qt::AlignCenter);
    auto *emptyHint = new QLabel(QStringLiteral("也可以在上方直接搜索群名称"), m_empty);
    emptyHint->setObjectName(QStringLiteral("ChatSubtitle"));
    emptyHint->setAlignment(Qt::AlignCenter);
    emptyLayout->addWidget(emptyIcon, 0, Qt::AlignHCenter);
    emptyLayout->addWidget(emptyTitle);
    emptyLayout->addWidget(emptyHint);
    emptyLayout->addStretch();

    m_list = new QListWidget(card);
    m_list->setObjectName(QStringLiteral("RequestList"));
    m_stack = new QStackedLayout;
    m_stack->addWidget(m_list);
    m_stack->addWidget(m_empty);
    m_stack->setCurrentWidget(m_empty);

    auto *cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(14, 14, 14, 14);
    cardLayout->setSpacing(10);
    cardLayout->addLayout(searchRow);
    cardLayout->addWidget(m_section);
    cardLayout->addLayout(m_stack);

    m_status = new QLabel(this);
    m_status->setObjectName(QStringLiteral("ChatSubtitle"));

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(18, 8, 18, 14);
    layout->setSpacing(10);
    layout->addLayout(header);
    layout->addWidget(card, 1);
    layout->addWidget(m_status);

    connect(searchButton, &QPushButton::clicked, this, &GroupSearchPage::onSearch);
    connect(m_search, &QLineEdit::returnPressed, this, &GroupSearchPage::onSearch);
    connect(m_client, &LunaClient::frameReceived, this, &GroupSearchPage::onFrameReceived);

    refresh();
}

void GroupSearchPage::refresh()
{
    m_search->clear();
    m_section->setText(QStringLiteral("推荐群聊"));
    setStatus(QStringLiteral("正在加载推荐群聊…"));
    QJsonObject data;
    data.insert(QStringLiteral("keyword"), QString());
    m_client->sendFrame(makeFrame(MsgType::kGroupSearch, data));
}

void GroupSearchPage::onSearch()
{
    const QString keyword = m_search->text().trimmed();
    QJsonObject data;
    data.insert(QStringLiteral("keyword"), keyword);
    m_client->sendFrame(makeFrame(MsgType::kGroupSearch, data));
    m_section->setText(keyword.isEmpty() ? QStringLiteral("推荐群聊")
                                         : QStringLiteral("搜索结果"));
    setStatus(keyword.isEmpty() ? QStringLiteral("正在加载推荐群聊…")
                                : QStringLiteral("正在搜索“%1”…").arg(keyword));
}

void GroupSearchPage::onFrameReceived(const QJsonObject &frame)
{
    if (frame.value(QStringLiteral("type")).toString() != MsgType::kGroupSearchResp)
        return;

    const QJsonObject data = frame.value(QStringLiteral("data")).toObject();
    rebuildResults(data.value(QStringLiteral("groups")).toArray(),
                   data.value(QStringLiteral("keyword")).toString());
}

void GroupSearchPage::rebuildResults(const QJsonArray &groups, const QString &keyword)
{
    m_list->clear();
    const bool recommended = keyword.isEmpty();
    m_section->setText(recommended ? QStringLiteral("推荐群聊") : QStringLiteral("搜索结果"));

    if (groups.isEmpty()) {
        m_stack->setCurrentWidget(m_empty);
        m_emptyTitle->setText(recommended ? QStringLiteral("暂时没有可推荐的群聊") : QStringLiteral("没有找到相关群聊"));
        setStatus(recommended ? QStringLiteral("暂时没有可推荐的群聊")
                              : QStringLiteral("没有找到相关群聊"));
        return;
    }

    m_stack->setCurrentWidget(m_list);

    for (const QJsonValue &value : groups) {
        const QJsonObject group = value.toObject();
        const qint64 groupId = static_cast<qint64>(group.value(QStringLiteral("groupId")).toDouble());
        const QString name = group.value(QStringLiteral("name")).toString();
        const int memberCount = group.value(QStringLiteral("memberCount")).toInt();
        const bool joined = group.value(QStringLiteral("joined")).toBool();
        const bool requested = group.value(QStringLiteral("requested")).toBool();

        auto *row = new QWidget(m_list);
        row->setObjectName(QStringLiteral("FriendResultRow"));
        auto *h = new QHBoxLayout(row);
        h->setContentsMargins(10, 8, 10, 8);
        h->setSpacing(12);

        auto *avatar = new QLabel(name.left(1), row);
        avatar->setObjectName(QStringLiteral("Avatar"));
        avatar->setFixedSize(40, 40);
        avatar->setAlignment(Qt::AlignCenter);

        auto *title = new QLabel(name, row);
        title->setObjectName(QStringLiteral("MessageHead"));
        auto *detail = new QLabel(QStringLiteral("%1 人").arg(memberCount), row);
        detail->setObjectName(QStringLiteral("ChatSubtitle"));
        auto *textCol = new QVBoxLayout;
        textCol->setSpacing(2);
        textCol->addWidget(title);
        textCol->addWidget(detail);

        h->addWidget(avatar);
        h->addLayout(textCol, 1);

        if (joined) {
            auto *tag = new QLabel(QStringLiteral("已加入"), row);
            tag->setObjectName(QStringLiteral("ChatSubtitle"));
            h->addWidget(tag);
        } else {
            auto *button = new QPushButton(requested ? QStringLiteral("已申请")
                                                     : QStringLiteral("申请加入"), row);
            button->setFixedWidth(96);
            button->setEnabled(!requested);
            connect(button, &QPushButton::clicked, this, [this, groupId, button]() {
                requestJoin(groupId, button);
            });
            h->addWidget(button);
        }

        auto *item = new QListWidgetItem();
        item->setSizeHint(row->sizeHint());
        m_list->addItem(item);
        m_list->setItemWidget(item, row);
    }

    setStatus(recommended ? QStringLiteral("为你推荐 %1 个群聊").arg(groups.size())
                          : QStringLiteral("找到 %1 个群聊").arg(groups.size()));
}

void GroupSearchPage::requestJoin(qint64 groupId, QPushButton *button)
{
    QJsonObject data;
    data.insert(QStringLiteral("groupId"), static_cast<double>(groupId));
    m_client->sendFrame(makeFrame(MsgType::kGroupJoinRequest, data));
    button->setText(QStringLiteral("已申请"));
    button->setEnabled(false);
    setStatus(QStringLiteral("入群申请已发送，等待群主同意"), 1);
}

void GroupSearchPage::setStatus(const QString &text, int tone)
{
    static const char *const colors[] = { "#9C93C4", "#6FE3B4", "#FF8E8E" };
    m_status->setText(text);
    m_status->setStyleSheet(QStringLiteral("color: %1; background: transparent;")
                                .arg(QString::fromLatin1(colors[qBound(0, tone, 2)])));
}
