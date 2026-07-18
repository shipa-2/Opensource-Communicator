#pragma once

#include <QObject>

namespace itl {

class UserSession;
class Database;

class AddressBookManager : public QObject {
    Q_OBJECT

public:
    explicit AddressBookManager(Database *db, QObject *parent = nullptr);

    void sendContactsToSession(UserSession *session);
    void createContact(UserSession *session, int requestId, const QJsonObject &contact);
    void deleteContact(UserSession *session, int requestId, const QString &contactId);
    void uploadContacts(UserSession *session, int requestId, const QJsonArray &contacts);

private:
    void pushContactsChanged(UserSession *session);

    Database *m_db;
};

} // namespace itl
