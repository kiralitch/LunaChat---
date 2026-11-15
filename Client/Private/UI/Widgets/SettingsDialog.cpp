#include "UI/Widgets/SettingsDialog.h"

#include <QAudioDeviceInfo>
#include <QAudioFormat>
#include <QAudioInput>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QSlider>
#include <QVBoxLayout>
#include <QtDebug>

#include "Core/AppSettings.h"

SettingsDialog::SettingsDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("设置"));
    resize(460, 420);

    // ---- 主题 ----
    m_themeBox = new QComboBox(this);
    m_themeBox->addItem(QStringLiteral("夜间"), QStringLiteral("dark"));
    m_themeBox->addItem(QStringLiteral("日间"), QStringLiteral("light"));
    m_themeBox->setCurrentIndex(AppSettings::theme() == QStringLiteral("light") ? 1 : 0);

    // ---- 音效 ----
    m_messageSound = new QCheckBox(QStringLiteral("消息提示音"), this);
    m_onlineSound = new QCheckBox(QStringLiteral("好友上线音"), this);
    m_matchSound = new QCheckBox(QStringLiteral("匹配成功音"), this);
    m_activitySound = new QCheckBox(QStringLiteral("活动提醒音"), this);
    m_messageSound->setChecked(AppSettings::soundEnabled(AppSettings::kSoundMessage));
    m_onlineSound->setChecked(AppSettings::soundEnabled(AppSettings::kSoundOnline));
    m_matchSound->setChecked(AppSettings::soundEnabled(AppSettings::kSoundMatch));
    m_activitySound->setChecked(AppSettings::soundEnabled(AppSettings::kSoundActivity));

    m_volumeSlider = new QSlider(Qt::Horizontal, this);
    m_volumeSlider->setRange(0, 100);
    m_volumeSlider->setValue(AppSettings::soundVolume());

    // ---- 录音设备 + 电平监测 ----
    m_deviceBox = new QComboBox(this);
    m_levelBar = new QProgressBar(this);
    m_levelBar->setRange(0, 100);
    m_levelBar->setValue(0);
    m_monitorButton = new QPushButton(QStringLiteral("测试电平"), this);

    auto *monitorRow = new QHBoxLayout;
    monitorRow->addWidget(m_deviceBox, 1);
    monitorRow->addWidget(m_monitorButton);

    auto *form = new QFormLayout;
    form->addRow(QStringLiteral("主题"), m_themeBox);
    form->addRow(QStringLiteral("消息提示音"), m_messageSound);
    form->addRow(QStringLiteral("好友上线音"), m_onlineSound);
    form->addRow(QStringLiteral("匹配成功音"), m_matchSound);
    form->addRow(QStringLiteral("活动提醒音"), m_activitySound);
    form->addRow(QStringLiteral("音量"), m_volumeSlider);
    form->addRow(QStringLiteral("录音设备"), monitorRow);
    form->addRow(QStringLiteral("输入电平"), m_levelBar);

    auto *box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(box, &QDialogButtonBox::accepted, this, &SettingsDialog::onAccepted);
    connect(box, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto *layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addStretch();
    layout->addWidget(box);

    connect(m_monitorButton, &QPushButton::clicked, this, &SettingsDialog::toggleLevelMonitor);

    refreshDevices();

    connect(this, &QDialog::finished, this, [this](int) { stopMonitor(); });
}

SettingsDialog::~SettingsDialog()
{
    stopMonitor();
}

void SettingsDialog::refreshDevices()
{
    m_deviceBox->clear();
    const QList<QAudioDeviceInfo> devices = QAudioDeviceInfo::availableDevices(QAudio::AudioInput);
    for (const QAudioDeviceInfo &device : devices)
        m_deviceBox->addItem(device.deviceName());

    const QString saved = AppSettings::inputDeviceName();
    const int index = m_deviceBox->findText(saved);
    if (index >= 0)
        m_deviceBox->setCurrentIndex(index);
}

void SettingsDialog::toggleLevelMonitor()
{
    if (m_input) {
        stopMonitor();
        return;
    }
    startMonitor();
}

void SettingsDialog::startMonitor()
{
    const QList<QAudioDeviceInfo> devices = QAudioDeviceInfo::availableDevices(QAudio::AudioInput);
    if (devices.isEmpty()) {
        m_monitorButton->setText(QStringLiteral("没有录音设备"));
        return;
    }

    QAudioDeviceInfo device;
    const int index = m_deviceBox->currentIndex();
    device = (index >= 0 && index < devices.size()) ? devices.at(index) : QAudioDeviceInfo::defaultInputDevice();

    QAudioFormat format;
    format.setSampleRate(16000);
    format.setChannelCount(1);
    format.setSampleSize(16);
    format.setCodec(QStringLiteral("audio/pcm"));
    format.setByteOrder(QAudioFormat::LittleEndian);
    format.setSampleType(QAudioFormat::SignedInt);
    if (!device.isFormatSupported(format))
        format = device.nearestFormat(format);

    m_input = new QAudioInput(device, format, this);
    m_device = m_input->start();
    if (!m_device) {
        delete m_input;
        m_input = nullptr;
        m_monitorButton->setText(QStringLiteral("启动失败"));
        return;
    }

    connect(m_device, &QIODevice::readyRead, this, [this]() {
        const QByteArray chunk = m_device->readAll();
        const int count = chunk.size() / 2;
        const qint16 *samples = reinterpret_cast<const qint16 *>(chunk.constData());
        qreal peak = 0.0;
        for (int i = 0; i < count; ++i)
            peak = qMax(peak, qAbs(static_cast<qreal>(samples[i])) / 32768.0);
        m_levelBar->setValue(static_cast<int>(peak * 100));
    });

    m_monitorButton->setText(QStringLiteral("停止测试"));
    qInfo() << "[settings] 电平监测启动:" << device.deviceName();
}

void SettingsDialog::stopMonitor()
{
    if (!m_input)
        return;

    m_input->stop();
    m_input->deleteLater();
    m_input = nullptr;
    m_device = nullptr;
    m_levelBar->setValue(0);
    m_monitorButton->setText(QStringLiteral("测试电平"));
}

void SettingsDialog::onAccepted()
{
    AppSettings::setTheme(m_themeBox->currentData().toString());
    AppSettings::setSoundEnabled(AppSettings::kSoundMessage, m_messageSound->isChecked());
    AppSettings::setSoundEnabled(AppSettings::kSoundOnline, m_onlineSound->isChecked());
    AppSettings::setSoundEnabled(AppSettings::kSoundMatch, m_matchSound->isChecked());
    AppSettings::setSoundEnabled(AppSettings::kSoundActivity, m_activitySound->isChecked());
    AppSettings::setSoundVolume(m_volumeSlider->value());
    AppSettings::setInputDeviceName(m_deviceBox->currentText());

    stopMonitor();
    accept();
}