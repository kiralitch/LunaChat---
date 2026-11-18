#include "UI/Windows/MainWindow.h"

#include <QCoreApplication>
#include <algorithm>
#include <QColor>
#include <QDateTime>
#include <QDate>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QMessageBox>
#include <QLabel>
#include <QListWidget>
#include <QMenu>
#include <QPushButton>
#include <QToolButton>
#include <QSplitter>
#include <QStatusBar>
#include <QTimer>
#include <QUuid>
#include <QVBoxLayout>
#include <QWidget>
#include <QtDebug>
#include <QGraphicsOpacityEffect>
#include <QPropertyAnimation>

#ifdef Q_OS_WIN
#include <windows.h>
#include <windowsx.h>
#include <dwmapi.h>
#pragma comment(lib, "dwmapi.lib")
#endif

#include "Core/Recorder.h"
#include "Core/SoundPlayer.h"
#include "LunaProtocol/LunaConstants.h"
#include "LunaProtocol/MessageTypes.h"
#include "Network/LunaClient.h"
#include "UI/Pages/ChatPage.h"
#include "UI/Pages/AddFriendPage.h"
#include "UI/Pages/CreateGroupPage.h"
#include "UI/Pages/RequestsPage.h"
#include "UI/Pages/GroupSearchPage.h"
#include "UI/Widgets/BadgeButton.h"
#include "UI/Widgets/ConfirmDialog.h"
#include "UI/Pages/ProfilePage.h"
#include "UI/Widgets/TitleBar.h"
#include <QImage>
#include <QPainter>
#include <QPainterPath>
#include <QSettings>
#include <QStackedWidget>
#include "UI/Pages/ActivityPage.h"
#include "UI/Pages/GroupFilesPage.h"
#include "UI/Pages/GroupInfoPage.h"
#include "UI/Widgets/IconRail.h"
#include "UI/Pages/MatchPage.h"
#include "UI/Pages/MomentPage.h"
#include "UI/Pages/SettingsPage.h"
#include <QApplication>
#include "Core/AppSettings.h"

namespace {

const qint64 kMaxFileBytes = 10LL * 1024 * 1024;   // 与协议一致：单文件 ≤10MB
const int kChunkBytes = 64 * 1024;                 // 单块原始数据 64KB，base64 后仍在 1MB 帧内

QString nowTimeText()
{
    return QDateTime::currentDateTime().toString(QStringLiteral("HH:mm"));
}

}

static QIcon roundedIcon(const QByteArray &bytes, int side);   // 前置声明，供 onFrameReceived 使用
namespace { QJsonObject makeFrame(const QString &type, const QJsonObject &data); }   // 定义在文件末尾

MainWindow::MainWindow(LunaClient *client, quint64 userId, const QString &nickname, QWidget *parent)
    : QMainWindow(parent), m_client(client), m_userId(userId), m_nickname(nickname)
{
    setWindowTitle(QStringLiteral("LunaChat 月之音 - %1").arg(nickname));
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
    setMinimumSize(920, 620);
    resize(1080, 720);

#ifdef Q_OS_WIN
    // 无边框但保留系统缩放/贴边能力：加回 THICKFRAME，再用 WM_NCCALCSIZE 去掉非客户区
    setAttribute(Qt::WA_NativeWindow);
    {
        const HWND hwnd = reinterpret_cast<HWND>(winId());
        LONG_PTR style = GetWindowLongPtr(hwnd, GWL_STYLE);
        style |= WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX;
        SetWindowLongPtr(hwnd, GWL_STYLE, style);
        SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
                     SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER);
    }
#endif

    m_newGroupButton = new QPushButton(this);
    m_newGroupButton->setIcon(QIcon(QStringLiteral(":/icons/group_new.png")));
    m_newGroupButton->setIconSize(QSize(24, 24));
    m_newGroupButton->setFixedSize(40, 40);
    m_newGroupButton->setToolTip(QStringLiteral("新建群组"));
    m_newGroupButton->setCursor(Qt::PointingHandCursor);
    m_addFriendButton = new QPushButton(this);
    m_addFriendButton->setIcon(QIcon(QStringLiteral(":/icons/friend_add.png")));
    m_addFriendButton->setIconSize(QSize(24, 24));
    m_addFriendButton->setFixedSize(40, 40);
    m_addFriendButton->setToolTip(QStringLiteral("加好友"));
    m_addFriendButton->setCursor(Qt::PointingHandCursor);
    m_groupSearchButton = new QToolButton(this);
    m_groupSearchButton->setIcon(QIcon(QStringLiteral(":/icons/search.png")));
    m_groupSearchButton->setIconSize(QSize(24, 24));
    m_groupSearchButton->setFixedSize(40, 40);
    m_groupSearchButton->setObjectName(QStringLiteral("SidebarIconButton"));
    m_groupSearchButton->setToolTip(QStringLiteral("搜索群聊"));
    m_groupSearchButton->setCursor(Qt::PointingHandCursor);
    m_requestsButton = new BadgeButton(this);
    m_requestsButton->setIcon(QIcon(QStringLiteral(":/icons/requests.png")));
    m_requestsButton->setIconSize(QSize(24, 24));
    m_requestsButton->setFixedSize(40, 40);
    m_requestsButton->setObjectName(QStringLiteral("SidebarIconButton"));
    m_requestsButton->setToolTip(QStringLiteral("消息与申请"));
    m_sessionList = new QListWidget(this);
    m_sessionList->setFixedWidth(220);
    m_sessionList->setObjectName(QStringLiteral("ChannelList"));
    m_sessionList->setContextMenuPolicy(Qt::CustomContextMenu);

    auto *leftPanel = new QWidget(this);
    leftPanel->setObjectName(QStringLiteral("Sidebar"));
    auto *leftLayout = new QVBoxLayout(leftPanel);
    leftLayout->setSpacing(6);
    leftLayout->setContentsMargins(0, 0, 0, 0);

    // ===== 群聊工具区：新建群组 + 加好友 放一起 =====
    auto *groupLabel = new QLabel(QStringLiteral("群聊工具"), this);
    groupLabel->setObjectName(QStringLiteral("SectionLabel"));

    auto *groupRow = new QHBoxLayout;
    groupRow->setSpacing(6);
    groupRow->addWidget(m_newGroupButton);
    groupRow->addWidget(m_addFriendButton);
    groupRow->addWidget(m_groupSearchButton);
    groupRow->addWidget(m_requestsButton);
    groupRow->addStretch();

    for (QPushButton *button : { m_newGroupButton, m_addFriendButton })
        button->setObjectName(QStringLiteral("SidebarButton"));

    leftLayout->addWidget(groupLabel);
    leftLayout->addLayout(groupRow);

    // 最左侧图标栏：只负责"切主界面"，不放具体操作
    m_rail = new IconRail(this);
    m_rail->addIconItem(QStringLiteral("chat"), QStringLiteral(":/icons/chat.png"), QStringLiteral("聊天"));
    m_rail->addIconItem(QStringLiteral("moments"), QStringLiteral(":/icons/moments.png"), QStringLiteral("朋友圈"));
    m_rail->addIconItem(QStringLiteral("match"), QStringLiteral(":/icons/match.png"), QStringLiteral("随机匹配"));
    m_rail->addIconItem(QStringLiteral("profile"), QStringLiteral(":/icons/profile.png"), QStringLiteral("个人资料"));
    m_rail->addIconItem(QStringLiteral("settings"), QStringLiteral(":/icons/settings.png"), QStringLiteral("设置"));
    m_rail->setCurrent(QStringLiteral("chat"));

    connect(m_rail, &IconRail::currentChanged, this, [this](const QString &id) {
        if (id == QStringLiteral("moments")) {
            setSection(QStringLiteral("朋友圈"));
            m_mainStack->setCurrentWidget(m_momentPage);
            m_momentPage->refresh();
            return;
        }

        if (id == QStringLiteral("settings")) {
            setSection(QStringLiteral("设置"));
            m_mainStack->setCurrentWidget(m_settingsPage);
            return;
        }

        // 其它页面都建立在"正常界面"上（设置页会盖住左栏）
        if (id == QStringLiteral("chat"))
            setSection(QStringLiteral("聊天"));
        else if (id == QStringLiteral("match"))
            setSection(QStringLiteral("随机匹配"));
        else if (id == QStringLiteral("profile"))
            setSection(QStringLiteral("个人资料"));

        m_mainStack->setCurrentWidget(m_mainStack->widget(0));
        if (id == QStringLiteral("chat")) {
            m_stack->setCurrentWidget(m_chatPage);
        } else if (id == QStringLiteral("moments")) {
            onOpenMoments();
        } else if (id == QStringLiteral("match")) {
            onOpenMatch();
        } else if (id == QStringLiteral("profile")) {
            onOpenProfile();
        }
    });


    leftLayout->addWidget(m_sessionList, 1);


    m_chatPage = new ChatPage(this);
    m_profilePage = new ProfilePage(m_client, m_userId, this);
    m_momentPage = new MomentPage(m_client, m_userId, m_nickname, this);
    m_activityPage = new ActivityPage(m_client, m_userId, this);
    m_matchPage = new MatchPage(m_client, m_userId, m_nickname, this);

    // 右侧用 QStackedWidget 承载"页面"：聊天 / 个人资料（以后朋友圈等也放这里）
    m_stack = new QStackedWidget(this);
    m_stack->addWidget(m_chatPage);
    m_stack->addWidget(m_activityPage);

    // 群资料页面（内嵌）：退群/解散后自动回到聊天并刷新群列表
    m_groupInfoPage = new GroupInfoPage(m_client, m_userId, this);
    m_stack->addWidget(m_groupInfoPage);
    connect(m_groupInfoPage, &GroupInfoPage::closed, this, [this]() {
        m_stack->setCurrentWidget(m_chatPage);
        refreshGroupList();
    });

    auto *splitter = new QSplitter(Qt::Horizontal, this);
    splitter->addWidget(leftPanel);
    splitter->addWidget(m_stack);
    splitter->setStretchFactor(1, 1);

    // 最左侧图标栏 + 可拖动的左右分栏
    // 主区域栈：0 = 正常界面（左栏 + 聊天），1 = 设置页（盖住左栏与聊天）
    m_mainStack = new QStackedWidget(this);
    m_mainStack->addWidget(splitter);
    m_settingsPage = new SettingsPage(this);
    m_mainStack->addWidget(m_settingsPage);
    m_mainStack->addWidget(m_profilePage);   // 个人资料也整页覆盖
    m_mainStack->addWidget(m_matchPage);     // 随机匹配也整页覆盖
    m_mainStack->addWidget(m_momentPage);   // 朋友圈也占满右侧整块（盖住左栏）

    m_groupFilesPage = new GroupFilesPage(m_client, m_userId, this);
    m_mainStack->addWidget(m_groupFilesPage);
    connect(m_groupFilesPage, &GroupFilesPage::uploadRequested, this, &MainWindow::onUploadToFolder);
    connect(m_groupFilesPage, &GroupFilesPage::backRequested, this, [this]() {
        m_mainStack->setCurrentWidget(m_mainStack->widget(0));
        m_stack->setCurrentWidget(m_chatPage);
        setSection(QStringLiteral("聊天"));
        m_rail->setCurrent(QStringLiteral("chat"));
    });

    m_addFriendPage = new AddFriendPage(m_client, m_userId, this);
    m_mainStack->addWidget(m_addFriendPage);
    connect(m_addFriendPage, &AddFriendPage::backRequested, this, [this]() {
        m_mainStack->setCurrentWidget(m_mainStack->widget(0));
        m_stack->setCurrentWidget(m_chatPage);
        setSection(QStringLiteral("聊天"));
        m_rail->setCurrent(QStringLiteral("chat"));
    });

    m_createGroupPage = new CreateGroupPage(m_client, this);
    m_mainStack->addWidget(m_createGroupPage);
    connect(m_createGroupPage, &CreateGroupPage::backRequested, this, [this]() {
        m_mainStack->setCurrentWidget(m_mainStack->widget(0));
        m_stack->setCurrentWidget(m_chatPage);
        setSection(QStringLiteral("聊天"));
        m_rail->setCurrent(QStringLiteral("chat"));
    });
    connect(m_createGroupPage, &CreateGroupPage::groupCreated, this, [this](qint64 groupId, const QString &name) {
        if (!name.isEmpty())
            m_groups.insert(groupId, name);
        refreshGroupList();
        m_mainStack->setCurrentWidget(m_mainStack->widget(0));
        m_stack->setCurrentWidget(m_chatPage);
        setSection(QStringLiteral("聊天"));
        m_rail->setCurrent(QStringLiteral("chat"));
        selectConversation(keyOfGroup(groupId));
    });

    m_requestsPage = new RequestsPage(m_client, m_userId, this);
    m_mainStack->addWidget(m_requestsPage);
    connect(m_requestsPage, &RequestsPage::backRequested, this, [this]() {
        m_mainStack->setCurrentWidget(m_mainStack->widget(0));
        m_stack->setCurrentWidget(m_chatPage);
        setSection(QStringLiteral("聊天"));
        m_rail->setCurrent(QStringLiteral("chat"));
    });
    connect(m_requestsPage, &RequestsPage::countsChanged, this,
            [this](int friendCount, int groupCount) {
                if (m_requestsButton)
                    m_requestsButton->setCount(friendCount + groupCount);
            });
    connect(m_requestsButton, &BadgeButton::clicked, this, &MainWindow::onOpenRequests);

    m_groupSearchPage = new GroupSearchPage(m_client, m_userId, this);
    m_mainStack->addWidget(m_groupSearchPage);
    connect(m_groupSearchPage, &GroupSearchPage::backRequested, this, [this]() {
        m_mainStack->setCurrentWidget(m_mainStack->widget(0));
        m_stack->setCurrentWidget(m_chatPage);
        setSection(QStringLiteral("聊天"));
        m_rail->setCurrent(QStringLiteral("chat"));
    });
    connect(m_groupSearchButton, &QToolButton::clicked, this, &MainWindow::onOpenGroupSearch);

    // 页面切换统一淡入，避免硬切
    connect(m_mainStack, &QStackedWidget::currentChanged, this, [this](int) {
        QWidget *page = m_mainStack->currentWidget();
        if (!page)
            return;
        auto *effect = new QGraphicsOpacityEffect(page);
        page->setGraphicsEffect(effect);
        effect->setOpacity(0.0);
        auto *anim = new QPropertyAnimation(effect, "opacity", page);
        anim->setDuration(180);
        anim->setStartValue(0.0);
        anim->setEndValue(1.0);
        connect(anim, &QPropertyAnimation::finished, page, [page]() {
            page->setGraphicsEffect(nullptr);
        });
        anim->start(QAbstractAnimation::DeleteWhenStopped);
    });
    connect(m_settingsPage, &SettingsPage::backRequested, this, [this]() {
        m_mainStack->setCurrentWidget(m_mainStack->widget(0));
        m_stack->setCurrentWidget(m_chatPage);      // 保存/返回后回到聊天
        m_rail->setCurrent(QStringLiteral("chat"));
    });

    auto *central = new QWidget(this);
    auto *centralLayout = new QVBoxLayout(central);
    centralLayout->setContentsMargins(0, 0, 0, 0);
    centralLayout->setSpacing(0);

    m_titleBar = new TitleBar(this, QStringLiteral("LunaChat 月之音"));
    centralLayout->addWidget(m_titleBar);

    auto *content = new QWidget(central);
    auto *contentLayout = new QHBoxLayout(content);
    contentLayout->setContentsMargins(0, 0, 0, 0);
    contentLayout->setSpacing(0);
    contentLayout->addWidget(m_rail);
    contentLayout->addWidget(m_mainStack, 1);
    centralLayout->addWidget(content, 1);

    setCentralWidget(central);

    // 页面顶部返回箭头：回到聊天（并把左侧图标栏复位）
    connect(m_groupInfoPage, &GroupInfoPage::backRequested, this, [this]() {
        m_stack->setCurrentWidget(m_chatPage);
        setSection(QStringLiteral("聊天"));
        m_rail->setCurrent(QStringLiteral("chat"));
    });
    connect(m_activityPage, &ActivityPage::backRequested, this, [this]() {
        m_stack->setCurrentWidget(m_chatPage);
        setSection(QStringLiteral("聊天"));
        m_rail->setCurrent(QStringLiteral("chat"));
    });

    m_recorder = new Recorder(this);
    connect(m_recorder, &Recorder::levelChanged, this, [this](qreal level) {
        if (m_recorder && m_recorder->isRecording())
            m_chatPage->setStatusText(QStringLiteral("录音中... 电平 %1%").arg(int(level * 100)));
    });
    connect(m_recorder, &Recorder::finished, this, &MainWindow::onRecorderFinished);

    m_chunkTimer = new QTimer(this);
    m_chunkTimer->setInterval(0);
    connect(m_chunkTimer, &QTimer::timeout, this, &MainWindow::sendNextChunk);

    connect(m_sessionList, &QListWidget::currentItemChanged, this, &MainWindow::onSessionItemChanged);
    connect(m_sessionList, &QWidget::customContextMenuRequested, this, &MainWindow::onContextMenu);
    connect(m_chatPage, &ChatPage::sendRequested, this, &MainWindow::onSendRequested);
    connect(m_chatPage, &ChatPage::fileSendRequested, this, &MainWindow::onFileSendRequested);
    connect(m_chatPage, &ChatPage::voiceRecordRequested, this, &MainWindow::onVoiceRecordRequested);
    connect(m_chatPage, &ChatPage::groupInfoRequested, this, &MainWindow::onOpenGroupInfo);
    connect(m_chatPage, &ChatPage::groupActivityRequested, this, &MainWindow::onOpenActivities);
    connect(m_chatPage, &ChatPage::groupInviteRequested, this, &MainWindow::onInviteMember);
    connect(m_chatPage, &ChatPage::groupFilesRequested, this, &MainWindow::onOpenGroupFiles);
    connect(m_chatPage, &ChatPage::privateSettingsRequested, this, [this](quint64 peerId) {
        requestDeleteFriend(peerId, m_friendNames.value(peerId));
    });
    connect(m_chatPage, &ChatPage::recallRequested, this, [this](qint64 msgId) {
        QJsonObject data;
        data.insert(QStringLiteral("msgId"), static_cast<double>(msgId));
        QJsonObject frame;
        frame.insert(QStringLiteral("type"), MsgType::kChatRecall);
        frame.insert(QStringLiteral("ts"), static_cast<double>(QDateTime::currentMSecsSinceEpoch()));
        frame.insert(QStringLiteral("data"), data);
        m_client->sendFrame(frame);
    });
    connect(m_chatPage, &ChatPage::avatarContextMenuRequested, this,
            [this](quint64 userId, const QPoint &globalPos) {
                if (userId == 0 || userId == m_userId)
                    return;

                QMenu menu(this);
                QAction *addFriend = nullptr;
                if (!m_friendIds.contains(userId))
                    addFriend = menu.addAction(QStringLiteral("加为好友"));

                QMenu *inviteMenu = menu.addMenu(QStringLiteral("邀请到我管理的群组"));
                QStringList managed;
                for (auto it = m_groupRole.constBegin(); it != m_groupRole.constEnd(); ++it) {
                    if (it.value() == QStringLiteral("owner") || it.value() == QStringLiteral("admin"))
                        managed << QString::number(it.key());
                }
                QList<QAction *> inviteActions;
                if (managed.isEmpty()) {
                    QAction *none = inviteMenu->addAction(QStringLiteral("（你还没有管理权限的群）"));
                    none->setEnabled(false);
                } else {
                    for (const QString &groupIdText : managed) {
                        const qint64 groupId = groupIdText.toLongLong();
                        inviteActions.append(inviteMenu->addAction(m_groups.value(groupId)));
                    }
                }

                QAction *chosen = menu.exec(globalPos);
                if (!chosen)
                    return;
                if (chosen == addFriend) {
                    QJsonObject data;
                    data.insert(QStringLiteral("username"), QString::number(userId));
                    QJsonObject frame;
                    frame.insert(QStringLiteral("type"), MsgType::kFriendRequest);
                    frame.insert(QStringLiteral("ts"), static_cast<double>(QDateTime::currentMSecsSinceEpoch()));
                    frame.insert(QStringLiteral("data"), data);
                    m_client->sendFrame(frame);
                    appendSystemLine(QStringLiteral("好友申请已发送"));
                } else {
                    const int index = inviteActions.indexOf(chosen);
                    if (index >= 0)
                        inviteUser(managed.at(index).toLongLong(), userId);
                }
            });
    connect(m_newGroupButton, &QPushButton::clicked, this, &MainWindow::onCreateGroup);
    connect(m_addFriendButton, &QPushButton::clicked, this, &MainWindow::onAddFriend);
    connect(m_profilePage, &ProfilePage::nicknameChanged, this, [this](const QString &nickname) {
        m_nickname = nickname;
        setWindowTitle(QStringLiteral("LunaChat 月之音 - %1").arg(nickname));
        appendSystemLine(QStringLiteral("昵称已更新"));
    });
    connect(m_client, &LunaClient::frameReceived, this, &MainWindow::onFrameReceived);

    statusBar()->showMessage(QStringLiteral("已登录：%1 (userId=%2)").arg(nickname).arg(userId));

#ifdef Q_OS_WIN
    // Windows 10/11：标题栏跟随深色主题，避免白色边框和夜紫界面割裂
    const BOOL darkTitle = TRUE;
    const HWND nativeHandle = reinterpret_cast<HWND>(winId());
    if (FAILED(DwmSetWindowAttribute(nativeHandle, 20, &darkTitle, sizeof(darkTitle))))
        DwmSetWindowAttribute(nativeHandle, 19, &darkTitle, sizeof(darkTitle));

    // Windows 11：无边框窗口也请求圆角（Win10 会忽略）
    const DWORD cornerPreference = 2;   // DWMWCP_ROUND
    DwmSetWindowAttribute(nativeHandle, 33, &cornerPreference, sizeof(cornerPreference));
#endif

    refreshGroupList();

    QJsonObject friendReq;
    friendReq.insert(QStringLiteral("type"), MsgType::kFriendList);
    friendReq.insert(QStringLiteral("ts"), static_cast<double>(QDateTime::currentMSecsSinceEpoch()));
    friendReq.insert(QStringLiteral("data"), QJsonObject());
    m_client->sendFrame(friendReq);
}

MainWindow::~MainWindow()
{
    if (m_outFile) {
        m_outFile->close();
    }
}

void MainWindow::appendSystemLine(const QString &text)
{
    statusBar()->showMessage(text, 4000);
}

QString MainWindow::keyOfUser(quint64 userId) const
{
    return QStringLiteral("u%1").arg(userId);
}

QString MainWindow::keyOfGroup(qint64 groupId) const
{
    return QStringLiteral("g%1").arg(groupId);
}

void MainWindow::refreshGroupList()
{
    QJsonObject frame;
    frame.insert(QStringLiteral("type"), MsgType::kGroupList);
    frame.insert(QStringLiteral("ts"), static_cast<double>(QDateTime::currentMSecsSinceEpoch()));
    frame.insert(QStringLiteral("data"), QJsonObject());
    m_client->sendFrame(frame);
}

void MainWindow::onSessionItemChanged()
{
    QListWidgetItem *item = m_sessionList->currentItem();
    if (!item)
        return;
    selectConversation(item->data(Qt::UserRole).toString());
}

void MainWindow::selectConversation(const QString &key)
{
    m_rail->setCurrent(QStringLiteral("chat"));   // 选会话即回到聊天界面
    setSection(QStringLiteral("聊天"));
    if (key.isEmpty() || key == m_currentKey)
        return;

    m_currentKey = key;
    if (m_stack)
        m_stack->setCurrentWidget(m_chatPage);   // 点会话时自动回到聊天页
    m_chatPage->clearMessages();

    if (key.startsWith(QLatin1Char('g'))) {
        const qint64 groupId = key.mid(1).toLongLong();
        QString title = m_groups.value(groupId);
        if (title.isEmpty())
            title = QStringLiteral("群 %1").arg(groupId);
        m_chatPage->openGroup(groupId, title);

        // 标题栏需要人数和群头像：打开会话时各拉一次
        QJsonObject infoReq;
        infoReq.insert(QStringLiteral("groupId"), static_cast<double>(groupId));
        m_client->sendFrame(makeFrame(MsgType::kGroupInfoGet, infoReq));
        m_client->sendFrame(makeFrame(MsgType::kGroupAvatarGet, infoReq));
    } else {
        const quint64 peerId = key.mid(1).toULongLong();
        QString nickname = m_friendNames.value(peerId);
        if (nickname.isEmpty())
            nickname = m_users.value(peerId);
        if (nickname.isEmpty())
            nickname = QStringLiteral("用户 %1").arg(peerId);
        m_chatPage->openPrivate(peerId, nickname);
    }

    // 每次切会话都重新从服务端拉历史：消息不会丢，只是用数据库里的最新数据重画，
    // 头像和昵称的变化因此能立刻反映出来
    m_messages.remove(key);
    m_chatPage->clearAvatarCache();
    m_avatarRequested.clear();
    requestHistory(key);
}

void MainWindow::requestHistory(const QString &key)
{
    QJsonObject data;
    data.insert(QStringLiteral("limit"), 50);
    if (key.startsWith(QLatin1Char('g'))) {
        data.insert(QStringLiteral("scope"), QStringLiteral("group"));
        data.insert(QStringLiteral("groupId"), static_cast<double>(key.mid(1).toLongLong()));
    } else {
        data.insert(QStringLiteral("scope"), QStringLiteral("private"));
        data.insert(QStringLiteral("toUser"), static_cast<double>(key.mid(1).toULongLong()));
    }

    QJsonObject frame;
    frame.insert(QStringLiteral("type"), MsgType::kChatHistory);
    frame.insert(QStringLiteral("ts"), static_cast<double>(QDateTime::currentMSecsSinceEpoch()));
    frame.insert(QStringLiteral("data"), data);
    m_client->sendFrame(frame);
}

void MainWindow::onSendRequested(const QString &text)
{
    if (m_currentKey.isEmpty())
        return;

    QJsonObject data;
    QJsonObject frame;
    frame.insert(QStringLiteral("ts"), static_cast<double>(QDateTime::currentMSecsSinceEpoch()));

    if (m_currentKey.startsWith(QLatin1Char('g'))) {
        data.insert(QStringLiteral("groupId"), static_cast<double>(m_chatPage->groupId()));
        data.insert(QStringLiteral("content"), text);
        frame.insert(QStringLiteral("type"), MsgType::kChatGroup);
    } else {
        data.insert(QStringLiteral("toUser"), static_cast<double>(m_chatPage->peerId()));
        data.insert(QStringLiteral("content"), text);
        frame.insert(QStringLiteral("type"), MsgType::kChatPrivate);
    }
    frame.insert(QStringLiteral("data"), data);
    m_client->sendFrame(frame);
    // 不做本地回显：等服务端回送，避免一条消息显示两次
}

void MainWindow::onCreateGroup()
{
    setSection(QStringLiteral("新建群聊"));
    m_createGroupPage->reset();
    m_mainStack->setCurrentWidget(m_createGroupPage);
}

void MainWindow::onOpenActivities()
{
    // m_currentKey 可能为空（启动后直接点图标），退回聊天页当前群
    const qint64 groupId = currentGroupId();
    if (groupId <= 0) {
        appendSystemLine(QStringLiteral("请先在左侧选择一个群组"));
        return;
    }
    const QString role = m_groupRole.value(groupId);
    setSection(QStringLiteral("群活动"));
    m_activityPage->setGroup(groupId, role == QStringLiteral("owner") || role == QStringLiteral("admin"));
    m_stack->setCurrentWidget(m_activityPage);
}

qint64 MainWindow::currentGroupId() const
{
    if (m_currentKey.startsWith(QLatin1Char('g')))
        return m_currentKey.mid(1).toLongLong();
    if (m_chatPage->groupId() > 0)
        return static_cast<qint64>(m_chatPage->groupId());
    // 兜底：还没选会话时用列表里的第一个群，避免点了图标没反应
    if (!m_groups.isEmpty())
        return m_groups.constBegin().key();
    return 0;
}

void MainWindow::playSound(const QString &fileName)
{
    // 具体播放逻辑收敛到 Core/SoundPlayer.h；这里保留同名方法，历史调用点不用改
    SoundPlayer::play(fileName);
}

void MainWindow::onUploadToFolder()
{
    if (!m_groupFilesPage || m_groupFilesPage->groupId() <= 0) {
        appendSystemLine(QStringLiteral("请先从群聊进入共享文件夹"));
        return;
    }

    const QString path = QFileDialog::getOpenFileName(this, QStringLiteral("选择要上传的文件"));
    if (path.isEmpty())
        return;

    QFileInfo info(path);
    if (!info.exists() || info.size() <= 0) {
        appendSystemLine(QStringLiteral("文件为空或不存在"));
        return;
    }

    m_outGroup = m_groupFilesPage->groupId();
    m_outPath = m_groupFilesPage->currentPath();
    m_outName = info.fileName();
    m_outTotal = info.size();
    m_outSent = 0;
    m_outSeq = 0;
    m_outTarget = 0;
    m_outFileId = QString::number(QDateTime::currentMSecsSinceEpoch());

    m_outFile = new QFile(path, this);
    if (!m_outFile->open(QIODevice::ReadOnly)) {
        appendSystemLine(QStringLiteral("无法打开文件"));
        m_outFile->deleteLater();
        m_outFile = nullptr;
        return;
    }

    QJsonObject data;
    data.insert(QStringLiteral("fileId"), m_outFileId);
    data.insert(QStringLiteral("fileName"), m_outName);
    data.insert(QStringLiteral("fileSize"), static_cast<double>(m_outTotal));
    data.insert(QStringLiteral("groupId"), static_cast<double>(m_outGroup));
    if (!m_outPath.isEmpty())
        data.insert(QStringLiteral("path"), m_outPath);

    QJsonObject frame;
    frame.insert(QStringLiteral("type"), MsgType::kFileMeta);
    frame.insert(QStringLiteral("ts"), static_cast<double>(QDateTime::currentMSecsSinceEpoch()));
    frame.insert(QStringLiteral("data"), data);
    m_client->sendFrame(frame);

    m_chatPage->showProgress(0, m_outTotal);
    m_chatPage->setStatusText(QStringLiteral("正在上传 %1...").arg(m_outName));
    m_chunkTimer->start();
}

void MainWindow::onOpenGroupFiles()
{
    const qint64 groupId = currentGroupId();
    if (groupId <= 0) {
        appendSystemLine(QStringLiteral("请先在左侧选择一个群组"));
        return;
    }
    const QString role = m_groupRole.value(groupId);
    setSection(QStringLiteral("共享文件夹"));
    m_groupFilesPage->setGroup(groupId,
                               role == QStringLiteral("owner") || role == QStringLiteral("admin"));
    m_mainStack->setCurrentWidget(m_groupFilesPage);
}

void MainWindow::onOpenGroupInfo()
{
    const qint64 groupId = currentGroupId();
    if (groupId <= 0) {
        appendSystemLine(QStringLiteral("请先在左侧选择一个群组"));
        return;
    }
    setSection(QStringLiteral("群资料"));
    m_groupInfoPage->setGroup(groupId, m_groupRole.value(groupId));
    m_stack->setCurrentWidget(m_groupInfoPage);
}

void MainWindow::onInviteMember()
{
    if (!m_currentKey.startsWith(QLatin1Char('g'))) {
        appendSystemLine(QStringLiteral("请先在左侧选择一个群组"));
        return;
    }

    const qint64 groupId = m_currentKey.mid(1).toLongLong();
    setSection(QStringLiteral("邀请成员"));
    m_addFriendPage->setInviteGroup(groupId, m_groups.value(groupId));
    m_addFriendPage->reset();
    m_mainStack->setCurrentWidget(m_addFriendPage);
}

void MainWindow::onLeaveGroup()
{
    if (!m_currentKey.startsWith(QLatin1Char('g'))) {
        appendSystemLine(QStringLiteral("请先在左侧选择一个群组"));
        return;
    }
    const qint64 groupId = m_currentKey.mid(1).toLongLong();
    if (QMessageBox::question(this, QStringLiteral("退出群聊"),
                              QStringLiteral("确定退出「%1」吗？群主需要先转让群主。")
                                  .arg(m_groups.value(groupId)))
        != QMessageBox::Yes)
        return;

    QJsonObject data;
    data.insert(QStringLiteral("groupId"), static_cast<double>(groupId));
    QJsonObject frame;
    frame.insert(QStringLiteral("type"), MsgType::kGroupLeave);
    frame.insert(QStringLiteral("ts"), static_cast<double>(QDateTime::currentMSecsSinceEpoch()));
    frame.insert(QStringLiteral("data"), data);
    m_client->sendFrame(frame);
}

void MainWindow::onOpenRequests()
{
    setSection(QStringLiteral("消息与申请"));
    m_requestsPage->refreshAll();
    m_mainStack->setCurrentWidget(m_requestsPage);
}

void MainWindow::onOpenGroupSearch()
{
    setSection(QStringLiteral("搜索群聊"));
    m_groupSearchPage->refresh();
    m_mainStack->setCurrentWidget(m_groupSearchPage);
}

void MainWindow::onAddFriend()
{
    setSection(QStringLiteral("添加好友"));
    m_addFriendPage->setInviteGroup(0, QString());
    m_addFriendPage->reset();
    m_mainStack->setCurrentWidget(m_addFriendPage);
}

void MainWindow::onOpenProfile()
{
    m_mainStack->setCurrentWidget(m_profilePage);
    m_profilePage->refresh();
}

void MainWindow::setSection(const QString &name)
{
    if (m_titleBar)
        m_titleBar->setSection(name);
}

void MainWindow::requestDeleteFriend(quint64 userId, const QString &nickname)
{
    if (userId == 0)
        return;

    const QString name = nickname.isEmpty() ? QString::number(userId) : nickname;
    if (!ConfirmDialog::ask(this, QStringLiteral("删除好友"),
                            QStringLiteral("确定删除好友「%1」吗？\n"
                                           "删除后将从双方好友列表移除，聊天记录不会删除。")
                                .arg(name),
                            QStringLiteral("删除")))
        return;

    QJsonObject data;
    data.insert(QStringLiteral("userId"), static_cast<double>(userId));
    QJsonObject frame;
    frame.insert(QStringLiteral("type"), MsgType::kFriendDelete);
    frame.insert(QStringLiteral("ts"), static_cast<double>(QDateTime::currentMSecsSinceEpoch()));
    frame.insert(QStringLiteral("data"), data);
    m_client->sendFrame(frame);
    appendSystemLine(QStringLiteral("正在删除好友 %1…").arg(name));
}

void MainWindow::onOpenSettings()
{
    m_mainStack->setCurrentWidget(m_settingsPage);
    m_rail->setCurrent(QStringLiteral("settings"));
}

void MainWindow::onOpenMatch()
{
    setSection(QStringLiteral("随机匹配"));
    m_mainStack->setCurrentWidget(m_matchPage);
}

void MainWindow::onOpenMoments()
{
    m_stack->setCurrentWidget(m_momentPage);
    m_momentPage->refresh();
}

void MainWindow::inviteUser(qint64 groupId, quint64 targetId)
{
    QJsonObject data;
    data.insert(QStringLiteral("groupId"), static_cast<double>(groupId));
    data.insert(QStringLiteral("userId"), static_cast<double>(targetId));

    QJsonObject frame;
    frame.insert(QStringLiteral("type"), MsgType::kGroupInvite);
    frame.insert(QStringLiteral("ts"), static_cast<double>(QDateTime::currentMSecsSinceEpoch()));
    frame.insert(QStringLiteral("data"), data);
    m_client->sendFrame(frame);
}

void MainWindow::onContextMenu(const QPoint &pos)
{
    QListWidgetItem *item = m_sessionList->itemAt(pos);
    if (!item)
        return;

    const QString key = item->data(Qt::UserRole).toString();
    if (!key.startsWith(QLatin1Char('u')))
        return;      // 只有在线用户可邀请

    const quint64 targetId = key.mid(1).toULongLong();

    // 列出"我有管理权限"的群组，让用户自己选要拉进哪个群（不再依赖当前会话）
    QMenu menu(this);
    QMenu *inviteMenu = menu.addMenu(QStringLiteral("邀请到我管理的群组"));

    QStringList managed;
    for (auto it = m_groupRole.constBegin(); it != m_groupRole.constEnd(); ++it) {
        if (it.value() == QStringLiteral("owner") || it.value() == QStringLiteral("admin"))
            managed << QString::number(it.key());
    }

    if (managed.isEmpty()) {
        QAction *none = inviteMenu->addAction(QStringLiteral("（你还没有管理权限的群）"));
        none->setEnabled(false);
    } else {
        for (const QString &groupIdText : managed) {
            const qint64 groupId = groupIdText.toLongLong();
            QAction *action = inviteMenu->addAction(m_groups.value(groupId));
            connect(action, &QAction::triggered, this, [this, groupId, targetId]() {
                inviteUser(groupId, targetId);
            });
        }
    }

    if (m_friendIds.contains(targetId)) {
        menu.addSeparator();
        QAction *removeFriend = menu.addAction(QStringLiteral("删除好友"));
        connect(removeFriend, &QAction::triggered, this, [this, targetId, item]() {
            requestDeleteFriend(targetId, item->text());
        });
    }

    menu.exec(m_sessionList->mapToGlobal(pos));
}

void MainWindow::onFileSendRequested()
{
    if (m_currentKey.isEmpty()) {
        appendSystemLine(QStringLiteral("请先选择一个会话"));
        return;
    }
    if (m_outFile) {
        appendSystemLine(QStringLiteral("上一个文件还在发送中"));
        return;
    }

    const QString path = QFileDialog::getOpenFileName(this, QStringLiteral("选择要发送的文件"));
    if (path.isEmpty())
        return;

    const QFileInfo info(path);
    if (info.size() <= 0 || info.size() > kMaxFileBytes) {
        appendSystemLine(QStringLiteral("文件需在 10MB 以内"));
        return;
    }

    m_outFile = new QFile(path, this);
    if (!m_outFile->open(QIODevice::ReadOnly)) {
        appendSystemLine(QStringLiteral("打开文件失败"));
        delete m_outFile;
        m_outFile = nullptr;
        return;
    }

    m_outFileId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    m_outName = info.fileName();
    m_outTarget = m_chatPage->peerId();
    m_outGroup = m_currentKey.startsWith(QLatin1Char('g')) ? m_currentKey.mid(1).toLongLong() : 0;
    m_outTotal = info.size();
    m_outSent = 0;
    m_outSeq = 0;

    QJsonObject data;
    if (m_outGroup > 0)
        data.insert(QStringLiteral("groupId"), static_cast<double>(m_outGroup));
    else
        data.insert(QStringLiteral("toUser"), static_cast<double>(m_outTarget));
    data.insert(QStringLiteral("fileName"), m_outName);
    data.insert(QStringLiteral("fileSize"), static_cast<double>(m_outTotal));
    data.insert(QStringLiteral("fileId"), m_outFileId);

    QJsonObject frame;
    frame.insert(QStringLiteral("type"), MsgType::kFileMeta);
    frame.insert(QStringLiteral("ts"), static_cast<double>(QDateTime::currentMSecsSinceEpoch()));
    frame.insert(QStringLiteral("data"), data);
    m_client->sendFrame(frame);

    m_chatPage->showProgress(0, m_outTotal);
    m_chatPage->setStatusText(QStringLiteral("正在发送 %1...").arg(m_outName));
    m_chunkTimer->start();
}

void MainWindow::sendNextChunk()
{
    if (!m_outFile) {
        m_chunkTimer->stop();
        return;
    }

    const QByteArray chunk = m_outFile->read(kChunkBytes);
    if (chunk.isEmpty()) {
        // 发完：file_done 让接收方落盘并回 file_resp
        QJsonObject data;
        data.insert(QStringLiteral("fileId"), m_outFileId);
        data.insert(QStringLiteral("seq"), m_outSeq);

        QJsonObject frame;
        frame.insert(QStringLiteral("type"), MsgType::kFileDone);
        frame.insert(QStringLiteral("ts"), static_cast<double>(QDateTime::currentMSecsSinceEpoch()));
        frame.insert(QStringLiteral("data"), data);
        m_client->sendFrame(frame);

        m_chunkTimer->stop();
        m_outFile->close();
        m_outFile->deleteLater();
        m_outFile = nullptr;
        if (m_outGroup > 0) {
            // 群文件：服务端直接落盘，不会有 file_resp，这里立刻收尾
            m_chatPage->hideProgress();
            m_chatPage->setStatusText(QStringLiteral("已存入群共享文件夹：%1").arg(m_outName));
            if (m_groupFilesPage)
                m_groupFilesPage->refresh();
            m_outGroup = 0;
            return;
        }
        m_chatPage->showProgress(m_outTotal, m_outTotal);
        m_chatPage->setStatusText(QStringLiteral("文件已发完，等待对方确认"));
        return;
    }

    m_outSent += chunk.size();
    const int totalChunks = static_cast<int>((m_outTotal + kChunkBytes - 1) / kChunkBytes);

    QJsonObject data;
    data.insert(QStringLiteral("fileId"), m_outFileId);
    data.insert(QStringLiteral("seq"), m_outSeq++);
    data.insert(QStringLiteral("total"), totalChunks);
    data.insert(QStringLiteral("chunkBase64"), QString::fromLatin1(chunk.toBase64()));

    QJsonObject frame;
    frame.insert(QStringLiteral("type"), MsgType::kFileChunk);
    frame.insert(QStringLiteral("ts"), static_cast<double>(QDateTime::currentMSecsSinceEpoch()));
    frame.insert(QStringLiteral("data"), data);
    m_client->sendFrame(frame);

    m_chatPage->showProgress(m_outSent, m_outTotal);
}

void MainWindow::onVoiceRecordRequested()
{
    if (!m_currentKey.startsWith(QLatin1Char('u'))) {
        appendSystemLine(QStringLiteral("目前只支持私聊发语音"));
        return;
    }

    if (m_recorder->isRecording()) {
        m_chatPage->setVoiceButtonText(QStringLiteral("语音"));
        m_recorder->stop();
        return;
    }

    if (!m_recorder->start()) {
        appendSystemLine(QStringLiteral("录音失败：%1").arg(m_recorder->lastError()));
        return;
    }
    m_chatPage->setVoiceButtonText(QStringLiteral("停止录音"));
    m_chatPage->setStatusText(QStringLiteral("录音中..."));
}

void MainWindow::onRecorderFinished(const QByteArray &wavData, int durationMs)
{
    m_chatPage->setVoiceButtonText(QStringLiteral("语音"));
    if (wavData.size() <= 44 || durationMs < 300) {   // 44 字节 = 只有 WAV 头
        m_chatPage->setStatusText(QStringLiteral("录音太短，已取消"));
        return;
    }
    sendVoice(wavData, durationMs);
}

void MainWindow::sendVoice(const QByteArray &wavData, int durationMs)
{
    const quint64 target = m_chatPage->peerId();
    if (target == 0) {
        appendSystemLine(QStringLiteral("当前会话没有语音接收方"));
        return;
    }

    const QString voiceId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    const QString fileName = QStringLiteral("voice_%1.wav").arg(voiceId);

    QJsonObject meta;
    meta.insert(QStringLiteral("toUser"), static_cast<double>(target));
    meta.insert(QStringLiteral("fileName"), fileName);
    meta.insert(QStringLiteral("duration"), durationMs);
    meta.insert(QStringLiteral("fileSize"), static_cast<double>(wavData.size()));
    meta.insert(QStringLiteral("format"), QStringLiteral("wav"));
    meta.insert(QStringLiteral("bitrate"), 256);          // 16kHz*16bit*单声道 = 256kbps
    meta.insert(QStringLiteral("voiceId"), voiceId);
    meta.insert(QStringLiteral("nickname"), m_nickname);

    QJsonObject metaFrame;
    metaFrame.insert(QStringLiteral("type"), MsgType::kVoiceMeta);
    metaFrame.insert(QStringLiteral("ts"), static_cast<double>(QDateTime::currentMSecsSinceEpoch()));
    metaFrame.insert(QStringLiteral("data"), meta);
    m_client->sendFrame(metaFrame);

    const int total = static_cast<int>((wavData.size() + kChunkBytes - 1) / kChunkBytes);
    for (int seq = 0; seq < total; ++seq) {
        const QByteArray part = wavData.mid(seq * kChunkBytes, kChunkBytes);
        QJsonObject chunk;
        chunk.insert(QStringLiteral("voiceId"), voiceId);
        chunk.insert(QStringLiteral("seq"), seq);
        chunk.insert(QStringLiteral("total"), total);
        chunk.insert(QStringLiteral("chunkBase64"), QString::fromLatin1(part.toBase64()));

        QJsonObject frame;
        frame.insert(QStringLiteral("type"), MsgType::kVoiceChunk);
        frame.insert(QStringLiteral("ts"), static_cast<double>(QDateTime::currentMSecsSinceEpoch()));
        frame.insert(QStringLiteral("data"), chunk);
        m_client->sendFrame(frame);
    }

    QJsonObject doneData;
    doneData.insert(QStringLiteral("voiceId"), voiceId);
    QJsonObject doneFrame;
    doneFrame.insert(QStringLiteral("type"), MsgType::kVoiceDone);
    doneFrame.insert(QStringLiteral("ts"), static_cast<double>(QDateTime::currentMSecsSinceEpoch()));
    doneFrame.insert(QStringLiteral("data"), doneData);
    m_client->sendFrame(doneFrame);

    // 发送者自己也要看到语音条：本地直接落盘插一条，不依赖服务端回送（省一次流量）
    const QString localDir = QCoreApplication::applicationDirPath() + QStringLiteral("/received");
    QDir().mkpath(localDir);
    const QString localPath = QDir(localDir).filePath(fileName);
    QFile local(localPath);
    if (local.open(QIODevice::WriteOnly)) {
        local.write(wavData);
        local.close();
    }
    m_chatPage->appendVoiceMessage(m_nickname,
                                   QDateTime::currentDateTime().toString(QStringLiteral("HH:mm")),
                                   localPath, durationMs, true);

    m_chatPage->setStatusText(QStringLiteral("语音已发送（%1 秒）").arg(durationMs / 1000.0, 0, 'f', 1));
}

void MainWindow::handleVoiceFrame(const QJsonObject &frame)
{
    const QString type = frame.value(QStringLiteral("type")).toString();
    const QJsonObject data = frame.value(QStringLiteral("data")).toObject();

    if (type == MsgType::kVoiceMeta) {
        const QString voiceId = data.value(QStringLiteral("voiceId")).toString();
        m_inVoice.insert(voiceId, QByteArray());
        m_inVoiceName.insert(voiceId, data.value(QStringLiteral("fileName")).toString());
        m_inVoiceDuration.insert(voiceId, data.value(QStringLiteral("duration")).toInt());
        m_inVoiceNick.insert(voiceId, data.value(QStringLiteral("nickname")).toString(QStringLiteral("对方")));
        m_inVoiceSize.insert(voiceId, static_cast<qint64>(data.value(QStringLiteral("fileSize")).toDouble()));
        m_chatPage->setStatusText(QStringLiteral("正在接收语音..."));
        return;
    }

    if (type == MsgType::kVoiceChunk) {
        const QString voiceId = data.value(QStringLiteral("voiceId")).toString();
        if (!m_inVoice.contains(voiceId))
            return;
        m_inVoice[voiceId].append(QByteArray::fromBase64(
            data.value(QStringLiteral("chunkBase64")).toString().toLatin1()));
        return;
    }

    if (type == MsgType::kVoiceDone) {
        const QString voiceId = data.value(QStringLiteral("voiceId")).toString();
        if (!m_inVoice.contains(voiceId))
            return;

        const QString dirPath = QCoreApplication::applicationDirPath() + QStringLiteral("/received");
        QDir().mkpath(dirPath);
        const QString filePath = QDir(dirPath).filePath(m_inVoiceName.value(voiceId));

        QFile file(filePath);
        bool ok = file.open(QIODevice::WriteOnly) && file.write(m_inVoice.value(voiceId)) >= 0;
        if (file.isOpen())
            file.close();
        ok = ok && (file.size() == m_inVoiceSize.value(voiceId));

        const int durationMs = m_inVoiceDuration.value(voiceId);
        const QString fromNick = m_inVoiceNick.value(voiceId, QStringLiteral("对方"));
        m_inVoiceNick.remove(voiceId);
        m_inVoice.remove(voiceId);
        m_inVoiceName.remove(voiceId);
        m_inVoiceDuration.remove(voiceId);
        m_inVoiceSize.remove(voiceId);

        QJsonObject respData;
        respData.insert(QStringLiteral("voiceId"), voiceId);
        respData.insert(QStringLiteral("ok"), ok);
        QJsonObject resp;
        resp.insert(QStringLiteral("type"), MsgType::kVoiceResp);
        resp.insert(QStringLiteral("ts"), static_cast<double>(QDateTime::currentMSecsSinceEpoch()));
        resp.insert(QStringLiteral("data"), respData);
        m_client->sendFrame(resp);

        // 语音不落 message 表（语音本身在 voice_record），这里只在当前会话插一条语音条
        if (ok && m_currentKey.startsWith(QLatin1Char('u'))) {
            m_chatPage->appendVoiceMessage(fromNick,
                                           QDateTime::currentDateTime().toString(QStringLiteral("HH:mm")),
                                           filePath, durationMs, false);
        }
        appendSystemLine(ok ? QStringLiteral("语音已保存到 received/%1").arg(QFileInfo(filePath).fileName())
                            : QStringLiteral("语音保存失败"));
        return;
    }

    if (type == MsgType::kVoiceResp) {
        appendSystemLine(data.value(QStringLiteral("ok")).toBool() ? QStringLiteral("对方已收到语音")
                                                                  : QStringLiteral("对方接收语音失败"));
        return;
    }
}

void MainWindow::handleFileFrame(const QJsonObject &frame)
{
    const QString type = frame.value(QStringLiteral("type")).toString();
    const QJsonObject data = frame.value(QStringLiteral("data")).toObject();

    if (type == MsgType::kFileMeta) {
        const QString fileId = data.value(QStringLiteral("fileId")).toString();
        m_inName.insert(fileId, data.value(QStringLiteral("fileName")).toString());
        m_inSize.insert(fileId, static_cast<qint64>(data.value(QStringLiteral("fileSize")).toDouble()));
        m_inData.insert(fileId, QByteArray());
        m_chatPage->setStatusText(QStringLiteral("正在接收文件：%1").arg(m_inName.value(fileId)));
        return;
    }

    if (type == MsgType::kFileChunk) {
        const QString fileId = data.value(QStringLiteral("fileId")).toString();
        if (!m_inData.contains(fileId))
            return;
        const QByteArray chunk = QByteArray::fromBase64(
            data.value(QStringLiteral("chunkBase64")).toString().toLatin1());
        m_inData[fileId].append(chunk);
        return;
    }

    if (type == MsgType::kFileDone) {
        const QString fileId = data.value(QStringLiteral("fileId")).toString();
        if (!m_inData.contains(fileId))
            return;

        const QString dirPath = QCoreApplication::applicationDirPath() + QStringLiteral("/received");
        QDir().mkpath(dirPath);
        const QString filePath = QDir(dirPath).filePath(m_inName.value(fileId));

        QFile file(filePath);
        bool ok = file.open(QIODevice::WriteOnly) && file.write(m_inData.value(fileId)) >= 0;
        if (file.isOpen())
            file.close();
        ok = ok && (file.size() == m_inSize.value(fileId));

        m_inData.remove(fileId);
        m_inName.remove(fileId);
        m_inSize.remove(fileId);

        // 回执：服务端转给发送方，发送方据此结束进度
        QJsonObject respData;
        respData.insert(QStringLiteral("fileId"), fileId);
        respData.insert(QStringLiteral("ok"), ok);

        QJsonObject resp;
        resp.insert(QStringLiteral("type"), MsgType::kFileResp);
        resp.insert(QStringLiteral("ts"), static_cast<double>(QDateTime::currentMSecsSinceEpoch()));
        resp.insert(QStringLiteral("data"), respData);
        m_client->sendFrame(resp);

        appendSystemLine(ok ? QStringLiteral("文件已保存到 received/%1").arg(QFileInfo(filePath).fileName())
                            : QStringLiteral("文件保存失败"));
        return;
    }

    if (type == MsgType::kFileResp) {
        m_chatPage->hideProgress();
        m_chatPage->setStatusText(data.value(QStringLiteral("ok")).toBool()
                                      ? QStringLiteral("对方已接收文件")
                                      : QStringLiteral("对方接收文件失败"));
        return;
    }
}

void MainWindow::requestAvatar(quint64 userId)
{
    if (userId == 0 || m_avatarRequested.contains(userId))
        return;
    m_avatarRequested.insert(userId);

    QJsonObject data;
    data.insert(QStringLiteral("userId"), static_cast<double>(userId));
    QJsonObject frame;
    frame.insert(QStringLiteral("type"), MsgType::kAvatarGet);
    frame.insert(QStringLiteral("ts"), static_cast<double>(QDateTime::currentMSecsSinceEpoch()));
    frame.insert(QStringLiteral("data"), data);
    m_client->sendFrame(frame);
}

void MainWindow::appendToConversation(const QString &key, const MessageLine &line)
{
    m_messages[key].append(line);
    if (key == m_currentKey)
        m_chatPage->appendMessage(line.userId, line.nickname, line.content, line.self,
                                  line.timeText, line.dateKey, line.msgId);
    requestAvatar(line.userId);
}

void MainWindow::handleChatBroadcast(const QJsonObject &frame)
{
    if (AppSettings::soundEnabled(AppSettings::kSoundMessage))
        playSound(QStringLiteral("message"));
    const QJsonObject data = frame.value(QStringLiteral("data")).toObject();
    const quint64 fromUser = static_cast<quint64>(data.value(QStringLiteral("fromUser")).toDouble());

    QString key;
    if (data.contains(QStringLiteral("groupId")))
        key = keyOfGroup(static_cast<qint64>(data.value(QStringLiteral("groupId")).toDouble()));
    else
        key = keyOfUser(fromUser == m_userId
                        ? static_cast<quint64>(data.value(QStringLiteral("toUser")).toDouble())
                        : fromUser);

    MessageLine line;
    line.userId = fromUser;
    line.nickname = data.value(QStringLiteral("nickname")).toString();
    line.content = data.value(QStringLiteral("content")).toString();
    line.self = (fromUser == m_userId);
    line.timeText = nowTimeText();
    line.dateKey = QDate::currentDate().toString(QStringLiteral("yyyy-MM-dd"));
    line.msgId = static_cast<qint64>(data.value(QStringLiteral("msgId")).toDouble());
    appendToConversation(key, line);

    if (key != m_currentKey && !line.self)
        appendSystemLine(QStringLiteral("收到来自 %1 的新消息").arg(line.nickname));
}

void MainWindow::onFrameReceived(const QJsonObject &frame)
{
    const QString type = frame.value(QStringLiteral("type")).toString();

    if (type == MsgType::kFileShared) {
        const QJsonObject shareData = frame.value(QStringLiteral("data")).toObject();
        const QString fileName = shareData.value(QStringLiteral("fileName")).toString();
        MessageLine line;
        line.userId = static_cast<quint64>(shareData.value(QStringLiteral("fromUser")).toDouble());
        line.nickname = shareData.value(QStringLiteral("nickname")).toString();
        line.content = QStringLiteral("[群文件] %1（已存入群共享文件夹）").arg(fileName);
        line.self = (line.userId == m_userId);
        line.timeText = nowTimeText();
        line.dateKey = QDate::currentDate().toString(QStringLiteral("yyyy-MM-dd"));
        const QString key = keyOfGroup(static_cast<qint64>(shareData.value(QStringLiteral("groupId")).toDouble()));
        appendToConversation(key, line);
        appendSystemLine(QStringLiteral("群文件已保存：%1").arg(shareData.value(QStringLiteral("savedPath")).toString()));
        return;
    }

    if (type == MsgType::kFileMeta || type == MsgType::kFileChunk
        || type == MsgType::kFileDone || type == MsgType::kFileResp) {
        handleFileFrame(frame);
        return;
    }

    if (type == MsgType::kFriendListResp) {
        const QJsonObject friendData = frame.value(QStringLiteral("data")).toObject();
        m_friendIds.clear();
        m_friendNames.clear();
        for (const QJsonValue &value : friendData.value(QStringLiteral("friends")).toArray()) {
            const QJsonObject friendItem = value.toObject();
            const quint64 friendId = static_cast<quint64>(friendItem.value(QStringLiteral("userId")).toDouble());
            m_friendIds.insert(friendId);
            m_friendNames.insert(friendId, friendItem.value(QStringLiteral("nickname")).toString());
            if (!m_friendIconAsked.contains(friendId)) {
                m_friendIconAsked.insert(friendId);
                QJsonObject iconData;
                iconData.insert(QStringLiteral("userId"), static_cast<double>(friendId));
                QJsonObject iconFrame;
                iconFrame.insert(QStringLiteral("type"), MsgType::kAvatarGet);
                iconFrame.insert(QStringLiteral("ts"), static_cast<double>(QDateTime::currentMSecsSinceEpoch()));
                iconFrame.insert(QStringLiteral("data"), iconData);
                m_client->sendFrame(iconFrame);
            }
        }
        rebuildConversationList();
        return;
    }

    if (type == MsgType::kFriendNotify) {
        const QJsonObject friendData = frame.value(QStringLiteral("data")).toObject();
        const QString event = friendData.value(QStringLiteral("event")).toString();
        appendSystemLine(friendData.value(QStringLiteral("message")).toString());

        if (event == QStringLiteral("request")) {
            m_requestsPage->refreshAll();
        } else {
            if (event == QStringLiteral("deleted")) {
                const quint64 fromUser = static_cast<quint64>(
                    friendData.value(QStringLiteral("fromUser")).toDouble());
                m_friendIds.remove(fromUser);
                m_friendNames.remove(fromUser);
                if (m_currentKey == keyOfUser(fromUser)) {
                    m_currentKey.clear();
                    m_chatPage->clearMessages();
                    if (!m_groups.isEmpty())
                        selectConversation(keyOfGroup(m_groups.firstKey()));
                }
            }
            QJsonObject refresh;
            refresh.insert(QStringLiteral("type"), MsgType::kFriendList);
            refresh.insert(QStringLiteral("ts"), static_cast<double>(QDateTime::currentMSecsSinceEpoch()));
            refresh.insert(QStringLiteral("data"), QJsonObject());
            m_client->sendFrame(refresh);
        }
        return;
    }

    if (type == MsgType::kFriendDeleteResp) {
        const QJsonObject deleteData = frame.value(QStringLiteral("data")).toObject();
        appendSystemLine(deleteData.value(QStringLiteral("msg")).toString());
        return;
    }

    if (type == MsgType::kGroupJoinNotify || type == MsgType::kGroupJoinResult) {
        const QJsonObject joinData = frame.value(QStringLiteral("data")).toObject();
        appendSystemLine(joinData.value(QStringLiteral("message")).toString());
        if (m_requestsPage)
            m_requestsPage->refreshAll();
        return;
    }

    if (type == MsgType::kGroupInfoResp) {
        const QJsonObject infoData = frame.value(QStringLiteral("data")).toObject();
        if (infoData.contains(QStringLiteral("info"))) {
            const QJsonObject info = infoData.value(QStringLiteral("info")).toObject();
            const qint64 headerGroupId = static_cast<qint64>(info.value(QStringLiteral("groupId")).toDouble());
            if (m_currentKey == keyOfGroup(headerGroupId))
                m_chatPage->setMemberCount(info.value(QStringLiteral("memberCount")).toInt());
        }
        return;
    }

    if (type == MsgType::kGroupAvatarResp) {
        const QJsonObject avatarData = frame.value(QStringLiteral("data")).toObject();
        const qint64 groupId = static_cast<qint64>(avatarData.value(QStringLiteral("groupId")).toDouble());
        const QByteArray bytes = QByteArray::fromBase64(
            avatarData.value(QStringLiteral("data")).toString().toLatin1());
        if (!bytes.isEmpty()) {
            const QIcon icon = roundedIcon(bytes, 20);
            if (!icon.isNull()) {
                m_groupIcons.insert(groupId, icon);
                refreshListIcons();
            }
            // 当前正开着这个群聊时，标题栏头像也一起刷新
            if (m_currentKey == keyOfGroup(groupId))
                m_chatPage->setGroupAvatar(bytes);
        }
        return;
    }

    if (type == MsgType::kAvatarResp) {
        const QJsonObject data = frame.value(QStringLiteral("data")).toObject();
        const quint64 userId = static_cast<quint64>(data.value(QStringLiteral("userId")).toDouble());
        const QByteArray bytes = QByteArray::fromBase64(
            data.value(QStringLiteral("data")).toString().toLatin1());
        if (!bytes.isEmpty()) {
            m_chatPage->setAvatar(userId, bytes);
            const QIcon icon = roundedIcon(bytes, 20);
            if (!icon.isNull()) {
                m_friendIcons.insert(userId, icon);
                refreshListIcons();
            }
        }
        return;
    }

    if (type == MsgType::kVoiceMeta || type == MsgType::kVoiceChunk
        || type == MsgType::kVoiceDone || type == MsgType::kVoiceResp) {
        handleVoiceFrame(frame);
        return;
    }

    if (type == MsgType::kUserList) {
        const QJsonArray users = frame.value(QStringLiteral("data")).toObject()
                                      .value(QStringLiteral("users")).toArray();
        const QList<quint64> previousUsers = m_users.keys();   // 用于"好友上线"提示音
        m_users.clear();
        for (const QJsonValue &value : users) {
            const QJsonObject item = value.toObject();
            m_users.insert(static_cast<quint64>(item.value(QStringLiteral("userId")).toDouble()),
                           item.value(QStringLiteral("nickname")).toString());
        }
        if (AppSettings::soundEnabled(AppSettings::kSoundOnline)) {
            for (auto it = m_users.constBegin(); it != m_users.constEnd(); ++it) {
                if (!previousUsers.contains(it.key()) && m_friendIds.contains(it.key())) {
                    playSound(QStringLiteral("online"));
                    break;
                }
            }
        }
        statusBar()->showMessage(QStringLiteral("在线 %1 人").arg(users.size()));
        rebuildConversationList();
        return;
    }

    if (type == MsgType::kGroupListResp) {
        const QJsonArray groups = frame.value(QStringLiteral("data")).toObject()
                                       .value(QStringLiteral("groups")).toArray();
        m_groups.clear();
        m_groupRole.clear();
        for (const QJsonValue &value : groups) {
            const QJsonObject item = value.toObject();
            const qint64 groupId = static_cast<qint64>(item.value(QStringLiteral("groupId")).toDouble());
            m_groups.insert(groupId, item.value(QStringLiteral("name")).toString());
            if (!m_groupIconAsked.contains(groupId)) {
                m_groupIconAsked.insert(groupId);
                QJsonObject iconData;
                iconData.insert(QStringLiteral("groupId"), static_cast<double>(groupId));
                QJsonObject iconFrame;
                iconFrame.insert(QStringLiteral("type"), MsgType::kGroupAvatarGet);
                iconFrame.insert(QStringLiteral("ts"), static_cast<double>(QDateTime::currentMSecsSinceEpoch()));
                iconFrame.insert(QStringLiteral("data"), iconData);
                m_client->sendFrame(iconFrame);
            }
            m_groupRole.insert(groupId, item.value(QStringLiteral("role")).toString());
        }
        rebuildConversationList();
        appendSystemLine(QStringLiteral("我的群组：%1 个").arg(m_groups.size()));
        return;
    }

    if (type == MsgType::kGroupCreateResp || type == MsgType::kGroupInviteResp) {
        const QJsonObject data = frame.value(QStringLiteral("data")).toObject();
        appendSystemLine(data.value(QStringLiteral("msg")).toString());
        refreshGroupList();
        return;
    }

    if (type == MsgType::kChatRecall) {
        const qint64 msgId = static_cast<qint64>(
            frame.value(QStringLiteral("data")).toObject().value(QStringLiteral("msgId")).toDouble());
        m_chatPage->markRecalled(msgId);
        for (MessageLine &line : m_messages[m_currentKey]) {
            if (line.msgId == msgId)
                line.content = QStringLiteral("该消息已撤回");
        }
        return;
    }

    if (type == MsgType::kChatRecallResp) {
        appendSystemLine(frame.value(QStringLiteral("data")).toObject()
                             .value(QStringLiteral("msg")).toString());
        return;
    }

    if (type == MsgType::kChatBroadcast) {
        handleChatBroadcast(frame);
        return;
    }

    if (type == MsgType::kOfflineMsg) {
        const QJsonArray messages = frame.value(QStringLiteral("data")).toObject()
                                         .value(QStringLiteral("messages")).toArray();
        for (const QJsonValue &value : messages)
            handleChatBroadcast(value.toObject());
        appendSystemLine(QStringLiteral("收到 %1 条离线消息").arg(messages.size()));
        return;
    }

    if (type == MsgType::kChatHistoryResp) {
        const QJsonObject data = frame.value(QStringLiteral("data")).toObject();
        const QString scope = data.value(QStringLiteral("scope")).toString();
        QString key;
        if (scope == QStringLiteral("group"))
            key = keyOfGroup(static_cast<qint64>(data.value(QStringLiteral("groupId")).toDouble()));
        else
            key = keyOfUser(static_cast<quint64>(data.value(QStringLiteral("toUser")).toDouble()));

        QList<MessageLine> lines;
        const QJsonArray messages = data.value(QStringLiteral("messages")).toArray();
        for (const QJsonValue &value : messages) {
            const QJsonObject item = value.toObject();
            MessageLine line;
            line.userId = static_cast<quint64>(item.value(QStringLiteral("fromUser")).toDouble());
            line.nickname = item.value(QStringLiteral("nickname")).toString();
            line.content = item.value(QStringLiteral("content")).toString();
            if (item.value(QStringLiteral("recalled")).toBool())
                line.content = QStringLiteral("该消息已撤回");
            line.msgId = static_cast<qint64>(item.value(QStringLiteral("msgId")).toDouble());
            line.self = (static_cast<quint64>(item.value(QStringLiteral("fromUser")).toDouble()) == m_userId);
            line.timeText = item.value(QStringLiteral("createdAt")).toString().mid(11, 5);
            line.dateKey = item.value(QStringLiteral("createdAt")).toString().left(10);
            lines.append(line);
        }
        m_messages[key] = lines;
        m_historyLoaded.insert(key);

        if (key == m_currentKey) {
            m_chatPage->clearMessages();
            for (const MessageLine &line : lines) {
            m_chatPage->appendMessage(line.userId, line.nickname, line.content, line.self,
                                      line.timeText, line.dateKey, line.msgId);
    requestAvatar(line.userId);
}
        }
        return;
    }

    if (type == MsgType::kError) {
        const QJsonObject data = frame.value(QStringLiteral("data")).toObject();
        appendSystemLine(QStringLiteral("错误：%1").arg(data.value(QStringLiteral("msg")).toString()));
        return;
    }

    if (type == MsgType::kKick) {
        const QJsonObject data = frame.value(QStringLiteral("data")).toObject();
        m_client->disconnectFromServer();
        setWindowTitle(QStringLiteral("LunaChat 月之音 - 已被踢下线"));
        appendSystemLine(QStringLiteral("已被踢下线：%1").arg(data.value(QStringLiteral("reason")).toString()));
        return;
    }
}

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

static QIcon roundedIcon(const QByteArray &bytes, int side)
{
    QImage image;
    if (!image.loadFromData(bytes))
        return QIcon();
    const int edge = qMin(image.width(), image.height());
    image = image.copy((image.width() - edge) / 2, (image.height() - edge) / 2, edge, edge)
                    .scaled(side, side, Qt::KeepAspectRatio, Qt::SmoothTransformation);

    QPixmap pixmap(side, side);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    QPainterPath path;
    path.addEllipse(0, 0, side, side);
    painter.setClipPath(path);
    painter.drawImage(0, 0, image);
    return QIcon(pixmap);
}

void MainWindow::refreshListIcons()
{
    // 列表项的图标：好友用真实头像，群用群头像，没有再回退首字
    for (int i = 0; i < m_sessionList->count(); ++i) {
        QListWidgetItem *item = m_sessionList->item(i);
        const QString key = item->data(Qt::UserRole).toString();
        if (key.startsWith(QLatin1Char('u'))) {
            const quint64 userId = key.mid(1).toULongLong();
            if (m_friendIcons.contains(userId))
                item->setIcon(m_friendIcons.value(userId));
        } else if (key.startsWith(QLatin1Char('g'))) {
            const qint64 groupId = key.mid(1).toLongLong();
            if (m_groupIcons.contains(groupId))
                item->setIcon(m_groupIcons.value(groupId));
        }
    }
}

void MainWindow::rebuildConversationList()
{
    const QString currentKey = m_currentKey;
    m_sessionList->blockSignals(true);
    m_sessionList->clear();

    for (auto it = m_groups.constBegin(); it != m_groups.constEnd(); ++it) {
        const qint64 groupId = it.key();
        QString text = it.value();
        const QString role = m_groupRole.value(groupId);
        if (role == QStringLiteral("owner"))
            text += QStringLiteral("（群主）");
        else if (role == QStringLiteral("admin"))
            text += QStringLiteral("（管理员）");

        auto *item = new QListWidgetItem(text, m_sessionList);
        item->setData(Qt::UserRole, keyOfGroup(groupId));
    }

    // 好友：在线优先、离线也保留；离线用 ○ 和灰色文字区分
    QList<quint64> friendIds = m_friendNames.keys();
    std::sort(friendIds.begin(), friendIds.end(), [this](quint64 a, quint64 b) {
        const bool onlineA = m_users.contains(a);
        const bool onlineB = m_users.contains(b);
        if (onlineA != onlineB)
            return onlineA;
        return m_friendNames.value(a).localeAwareCompare(m_friendNames.value(b)) < 0;
    });
    for (quint64 friendId : friendIds) {
        const bool online = m_users.contains(friendId);
        auto *item = new QListWidgetItem(
            QStringLiteral("%1 %2").arg(online ? QStringLiteral("●") : QStringLiteral("○"),
                                        m_friendNames.value(friendId)),
            m_sessionList);
        item->setData(Qt::UserRole, keyOfUser(friendId));
        item->setToolTip(online ? QStringLiteral("在线") : QStringLiteral("离线"));
        if (!online)
            item->setForeground(QColor(0x8A, 0x82, 0xA8));
    }

    // 在线的非好友用户继续保留在列表末尾（方便临时私聊）
    for (auto it = m_users.constBegin(); it != m_users.constEnd(); ++it) {
        if (it.key() == m_userId || m_friendNames.contains(it.key()))
            continue;
        auto *item = new QListWidgetItem(QStringLiteral("● %1").arg(it.value()), m_sessionList);
        item->setData(Qt::UserRole, keyOfUser(it.key()));
    }

    QString wantKey = currentKey;
    if (wantKey.isEmpty() && m_groups.contains(1))
        wantKey = keyOfGroup(1);

    int targetRow = -1;
    for (int i = 0; i < m_sessionList->count(); ++i) {
        if (m_sessionList->item(i)->data(Qt::UserRole).toString() == wantKey) {
            targetRow = i;
            break;
        }
    }
    if (targetRow >= 0)
        m_sessionList->setCurrentRow(targetRow);

    m_sessionList->blockSignals(false);
    refreshListIcons();

    if (!wantKey.isEmpty())
        selectConversation(wantKey);
}

bool MainWindow::nativeEvent(const QByteArray &eventType, void *message, long *result)
{
#ifdef Q_OS_WIN
    if (eventType == QByteArrayLiteral("windows_generic_MSG")) {
        auto *msg = static_cast<MSG *>(message);
        if (msg->message == WM_NCCALCSIZE && msg->wParam == TRUE) {
            *result = 0;
            return true;
        }
        if (msg->message == WM_NCHITTEST && !isMaximized()) {
            const int screenX = GET_X_LPARAM(msg->lParam);
            const int screenY = GET_Y_LPARAM(msg->lParam);
            POINT pt{ screenX, screenY };
            ScreenToClient(reinterpret_cast<HWND>(winId()), &pt);

            const int border = 6;
            const bool left = pt.x < border;
            const bool right = pt.x >= width() - border;
            const bool top = pt.y < border;
            const bool bottom = pt.y >= height() - border;

            if (top && left) { *result = HTTOPLEFT; return true; }
            if (top && right) { *result = HTTOPRIGHT; return true; }
            if (bottom && left) { *result = HTBOTTOMLEFT; return true; }
            if (bottom && right) { *result = HTBOTTOMRIGHT; return true; }
            if (left) { *result = HTLEFT; return true; }
            if (right) { *result = HTRIGHT; return true; }
            if (top) { *result = HTTOP; return true; }
            if (bottom) { *result = HTBOTTOM; return true; }
        }
    }
#endif
    return QMainWindow::nativeEvent(eventType, message, result);
}
