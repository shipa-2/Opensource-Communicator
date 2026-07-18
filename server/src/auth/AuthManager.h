#pragma once

#include <QObject>
#include <QString>

namespace itl {

class Database;

class AuthManager : public QObject {
    Q_OBJECT

public:
    explicit AuthManager(Database *db, QObject *parent = nullptr);

    void setDemoOnly(bool demo) { m_demoOnly = demo; }
    void setDefaultPartner(const QString &partner) { m_defaultPartner = partner; }

    struct AuthResult {
        bool success = false;
        int userId = -1;
        QString domain;
        QString role;
        QString partner;
        QString displayName;
        QString error;
    };

    AuthResult authenticate(const QString &login, const QString &password);
    QString hashPassword(const QString &password);

private:
    Database *m_db;
    bool m_demoOnly = false;
    QString m_defaultPartner;
};

} // namespace itl
