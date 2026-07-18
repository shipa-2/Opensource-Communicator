#pragma once

#include "../protocol/ProtocolTypes.h"

#include <QObject>
#include <QString>

namespace itl {

class WsSession;
class UserSession;
class SessionManager;
class AuthManager;
class Database;
class ImManager;
class PresenceManager;
class CallManager;
class AddressBookManager;
class HistoryManager;
class SmsManager;
class ConferenceManager;

class CommandDispatcher : public QObject {
    Q_OBJECT

public:
    explicit CommandDispatcher(QObject *parent = nullptr);

    void setSessionManager(SessionManager *mgr) { m_sessionMgr = mgr; }
    void setAuthManager(AuthManager *mgr) { m_authMgr = mgr; }
    void setDatabase(Database *db) { m_db = db; }
    void setImManager(ImManager *mgr) { m_imMgr = mgr; }
    void setPresenceManager(PresenceManager *mgr) { m_presenceMgr = mgr; }
    void setCallManager(CallManager *mgr) { m_callMgr = mgr; }
    void setAddressBookManager(AddressBookManager *mgr) { m_abMgr = mgr; }
    void setHistoryManager(HistoryManager *mgr) { m_histMgr = mgr; }
    void setSmsManager(SmsManager *mgr) { m_smsMgr = mgr; }
    void setConferenceManager(ConferenceManager *mgr) { m_confMgr = mgr; }
    void setConfigPartner(const QString &partner) { m_configPartner = partner; }
    void setVideoEnabled(bool enabled) { m_videoEnabled = enabled; }

    void handleHandshake(WsSession *ws, const QJsonObject &hello);
    void handlePayload(WsSession *ws, const QJsonObject &payload);

signals:
    void sessionAuthenticated(WsSession *ws);

private:
    void handleCommand(WsSession *ws, UserSession *session, const QJsonObject &payload);
    void sendResponse(WsSession *ws, int requestId, const QJsonObject &response);

    SessionManager *m_sessionMgr = nullptr;
    AuthManager *m_authMgr = nullptr;
    Database *m_db = nullptr;
    ImManager *m_imMgr = nullptr;
    PresenceManager *m_presenceMgr = nullptr;
    CallManager *m_callMgr = nullptr;
    AddressBookManager *m_abMgr = nullptr;
    HistoryManager *m_histMgr = nullptr;
    SmsManager *m_smsMgr = nullptr;
    ConferenceManager *m_confMgr = nullptr;
    QString m_configPartner;
    bool m_videoEnabled = false;
};

} // namespace itl
