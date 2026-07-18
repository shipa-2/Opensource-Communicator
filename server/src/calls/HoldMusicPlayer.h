#pragma once

#include <QByteArray>
#include <QObject>
#include <QString>
#include <QVector>

namespace itl {

class HoldMusicPlayer : public QObject {
    Q_OBJECT

public:
    explicit HoldMusicPlayer(QObject *parent = nullptr);

    bool loadWav(const QString &path);
    bool loadFromResource(const QString &resourcePath);

    // Read next chunk of PCM samples (16-bit mono)
    // Returns empty when loop restarts
    QByteArray readChunk(int sampleCount);

    bool isLoaded() const { return !m_samples.isEmpty(); }
    int sampleRate() const { return m_sampleRate; }
    int totalSamples() const { return m_samples.size(); }

    // Reset playback position to start
    void reset() { m_position = 0; }

private:
    QVector<int16_t> m_samples;
    int m_sampleRate = 8000;
    int m_position = 0;
};

} // namespace itl
