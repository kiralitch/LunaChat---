#include "UI/Pages/MatchPage.h"

#include <QComboBox>
#include <QDateTime>
#include <QGraphicsDropShadowEffect>
#include <QGraphicsOpacityEffect>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPropertyAnimation>
#include <QPushButton>
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

MatchPage::MatchPage(LunaClient *client, quint64 userId, const QString &nickname, QWidget *parent)
    : QWidget(parent), m_client(client), m_userId(userId), m_nickname(nickname)
{
    setObjectName(QStringLiteral("MatchPage"));
    resize(620, 520);

    m_modeBox = new QComboBox(this);
    m_modeBox->addItem(QStringLiteral("文字匹配"), QStringLiteral("text"));
    m_modeBox->addItem(QStringLiteral("语音匹配"), QStringLiteral("voice"));

    m_startButton = new QPushButton(QStringLiteral("开始匹配"), this);
    m_startButton->setObjectName(QStringLiteral("PrimaryButton"));
    m_cancelButton = new QPushButton(QStringLiteral("取消匹配"), this);
    m_endButton = new QPushButton(QStringLiteral("结束会话"), this);
    m_sendButton = new QPushButton(QStringLiteral("发送"), this);
    m_sendButton->setObjectName(QStringLiteral("PrimaryButton"));
    m_cancelButton->setEnabled(false);
    m_endButton->setEnabled(false);
    m_sendButton->setEnabled(false);

    m_moonLabel = new QLabel(QStringLiteral("月"), this);
    m_moonLabel->setAlignment(Qt::AlignCenter);
    m_moonLabel->setStyleSheet(QStringLiteral("font-size:48px; color:#B9A8FF;"));
    {
        auto *glow = new QGraphicsDropShadowEffect(m_moonLabel);
        glow->setBlurRadius(38);
        glow->setColor(QColor(124, 92, 255, 190));
        glow->setOffset(0, 0);
        m_moonLabel->setGraphicsEffect(glow);
    }

    m_statusLabel = new QLabel(QStringLiteral("选择模式后点击开始匹配"), this);
    m_statusLabel->setObjectName(QStringLiteral("ChatSubtitle"));
    m_statusLabel->setAlignment(Qt::AlignCenter);

    auto *pageTitle = new QLabel(QStringLiteral("随机匹配"), this);
    pageTitle->setObjectName(QStringLiteral("ChatTitle"));
    auto *pageHint = new QLabel(QStringLiteral("随机配对一位在线同学，聊完即散"), this);
    pageHint->setObjectName(QStringLiteral("ChatSubtitle"));

    auto *titleBox = new QVBoxLayout;
    titleBox->setContentsMargins(0, 0, 0, 0);
    titleBox->setSpacing(0);
    titleBox->addWidget(pageTitle);
    titleBox->addWidget(pageHint);

    m_list = new QListWidget(this);
    m_input = new QLineEdit(this);
    m_input->setPlaceholderText(QStringLiteral("匹配成功后才能发送消息"));
    m_input->setEnabled(false);

    auto *topRow = new QHBoxLayout;
    topRow->addWidget(m_modeBox);
    topRow->addWidget(m_startButton);
    topRow->addWidget(m_cancelButton);
    topRow->addWidget(m_endButton);

    auto *inputRow = new QHBoxLayout;
    inputRow->addWidget(m_input, 1);
    inputRow->addWidget(m_sendButton);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(18, 8, 18, 14);
    layout->setSpacing(10);
    layout->addLayout(titleBox);
    layout->addWidget(m_moonLabel);
    layout->addWidget(m_statusLabel);
    layout->addLayout(topRow);
    layout->addWidget(m_list, 1);
    layout->addLayout(inputRow);

    connect(m_startButton, &QPushButton::clicked, this, &MatchPage::onStart);
    connect(m_cancelButton, &QPushButton::clicked, this, &MatchPage::onCancel);
    connect(m_endButton, &QPushButton::clicked, this, &MatchPage::onEnd);
    connect(m_sendButton, &QPushButton::clicked, this, &MatchPage::onSend);
    connect(m_input, &QLineEdit::returnPressed, this, &MatchPage::onSend);
    connect(m_client, &LunaClient::frameReceived, this, &MatchPage::onFrameReceived);

    // 月相呼吸动画：QPropertyAnimation 只改透明度，不需要任何图片资源
    m_moonEffect = new QGraphicsOpacityEffect(m_moonLabel);
    m_moonLabel->setGraphicsEffect(m_moonEffect);
    m_moonAnimation = new QPropertyAnimation(m_moonEffect, "opacity", this);
    m_moonAnimation->setDuration(1200);
    m_moonAnimation->setStartValue(0.25);
    m_moonAnimation->setEndValue(1.0);
    m_moonAnimation->setEasingCurve(QEasingCurve::InOutSine);
    m_moonAnimation->setLoopCount(-1);
    m_moonAnimation->start();
}

void MatchPage::onStart()
{
    QJsonObject data;
    data.insert(QStringLiteral("mode"), m_modeBox->currentData().toString());
    m_client->sendFrame(makeFrame(MsgType::kMatchStart, data));

    m_statusLabel->setText(QStringLiteral("正在寻找同频的人..."));
    m_startButton->setEnabled(false);
    m_cancelButton->setEnabled(true);
    m_moonAnimation->start();
}

void MatchPage::onCancel()
{
    m_client->sendFrame(makeFrame(MsgType::kMatchCancel, QJsonObject()));
    m_statusLabel->setText(QStringLiteral("已取消匹配"));
    m_startButton->setEnabled(true);
    m_cancelButton->setEnabled(false);
}

void MatchPage::onEnd()
{
    QJsonObject data;
    data.insert(QStringLiteral("sessionId"), m_sessionId);
    m_client->sendFrame(makeFrame(MsgType::kMatchEnd, data));
    appendLine(QStringLiteral("（你结束了本次会话）"), true);
    resetSession();
}

void MatchPage::onSend()
{
    const QString text = m_input->text().trimmed();
    if (text.isEmpty() || m_sessionId.isEmpty())
        return;

    QJsonObject data;
    data.insert(QStringLiteral("sessionId"), m_sessionId);
    data.insert(QStringLiteral("content"), text);
    m_client->sendFrame(makeFrame(MsgType::kMatchMessage, data));
    m_input->clear();
}

void MatchPage::appendLine(const QString &text, bool self)
{
    QListWidgetItem *item = new QListWidgetItem(m_list);
    item->setText(text);
    item->setTextAlignment(self ? Qt::AlignRight : Qt::AlignLeft);
    m_list->scrollToBottom();
}

void MatchPage::resetSession()
{
    m_sessionId.clear();
    m_peerId = 0;
    m_peerName.clear();
    m_input->setEnabled(false);
    m_sendButton->setEnabled(false);
    m_endButton->setEnabled(false);
    m_startButton->setEnabled(true);
    m_cancelButton->setEnabled(false);
}

void MatchPage::onFrameReceived(const QJsonObject &frame)
{
    const QString type = frame.value(QStringLiteral("type")).toString();
    const QJsonObject data = frame.value(QStringLiteral("data")).toObject();

    if (type == MsgType::kMatchResult) {
        m_sessionId = data.value(QStringLiteral("sessionId")).toString();
        m_peerId = static_cast<quint64>(data.value(QStringLiteral("peerUserId")).toDouble());
        m_peerName = data.value(QStringLiteral("peerNickname")).toString();

        m_statusLabel->setText(QStringLiteral("已匹配到 %1，聊两句吧").arg(m_peerName));
        m_list->clear();
        appendLine(QStringLiteral("（已与 %1 建立临时会话，%2）")
                       .arg(m_peerName, data.value(QStringLiteral("mode")).toString()),
                   false);

        m_input->setEnabled(true);
        m_sendButton->setEnabled(true);
        m_endButton->setEnabled(true);
        m_startButton->setEnabled(false);
        m_cancelButton->setEnabled(false);
        m_moonAnimation->stop();

        // 匹配成功：会话已建立，响一声（开关 + 音量的取值逻辑都在 SoundPlayer 里）
        if (AppSettings::soundEnabled(AppSettings::kSoundMatch))
            SoundPlayer::play(QStringLiteral("match"));
        return;
    }

    if (type == MsgType::kMatchMessage) {
        const bool self = (static_cast<quint64>(data.value(QStringLiteral("fromUser")).toDouble()) == m_userId);
        appendLine(QStringLiteral("%1: %2")
                       .arg(data.value(QStringLiteral("nickname")).toString(),
                            data.value(QStringLiteral("content")).toString()),
                   self);
        return;
    }

    if (type == MsgType::kMatchTimeout) {
        m_statusLabel->setText(QStringLiteral("暂时没匹配到人，可以再试一次"));
        resetSession();
        m_moonAnimation->start();
        return;
    }

    if (type == MsgType::kMatchEnd) {
        m_statusLabel->setText(QStringLiteral("会话结束：%1").arg(data.value(QStringLiteral("reason")).toString()));
        appendLine(QStringLiteral("（会话已结束）"), false);
        resetSession();
        m_moonAnimation->start();
        return;
    }

    if (type == MsgType::kError) {
        m_statusLabel->setText(QStringLiteral("错误：%1").arg(data.value(QStringLiteral("msg")).toString()));
        resetSession();
        m_moonAnimation->start();
        return;
    }
}