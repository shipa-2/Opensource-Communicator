#pragma once

#include "AddressBookManager.h"
#include "WsApiClient.h"
#include "settings/AppSettings.h"

#include <QDateTime>
#include <QList>
#include <QObject>
#include <QSettings>

namespace itl {

struct LoginCredentials {
    QString login;
    QString password;
    QString domain;
    QString authDomain;
    QString partner;
    int serverPort = 0; // 0 = wss default (443)
    bool ignoreInsecureTls = false; // Debug: self-signed wss or plain ws
};

} // namespace itl

Q_DECLARE_METATYPE(itl::LoginCredentials)

namespace itl {

class ChatManager;

class CommunicatorClient : public QObject {
    Q_OBJECT

public:
    explicit CommunicatorClient(QObject *parent = nullptr);
    ~CommunicatorClient() override;

    WsApiClient *api() { return &m_api; }
    ChatManager *chat() { return m_chat; }
    AddressBookManager *addressBook() { return m_addressBook; }
    AppSettings &appSettings() { return m_appSettings; }
    AppState state() const { return m_api.appState(); }

    void loadSettings();
    void saveSettings();

    LoginCredentials credentials() const { return m_credentials; }
    void setCredentials(const LoginCredentials &credentials);

    bool rememberMe() const { return m_rememberMe; }
    void setRememberMe(bool remember);
    QList<LoginCredentials> savedAccounts() const { return m_savedAccounts; }
    void rememberAccount(const LoginCredentials &credentials);
    void removeSavedAccount(const QString &login);

    void login();
    void logout();
    void enterDemoMode();
    void leaveDemoMode();
    bool isDemoMode() const { return m_demoMode; }

    bool serverVideoEnabled() const { return m_serverVideoEnabled; }

    QString buildWebSocketUrl() const;

signals:
    void stateChanged(itl::AppState state);
    void statusMessage(const QString &message);
    void chatMessage(const QString &peer, const QString &text, bool incoming, const QDateTime &timestamp);
    void contactUpdated(const QString &peer, const QString &name, const QString &presence);
    void addressBookChanged();
    void callEvent(const QString &leg, const QString &what, const QJsonObject &payload);
    void serverVideoEnabledChanged(bool enabled);

private slots:
    void onConnectionEstablished();
    void onAuthResult(bool success, const QJsonObject &payload);
    void onServerPayload(const QJsonObject &payload);
    void onConnectionFailed(const QString &error);
    void onConnectionClosed(const QString &reason);
    void onApiResponse(int requestId, const QJsonObject &response);

private:
    void handlePresencePayload(const QJsonObject &payload);
    void setServerVideoEnabled(bool enabled);
    void loadSavedAccounts();
    void saveSavedAccounts();
    static QString accountKey(const LoginCredentials &credentials);

    WsApiClient m_api;
    ChatManager *m_chat = nullptr;
    AddressBookManager *m_addressBook = nullptr;
    LoginCredentials m_credentials;
    QList<LoginCredentials> m_savedAccounts;
    AppSettings m_appSettings;
    QSettings m_settings;
    bool m_demoMode = false;
    bool m_rememberMe = true;
    bool m_serverVideoEnabled = false;
};

} // namespace itl
