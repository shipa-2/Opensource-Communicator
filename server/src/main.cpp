#include "server/Server.h"
#include "server/Config.h"
#include "db/Database.h"

#include <QCoreApplication>
#include <QCommandLineParser>
#include <QCryptographicHash>
#include <QLoggingCategory>
#include <iostream>

Q_LOGGING_CATEGORY(lcMain, "server")

static QString hashPassword(const QString &password)
{
    return QString::fromUtf8(
        QCryptographicHash::hash(password.toUtf8(), QCryptographicHash::Sha256).toHex());
}

static bool interactiveNewUser(const itl::ServerConfig &config)
{
    std::cout << "\n=== Create New User ===\n\n";

    std::string login, password, domain, displayName;
    int roleChoice = 0;

    std::cout << "Login (user@domain): ";
    std::getline(std::cin, login);
    if (login.empty()) {
        std::cerr << "Login cannot be empty.\n";
        return false;
    }

    std::cout << "Display name: ";
    std::getline(std::cin, displayName);

    std::cout << "Internal extension (ext, e.g. 702, leave empty to skip): ";
    std::string ext;
    std::getline(std::cin, ext);

    std::cout << "Mobile phone (e.g. +7..., leave empty to skip): ";
    std::string phone;
    std::getline(std::cin, phone);

    std::cout << "Password: ";
    std::getline(std::cin, password);
    if (password.empty()) {
        std::cerr << "Password cannot be empty.\n";
        return false;
    }

    std::cout << "Domain (leave empty to extract from login): ";
    std::getline(std::cin, domain);

    std::cout << "Role:\n  1) User\n  2) Admin\n  3) RestrictedUser\nChoice [1]: ";
    std::string roleStr;
    std::getline(std::cin, roleStr);
    roleChoice = roleStr.empty() ? 1 : std::stoi(roleStr);

    QString qLogin = QString::fromStdString(login);
    QString qPassword = QString::fromStdString(password);
    QString qDisplayName = QString::fromStdString(displayName);
    QString qDomain = QString::fromStdString(domain);
    QString qPhone = QString::fromStdString(phone);
    QString qExt = QString::fromStdString(ext);
    QString qRole;

    switch (roleChoice) {
    case 2:  qRole = QStringLiteral("Admin"); break;
    case 3:  qRole = QStringLiteral("RestrictedUser"); break;
    default: qRole = QStringLiteral("User"); break;
    }

    if (qDomain.isEmpty()) {
        const int at = qLogin.indexOf(QLatin1Char('@'));
        qDomain = at > 0 ? qLogin.mid(at + 1) : QStringLiteral("default");
    }

    itl::Database db;
    if (!db.initialize(config.database.host, config.database.port,
                      config.database.database, config.database.user,
                      config.database.password)) {
        std::cerr << "Cannot connect to database.\n";
        return false;
    }
    db.ensureSchema();

    if (db.userExists(qLogin)) {
        std::cerr << "User '" << qLogin.toStdString() << "' already exists.\n";
        return false;
    }

    if (!db.createUser(qLogin, hashPassword(qPassword), qDomain, qDisplayName, qRole, qPhone, qExt)) {
        std::cerr << "Failed to create user.\n";
        return false;
    }

    std::cout << "\nUser '" << qLogin.toStdString() << "' created successfully.\n";
    std::cout << "  Domain: " << qDomain.toStdString() << "\n";
    std::cout << "  Role:   " << qRole.toStdString() << "\n\n";
    return true;
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("communicator-server"));
    QCoreApplication::setApplicationVersion(QStringLiteral("0.1.0"));

    QCommandLineParser parser;
    parser.setApplicationDescription(
        QStringLiteral("OpenSource Communicator Server\n\n"
                       "Self-hosted signaling server for OpenSource Communicator client.\n"
                       "Implements WebSocket protocol compatible with ITooLabs/Megafon PBX."));
    parser.addHelpOption();
    parser.addVersionOption();

    parser.addOption({QStringLiteral("config"), QStringLiteral("Path to config file (JSON)"),
                      QStringLiteral("path")});
    parser.addOption({QStringLiteral("ip"), QStringLiteral("IP address to bind to (default: 0.0.0.0)"),
                      QStringLiteral("address")});
    parser.addOption({QStringLiteral("port"), QStringLiteral("Port to listen on (default: 8443)"),
                      QStringLiteral("number"), QStringLiteral("8443")});
    parser.addOption({QStringLiteral("partner"), QStringLiteral("Partner identifier (e.g. megafon, mtt)"),
                      QStringLiteral("name")});
    parser.addOption({QStringLiteral("demo"), QStringLiteral("Demo mode: only demo/demo login allowed")});
    parser.addOption({QStringLiteral("allowvideo"), QStringLiteral("Enable video call support (WIP)")});
    parser.addOption({QStringLiteral("bigmessages"),
                      QStringLiteral("Increase max message size limit (for file transfers)")});
    parser.addOption({QStringLiteral("incall"),
                      QStringLiteral("Allow users to set 'on call' presence status")});
    parser.addOption({QStringLiteral("servercontacts"),
                      QStringLiteral("Enable server-side address book (personal contacts on server)")});
    parser.addOption({QStringLiteral("newuser"),
                      QStringLiteral("Interactive CLI to create a new user in the database")});
    parser.addOption({QStringLiteral("deluser"),
                      QStringLiteral("Delete a user by login (prompts for confirmation)"),
                      QStringLiteral("login")});
    parser.addOption({QStringLiteral("listusers"),
                      QStringLiteral("List all users in the database")});

    parser.process(app);

    // ─── Config file ───
    QString configPath = parser.value(QStringLiteral("config"));
    if (configPath.isEmpty()) {
        configPath = QStringLiteral("/etc/communicator-server/server.conf");
    }
    itl::ServerConfig config = itl::ServerConfig::load(configPath);

    // ─── CLI overrides ───
    if (parser.isSet(QStringLiteral("ip"))) {
        config.server.host = parser.value(QStringLiteral("ip"));
    }
    if (parser.isSet(QStringLiteral("port"))) {
        bool ok = false;
        quint16 p = parser.value(QStringLiteral("port")).toUShort(&ok);
        if (ok) {
            config.server.port = p;
        }
    }
    if (parser.isSet(QStringLiteral("partner"))) {
        config.partner = parser.value(QStringLiteral("partner"));
    }
    if (parser.isSet(QStringLiteral("demo"))) {
        config.demoOnly = true;
    }
    if (parser.isSet(QStringLiteral("allowvideo"))) {
        config.videoEnabled = true;
    }
    if (parser.isSet(QStringLiteral("bigmessages"))) {
        config.bigMessages = true;
    }
    if (parser.isSet(QStringLiteral("incall"))) {
        config.inCallStatus = true;
    }
    if (parser.isSet(QStringLiteral("servercontacts"))) {
        config.serverContacts = true;
    }

    // ─── List users mode (exits after) ───
    if (parser.isSet(QStringLiteral("listusers"))) {
        config.applyLogging();
        itl::Database db;
        if (!db.initialize(config.database.host, config.database.port,
                          config.database.database, config.database.user,
                          config.database.password)) {
            std::cerr << "Cannot connect to database.\n";
            return 1;
        }
        const QList<QJsonObject> users = db.listUsers();
        if (users.isEmpty()) {
            std::cout << "No users found.\n";
            return 0;
        }
        std::cout << "\n  LOGIN                    DISPLAY NAME         DOMAIN          ROLE\n";
        std::cout << "  " << std::string(74, '-') << "\n";
        for (const QJsonObject &u : users) {
            std::string login = u.value(QStringLiteral("login")).toString().toStdString();
            std::string name = u.value(QStringLiteral("displayName")).toString().toStdString();
            std::string domain = u.value(QStringLiteral("domain")).toString().toStdString();
            std::string role = u.value(QStringLiteral("role")).toString().toStdString();
            printf("  %-24s %-20s %-15s %s\n", login.c_str(), name.c_str(),
                   domain.c_str(), role.c_str());
        }
        std::cout << "\nTotal: " << users.size() << " user(s)\n";
        return 0;
    }

    // ─── Delete user mode (exits after) ───
    if (parser.isSet(QStringLiteral("deluser"))) {
        config.applyLogging();
        const QString login = parser.value(QStringLiteral("deluser"));
        if (login.isEmpty()) {
            std::cerr << "Login cannot be empty.\n";
            return 1;
        }
        itl::Database db;
        if (!db.initialize(config.database.host, config.database.port,
                          config.database.database, config.database.user,
                          config.database.password)) {
            std::cerr << "Cannot connect to database.\n";
            return 1;
        }
        if (!db.userExists(login)) {
            std::cerr << "User '" << login.toStdString() << "' not found.\n";
            return 1;
        }
        std::cout << "Delete user '" << login.toStdString() << "'? [y/N]: ";
        std::string confirm;
        std::getline(std::cin, confirm);
        if (confirm != "y" && confirm != "Y") {
            std::cout << "Cancelled.\n";
            return 0;
        }
        if (db.deleteUser(login)) {
            std::cout << "User '" << login.toStdString() << "' deleted.\n";
        } else {
            std::cerr << "Failed to delete user.\n";
            return 1;
        }
        return 0;
    }

    // ─── New user mode (exits after) ───
    if (parser.isSet(QStringLiteral("newuser"))) {
        config.applyLogging();
        bool ok = interactiveNewUser(config);
        return ok ? 0 : 1;
    }

    // ─── Start server ───
    config.applyLogging();

    itl::Server server;
    if (!server.start(config)) {
        qCCritical(lcMain) << "Failed to start server";
        return 1;
    }

    qCInfo(lcMain) << "Server running. Press Ctrl+C to stop.";
    return app.exec();
}
