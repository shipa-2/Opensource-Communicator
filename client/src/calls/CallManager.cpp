#include "CallManager.h"

#include "protocol/ProtocolTypes.h"
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
constexpr int kPublishFallbackMs = 1500;

uint32_t randomSsrc()
{
  return static_cast<uint32_t>(QUuid::createUuid().data1);
}

QString localIPv4()
{
  for (const QNetworkInterface &iface : QNetworkInterface::allInterfaces()) {
    if (!(iface.flags() & QNetworkInterface::IsUp) || (iface.flags() & QNetworkInterface::IsLoopBack)) {
      continue;
    }
    for (const QNetworkAddressEntry &entry : iface.addressEntries()) {
      const QHostAddress ip = entry.ip();
      if (ip.protocol() == QAbstractSocket::IPv4Protocol && !ip.isLoopback() && !ip.isLinkLocal()) {
        return ip.toString();
      }
    }
  }
  return {};
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
    auto track = m_peers[m_activeLeg].localAudioTrack;
    if (!track || !track->isOpen()) {
      return;
    }
    rtc::FrameInfo info(static_cast<uint32_t>(QDateTime::currentMSecsSinceEpoch()));
    track->sendFrame(reinterpret_cast<const rtc::byte *>(opus.constData()), static_cast<size_t>(opus.size()), info);
  });
}

CallManager::~CallManager()
{
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

QString CallManager::patchSdpLocalAddress(const QString &sdp) const
{
  const QString ip = localIPv4();
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

void CallManager::startAudio()
{
  if (!m_audio.isRunning()) {
    m_audio.start();
  }
}

void CallManager::stopAudio()
{
  if (m_audio.isRunning()) {
    m_audio.stop();
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

void CallManager::setupAudioTrack(const QString &leg, const std::shared_ptr<rtc::PeerConnection> &pc)
{
  PeerContext ctx;
  ctx.pc = pc;
  ctx.ssrc = randomSsrc();

  rtc::Description::Audio audio("audio", rtc::Description::Direction::SendRecv);
  audio.addOpusCodec(kOpusPayload);
  audio.addSSRC(ctx.ssrc, "audio", "stream", "audio");
  ctx.localAudioTrack = pc->addTrack(audio);

  auto rtpConfig = std::make_shared<rtc::RtpPacketizationConfig>(ctx.ssrc, "audio", kOpusPayload, rtc::OpusRtpPacketizer::DefaultClockRate);
  auto handler = std::make_shared<rtc::OpusRtpPacketizer>(rtpConfig);
  handler->addToChain(std::make_shared<rtc::OpusRtpDepacketizer>());
  handler->addToChain(std::make_shared<rtc::RtcpReceivingSession>());
  handler->addToChain(std::make_shared<rtc::RtcpSrReporter>(rtpConfig));
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
      m_audio.decodeAndPlayOpus(payload);
    }, Qt::QueuedConnection);
  });

  m_peers.insert(leg, ctx);
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
      m_audio.decodeAndPlayOpus(payload);
    }, Qt::QueuedConnection);
  });
}

void CallManager::beginNegotiation(const QString &leg, bool createOffer)
{
  if (!m_calls.contains(leg)) {
    return;
  }

  const QString localIp = localIPv4();

  rtc::Configuration config;
  config.enableIceUdpMux = true;
  config.forceMediaTransport = true;
  if (!localIp.isEmpty()) {
    config.bindAddress = localIp.toStdString();
  }

  auto pc = std::make_shared<rtc::PeerConnection>(config);

  pc->onLocalDescription([this, leg, pc](rtc::Description description) {
    QMetaObject::invokeMethod(this, [this, leg, pc, description]() {
      if (!m_calls.contains(leg)) {
        return;
      }
      CallSession &session = m_calls[leg];
      session.localSdp = patchSdpLocalAddress(QString::fromStdString(std::string(description)));
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
    if (mediaType != QStringLiteral("audio")) {
      return;
    }
    QMetaObject::invokeMethod(this, [this, leg, track]() {
      attachIncomingTrack(leg, track);
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
    setupAudioTrack(leg, pc);

    if (!createOffer) {
      const QString remote = m_calls[leg].remoteSdp;
      if (!remote.isEmpty()) {
        pc->setRemoteDescription(rtc::Description(remote.toStdString(), rtc::Description::Type::Offer));
      }
    }

    pc->setLocalDescription(createOffer ? rtc::Description::Type::Offer : rtc::Description::Type::Answer);
    m_calls[leg].phase = CallPhase::Negotiating;
    schedulePublishFallback(leg, pc);

    if (pc->gatheringState() == rtc::PeerConnection::GatheringState::Complete) {
      publishLocalSdp(leg, pc);
    }
  } catch (const std::exception &e) {
    qCCritical(lcCall) << "WebRTC negotiation failed:" << e.what();
    emit callStateChanged(leg, QStringLiteral("error"), QString::fromUtf8(e.what()));
    teardownCall(leg);
  }
}

void CallManager::applyRemoteSdp(const QString &leg, const QString &sdp, SdpType type, bool force)
{
  if (!m_peers.contains(leg) || sdp.isEmpty() || !m_peers[leg].pc) {
    return;
  }

  PeerContext &ctx = m_peers[leg];
  if (ctx.remoteSdpApplied && !force) {
    return;
  }

  try {
    const auto rtcType = type == SdpType::Offer ? rtc::Description::Type::Offer : rtc::Description::Type::Answer;
    ctx.pc->setRemoteDescription(rtc::Description(sdp.toStdString(), rtcType));
    ctx.remoteSdpApplied = true;
    qCInfo(lcCall) << "Applied remote SDP for" << leg;
  } catch (const std::exception &e) {
    qCWarning(lcCall) << "Failed to apply remote SDP:" << e.what();
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
  session.localSdp = patchSdpLocalAddress(QString::fromStdString(std::string(*desc)));

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
    m_api->acceptCall(leg, session.localSdp);
    emit callStateChanged(leg, QStringLiteral("accepting"), session.peer);
  } else if (session.phase == CallPhase::Negotiating) {
    m_api->startCall(leg, session.peer, session.localSdp, {}, session.conference);
    startRingback();
    emit callStateChanged(leg, QStringLiteral("dialing"), session.peer);
  }
}

QString CallManager::startOutgoingCall(const QString &peer)
{
  hangupAll();

  const QString leg = createLegId();
  CallSession session;
  session.leg = leg;
  session.peer = peer;
  session.incoming = false;
  session.phase = CallPhase::Negotiating;
  m_calls.insert(leg, session);

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
        {QStringLiteral("muted"), false},
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

  beginNegotiation(leg, true);
  emit callStateChanged(leg, QStringLiteral("connecting"), subject);
  return leg;
}

void CallManager::acceptIncomingCall(const QString &leg)
{
  if (!m_calls.contains(leg)) {
    return;
  }

  stopIncomingRing();

  CallSession &session = m_calls[leg];
  session.acceptPending = true;

  if (!session.localSdp.isEmpty()) {
    sendLocalSdp(leg);
    return;
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
  emit callStateChanged(leg, hold ? QStringLiteral("hold") : QStringLiteral("resumed"), session.peer);
}

void CallManager::blindTransfer(const QString &leg, const QString &targetPeer)
{
  m_api->blindTransfer(leg, targetPeer);
  emit callStateChanged(leg, QStringLiteral("transfer"), targetPeer);
}

CallSession *CallManager::call(const QString &leg)
{
  return m_calls.contains(leg) ? &m_calls[leg] : nullptr;
}

void CallManager::teardownCall(const QString &leg)
{
  const bool incoming = m_calls.value(leg).incoming;
  cancelPublishFallback(leg);
  m_peers.remove(leg);
  m_calls.remove(leg);
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

    const QJsonObject dest = payload.value(QStringLiteral("dest")).toObject();
    session.peer = dest.value(QStringLiteral("From")).toObject().value(QString::fromUtf8(kEmptyKey)).toString();
    if (session.peer.isEmpty()) {
      session.peer = dest.value(QStringLiteral("From")).toObject().value(QString::fromUtf8(kEmptyKey)).toObject().value(QString::fromUtf8(kEmptyKey)).toString();
    }
    session.realName = dest.value(QStringLiteral("From")).toObject().value(QStringLiteral("@realName")).toString();

    m_calls.insert(leg, session);
    m_api->provisionCall(leg);
    startIncomingRing();
    emit callStateChanged(leg, QStringLiteral("incoming"), session.realName.isEmpty() ? session.peer : session.realName);
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
    if (session.incoming && !session.remoteSdp.isEmpty() && !m_peers.contains(leg)) {
      beginNegotiation(leg, false);
    } else if (!session.incoming && !session.remoteSdp.isEmpty()) {
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
    m_activeLeg = leg;
    startAudio();
    emit callStateChanged(leg, QStringLiteral("connected"), session.peer);
    return;
  }

  if (what == QStringLiteral("updated") || what == QStringLiteral("updateAccepted")) {
    session.remoteSdp = payload.value(QStringLiteral("sdp")).toString();
    if (m_peers.contains(leg)) {
      m_peers[leg].remoteSdpApplied = false;
    }
    applyRemoteSdp(leg, session.remoteSdp, SdpType::Offer, true);
    if (session.localSdp.isEmpty() && m_peers.contains(leg)) {
      m_peers[leg].pc->setLocalDescription(rtc::Description::Type::Answer);
    }
    return;
  }

  if (what == QStringLiteral("terminated") || what == QStringLiteral("rejected") || what == QStringLiteral("cancelled")) {
    teardownCall(leg);
    emit callStateChanged(leg, QStringLiteral("ended"), payload.value(QStringLiteral("reason")).toString());
  }
}

} // namespace itl
