#include "Config.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLoggingCategory>
#include <QStringConverter>

Q_LOGGING_CATEGORY(lcConfig, "server.config")

namespace itl {

ServerConfig ServerConfig::load(const QString &filePath)
{
    ServerConfig config;

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qCWarning(lcConfig) << "Cannot open config file:" << filePath << "- using defaults";
        return config;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (doc.isNull() || !doc.isObject()) {
        qCWarning(lcConfig) << "Invalid config file:" << filePath;
        return config;
    }

    const QJsonObject root = doc.object();

    const QJsonObject server = root.value(QStringLiteral("server")).toObject();
    config.server.host = server.value(QStringLiteral("host")).toString(config.server.host);
    config.server.port = server.value(QStringLiteral("port")).toInt(config.server.port);
    config.server.sslCert = server.value(QStringLiteral("ssl_cert")).toString();
    config.server.sslKey = server.value(QStringLiteral("ssl_key")).toString();

    const QJsonObject db = root.value(QStringLiteral("database")).toObject();
    config.database.host = db.value(QStringLiteral("host")).toString(config.database.host);
    config.database.port = db.value(QStringLiteral("port")).toInt(config.database.port);
    config.database.database = db.value(QStringLiteral("name")).toString(config.database.database);
    config.database.user = db.value(QStringLiteral("user")).toString(config.database.user);
    config.database.password = db.value(QStringLiteral("password")).toString();

    const QJsonObject auth = root.value(QStringLiteral("auth")).toObject();
    config.auth.sessionTimeoutSec = auth.value(QStringLiteral("session_timeout_sec")).toInt(config.auth.sessionTimeoutSec);
    config.auth.maxSessionsPerUser = auth.value(QStringLiteral("max_sessions_per_user")).toInt(config.auth.maxSessionsPerUser);

    const QJsonObject log = root.value(QStringLiteral("logging")).toObject();
    config.logging.level = log.value(QStringLiteral("level")).toString(config.logging.level);

    qCInfo(lcConfig) << "Config loaded from" << filePath;
    return config;
}

void ServerConfig::applyLogging() const
{
    QByteArray rules;
    if (logging.level == QStringLiteral("debug")) {
        rules = "*.debug=true; qt.*.debug=false";
    } else if (logging.level == QStringLiteral("warning")) {
        rules = "*.warning=true; *.critical=true";
    } else if (logging.level == QStringLiteral("critical")) {
        rules = "*.critical=true";
    } else {
        rules = "*.info=true; *.warning=true; *.critical=true";
    }
    qputenv("QT_LOGGING_RULES", rules);
}

} // namespace itl
