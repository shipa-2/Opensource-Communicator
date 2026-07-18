#pragma once

#include "Config.h"
#include "../network/WsSession.h"

#include <QObject>

namespace itl {

class WsServer;
class SessionManager;
class AuthManager;
class Database;
class CommandDispatcher;
class ImManager;
class PresenceManager;
class CallManager;
class AddressBookManager;
class HistoryManager;
class SmsManager;
class ConferenceManager;

class Server : public QObject {
    Q_OBJECT

public:
    explicit Server(QObject *parent = nullptr);
    ~Server() override;

    bool start(const ServerConfig &config);
    void stop();

    const ServerConfig &config() const { return m_config; }

private slots:
    void onNewSession(WsSession *session);
    void onSessionClosed(WsSession *session);

private:
    void wireModules();

    WsServer *m_wsServer = nullptr;
    SessionManager *m_sessionMgr = nullptr;
    AuthManager *m_authMgr = nullptr;
    Database *m_db = nullptr;
    CommandDispatcher *m_dispatcher = nullptr;
    ImManager *m_imMgr = nullptr;
    PresenceManager *m_presenceMgr = nullptr;
    CallManager *m_callMgr = nullptr;
    AddressBookManager *m_abMgr = nullptr;
    HistoryManager *m_histMgr = nullptr;
    SmsManager *m_smsMgr = nullptr;
    ConferenceManager *m_confMgr = nullptr;
    ServerConfig m_config;
};

} // namespace itl
