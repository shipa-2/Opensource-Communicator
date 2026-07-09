#include "AudioBridge.h"

#include "AudioDeviceUtils.h"
#include "settings/AppSettings.h"

#include <opus/opus.h>

#include <QAudioDevice>
#include <QAudioFormat>
#include <QAudioSink>
#include <QAudioSource>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(lcAudio, "itl.audio")

namespace itl {

AudioBridge::AudioBridge(QObject *parent)
    : QObject(parent)
{
}

AudioBridge::~AudioBridge()
{
  stop();
  if (m_encoder) {
    opus_encoder_destroy(static_cast<OpusEncoder *>(m_encoder));
  }
  if (m_decoder) {
    opus_decoder_destroy(static_cast<OpusDecoder *>(m_decoder));
  }
}

void AudioBridge::applySettings(const AppSettings *settings)
{
  m_settings = settings;
  if (m_running) {
    stop();
    start();
  }
}

bool AudioBridge::start()
{
  if (m_running) {
    return true;
  }

  int err = 0;
  m_encoder = opus_encoder_create(sampleRate, channels, OPUS_APPLICATION_VOIP, &err);
  if (err != OPUS_OK) {
    emit errorOccurred(QStringLiteral("Opus encoder init failed: %1").arg(err));
    return false;
  }
  opus_encoder_ctl(static_cast<OpusEncoder *>(m_encoder), OPUS_SET_BITRATE(32000));
  opus_encoder_ctl(static_cast<OpusEncoder *>(m_encoder), OPUS_SET_SIGNAL(OPUS_SIGNAL_VOICE));

  m_decoder = opus_decoder_create(sampleRate, channels, &err);
  if (err != OPUS_OK) {
    emit errorOccurred(QStringLiteral("Opus decoder init failed: %1").arg(err));
    return false;
  }

  QAudioFormat inFormat;
  inFormat.setSampleRate(sampleRate);
  inFormat.setChannelCount(channels);
  inFormat.setSampleFormat(QAudioFormat::Int16);

  QAudioFormat outFormat;
  outFormat.setSampleRate(sampleRate);
  outFormat.setChannelCount(2);
  outFormat.setSampleFormat(QAudioFormat::Int16);

  const QString inputId = m_settings ? m_settings->inputDeviceId() : QString();
  const QString outputId = m_settings ? m_settings->outputDeviceId() : QString();
  const QAudioDevice inputDevice = AudioDeviceUtils::findInputDevice(inputId);
  const QAudioDevice outputDevice = AudioDeviceUtils::findOutputDevice(outputId);

  if (inputDevice.isNull() || outputDevice.isNull()) {
    emit errorOccurred(QStringLiteral("Audio device not found"));
    return false;
  }

  if (!inputDevice.isFormatSupported(inFormat)) {
    inFormat = inputDevice.preferredFormat();
    inFormat.setChannelCount(channels);
    inFormat.setSampleRate(sampleRate);
    if (inFormat.sampleFormat() != QAudioFormat::Int16) {
      inFormat.setSampleFormat(QAudioFormat::Int16);
    }
  }

  if (!outputDevice.isFormatSupported(outFormat)) {
    outFormat = outputDevice.preferredFormat();
    outFormat.setSampleRate(sampleRate);
    if (outFormat.sampleFormat() != QAudioFormat::Int16) {
      outFormat.setSampleFormat(QAudioFormat::Int16);
    }
    if (outFormat.channelCount() < 2) {
      outFormat.setChannelCount(2);
    }
  }

  m_source = new QAudioSource(inputDevice, inFormat, this);
  m_sink = new QAudioSink(outputDevice, outFormat, this);
  m_sink->setBufferSize(sampleRate * outFormat.channelCount() * static_cast<int>(sizeof(qint16)) / 10);

  m_input = m_source->start();
  m_output = m_sink->start();

  if (!m_input || !m_output) {
    emit errorOccurred(QStringLiteral("Failed to start audio IO"));
    stop();
    return false;
  }

  connect(m_input, &QIODevice::readyRead, this, &AudioBridge::onMicData);
  m_running = true;
  qCInfo(lcAudio) << "Audio bridge started";
  return true;
}

void AudioBridge::stop()
{
  if (!m_running) {
    return;
  }

  if (m_source) {
    m_source->stop();
    m_source->deleteLater();
    m_source = nullptr;
  }
  if (m_sink) {
    m_sink->stop();
    m_sink->deleteLater();
    m_sink = nullptr;
  }

  m_input = nullptr;
  m_output = nullptr;
  m_pcmBuffer.clear();
  m_running = false;
}

void AudioBridge::onMicData()
{
  if (!m_input) {
    return;
  }

  m_pcmBuffer.append(m_input->readAll());
  const int bytesPerFrame = frameSize * channels * static_cast<int>(sizeof(qint16));

  while (m_pcmBuffer.size() >= bytesPerFrame) {
    const QByteArray frame = m_pcmBuffer.left(bytesPerFrame);
    m_pcmBuffer.remove(0, bytesPerFrame);
    emit localPcmFrameReady(frame);
    const QByteArray opus = encodeOpusFrame(frame);
    if (!opus.isEmpty()) {
      emit opusFrameReady(opus);
    }
  }
}

QByteArray AudioBridge::encodeOpusFrame(const QByteArray &pcm)
{
  if (!m_encoder || pcm.size() < frameSize * static_cast<int>(sizeof(qint16))) {
    return {};
  }

  unsigned char out[4000];
  const int len = opus_encode(static_cast<OpusEncoder *>(m_encoder),
                              reinterpret_cast<const opus_int16 *>(pcm.constData()),
                              frameSize, out, sizeof(out));
  if (len < 0) {
    return {};
  }
  return QByteArray(reinterpret_cast<char *>(out), len);
}

void AudioBridge::decodeAndPlayOpus(const QByteArray &opus)
{
  if (!m_decoder || !m_output || opus.isEmpty()) {
    return;
  }

  opus_int16 pcm[frameSize * channels];
  const int samples = opus_decode(static_cast<OpusDecoder *>(m_decoder),
                                reinterpret_cast<const unsigned char *>(opus.constData()),
                                opus.size(), pcm, frameSize, 0);
  if (samples <= 0) {
    return;
  }

  const QByteArray pcmBytes(reinterpret_cast<const char *>(pcm),
                            samples * channels * static_cast<int>(sizeof(qint16)));
  emit remotePcmFrameReady(pcmBytes);

  float sum = 0;
  for (int i = 0; i < samples; ++i) {
    sum += static_cast<float>(qAbs(pcm[i]));
  }
  const float avgLevel = sum / samples / 32768.0f;
  emit remoteAudioLevel(avgLevel);

  qint16 stereo[frameSize * 2];
  for (int i = 0; i < samples; ++i) {
    stereo[i * 2] = pcm[i];
    stereo[i * 2 + 1] = pcm[i];
  }

  QAudioFormat outFormat = m_sink->format();
  const int outChannels = outFormat.channelCount();
  if (outChannels >= 2) {
    m_output->write(reinterpret_cast<const char *>(stereo), samples * 2 * static_cast<int>(sizeof(qint16)));
  } else {
    m_output->write(reinterpret_cast<const char *>(pcm), samples * channels * static_cast<int>(sizeof(qint16)));
  }
}

} // namespace itl
