#include "Database.h"
#include "../protocol/ProtocolTypes.h"

#include <QJsonArray>
#include <QDateTime>
#include <QLoggingCategory>
#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>

Q_LOGGING_CATEGORY(lcDb, "server.db")

namespace itl {

Database::Database(QObject *parent)
    : QObject(parent)
    , m_connectionName(QUuid::createUuid().toString(QUuid::WithoutBraces))
{
}

Database::~Database() = default;

bool Database::initialize(const QString &host, quint16 port, const QString &dbName,
                          const QString &user, const QString &password)
{
    QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QPSQL"), m_connectionName);
    db.setHostName(host);
    db.setPort(port);
    db.setDatabaseName(dbName);
    db.setUserName(user);
    db.setPassword(password);

    if (!db.open()) {
        qCCritical(lcDb) << "Database connection failed:" << db.lastError().text();
        return false;
    }

    m_db = db;
    qCInfo(lcDb) << "Connected to PostgreSQL" << host << ":" << port << "/" << dbName;
    return true;
}

bool Database::isConnected() const
{
    return m_db.isValid() && m_db.isOpen();
}

bool Database::ensureSchema()
{
    if (!isConnected()) {
        return false;
    }

    QSqlQuery q(m_db);

    q.exec(QStringLiteral(
        "CREATE TABLE IF NOT EXISTS users ("
        "  id SERIAL PRIMARY KEY,"
        "  login VARCHAR(255) NOT NULL UNIQUE,"
        "  password_hash VARCHAR(255) NOT NULL,"
        "  domain VARCHAR(255) NOT NULL,"
        "  partner VARCHAR(64) DEFAULT 'opensource',"
        "  display_name VARCHAR(255),"
        "  role VARCHAR(32) DEFAULT 'User',"
        "  presence_status VARCHAR(32) DEFAULT 'offline',"
        "  created_at TIMESTAMPTZ DEFAULT NOW()"
        ")"
    ));

    q.exec(QStringLiteral(
        "CREATE TABLE IF NOT EXISTS personal_contacts ("
        "  id SERIAL PRIMARY KEY,"
        "  owner_id INT REFERENCES users(id) ON DELETE CASCADE,"
        "  server_id VARCHAR(255),"
        "  name_full VARCHAR(255),"
        "  phone VARCHAR(64),"
        "  ext VARCHAR(255),"
        "  peer VARCHAR(255),"
        "  created_at TIMESTAMPTZ DEFAULT NOW()"
        ")"
    ));

    q.exec(QStringLiteral(
        "CREATE TABLE IF NOT EXISTS im_messages ("
        "  id BIGSERIAL PRIMARY KEY,"
        "  chat_id VARCHAR(512) NOT NULL,"
        "  sender_id INT REFERENCES users(id),"
        "  sender_login VARCHAR(255),"
        "  body TEXT,"
        "  msg_type VARCHAR(32) DEFAULT 'text',"
        "  persist BOOLEAN DEFAULT true,"
        "  created_at TIMESTAMPTZ DEFAULT NOW()"
        ")"
    ));

    q.exec(QStringLiteral(
        "CREATE TABLE IF NOT EXISTS call_history ("
        "  id SERIAL PRIMARY KEY,"
        "  uid VARCHAR(255) NOT NULL UNIQUE,"
        "  owner_id INT REFERENCES users(id),"
        "  caller VARCHAR(255),"
        "  callee VARCHAR(255),"
        "  direction VARCHAR(16),"
        "  start_time TIMESTAMPTZ,"
        "  end_time TIMESTAMPTZ,"
        "  duration INT,"
        "  status VARCHAR(32),"
        "  note TEXT,"
        "  note_author VARCHAR(255),"
        "  created_at TIMESTAMPTZ DEFAULT NOW()"
        ")"
    ));

    q.exec(QStringLiteral("CREATE INDEX IF NOT EXISTS idx_users_domain ON users(domain)"));
    q.exec(QStringLiteral("CREATE INDEX IF NOT EXISTS idx_contacts_owner ON personal_contacts(owner_id)"));
    q.exec(QStringLiteral("CREATE INDEX IF NOT EXISTS idx_im_chat ON im_messages(chat_id, created_at)"));
    q.exec(QStringLiteral("CREATE INDEX IF NOT EXISTS idx_call_history_owner ON call_history(owner_id, start_time DESC)"));

    // Add phone columns if missing (migration)
    q.exec(QStringLiteral("ALTER TABLE users ADD COLUMN IF NOT EXISTS phone VARCHAR(64) DEFAULT ''"));
    q.exec(QStringLiteral("ALTER TABLE users ADD COLUMN IF NOT EXISTS ext VARCHAR(32) DEFAULT ''"));

    qCInfo(lcDb) << "Database schema ensured";
    return true;
}

bool Database::authenticateUser(const QString &login, const QString &passwordHash, QString *outDomain,
                                QString *outRole, QString *outPartner)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "SELECT domain, role, partner FROM users WHERE login = :login AND password_hash = :pw"
    ));
    q.bindValue(QStringLiteral(":login"), login);
    q.bindValue(QStringLiteral(":pw"), passwordHash);

    if (!q.exec() || !q.next()) {
        return false;
    }

    if (outDomain) *outDomain = q.value(0).toString();
    if (outRole) *outRole = q.value(1).toString();
    if (outPartner) *outPartner = q.value(2).toString();
    return true;
}

bool Database::authenticateUserByName(const QString &login, const QString &passwordHash,
                                      QString *outLogin, QString *outDomain,
                                      QString *outRole, QString *outPartner)
{
    const QString user = login.section(QLatin1Char('@'), 0, 0).trimmed().toLower();
    const QString requestedDomain = login.section(QLatin1Char('@'), 1).trimmed().toLower();
    if (user.isEmpty()) {
        return false;
    }

    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "SELECT login, domain, role, partner FROM users "
        "WHERE LOWER(SPLIT_PART(login, '@', 1)) = :user AND password_hash = :pw "
        "ORDER BY CASE WHEN LOWER(domain) = :domain THEN 0 ELSE 1 END, login"
    ));
    q.bindValue(QStringLiteral(":user"), user);
    q.bindValue(QStringLiteral(":pw"), passwordHash);
    q.bindValue(QStringLiteral(":domain"), requestedDomain);
    if (!q.exec() || !q.next()) {
        return false;
    }

    const QString matchedLogin = q.value(0).toString();
    const QString matchedDomain = q.value(1).toString();
    const QString matchedRole = q.value(2).toString();
    const QString matchedPartner = q.value(3).toString();

    // If several domains contain the same user/password, an arbitrary domain
    // alias would be unsafe. A directly matching requested domain remains
    // deterministic because it is sorted first.
    if (q.next() && matchedDomain.compare(requestedDomain, Qt::CaseInsensitive) != 0) {
        return false;
    }

    if (outLogin) *outLogin = matchedLogin;
    if (outDomain) *outDomain = matchedDomain;
    if (outRole) *outRole = matchedRole;
    if (outPartner) *outPartner = matchedPartner;
    return true;
}

bool Database::userExists(const QString &login)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("SELECT 1 FROM users WHERE login = :login"));
    q.bindValue(QStringLiteral(":login"), login);
    return q.exec() && q.next();
}

int Database::userId(const QString &login)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("SELECT id FROM users WHERE login = :login"));
    q.bindValue(QStringLiteral(":login"), login);
    if (q.exec() && q.next()) {
        return q.value(0).toInt();
    }
    return -1;
}

QString Database::userDomain(const QString &login)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("SELECT domain FROM users WHERE login = :login"));
    q.bindValue(QStringLiteral(":login"), login);
    if (q.exec() && q.next()) {
        return q.value(0).toString();
    }
    return {};
}

QString Database::userDisplayName(const QString &login)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("SELECT display_name FROM users WHERE login = :login"));
    q.bindValue(QStringLiteral(":login"), login);
    if (q.exec() && q.next()) {
        return q.value(0).toString();
    }
    return {};
}

QList<QJsonObject> Database::domainAccounts(const QString &domain)
{
    QList<QJsonObject> result;
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "SELECT login, display_name, presence_status, phone, ext FROM users WHERE domain = :domain ORDER BY login"
    ));
    q.bindValue(QStringLiteral(":domain"), domain);

    if (!q.exec()) {
        return result;
    }

    while (q.next()) {
        QJsonObject account;
        account[QStringLiteral("login")] = q.value(0).toString();
        account[QStringLiteral("displayName")] = q.value(1).toString();
        account[QStringLiteral("presence")] = q.value(2).toString();
        account[QStringLiteral("phone")] = q.value(3).toString();
        account[QStringLiteral("ext")] = q.value(4).toString();
        result.append(account);
    }
    return result;
}

bool Database::updateUserPresence(const QString &login, const QString &status)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("UPDATE users SET presence_status = :status WHERE login = :login"));
    q.bindValue(QStringLiteral(":status"), status);
    q.bindValue(QStringLiteral(":login"), login);
    return q.exec();
}

bool Database::createUser(const QString &login, const QString &passwordHash, const QString &domain,
                          const QString &displayName, const QString &role,
                          const QString &phone, const QString &ext)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "INSERT INTO users (login, password_hash, domain, display_name, role, phone, ext) "
        "VALUES (:login, :pw, :domain, :name, :role, :phone, :ext)"
    ));
    q.bindValue(QStringLiteral(":login"), login);
    q.bindValue(QStringLiteral(":pw"), passwordHash);
    q.bindValue(QStringLiteral(":domain"), domain);
    q.bindValue(QStringLiteral(":name"), displayName);
    q.bindValue(QStringLiteral(":role"), role);
    q.bindValue(QStringLiteral(":phone"), phone);
    q.bindValue(QStringLiteral(":ext"), ext);
    return q.exec();
}

bool Database::deleteUser(const QString &login)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("DELETE FROM users WHERE login = :login"));
    q.bindValue(QStringLiteral(":login"), login);
    return q.exec();
}

QList<QJsonObject> Database::listUsers()
{
    QList<QJsonObject> result;
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("SELECT login, display_name, domain, role FROM users ORDER BY login"));
    if (!q.exec()) {
        return result;
    }
    while (q.next()) {
        QJsonObject user;
        user[QStringLiteral("login")] = q.value(0).toString();
        user[QStringLiteral("displayName")] = q.value(1).toString();
        user[QStringLiteral("domain")] = q.value(2).toString();
        user[QStringLiteral("role")] = q.value(3).toString();
        result.append(user);
    }
    return result;
}

QList<QJsonObject> Database::personalContacts(int ownerId)
{
    QList<QJsonObject> result;
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "SELECT server_id, name_full, phone, ext, peer FROM personal_contacts WHERE owner_id = :oid"
    ));
    q.bindValue(QStringLiteral(":oid"), ownerId);

    if (!q.exec()) {
        return result;
    }

    while (q.next()) {
        QJsonObject c;
        c[QStringLiteral("id")] = q.value(0).toString();
        c[QStringLiteral("name")] = QJsonObject{{QStringLiteral("full"), q.value(1).toString()}};
        QJsonArray phones;
        if (!q.value(2).toString().isEmpty()) {
            phones.append(QJsonObject{{QStringLiteral(""), q.value(2).toString()},
                                       {QStringLiteral("type"), QStringLiteral("main")}});
        }
        if (!q.value(3).toString().isEmpty()) {
            phones.append(QJsonObject{{QStringLiteral(""), q.value(3).toString()},
                                       {QStringLiteral("type"), QStringLiteral("ext")}});
        }
        c[QStringLiteral("phones")] = phones;
        c[QStringLiteral("peer")] = q.value(4).toString();
        result.append(c);
    }
    return result;
}

int Database::createPersonalContact(int ownerId, const QJsonObject &contact)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "INSERT INTO personal_contacts (owner_id, server_id, name_full, phone, ext, peer) "
        "VALUES (:oid, :sid, :name, :phone, :ext, :peer) RETURNING id"
    ));
    q.bindValue(QStringLiteral(":oid"), ownerId);
    q.bindValue(QStringLiteral(":sid"), contact.value(QStringLiteral("contactId")).toString());
    q.bindValue(QStringLiteral(":name"), contact.value(QStringLiteral("name")).toObject()
                   .value(QStringLiteral("full")).toString());

    const QJsonArray phones = contact.value(QStringLiteral("phones")).toArray();
    QString phone, ext;
    for (const QJsonValue &v : phones) {
        const QJsonObject ph = v.toObject();
        const QString type = ph.value(QStringLiteral("type")).toString();
        const QString num = ph.value(QString::fromUtf8(kEmptyKey)).toString();
        if (type == QStringLiteral("ext")) {
            ext = num;
        } else if (phone.isEmpty()) {
            phone = num;
        }
    }
    q.bindValue(QStringLiteral(":phone"), phone);
    q.bindValue(QStringLiteral(":ext"), ext);
    q.bindValue(QStringLiteral(":peer"), contact.value(QStringLiteral("peer")).toString());

    if (!q.exec() || !q.next()) {
        qCWarning(lcDb) << "createPersonalContact failed:" << q.lastError().text();
        return -1;
    }
    return q.value(0).toInt();
}

bool Database::deletePersonalContact(int ownerId, const QString &contactId)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("DELETE FROM personal_contacts WHERE owner_id = :oid AND server_id = :sid"));
    q.bindValue(QStringLiteral(":oid"), ownerId);
    q.bindValue(QStringLiteral(":sid"), contactId);
    return q.exec();
}

bool Database::replaceAllContacts(int ownerId, const QJsonArray &contacts)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("DELETE FROM personal_contacts WHERE owner_id = :oid"));
    q.bindValue(QStringLiteral(":oid"), ownerId);
    if (!q.exec()) {
        return false;
    }

    for (const QJsonValue &v : contacts) {
        createPersonalContact(ownerId, v.toObject());
    }
    return true;
}

qint64 Database::insertImMessage(const QString &chatId, const QString &senderLogin,
                                 const QString &body, const QString &type, bool persist)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "INSERT INTO im_messages (chat_id, sender_login, body, msg_type, persist) "
        "VALUES (:cid, :sender, :body, :type, :persist) RETURNING id"
    ));
    q.bindValue(QStringLiteral(":cid"), chatId);
    q.bindValue(QStringLiteral(":sender"), senderLogin);
    q.bindValue(QStringLiteral(":body"), body);
    q.bindValue(QStringLiteral(":type"), type);
    q.bindValue(QStringLiteral(":persist"), persist);

    if (!q.exec() || !q.next()) {
        qCWarning(lcDb) << "insertImMessage failed:" << q.lastError().text();
        return -1;
    }
    return q.value(0).toLongLong();
}

QList<QJsonObject> Database::imHistory(const QString &chatId, int limit)
{
    QList<QJsonObject> result;
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "SELECT id, sender_login, body, msg_type, created_at "
        "FROM im_messages WHERE chat_id = :cid ORDER BY created_at DESC LIMIT :lim"
    ));
    q.bindValue(QStringLiteral(":cid"), chatId);
    q.bindValue(QStringLiteral(":lim"), limit);

    if (!q.exec()) {
        return result;
    }

    while (q.next()) {
        QJsonObject msg;
        msg[QStringLiteral("id")] = q.value(0).toString();
        msg[QStringLiteral("from")] = q.value(1).toString();
        msg[QStringLiteral("body")] = q.value(2).toString();
        msg[QStringLiteral("type")] = q.value(3).toString();
        msg[QStringLiteral("timestamp")] = q.value(4).toDateTime().toMSecsSinceEpoch();
        result.prepend(msg);
    }
    return result;
}

QList<QJsonObject> Database::imHistoryForUser(const QString &login, int limit)
{
    QList<QJsonObject> result;
    const QString userPart = login.section(QLatin1Char('@'), 0, 0).toLower();
    const QString userDomain = login.section(QLatin1Char('@'), 1);
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "SELECT id, sender_login, body, msg_type, created_at, chat_id "
        "FROM im_messages "
        "WHERE chat_id LIKE :pat "
        "ORDER BY created_at DESC LIMIT :lim"
    ));
    q.bindValue(QStringLiteral(":pat"), QStringLiteral("%%1%").arg(userPart));
    q.bindValue(QStringLiteral(":lim"), limit);

    if (!q.exec()) {
        return result;
    }

    while (q.next()) {
        QJsonObject msg;
        const QString msgId = q.value(0).toString();
        const QString senderLogin = q.value(1).toString();
        const QString chatId = q.value(5).toString();
        const QString body = q.value(2).toString();

        // Skip ephemeral/filtered messages server-side
        if (body.isEmpty() || body == QStringLiteral("Openping!")
            || body == QStringLiteral("Themeapplied!")
            || body.startsWith(QStringLiteral("**fnm="))
            || body.startsWith(QStringLiteral("**#"))) {
            continue;
        }

        msg[QStringLiteral("id")] = msgId;
        msg[QStringLiteral("body")] = body;
        msg[QStringLiteral("type")] = q.value(3).toString();
        msg[QStringLiteral("timestamp")] = q.value(4).toDateTime().toMSecsSinceEpoch();

        // Determine peer from chat_id
        const QStringList parts = chatId.split(QLatin1Char(':'));
        QString peerLogin;
        for (const QString &part : parts) {
            if (part.toLower() != userPart) {
                peerLogin = part;
                break;
            }
        }
        if (peerLogin.isEmpty()) {
            peerLogin = userPart;
        }

        // Ensure peer has @domain
        const QString peer = peerLogin.contains(QLatin1Char('@'))
            ? peerLogin
            : peerLogin + QLatin1Char('@') + userDomain;

        const QString senderUser = senderLogin.section(QLatin1Char('@'), 0, 0).toLower();
        if (senderUser == userPart) {
            msg[QStringLiteral("from")] = login;
            msg[QStringLiteral("to")] = peer;
        } else {
            msg[QStringLiteral("from")] = senderLogin;
            msg[QStringLiteral("to")] = login;
        }

        result.prepend(msg);
    }
    return result;
}

bool Database::insertCallRecord(const QJsonObject &record)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "INSERT INTO call_history (uid, owner_id, caller, callee, direction, "
        "start_time, end_time, duration, status) "
        "VALUES (:uid, :oid, :caller, :callee, :dir, :start, :end, :dur, :status) "
        "ON CONFLICT (uid) DO NOTHING"
    ));
    q.bindValue(QStringLiteral(":uid"), record.value(QStringLiteral("uid")).toString());
    q.bindValue(QStringLiteral(":oid"), record.value(QStringLiteral("ownerId")).toInt());
    q.bindValue(QStringLiteral(":caller"), record.value(QStringLiteral("caller")).toString());
    q.bindValue(QStringLiteral(":callee"), record.value(QStringLiteral("callee")).toString());
    q.bindValue(QStringLiteral(":dir"), record.value(QStringLiteral("direction")).toString());
    q.bindValue(QStringLiteral(":start"), QDateTime::fromMSecsSinceEpoch(
        record.value(QStringLiteral("startTime")).toVariant().toLongLong()));
    q.bindValue(QStringLiteral(":end"), QDateTime::fromMSecsSinceEpoch(
        record.value(QStringLiteral("endTime")).toVariant().toLongLong()));
    q.bindValue(QStringLiteral(":dur"), record.value(QStringLiteral("duration")).toInt());
    q.bindValue(QStringLiteral(":status"), record.value(QStringLiteral("status")).toString());
    return q.exec();
}

QList<QJsonObject> Database::callHistory(int ownerId, const QString &type,
                                         const QString &search, int limit)
{
    QList<QJsonObject> result;
    QString whereClause = QStringLiteral("owner_id = :oid");
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "SELECT uid, caller, callee, direction, start_time, end_time, duration, status, note, note_author "
        "FROM call_history WHERE %1 "
        "ORDER BY start_time DESC LIMIT :lim"
    ).arg(whereClause));
    q.bindValue(QStringLiteral(":oid"), ownerId);
    q.bindValue(QStringLiteral(":lim"), limit);

    if (!q.exec()) {
        return result;
    }

    while (q.next()) {
        QJsonObject rec;
        rec[QStringLiteral("uid")] = q.value(0).toString();
        rec[QStringLiteral("caller")] = q.value(1).toString();
        rec[QStringLiteral("callee")] = q.value(2).toString();
        rec[QStringLiteral("direction")] = q.value(3).toString();
        rec[QStringLiteral("start")] = q.value(4).toDateTime().toString(Qt::ISODate);
        rec[QStringLiteral("end")] = q.value(5).toDateTime().toString(Qt::ISODate);
        rec[QStringLiteral("duration")] = q.value(6).toInt();
        rec[QStringLiteral("status")] = q.value(7).toString();
        rec[QStringLiteral("note")] = q.value(8).toString();
        rec[QStringLiteral("noteAuthor")] = q.value(9).toString();
        result.append(rec);
    }
    return result;
}

bool Database::updateCallNote(const QString &uid, const QString &text, const QString &author)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("UPDATE call_history SET note = :text, note_author = :author WHERE uid = :uid"));
    q.bindValue(QStringLiteral(":text"), text);
    q.bindValue(QStringLiteral(":author"), author);
    q.bindValue(QStringLiteral(":uid"), uid);
    return q.exec();
}

bool Database::deleteCallNote(const QString &uid)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("UPDATE call_history SET note = NULL, note_author = NULL WHERE uid = :uid"));
    q.bindValue(QStringLiteral(":uid"), uid);
    return q.exec();
}

} // namespace itl
