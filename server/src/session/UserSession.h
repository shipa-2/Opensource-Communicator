#pragma once

#include <QDateTime>
#include <QObject>
#include <QString>

namespace itl {

class WsSession;

class UserSession : public QObject {
    Q_OBJECT

public:
    explicit UserSession(const QString &sid, QObject *parent = nullptr);

    QString sid() const { return m_sid; }
    QString login() const { return m_login; }
    QString domain() const { return m_domain; }
    QString partner() const { return m_partner; }
    QString role() const { return m_role; }
    int userId() const { return m_userId; }

    void setLogin(const QString &login) { m_login = login; }
    void setDomain(const QString &domain) { m_domain = domain; }
    void setPartner(const QString &partner) { m_partner = partner; }
    void setRole(const QString &role) { m_role = role; }
    void setUserId(int id) { m_userId = id; }

    bool isBound() const { return m_bound; }
    void setBound(bool bound) { m_bound = bound; }

    bool isImBound() const { return m_imBound; }
    void setImBound(bool bound) { m_imBound = bound; }

    bool isAddressBookSubscribed() const { return m_abSubscribed; }
    void setAddressBookSubscribed(bool sub) { m_abSubscribed = sub; }

    QString presenceStatus() const { return m_presenceStatus; }
    void setPresenceStatus(const QString &status) { m_presenceStatus = status; }

    WsSession *wsSession() const { return m_ws; }
    void setWsSession(WsSession *ws) { m_ws = ws; }

    qint64 lastActivityMs() const { return m_lastActivityMs; }
    void touch() { m_lastActivityMs = QDateTime::currentMSecsSinceEpoch(); }

private:
    QString m_sid;
    QString m_login;
    QString m_domain;
    QString m_partner;
    QString m_role;
    int m_userId = -1;
    bool m_bound = false;
    bool m_imBound = false;
    bool m_abSubscribed = false;
    QString m_presenceStatus = QStringLiteral("offline");
    WsSession *m_ws = nullptr;
    qint64 m_lastActivityMs = 0;
};

} // namespace itl
