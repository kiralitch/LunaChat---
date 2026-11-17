#pragma once

#include <QWidget>

class QAudioInput;
class QCheckBox;
class QComboBox;
class QIODevice;
class QProgressBar;
class QPushButton;
class QSlider;

// 设置页面（覆盖整个右侧区域：左栏 + 聊天区，只留最左侧图标栏）
class SettingsPage : public QWidget
{
    Q_OBJECT

public:
    explicit SettingsPage(QWidget *parent = nullptr);
    ~SettingsPage() override;

signals:
    void backRequested();   // 点"返回"回到主界面

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