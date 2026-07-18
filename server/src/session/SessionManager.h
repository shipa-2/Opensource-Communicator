#pragma once

#include "UserSession.h"

#include <QHash>
#include <QObject>
#include <QString>
#include <QTimer>

namespace itl {

class SessionManager : public QObject {
    Q_OBJECT

public:
    explicit SessionManager(QObject *parent = nullptr);

    QString createSession();
    void destroySession(const QString &sid);
    UserSession *session(const QString &sid) const;
    QList<UserSession *> sessionsForLogin(const QString &login) const;
    QList<UserSession *> sessionsForDomain(const QString &domain) const;
    int sessionCountForLogin(const QString &login) const;
    int maxSessionsPerUser() const { return m_maxSessionsPerUser; }
    void setMaxSessionsPerUser(int max) { m_maxSessionsPerUser = max; }
    void setSessionTimeoutSec(int sec) { m_sessionTimeoutSec = sec; }

signals:
    void sessionCreated(const QString &sid);
    void sessionDestroyed(const QString &sid);
    void userWentOffline(const QString &login, const QString &domain);

private slots:
    void cleanupExpiredSessions();

private:
    QHash<QString, UserSession *> m_sessions;
    QTimer m_cleanupTimer;
    int m_maxSessionsPerUser = 5;
    int m_sessionTimeoutSec = 3600;
};

} // namespace itl
