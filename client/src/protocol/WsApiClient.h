#pragma once

#include "ProtocolTypes.h"
#include "WsConnection.h"

#include <QObject>

namespace itl {

class WsApiClient : public QObject {
    Q_OBJECT

public:
    explicit WsApiClient(QObject *parent = nullptr);

    AppState appState() const { return m_appState; }
    QString sid() const;
    bool isConnected() const;

    void initialize(const QUrl &url, const QString &ssoLogin = {});
    void disconnect();

    void login(const QString &username, const QString &password, const QString &partner = {});
    void sendBye();

    void bind();
    void bindIm(const QString &lastKnownId = {}, int maxHistSize = 32767, bool loadContacts = true);
    void subscribeToAddressBook();

    int createContact(const QJsonObject &contact);
    int deleteContact(const QString &contactId);
    int uploadContacts(const QJsonArray &contacts);

    void provisionCall(const QString &leg);
    void startCall(const QString &leg, const QString &peer, const QString &localSdp,
                   const QString &callerId = {}, const QJsonObject &conference = {});
    void acceptCall(const QString &leg, const QString &localSdp);
    void rejectCall(const QString &leg, int code, const QString &reason);
    void cancelCall(const QString &leg, int code, const QString &reason);
    void disconnectCall(const QString &leg, int code);
    void ackAccept(const QString &leg);
    void updateCall(const QString &leg, const QString &localSdp);
    void blindTransfer(const QString &leg, const QString &peer);

    void setOwnPresence(const QString &status, bool manual = true);
    void sendSms(const QString &from, const QString &to, const QString &text);

    void getDomainContacts();
    int getHistory(const QJsonObject &params);
    int getSmsTelnums();

    int sendIm(const QString &peer, const QString &body, const QString &type = {},
               bool persist = true, bool copyToSelf = true);
    int sendIm(const QString &peer, const QStringList &body, const QString &type);
    int loadImHistory(const QString &lastLoadedId, int loadSize = 32767);

signals:
    void connectionEstablished();
    void connectionClosed(const QString &reason);
    void connectionFailed(const QString &error);
    void authResult(bool success, const QJsonObject &payload);
    void bindResult(bool success);
    void serverPayload(const QJsonObject &payload);
    void responseReceived(int requestId, const QJsonObject &response);
    void domainContactsLoaded(const QJsonObject &contacts);
    void historyLoaded(int requestId, const QJsonObject &data);
    void smsChannelsLoaded(int requestId, const QJsonObject &data);

private slots:
    void onConnected();
    void onDisconnected(const QString &reason);
    void onConnectionFailed(const QString &error, const QString &effectiveLogin);
    void onPayload(const QJsonObject &payload);
    void onResponse(int requestId, const QJsonObject &response);

private:
    bool ensureOnline() const;

    WsConnection *m_connection = nullptr;
    AppState m_appState = AppState::Offline;
    bool m_isConnected = false;
};

} // namespace itl
