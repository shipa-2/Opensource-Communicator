#pragma once

#include "CallSession.h"
#include "HoldMusicPlayer.h"

#include <QObject>
#include <QHash>

namespace itl {

class UserSession;
class SessionManager;

class CallManager : public QObject {
    Q_OBJECT

public:
    explicit CallManager(SessionManager *sessions, QObject *parent = nullptr);

    void handleProvisionCall(UserSession *session, int requestId, const QJsonObject &payload);
    void handleStartCall(UserSession *session, int requestId, const QJsonObject &payload);
    void handleAcceptCall(UserSession *session, int requestId, const QJsonObject &payload);
    void handleRejectCall(UserSession *session, int requestId, const QJsonObject &payload);
    void handleCancelCall(UserSession *session, int requestId, const QJsonObject &payload);
    void handleDisconnectCall(UserSession *session, int requestId, const QJsonObject &payload);
    void handleAckAccept(UserSession *session, int requestId, const QJsonObject &payload);
    void handleUpdateCall(UserSession *session, int requestId, const QJsonObject &payload);
    void handleAcceptUpdate(UserSession *session, int requestId, const QJsonObject &payload);
    void handleTransfer(UserSession *session, int requestId, const QJsonObject &payload);

private:
    void pushToSession(UserSession *target, const QJsonObject &payload);
    QString generateLegId();
    ServerCallSession *findCallByLeg(const QString &leg);
    ServerCallSession *findCallByPeer(const QString &peer);

    SessionManager *m_sessions;
    QHash<QString, ServerCallSession> m_calls;
    int m_legCounter = 0;
    HoldMusicPlayer m_holdMusic;
};

} // namespace itl
