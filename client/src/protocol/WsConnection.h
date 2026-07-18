#pragma once

#include "ProtocolTypes.h"

#include <QHash>
#include <QJsonObject>
#include <QObject>
#include <QTimer>
#include <QWebSocket>

#include <functional>

namespace itl {

class WsConnection : public QObject {
    Q_OBJECT

public:
    static constexpr int requestTimeoutMs = 9000;

    explicit WsConnection(QObject *parent = nullptr);

    QString sid() const { return m_sid; }
    bool isConnected() const { return m_socket && m_socket->state() == QAbstractSocket::ConnectedState; }

    void connectToServer(const QUrl &url, const QString &ssoLogin = {}, bool ignoreInsecureTls = false);
    void disconnectFromServer();

    void sendMessage(const QJsonObject &payload);
    int sendRequestWithResponse(const QJsonObject &request);
    void sendRequest(const QJsonObject &request, std::function<void(const QJsonObject &)> onSuccess = {},
                     std::function<void(const QString &)> onError = {});

    int resolveUser(const QString &login);

signals:
    void connected();
    void disconnected(const QString &reason);
    void connectionFailed(const QString &error, const QString &effectiveLogin = {});
    void payloadReceived(const QJsonObject &payload);
    void responseReceived(int requestId, const QJsonObject &response);

private slots:
    void onSocketConnected();
    void onSocketTextMessage(const QString &message);
    void onSocketDisconnected();
    void onSocketError(QAbstractSocket::SocketError error);
    void sendNoop();

private:
    void sendRaw(const QJsonObject &message);
    void removeAcked(int ack);
    bool processPayload(const QJsonObject &payload);
    void receivePayload(const QJsonObject &message);
    void handleHelloResponse(const QJsonObject &data);
    void handleRuntimeMessage(const QJsonObject &data);
    void resetState();
    void openSocket(const QUrl &url);
    void tryInsecureAlternateScheme();
    static QUrl alternateInsecureScheme(const QUrl &url);
    void applyInsecureTlsOptions(QWebSocket *socket, const QUrl &url);
    void failInitialConnect(const QString &error);

    QWebSocket *m_socket = nullptr;
    QUrl m_connectUrl;
    bool m_ignoreInsecureTls = false;
    bool m_insecureAlternateTried = false;
    bool m_connectedOnce = false;
    QString m_sid;
    QString m_ssoLogin;
    QJsonObject m_extParams;
    int m_ack = 0;
    int m_lastSentAck = 0;
    int m_seq = 0;
    int m_reqSeq = 0;
    bool m_awaitingHello = false;

    QList<QJsonObject> m_unackedMessages;
    QList<QJsonObject> m_deferredPayloads;

    struct PendingRequest {
        QTimer *timer = nullptr;
        std::function<void(const QJsonObject &)> onSuccess;
        std::function<void(const QString &)> onError;
    };
    QHash<int, PendingRequest> m_pendingRequests;

    QTimer m_noopTimer;
};

} // namespace itl
