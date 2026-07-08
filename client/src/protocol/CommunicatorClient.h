#pragma once

#include "WsApiClient.h"
#include "settings/AppSettings.h"

#include <QObject>
#include <QSettings>

namespace itl {

struct LoginCredentials {
    QString login;
    QString password;
    QString domain;
    QString authDomain;
    QString partner;
};

class ChatManager;

class CommunicatorClient : public QObject {
    Q_OBJECT

public:
    explicit CommunicatorClient(QObject *parent = nullptr);
    ~CommunicatorClient() override;

    WsApiClient *api() { return &m_api; }
    ChatManager *chat() { return m_chat; }
    AppSettings &appSettings() { return m_appSettings; }
    AppState state() const { return m_api.appState(); }

    void loadSettings();
    void saveSettings();

    LoginCredentials credentials() const { return m_credentials; }
    void setCredentials(const LoginCredentials &credentials);

    void login();
    void logout();

    QString buildWebSocketUrl() const;

signals:
    void stateChanged(itl::AppState state);
    void statusMessage(const QString &message);
    void chatMessage(const QString &peer, const QString &text, bool incoming);
    void contactUpdated(const QString &peer, const QString &name, const QString &presence);
    void callEvent(const QString &leg, const QString &what, const QJsonObject &payload);

private slots:
    void onConnectionEstablished();
    void onAuthResult(bool success, const QJsonObject &payload);
    void onServerPayload(const QJsonObject &payload);
    void onConnectionFailed(const QString &error);
    void onConnectionClosed(const QString &reason);
    void onApiResponse(int requestId, const QJsonObject &response);

private:
    void handlePresencePayload(const QJsonObject &payload);

    WsApiClient m_api;
    ChatManager *m_chat = nullptr;
    LoginCredentials m_credentials;
    AppSettings m_appSettings;
    QSettings m_settings;
};

} // namespace itl
