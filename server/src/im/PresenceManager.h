#pragma once

#include <QObject>
#include <QString>

namespace itl {

class UserSession;
class SessionManager;

class PresenceManager : public QObject {
    Q_OBJECT

public:
    explicit PresenceManager(SessionManager *sessions, QObject *parent = nullptr);

    void broadcastPresence(UserSession *sender, const QString &status);
    void sendInitialPresence(UserSession *target);

private:
    SessionManager *m_sessions;
};

} // namespace itl
