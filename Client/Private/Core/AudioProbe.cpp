#include "Core/AudioProbe.h"

#include <QAudioDeviceInfo>
#include <QAudioFormat>
#include <QMediaPlayer>
#include <QMediaRecorder>
#include <QStringList>

namespace LunaAudio {

QStringList describeCapabilities()
{
    QStringList lines;
    lines << QStringLiteral("===== 音频能力探测 =====");

    const QList<QAudioDeviceInfo> inputs = QAudioDeviceInfo::availableDevices(QAudio::AudioInput);
    lines << QStringLiteral("输入设备数量: %1").arg(inputs.size());

    for (const QAudioDeviceInfo &device : inputs) {
        lines << QStringLiteral("  设备: %1%2")
                     .arg(device.deviceName(), device.isNull() ? QStringLiteral(" (无效)")
                                                               : QString());

        QStringList rates;
        for (int rate : device.supportedSampleRates())
            rates << QString::number(rate);
        lines << QStringLiteral("    支持采样率: %1").arg(rates.join(QStringLiteral(", ")));

        QStringList channels;
        for (int channel : device.supportedChannelCounts())
            channels << QString::number(channel);
        lines << QStringLiteral("    支持声道数: %1").arg(channels.join(QStringLiteral(", ")));
    }

    // 这两行决定我们到底能不能"转码"：只有列出来的容器/编码才有希望
    // Qt5 的 supportedXxx() 是实例方法，必须挂在一个 QMediaPlayer 上
    QMediaPlayer player;
    QMediaRecorder recorder(&player);

    const QStringList containers = recorder.supportedContainers();
    const QStringList codecs = recorder.supportedAudioCodecs();
    lines << QStringLiteral("QMediaRecorder 支持容器: %1")
                 .arg(containers.join(QStringLiteral(", ")));
    lines << QStringLiteral("QMediaRecorder 支持音频编码: %1")
                 .arg(codecs.join(QStringLiteral(", ")));
    const bool ogg = codecs.contains(QStringLiteral("vorbis"), Qt::CaseInsensitive)
                     || containers.contains(QStringLiteral("ogg"), Qt::CaseInsensitive);
    const bool mp3 = codecs.contains(QStringLiteral("mp3"), Qt::CaseInsensitive)
                     || containers.contains(QStringLiteral("mp3"), Qt::CaseInsensitive);
    lines << QStringLiteral("结论: ogg=%1 mp3=%2 -> 目标格式 %3")
                 .arg(ogg ? QStringLiteral("可用") : QStringLiteral("不可用"),
                      mp3 ? QStringLiteral("可用") : QStringLiteral("不可用"),
                      ogg ? QStringLiteral("ogg")
                          : (mp3 ? QStringLiteral("mp3") : QStringLiteral("wav(降级)")));
    return lines;
}

}