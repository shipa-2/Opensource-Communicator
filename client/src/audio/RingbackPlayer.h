#pragma once

#include <QByteArray>
#include <QObject>

class QAudioSink;
class QAudioSource;
class QIODevice;
class QMediaPlayer;
class QAudioOutput;
class QTimer;

namespace itl {

class AppSettings;

// Russian-style ringback: 425 Hz, 1 s tone / 4 s pause.
class RingbackPlayer : public QObject {
    Q_OBJECT

public:
    static constexpr int sampleRate = 48000;
    static constexpr double toneHz = 425.0;

    explicit RingbackPlayer(QObject *parent = nullptr);
    ~RingbackPlayer() override;

    void applySettings(const AppSettings *settings);
    void start();
    void stop();
    bool isPlaying() const { return m_running; }

private:
    void startBuiltin();
    void startCustomFile();
    void tick();
    void stopBuiltin();
    void stopCustomFile();

    const AppSettings *m_settings = nullptr;
    QAudioSink *m_sink = nullptr;
    QIODevice *m_output = nullptr;
    QTimer *m_timer = nullptr;
    QMediaPlayer *m_player = nullptr;
    QAudioOutput *m_audioOutput = nullptr;

    double m_phase = 0.0;
    int m_ticksInPhase = 0;
    bool m_tonePhase = true;
    bool m_running = false;
};

} // namespace itl
