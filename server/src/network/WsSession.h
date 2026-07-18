#pragma once

#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QWebSocket>
#include <QTimer>

namespace itl {

class UserSession;

class WsSession : public QObject {
    Q_OBJECT

public:
    explicit WsSession(QWebSocket *socket, QObject *parent = nullptr);
    ~WsSession() override;

    QString sid() const;
    bool isHandshakeComplete() const { return m_handshakeComplete; }
    void setHandshakeComplete(bool complete) { m_handshakeComplete = complete; }

    UserSession *userSession() const { return m_userSession; }
    void setUserSession(UserSession *session) { m_userSession = session; }

    void sendJson(const QJsonObject &msg);
    void sendMessage(const QJsonObject &payload);
    void sendResponse(int requestId, const QJsonObject &payload);
    void sendPayload(const QJsonObject &payload);
    void sendBye();

    bool isConnected() const;

signals:
    void textMessageReceived(const QString &message);
    void disconnected();
    void helloReceived(const QJsonObject &hello);

private slots:
    void onSocketTextMessage(const QString &message);
    void onSocketDisconnected();
    void onNoopTimeout();

private:
    void processIncomingMessage(const QJsonObject &msg);

    QWebSocket *m_socket = nullptr;
    UserSession *m_userSession = nullptr;
    bool m_handshakeComplete = false;
    int m_seq = 0;
    int m_ack = 0;
    int m_lastSentAck = 0;
    QTimer m_noopTimer;
};

} // namespace itl
