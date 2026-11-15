#pragma once

#include <QDialog>

class QAudioInput;
class QCheckBox;
class QComboBox;
class QIODevice;
class QProgressBar;
class QSlider;

// 设置页：主题、音效开关与音量、录音设备枚举与实时电平监测
class SettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SettingsDialog(QWidget *parent = nullptr);
    ~SettingsDialog() override;

private slots:
    void toggleLevelMonitor();
    void onAccepted();

private:
    void startMonitor();
    void stopMonitor();
    void refreshDevices();

    QComboBox *m_themeBox = nullptr;
    QComboBox *m_deviceBox = nullptr;
    QProgressBar *m_levelBar = nullptr;
    QPushButton *m_monitorButton = nullptr;
    QSlider *m_volumeSlider = nullptr;
    QCheckBox *m_messageSound = nullptr;
    QCheckBox *m_onlineSound = nullptr;
    QCheckBox *m_matchSound = nullptr;
    QCheckBox *m_activitySound = nullptr;

    QAudioInput *m_input = nullptr;
    QIODevice *m_device = nullptr;
};