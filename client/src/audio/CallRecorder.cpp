#include "CallRecorder.h"

#include "AudioBridge.h"
#include "settings/AppSettings.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QLoggingCategory>
#include <QRegularExpression>

Q_LOGGING_CATEGORY(lcRecord, "itl.record")

namespace itl {

namespace {

constexpr quint16 kBitsPerSample = 16;

QString sanitizePathComponent(const QString &name)
{
  QString result = name.trimmed();
  result.replace(QRegularExpression(QStringLiteral("\\s+")), QStringLiteral("_"));
  const QString invalid = QStringLiteral("<>:\"/\\|?*");
  for (const QChar ch : invalid) {
    result.replace(ch, QLatin1Char('_'));
  }
  while (result.contains(QStringLiteral(".."))) {
    result.replace(QStringLiteral(".."), QStringLiteral("_"));
  }
  return result.isEmpty() ? QStringLiteral("call") : result;
}

QString uniqueBasePath(const QString &basePath, bool dualTrack)
{
  auto exists = [&](const QString &path) {
    if (QFile::exists(path + QStringLiteral(".wav"))) {
      return true;
    }
    if (dualTrack) {
      return QFile::exists(path + QStringLiteral("_manager.wav"))
             || QFile::exists(path + QStringLiteral("_caller.wav"));
    }
    return false;
  };

  if (!exists(basePath)) {
    return basePath;
  }
  for (int index = 2; index < 1000; ++index) {
    const QString candidate = basePath + QLatin1Char('_') + QString::number(index);
    if (!exists(candidate)) {
      return candidate;
    }
  }
  return basePath + QLatin1Char('_') + QString::number(QDateTime::currentMSecsSinceEpoch());
}

class WavWriter {
public:
    bool open(const QString &path, int sampleRate, int channels)
    {
        close();
        m_file.setFileName(path);
        if (!m_file.open(QIODevice::WriteOnly)) {
            return false;
        }
        m_sampleRate = sampleRate;
        m_channels = static_cast<quint16>(channels);
        m_dataBytes = 0;
        writeHeader(0);
        return true;
    }

    void write(const char *data, qint64 size)
    {
        if (!m_file.isOpen() || size <= 0) {
            return;
        }
        m_file.write(data, size);
        m_dataBytes += static_cast<quint32>(size);
    }

    void close()
    {
        if (!m_file.isOpen()) {
            return;
        }
        writeHeader(m_dataBytes);
        m_file.close();
    }

    QString path() const { return m_file.fileName(); }

private:
    void writeHeader(quint32 dataBytes)
    {
        const quint32 byteRate = static_cast<quint32>(m_sampleRate) * m_channels * kBitsPerSample / 8;
        const quint16 blockAlign = static_cast<quint16>(m_channels * kBitsPerSample / 8);
        const quint32 riffSize = 36 + dataBytes;

        m_file.seek(0);
        m_file.write("RIFF", 4);
        m_file.write(reinterpret_cast<const char *>(&riffSize), 4);
        m_file.write("WAVE", 4);
        m_file.write("fmt ", 4);
        const quint32 fmtSize = 16;
        m_file.write(reinterpret_cast<const char *>(&fmtSize), 4);
        const quint16 audioFormat = 1;
        m_file.write(reinterpret_cast<const char *>(&audioFormat), 2);
        m_file.write(reinterpret_cast<const char *>(&m_channels), 2);
        const quint32 sampleRate = static_cast<quint32>(m_sampleRate);
        m_file.write(reinterpret_cast<const char *>(&sampleRate), 4);
        m_file.write(reinterpret_cast<const char *>(&byteRate), 4);
        m_file.write(reinterpret_cast<const char *>(&blockAlign), 2);
        m_file.write(reinterpret_cast<const char *>(&kBitsPerSample), 2);
        m_file.write("data", 4);
        m_file.write(reinterpret_cast<const char *>(&dataBytes), 4);
        if (dataBytes > 0) {
            m_file.seek(44);
        }
    }

    QFile m_file;
    int m_sampleRate = AudioBridge::sampleRate;
    quint16 m_channels = 1;
    quint32 m_dataBytes = 0;
};

} // namespace

CallRecorder::CallRecorder(QObject *parent)
    : QObject(parent)
{
}

QByteArray CallRecorder::silenceFrame()
{
    return QByteArray(AudioBridge::frameSize * AudioBridge::channels * static_cast<int>(sizeof(qint16)), '\0');
}

void CallRecorder::flushMixedFrame(const QByteArray &local, const QByteArray &remote)
{
    if (!m_mixedWriter) {
        return;
    }

    const QByteArray localFrame = local.isEmpty() ? silenceFrame() : local;
    const QByteArray remoteFrame = remote.isEmpty() ? silenceFrame() : remote;
    const auto *localSamples = reinterpret_cast<const qint16 *>(localFrame.constData());
    const auto *remoteSamples = reinterpret_cast<const qint16 *>(remoteFrame.constData());
    const int sampleCount = qMin(localFrame.size(), remoteFrame.size()) / static_cast<int>(sizeof(qint16));

    QByteArray mixed(sampleCount * static_cast<int>(sizeof(qint16)), Qt::Uninitialized);
    auto *out = reinterpret_cast<qint16 *>(mixed.data());
    for (int i = 0; i < sampleCount; ++i) {
        const int sum = static_cast<int>(localSamples[i]) + static_cast<int>(remoteSamples[i]);
        const int halved = sum / 2;
        out[i] = static_cast<qint16>(qBound(-32768, halved, 32767));
    }
    static_cast<WavWriter *>(m_mixedWriter)->write(mixed.constData(), mixed.size());
}

bool CallRecorder::start(const AppSettings *settings, const QString &contactName)
{
    stop();
    if (!settings || !settings->recordingEnabled()) {
        return false;
    }

    QString directory = settings->recordingDirectory();
    if (directory.isEmpty()) {
        directory = AppSettings::defaultRecordingDirectory();
    }
    QDir().mkpath(directory);

    const QString baseName = sanitizePathComponent(
        AppSettings::expandRecordingFilenameTemplate(settings->recordingFilenameTemplate(), contactName));
    const QString basePath = uniqueBasePath(QDir(directory).filePath(baseName), settings->recordingDualTrack());

    m_dualTrack = settings->recordingDualTrack();

    auto *mixedWriter = new WavWriter;
    if (!mixedWriter->open(basePath + QStringLiteral(".wav"), AudioBridge::sampleRate, 1)) {
        qCWarning(lcRecord) << "Failed to open mixed recording file" << basePath;
        delete mixedWriter;
        stop();
        return false;
    }
    m_mixedWriter = mixedWriter;
    m_lastSavedPath = mixedWriter->path();

    if (m_dualTrack) {
        auto *managerWriter = new WavWriter;
        auto *callerWriter = new WavWriter;
        if (!managerWriter->open(basePath + QStringLiteral("_manager.wav"), AudioBridge::sampleRate, 1)
            || !callerWriter->open(basePath + QStringLiteral("_caller.wav"), AudioBridge::sampleRate, 1)) {
            qCWarning(lcRecord) << "Failed to open dual-track recording files in" << directory;
            delete managerWriter;
            delete callerWriter;
            stop();
            return false;
        }
        m_managerWriter = managerWriter;
        m_callerWriter = callerWriter;
    }

    m_active = true;
    qCInfo(lcRecord) << "Recording started:" << m_lastSavedPath;
    return true;
}

void CallRecorder::stop()
{
    if (!m_active && !m_managerWriter && !m_callerWriter && !m_mixedWriter) {
        return;
    }

    if (m_managerWriter) {
        auto *writer = static_cast<WavWriter *>(m_managerWriter);
        writer->close();
        delete writer;
        m_managerWriter = nullptr;
    }
    if (m_callerWriter) {
        auto *writer = static_cast<WavWriter *>(m_callerWriter);
        writer->close();
        delete writer;
        m_callerWriter = nullptr;
    }
    if (m_mixedWriter) {
        auto *writer = static_cast<WavWriter *>(m_mixedWriter);
        writer->close();
        delete writer;
        m_mixedWriter = nullptr;
    }

    const QString saved = m_lastSavedPath;
    m_active = false;

    if (!saved.isEmpty()) {
        qCInfo(lcRecord) << "Recording saved:" << saved;
        emit recordingFinished(saved);
    }
}

void CallRecorder::appendLocal(const QByteArray &pcm)
{
    if (!m_active || pcm.isEmpty()) {
        return;
    }

    if (m_managerWriter) {
        static_cast<WavWriter *>(m_managerWriter)->write(pcm.constData(), pcm.size());
    }
    if (m_mixedWriter) {
        flushMixedFrame(pcm, QByteArray());
    }
}

void CallRecorder::appendRemote(const QByteArray &pcm)
{
    if (!m_active || pcm.isEmpty()) {
        return;
    }

    if (m_callerWriter) {
        static_cast<WavWriter *>(m_callerWriter)->write(pcm.constData(), pcm.size());
    }
    if (m_mixedWriter) {
        flushMixedFrame(QByteArray(), pcm);
    }
}

} // namespace itl
