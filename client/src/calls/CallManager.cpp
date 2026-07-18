#include "CallManager.h"

#include "network/NetworkUtils.h"
#include "settings/AppSettings.h"

#include <rtc/rtc.hpp>

#include <QAbstractSocket>
#include <QDateTime>
#include <QLoggingCategory>
#include <QNetworkInterface>
#include <QRegularExpression>
#include <QThread>

Q_LOGGING_CATEGORY(lcCall, "itl.call")

namespace itl {

namespace {
constexpr uint8_t kOpusPayload = 111;
constexpr uint8_t kH264Payload = 96;
constexpr int kVideoWidth = 640;
constexpr int kVideoHeight = 360;
constexpr int kVideoFps = 15;
constexpr uint32_t kVideoTimestampStep = 90000 / kVideoFps;
constexpr int kPublishFallbackMs = 500;

uint32_t randomSsrc()
{
  return static_cast<uint32_t>(QUuid::createUuid().data1);
}

} // namespace

CallManager::CallManager(WsApiClient *api, AppSettings *settings, QObject *parent)
    : QObject(parent)
    , m_api(api)
    , m_settings(settings)
{
  rtc::InitLogger(rtc::LogLevel::Warning);
  applySettings();

  connect(&m_audio, &AudioBridge::opusFrameReady, this, [this](const QByteArray &opus) {
    if (m_activeLeg.isEmpty() || !m_peers.contains(m_activeLeg)) {
      return;
    }
    if (m_calls.contains(m_activeLeg) && m_calls[m_activeLeg].onHold) {
      return;
    }
    PeerContext &ctx = m_peers[m_activeLeg];
    auto track = ctx.localAudioTrack;
    if (!track || !track->isOpen()) {
      return;
    }
    static int sendCount = 0;
    if ((sendCount++ % 50) == 0) {
      qCInfo(lcCall) << "Sending Opus frame" << opus.size() << "bytes on" << m_activeLeg
                     << "(#" << sendCount << ")";
    }
    // Opus @ 48 kHz: 20 ms frame = 960 timestamp units (not wall-clock ms).
    rtc::FrameInfo info(ctx.nextRtpTimestamp);
    ctx.nextRtpTimestamp += 960;
    track->sendFrame(reinterpret_cast<const rtc::byte *>(opus.constData()), static_cast<size_t>(opus.size()), info);
  });

  connect(&m_audio, &AudioBridge::localPcmFrameReady, this, [this](const QByteArray &pcm) {
    if (m_recorder.isActive()) {
      m_recorder.appendLocal(pcm);
    }
  });
  connect(&m_audio, &AudioBridge::remotePcmFrameReady, this, [this](const QByteArray &pcm) {
    if (m_recorder.isActive()) {
      m_recorder.appendRemote(pcm);
    }
  });
  connect(&m_audio, &AudioBridge::remoteAudioLevel, this, &CallManager::remoteAudioLevel);
  connect(&m_recorder, &CallRecorder::recordingFinished, this, &CallManager::callRecordingFinished);
  connect(&m_videoCapture, &VideoCapture::frameReady,
          this, &CallManager::sendCapturedVideoFrame);
  connect(&m_videoCapture, &VideoCapture::error, this, [this](const QString &message) {
    qCWarning(lcCall) << "Video capture failed:" << message;
    if (!m_videoCaptureLeg.isEmpty() && m_calls.contains(m_videoCaptureLeg)) {
      m_calls[m_videoCaptureLeg].sendVideo = false;
      emit videoSendingChanged(m_videoCaptureLeg, false);
    }
    stopVideoCapture();
  });
  connect(&m_screenCapture, &ScreenCapture::frameReady,
          this, &CallManager::sendCapturedVideoFrame);
  connect(&m_screenCapture, &ScreenCapture::error, this, [this](const QString &message) {
    qCWarning(lcCall) << "Screen capture failed:" << message;
    const QString leg = m_videoCaptureLeg;
    if (!leg.isEmpty() && m_calls.contains(leg)) {
      m_calls[leg].sendVideo = false;
    }
    stopVideoCapture();
    m_screenSharing = false;
    if (!leg.isEmpty()) {
      emit screenSharingChanged(leg, false);
    }
  });
}

CallManager::~CallManager()
{
  stopVideoCapture();
  m_videoDecoder.close();
  resumeExternalMedia();
  m_audio.stop();
}

void CallManager::applySettings()
{
  m_audio.applySettings(m_settings);
  m_ringback.applySettings(m_settings);
  m_incomingRing.applySettings(m_settings);
}

QString CallManager::createLegId() const
{
  return QStringLiteral("C3Out%1").arg(m_outgoingLegCounter++);
}

QString CallManager::modifySdpForHold(const QString &sdp, bool hold) const
{
  QString result = sdp;
  QRegularExpression re(QStringLiteral("a=sendrecv"));
  return hold ? result.replace(re, QStringLiteral("a=sendonly"))
              : result.replace(QRegularExpression(QStringLiteral("a=sendonly")), QStringLiteral("a=sendrecv"));
}

bool CallManager::isSdpMediaOnHold(const QString &sdp) const
{
  const QStringList lines = sdp.split(QStringLiteral("\r\n"), Qt::SkipEmptyParts);
  bool inAudio = false;
  for (const QString &line : lines) {
    if (line.startsWith(QStringLiteral("m="))) {
      inAudio = line.startsWith(QStringLiteral("m=audio"));
      continue;
    }
    if (!inAudio) {
      continue;
    }
    if (line == QStringLiteral("a=sendonly") || line == QStringLiteral("a=inactive")) {
      return true;
    }
    if (line == QStringLiteral("a=sendrecv") || line == QStringLiteral("a=recvonly")) {
      return false;
    }
  }
  return false;
}

void CallManager::updateHoldStateFromSdp(const QString &leg, const QString &sdp, bool remoteInitiated)
{
  if (!m_calls.contains(leg) || !remoteInitiated) {
    return;
  }

  CallSession &session = m_calls[leg];
  const bool hold = isSdpMediaOnHold(sdp);
  const CallPhase targetPhase = hold ? CallPhase::Hold : CallPhase::Connected;
  if (session.onHold == hold && session.phase == targetPhase) {
    return;
  }

  session.onHold = hold;
  session.phase = targetPhase;
  const QString detail = hold ? QStringLiteral("remote") : QString();
  emit callStateChanged(leg, hold ? QStringLiteral("hold") : QStringLiteral("resumed"), detail);
}

void CallManager::answerRemoteUpdate(const QString &leg)
{
  if (!m_calls.contains(leg) || !m_peers.contains(leg) || !m_peers[leg].pc) {
    return;
  }

  CallSession &session = m_calls[leg];
  if (!session.connected || session.pendingUpdateAnswer) {
    return;
  }

  session.pendingUpdateAnswer = true;
  try {
    m_peers[leg].pc->setLocalDescription(rtc::Description::Type::Answer);
  } catch (const std::exception &e) {
    session.pendingUpdateAnswer = false;
    qCWarning(lcCall) << "Failed to create update answer for" << leg << ":" << e.what();
  }
}

QString CallManager::bindIPv4() const
{
  if (!m_settings) {
    return NetworkUtils::ipv4ForBinding({});
  }
  return NetworkUtils::ipv4ForBinding(m_settings->networkInterfaceName());
}

QString CallManager::patchSdpLocalAddress(const QString &sdp) const
{
  const QString ip = bindIPv4();
  if (ip.isEmpty()) {
    return sdp;
  }

  QString result = sdp;
  static const QRegularExpression originRe(QStringLiteral("^o=(.* IN IP4 )\\S+"), QRegularExpression::MultilineOption);
  static const QRegularExpression connRe(QStringLiteral("^c=IN IP4 \\S+"), QRegularExpression::MultilineOption);
  result.replace(originRe, QStringLiteral("o=\\1%1").arg(ip));
  result.replace(connRe, QStringLiteral("c=IN IP4 %1").arg(ip));
  return result;
}

QString CallManager::extractAudioMid(const QString &sdp) const
{
  const QStringList lines = sdp.split(QStringLiteral("\r\n"), Qt::SkipEmptyParts);
  bool inAudio = false;
  for (const QString &line : lines) {
    if (line.startsWith(QStringLiteral("m="))) {
      inAudio = line.startsWith(QStringLiteral("m=audio"));
      continue;
    }
    if (inAudio && line.startsWith(QStringLiteral("a=mid:"))) {
      return line.mid(6).trimmed();
    }
  }
  // Megafon/ITooLabs offers often omit a=mid; libdatachannel then uses "0".
  return QStringLiteral("0");
}

QString CallManager::sanitizeLocalSdp(const QString &sdp, bool hasVideo) const
{
  // libdatachannel can emit a dual m=audio answer when the offer has no mid and we
  // add a local track with a different mid. Keep a single audio section and drop
  // remote rtcp attributes that leak into the answer.
  QStringList lines = sdp.split(QStringLiteral("\r\n"), Qt::KeepEmptyParts);
  if (lines.isEmpty()) {
    lines = sdp.split(QLatin1Char('\n'), Qt::KeepEmptyParts);
  }

  QList<QStringList> sessionLines;
  QList<QStringList> mediaSections;
  QStringList *current = nullptr;

  for (QString line : lines) {
    if (line.endsWith(QLatin1Char('\r'))) {
      line.chop(1);
    }
    if (line.startsWith(QStringLiteral("m="))) {
      mediaSections.append(QStringList{});
      current = &mediaSections.last();
      current->append(line);
      continue;
    }
    if (!current) {
      if (sessionLines.isEmpty()) {
        sessionLines.append(QStringList{});
      }
      sessionLines[0].append(line);
    } else {
      current->append(line);
    }
  }

  if (sessionLines.isEmpty()) {
    return sdp;
  }

  QStringList &session = sessionLines[0];
  QList<QStringList> audioSections;
  QList<QStringList> videoSections;
  for (const QStringList &section : mediaSections) {
    if (!section.isEmpty() && section.first().startsWith(QStringLiteral("m=audio"))) {
      audioSections.append(section);
    } else if (hasVideo && !section.isEmpty() && section.first().startsWith(QStringLiteral("m=video"))) {
      videoSections.append(section);
    }
  }

  if (audioSections.isEmpty()) {
    return sdp;
  }

  // Keep the first audio m-line (matches offer mid "0"). Merge SSRC/msid from a
  // duplicate Opus-only section if libdatachannel emitted one.
  QStringList media = audioSections.first();
  QStringList extraAttrs;
  for (int i = 1; i < audioSections.size(); ++i) {
    for (const QString &line : audioSections[i]) {
      if (line.startsWith(QStringLiteral("a=ssrc:")) || line.startsWith(QStringLiteral("a=msid:"))) {
        extraAttrs.append(line);
      }
    }
  }

  QString mid;
  for (const QString &line : media) {
    if (line.startsWith(QStringLiteral("a=mid:"))) {
      mid = line.mid(6).trimmed();
      break;
    }
  }
  if (mid.isEmpty()) {
    mid = QStringLiteral("0");
    media.insert(1, QStringLiteral("a=mid:%1").arg(mid));
  }

  // Force Opus-only answer. Megafon offers PCMA/PCMU first; if we echo that order
  // the gateway sends PCMA while we only encode/decode Opus → silent call, hold still works.
  QStringList cleanedMedia;
  for (const QString &line : media) {
    if (line.startsWith(QStringLiteral("m=audio"))) {
      // Keep port/proto, replace payload list with Opus only.
      const QStringList parts = line.split(QLatin1Char(' '));
      if (parts.size() >= 3) {
        cleanedMedia.append(QStringLiteral("%1 %2 %3 %4")
                                .arg(parts[0], parts[1], parts[2], QString::number(kOpusPayload)));
      } else {
        cleanedMedia.append(line);
      }
      continue;
    }
    if (line.startsWith(QStringLiteral("a=rtcp:")) && line.contains(QStringLiteral("IN IP4"))) {
      continue;
    }
    if (line.startsWith(QStringLiteral("a=rtpmap:")) || line.startsWith(QStringLiteral("a=fmtp:"))) {
      // Drop non-Opus codec lines.
      if (!line.contains(QStringLiteral(":%1 ").arg(kOpusPayload))
          && !line.startsWith(QStringLiteral("a=rtpmap:%1 ").arg(kOpusPayload))
          && !line.startsWith(QStringLiteral("a=fmtp:%1 ").arg(kOpusPayload))) {
        continue;
      }
    }
    if (line.startsWith(QStringLiteral("a=setup:actpass"))) {
      cleanedMedia.append(QStringLiteral("a=setup:active"));
      continue;
    }
    cleanedMedia.append(line);
  }
  for (const QString &line : extraAttrs) {
    if (!cleanedMedia.contains(line)) {
      cleanedMedia.append(line);
    }
  }
  // Ensure Opus rtpmap/fmtp exist even if the chosen section was incomplete.
  const QString opusMap = QStringLiteral("a=rtpmap:%1 opus/48000/2").arg(kOpusPayload);
  const QString opusFmtp = QStringLiteral("a=fmtp:%1 minptime=10;useinbandfec=1").arg(kOpusPayload);
  if (!cleanedMedia.contains(opusMap)) {
    cleanedMedia.append(opusMap);
  }
  bool hasOpusFmtp = false;
  for (const QString &line : cleanedMedia) {
    if (line.startsWith(QStringLiteral("a=fmtp:%1 ").arg(kOpusPayload))) {
      hasOpusFmtp = true;
      break;
    }
  }
  if (!hasOpusFmtp) {
    cleanedMedia.append(opusFmtp);
  }
  media = cleanedMedia;

  // Build BUNDLE/LS groups with the actual media mids generated by
  // libdatachannel. Hard-coding "1" breaks BUNDLE when the track mid is
  // "video" (and vice versa).
  QString videoMid;
  if (!videoSections.isEmpty()) {
    for (const QString &line : videoSections.first()) {
      if (line.startsWith(QStringLiteral("a=mid:"))) {
        videoMid = line.mid(6).trimmed();
        break;
      }
    }
    if (videoMid.isEmpty()) {
      videoMid = QStringLiteral("video");
    }
  }
  QStringList cleanedSession;
  for (const QString &line : session) {
    if (line.startsWith(QStringLiteral("a=group:BUNDLE")) || line.startsWith(QStringLiteral("a=group:LS"))) {
      const QString prefix = line.startsWith(QStringLiteral("a=group:BUNDLE"))
                                 ? QStringLiteral("a=group:BUNDLE")
                                 : QStringLiteral("a=group:LS");
      if (!videoSections.isEmpty()) {
        cleanedSession.append(QStringLiteral("%1 %2 %3").arg(prefix, mid, videoMid));
      } else {
        cleanedSession.append(QStringLiteral("%1 %2").arg(prefix, mid));
      }
      continue;
    }
    cleanedSession.append(line);
  }

  QStringList out = cleanedSession;
  out.append(media);

  // Append video section if present
  if (!videoSections.isEmpty()) {
    QStringList videoMedia = videoSections.first();
    // Ensure video mid is set
    bool hasVideoMid = false;
    for (const QString &line : videoMedia) {
      if (line.startsWith(QStringLiteral("a=mid:"))) {
        hasVideoMid = true;
        break;
      }
    }
    if (!hasVideoMid && !videoMid.isEmpty()) {
      videoMedia.insert(1, QStringLiteral("a=mid:%1").arg(videoMid));
    }
    // Force setup:active for video too
    for (QString &line : videoMedia) {
      if (line.startsWith(QStringLiteral("a=setup:actpass"))) {
        line = QStringLiteral("a=setup:active");
      }
    }
    out.append(videoMedia);
  }

  while (!out.isEmpty() && out.last().isEmpty()) {
    out.removeLast();
  }
  out.append(QString());
  return out.join(QStringLiteral("\r\n"));
}

void CallManager::onCallSessionStarted()
{
  if (m_calls.size() == 1) {
    pauseExternalMedia();
  }
}

void CallManager::onCallSessionEnded()
{
  if (m_calls.isEmpty()) {
    resumeExternalMedia();
  }
}

void CallManager::pauseExternalMedia()
{
  m_externalMedia.pause();
}

void CallManager::resumeExternalMedia()
{
  m_externalMedia.resume();
}

void CallManager::startAudio()
{
  if (!m_audio.isRunning()) {
    m_audio.start();
  }
  // Recording and conversation timer start on first remote audio, not on SIP connect.
}

void CallManager::stopAudio()
{
  stopCallRecording();
  m_remoteAudioStartedLeg.clear();
  if (m_audio.isRunning()) {
    m_audio.stop();
  }
}

bool CallManager::isAudibleOpusFrame(const QByteArray &opus)
{
  // Comfort-noise / DTX Opus frames are typically ~20-35 bytes. Real speech is larger.
  return opus.size() >= 40;
}

void CallManager::noteRemoteOpusFrame(const QString &leg, const QByteArray &opus)
{
  if (leg.isEmpty() || !m_calls.contains(leg) || !m_calls[leg].connected) {
    return;
  }
  if (m_remoteAudioStartedLeg == leg) {
    return;
  }
  if (!isAudibleOpusFrame(opus)) {
    return;
  }
  onFirstRemoteAudio(leg);
}

void CallManager::onFirstRemoteAudio(const QString &leg)
{
  if (leg.isEmpty() || m_remoteAudioStartedLeg == leg) {
    return;
  }
  m_remoteAudioStartedLeg = leg;
  qCInfo(lcCall) << "Remote audio started for" << leg;
  startCallRecording();
  emit remoteAudioStarted(leg);
}

QString CallManager::contactNameForLeg(const QString &leg) const
{
  if (m_recordingNames.contains(leg)) {
    return m_recordingNames.value(leg);
  }

  const CallSession session = m_calls.value(leg);
  if (!session.realName.isEmpty()) {
    return session.realName;
  }
  if (!session.peer.isEmpty()) {
    const QString peer = session.peer;
    if (!peer.contains(QLatin1Char('@'))) {
      return peer;
    }
    return peer.section(QLatin1Char('@'), 0, 0);
  }
  return QStringLiteral("call");
}

void CallManager::setRecordingName(const QString &leg, const QString &name)
{
  if (leg.isEmpty() || name.trimmed().isEmpty()) {
    return;
  }
  m_recordingNames.insert(leg, name.trimmed());
}

void CallManager::startCallRecording()
{
  if (!m_settings || m_recorder.isActive() || m_activeLeg.isEmpty()) {
    return;
  }
  m_recorder.start(m_settings, contactNameForLeg(m_activeLeg));
}

void CallManager::stopCallRecording()
{
  if (m_recorder.isActive()) {
    m_recorder.stop();
  }
}

void CallManager::startRingback()
{
  m_ringback.start();
}

void CallManager::stopRingback()
{
  m_ringback.stop();
}

void CallManager::setupAudioTrack(const QString &leg, const std::shared_ptr<rtc::PeerConnection> &pc,
                                  const QString &audioMid)
{
  PeerContext &ctx = m_peers[leg];
  ctx.pc = pc;
  if (ctx.ssrc == 0) {
    ctx.ssrc = randomSsrc();
  }

  // Mid must match the remote offer's audio mid, otherwise libdatachannel emits a
  // second m=audio section and AcceptCall fails / media stays silent.
  const std::string mid = (audioMid.isEmpty() ? QStringLiteral("audio") : audioMid).toStdString();
  rtc::Description::Audio audio(mid, rtc::Description::Direction::SendRecv);
  audio.addOpusCodec(kOpusPayload);
  audio.addSSRC(ctx.ssrc, "audio", "stream", "audio");
  ctx.localAudioTrack = pc->addTrack(audio);

  ctx.rtpConfig = std::make_shared<rtc::RtpPacketizationConfig>(ctx.ssrc, "audio", kOpusPayload,
                                                                rtc::OpusRtpPacketizer::DefaultClockRate);
  ctx.nextRtpTimestamp = 0;
  auto handler = std::make_shared<rtc::OpusRtpPacketizer>(ctx.rtpConfig);
  handler->addToChain(std::make_shared<rtc::OpusRtpDepacketizer>());
  handler->addToChain(std::make_shared<rtc::RtcpReceivingSession>());
  handler->addToChain(std::make_shared<rtc::RtcpSrReporter>(ctx.rtpConfig));
  handler->addToChain(std::make_shared<rtc::RtcpNackResponder>());
  ctx.localAudioTrack->setMediaHandler(handler);

  ctx.localAudioTrack->onOpen([leg]() {
    qCInfo(lcCall) << "Local audio track open for" << leg;
  });

  ctx.localAudioTrack->onFrame([this, leg](rtc::binary data, rtc::FrameInfo) {
    if (data.empty()) {
      return;
    }
    QMetaObject::invokeMethod(this, [this, leg, payload = QByteArray(reinterpret_cast<const char *>(data.data()), static_cast<int>(data.size()))]() {
      if (!m_calls.contains(leg) || !m_calls[leg].connected) {
        return;
      }
      static int recvCount = 0;
      if ((recvCount++ % 50) == 0) {
        qCInfo(lcCall) << "Receiving Opus frame" << payload.size() << "bytes on" << leg
                       << "(#" << recvCount << ", via local track)";
      }
      noteRemoteOpusFrame(leg, payload);
      m_audio.decodeAndPlayOpus(payload);
    }, Qt::QueuedConnection);
  });
}

void CallManager::attachIncomingTrack(const QString &leg, const std::shared_ptr<rtc::Track> &track)
{
  if (!m_peers.contains(leg)) {
    qCWarning(lcCall) << "Remote track before peer context for" << leg;
    return;
  }

  PeerContext &ctx = m_peers[leg];
  ctx.remoteAudioTrack = track;

  auto depacketizer = std::make_shared<rtc::OpusRtpDepacketizer>();
  depacketizer->addToChain(std::make_shared<rtc::RtcpReceivingSession>());
  track->setMediaHandler(depacketizer);

  track->onOpen([leg]() {
    qCInfo(lcCall) << "Remote audio track open for" << leg;
  });

  track->onFrame([this, leg](rtc::binary data, rtc::FrameInfo) {
    QMetaObject::invokeMethod(this, [this, leg, payload = QByteArray(reinterpret_cast<const char *>(data.data()), static_cast<int>(data.size()))]() {
      if (!m_calls.contains(leg) || !m_calls[leg].connected) {
        return;
      }
      static int recvCount = 0;
      if ((recvCount++ % 50) == 0) {
        qCInfo(lcCall) << "Receiving Opus frame" << payload.size() << "bytes on" << leg
                       << "(#" << recvCount << ", via remote track)";
      }
      noteRemoteOpusFrame(leg, payload);
      m_audio.decodeAndPlayOpus(payload);
    }, Qt::QueuedConnection);
  });
}

void CallManager::beginIncomingIceGathering(const QString &leg)
{
  if (!m_calls.contains(leg) || !m_peers.contains(leg)) {
    return;
  }

  CallSession &session = m_calls[leg];
  PeerContext &ctx = m_peers[leg];
  if (!session.acceptPending || !ctx.pc) {
    return;
  }

  ctx.sdpSent = false;

  if (!ctx.iceGatheringStarted) {
    ctx.iceGatheringStarted = true;
    qCInfo(lcCall) << "Starting ICE gathering for accepted incoming" << leg;
    try {
      ctx.pc->gatherLocalCandidates();
    } catch (const std::exception &e) {
      qCWarning(lcCall) << "gatherLocalCandidates failed for" << leg << ":" << e.what();
    }
  }

  if (ctx.pc->gatheringState() == rtc::PeerConnection::GatheringState::Complete) {
    publishLocalSdp(leg, ctx.pc);
  } else {
    schedulePublishFallback(leg, ctx.pc);
  }
}

void CallManager::beginNegotiation(const QString &leg, bool createOffer, bool deferIceGathering)
{
  if (!m_calls.contains(leg) || m_peers.contains(leg)) {
    return;
  }

  const QString localIp = bindIPv4();

  rtc::Configuration config;
  config.enableIceUdpMux = true;
  config.forceMediaTransport = true;
  // We drive offer/answer explicitly. Leaving auto-negotiation on makes
  // setRemoteDescription(offer) create an answer immediately; our follow-up
  // setLocalDescription(Answer) then throws ("answer in signaling state stable"),
  // the peer is dropped, and AcceptCall goes out with orphan SDP → hold works, no audio.
  config.disableAutoNegotiation = true;
  if (!createOffer && deferIceGathering) {
    // Prepare answer SDP while ringing but do not probe the media gateway until AcceptCall.
    config.disableAutoGathering = true;
  }
  if (!localIp.isEmpty()) {
    config.bindAddress = localIp.toStdString();
    qCInfo(lcCall) << "WebRTC bind address" << localIp
                   << (m_settings && !m_settings->networkInterfaceName().isEmpty()
                           ? m_settings->networkInterfaceName()
                           : QStringLiteral("auto"));
  }

  auto pc = std::make_shared<rtc::PeerConnection>(config);

  pc->onLocalDescription([this, leg, pc](rtc::Description description) {
    QMetaObject::invokeMethod(this, [this, leg, pc, description]() {
      if (!m_calls.contains(leg)) {
        return;
      }
      CallSession &session = m_calls[leg];
      session.localSdp = sanitizeLocalSdp(patchSdpLocalAddress(QString::fromStdString(std::string(description))), session.videoCall);
      if (session.pendingUpdateAnswer) {
        session.pendingUpdateAnswer = false;
        qCInfo(lcCall) << "Sending AcceptUpdate for" << leg << "sdpBytes:" << session.localSdp.size();
        m_api->acceptUpdate(leg, session.localSdp);
        return;
      }
      if (!session.incoming || session.acceptPending) {
        publishLocalSdp(leg, pc);
      }
    }, Qt::QueuedConnection);
  });

  pc->onGatheringStateChange([this, leg, pc](rtc::PeerConnection::GatheringState state) {
    if (state != rtc::PeerConnection::GatheringState::Complete) {
      return;
    }
    QMetaObject::invokeMethod(this, [this, leg, pc]() {
      publishLocalSdp(leg, pc);
    }, Qt::QueuedConnection);
  });

  pc->onIceStateChange([leg](rtc::PeerConnection::IceState state) {
    qCInfo(lcCall) << "ICE state for" << leg << ":" << static_cast<int>(state);
  });

  pc->onTrack([this, leg](std::shared_ptr<rtc::Track> track) {
    if (!track) {
      return;
    }
    const QString mediaType = QString::fromStdString(track->description().type());
    qCInfo(lcCall) << "Remote track for" << leg << "type:" << mediaType;
    QMetaObject::invokeMethod(this, [this, leg, track, mediaType]() {
      if (mediaType == QStringLiteral("audio")) {
        attachIncomingTrack(leg, track);
      } else if (mediaType == QStringLiteral("video")) {
        attachIncomingVideoTrack(leg, track);
      }
    }, Qt::QueuedConnection);
  });

  pc->onStateChange([this, leg](rtc::PeerConnection::State state) {
    if (state != rtc::PeerConnection::State::Connected) {
      return;
    }
    QMetaObject::invokeMethod(this, [this, leg]() {
      if (!m_calls.contains(leg)) {
        return;
      }
      CallSession &session = m_calls[leg];
      if (session.connected) {
        m_activeLeg = leg;
        startAudio();
        emit callStateChanged(leg, QStringLiteral("connected"), session.peer);
      }
    }, Qt::QueuedConnection);
  });

  try {
    QString audioMid = QStringLiteral("audio");
    if (!createOffer) {
      const QString remote = m_calls[leg].remoteSdp;
      if (remote.isEmpty()) {
        throw std::runtime_error("incoming call has empty remote SDP");
      }
      audioMid = extractAudioMid(remote);
      qCInfo(lcCall) << "Incoming offer audio mid for" << leg << ":" << audioMid;
    }

    // Answerer: add local track with the offer mid FIRST, then setRemoteDescription.
    // Reverse order makes libdatachannel emit a second m=audio and AcceptCall breaks.
    setupAudioTrack(leg, pc, audioMid);

    // Setup video track if this is a video call or remote SDP has m=video
    if (m_calls[leg].videoCall || m_calls[leg].remoteSdp.contains(QStringLiteral("m=video"))) {
      m_calls[leg].videoCall = true;
      m_calls[leg].receiveVideo = true;
      const QString videoMid = createOffer ? QStringLiteral("video")
                                           : extractVideoMid(m_calls[leg].remoteSdp);
      setupVideoTrack(leg, pc, videoMid);
      qCInfo(lcCall) << "Video track setup for" << leg;
    }

    if (!createOffer) {
      pc->setRemoteDescription(
          rtc::Description(m_calls[leg].remoteSdp.toStdString(), rtc::Description::Type::Offer));
      m_peers[leg].remoteSdpApplied = true;
      m_peers[leg].appliedRemoteSdp = m_calls[leg].remoteSdp;
    }

    pc->setLocalDescription(createOffer ? rtc::Description::Type::Offer : rtc::Description::Type::Answer);
    m_calls[leg].phase = CallPhase::Negotiating;
    if (!deferIceGathering) {
      schedulePublishFallback(leg, pc);
      if (pc->gatheringState() == rtc::PeerConnection::GatheringState::Complete) {
        publishLocalSdp(leg, pc);
      }
    }
  } catch (const std::exception &e) {
    qCCritical(lcCall) << "WebRTC negotiation failed:" << e.what();
    // Keep the ringing session so the user can still reject; only drop the peer.
    // Clear any SDP produced by a half-finished negotiation so AcceptCall cannot
    // be sent without a live PeerConnection (silent call, hold still "works").
    if (m_peers.contains(leg)) {
      releasePeer(leg);
    }
    if (m_calls.contains(leg)) {
      m_calls[leg].localSdp.clear();
      m_calls[leg].acceptPending = false;
    }
    emit callStateChanged(leg, QStringLiteral("error"), QString::fromUtf8(e.what()));
    if (!m_calls.value(leg).incoming) {
      teardownCall(leg);
    }
  }
}

void CallManager::applyRemoteSdp(const QString &leg, const QString &sdp, SdpType type, bool force)
{
  if (!m_peers.contains(leg) || sdp.isEmpty() || !m_peers[leg].pc) {
    qCInfo(lcCall) << "applyRemoteSdp skipped for" << leg << "peerReady:" << m_peers.contains(leg)
                   << "sdpEmpty:" << sdp.isEmpty();
    return;
  }

  PeerContext &ctx = m_peers[leg];
  if (sdp == ctx.appliedRemoteSdp) {
    qCInfo(lcCall) << "Skipping unchanged remote SDP for" << leg;
    return;
  }

  if (ctx.remoteSdpApplied && !force) {
    qCInfo(lcCall) << "Remote SDP already applied for" << leg << "(no force)";
    return;
  }

  const auto rtcType = type == SdpType::Offer ? rtc::Description::Type::Offer : rtc::Description::Type::Answer;
  const char *typeLabel = type == SdpType::Offer ? "offer" : "answer";

  try {
    ctx.pc->setRemoteDescription(rtc::Description(sdp.toStdString(), rtcType));
    ctx.remoteSdpApplied = true;
    ctx.appliedRemoteSdp = sdp;
    qCInfo(lcCall) << "Applied remote SDP" << typeLabel << "for" << leg << "bytes:" << sdp.size();
  } catch (const std::exception &e) {
    qCWarning(lcCall) << "Failed to apply remote SDP" << typeLabel << "for" << leg << ":" << e.what();
  }
}

void CallManager::publishLocalSdp(const QString &leg, const std::shared_ptr<rtc::PeerConnection> &pc)
{
  if (!m_calls.contains(leg) || !pc) {
    return;
  }

  const auto desc = pc->localDescription();
  if (!desc) {
    return;
  }

  CallSession &session = m_calls[leg];
  session.localSdp = sanitizeLocalSdp(
      patchSdpLocalAddress(QString::fromStdString(std::string(*desc))), session.videoCall);
  qCInfo(lcCall) << "Local SDP ready for" << leg << "bytes:" << session.localSdp.size()
                 << "m-lines:" << session.localSdp.count(QStringLiteral("m=audio"));

  if (session.incoming && !session.acceptPending) {
    return;
  }

  sendLocalSdp(leg);
}

void CallManager::schedulePublishFallback(const QString &leg, const std::shared_ptr<rtc::PeerConnection> &pc)
{
  cancelPublishFallback(leg);

  auto *timer = new QTimer(this);
  timer->setSingleShot(true);
  m_publishTimers.insert(leg, timer);
  connect(timer, &QTimer::timeout, this, [this, leg, pc, timer]() {
    m_publishTimers.remove(leg);
    if (m_calls.contains(leg) && m_peers.contains(leg) && !m_peers[leg].sdpSent) {
      qCInfo(lcCall) << "Publishing local SDP via fallback timeout for" << leg;
      publishLocalSdp(leg, pc);
    }
    timer->deleteLater();
  });
  timer->start(kPublishFallbackMs);
}

void CallManager::cancelPublishFallback(const QString &leg)
{
  if (QTimer *timer = m_publishTimers.take(leg)) {
    timer->stop();
    timer->deleteLater();
  }
}

void CallManager::startIncomingRing()
{
  m_incomingRing.start();
}

void CallManager::stopIncomingRing()
{
  m_incomingRing.stop();
}

void CallManager::sendLocalSdp(const QString &leg)
{
  if (!m_calls.contains(leg)) {
    return;
  }

  CallSession &session = m_calls[leg];
  PeerContext &ctx = m_peers[leg];
  if (ctx.sdpSent || session.localSdp.isEmpty()) {
    return;
  }
  ctx.sdpSent = true;
  cancelPublishFallback(leg);

  if (session.incoming) {
    qCInfo(lcCall) << "Sending AcceptCall for" << leg << "sdpBytes:" << session.localSdp.size();
    m_api->acceptCall(leg, session.localSdp);
    emit callStateChanged(leg, QStringLiteral("accepting"), session.peer);
  } else if (session.phase == CallPhase::Negotiating) {
    m_api->startCall(leg, session.peer, session.localSdp, {}, session.conference);
    startRingback();
    emit callStateChanged(leg, QStringLiteral("dialing"), session.peer);
  }
}

QString CallManager::startOutgoingCall(const QString &peer, bool videoCall)
{
  hangupAll();

  const QString leg = createLegId();
  CallSession session;
  session.leg = leg;
  session.peer = peer;
  session.incoming = false;
  session.phase = CallPhase::Negotiating;
  session.videoCall = videoCall;
  session.sendVideo = videoCall;
  session.receiveVideo = videoCall;
  m_calls.insert(leg, session);
  onCallSessionStarted();

  if (videoCall && !startVideoCapture(leg)) {
    m_calls[leg].sendVideo = false;
  }
  beginNegotiation(leg, true);
  emit callStateChanged(leg, QStringLiteral("connecting"), peer);
  return leg;
}

QString CallManager::startConferenceCall(const QString &subject, const QList<ConferenceParticipant> &participants)
{
  if (participants.isEmpty()) {
    return {};
  }

  hangupAll();

  const QString confPeer = QStringLiteral("conf:%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
  QJsonArray partsArray;
  for (const ConferenceParticipant &participant : participants) {
    partsArray.append(QJsonObject{
        {QStringLiteral("peer"), participant.peer},
        {QStringLiteral("realName"), participant.name},
        {QStringLiteral("op"), participant.operatorPeer},
        {QStringLiteral("owner"), participant.owner},
        {QStringLiteral("muted"), participant.listener},
    });
  }

  const QString leg = createLegId();
  CallSession session;
  session.leg = leg;
  session.peer = confPeer;
  session.incoming = false;
  session.isConference = true;
  session.phase = CallPhase::Negotiating;
  session.conference = QJsonObject{
      {QStringLiteral("subject"), subject.isEmpty() ? QStringLiteral("Конференция") : subject},
      {QStringLiteral("description"), QString()},
      {QStringLiteral("participants"), partsArray},
  };
  m_calls.insert(leg, session);
  onCallSessionStarted();

  beginNegotiation(leg, true);
  emit callStateChanged(leg, QStringLiteral("connecting"), subject);
  return leg;
}

void CallManager::acceptIncomingCall(const QString &leg)
{
  if (!m_calls.contains(leg)) {
    qCWarning(lcCall) << "acceptIncomingCall: unknown leg" << leg;
    return;
  }

  stopIncomingRing();

  CallSession &session = m_calls[leg];
  session.acceptPending = true;
  if (session.videoCall) {
    session.sendVideo = startVideoCapture(leg);
  }
  qCInfo(lcCall) << "Accepting incoming" << leg << "localSdpReady:" << !session.localSdp.isEmpty()
                 << "peerReady:" << m_peers.contains(leg);

  if (m_peers.contains(leg)) {
    beginIncomingIceGathering(leg);
    return;
  }

  if (!session.localSdp.isEmpty()) {
    qCWarning(lcCall) << "Dropping orphan local SDP for" << leg << "; renegotiating";
    session.localSdp.clear();
  }

  beginNegotiation(leg, false);
}

void CallManager::rejectIncomingCall(const QString &leg)
{
  stopIncomingRing();
  m_api->rejectCall(leg, 486, QStringLiteral("Busy Here"));
  teardownCall(leg);
  emit callStateChanged(leg, QStringLiteral("rejected"), {});
}

void CallManager::hangup(const QString &leg)
{
  if (!m_calls.contains(leg)) {
    return;
  }

  const CallSession &session = m_calls[leg];
  if (session.incoming && session.phase == CallPhase::Ringing) {
    m_api->rejectCall(leg, 603, QStringLiteral("Decline"));
  } else if (!session.incoming && !session.connected) {
    m_api->cancelCall(leg, 487, QStringLiteral("Request Terminated"));
  } else {
    m_api->disconnectCall(leg, 0);
  }

  teardownCall(leg);
  emit callStateChanged(leg, QStringLiteral("ended"), {});
}

void CallManager::hangupAll()
{
  const QStringList legs = m_calls.keys();
  for (const QString &leg : legs) {
    if (!m_calls.contains(leg)) {
      continue;
    }
    const CallSession session = m_calls[leg];
    if (session.incoming && session.phase == CallPhase::Ringing) {
      m_api->rejectCall(leg, 603, QStringLiteral("Decline"));
    } else if (!session.incoming && !session.connected) {
      m_api->cancelCall(leg, 487, QStringLiteral("Request Terminated"));
    } else {
      m_api->disconnectCall(leg, 0);
    }
    teardownCall(leg);
    emit callStateChanged(leg, QStringLiteral("ended"), {});
  }
}

bool CallManager::hasActiveOutgoing() const
{
  for (auto it = m_calls.cbegin(); it != m_calls.cend(); ++it) {
    if (!it->incoming && it->phase != CallPhase::Ended && it->phase != CallPhase::Idle) {
      return true;
    }
  }
  return false;
}

QString CallManager::primaryOutgoingLeg() const
{
  for (auto it = m_calls.cbegin(); it != m_calls.cend(); ++it) {
    if (!it->incoming && it->phase != CallPhase::Ended && it->phase != CallPhase::Idle) {
      return it->leg;
    }
  }
  return {};
}

void CallManager::setHold(const QString &leg, bool hold)
{
  if (!m_calls.contains(leg)) {
    return;
  }

  CallSession &session = m_calls[leg];
  session.onHold = hold;
  session.phase = hold ? CallPhase::Hold : CallPhase::Connected;
  session.localSdp = modifySdpForHold(session.localSdp, hold);
  m_api->updateCall(leg, session.localSdp);
  emit callStateChanged(leg, hold ? QStringLiteral("hold") : QStringLiteral("resumed"), QStringLiteral("local"));
}

void CallManager::sendDtmf(const QString &leg, QChar digit)
{
  if (leg.isEmpty() || !m_calls.contains(leg) || !m_calls[leg].connected) {
    return;
  }
  if (!QStringLiteral("0123456789*#").contains(digit)) {
    return;
  }
  qCInfo(lcCall) << "Sending DTMF" << digit << "on" << leg;
  m_audio.playDtmf(digit);
}

void CallManager::blindTransfer(const QString &leg, const QString &targetPeer)
{
  if (!m_calls.contains(leg)) {
    return;
  }
  m_api->blindTransfer(leg, targetPeer);
  // Blind transfer leaves this client; don't wait for server "terminated"
  // (it may be delayed or missing) — drop media and close the session now.
  teardownCall(leg);
  emit callStateChanged(leg, QStringLiteral("transferred"), targetPeer);
}

CallSession *CallManager::call(const QString &leg)
{
  return m_calls.contains(leg) ? &m_calls[leg] : nullptr;
}

void CallManager::maybePreNegotiateIncoming(const QString &leg)
{
  if (!m_calls.contains(leg)) {
    return;
  }
  const CallSession &session = m_calls[leg];
  if (!session.incoming || session.connected || session.remoteSdp.isEmpty() || m_peers.contains(leg)) {
    return;
  }
  qCInfo(lcCall) << "Pre-negotiating incoming call while ringing:" << leg;
  beginNegotiation(leg, false, true);
}

void CallManager::releasePeer(const QString &leg)
{
  if (!m_peers.contains(leg)) {
    return;
  }
  PeerContext ctx = m_peers.take(leg);
  if (ctx.pc) {
    ctx.pc->resetCallbacks();
    ctx.pc->close();
  }
  if (m_videoCaptureLeg == leg) {
    stopVideoCapture();
  }
  m_screenSharing = false;
  m_videoBlur = false;
  m_videoDecoder.close();
}

void CallManager::teardownCall(const QString &leg)
{
  const bool incoming = m_calls.value(leg).incoming;
  cancelPublishFallback(leg);
  releasePeer(leg);
  m_calls.remove(leg);
  m_recordingNames.remove(leg);
  if (m_remoteAudioStartedLeg == leg) {
    m_remoteAudioStartedLeg.clear();
  }
  if (m_activeLeg == leg) {
    m_activeLeg.clear();
    stopAudio();
  }
  if (incoming) {
    if (!hasActiveOutgoing()) {
      stopIncomingRing();
    }
  } else {
    stopRingback();
  }
  onCallSessionEnded();
}

void CallManager::handleServerCallEvent(const QString &leg, const QString &what, const QJsonObject &payload)
{
  if (what == QStringLiteral("incomingCall")) {
    if (hasActiveOutgoing()) {
      qCInfo(lcCall) << "Rejecting incoming" << leg << "while outgoing call is active";
      m_api->rejectCall(leg, 486, QStringLiteral("Busy Here"));
      return;
    }

    CallSession session;
    session.leg = leg;
    session.incoming = true;
    session.phase = CallPhase::Ringing;
    session.remoteSdp = payload.value(QStringLiteral("sdp")).toString();
    session.videoCall = session.remoteSdp.contains(QStringLiteral("m=video"));

    const QJsonObject dest = payload.value(QStringLiteral("dest")).toObject();
    session.peer = dest.value(QStringLiteral("From")).toObject().value(QString::fromUtf8(kEmptyKey)).toString();
    if (session.peer.isEmpty()) {
      session.peer = dest.value(QStringLiteral("From")).toObject().value(QString::fromUtf8(kEmptyKey)).toObject().value(QString::fromUtf8(kEmptyKey)).toString();
    }
    session.realName = dest.value(QStringLiteral("From")).toObject().value(QStringLiteral("@realName")).toString();

    m_calls.insert(leg, session);
    onCallSessionStarted();
    qCInfo(lcCall) << "Incoming call" << leg << "from"
                   << (session.realName.isEmpty() ? session.peer : session.realName)
                   << "offerSdpBytes:" << session.remoteSdp.size();
    m_api->provisionCall(leg);
    // Incoming SDP arrives with incomingCall; PBX acks ProvisionCall with a bare
    // "response", not "provisioned". Start WebRTC prep once that round-trip settles.
    QTimer::singleShot(100, this, [this, leg]() { maybePreNegotiateIncoming(leg); });
    startIncomingRing();
    emit callStateChanged(leg, QStringLiteral("incoming"),
                           session.realName.isEmpty() ? session.peer : session.realName);
    return;
  }

  if (what == QStringLiteral("provisioned")) {
    if (!m_calls.contains(leg)) {
      return;
    }
    CallSession &session = m_calls[leg];
    if (!payload.value(QStringLiteral("sdp")).toString().isEmpty()) {
      session.remoteSdp = payload.value(QStringLiteral("sdp")).toString();
    }
    if (session.incoming) {
      maybePreNegotiateIncoming(leg);
    } else if (!session.remoteSdp.isEmpty()) {
      applyRemoteSdp(leg, session.remoteSdp, SdpType::Answer);
      session.phase = CallPhase::Ringing;
      emit callStateChanged(leg, QStringLiteral("ringing"), session.peer);
    }
    return;
  }

  if (!m_calls.contains(leg)) {
    return;
  }

  CallSession &session = m_calls[leg];

  if (what == QStringLiteral("accepted")) {
    session.connected = true;
    session.phase = CallPhase::Connected;
    session.remoteSdp = payload.value(QStringLiteral("sdp")).toString();
    stopRingback();
    qCInfo(lcCall) << "Call accepted (outgoing) for" << leg;
    applyRemoteSdp(leg, session.remoteSdp, SdpType::Answer);
    if (!session.incoming) {
      m_api->ackAccept(leg);
    }
    m_activeLeg = leg;
    startAudio();
    emit callStateChanged(leg, QStringLiteral("connected"), session.peer);
    return;
  }

  if (what == QStringLiteral("acceptAcked")) {
    session.connected = true;
    session.phase = CallPhase::Connected;
    stopRingback();
    qCInfo(lcCall) << "Call acceptAcked (incoming) for" << leg;
    m_activeLeg = leg;
    startAudio();
    emit callStateChanged(leg, QStringLiteral("connected"), session.peer);
    return;
  }

  if (what == QStringLiteral("updated") || what == QStringLiteral("updateAccepted")) {
    const QString newSdp = payload.value(QStringLiteral("sdp")).toString();
    if (newSdp.isEmpty()) {
      return;
    }
    session.remoteSdp = newSdp;

    // Mid-ring offer refresh from PBX: keep stored SDP but don't re-apply while answering.
    if (session.incoming && !session.connected) {
      if (m_peers.contains(leg) && m_peers[leg].remoteSdpApplied) {
        qCInfo(lcCall) << "Ignoring mid-ring SDP update for" << leg;
        return;
      }
      if (!m_peers.contains(leg)) {
        qCInfo(lcCall) << "Starting negotiation after mid-ring SDP update for" << leg;
        beginNegotiation(leg, false, true);
      }
      return;
    }

    const SdpType sdpType =
        what == QStringLiteral("updateAccepted") ? SdpType::Answer : SdpType::Offer;
    const bool remoteInitiated = what == QStringLiteral("updated");
    qCInfo(lcCall) << "Call" << what << "for" << leg << "sdpBytes:" << newSdp.size()
                   << "hold:" << isSdpMediaOnHold(newSdp);
    updateHoldStateFromSdp(leg, newSdp, remoteInitiated);
    applyRemoteSdp(leg, newSdp, sdpType, true);
    if (sdpType == SdpType::Offer && session.connected) {
      answerRemoteUpdate(leg);
    }
    return;
  }

  if (what == QStringLiteral("terminated") || what == QStringLiteral("rejected")
      || what == QStringLiteral("cancelled")) {
    qCInfo(lcCall) << "Call ended:" << leg << what
                   << payload.value(QStringLiteral("reason")).toString();
    teardownCall(leg);
    emit callStateChanged(leg, QStringLiteral("ended"), payload.value(QStringLiteral("reason")).toString());
  }
}

void CallManager::setupVideoTrack(const QString &leg, const std::shared_ptr<rtc::PeerConnection> &pc,
                                  const QString &videoMid)
{
  PeerContext &ctx = m_peers[leg];
  if (ctx.localVideoTrack) {
    return;
  }
  ctx.pc = pc;
  if (ctx.videoSsrc == 0) {
    ctx.videoSsrc = randomSsrc();
  }

  const std::string mid = (videoMid.isEmpty() ? QStringLiteral("video") : videoMid).toStdString();
  rtc::Description::Video videoDesc(mid, rtc::Description::Direction::SendRecv);
  videoDesc.addH264Codec(kH264Payload);
  videoDesc.addSSRC(ctx.videoSsrc, "video", "stream", "video");
  ctx.localVideoTrack = pc->addTrack(videoDesc);

  ctx.videoRtpConfig = std::make_shared<rtc::RtpPacketizationConfig>(
      ctx.videoSsrc, "video", kH264Payload, rtc::H264RtpPacketizer::ClockRate);
  ctx.nextVideoRtpTimestamp = 0;
  auto handler = std::make_shared<rtc::H264RtpPacketizer>(
      rtc::NalUnit::Separator::StartSequence, ctx.videoRtpConfig);
  handler->addToChain(std::make_shared<rtc::H264RtpDepacketizer>(
      rtc::NalUnit::Separator::StartSequence));
  handler->addToChain(std::make_shared<rtc::RtcpReceivingSession>());
  handler->addToChain(std::make_shared<rtc::RtcpSrReporter>(ctx.videoRtpConfig));
  handler->addToChain(std::make_shared<rtc::RtcpNackResponder>());
  ctx.localVideoTrack->setMediaHandler(handler);

  ctx.localVideoTrack->onOpen([this, leg]() {
    QMetaObject::invokeMethod(this, [this, leg]() {
      if (!m_peers.contains(leg)) {
        return;
      }
      m_peers[leg].videoTrackOpen = true;
      qCInfo(lcCall) << "Local video track open for" << leg;
    }, Qt::QueuedConnection);
  });
  ctx.localVideoTrack->onFrame([this, leg](rtc::binary data, rtc::FrameInfo) {
    if (data.empty()) {
      return;
    }
    const QByteArray payload(reinterpret_cast<const char *>(data.data()),
                             static_cast<int>(data.size()));
    QMetaObject::invokeMethod(this, [this, leg, payload]() {
      decodeRemoteVideoFrame(leg, payload);
    }, Qt::QueuedConnection);
  });
}

void CallManager::attachIncomingVideoTrack(const QString &leg, const std::shared_ptr<rtc::Track> &track)
{
  if (!m_peers.contains(leg)) {
    qCWarning(lcCall) << "Remote video track before peer context for" << leg;
    return;
  }

  PeerContext &ctx = m_peers[leg];
  ctx.remoteVideoTrack = track;

  auto depacketizer = std::make_shared<rtc::H264RtpDepacketizer>(
      rtc::NalUnit::Separator::StartSequence);
  depacketizer->addToChain(std::make_shared<rtc::RtcpReceivingSession>());
  track->setMediaHandler(depacketizer);

  track->onOpen([leg]() {
    qCInfo(lcCall) << "Remote video track open for" << leg;
  });

  track->onFrame([this, leg](rtc::binary data, rtc::FrameInfo) {
    QMetaObject::invokeMethod(this, [this, leg, payload = QByteArray(reinterpret_cast<const char *>(data.data()), static_cast<int>(data.size()))]() {
      if (!m_calls.contains(leg) || !m_calls[leg].connected) {
        return;
      }
      static int recvCount = 0;
      if ((recvCount++ % 50) == 0) {
        qCInfo(lcCall) << "Receiving video frame" << payload.size() << "bytes on" << leg
                       << "(#" << recvCount << ", via remote track)";
      }
      decodeRemoteVideoFrame(leg, payload);
    }, Qt::QueuedConnection);
  });
}

QString CallManager::extractVideoMid(const QString &sdp) const
{
  const QStringList lines = sdp.split(QStringLiteral("\r\n"), Qt::SkipEmptyParts);
  bool inVideo = false;
  for (const QString &line : lines) {
    if (line.startsWith(QStringLiteral("m="))) {
      inVideo = line.startsWith(QStringLiteral("m=video"));
      continue;
    }
    if (inVideo && line.startsWith(QStringLiteral("a=mid:"))) {
      return line.mid(6).trimmed();
    }
  }
  return QStringLiteral("1");
}

void CallManager::sendVideo(const QString &leg, bool send)
{
  if (!m_calls.contains(leg)) {
    return;
  }
  CallSession &session = m_calls[leg];
  if (send == session.sendVideo) {
    return;
  }
  session.sendVideo = send && startVideoCapture(leg);
  if (!session.sendVideo && m_videoCaptureLeg == leg) {
    stopVideoCapture();
  }
  emit videoSendingChanged(leg, session.sendVideo);
  qCInfo(lcCall) << "Video send" << (session.sendVideo ? "enabled" : "disabled") << "for" << leg;
}

void CallManager::toggleSendVideo(const QString &leg)
{
  if (!m_calls.contains(leg)) {
    return;
  }
  sendVideo(leg, !m_calls[leg].sendVideo);
}

bool CallManager::startVideoCapture(const QString &leg)
{
  if (leg.isEmpty() || !m_calls.contains(leg)) {
    return false;
  }
  if (m_videoCapture.isRunning() || m_screenCapture.isRunning()) {
    return m_videoCaptureLeg == leg;
  }
  if (!m_videoEncoder.open(kVideoWidth, kVideoHeight, kVideoFps, 700000)) {
    return false;
  }
  const bool sourceStarted = m_screenSharing
                                 ? m_screenCapture.start(kVideoWidth, kVideoHeight, kVideoFps)
                                 : m_videoCapture.start(kVideoWidth, kVideoHeight, kVideoFps);
  if (!sourceStarted) {
    m_videoEncoder.close();
    return false;
  }
  m_videoCaptureLeg = leg;
  m_videoFrameTimer.restart();
  emit videoSendingChanged(leg, true);
  emit screenSharingChanged(leg, m_screenSharing);
  return true;
}

void CallManager::stopVideoCapture()
{
  const QString leg = m_videoCaptureLeg;
  m_videoCapture.stop();
  m_screenCapture.stop();
  m_videoEncoder.close();
  m_videoCaptureLeg.clear();
  m_videoFrameTimer.invalidate();
  if (!leg.isEmpty()) {
    emit videoSendingChanged(leg, false);
  }
}

void CallManager::sendCapturedVideoFrame(const QImage &frame)
{
  if (m_videoCaptureLeg.isEmpty() || !m_calls.contains(m_videoCaptureLeg)
      || !m_peers.contains(m_videoCaptureLeg)) {
    return;
  }
  if (m_videoFrameTimer.isValid() && m_videoFrameTimer.elapsed() < 1000 / kVideoFps) {
    return;
  }
  m_videoFrameTimer.restart();

  const QString leg = m_videoCaptureLeg;
  CallSession &session = m_calls[leg];
  PeerContext &ctx = m_peers[leg];
  if (!session.sendVideo || session.onHold || !ctx.localVideoTrack
      || !ctx.localVideoTrack->isOpen()) {
    emit localVideoFrame(frame);
    return;
  }

  QImage outgoingFrame = frame;
  if (m_videoBlur) {
    outgoingFrame = frame.scaled(40, 23, Qt::IgnoreAspectRatio, Qt::SmoothTransformation)
                        .scaled(frame.size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
  }
  const QByteArray encoded = m_videoEncoder.encode(outgoingFrame);
  if (encoded.isEmpty()) {
    return;
  }
  rtc::FrameInfo info(ctx.nextVideoRtpTimestamp);
  ctx.nextVideoRtpTimestamp += kVideoTimestampStep;
  ctx.localVideoTrack->sendFrame(
      reinterpret_cast<const rtc::byte *>(encoded.constData()),
      static_cast<size_t>(encoded.size()), info);
  emit localVideoFrame(outgoingFrame);
}

void CallManager::decodeRemoteVideoFrame(const QString &leg, const QByteArray &payload)
{
  if (payload.isEmpty() || !m_calls.contains(leg) || !m_calls[leg].connected) {
    return;
  }
  if (!m_videoDecoder.isOpen() && !m_videoDecoder.open()) {
    return;
  }
  const QImage frame = m_videoDecoder.decode(payload);
  if (!frame.isNull()) {
    emit remoteVideoFrame(leg, frame);
  }
}

void CallManager::setScreenSharing(const QString &leg, bool enabled)
{
  if (leg.isEmpty() || !m_calls.contains(leg) || m_screenSharing == enabled) {
    return;
  }
  const bool wasSending = m_calls[leg].sendVideo;
  stopVideoCapture();
  m_screenSharing = enabled;
  if (enabled || wasSending) {
    m_calls[leg].sendVideo = startVideoCapture(leg);
  }
  emit screenSharingChanged(leg, m_screenSharing && m_calls[leg].sendVideo);
}

} // namespace itl
