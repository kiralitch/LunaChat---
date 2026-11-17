#include "UI/Pages/ActivityPage.h"

#include <QDateTime>
#include <QJsonArray>
#include <QDateTimeEdit>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QIcon>
#include <QPushButton>
#include <QToolButton>
#include <QSpinBox>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QtDebug>

#include "Core/AppSettings.h"
#include "Core/SoundPlayer.h"
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

ActivityPage::ActivityPage(LunaClient *client, quint64 userId, QWidget *parent)
    : QWidget(parent), m_client(client), m_userId(userId)
{
    setWindowTitle(QStringLiteral("群活动"));
    resize(820, 480);

    m_table = new QTableWidget(this);
    m_table->setColumnCount(6);
    m_table->setHorizontalHeaderLabels(QStringList()
                                       << QStringLiteral("标题")
                                       << QStringLiteral("时间")
                                       << QStringLiteral("地点")
                                       << QStringLiteral("报名")
                                       << QStringLiteral("状态")
                                       << QStringLiteral("我"));
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->verticalHeader()->setVisible(false);

    auto *createButton = new QPushButton(QStringLiteral("发布活动"), this);
    auto *signupButton = new QPushButton(QStringLiteral("报名"), this);
    auto *cancelSignupButton = new QPushButton(QStringLiteral("取消报名"), this);
    auto *cancelButton = new QPushButton(QStringLiteral("取消活动"), this);
    auto *detailButton = new QPushButton(QStringLiteral("详情"), this);
    auto *refreshButton = new QPushButton(QStringLiteral("刷新"), this);

    m_createButton = createButton;
    m_cancelButton = cancelButton;

    auto *buttonRow = new QHBoxLayout;
    buttonRow->addWidget(createButton);
    buttonRow->addWidget(signupButton);
    buttonRow->addWidget(cancelSignupButton);
    buttonRow->addWidget(cancelButton);
    buttonRow->addWidget(detailButton);
    buttonRow->addWidget(refreshButton);
    buttonRow->addStretch();

    m_statusLabel = new QLabel(QStringLiteral("加载中..."), this);

    auto *layout = new QVBoxLayout(this);

    // 顶部返回栏（图标 + 页面标题）
    m_backButton = new QToolButton(this);
    m_backButton->setObjectName(QStringLiteral("PageBackButton"));
    m_backButton->setIcon(QIcon(QStringLiteral(":/icons/back.png")));
    m_backButton->setIconSize(QSize(26, 26));
    m_backButton->setFixedSize(40, 40);
    m_backButton->setAutoRaise(true);
    m_backButton->setCursor(Qt::PointingHandCursor);
    m_backButton->setToolTip(QStringLiteral("返回聊天"));
    connect(m_backButton, &QToolButton::clicked, this, &ActivityPage::backRequested);

    auto *pageHeader = new QHBoxLayout;
    pageHeader->setContentsMargins(0, 0, 0, 4);
    pageHeader->addWidget(m_backButton);
    auto *pageTitle = new QLabel(QStringLiteral("群活动"), this);
    pageTitle->setObjectName(QStringLiteral("ChatTitle"));
    pageHeader->addWidget(pageTitle);
    pageHeader->addStretch();
    layout->addLayout(pageHeader);
    layout->addWidget(m_table, 1);
    layout->addWidget(m_statusLabel);
    layout->addLayout(buttonRow);

    connect(createButton, &QPushButton::clicked, this, &ActivityPage::onCreate);
    connect(signupButton, &QPushButton::clicked, this, &ActivityPage::onSignup);
    connect(cancelSignupButton, &QPushButton::clicked, this, &ActivityPage::onCancelSignup);
    connect(cancelButton, &QPushButton::clicked, this, &ActivityPage::onCancelActivity);
    connect(detailButton, &QPushButton::clicked, this, &ActivityPage::onDetail);
    connect(refreshButton, &QPushButton::clicked, this, &ActivityPage::refresh);
    connect(m_client, &LunaClient::frameReceived, this, &ActivityPage::onFrameReceived);

}

void ActivityPage::requestList()
{
    QJsonObject data;
    data.insert(QStringLiteral("groupId"), static_cast<double>(m_groupId));
    m_client->sendFrame(makeFrame(MsgType::kActivityList, data));
}

void ActivityPage::requestDetail(qint64 activityId)
{
    QJsonObject data;
    data.insert(QStringLiteral("activityId"), static_cast<double>(activityId));
    m_client->sendFrame(makeFrame(MsgType::kActivityDetail, data));
}

void ActivityPage::refresh()
{
    requestList();
}

qint64 ActivityPage::selectedActivityId() const
{
    const int row = m_table->currentRow();
    if (row < 0 || row >= m_table->rowCount())
        return 0;
    return static_cast<qint64>(m_table->item(row, 0)->data(Qt::UserRole).toLongLong());
}

void ActivityPage::onSignup()
{
    const qint64 activityId = selectedActivityId();
    if (activityId == 0) {
        m_statusLabel->setText(QStringLiteral("请先选中一个活动"));
        return;
    }
    QJsonObject data;
    data.insert(QStringLiteral("activityId"), static_cast<double>(activityId));
    m_client->sendFrame(makeFrame(MsgType::kActivitySignup, data));
    m_statusLabel->setText(QStringLiteral("报名中..."));
}

void ActivityPage::onCancelSignup()
{
    const qint64 activityId = selectedActivityId();
    if (activityId == 0) {
        m_statusLabel->setText(QStringLiteral("请先选中一个活动"));
        return;
    }
    QJsonObject data;
    data.insert(QStringLiteral("activityId"), static_cast<double>(activityId));
    m_client->sendFrame(makeFrame(MsgType::kActivitySignupCancel, data));
    m_statusLabel->setText(QStringLiteral("取消报名中..."));
}

void ActivityPage::onCancelActivity()
{
    const qint64 activityId = selectedActivityId();
    if (activityId == 0) {
        m_statusLabel->setText(QStringLiteral("请先选中一个活动"));
        return;
    }
    if (QMessageBox::question(this, QStringLiteral("取消活动"),
                              QStringLiteral("确定取消选中的活动吗？")) != QMessageBox::Yes)
        return;

    QJsonObject data;
    data.insert(QStringLiteral("activityId"), static_cast<double>(activityId));
    data.insert(QStringLiteral("reason"), QStringLiteral("组织者取消了活动"));
    m_client->sendFrame(makeFrame(MsgType::kActivityCancel, data));
    m_statusLabel->setText(QStringLiteral("取消活动请求已发送..."));
}

void ActivityPage::onDetail()
{
    const qint64 activityId = selectedActivityId();
    if (activityId == 0) {
        m_statusLabel->setText(QStringLiteral("请先选中一个活动"));
        return;
    }
    requestDetail(activityId);
}

void ActivityPage::onCreate()
{
    QDialog form(this);
    form.setWindowTitle(QStringLiteral("发布活动"));

    auto *titleEdit = new QLineEdit(&form);
    auto *descEdit = new QLineEdit(&form);
    auto *locationEdit = new QLineEdit(&form);
    auto *startEdit = new QDateTimeEdit(QDateTime::currentDateTime().addDays(1), &form);
    auto *endEdit = new QDateTimeEdit(QDateTime::currentDateTime().addDays(1).addSecs(7200), &form);
    auto *deadlineEdit = new QDateTimeEdit(QDateTime::currentDateTime().addSecs(600), &form);
    auto *maxSpin = new QSpinBox(&form);

    for (QDateTimeEdit *edit : { startEdit, endEdit, deadlineEdit })
        edit->setDisplayFormat(QStringLiteral("yyyy-MM-dd HH:mm"));
    maxSpin->setRange(0, 200);
    maxSpin->setValue(3);
    maxSpin->setSpecialValueText(QStringLiteral("不限"));

    auto *formLayout = new QFormLayout;
    formLayout->addRow(QStringLiteral("标题"), titleEdit);
    formLayout->addRow(QStringLiteral("描述"), descEdit);
    formLayout->addRow(QStringLiteral("地点"), locationEdit);
    formLayout->addRow(QStringLiteral("开始时间"), startEdit);
    formLayout->addRow(QStringLiteral("结束时间"), endEdit);
    formLayout->addRow(QStringLiteral("报名截止"), deadlineEdit);
    formLayout->addRow(QStringLiteral("人数上限"), maxSpin);

    auto *box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &form);
    connect(box, &QDialogButtonBox::accepted, &form, &QDialog::accept);
    connect(box, &QDialogButtonBox::rejected, &form, &QDialog::reject);

    auto *layout = new QVBoxLayout(&form);
    layout->addLayout(formLayout);
    layout->addWidget(box);

    if (form.exec() != QDialog::Accepted)
        return;
    if (titleEdit->text().trimmed().isEmpty()) {
        m_statusLabel->setText(QStringLiteral("标题不能为空"));
        return;
    }

    QJsonObject data;
    data.insert(QStringLiteral("groupId"), static_cast<double>(m_groupId));
    data.insert(QStringLiteral("title"), titleEdit->text().trimmed());
    data.insert(QStringLiteral("description"), descEdit->text());
    data.insert(QStringLiteral("location"), locationEdit->text());
    data.insert(QStringLiteral("startTime"), startEdit->dateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")));
    data.insert(QStringLiteral("endTime"), endEdit->dateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")));
    data.insert(QStringLiteral("signupDeadline"), deadlineEdit->dateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")));
    data.insert(QStringLiteral("maxParticipants"), maxSpin->value());
    m_client->sendFrame(makeFrame(MsgType::kActivityCreate, data));
    m_statusLabel->setText(QStringLiteral("发布中..."));
}

void ActivityPage::onFrameReceived(const QJsonObject &frame)
{
    const QString type = frame.value(QStringLiteral("type")).toString();
    const QJsonObject data = frame.value(QStringLiteral("data")).toObject();

    if (type == MsgType::kActivityListResp) {
        const QJsonArray activities = data.value(QStringLiteral("activities")).toArray();
        m_table->setRowCount(0);
        for (const QJsonValue &value : activities) {
            const QJsonObject item = value.toObject();
            const int row = m_table->rowCount();
            m_table->insertRow(row);

            auto *titleItem = new QTableWidgetItem(item.value(QStringLiteral("title")).toString());
            titleItem->setData(Qt::UserRole,
                               static_cast<qlonglong>(item.value(QStringLiteral("activityId")).toDouble()));
            m_table->setItem(row, 0, titleItem);
            m_table->setItem(row, 1, new QTableWidgetItem(QStringLiteral("%1 ~ %2")
                .arg(item.value(QStringLiteral("startTime")).toString(),
                     item.value(QStringLiteral("endTime")).toString().mid(11))));
            m_table->setItem(row, 2, new QTableWidgetItem(item.value(QStringLiteral("location")).toString()));
            m_table->setItem(row, 3, new QTableWidgetItem(QStringLiteral("%1/%2")
                .arg(item.value(QStringLiteral("signedCount")).toInt())
                .arg(item.value(QStringLiteral("maxParticipants")).toInt() == 0
                         ? QStringLiteral("不限")
                         : QString::number(item.value(QStringLiteral("maxParticipants")).toInt()))));
            m_table->setItem(row, 4, new QTableWidgetItem(item.value(QStringLiteral("status")).toString()));
            m_table->setItem(row, 5, new QTableWidgetItem(
                item.value(QStringLiteral("signed")).toBool() ? QStringLiteral("已报名")
                                                              : QStringLiteral("未报名")));
        }
        m_table->resizeColumnsToContents();
        m_statusLabel->setText(QStringLiteral("共 %1 个活动").arg(activities.size()));
        return;
    }

    if (type == MsgType::kActivityNotify) {
        m_statusLabel->setText(data.value(QStringLiteral("message")).toString());
        requestList();

        // 新活动发布 / 活动开始 / 活动取消都会走 kActivityNotify：统一响"活动提醒音"
        if (AppSettings::soundEnabled(AppSettings::kSoundActivity))
            SoundPlayer::play(QStringLiteral("activity"));
        return;
    }

    if (type == MsgType::kActivityDetailResp) {
        QStringList lines;
        lines << QStringLiteral("活动：%1").arg(data.value(QStringLiteral("title")).toString());
        lines << QStringLiteral("状态：%1").arg(data.value(QStringLiteral("status")).toString());
        lines << QStringLiteral("地点：%1").arg(data.value(QStringLiteral("location")).toString());
        lines << QStringLiteral("时间：%1 ~ %2").arg(data.value(QStringLiteral("startTime")).toString(),
                                                     data.value(QStringLiteral("endTime")).toString());
        lines << QStringLiteral("报名截止：%1").arg(data.value(QStringLiteral("signupDeadline")).toString());
        lines << QStringLiteral("发起人：%1").arg(data.value(QStringLiteral("creatorNickname")).toString());
        lines << QStringLiteral("描述：%1").arg(data.value(QStringLiteral("description")).toString());

        QStringList people;
        const QJsonArray participants = data.value(QStringLiteral("participants")).toArray();
        for (const QJsonValue &value : participants)
            people << value.toObject().value(QStringLiteral("nickname")).toString();
        lines << QStringLiteral("已报名（%1）：%2").arg(people.size()).arg(people.join(QStringLiteral("、")));

        QMessageBox::information(this, QStringLiteral("活动详情"), lines.join(QLatin1Char('\n')));
        return;
    }

    if (type == MsgType::kError) {
        m_statusLabel->setText(QStringLiteral("错误：%1").arg(data.value(QStringLiteral("msg")).toString()));
        return;
    }
}
void ActivityPage::setGroup(qint64 groupId, bool canManage)
{
    m_groupId = groupId;
    m_canManage = canManage;
    if (m_createButton)
        m_createButton->setEnabled(canManage);
    if (m_cancelButton)
        m_cancelButton->setEnabled(canManage);
    refresh();
}
