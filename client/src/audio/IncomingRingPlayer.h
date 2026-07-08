#pragma once

#include "settings/AppSettings.h"

#include <QObject>

class QAudioSink;
class QIODevice;
class QMediaPlayer;
class QAudioOutput;
class QTimer;

namespace itl {

class IncomingRingPlayer : public QObject {
    Q_OBJECT

public:
    static constexpr int sampleRate = 48000;

    explicit IncomingRingPlayer(QObject *parent = nullptr);
    ~IncomingRingPlayer() override;

    void applySettings(const AppSettings *settings);
    void start();
    void stop();
    bool isPlaying() const { return m_running; }

private:
    void startBuiltin();
    void startCustomFile();
    void tickBuiltin();
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
    AppSettings::RingtoneKind m_kind = AppSettings::RingtoneKind::BuiltinClassic;
};

} // namespace itl
