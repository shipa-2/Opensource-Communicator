#include "Server.h"
#include "../network/WsServer.h"
#include "../network/WsSession.h"
#include "../session/SessionManager.h"
#include "../auth/AuthManager.h"
#include "../db/Database.h"
#include "../protocol/CommandDispatcher.h"
#include "../im/ImManager.h"
#include "../im/PresenceManager.h"
#include "../calls/CallManager.h"
#include "../addressbook/AddressBookManager.h"
#include "../history/HistoryManager.h"
#include "../sms/SmsManager.h"
#include "../conference/ConferenceManager.h"

#include <QLoggingCategory>

Q_LOGGING_CATEGORY(lcServer, "server.main")

namespace itl {

Server::Server(QObject *parent)
    : QObject(parent)
{
}

Server::~Server()
{
    stop();
}

bool Server::start(const ServerConfig &config)
{
    qCInfo(lcServer) << "Starting OpenSource Communicator Server v0.1.0";

    m_config = config;

    if (config.demoOnly) {
        qCInfo(lcServer) << "Demo mode: only demo/demo login allowed";
    }
    if (config.videoEnabled) {
        qCInfo(lcServer) << "Video support: enabled (WIP)";
    }
    if (config.bigMessages) {
        qCInfo(lcServer) << "Big messages: enabled (extended size limit)";
    }
    if (config.onCallStatus) {
        qCInfo(lcServer) << "On-call status: enabled";
    }
    if (config.serverContacts) {
        qCInfo(lcServer) << "Server contacts: enabled";
    }
    if (!config.partner.isEmpty()) {
        qCInfo(lcServer) << "Partner:" << config.partner;
    }

    // Database
    m_db = new Database(this);
    if (m_db->initialize(config.database.host, config.database.port,
                        config.database.database, config.database.user,
                        config.database.password)) {
        m_db->ensureSchema();
        qCInfo(lcServer) << "Database connected";
    } else {
        qCWarning(lcServer) << "Database connection failed, running without persistence";
    }

    wireModules();

    // WebSocket server
    m_wsServer = new WsServer(this);
    connect(m_wsServer, &WsServer::newSession, this, &Server::onNewSession);
    connect(m_wsServer, &WsServer::sessionClosed, this, &Server::onSessionClosed);

    if (!config.server.sslCert.isEmpty() && !config.server.sslKey.isEmpty()) {
        if (!m_wsServer->startTls(config.server.host, config.server.port,
                                  config.server.sslCert, config.server.sslKey)) {
            return false;
        }
    } else {
        if (!m_wsServer->start(config.server.host, config.server.port)) {
            return false;
        }
    }

    qCInfo(lcServer) << "Server started successfully";
    return true;
}

void Server::stop()
{
    if (m_wsServer) {
        m_wsServer->stop();
    }
}

void Server::wireModules()
{
    m_sessionMgr = new SessionManager(this);
    m_sessionMgr->setMaxSessionsPerUser(5);
    m_sessionMgr->setSessionTimeoutSec(3600);

    m_authMgr = new AuthManager(m_db, this);
    m_authMgr->setDemoOnly(m_config.demoOnly);
    m_authMgr->setAllowDomainAliases(
        m_config.server.host == QStringLiteral("0.0.0.0")
        || m_config.server.host == QStringLiteral("::"));
    if (!m_config.partner.isEmpty()) {
        m_authMgr->setDefaultPartner(m_config.partner);
    }
    m_imMgr = new ImManager(m_db, this);
    m_imMgr->setSessionManager(m_sessionMgr);
    m_presenceMgr = new PresenceManager(m_sessionMgr, this);
    m_callMgr = new CallManager(m_sessionMgr, this);
    m_abMgr = new AddressBookManager(m_db, this);
    m_histMgr = new HistoryManager(m_db, this);
    m_smsMgr = new SmsManager(this);
    m_confMgr = new ConferenceManager(this);

    m_dispatcher = new CommandDispatcher(this);
    m_dispatcher->setSessionManager(m_sessionMgr);
    m_dispatcher->setAuthManager(m_authMgr);
    m_dispatcher->setDatabase(m_db);
    m_dispatcher->setImManager(m_imMgr);
    m_dispatcher->setPresenceManager(m_presenceMgr);
    m_dispatcher->setCallManager(m_callMgr);
    m_dispatcher->setAddressBookManager(m_abMgr);
    m_dispatcher->setHistoryManager(m_histMgr);
    m_dispatcher->setSmsManager(m_smsMgr);
    m_dispatcher->setConferenceManager(m_confMgr);
    if (!m_config.partner.isEmpty()) {
        m_dispatcher->setConfigPartner(m_config.partner);
    }
    m_dispatcher->setVideoEnabled(m_config.videoEnabled);

    qCInfo(lcServer) << "All modules wired";
}

void Server::onNewSession(WsSession *session)
{
    connect(session, &WsSession::helloReceived, this, [this, session](const QJsonObject &hello) {
        m_dispatcher->handleHandshake(session, hello);
    });

    connect(session, &WsSession::textMessageReceived, this, [this, session](const QString &msg) {
        const QJsonObject payload = QJsonDocument::fromJson(msg.toUtf8()).object();
        if (!payload.isEmpty()) {
            m_dispatcher->handlePayload(session, payload);
        }
    });
}

void Server::onSessionClosed(WsSession *session)
{
    UserSession *userSession = session->userSession();
    if (userSession) {
        qCInfo(lcServer) << "Session closed for" << userSession->login();
        m_sessionMgr->destroySession(userSession->sid());
    }
}

} // namespace itl
