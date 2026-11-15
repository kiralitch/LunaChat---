#pragma once

#include <QWidget>

class QLabel;
class QMediaPlayer;
class QPushButton;

// 语音条：播放/停止 + 时长显示（QMediaPlayer 只解码，符合"不引第三方库"的约束）
class VoiceMessageWidget : public QWidget
{
    Q_OBJECT

public:
    VoiceMessageWidget(const QString &filePath, int durationMs, bool self,
                       QWidget *parent = nullptr);

private slots:
    void togglePlay();

private:
    QMediaPlayer *m_player = nullptr;
    QPushButton *m_button = nullptr;
    QLabel *m_durationLabel = nullptr;
    QString m_path;
    int m_durationMs = 0;
};