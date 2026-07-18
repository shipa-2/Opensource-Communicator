#include "WsSession.h"
#include "../session/UserSession.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(lcWs, "server.ws")

namespace itl {

WsSession::WsSession(QWebSocket *socket, QObject *parent)
    : QObject(parent)
    , m_socket(socket)
{
    connect(m_socket, &QWebSocket::textMessageReceived, this, &WsSession::onSocketTextMessage);
    connect(m_socket, &QWebSocket::disconnected, this, &WsSession::onSocketDisconnected);

    m_noopTimer.setSingleShot(true);
    connect(&m_noopTimer, &QTimer::timeout, this, &WsSession::onNoopTimeout);
    m_noopTimer.start(60000);
}

WsSession::~WsSession() = default;

QString WsSession::sid() const
{
    return m_userSession ? m_userSession->sid() : QString();
}

void WsSession::sendJson(const QJsonObject &msg)
{
    if (!m_socket || m_socket->state() != QAbstractSocket::ConnectedState) {
        return;
    }
    qCInfo(lcWs) << "Send:" << QJsonDocument(msg).toJson(QJsonDocument::Compact);
    m_socket->sendTextMessage(QJsonDocument(msg).toJson(QJsonDocument::Compact));
    m_noopTimer.stop();
    m_noopTimer.start(60000);
}

void WsSession::sendMessage(const QJsonObject &payload)
{
    ++m_seq;
    QJsonObject msg{
        {QStringLiteral("seq"), m_seq},
        {QStringLiteral("ack"), m_ack},
        {QStringLiteral("payload"), payload},
    };
    m_lastSentAck = m_ack;
    sendJson(msg);
}

void WsSession::sendResponse(int requestId, const QJsonObject &payload)
{
    // Megafon/ITooLabs protocol: response fields go inside "" key
    QJsonObject wrapped{
        {QStringLiteral("What"), QStringLiteral("response")},
        {QStringLiteral("id"), requestId},
        {QStringLiteral(""), payload},
    };
    sendMessage(wrapped);
}

void WsSession::sendPayload(const QJsonObject &payload)
{
    sendMessage(payload);
}

void WsSession::sendBye()
{
    QJsonObject msg;
    msg.insert(QStringLiteral("bye"), true);
    sendJson(msg);
}

bool WsSession::isConnected() const
{
    return m_socket && m_socket->state() == QAbstractSocket::ConnectedState;
}

void WsSession::onSocketTextMessage(const QString &message)
{
    const QJsonObject data = QJsonDocument::fromJson(message.toUtf8()).object();
    if (data.isEmpty()) {
        qCWarning(lcWs) << "Invalid JSON from client";
        return;
    }

    m_noopTimer.stop();
    m_noopTimer.start(60000);

    if (!m_handshakeComplete) {
        emit helloReceived(data);
        return;
    }

    processIncomingMessage(data);
}

void WsSession::processIncomingMessage(const QJsonObject &msg)
{
    // Update ack
    const int incomingAck = msg.value(QStringLiteral("ack")).toInt();
    if (incomingAck > m_ack) {
        m_ack = incomingAck;
    }

    // Handle payloads array
    if (msg.contains(QStringLiteral("payloads"))) {
        const QJsonArray payloads = msg.value(QStringLiteral("payloads")).toArray();
        for (const QJsonValue &v : payloads) {
            const QJsonObject payload = v.toObject();
            if (!payload.isEmpty()) {
                emit textMessageReceived(QJsonDocument(payload).toJson(QJsonDocument::Compact));
            }
        }
        return;
    }

    // Handle single payload
    if (msg.contains(QStringLiteral("payload"))) {
        const QJsonObject payload = msg.value(QStringLiteral("payload")).toObject();
        if (!payload.isEmpty()) {
            emit textMessageReceived(QJsonDocument(payload).toJson(QJsonDocument::Compact));
        }
        return;
    }

    // Handle bare ack
    if (msg.contains(QStringLiteral("ack")) && !msg.contains(QStringLiteral("payload"))) {
        return;
    }

    // Handle bye
    if (msg.contains(QStringLiteral("bye"))) {
        emit disconnected();
        return;
    }

    // Handle noop
    const QString cmd = msg.value(QString::fromUtf8("")).toString();
    if (cmd == QStringLiteral("noop")) {
        return;
    }

    // Otherwise treat entire message as payload (for handshake responses etc.)
    emit textMessageReceived(QJsonDocument(msg).toJson(QJsonDocument::Compact));
}

void WsSession::onSocketDisconnected()
{
    m_noopTimer.stop();
    emit disconnected();
}

void WsSession::onNoopTimeout()
{
    qCWarning(lcWs) << "Client noop timeout, disconnecting:" << sid();
    if (m_socket) {
        m_socket->close();
    }
}

} // namespace itl
