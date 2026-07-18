#pragma once

#include <QJsonObject>
#include <QObject>
#include <QSqlDatabase>
#include <QString>
#include <QVariantMap>

namespace itl {

class Database : public QObject {
    Q_OBJECT

public:
    explicit Database(QObject *parent = nullptr);
    ~Database() override;

    bool initialize(const QString &host, quint16 port, const QString &dbName,
                    const QString &user, const QString &password);
    bool isConnected() const;

    bool ensureSchema();

    // Users
    bool authenticateUser(const QString &login, const QString &passwordHash, QString *outDomain,
                          QString *outRole, QString *outPartner);
    bool authenticateUserByName(const QString &login, const QString &passwordHash,
                                QString *outLogin, QString *outDomain,
                                QString *outRole, QString *outPartner);
    bool userExists(const QString &login);
    int userId(const QString &login);
    QString userDomain(const QString &login);
    QString userDisplayName(const QString &login);
    QList<QJsonObject> domainAccounts(const QString &domain);
    bool updateUserPresence(const QString &login, const QString &status);
    bool createUser(const QString &login, const QString &passwordHash, const QString &domain,
                    const QString &displayName, const QString &role,
                    const QString &phone = {}, const QString &ext = {});
    bool deleteUser(const QString &login);
    QList<QJsonObject> listUsers();

    // Personal contacts
    QList<QJsonObject> personalContacts(int ownerId);
    int createPersonalContact(int ownerId, const QJsonObject &contact);
    bool deletePersonalContact(int ownerId, const QString &contactId);
    bool replaceAllContacts(int ownerId, const QJsonArray &contacts);

    // IM messages
    qint64 insertImMessage(const QString &chatId, const QString &senderLogin,
                           const QString &body, const QString &type = QStringLiteral("text"),
                           bool persist = true);
    QList<QJsonObject> imHistory(const QString &chatId, int limit = 32767);
    QList<QJsonObject> imHistoryForUser(const QString &login, int limit = 32767);

    // Call history
    bool insertCallRecord(const QJsonObject &record);
    QList<QJsonObject> callHistory(int ownerId, const QString &type = {},
                                   const QString &search = {}, int limit = 100);
    bool updateCallNote(const QString &uid, const QString &text, const QString &author);
    bool deleteCallNote(const QString &uid);

private:
    QSqlDatabase m_db;
    QString m_connectionName;
};

} // namespace itl
