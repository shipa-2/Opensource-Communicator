#include "HoldMusicPlayer.h"

#include <QFile>
#include <QLoggingCategory>
#include <QDataStream>

Q_LOGGING_CATEGORY(lcHold, "server.holdmusic")

namespace itl {

HoldMusicPlayer::HoldMusicPlayer(QObject *parent)
    : QObject(parent)
{
}

bool HoldMusicPlayer::loadWav(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        qCWarning(lcHold) << "Cannot open hold music file:" << path;
        return false;
    }

    // Read WAV header
    char riffHeader[4];
    file.read(riffHeader, 4);
    if (QByteArray(riffHeader, 4) != "RIFF") {
        qCWarning(lcHold) << "Not a valid WAV file:" << path;
        return false;
    }

    file.read(4); // file size
    char waveHeader[4];
    file.read(waveHeader, 4);
    if (QByteArray(waveHeader, 4) != "WAVE") {
        qCWarning(lcHold) << "Not a WAV file:" << path;
        return false;
    }

    int16_t numChannels = 0;
    int32_t sampleRate = 0;
    int16_t bitsPerSample = 0;
    QByteArray audioData;

    // Parse chunks
    while (!file.atEnd()) {
        char chunkId[4];
        if (file.read(chunkId, 4) != 4) break;

        int32_t chunkSize;
        QDataStream ds(&file);
        ds.setByteOrder(QDataStream::LittleEndian);
        ds >> chunkSize;

        if (QByteArray(chunkId, 4) == "fmt ") {
            int16_t audioFormat;
            ds >> audioFormat >> numChannels >> sampleRate;
            file.read(4); // byteRate
            ds >> bitsPerSample; // blockAlign, bitsPerSample
            // Skip rest of fmt chunk
            file.read(chunkSize - 16);
        } else if (QByteArray(chunkId, 4) == "data") {
            audioData = file.read(chunkSize);
        } else {
            file.read(chunkSize);
        }
    }

    if (numChannels != 1 || bitsPerSample != 16) {
        qCWarning(lcHold) << "Unsupported WAV format:" << numChannels << "channels," << bitsPerSample << "bits";
        return false;
    }

    m_sampleRate = sampleRate;
    m_samples.resize(audioData.size() / sizeof(int16_t));
    memcpy(m_samples.data(), audioData.constData(), audioData.size());
    m_position = 0;

    qCInfo(lcHold) << "Loaded hold music:" << path
                   << m_samples.size() << "samples," << m_sampleRate << "Hz,"
                   << (m_samples.size() / m_sampleRate) << "seconds";
    return true;
}

bool HoldMusicPlayer::loadFromResource(const QString &resourcePath)
{
    QFile file(resourcePath);
    if (!file.exists()) {
        qCWarning(lcHold) << "Hold music resource not found:" << resourcePath;
        return false;
    }
    return loadWav(resourcePath);
}

QByteArray HoldMusicPlayer::readChunk(int sampleCount)
{
    if (m_samples.isEmpty()) {
        return {};
    }

    QByteArray result;
    result.reserve(sampleCount * sizeof(int16_t));

    for (int i = 0; i < sampleCount; ++i) {
        if (m_position >= m_samples.size()) {
            m_position = 0; // loop
        }
        int16_t sample = m_samples[m_position++];
        result.append(reinterpret_cast<const char *>(&sample), sizeof(int16_t));
    }

    return result;
}

} // namespace itl
