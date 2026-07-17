#include "AudioBridge.h"

#include "AudioDeviceUtils.h"
#include "settings/AppSettings.h"

#include <opus/opus.h>

#include <QAudioDevice>
#include <QAudioFormat>
#include <QAudioSink>
#include <QAudioSource>
#include <QLoggingCategory>
#include <QTimer>

#include <cmath>

Q_LOGGING_CATEGORY(lcAudio, "itl.audio")

namespace itl {

namespace {

constexpr int kDtmfDurationMs = 250;
constexpr double kPi = 3.14159265358979323846;

bool dtmfFrequencies(QChar digit, double &lowHz, double &highHz)
{
  const ushort ch = digit.unicode();
  const double low[] = {697.0, 770.0, 852.0, 941.0};
  const double high[] = {1209.0, 1336.0, 1477.0};

  int row = -1;
  int col = -1;
  switch (ch) {
  case '1':
    row = 0;
    col = 0;
    break;
  case '2':
    row = 0;
    col = 1;
    break;
  case '3':
    row = 0;
    col = 2;
    break;
  case '4':
    row = 1;
    col = 0;
    break;
  case '5':
    row = 1;
    col = 1;
    break;
  case '6':
    row = 1;
    col = 2;
    break;
  case '7':
    row = 2;
    col = 0;
    break;
  case '8':
    row = 2;
    col = 1;
    break;
  case '9':
    row = 2;
    col = 2;
    break;
  case '*':
    row = 3;
    col = 0;
    break;
  case '0':
    row = 3;
    col = 1;
    break;
  case '#':
    row = 3;
    col = 2;
    break;
  default:
    return false;
  }

  lowHz = low[row];
  highHz = high[col];
  return true;
}

} // namespace

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
  m_dtmfPcm.clear();
  m_dtmfQueue.clear();
  m_dtmfOffset = 0;
  if (m_dtmfPumpTimer) {
    m_dtmfPumpTimer->stop();
  }
  m_running = false;
}

bool AudioBridge::hasActiveDtmf() const
{
  return m_dtmfOffset < m_dtmfPcm.size() || !m_dtmfQueue.isEmpty();
}

QByteArray AudioBridge::generateDtmfPcm(QChar digit)
{
  double lowHz = 0;
  double highHz = 0;
  if (!dtmfFrequencies(digit, lowHz, highHz)) {
    return {};
  }

  const int samples = sampleRate * kDtmfDurationMs / 1000;
  QByteArray pcm(samples * static_cast<int>(sizeof(qint16)), Qt::Uninitialized);
  auto *out = reinterpret_cast<qint16 *>(pcm.data());
  constexpr double amplitude = 0.35 * 32767.0;

  for (int i = 0; i < samples; ++i) {
    const double t = static_cast<double>(i) / sampleRate;
    const double sample = amplitude * (std::sin(2.0 * kPi * lowHz * t) + std::sin(2.0 * kPi * highHz * t));
    out[i] = static_cast<qint16>(qBound(-32767, static_cast<int>(sample), 32767));
  }

  const int fadeSamples = sampleRate / 100; // 10 ms
  for (int i = 0; i < fadeSamples && i < samples; ++i) {
    const double gain = static_cast<double>(i) / fadeSamples;
    out[i] = static_cast<qint16>(out[i] * gain);
    out[samples - 1 - i] = static_cast<qint16>(out[samples - 1 - i] * gain);
  }

  return pcm;
}

void AudioBridge::startNextDtmfTone()
{
  if (m_dtmfQueue.isEmpty()) {
    m_dtmfPcm.clear();
    m_dtmfOffset = 0;
    return;
  }

  const QChar digit = m_dtmfQueue.at(0);
  m_dtmfQueue.remove(0, 1);
  m_dtmfPcm = generateDtmfPcm(digit);
  m_dtmfOffset = 0;
}

void AudioBridge::queueDtmfTone(QChar digit)
{
  if (!QStringLiteral("0123456789*#").contains(digit)) {
    return;
  }

  m_dtmfQueue.append(digit);
  if (m_dtmfOffset >= m_dtmfPcm.size()) {
    startNextDtmfTone();
  }

  if (!m_dtmfPumpTimer) {
    m_dtmfPumpTimer = new QTimer(this);
    m_dtmfPumpTimer->setInterval(20);
    connect(m_dtmfPumpTimer, &QTimer::timeout, this, &AudioBridge::processOutgoingFrames);
  }
  if (!m_dtmfPumpTimer->isActive()) {
    m_dtmfPumpTimer->start();
  }
  processOutgoingFrames();
}

void AudioBridge::playDtmf(QChar digit)
{
  if (!m_running) {
    return;
  }
  queueDtmfTone(digit);
}

void AudioBridge::mixDtmfIntoFrame(QByteArray &frame)
{
  if (m_dtmfOffset >= m_dtmfPcm.size()) {
    if (!m_dtmfQueue.isEmpty()) {
      startNextDtmfTone();
    } else {
      return;
    }
  }

  const int sampleCount = frameSize;
  auto *frameSamples = reinterpret_cast<qint16 *>(frame.data());
  const auto *dtmfSamples = reinterpret_cast<const qint16 *>(m_dtmfPcm.constData() + m_dtmfOffset);
  const int dtmfSamplesLeft = (m_dtmfPcm.size() - m_dtmfOffset) / static_cast<int>(sizeof(qint16));
  const int mixCount = qMin(sampleCount, dtmfSamplesLeft);
  QByteArray sidetone(mixCount * static_cast<int>(sizeof(qint16)), Qt::Uninitialized);
  auto *sidetoneSamples = reinterpret_cast<qint16 *>(sidetone.data());

  for (int i = 0; i < mixCount; ++i) {
    sidetoneSamples[i] = dtmfSamples[i];
    const int mixed = static_cast<int>(frameSamples[i]) + static_cast<int>(dtmfSamples[i]);
    frameSamples[i] = static_cast<qint16>(qBound(-32767, mixed, 32767));
  }

  m_dtmfOffset += mixCount * static_cast<int>(sizeof(qint16));
  if (mixCount > 0) {
    playLocalSidetone(sidetone);
  }
  if (m_dtmfOffset >= m_dtmfPcm.size() && !m_dtmfQueue.isEmpty()) {
    startNextDtmfTone();
  }
}

void AudioBridge::playLocalSidetone(const QByteArray &monoFrame)
{
  if (!m_output || !m_sink || monoFrame.isEmpty()) {
    return;
  }

  const auto *mono = reinterpret_cast<const qint16 *>(monoFrame.constData());
  const int samples = monoFrame.size() / static_cast<int>(sizeof(qint16));
  qint16 stereo[frameSize * 2];
  const int count = qMin(samples, frameSize);
  for (int i = 0; i < count; ++i) {
    stereo[i * 2] = mono[i];
    stereo[i * 2 + 1] = mono[i];
  }

  QAudioFormat outFormat = m_sink->format();
  if (outFormat.channelCount() >= 2) {
    m_output->write(reinterpret_cast<const char *>(stereo), count * 2 * static_cast<int>(sizeof(qint16)));
  } else {
    m_output->write(monoFrame.constData(), count * static_cast<int>(sizeof(qint16)));
  }
}

void AudioBridge::processOutgoingFrames()
{
  const int bytesPerFrame = frameSize * channels * static_cast<int>(sizeof(qint16));

  while (m_pcmBuffer.size() >= bytesPerFrame || hasActiveDtmf()) {
    QByteArray frame;
    if (m_pcmBuffer.size() >= bytesPerFrame) {
      frame = m_pcmBuffer.left(bytesPerFrame);
      m_pcmBuffer.remove(0, bytesPerFrame);
    } else {
      frame = QByteArray(bytesPerFrame, '\0');
    }

    const bool hadDtmf = hasActiveDtmf();
    if (hadDtmf) {
      mixDtmfIntoFrame(frame);
    }

    emit localPcmFrameReady(frame);
    const QByteArray opus = encodeOpusFrame(frame);
    if (!opus.isEmpty()) {
      emit opusFrameReady(opus);
    }

    if (m_pcmBuffer.size() < bytesPerFrame && !hasActiveDtmf()) {
      break;
    }
  }

  if (m_dtmfPumpTimer && !hasActiveDtmf() && m_pcmBuffer.size() < bytesPerFrame) {
    m_dtmfPumpTimer->stop();
  }
}

void AudioBridge::onMicData()
{
  if (!m_input) {
    return;
  }

  m_pcmBuffer.append(m_input->readAll());
  processOutgoingFrames();
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
    // qAbs(int16_t) asserts on INT16_MIN (-32768); promote to int first.
    sum += static_cast<float>(qAbs(static_cast<int>(pcm[i])));
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
