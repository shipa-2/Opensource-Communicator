#include "IncomingRingPlayer.h"

#include "AudioDeviceUtils.h"

#include <QtMath>

#include <QAudioDevice>
#include <QAudioFormat>
#include <QAudioOutput>
#include <QAudioSink>
#include <QLoggingCategory>
#include <QMediaPlayer>
#include <QTimer>
#include <QUrl>

Q_LOGGING_CATEGORY(lcIncomingRing, "itl.incoming")

namespace itl {

namespace {
constexpr int kFrameSamples = 960;
constexpr double kAmplitude = 7000.0;
} // namespace

IncomingRingPlayer::IncomingRingPlayer(QObject *parent)
    : QObject(parent)
{
}

IncomingRingPlayer::~IncomingRingPlayer()
{
  stop();
}

void IncomingRingPlayer::applySettings(const AppSettings *settings)
{
  m_settings = settings;
}

void IncomingRingPlayer::start()
{
  if (m_running) {
    return;
  }

  m_kind = m_settings ? m_settings->incomingRingKind() : AppSettings::RingtoneKind::BuiltinClassic;
  if (m_kind == AppSettings::RingtoneKind::CustomFile
      && m_settings && !m_settings->incomingRingCustomPath().isEmpty()) {
    startCustomFile();
    return;
  }

  startBuiltin();
}

void IncomingRingPlayer::stop()
{
  stopCustomFile();
  stopBuiltin();
  m_running = false;
}

void IncomingRingPlayer::startBuiltin()
{
  QAudioFormat format;
  format.setSampleRate(sampleRate);
  format.setChannelCount(2);
  format.setSampleFormat(QAudioFormat::Int16);

  const QString outputId = m_settings ? m_settings->outputDeviceId() : QString();
  const QAudioDevice outputDevice = AudioDeviceUtils::findOutputDevice(outputId);
  if (outputDevice.isNull()) {
    qCWarning(lcIncomingRing) << "No audio output device for incoming ring";
    return;
  }

  m_sink = new QAudioSink(outputDevice, format, this);
  m_output = m_sink->start();
  if (!m_output) {
    stopBuiltin();
    return;
  }

  m_phase = 0.0;
  m_ticksInPhase = 0;
  m_tonePhase = true;
  m_running = true;

  m_timer = new QTimer(this);
  connect(m_timer, &QTimer::timeout, this, &IncomingRingPlayer::tickBuiltin);
  m_timer->start(20);
}

void IncomingRingPlayer::tickBuiltin()
{
  if (!m_output) {
    return;
  }

  const bool shortTone = m_kind == AppSettings::RingtoneKind::BuiltinShort;
  const double hz = m_kind == AppSettings::RingtoneKind::BuiltinRussian ? 425.0 : 440.0;
  const int toneTicks = shortTone ? 15 : 25;
  const int pauseTicks = shortTone ? 15 : 25;

  qint16 buffer[kFrameSamples * 2];
  for (int i = 0; i < kFrameSamples; ++i) {
    const qint16 sample = m_tonePhase ? static_cast<qint16>(kAmplitude * qSin(m_phase)) : qint16(0);
    if (m_tonePhase) {
      m_phase += 2.0 * M_PI * hz / sampleRate;
    }
    buffer[i * 2] = sample;
    buffer[i * 2 + 1] = sample;
  }
  m_output->write(reinterpret_cast<const char *>(buffer), sizeof(buffer));

  ++m_ticksInPhase;
  if (m_tonePhase && m_ticksInPhase >= toneTicks) {
    m_tonePhase = false;
    m_ticksInPhase = 0;
  } else if (!m_tonePhase && m_ticksInPhase >= pauseTicks) {
    m_tonePhase = true;
    m_ticksInPhase = 0;
  }
}

void IncomingRingPlayer::stopBuiltin()
{
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
}

void IncomingRingPlayer::startCustomFile()
{
  const QString path = m_settings->incomingRingCustomPath();
  m_player = new QMediaPlayer(this);
  m_audioOutput = new QAudioOutput(this);
  const QString outputId = m_settings->outputDeviceId();
  if (!outputId.isEmpty()) {
    m_audioOutput->setDevice(AudioDeviceUtils::findOutputDevice(outputId));
  }
  m_player->setAudioOutput(m_audioOutput);
  m_player->setSource(QUrl::fromLocalFile(path));
  m_player->setLoops(QMediaPlayer::Infinite);
  connect(m_player, &QMediaPlayer::playbackStateChanged, this, [this](QMediaPlayer::PlaybackState state) {
    m_running = state == QMediaPlayer::PlayingState;
  });
  m_player->play();
  m_running = true;
}

void IncomingRingPlayer::stopCustomFile()
{
  if (m_player) {
    m_player->stop();
    m_player->deleteLater();
    m_player = nullptr;
  }
  if (m_audioOutput) {
    m_audioOutput->deleteLater();
    m_audioOutput = nullptr;
  }
}

} // namespace itl
