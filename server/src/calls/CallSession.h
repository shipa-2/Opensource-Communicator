#pragma once

#include <QJsonObject>
#include <QString>

namespace itl {

enum class ServerCallState {
    Idle,
    Provisioning,
    Ringing,
    Connected,
    Hold,
    Ended
};

struct ServerCallSession {
    QString leg;
    QString callerLogin;
    QString callerSid;
    QString calleeLogin;
    QString calleeSid;
    QString callerLeg;
    QString calleeLeg;
    QString callerSdp;
    QString calleeSdp;
    QString peer;
    ServerCallState state = ServerCallState::Idle;
    QString heldBy;  // login of the user who put the call on hold
    bool isConference = false;
};

} // namespace itl
