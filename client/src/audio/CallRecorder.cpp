#include "CallRecorder.h"

#include "AudioBridge.h"
#include "settings/AppSettings.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLoggingCategory>
#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>

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
    if (QFile::exists(path + QStringLiteral(".wav")) || QFile::exists(path + QStringLiteral(".mp3"))) {
      return true;
    }
    if (dualTrack) {
      return QFile::exists(path + QStringLiteral("_manager.wav"))
             || QFile::exists(path + QStringLiteral("_caller.wav"))
             || QFile::exists(path + QStringLiteral("_manager.mp3"))
             || QFile::exists(path + QStringLiteral("_caller.mp3"));
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

CallRecorder::~CallRecorder()
{
  if (m_convertProcess) {
    m_convertProcess->kill();
    m_convertProcess->waitForFinished(1000);
    delete m_convertProcess;
    m_convertProcess = nullptr;
  }
  closeWriters();
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

void CallRecorder::enqueueAndMix(bool localSide, const QByteArray &pcm)
{
  if (localSide) {
    m_localQueue.append(pcm);
  } else {
    m_remoteQueue.append(pcm);
  }

  // One mixed frame per local+remote pair — writing on every side independently
  // made the mix ~2x longer and play at half speed.
  while (!m_localQueue.isEmpty() && !m_remoteQueue.isEmpty()) {
    const QByteArray local = m_localQueue.takeFirst();
    const QByteArray remote = m_remoteQueue.takeFirst();
    flushMixedFrame(local, remote);
  }
}

void CallRecorder::drainMixedQueues(bool padWithSilence)
{
  if (!m_mixedWriter) {
    m_localQueue.clear();
    m_remoteQueue.clear();
    return;
  }

  while (!m_localQueue.isEmpty() && !m_remoteQueue.isEmpty()) {
    flushMixedFrame(m_localQueue.takeFirst(), m_remoteQueue.takeFirst());
  }

  if (!padWithSilence) {
    m_localQueue.clear();
    m_remoteQueue.clear();
    return;
  }

  while (!m_localQueue.isEmpty()) {
    flushMixedFrame(m_localQueue.takeFirst(), QByteArray());
  }
  while (!m_remoteQueue.isEmpty()) {
    flushMixedFrame(QByteArray(), m_remoteQueue.takeFirst());
  }
}

bool CallRecorder::start(const AppSettings *settings, const QString &contactName)
{
  if (m_convertProcess) {
    m_convertProcess->kill();
    m_convertProcess->waitForFinished(2000);
    m_convertProcess->deleteLater();
    m_convertProcess = nullptr;
    m_pendingWavPaths.clear();
    m_convertIndex = 0;
    m_pendingMixedPath.clear();
  }

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
  m_localQueue.clear();
  m_remoteQueue.clear();

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

void CallRecorder::closeWriters()
{
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
}

QString CallRecorder::findEncoder()
{
  const QString ffmpeg = QStandardPaths::findExecutable(QStringLiteral("ffmpeg"));
  if (!ffmpeg.isEmpty()) {
    return ffmpeg;
  }
  return QStandardPaths::findExecutable(QStringLiteral("lame"));
}

void CallRecorder::beginMp3Conversion(const QStringList &wavPaths)
{
  m_pendingWavPaths = wavPaths;
  m_convertIndex = 0;
  convertNext();
}

void CallRecorder::convertNext()
{
  if (m_convertIndex >= m_pendingWavPaths.size()) {
    finishConversion(true, QString());
    return;
  }

  const QString encoder = findEncoder();
  if (encoder.isEmpty()) {
    finishConversion(false, QStringLiteral("ffmpeg/lame not found; keeping WAV"));
    return;
  }

  const QString wavPath = m_pendingWavPaths.at(m_convertIndex);
  QFileInfo wavInfo(wavPath);
  const QString mp3Path =
      wavInfo.absolutePath() + QLatin1Char('/') + wavInfo.completeBaseName() + QStringLiteral(".mp3");

  if (m_convertProcess) {
    m_convertProcess->deleteLater();
    m_convertProcess = nullptr;
  }

  m_convertProcess = new QProcess(this);
  QStringList args;
  if (QFileInfo(encoder).fileName().startsWith(QStringLiteral("ffmpeg"), Qt::CaseInsensitive)) {
    args = QStringList{
        QStringLiteral("-y"),
        QStringLiteral("-i"),
        wavPath,
        QStringLiteral("-codec:a"),
        QStringLiteral("libmp3lame"),
        QStringLiteral("-qscale:a"),
        QStringLiteral("2"),
        mp3Path,
    };
  } else {
    args = QStringList{
        QStringLiteral("-V2"),
        wavPath,
        mp3Path,
    };
  }

  connect(m_convertProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
          [this, wavPath, mp3Path, encoder](int exitCode, QProcess::ExitStatus status) {
            const QByteArray err = m_convertProcess ? m_convertProcess->readAllStandardError() : QByteArray();
            if (status != QProcess::NormalExit || exitCode != 0 || !QFile::exists(mp3Path)) {
              qCWarning(lcRecord) << "MP3 encode failed with" << encoder << "exit" << exitCode << err;
              finishConversion(false, QStringLiteral("encode failed; keeping WAV"));
              return;
            }

            if (!QFile::remove(wavPath)) {
              qCWarning(lcRecord) << "Encoded MP3 but failed to remove WAV" << wavPath;
            }

            if (wavPath == m_pendingMixedPath || QFileInfo(wavPath).fileName() == QFileInfo(m_pendingMixedPath).fileName()) {
              m_lastSavedPath = mp3Path;
            }

            ++m_convertIndex;
            convertNext();
          });

  qCInfo(lcRecord) << "Encoding to MP3:" << wavPath << "->" << mp3Path << "via" << encoder;
  m_convertProcess->start(encoder, args);
  if (!m_convertProcess->waitForStarted(3000)) {
    qCWarning(lcRecord) << "Failed to start encoder" << encoder;
    finishConversion(false, QStringLiteral("encoder start failed; keeping WAV"));
  }
}

void CallRecorder::finishConversion(bool success, const QString &message)
{
  if (!message.isEmpty()) {
    if (success) {
      qCInfo(lcRecord) << message;
    } else {
      qCWarning(lcRecord) << message;
    }
  }

  if (m_convertProcess) {
    m_convertProcess->deleteLater();
    m_convertProcess = nullptr;
  }
  m_pendingWavPaths.clear();
  m_convertIndex = 0;

  const QString saved = m_lastSavedPath;
  m_pendingMixedPath.clear();
  if (!saved.isEmpty()) {
    qCInfo(lcRecord) << "Recording saved:" << saved;
    emit recordingFinished(saved);
  }
}

void CallRecorder::stop()
{
  if (m_convertProcess) {
    // A previous conversion is still running; leave it alone and just close writers if any.
  }

  if (!m_active && !m_managerWriter && !m_callerWriter && !m_mixedWriter) {
    return;
  }

  QStringList wavPaths;
  QString mixedPath;

  if (m_managerWriter) {
    auto *writer = static_cast<WavWriter *>(m_managerWriter);
    writer->close();
    wavPaths.append(writer->path());
    delete writer;
    m_managerWriter = nullptr;
  }
  if (m_callerWriter) {
    auto *writer = static_cast<WavWriter *>(m_callerWriter);
    writer->close();
    wavPaths.append(writer->path());
    delete writer;
    m_callerWriter = nullptr;
  }
  if (m_mixedWriter) {
    drainMixedQueues(true);
    auto *writer = static_cast<WavWriter *>(m_mixedWriter);
    writer->close();
    mixedPath = writer->path();
    wavPaths.prepend(mixedPath);
    delete writer;
    m_mixedWriter = nullptr;
  } else {
    m_localQueue.clear();
    m_remoteQueue.clear();
  }

  m_active = false;
  m_lastSavedPath = mixedPath;
  m_pendingMixedPath = mixedPath;

  if (wavPaths.isEmpty()) {
    m_localQueue.clear();
    m_remoteQueue.clear();
    return;
  }

  beginMp3Conversion(wavPaths);
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
    enqueueAndMix(true, pcm);
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
    enqueueAndMix(false, pcm);
  }
}

} // namespace itl
