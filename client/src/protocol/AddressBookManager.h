#pragma once

#include "settings/AppSettings.h"

#include <QHash>
#include <QObject>
#include <QString>

namespace itl {

class WsApiClient;

class AddressBookManager : public QObject {
    Q_OBJECT

public:
    explicit AddressBookManager(WsApiClient *api, QObject *parent = nullptr);

    void setDomain(const QString &domain);

    QList<CustomContact> contacts() const;
    QString serverIdForPeer(const QString &peer) const;

    int createContact(const CustomContact &contact);
    int deleteContactByPeer(const QString &peer);
    int uploadLocalContacts(const QList<CustomContact> &local);

    void handlePayload(const QJsonObject &payload);
    void handleResponse(int requestId, const QJsonObject &response);
    void clear();

    static QString normalizePhone(QString phone);

signals:
    void contactsChanged();
    void uploadCompleted(bool success);
    void deleteFailed(const QString &peer, const QString &reason);

private:
    static QString normalizeSubId(const QString &raw);
    static bool peersMatch(const QString &a, const QString &b, const QString &domain);
    static QString buildServerId(const QString &rawId, const QString &subId);
    QString findServerIdForPeer(const QString &peer) const;
  bool removeContactByServerId(const QString &serverId, bool emitChanged = true);
    static QString peerFromAbObject(const QJsonObject &obj, const QString &domain);
    static QJsonObject contactToServerJson(const CustomContact &contact);
    CustomContact parseAbContact(const QJsonObject &obj, const QString &subId, const QString &domain) const;

    WsApiClient *m_api = nullptr;
    QString m_domain;
    QHash<QString, CustomContact> m_byServerId;
    QHash<QString, QString> m_peerToServerId;
    QHash<int, QString> m_pendingCreatePeers;
    QHash<int, QString> m_pendingDeleteServerIds;
    int m_uploadRequestId = -1;
};

} // namespace itl
