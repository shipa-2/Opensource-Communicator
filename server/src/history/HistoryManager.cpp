#include "HistoryManager.h"
#include "../db/Database.h"
#include "../network/WsSession.h"
#include "../session/UserSession.h"

#include <QJsonArray>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(lcHist, "server.history")

namespace itl {

HistoryManager::HistoryManager(Database *db, QObject *parent)
    : QObject(parent)
    , m_db(db)
{
}

void HistoryManager::handleGetHistory(UserSession *session, int requestId, const QJsonObject &payload)
{
    const QString type = payload.value(QStringLiteral("CallType")).toString();
    const int limit = payload.value(QStringLiteral("Limit")).toInt(100);

    QJsonObject resp;
    if (m_db) {
        const int uid = m_db->userId(session->login());
        const QList<QJsonObject> records = m_db->callHistory(uid, type, {}, limit);
        QJsonArray arr;
        for (const QJsonObject &r : records) {
            arr.append(r);
        }
        resp.insert(QStringLiteral("response"), QJsonObject{
            {QStringLiteral("calls"), arr},
        });
    } else {
        resp.insert(QStringLiteral("response"), QJsonObject{
            {QStringLiteral("calls"), QJsonArray()},
        });
    }

    session->wsSession()->sendResponse(requestId, resp);
    qCInfo(lcHist) << "History loaded for" << session->login();
}

void HistoryManager::handleCreateOrUpdateNote(UserSession *session, int requestId, const QJsonObject &payload)
{
    const QString uid = payload.value(QStringLiteral("uid")).toString();
    const QString text = payload.value(QStringLiteral("text")).toString();
    const QString author = payload.value(QStringLiteral("author")).toString();

    QJsonObject resp;
    if (m_db) {
        const bool ok = m_db->updateCallNote(uid, text, author);
        resp.insert(QStringLiteral("response"), ok ? QStringLiteral("ok") : QStringLiteral("error"));
    } else {
        resp.insert(QStringLiteral("response"), QStringLiteral("ok"));
    }

    session->wsSession()->sendResponse(requestId, resp);
}

void HistoryManager::handleDeleteNote(UserSession *session, int requestId, const QJsonObject &payload)
{
    const QString uid = payload.value(QStringLiteral("uid")).toString();

    QJsonObject resp;
    if (m_db) {
        const bool ok = m_db->deleteCallNote(uid);
        resp.insert(QStringLiteral("response"), ok ? QStringLiteral("ok") : QStringLiteral("error"));
    } else {
        resp.insert(QStringLiteral("response"), QStringLiteral("ok"));
    }

    session->wsSession()->sendResponse(requestId, resp);
}

} // namespace itl
