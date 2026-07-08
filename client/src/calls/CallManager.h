#pragma once

#include "CallTypes.h"
#include "audio/AudioBridge.h"
#include "audio/IncomingRingPlayer.h"
#include "audio/RingbackPlayer.h"
#include "protocol/WsApiClient.h"

#include <QHash>
#include <QObject>
#include <QTimer>
#include <QUuid>
#include <memory>

namespace rtc {
class PeerConnection;
class Track;
}

namespace itl {

class AppSettings;

struct ConferenceParticipant {
    QString peer;
    QString name;
    bool owner = false;
    bool operatorPeer = false;
};

struct PeerContext {
    std::shared_ptr<rtc::PeerConnection> pc;
    std::shared_ptr<rtc::Track> localAudioTrack;
    std::shared_ptr<rtc::Track> remoteAudioTrack;
    uint32_t ssrc = 0;
    bool sdpSent = false;
    bool remoteSdpApplied = false;
};

class CallManager : public QObject {
    Q_OBJECT

public:
    explicit CallManager(WsApiClient *api, AppSettings *settings, QObject *parent = nullptr);
    ~CallManager() override;

    void applySettings();

    QString startOutgoingCall(const QString &peer);
    QString startConferenceCall(const QString &subject, const QList<ConferenceParticipant> &participants);
    void acceptIncomingCall(const QString &leg);
    void rejectIncomingCall(const QString &leg);
    void hangup(const QString &leg);
    void hangupAll();
    void setHold(const QString &leg, bool hold);
    void blindTransfer(const QString &leg, const QString &targetPeer);

    CallSession *call(const QString &leg);
    QString activeLeg() const { return m_activeLeg; }

signals:
    void callStateChanged(const QString &leg, const QString &state, const QString &detail);

public slots:
    void handleServerCallEvent(const QString &leg, const QString &what, const QJsonObject &payload);

private:
    QString createLegId() const;
    void beginNegotiation(const QString &leg, bool createOffer);
    void setupAudioTrack(const QString &leg, const std::shared_ptr<rtc::PeerConnection> &pc);
    void attachIncomingTrack(const QString &leg, const std::shared_ptr<rtc::Track> &track);
    void applyRemoteSdp(const QString &leg, const QString &sdp, SdpType type, bool force = false);
    void sendLocalSdp(const QString &leg);
    void publishLocalSdp(const QString &leg, const std::shared_ptr<rtc::PeerConnection> &pc);
    void schedulePublishFallback(const QString &leg, const std::shared_ptr<rtc::PeerConnection> &pc);
    void cancelPublishFallback(const QString &leg);
    void teardownCall(const QString &leg);
    bool hasActiveOutgoing() const;
    QString primaryOutgoingLeg() const;
    QString modifySdpForHold(const QString &sdp, bool hold) const;
    QString patchSdpLocalAddress(const QString &sdp) const;
    void startAudio();
    void stopAudio();
    void startRingback();
    void stopRingback();
    void startIncomingRing();
    void stopIncomingRing();

    WsApiClient *m_api = nullptr;
    AppSettings *m_settings = nullptr;
    AudioBridge m_audio;
    RingbackPlayer m_ringback;
    IncomingRingPlayer m_incomingRing;
    QHash<QString, CallSession> m_calls;
    QHash<QString, PeerContext> m_peers;
    QHash<QString, QTimer *> m_publishTimers;
    QString m_activeLeg;
    mutable int m_outgoingLegCounter = 0;
};

} // namespace itl
