#include "MessageNotifyPlayer.h"

#include "AudioDeviceUtils.h"

#include <QtMath>

#include <QAudioDevice>
#include <QAudioFormat>
#include <QAudioSink>
#include <QLoggingCategory>
#include <QTimer>

Q_LOGGING_CATEGORY(lcMessageNotify, "itl.message")

namespace itl {

namespace {
constexpr int kFrameSamples = 480;
constexpr double kAmplitude = 4500.0;
constexpr double kFrequencyHz = 880.0;
constexpr int kDurationMs = 180;
} // namespace

MessageNotifyPlayer::MessageNotifyPlayer(QObject *parent)
    : QObject(parent)
{
}

MessageNotifyPlayer::~MessageNotifyPlayer()
{
  stop();
}

void MessageNotifyPlayer::applySettings(const AppSettings *settings)
{
  m_settings = settings;
}

void MessageNotifyPlayer::play()
{
  stop();

  QAudioFormat format;
  format.setSampleRate(sampleRate);
  format.setChannelCount(1);
  format.setSampleFormat(QAudioFormat::Int16);

  const QString outputId = m_settings ? m_settings->outputDeviceId() : QString();
  const QAudioDevice outputDevice = AudioDeviceUtils::findOutputDevice(outputId);
  if (outputDevice.isNull()) {
    qCWarning(lcMessageNotify) << "No audio output device for message notification";
    return;
  }

  m_sink = new QAudioSink(outputDevice, format, this);
  m_output = m_sink->start();
  if (!m_output) {
    stop();
    return;
  }

  m_phase = 0.0;
  m_ticksLeft = qMax(1, (kDurationMs * sampleRate) / (1000 * kFrameSamples));
  m_playing = true;

  m_timer = new QTimer(this);
  connect(m_timer, &QTimer::timeout, this, &MessageNotifyPlayer::tick);
  m_timer->start(1000 * kFrameSamples / sampleRate);
  tick();
}

void MessageNotifyPlayer::stop()
{
  m_playing = false;
  if (m_timer) {
    m_timer->stop();
    m_timer->deleteLater();
    m_timer = nullptr;
  }
  if (m_sink) {
    m_sink->stop();
    m_sink->deleteLater();
    m_sink = nullptr;
  }
  m_output = nullptr;
  m_ticksLeft = 0;
}

void MessageNotifyPlayer::tick()
{
  if (!m_playing || !m_sink || !m_output) {
    stop();
    return;
  }
  if (m_ticksLeft <= 0) {
    stop();
    return;
  }

  QByteArray buffer;
  buffer.resize(kFrameSamples * static_cast<int>(sizeof(qint16)));
  auto *samples = reinterpret_cast<qint16 *>(buffer.data());
  const double phaseStep = (2.0 * M_PI * kFrequencyHz) / sampleRate;
  for (int i = 0; i < kFrameSamples; ++i) {
    samples[i] = static_cast<qint16>(kAmplitude * qSin(m_phase));
    m_phase += phaseStep;
  }

  if (m_output->write(buffer) <= 0) {
    stop();
    return;
  }
  --m_ticksLeft;
  if (m_ticksLeft <= 0) {
    stop();
  }
}

} // namespace itl
