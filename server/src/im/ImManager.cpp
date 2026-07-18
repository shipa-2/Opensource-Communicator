#include "ImManager.h"
#include "../db/Database.h"
#include "../network/WsSession.h"
#include "../session/UserSession.h"
#include "../session/SessionManager.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(lcIm, "server.im")

namespace itl {

ImManager::ImManager(Database *db, QObject *parent)
    : QObject(parent)
    , m_db(db)
{
}

void ImManager::sendUnreadContacts(UserSession *session)
{
    QJsonObject payload;
    payload.insert(QStringLiteral("What"), QStringLiteral("[IM_CONTACTS]"));
    payload.insert(QStringLiteral("contacts"), QJsonObject());
    session->wsSession()->sendPayload(payload);
}

void ImManager::handleSendIm(UserSession *session, int requestId, const QJsonObject &payload)
{
    const QString to = payload.value(QStringLiteral("to")).toString();
    const QJsonValue bodyVal = payload.value(QStringLiteral("body"));
    const QString body = bodyVal.isString() ? bodyVal.toString() : QString();
    const QString type = payload.value(QStringLiteral("type")).toString();
    const bool persist = !payload.value(QStringLiteral("persist")).toBool(false)
                         || !payload.contains(QStringLiteral("persist"));
    const bool copyToSelf = !payload.value(QStringLiteral("copyToSelf")).toBool(false)
                            || !payload.contains(QStringLiteral("copyToSelf"));

    if (to.isEmpty() || body.isEmpty()) {
        QJsonObject resp;
        resp.insert(QStringLiteral("error"), QStringLiteral("missing to or body"));
        session->wsSession()->sendResponse(requestId, resp);
        return;
    }

    // Send response to sender
    QJsonObject resp;
    resp.insert(QStringLiteral("response"), QJsonObject{{QStringLiteral("id"), 0}});
    session->wsSession()->sendResponse(requestId, resp);

    // Always advertise sender as login@session-domain (login field may contain another host).
    const QString loginOnly = session->login().section(QLatin1Char('@'), 0, 0);
    const QString fromAddr = loginOnly + QLatin1Char('@') + session->domain();

    // Persist
    qint64 msgId = -1;
    if (persist && m_db) {
        const QString chatId = buildChatId(session->login(), to);
        msgId = m_db->insertImMessage(chatId, session->login(), body, type, true);
    }

    // Build [IM] push payload
    QJsonObject imMsg;
    imMsg.insert(QStringLiteral("id"), msgId >= 0 ? QString::number(msgId) : QStringLiteral("0"));
    imMsg.insert(QStringLiteral("from"), fromAddr);
    imMsg.insert(QStringLiteral("to"), to);
    imMsg.insert(QStringLiteral("body"), body);
    if (!type.isEmpty()) {
        imMsg.insert(QStringLiteral("type"), type);
    }
    imMsg.insert(QStringLiteral("timestamp"), QDateTime::currentMSecsSinceEpoch());

    QJsonObject push;
    push.insert(QStringLiteral("What"), QStringLiteral("[IM]"));
    push.insert(QStringLiteral("message"), imMsg);

    // Deliver to recipient
    deliverToRecipient(push, to);

    // Copy to self
    if (copyToSelf) {
        session->wsSession()->sendPayload(push);
    }

    qCInfo(lcIm) << "IM sent:" << fromAddr << "->" << to << "persist:" << persist
                 << "id:" << msgId;
}

void ImManager::deliverToRecipient(const QJsonObject &push, const QString &to)
{
    if (!m_sessions) {
        return;
    }

    // to can be "user@domain" or just "user"
    QString recipientLogin = to;
    const int atIdx = to.indexOf(QLatin1Char('@'));
    if (atIdx > 0) {
        recipientLogin = to.left(atIdx);
    }

    // Find all sessions for this login and deliver
    const QList<UserSession *> sessions = m_sessions->sessionsForLogin(recipientLogin);
    int delivered = 0;
    for (UserSession *target : sessions) {
        if (target->wsSession() && target->isImBound()) {
            target->wsSession()->sendPayload(push);
            delivered++;
        }
    }
    if (delivered == 0) {
        qCInfo(lcIm) << "IM delivery: recipient" << to << "has no active IM sessions";
    }
}

void ImManager::handleLoadHistory(UserSession *session, int requestId, const QJsonObject &payload)
{
    const int maxSize = payload.value(QStringLiteral("maxSize")).toInt(32767);
    const QString chatId = payload.value(QStringLiteral("chatId")).toString();

    if (m_db) {
        QList<QJsonObject> messages;
        if (!chatId.isEmpty()) {
            // Specific chat — respond directly
            messages = m_db->imHistory(chatId, maxSize);
            QJsonArray arr;
            for (const QJsonObject &msg : messages) {
                arr.append(msg);
            }
            QJsonObject resp;
            resp.insert(QStringLiteral("response"), QJsonObject{
                {QStringLiteral("chatId"), chatId},
                {QStringLiteral("messages"), arr},
            });
            session->wsSession()->sendResponse(requestId, resp);
        } else {
            // All chats — send empty RPC response, then push per-peer [IM_HIST]
            QJsonObject resp;
            resp.insert(QStringLiteral("response"), QJsonObject{
                {QStringLiteral("chatId"), QString()},
                {QStringLiteral("messages"), QJsonArray()},
            });
            session->wsSession()->sendResponse(requestId, resp);

            // Load all messages and group by peer
            messages = m_db->imHistoryForUser(session->login(), maxSize);
            QHash<QString, QJsonArray> byPeer;
            for (const QJsonObject &msg : messages) {
                const QString peer = msg.value(QStringLiteral("to")).toString();
                if (peer.isEmpty()) {
                    continue;
                }
                byPeer[peer].append(msg);
            }

            // Send separate [IM_HIST] push for each peer
            for (auto it = byPeer.constBegin(); it != byPeer.constEnd(); ++it) {
                QJsonObject push;
                push.insert(QStringLiteral("What"), QStringLiteral("[IM_HIST]"));
                push.insert(QStringLiteral("chatId"), it.key());
                push.insert(QStringLiteral("messages"), it.value());
                session->wsSession()->sendPayload(push);
            }

            qCInfo(lcIm) << "History for" << session->login() << ":"
                         << messages.size() << "msgs," << byPeer.size() << "peers";
        }
    } else {
        QJsonObject resp;
        resp.insert(QStringLiteral("response"), QJsonObject{
            {QStringLiteral("chatId"), chatId},
            {QStringLiteral("messages"), QJsonArray()},
        });
        session->wsSession()->sendResponse(requestId, resp);
    }
}

QString ImManager::buildChatId(const QString &loginA, const QString &loginB)
{
    const QString a = loginA.section(QLatin1Char('@'), 0, 0).toLower();
    const QString b = loginB.section(QLatin1Char('@'), 0, 0).toLower();
    return a < b ? a + QLatin1Char(':') + b : b + QLatin1Char(':') + a;
}

} // namespace itl
