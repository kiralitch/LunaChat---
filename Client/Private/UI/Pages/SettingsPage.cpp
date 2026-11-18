#include "UI/Pages/SettingsPage.h"

#include <QApplication>
#include <QAudioDeviceInfo>
#include <QAudioFormat>
#include <QAudioInput>
#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QSlider>
#include <QStringList>
#include <QVBoxLayout>
#include <QtDebug>

#include "Core/AppSettings.h"

SettingsPage::SettingsPage(QWidget *parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("SettingsPage"));
    resize(460, 420);

    // ---- 主题 ----
    m_themeBox = new QComboBox(this);
    m_themeBox->addItem(QStringLiteral("夜间"), QStringLiteral("dark"));
    m_themeBox->addItem(QStringLiteral("日间"), QStringLiteral("light"));
    m_themeBox->setCurrentIndex(AppSettings::theme() == QStringLiteral("light") ? 1 : 0);

    // ---- 音效 ----
    // 行标签已经写明"消息提示音"等名称，复选框只表示开关，避免一行出现两个同样的字
    m_messageSound = new QCheckBox(QStringLiteral("启用"), this);
    m_onlineSound = new QCheckBox(QStringLiteral("启用"), this);
    m_matchSound = new QCheckBox(QStringLiteral("启用"), this);
    m_activitySound = new QCheckBox(QStringLiteral("启用"), this);
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

    // 每类音效一行：复选框 + [选择文件] + 当前文件名。
    // 文件直接落到程序目录的 sounds/<kind>.<ext>，与 SoundPlayer 的查找规则一致，打包后照样可用。
    const QString soundDir = QCoreApplication::applicationDirPath() + QStringLiteral("/sounds");
    const QStringList soundExtensions = { QStringLiteral("wav"), QStringLiteral("mp3"), QStringLiteral("ogg") };

    auto makeSoundRow = [this, soundDir, soundExtensions](QCheckBox *box, const QString &kind) -> QWidget * {
        auto *row = new QWidget(this);
        auto *h = new QHBoxLayout(row);
        h->setContentsMargins(0, 0, 0, 0);
        h->addWidget(box);

        auto *pickButton = new QPushButton(QStringLiteral("选择文件"), row);
        auto *fileLabel = new QLabel(row);
        fileLabel->setObjectName(QStringLiteral("ChatSubtitle"));   // 复用主题里的次要文字样式
        h->addWidget(pickButton);
        h->addWidget(fileLabel, 1);

        const auto refreshLabel = [fileLabel, soundDir, soundExtensions, kind]() {
            for (const QString &ext : soundExtensions) {
                const QString file = soundDir + QStringLiteral("/%1.%2").arg(kind, ext);
                if (QFile::exists(file)) {
                    fileLabel->setText(QFileInfo(file).fileName());
                    return;
                }
            }
            fileLabel->setText(QStringLiteral("未选择（无文件时不响）"));
        };
        refreshLabel();

        connect(pickButton, &QPushButton::clicked, this,
                [this, kind, soundDir, soundExtensions, fileLabel, refreshLabel]() {
            const QString path = QFileDialog::getOpenFileName(
                this, QStringLiteral("选择音频文件"), QString(),
                QStringLiteral("音频 (*.wav *.mp3 *.ogg)"));
            if (path.isEmpty())
                return;

            const QString ext = QFileInfo(path).suffix().toLower();
            if (!soundExtensions.contains(ext)) {
                fileLabel->setText(QStringLiteral("只支持 wav / mp3 / ogg"));
                return;
            }

            QDir().mkpath(soundDir);
            // 同一个 kind 只保留一份：先清掉三种扩展名的旧文件，避免播放时命中旧音频
            for (const QString &oldExt : soundExtensions)
                QFile::remove(soundDir + QStringLiteral("/%1.%2").arg(kind, oldExt));

            const QString target = soundDir + QStringLiteral("/%1.%2").arg(kind, ext);
            if (!QFile::copy(path, target)) {
                fileLabel->setText(QStringLiteral("拷贝失败"));
                return;
            }
            refreshLabel();
        });
        return row;
    };

    auto *form = new QFormLayout;
    form->addRow(QStringLiteral("主题"), m_themeBox);
    form->addRow(QStringLiteral("消息提示音"), makeSoundRow(m_messageSound, QStringLiteral("message")));
    form->addRow(QStringLiteral("好友上线音"), makeSoundRow(m_onlineSound, QStringLiteral("online")));
    form->addRow(QStringLiteral("匹配成功音"), makeSoundRow(m_matchSound, QStringLiteral("match")));
    form->addRow(QStringLiteral("活动提醒音"), makeSoundRow(m_activitySound, QStringLiteral("activity")));
    form->addRow(QStringLiteral("音量"), m_volumeSlider);
    form->addRow(QStringLiteral("录音设备"), monitorRow);
    form->addRow(QStringLiteral("输入电平"), m_levelBar);
    // 页面版：保存 + 返回（不再是对话框的 OK/Cancel）
    auto *saveButton = new QPushButton(QStringLiteral("保存设置"), this);
    saveButton->setObjectName(QStringLiteral("PrimaryButton"));
    auto *backButton = new QPushButton(QStringLiteral("返回"), this);
    auto *box = new QHBoxLayout;
    box->addStretch();
    box->addWidget(backButton);
    box->addWidget(saveButton);
    connect(saveButton, &QPushButton::clicked, this, &SettingsPage::onAccepted);
    connect(backButton, &QPushButton::clicked, this, [this]() {
        stopMonitor();
        emit backRequested();
    });

    auto *pageTitle = new QLabel(QStringLiteral("设置"), this);
    pageTitle->setObjectName(QStringLiteral("ChatTitle"));
    auto *pageHint = new QLabel(QStringLiteral("外观 · 音效 · 录音"), this);
    pageHint->setObjectName(QStringLiteral("ChatSubtitle"));

    auto *titleBox = new QVBoxLayout;
    titleBox->setContentsMargins(0, 0, 0, 0);
    titleBox->setSpacing(0);
    titleBox->addWidget(pageTitle);
    titleBox->addWidget(pageHint);

    auto *headerRow = new QHBoxLayout;
    headerRow->setContentsMargins(0, 0, 0, 0);
    headerRow->addLayout(titleBox, 1);

    auto *card = new QFrame(this);
    card->setObjectName(QStringLiteral("SettingsCard"));
    auto *cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(20, 18, 20, 18);
    cardLayout->addLayout(form);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(18, 8, 18, 14);
    layout->setSpacing(10);
    layout->addLayout(headerRow);
    layout->addWidget(card);
    layout->addStretch();

    layout->addLayout(box);

    connect(m_monitorButton, &QPushButton::clicked, this, &SettingsPage::toggleLevelMonitor);

    refreshDevices();
}

SettingsPage::~SettingsPage()
{
    stopMonitor();
}

void SettingsPage::refreshDevices()
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

void SettingsPage::toggleLevelMonitor()
{
    if (m_input) {
        stopMonitor();
        return;
    }
    startMonitor();
}

void SettingsPage::startMonitor()
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

void SettingsPage::stopMonitor()
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

void SettingsPage::onAccepted()
{
    // 保存后立刻应用新主题（否则只有重启才生效）
    AppSettings::setTheme(m_themeBox->currentData().toString());
    if (qApp)
        qApp->setStyleSheet(AppSettings::loadStyleSheet(AppSettings::theme()));
    AppSettings::setSoundEnabled(AppSettings::kSoundMessage, m_messageSound->isChecked());
    AppSettings::setSoundEnabled(AppSettings::kSoundOnline, m_onlineSound->isChecked());
    AppSettings::setSoundEnabled(AppSettings::kSoundMatch, m_matchSound->isChecked());
    AppSettings::setSoundEnabled(AppSettings::kSoundActivity, m_activitySound->isChecked());
    AppSettings::setSoundVolume(m_volumeSlider->value());
    AppSettings::setInputDeviceName(m_deviceBox->currentText());

    stopMonitor();
    emit backRequested();
}