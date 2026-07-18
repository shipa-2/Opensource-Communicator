#pragma once

#include "CallTypes.h"
#include "audio/AudioBridge.h"
#include "audio/CallRecorder.h"
#include "audio/ExternalMediaPauser.h"
#include "audio/IncomingRingPlayer.h"
#include "audio/RingbackPlayer.h"
#include "protocol/WsApiClient.h"

#include <QHash>
#include <QImage>
#include <QObject>
#include <QTimer>
#include <QUuid>
#include <memory>

namespace rtc {
class PeerConnection;
class Track;
class RtpPacketizationConfig;
}

namespace itl {

class AppSettings;

struct ConferenceParticipant {
    QString peer;
    QString name;
    bool owner = false;
    bool operatorPeer = false;
    bool listener = false; // слушатель: только слушает (muted), не говорит
};

struct PeerContext {
    std::shared_ptr<rtc::PeerConnection> pc;
    std::shared_ptr<rtc::Track> localAudioTrack;
    std::shared_ptr<rtc::Track> remoteAudioTrack;
    std::shared_ptr<rtc::RtpPacketizationConfig> rtpConfig;
    uint32_t ssrc = 0;
    uint32_t nextRtpTimestamp = 0;
    bool sdpSent = false;
    bool remoteSdpApplied = false;
    bool iceGatheringStarted = false;
    QString appliedRemoteSdp;
    std::shared_ptr<rtc::Track> localVideoTrack;
    std::shared_ptr<rtc::Track> remoteVideoTrack;
    std::shared_ptr<rtc::RtpPacketizationConfig> videoRtpConfig;
    uint32_t videoSsrc = 0;
    uint32_t nextVideoRtpTimestamp = 0;
    bool videoTrackOpen = false;
};

class CallManager : public QObject {
    Q_OBJECT

public:
    explicit CallManager(WsApiClient *api, AppSettings *settings, QObject *parent = nullptr);
    ~CallManager() override;

    void applySettings();

    QString startOutgoingCall(const QString &peer, bool videoCall = false);
    QString startConferenceCall(const QString &subject, const QList<ConferenceParticipant> &participants);
    void acceptIncomingCall(const QString &leg);
    void rejectIncomingCall(const QString &leg);
    void hangup(const QString &leg);
    void hangupAll();
    void setHold(const QString &leg, bool hold);
    void blindTransfer(const QString &leg, const QString &targetPeer);
    void setRecordingName(const QString &leg, const QString &name);
    void pauseExternalMedia();
    void resumeExternalMedia();
    void sendDtmf(const QString &leg, QChar digit);
    void sendVideo(const QString &leg, bool send);
    void toggleSendVideo(const QString &leg);

    CallSession *call(const QString &leg);
    QString activeLeg() const { return m_activeLeg; }
    bool hasActiveCalls() const { return !m_calls.isEmpty(); }

signals:
    void callStateChanged(const QString &leg, const QString &state, const QString &detail);
    void callRecordingFinished(const QString &path);
    void remoteAudioStarted(const QString &leg);
    void remoteAudioLevel(float level);
    void remoteVideoFrame(const QString &leg, const QImage &frame);

public slots:
    void handleServerCallEvent(const QString &leg, const QString &what, const QJsonObject &payload);

private:
    QString createLegId() const;
    void beginNegotiation(const QString &leg, bool createOffer, bool deferIceGathering = false);
    void beginIncomingIceGathering(const QString &leg);
    void setupAudioTrack(const QString &leg, const std::shared_ptr<rtc::PeerConnection> &pc,
                         const QString &audioMid = QStringLiteral("audio"));
    void setupVideoTrack(const QString &leg, const std::shared_ptr<rtc::PeerConnection> &pc);
    void attachIncomingTrack(const QString &leg, const std::shared_ptr<rtc::Track> &track);
    void attachIncomingVideoTrack(const QString &leg, const std::shared_ptr<rtc::Track> &track);
    void applyRemoteSdp(const QString &leg, const QString &sdp, SdpType type, bool force = false);
    void sendLocalSdp(const QString &leg);
    void publishLocalSdp(const QString &leg, const std::shared_ptr<rtc::PeerConnection> &pc);
    void schedulePublishFallback(const QString &leg, const std::shared_ptr<rtc::PeerConnection> &pc);
    void cancelPublishFallback(const QString &leg);
    void releasePeer(const QString &leg);
    void maybePreNegotiateIncoming(const QString &leg);
    void teardownCall(const QString &leg);
    bool hasActiveOutgoing() const;
    QString primaryOutgoingLeg() const;
    QString modifySdpForHold(const QString &sdp, bool hold) const;
    bool isSdpMediaOnHold(const QString &sdp) const;
    void updateHoldStateFromSdp(const QString &leg, const QString &sdp, bool remoteInitiated);
    void answerRemoteUpdate(const QString &leg);
    QString patchSdpLocalAddress(const QString &sdp) const;
    QString extractAudioMid(const QString &sdp) const;
    QString extractVideoMid(const QString &sdp) const;
    QString sanitizeLocalSdp(const QString &sdp, bool hasVideo = false) const;
    QString bindIPv4() const;
    void startAudio();
    void stopAudio();
    void startRingback();
    void stopRingback();
    void startIncomingRing();
    void stopIncomingRing();
    void startCallRecording();
    void stopCallRecording();
    void onFirstRemoteAudio(const QString &leg);
    void noteRemoteOpusFrame(const QString &leg, const QByteArray &opus);
    QString contactNameForLeg(const QString &leg) const;
    static bool isAudibleOpusFrame(const QByteArray &opus);
    void onCallSessionStarted();
    void onCallSessionEnded();

    WsApiClient *m_api = nullptr;
    AppSettings *m_settings = nullptr;
    AudioBridge m_audio;
    CallRecorder m_recorder;
    RingbackPlayer m_ringback;
    IncomingRingPlayer m_incomingRing;
    ExternalMediaPauser m_externalMedia;
    QHash<QString, CallSession> m_calls;
    QHash<QString, PeerContext> m_peers;
    QHash<QString, QTimer *> m_publishTimers;
    QHash<QString, QString> m_recordingNames;
    QString m_activeLeg;
    QString m_remoteAudioStartedLeg;
    mutable int m_outgoingLegCounter = 0;
};

} // namespace itl
