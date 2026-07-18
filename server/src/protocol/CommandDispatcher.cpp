#include "CommandDispatcher.h"
#include "../auth/AuthManager.h"
#include "../db/Database.h"
#include "../im/ImManager.h"
#include "../im/PresenceManager.h"
#include "../calls/CallManager.h"
#include "../addressbook/AddressBookManager.h"
#include "../history/HistoryManager.h"
#include "../sms/SmsManager.h"
#include "../conference/ConferenceManager.h"
#include "../network/WsSession.h"
#include "../session/SessionManager.h"
#include "../session/UserSession.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QLoggingCategory>
#include <QUuid>

Q_LOGGING_CATEGORY(lcDispatch, "server.dispatch")

namespace itl {

CommandDispatcher::CommandDispatcher(QObject *parent)
    : QObject(parent)
{
}

void CommandDispatcher::handleHandshake(WsSession *ws, const QJsonObject &hello)
{
    const QString task = hello.value(QStringLiteral("task")).toString();
    if (task != QStringLiteral("Main")) {
        qCWarning(lcDispatch) << "Unexpected task in hello:" << task;
        return;
    }

    // Create session
    const QString sid = m_sessionMgr->createSession();
    UserSession *session = m_sessionMgr->session(sid);
    session->setWsSession(ws);
    ws->setUserSession(session);
    ws->setHandshakeComplete(true);

    // Send hello response
    QJsonObject response;
    response.insert(QStringLiteral("status"), QStringLiteral("ok"));
    response.insert(QStringLiteral("sid"), sid);
    ws->sendJson(response);

    qCInfo(lcDispatch) << "Hello handshake completed, sid:" << sid;
}

void CommandDispatcher::handlePayload(WsSession *ws, const QJsonObject &payload)
{
    UserSession *session = ws->userSession();
    if (!session) {
        qCWarning(lcDispatch) << "Payload from unauthenticated session";
        return;
    }

    session->touch();
    handleCommand(ws, session, payload);
}

void CommandDispatcher::handleCommand(WsSession *ws, UserSession *session, const QJsonObject &payload)
{
    const int id = payload.value(QStringLiteral("id")).toInt(-1);

    // Debug: dump full payload structure
    qCInfo(lcDispatch) << "RAW payload:" << QJsonDocument(payload).toJson(QJsonDocument::Compact);

    // Client wraps command in nested "" objects:
    // {"What":"request","id":0,"":{ "": "login", "username":"...", "password":"..." }}
    // or fire-and-forget: { "": "login", "username":"..." }
    // We need to unwrap to get the actual command object.
    const QJsonValue outerEmpty = payload.value(QString::fromUtf8(kEmptyKey));
    QJsonObject cmdObj;
    if (outerEmpty.isObject()) {
        cmdObj = outerEmpty.toObject();
    } else {
        cmdObj = payload;
    }

    qCInfo(lcDispatch) << "UNWRAPPED cmdObj:" << QJsonDocument(cmdObj).toJson(QJsonDocument::Compact);

    const QString command = cmdObj.value(QString::fromUtf8(kEmptyKey)).toString();

    qCInfo(lcDispatch) << "Command:" << command << "from" << session->login() << "id:" << id;

    // ─── Auth ───
    if (command == QStringLiteral("login")) {
        const QString username = cmdObj.value(QStringLiteral("username")).toString();
        const QString password = cmdObj.value(QStringLiteral("password")).toString();
        const QString clientPartner = cmdObj.value(QStringLiteral("partner")).toString();

        // Partner filter: reject if server has --partner set and client doesn't match
        if (!m_configPartner.isEmpty() && !clientPartner.isEmpty()
            && clientPartner.toLower() != m_configPartner.toLower()) {
            QJsonObject resp;
            resp.insert(QStringLiteral("error"), QStringLiteral("partner mismatch"));
            sendResponse(ws, id, resp);
            qCWarning(lcDispatch) << "Rejected login: partner mismatch, expected"
                                  << m_configPartner << "got" << clientPartner;
            return;
        }

        auto result = m_authMgr->authenticate(username, password);

        QJsonObject resp;
        if (result.success) {
            session->setLogin(result.login.isEmpty() ? username : result.login);
            session->setDomain(result.domain);
            session->setRole(result.role);
            session->setPartner(clientPartner.isEmpty() ? result.partner : clientPartner);
            session->setUserId(result.userId);

            resp.insert(QStringLiteral("userRole"), result.role);
            resp.insert(QStringLiteral("partner"), session->partner());
            resp.insert(QStringLiteral("domain"), result.domain);
            resp.insert(QStringLiteral("userId"), result.userId);
            resp.insert(QStringLiteral("displayName"), result.displayName);
        } else {
            resp.insert(QStringLiteral("error"), result.error);
        }

        sendResponse(ws, id, resp);
        emit sessionAuthenticated(ws);
        return;
    }

    if (command == QStringLiteral("bye")) {
        ws->sendBye();
        m_sessionMgr->destroySession(session->sid());
        return;
    }

    // ─── Bind / BindIM ───
    if (command == QStringLiteral("Bind")) {
        session->setBound(true);
        QJsonObject resp;
        resp.insert(QStringLiteral("response"), QStringLiteral("ok"));
        sendResponse(ws, id, resp);
        qCInfo(lcDispatch) << "Bind OK for" << session->login();
        return;
    }

    if (command == QStringLiteral("BindIM")) {
        session->setImBound(true);
        // Send [IM_CONTACTS] with unread counts
        if (m_imMgr) {
            m_imMgr->sendUnreadContacts(session);
        }
        QJsonObject resp;
        resp.insert(QStringLiteral("response"), QStringLiteral("ok"));
        sendResponse(ws, id, resp);
        qCInfo(lcDispatch) << "BindIM OK for" << session->login();
        return;
    }

    // ─── Presence ───
    if (command == QStringLiteral("SetPresence")) {
        const QJsonObject presence = cmdObj.value(QStringLiteral("presence")).toObject();
        const QString status = presence.value(QStringLiteral("status")).toString();
        session->setPresenceStatus(status);
        if (m_presenceMgr) {
            // Send current presence of all other online users to this session
            m_presenceMgr->sendInitialPresence(session);
            // Broadcast this user's presence to all others
            m_presenceMgr->broadcastPresence(session, status);
        }
        if (m_db) {
            m_db->updateUserPresence(session->login(), status);
        }
        QJsonObject resp;
        resp.insert(QStringLiteral("response"), QStringLiteral("ok"));
        sendResponse(ws, id, resp);
        return;
    }

    // ─── Address Book ───
    if (command == QStringLiteral("listaccounts")) {
        QJsonObject accList;
        if (m_db) {
            const QList<QJsonObject> accounts = m_db->domainAccounts(session->domain());
            for (const QJsonObject &acc : accounts) {
                const QString login = acc.value(QStringLiteral("login")).toString();
                const QString displayName = acc.value(QStringLiteral("displayName")).toString();
                const QString phone = acc.value(QStringLiteral("phone")).toString();
                const QString ext = acc.value(QStringLiteral("ext")).toString();
                // Strip @domain from login for the key
                const QString key = login.section(QLatin1Char('@'), 0, 0);

                QJsonObject entry;
                entry.insert(QStringLiteral("RealName"), displayName);
                QJsonArray extArr;
                if (!ext.isEmpty()) {
                    extArr.append(ext);
                }
                entry.insert(QStringLiteral("ext"), extArr);
                entry.insert(QStringLiteral("mobile"), phone);
                QJsonArray tnArr;
                if (!phone.isEmpty()) {
                    tnArr.append(phone);
                }
                entry.insert(QStringLiteral("tn"), tnArr);
                entry.insert(QStringLiteral("sim"), QJsonValue());
                entry.insert(QStringLiteral("Email"), QString());
                entry.insert(QStringLiteral("Position"), QString());
                accList.insert(key, entry);
            }
        }

        // Dev fallback: if DB is empty or unavailable, return current user
        if (accList.isEmpty() && !session->login().isEmpty()) {
            const QString key = session->login().section(QLatin1Char('@'), 0, 0);
            QJsonObject entry;
            entry.insert(QStringLiteral("RealName"), key);
            entry.insert(QStringLiteral("ext"), QJsonArray());
            entry.insert(QStringLiteral("mobile"), QString());
            entry.insert(QStringLiteral("tn"), QJsonArray());
            entry.insert(QStringLiteral("sim"), QJsonValue());
            entry.insert(QStringLiteral("Email"), QString());
            entry.insert(QStringLiteral("Position"), QString());
            accList.insert(key, entry);
        }

        QJsonObject response;
        response.insert(QStringLiteral("response"), QJsonObject{
            {QStringLiteral("accList"), accList},
        });
        sendResponse(ws, id, response);
        return;
    }

    if (command == QStringLiteral("subscribetoaddressbook")) {
        session->setAddressBookSubscribed(true);
        if (m_abMgr) {
            m_abMgr->sendContactsToSession(session);
        }
        QJsonObject resp;
        resp.insert(QStringLiteral("response"), QStringLiteral("ok"));
        sendResponse(ws, id, resp);
        return;
    }

    if (command == QStringLiteral("createcontact")) {
        const QJsonObject contact = cmdObj.value(QStringLiteral("contact")).toObject();
        if (m_abMgr) {
            m_abMgr->createContact(session, id, contact);
        }
        return;
    }

    if (command == QStringLiteral("deletecontact")) {
        const QJsonObject contact = cmdObj.value(QStringLiteral("contact")).toObject();
        const QString contactId = contact.value(QStringLiteral("contactId")).toString();
        if (m_abMgr) {
            m_abMgr->deleteContact(session, id, contactId);
        }
        return;
    }

    if (command == QStringLiteral("uploadcontacts")) {
        const QJsonArray contacts = cmdObj.value(QStringLiteral("contacts")).toArray();
        if (m_abMgr) {
            m_abMgr->uploadContacts(session, id, contacts);
        }
        return;
    }

    // ─── IM ───
    if (command == QStringLiteral("SendIM")) {
        if (m_imMgr) {
            m_imMgr->handleSendIm(session, id, cmdObj);
        }
        return;
    }

    if (command == QStringLiteral("loadimhistory")) {
        if (m_imMgr) {
            m_imMgr->handleLoadHistory(session, id, cmdObj);
        }
        return;
    }

    if (command == QStringLiteral("smstelnums")) {
        QJsonObject resp;
        QJsonArray nums;
        resp.insert(QStringLiteral("response"), nums);
        sendResponse(ws, id, resp);
        return;
    }

    // ─── Calls ───
    if (command == QStringLiteral("ProvisionCall")) {
        if (m_callMgr) {
            m_callMgr->handleProvisionCall(session, id, cmdObj);
        }
        return;
    }

    if (command == QStringLiteral("StartCall")) {
        if (m_callMgr) {
            m_callMgr->handleStartCall(session, id, cmdObj);
        }
        return;
    }

    if (command == QStringLiteral("AcceptCall")) {
        if (m_callMgr) {
            m_callMgr->handleAcceptCall(session, id, cmdObj);
        }
        return;
    }

    if (command == QStringLiteral("RejectCall")) {
        if (m_callMgr) {
            m_callMgr->handleRejectCall(session, id, cmdObj);
        }
        return;
    }

    if (command == QStringLiteral("CancelCall")) {
        if (m_callMgr) {
            m_callMgr->handleCancelCall(session, id, cmdObj);
        }
        return;
    }

    if (command == QStringLiteral("DisconnectCall")) {
        if (m_callMgr) {
            m_callMgr->handleDisconnectCall(session, id, cmdObj);
        }
        return;
    }

    if (command == QStringLiteral("AckAccept")) {
        if (m_callMgr) {
            m_callMgr->handleAckAccept(session, id, cmdObj);
        }
        return;
    }

    if (command == QStringLiteral("UpdateCall")) {
        if (m_callMgr) {
            m_callMgr->handleUpdateCall(session, id, cmdObj);
        }
        return;
    }

    if (command == QStringLiteral("AcceptUpdate")) {
        if (m_callMgr) {
            m_callMgr->handleAcceptUpdate(session, id, cmdObj);
        }
        return;
    }

    if (command == QStringLiteral("Transfer")) {
        if (m_callMgr) {
            m_callMgr->handleTransfer(session, id, cmdObj);
        }
        return;
    }

    // ─── History ───
    if (command == QStringLiteral("gethistory")) {
        if (m_histMgr) {
            m_histMgr->handleGetHistory(session, id, cmdObj);
        }
        return;
    }

    if (command == QStringLiteral("createorupdatenote")) {
        if (m_histMgr) {
            m_histMgr->handleCreateOrUpdateNote(session, id, cmdObj);
        }
        return;
    }

    if (command == QStringLiteral("deletenote")) {
        if (m_histMgr) {
            m_histMgr->handleDeleteNote(session, id, cmdObj);
        }
        return;
    }

    // ─── SMS ───
    if (command == QStringLiteral("getsmschannels")) {
        if (m_smsMgr) {
            m_smsMgr->handleGetSmsChannels(session, id, cmdObj);
        }
        return;
    }

    if (command == QStringLiteral("getsms")) {
        if (m_smsMgr) {
            m_smsMgr->handleGetSms(session, id, cmdObj);
        }
        return;
    }

    if (command == QStringLiteral("sendsms")) {
        if (m_smsMgr) {
            m_smsMgr->handleSendSms(session, id, cmdObj);
        }
        return;
    }

    // ─── Server settings ───
    if (command == QStringLiteral("getcommunicatorsettings")) {
        QJsonObject settings;
        settings.insert(QStringLiteral("videoEnabled"), m_videoEnabled);
        QJsonObject resp;
        resp.insert(QStringLiteral("response"), settings);
        sendResponse(ws, id, resp);
        qCInfo(lcDispatch) << "getcommunicatorsettings -> videoEnabled:" << m_videoEnabled;
        return;
    }

    // ─── Conference ───
    if (command == QStringLiteral("kickAll") || command == QStringLiteral("changedescription")
        || command == QStringLiteral("inviteParticipants") || command == QStringLiteral("kickparticipants")
        || command == QStringLiteral("muteparticipant") || command == QStringLiteral("giveadmin")
        || command == QStringLiteral("listConferences")) {
        if (m_confMgr) {
            m_confMgr->handleCommand(session, id, command, cmdObj);
        }
        return;
    }

    qCWarning(lcDispatch) << "Unknown command:" << command;
    if (id >= 0) {
        QJsonObject resp;
        resp.insert(QStringLiteral("error"), QStringLiteral("unknown command"));
        sendResponse(ws, id, resp);
    }
}

void CommandDispatcher::sendResponse(WsSession *ws, int requestId, const QJsonObject &response)
{
    if (requestId >= 0 && ws) {
        ws->sendResponse(requestId, response);
    }
}

} // namespace itl
