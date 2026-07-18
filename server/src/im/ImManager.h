#pragma once

#include <QObject>

namespace itl {

class UserSession;
class Database;
class SessionManager;

class ImManager : public QObject {
    Q_OBJECT

public:
    explicit ImManager(Database *db, QObject *parent = nullptr);

    void setSessionManager(SessionManager *mgr) { m_sessions = mgr; }

    void sendUnreadContacts(UserSession *session);
    void handleSendIm(UserSession *session, int requestId, const QJsonObject &payload);
    void handleLoadHistory(UserSession *session, int requestId, const QJsonObject &payload);

private:
    void deliverToRecipient(const QJsonObject &push, const QString &to);
    QString buildChatId(const QString &loginA, const QString &loginB);

    Database *m_db;
    SessionManager *m_sessions = nullptr;
};

} // namespace itl
