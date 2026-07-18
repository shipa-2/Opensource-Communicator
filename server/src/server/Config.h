#pragma once

#include <QJsonObject>
#include <QString>

namespace itl {

struct ServerConfig {
    struct {
        QString host = QStringLiteral("0.0.0.0");
        quint16 port = 8443;
        QString sslCert;
        QString sslKey;
    } server;

    struct {
        QString host = QStringLiteral("localhost");
        quint16 port = 5432;
        QString database = QStringLiteral("communicator");
        QString user = QStringLiteral("communicator");
        QString password;
    } database;

    struct {
        int sessionTimeoutSec = 3600;
        int maxSessionsPerUser = 5;
    } auth;

    struct {
        QString level = QStringLiteral("info");
    } logging;

    // Feature flags
    QString partner;
    bool demoOnly = false;
    bool videoEnabled = false;
    bool bigMessages = false;
    bool inCallStatus = false;
    bool serverContacts = false;

    static ServerConfig load(const QString &filePath);
    void applyLogging() const;
};

} // namespace itl
