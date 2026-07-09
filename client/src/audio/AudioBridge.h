#pragma once

#include <QByteArray>
#include <QObject>

class QAudioSink;
class QAudioSource;
class QIODevice;

namespace itl {

class AppSettings;

class AudioBridge : public QObject {
    Q_OBJECT

public:
    static constexpr int sampleRate = 48000;
    static constexpr int channels = 1;
    static constexpr int frameSize = 960; // 20ms @ 48kHz

    explicit AudioBridge(QObject *parent = nullptr);
    ~AudioBridge() override;

    void applySettings(const AppSettings *settings);
    bool start();
    void stop();

    bool isRunning() const { return m_running; }

    QByteArray encodeOpusFrame(const QByteArray &pcm);
    void decodeAndPlayOpus(const QByteArray &opus);

signals:
    void opusFrameReady(const QByteArray &opus);
    void localPcmFrameReady(const QByteArray &pcm);
    void remotePcmFrameReady(const QByteArray &pcm);
    void remoteAudioLevel(float level);
    void errorOccurred(const QString &message);

private:
    void onMicData();

    const AppSettings *m_settings = nullptr;
    QAudioSource *m_source = nullptr;
    QAudioSink *m_sink = nullptr;
    QIODevice *m_input = nullptr;
    QIODevice *m_output = nullptr;

    void *m_encoder = nullptr;
    void *m_decoder = nullptr;

    QByteArray m_pcmBuffer;
    bool m_running = false;
};

} // namespace itl
