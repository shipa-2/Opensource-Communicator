#pragma once

#include <QJsonObject>
#include <QString>

namespace itl {

enum class CallPhase {
    Idle,
    Provisioning,
    Negotiating,
    Ringing,
    Connected,
    Hold,
    Ended
};

enum class SdpType {
    Offer,
    Answer
};

struct CallSession {
    QString leg;
    QString peer;
    QString remoteSdp;
    QString localSdp;
    QString realName;
    QJsonObject conference;
    bool incoming = false;
    bool connected = false;
    bool onHold = false;
    bool acceptPending = false;
    bool pendingUpdateAnswer = false;
    bool isConference = false;
    CallPhase phase = CallPhase::Idle;
};

} // namespace itl
