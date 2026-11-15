#include "UI/Widgets/VoiceMessageWidget.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QMediaPlayer>
#include <QPushButton>
#include <QUrl>

VoiceMessageWidget::VoiceMessageWidget(const QString &filePath, int durationMs, bool self,
                                       QWidget *parent)
    : QWidget(parent), m_path(filePath), m_durationMs(durationMs)
{
    m_button = new QPushButton(QStringLiteral("▶"), this);
    m_button->setFixedWidth(34);

    m_durationLabel = new QLabel(QStringLiteral("%1s").arg(durationMs / 1000.0, 0, 'f', 1), this);
    m_durationLabel->setStyleSheet(QStringLiteral("color:#666;"));

    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_button);
    layout->addWidget(m_durationLabel);
    layout->addStretch();

    m_player = new QMediaPlayer(this);
    connect(m_button, &QPushButton::clicked, this, &VoiceMessageWidget::togglePlay);
    connect(m_player, &QMediaPlayer::stateChanged, this, [this](QMediaPlayer::State state) {
        m_button->setText(state == QMediaPlayer::PlayingState ? QStringLiteral("■")
                                                             : QStringLiteral("▶"));
    });

    Q_UNUSED(self)
}

void VoiceMessageWidget::togglePlay()
{
    if (m_player->state() == QMediaPlayer::PlayingState) {
        m_player->stop();
        return;
    }
    m_player->setMedia(QUrl::fromLocalFile(m_path));
    m_player->play();
}