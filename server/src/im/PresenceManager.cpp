#include "PresenceManager.h"
#include "../network/WsSession.h"
#include "../session/SessionManager.h"
#include "../session/UserSession.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(lcPresence, "server.presence")

namespace itl {

PresenceManager::PresenceManager(SessionManager *sessions, QObject *parent)
    : QObject(parent)
    , m_sessions(sessions)
{
}

static QString bareLogin(const QString &login)
{
    const int at = login.indexOf(QLatin1Char('@'));
    return at > 0 ? login.left(at) : login;
}

void PresenceManager::broadcastPresence(UserSession *sender, const QString &status)
{
    if (!sender || sender->domain().isEmpty()) {
        return;
    }

    const QList<UserSession *> domainSessions = m_sessions->sessionsForDomain(sender->domain());

    QJsonObject entry;
    entry.insert(QStringLiteral("voice"), status);
    entry.insert(QStringLiteral("im"), QJsonObject{{QStringLiteral("status"), status}});

    QJsonObject batch;
    batch.insert(bareLogin(sender->login()), entry);

    QJsonObject payload;
    payload.insert(QStringLiteral("What"), QStringLiteral("[PRESENCE]"));
    payload.insert(QStringLiteral("batch"), batch);

    int delivered = 0;
    for (UserSession *target : domainSessions) {
        if (target->login() == sender->login()) {
            continue;
        }
        if (target->wsSession() && target->isBound()) {
            target->wsSession()->sendPayload(payload);
            delivered++;
        }
    }

    qCInfo(lcPresence) << "Presence broadcast:" << sender->login() << status
                       << "to" << delivered << "peers";
}

void PresenceManager::sendInitialPresence(UserSession *target)
{
    if (!target || !m_sessions) {
        return;
    }

    const QList<UserSession *> domainSessions = m_sessions->sessionsForDomain(target->domain());

    QJsonObject batch;
    for (UserSession *other : domainSessions) {
        if (other->login() == target->login()) {
            continue;
        }
        const QString status = other->presenceStatus();
        if (status.isEmpty() || status == QStringLiteral("offline")) {
            continue;
        }

        QJsonObject entry;
        entry.insert(QStringLiteral("voice"), status);
        entry.insert(QStringLiteral("im"), QJsonObject{
            {QStringLiteral("status"), status},
        });
        batch.insert(bareLogin(other->login()), entry);
    }

    if (batch.isEmpty()) {
        return;
    }

    QJsonObject payload;
    payload.insert(QStringLiteral("What"), QStringLiteral("[PRESENCE]"));
    payload.insert(QStringLiteral("batch"), batch);

    target->wsSession()->sendPayload(payload);
    qCInfo(lcPresence) << "Initial presence sent to" << target->login()
                       << ":" << batch.size() << "online users";
}

} // namespace itl
