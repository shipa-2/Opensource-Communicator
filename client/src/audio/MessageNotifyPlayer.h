#pragma once

#include "settings/AppSettings.h"

#include <QObject>

class QAudioSink;
class QIODevice;
class QTimer;

namespace itl {

class MessageNotifyPlayer : public QObject {
    Q_OBJECT

public:
    static constexpr int sampleRate = 48000;

    explicit MessageNotifyPlayer(QObject *parent = nullptr);
    ~MessageNotifyPlayer() override;

    void applySettings(const AppSettings *settings);
    void play();

private:
    void stop();
    void tick();

    const AppSettings *m_settings = nullptr;
    QAudioSink *m_sink = nullptr;
    QIODevice *m_output = nullptr;
    QTimer *m_timer = nullptr;
    double m_phase = 0.0;
    int m_ticksLeft = 0;
};

} // namespace itl
