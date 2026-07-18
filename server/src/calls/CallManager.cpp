#include "CallManager.h"
#include "../network/WsSession.h"
#include "../session/SessionManager.h"
#include "../session/UserSession.h"

#include <QJsonArray>
#include <QLoggingCategory>
#include <QUuid>

Q_LOGGING_CATEGORY(lcCall, "server.call")

namespace itl {

CallManager::CallManager(SessionManager *sessions, QObject *parent)
    : QObject(parent)
    , m_sessions(sessions)
{
    // Try to load hold music from common locations
    const QStringList paths = {
        QStringLiteral("/usr/share/communicator-server/hold_music.wav"),
        QStringLiteral("/opt/communicator-server/hold_music.wav"),
        QStringLiteral("hold_music.wav"),
    };
    for (const QString &path : paths) {
        if (m_holdMusic.loadWav(path)) {
            break;
        }
    }
}

QString CallManager::generateLegId()
{
    return QStringLiteral("S%1").arg(m_legCounter++);
}

ServerCallSession *CallManager::findCallByLeg(const QString &leg)
{
    return m_calls.contains(leg) ? &m_calls[leg] : nullptr;
}

ServerCallSession *CallManager::findCallByPeer(const QString &peer)
{
    for (auto it = m_calls.begin(); it != m_calls.end(); ++it) {
        if (it->callerLogin == peer || it->calleeLogin == peer) {
            return &(*it);
        }
    }
    return nullptr;
}

void CallManager::pushToSession(UserSession *target, const QJsonObject &payload)
{
    if (target && target->wsSession()) {
        target->wsSession()->sendPayload(payload);
    }
}

void CallManager::handleProvisionCall(UserSession *session, int requestId, const QJsonObject &payload)
{
    const QString leg = payload.value(QStringLiteral("leg")).toString();
    QJsonObject resp;
    resp.insert(QStringLiteral("response"), QStringLiteral("ok"));
    session->wsSession()->sendResponse(requestId, resp);
    qCInfo(lcCall) << "ProvisionCall" << leg << "from" << session->login();
}

void CallManager::handleStartCall(UserSession *session, int requestId, const QJsonObject &payload)
{
    const QString leg = payload.value(QStringLiteral("leg")).toString();
    const QJsonObject addr = payload.value(QStringLiteral("addr")).toObject();
    const QString peer = addr.value(QString::fromUtf8("")).toString();
    const QString sdp = payload.value(QStringLiteral("sdp")).toString();

    // Find target user
    QString targetLogin = peer;
    const int atIdx = peer.indexOf(QLatin1Char('@'));
    if (atIdx > 0) {
        targetLogin = peer.left(atIdx);
    }

    QList<UserSession *> targets = m_sessions->sessionsForLogin(targetLogin);
    if (targets.isEmpty()) {
        QJsonObject resp;
        resp.insert(QStringLiteral("error"), QStringLiteral("user not found"));
        session->wsSession()->sendResponse(requestId, resp);
        return;
    }

    // Response to caller
    QJsonObject resp;
    resp.insert(QStringLiteral("response"), QStringLiteral("ok"));
    session->wsSession()->sendResponse(requestId, resp);

    // Create call sessions
    const QString callerLeg = leg;
    const QString calleeLeg = generateLegId();

    ServerCallSession call;
    call.leg = callerLeg;
    call.callerLogin = session->login();
    call.callerSid = session->sid();
    call.calleeLogin = targetLogin;
    call.calleeSid = targets.first()->sid();
    call.callerLeg = callerLeg;
    call.calleeLeg = calleeLeg;
    call.callerSdp = sdp;
    call.peer = peer;
    call.state = ServerCallState::Ringing;

    m_calls.insert(callerLeg, call);
    m_calls.insert(calleeLeg, ServerCallSession{
        .leg = calleeLeg,
        .callerLogin = session->login(),
        .callerSid = session->sid(),
        .calleeLogin = targetLogin,
        .calleeSid = targets.first()->sid(),
        .callerLeg = callerLeg,
        .calleeLeg = calleeLeg,
        .callerSdp = sdp,
        .peer = session->login() + QLatin1Char('@') + session->domain(),
        .state = ServerCallState::Ringing,
    });

    // Push incomingCall to callee
    QJsonObject fromObj;
    fromObj.insert(QString::fromUtf8(""), session->login() + QLatin1Char('@') + session->domain());
    fromObj.insert(QStringLiteral("@realName"), session->login());

    QJsonObject incoming;
    incoming.insert(QStringLiteral("What"), QStringLiteral("incomingCall"));
    incoming.insert(QStringLiteral("leg"), calleeLeg);
    incoming.insert(QStringLiteral("sdp"), sdp);
    incoming.insert(QStringLiteral("dest"), QJsonObject{
        {QStringLiteral("From"), fromObj},
    });

    pushToSession(targets.first(), incoming);
    qCInfo(lcCall) << "StartCall" << session->login() << "->" << peer
                   << "legs:" << callerLeg << calleeLeg;
}

void CallManager::handleAcceptCall(UserSession *session, int requestId, const QJsonObject &payload)
{
    const QString leg = payload.value(QStringLiteral("leg")).toString();
    const QString sdp = payload.value(QStringLiteral("sdp")).toString();

    ServerCallSession *call = findCallByLeg(leg);
    if (!call) {
        QJsonObject resp;
        resp.insert(QStringLiteral("error"), QStringLiteral("unknown leg"));
        session->wsSession()->sendResponse(requestId, resp);
        return;
    }

    call->calleeSdp = sdp;
    call->state = ServerCallState::Connected;

    // Response to callee
    QJsonObject resp;
    resp.insert(QStringLiteral("response"), QStringLiteral("ok"));
    session->wsSession()->sendResponse(requestId, resp);

    // Forward SDP to caller
    UserSession *callerSession = m_sessions->session(call->callerSid);
    if (callerSession) {
        QJsonObject accepted;
        accepted.insert(QStringLiteral("What"), QStringLiteral("accepted"));
        accepted.insert(QStringLiteral("leg"), call->callerLeg);
        accepted.insert(QStringLiteral("sdp"), sdp);
        pushToSession(callerSession, accepted);
    }

    qCInfo(lcCall) << "AcceptCall" << leg << "from" << session->login();
}

void CallManager::handleRejectCall(UserSession *session, int requestId, const QJsonObject &payload)
{
    const QString leg = payload.value(QStringLiteral("leg")).toString();
    const int code = payload.value(QStringLiteral("code")).toInt();
    const QString reason = payload.value(QStringLiteral("reason")).toString();

    ServerCallSession *call = findCallByLeg(leg);
    if (!call) {
        QJsonObject resp;
        resp.insert(QStringLiteral("error"), QStringLiteral("unknown leg"));
        session->wsSession()->sendResponse(requestId, resp);
        return;
    }

    QJsonObject resp;
    resp.insert(QStringLiteral("response"), QStringLiteral("ok"));
    session->wsSession()->sendResponse(requestId, resp);

    // Notify caller
    UserSession *callerSession = m_sessions->session(call->callerSid);
    if (callerSession) {
        QJsonObject rejected;
        rejected.insert(QStringLiteral("What"), QStringLiteral("rejected"));
        rejected.insert(QStringLiteral("leg"), call->callerLeg);
        rejected.insert(QStringLiteral("code"), code);
        rejected.insert(QStringLiteral("reason"), reason);
        pushToSession(callerSession, rejected);
    }

    m_calls.remove(call->callerLeg);
    m_calls.remove(call->calleeLeg);
    qCInfo(lcCall) << "RejectCall" << leg << "code:" << code;
}

void CallManager::handleCancelCall(UserSession *session, int requestId, const QJsonObject &payload)
{
    const QString leg = payload.value(QStringLiteral("leg")).toString();
    const int code = payload.value(QStringLiteral("code")).toInt();

    ServerCallSession *call = findCallByLeg(leg);
    if (!call) {
        QJsonObject resp;
        resp.insert(QStringLiteral("error"), QStringLiteral("unknown leg"));
        session->wsSession()->sendResponse(requestId, resp);
        return;
    }

    QJsonObject resp;
    resp.insert(QStringLiteral("response"), QStringLiteral("ok"));
    session->wsSession()->sendResponse(requestId, resp);

    // Notify callee
    UserSession *calleeSession = m_sessions->session(call->calleeSid);
    if (calleeSession) {
        QJsonObject cancelled;
        cancelled.insert(QStringLiteral("What"), QStringLiteral("cancelled"));
        cancelled.insert(QStringLiteral("leg"), call->calleeLeg);
        cancelled.insert(QStringLiteral("code"), code);
        pushToSession(calleeSession, cancelled);
    }

    m_calls.remove(call->callerLeg);
    m_calls.remove(call->calleeLeg);
    qCInfo(lcCall) << "CancelCall" << leg;
}

void CallManager::handleDisconnectCall(UserSession *session, int requestId, const QJsonObject &payload)
{
    const QString leg = payload.value(QStringLiteral("leg")).toString();
    const int code = payload.value(QStringLiteral("code")).toInt();

    ServerCallSession *call = findCallByLeg(leg);
    if (!call) {
        QJsonObject resp;
        resp.insert(QStringLiteral("error"), QStringLiteral("unknown leg"));
        session->wsSession()->sendResponse(requestId, resp);
        return;
    }

    QJsonObject resp;
    resp.insert(QStringLiteral("response"), QStringLiteral("ok"));
    session->wsSession()->sendResponse(requestId, resp);

    // Notify the other party
    const QString otherLeg = (leg == call->callerLeg) ? call->calleeLeg : call->callerLeg;
    const QString otherSid = (leg == call->callerLeg) ? call->calleeSid : call->callerSid;
    UserSession *otherSession = m_sessions->session(otherSid);
    if (otherSession) {
        QJsonObject terminated;
        terminated.insert(QStringLiteral("What"), QStringLiteral("terminated"));
        terminated.insert(QStringLiteral("leg"), otherLeg);
        terminated.insert(QStringLiteral("code"), code);
        pushToSession(otherSession, terminated);
    }

    m_calls.remove(call->callerLeg);
    m_calls.remove(call->calleeLeg);
    qCInfo(lcCall) << "DisconnectCall" << leg;
}

void CallManager::handleAckAccept(UserSession *session, int requestId, const QJsonObject &payload)
{
    const QString leg = payload.value(QStringLiteral("leg")).toString();

    ServerCallSession *call = findCallByLeg(leg);
    if (call) {
        // Notify callee that accept was acked
        UserSession *calleeSession = m_sessions->session(call->calleeSid);
        if (calleeSession) {
            QJsonObject acked;
            acked.insert(QStringLiteral("What"), QStringLiteral("acceptAcked"));
            acked.insert(QStringLiteral("leg"), call->calleeLeg);
            pushToSession(calleeSession, acked);
        }
    }

    QJsonObject resp;
    resp.insert(QStringLiteral("response"), QStringLiteral("ok"));
    session->wsSession()->sendResponse(requestId, resp);
    qCInfo(lcCall) << "AckAccept" << leg;
}

void CallManager::handleUpdateCall(UserSession *session, int requestId, const QJsonObject &payload)
{
    const QString leg = payload.value(QStringLiteral("leg")).toString();
    const QString sdp = payload.value(QStringLiteral("sdp")).toString();

    ServerCallSession *call = findCallByLeg(leg);
    if (!call) {
        QJsonObject resp;
        resp.insert(QStringLiteral("error"), QStringLiteral("unknown leg"));
        session->wsSession()->sendResponse(requestId, resp);
        return;
    }

    const bool isHoldSdp = sdp.contains(QStringLiteral("a=sendonly"))
                        || sdp.contains(QStringLiteral("a=inactive"));
    const bool isUnholdSdp = sdp.contains(QStringLiteral("a=sendrecv"));

    // Only the user who put the call on hold can unhold it
    if (!call->heldBy.isEmpty() && isUnholdSdp) {
        if (session->login() != call->heldBy) {
            qCWarning(lcCall) << "Reject unhold:" << session->login()
                             << "is not the one who held the call (" << call->heldBy << ")";
            QJsonObject resp;
            resp.insert(QStringLiteral("response"), QStringLiteral("ok"));
            session->wsSession()->sendResponse(requestId, resp);
            return;
        }
        qCInfo(lcCall) << "Call" << leg << "resumed from hold by" << session->login();
        call->heldBy.clear();
        call->state = ServerCallState::Connected;
    }

    // Cannot put on hold if already on hold
    if (!call->heldBy.isEmpty() && isHoldSdp) {
        qCWarning(lcCall) << "Reject hold:" << leg << "already held by" << call->heldBy;
        QJsonObject resp;
        resp.insert(QStringLiteral("response"), QStringLiteral("ok"));
        session->wsSession()->sendResponse(requestId, resp);
        return;
    }

    // Put on hold
    if (isHoldSdp && call->heldBy.isEmpty()) {
        call->heldBy = session->login();
        call->state = ServerCallState::Hold;
        qCInfo(lcCall) << "Call" << leg << "put on hold by" << session->login();
        if (m_holdMusic.isLoaded()) {
            qCInfo(lcCall) << "Hold music available:" << m_holdMusic.totalSamples() / m_holdMusic.sampleRate() << "s loop";
        }
    }

    // Forward SDP to the other party
    const QString otherLeg = (leg == call->callerLeg) ? call->calleeLeg : call->callerLeg;
    const QString otherSid = (leg == call->callerLeg) ? call->calleeSid : call->callerSid;
    UserSession *otherSession = m_sessions->session(otherSid);
    if (otherSession) {
        QJsonObject updated;
        updated.insert(QStringLiteral("What"), QStringLiteral("updated"));
        updated.insert(QStringLiteral("leg"), otherLeg);
        updated.insert(QStringLiteral("sdp"), sdp);
        pushToSession(otherSession, updated);
    }

    QJsonObject resp;
    resp.insert(QStringLiteral("response"), QStringLiteral("ok"));
    session->wsSession()->sendResponse(requestId, resp);
}

void CallManager::handleAcceptUpdate(UserSession *session, int requestId, const QJsonObject &payload)
{
    const QString leg = payload.value(QStringLiteral("leg")).toString();
    const QString sdp = payload.value(QStringLiteral("sdp")).toString();

    ServerCallSession *call = findCallByLeg(leg);
    if (call) {
        const QString otherLeg = (leg == call->callerLeg) ? call->calleeLeg : call->callerLeg;
        const QString otherSid = (leg == call->callerLeg) ? call->calleeSid : call->callerSid;
        UserSession *otherSession = m_sessions->session(otherSid);
        if (otherSession) {
            QJsonObject accepted;
            accepted.insert(QStringLiteral("What"), QStringLiteral("updateAccepted"));
            accepted.insert(QStringLiteral("leg"), otherLeg);
            accepted.insert(QStringLiteral("sdp"), sdp);
            pushToSession(otherSession, accepted);
        }
    }

    QJsonObject resp;
    resp.insert(QStringLiteral("response"), QStringLiteral("ok"));
    session->wsSession()->sendResponse(requestId, resp);
}

void CallManager::handleTransfer(UserSession *session, int requestId, const QJsonObject &payload)
{
    const QString leg = payload.value(QStringLiteral("leg")).toString();
    const QString address = payload.value(QStringLiteral("address")).toString();

    QJsonObject resp;
    resp.insert(QStringLiteral("response"), QStringLiteral("ok"));
    session->wsSession()->sendResponse(requestId, resp);

    // In a full implementation, this would initiate a new call to the target
    // and bridge the audio. For now, just notify.
    qCInfo(lcCall) << "Transfer" << leg << "to" << address;
}

} // namespace itl
