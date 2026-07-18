#include "AddressBookManager.h"
#include "../db/Database.h"
#include "../network/WsSession.h"
#include "../session/UserSession.h"

#include <QJsonArray>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(lcAb, "server.addressbook")

namespace itl {

AddressBookManager::AddressBookManager(Database *db, QObject *parent)
    : QObject(parent)
    , m_db(db)
{
}

void AddressBookManager::sendContactsToSession(UserSession *session)
{
    if (!m_db || !session) {
        return;
    }

    const int uid = m_db->userId(session->login());
    const QList<QJsonObject> contacts = m_db->personalContacts(uid);

    QJsonArray objects;
    for (const QJsonObject &c : contacts) {
        objects.append(c);
    }

    QJsonObject payload;
    payload.insert(QStringLiteral("What"), QStringLiteral("[CONTACTS]"));
    payload.insert(QStringLiteral("objects"), objects);
    payload.insert(QStringLiteral("subId"), session->login());

    session->wsSession()->sendPayload(payload);
    qCInfo(lcAb) << "Contacts sent to" << session->login() << ":" << contacts.size();
}

void AddressBookManager::createContact(UserSession *session, int requestId, const QJsonObject &contact)
{
    if (!m_db || !session) {
        return;
    }

    const int uid = m_db->userId(session->login());
    const int id = m_db->createPersonalContact(uid, contact);

    QJsonObject resp;
    if (id >= 0) {
        resp.insert(QStringLiteral("response"), QJsonObject{
            {QStringLiteral("contactId"), QString::number(id)},
        });
        pushContactsChanged(session);
    } else {
        resp.insert(QStringLiteral("error"), QStringLiteral("failed to create contact"));
    }

    session->wsSession()->sendResponse(requestId, resp);
    qCInfo(lcAb) << "Contact created for" << session->login() << "id:" << id;
}

void AddressBookManager::deleteContact(UserSession *session, int requestId, const QString &contactId)
{
    if (!m_db || !session) {
        return;
    }

    const int uid = m_db->userId(session->login());
    const bool ok = m_db->deletePersonalContact(uid, contactId);

    QJsonObject resp;
    if (ok) {
        resp.insert(QStringLiteral("response"), QStringLiteral("ok"));
        pushContactsChanged(session);
    } else {
        resp.insert(QStringLiteral("error"), QStringLiteral("failed to delete contact"));
    }

    session->wsSession()->sendResponse(requestId, resp);
    qCInfo(lcAb) << "Contact deleted for" << session->login() << "id:" << contactId;
}

void AddressBookManager::uploadContacts(UserSession *session, int requestId, const QJsonArray &contacts)
{
    if (!m_db || !session) {
        return;
    }

    const int uid = m_db->userId(session->login());
    const bool ok = m_db->replaceAllContacts(uid, contacts);

    QJsonObject resp;
    resp.insert(QStringLiteral("response"), ok ? QStringLiteral("ok") : QStringLiteral("error"));
    session->wsSession()->sendResponse(requestId, resp);

    if (ok) {
        pushContactsChanged(session);
    }

    qCInfo(lcAb) << "Contacts uploaded for" << session->login() << ":" << contacts.size();
}

void AddressBookManager::pushContactsChanged(UserSession *session)
{
    sendContactsToSession(session);
}

} // namespace itl
