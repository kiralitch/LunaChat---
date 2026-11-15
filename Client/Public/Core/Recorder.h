#pragma once

#include <QByteArray>
#include <QObject>
#include <QString>

class QAudioInput;
class QIODevice;
class QTimer;

// 录音：QAudioInput 采集 16kHz/16bit/单声道 PCM，停止时拼成 WAV
// 本机没有 Ogg/MP3 编码器（见 AudioProbe 探测结果），所以按预案直接产出 WAV
class Recorder : public QObject
{
    Q_OBJECT

public:
    explicit Recorder(QObject *parent = nullptr);
    ~Recorder() override;

    bool start();
    void stop();
    bool isRecording() const { return m_recording; }
    QString lastError() const { return m_lastError; }

signals:
    void levelChanged(qreal level);                      // 0.0 ~ 1.0，供电平条
    void finished(const QByteArray &wavData, int durationMs);

private:
    void stopInternal(bool emitSignal);
    static QByteArray makeWav(const QByteArray &pcm, int sampleRate, int channels, int bitsPerSample);

    QAudioInput *m_input = nullptr;
    QIODevice *m_device = nullptr;
    QByteArray m_pcm;
    QTimer *m_limitTimer = nullptr;
    int m_sampleRate = 16000;
    int m_channels = 1;
    bool m_recording = false;
    QString m_lastError;
};