#include "SessionManager.h"

#include <QDateTime>
#include <QLoggingCategory>
#include <QUuid>

Q_LOGGING_CATEGORY(lcSession, "server.session")

namespace itl {

SessionManager::SessionManager(QObject *parent)
    : QObject(parent)
{
    connect(&m_cleanupTimer, &QTimer::timeout, this, &SessionManager::cleanupExpiredSessions);
    m_cleanupTimer.start(30000);
}

QString SessionManager::createSession()
{
    const QString sid = QUuid::createUuid().toString(QUuid::WithoutBraces);

    auto *session = new UserSession(sid, this);
    session->touch();
    m_sessions.insert(sid, session);

    qCInfo(lcSession) << "Session created:" << sid;
    emit sessionCreated(sid);
    return sid;
}

void SessionManager::destroySession(const QString &sid)
{
    UserSession *session = m_sessions.take(sid);
    if (!session) {
        return;
    }

    const QString login = session->login();
    const QString domain = session->domain();
    bool hasAnotherSession = false;
    if (!login.isEmpty() && !domain.isEmpty()) {
        const QString user = login.section(QLatin1Char('@'), 0, 0);
        for (UserSession *other : m_sessions) {
            if (other && other->domain().compare(domain, Qt::CaseInsensitive) == 0
                && other->login().section(QLatin1Char('@'), 0, 0)
                       .compare(user, Qt::CaseInsensitive) == 0) {
                hasAnotherSession = true;
                break;
            }
        }
    }

    qCInfo(lcSession) << "Session destroyed:" << sid << session->login();
    emit sessionDestroyed(sid);
    if (!login.isEmpty() && !domain.isEmpty() && !hasAnotherSession) {
        emit userWentOffline(login, domain);
    }
    session->deleteLater();
}

UserSession *SessionManager::session(const QString &sid) const
{
    return m_sessions.value(sid);
}

QList<UserSession *> SessionManager::sessionsForLogin(const QString &login) const
{
    QList<UserSession *> result;
    // Match by user part (before @) — handles "shipa" matching "shipa@localhost"
    const QString needle = login.section(QLatin1Char('@'), 0, 0).toLower();
    if (needle.isEmpty()) {
        return result;
    }
    for (UserSession *s : m_sessions) {
        const QString sessionUser = s->login().section(QLatin1Char('@'), 0, 0).toLower();
        if (sessionUser == needle && !s->login().isEmpty()) {
            result.append(s);
        }
    }
    return result;
}

QList<UserSession *> SessionManager::sessionsForDomain(const QString &domain) const
{
    QList<UserSession *> result;
    const QString lower = domain.toLower();
    for (UserSession *s : m_sessions) {
        if (s->domain().toLower() == lower && !s->login().isEmpty()) {
            result.append(s);
        }
    }
    return result;
}

int SessionManager::sessionCountForLogin(const QString &login) const
{
    return sessionsForLogin(login).size();
}

void SessionManager::cleanupExpiredSessions()
{
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    const qint64 timeoutMs = static_cast<qint64>(m_sessionTimeoutSec) * 1000;
    QStringList expired;

    for (auto it = m_sessions.cbegin(); it != m_sessions.cend(); ++it) {
        if (it.value()->login().isEmpty() && (now - it.value()->lastActivityMs()) > 30000) {
            // Unauthenticated session — 30s timeout
            expired.append(it.key());
        } else if (!it.value()->login().isEmpty() && (now - it.value()->lastActivityMs()) > timeoutMs) {
            expired.append(it.key());
        }
    }

    for (const QString &sid : expired) {
        qCInfo(lcSession) << "Expired session:" << sid;
        destroySession(sid);
    }
}

} // namespace itl
