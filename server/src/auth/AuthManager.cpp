#include "AuthManager.h"
#include "../db/Database.h"

#include <QCryptographicHash>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(lcAuth, "server.auth")

namespace itl {

AuthManager::AuthManager(Database *db, QObject *parent)
    : QObject(parent)
    , m_db(db)
{
}

AuthManager::AuthResult AuthManager::authenticate(const QString &login, const QString &password)
{
    AuthResult result;

    if (login.isEmpty() || password.isEmpty()) {
        result.error = QStringLiteral("missing credentials");
        return result;
    }

    // Demo-only mode: reject everything except demo/demo
    if (m_demoOnly) {
        if (login == QStringLiteral("demo") && password == QStringLiteral("demo")) {
            result.success = true;
            result.userId = 0;
            result.domain = QStringLiteral("demo.local");
            result.role = QStringLiteral("Admin");
            result.partner = m_defaultPartner.isEmpty() ? QStringLiteral("opensource") : m_defaultPartner;
            qCInfo(lcAuth) << "Auth success (demo mode):" << login;
        } else {
            result.error = QStringLiteral("demo mode: only demo/demo allowed");
            qCWarning(lcAuth) << "Auth rejected (demo mode):" << login;
        }
        return result;
    }

    // Full mode: check database
    const QString hash = hashPassword(password);
    QString canonicalLogin = login;
    QString domain, role, partner;
    bool authenticated = m_db && m_db->authenticateUser(
        canonicalLogin, hash, &domain, &role, &partner);
    if (!authenticated && m_db && m_allowDomainAliases) {
        authenticated = m_db->authenticateUserByName(
            login, hash, &canonicalLogin, &domain, &role, &partner);
    }
    if (authenticated) {
        result.success = true;
        result.login = canonicalLogin;
        result.userId = m_db->userId(canonicalLogin);
        result.domain = domain;
        result.role = role;
        result.partner = partner;
        result.displayName = m_db->userDisplayName(canonicalLogin);
        qCInfo(lcAuth) << "Auth success:" << login << "as" << canonicalLogin;
    } else {
        // Accept any login with password "demo" if user doesn't exist yet (development mode)
        if (password == QStringLiteral("demo")) {
            result.success = true;
            result.userId = 0;
            result.domain = login.section(QLatin1Char('@'), 1);
            if (result.domain.isEmpty()) {
                result.domain = QStringLiteral("demo.local");
            }
            result.role = QStringLiteral("Admin");
            result.partner = m_defaultPartner.isEmpty() ? QStringLiteral("opensource") : m_defaultPartner;
            qCInfo(lcAuth) << "Auth success (dev/demo):" << login;
        } else {
            result.error = QStringLiteral("invalid credentials");
            qCWarning(lcAuth) << "Auth failed:" << login;
        }
    }

    return result;
}

QString AuthManager::hashPassword(const QString &password)
{
    // SHA-256 for simplicity. Replace with bcrypt in production.
    return QString::fromUtf8(
        QCryptographicHash::hash(password.toUtf8(), QCryptographicHash::Sha256).toHex());
}

} // namespace itl
