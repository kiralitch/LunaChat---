#include "Core/Recorder.h"

#include <QAudioDeviceInfo>
#include <QAudioFormat>
#include <QAudioInput>
#include <QDataStream>
#include <QElapsedTimer>
#include <QTimer>
#include <QtDebug>
#include <QtEndian>

namespace {
const int kMaxDurationMs = 60 * 1000;
}

Recorder::Recorder(QObject *parent)
    : QObject(parent)
{
    m_limitTimer = new QTimer(this);
    m_limitTimer->setSingleShot(true);
    connect(m_limitTimer, &QTimer::timeout, this, [this]() {
        qInfo() << "[recorder] 达到 60 秒上限，自动停止";
        stop();
    });
}

Recorder::~Recorder()
{
    if (m_recording)
        stopInternal(false);
}

bool Recorder::start()
{
    if (m_recording)
        return true;

    const QAudioDeviceInfo device = QAudioDeviceInfo::defaultInputDevice();
    if (device.isNull()) {
        m_lastError = QStringLiteral("没有可用的录音设备");
        return false;
    }

    QAudioFormat format;
    format.setSampleRate(m_sampleRate);
    format.setChannelCount(m_channels);
    format.setSampleSize(16);
    format.setCodec(QStringLiteral("audio/pcm"));
    format.setByteOrder(QAudioFormat::LittleEndian);
    format.setSampleType(QAudioFormat::SignedInt);

    if (!device.isFormatSupported(format))
        format = device.nearestFormat(format);

    m_sampleRate = format.sampleRate();
    m_channels = format.channelCount();

    m_pcm.clear();
    m_input = new QAudioInput(device, format, this);
    m_device = m_input->start();
    if (!m_device) {
        m_lastError = QStringLiteral("录音启动失败");
        delete m_input;
        m_input = nullptr;
        return false;
    }

    connect(m_device, &QIODevice::readyRead, this, [this]() {
        const QByteArray chunk = m_device->readAll();
        m_pcm.append(chunk);

        // 电平：取本块 16bit 采样的峰值，归一化到 0~1
        qreal peak = 0.0;
        const int sampleCount = chunk.size() / 2;
        const qint16 *samples = reinterpret_cast<const qint16 *>(chunk.constData());
        for (int i = 0; i < sampleCount; ++i)
            peak = qMax(peak, qAbs(static_cast<qreal>(samples[i])) / 32768.0);
        emit levelChanged(peak);
    });

    m_recording = true;
    m_limitTimer->start(kMaxDurationMs);
    qInfo() << "[recorder] start" << m_sampleRate << "Hz" << m_channels << "ch";
    return true;
}

void Recorder::stop()
{
    stopInternal(true);
}

void Recorder::stopInternal(bool emitSignal)
{
    if (!m_recording)
        return;

    m_recording = false;
    m_limitTimer->stop();

    if (m_input) {
        m_input->stop();
        m_input->deleteLater();
        m_input = nullptr;
    }
    m_device = nullptr;

    const int bytesPerSecond = m_sampleRate * m_channels * 2;
    const int durationMs = bytesPerSecond > 0 ? (m_pcm.size() * 1000 / bytesPerSecond) : 0;

    emit levelChanged(0.0);
    if (emitSignal)
        emit finished(makeWav(m_pcm, m_sampleRate, m_channels, 16), durationMs);

    qInfo() << "[recorder] stop, pcm bytes =" << m_pcm.size() << "duration =" << durationMs << "ms";
}

QByteArray Recorder::makeWav(const QByteArray &pcm, int sampleRate, int channels, int bitsPerSample)
{
    QByteArray wav;
    QDataStream out(&wav, QIODevice::WriteOnly);
    out.setByteOrder(QDataStream::LittleEndian);

    const quint32 dataSize = static_cast<quint32>(pcm.size());
    const quint32 byteRate = sampleRate * channels * bitsPerSample / 8;
    const quint16 blockAlign = static_cast<quint16>(channels * bitsPerSample / 8);

    out.writeRawData("RIFF", 4);
    out << static_cast<quint32>(36 + dataSize);
    out.writeRawData("WAVE", 4);
    out.writeRawData("fmt ", 4);
    out << static_cast<quint32>(16);
    out << static_cast<quint16>(1);                       // PCM
    out << static_cast<quint16>(channels);
    out << static_cast<quint32>(sampleRate);
    out << byteRate;
    out << blockAlign;
    out << static_cast<quint16>(bitsPerSample);
    out.writeRawData("data", 4);
    out << dataSize;
    out.writeRawData(pcm.constData(), pcm.size());
    return wav;
}