#include "WsServer.h"
#include "WsSession.h"

#include <QFile>
#include <QLoggingCategory>
#include <QSslKey>
#include <QWebSocketServer>

Q_LOGGING_CATEGORY(lcWsServer, "server.wsserver")

namespace itl {

class WsServer::Impl {
public:
    QWebSocketServer *server = nullptr;
    QList<WsSession *> sessions;
};

WsServer::WsServer(QObject *parent)
    : QObject(parent)
    , m_impl(new Impl)
{
}

WsServer::~WsServer()
{
    stop();
    delete m_impl;
}

bool WsServer::start(const QString &host, quint16 port)
{
    m_impl->server = new QWebSocketServer(
        QStringLiteral("OpenSource Communicator Server"),
        QWebSocketServer::NonSecureMode,
        this);

    connect(m_impl->server, &QWebSocketServer::newConnection,
            this, &WsServer::onNewConnection);

    if (!m_impl->server->listen(QHostAddress(host), port)) {
        qCCritical(lcWsServer) << "Failed to listen on" << host << ":" << port
                               << m_impl->server->errorString();
        return false;
    }

    qCInfo(lcWsServer) << "Listening on ws://" << host << ":" << port;
    return true;
}

bool WsServer::startTls(const QString &host, quint16 port,
                         const QString &certPath, const QString &keyPath)
{
    QFile certFile(certPath);
    if (!certFile.open(QIODevice::ReadOnly)) {
        qCCritical(lcWsServer) << "Cannot open certificate:" << certPath;
        return false;
    }
    QFile keyFile(keyPath);
    if (!keyFile.open(QIODevice::ReadOnly)) {
        qCCritical(lcWsServer) << "Cannot open private key:" << keyPath;
        return false;
    }

    QSslCertificate cert(&certFile, QSsl::Pem);
    QSslKey key(&keyFile, QSsl::Rsa, QSsl::Pem);
    certFile.close();
    keyFile.close();

    m_impl->server = new QWebSocketServer(
        QStringLiteral("OpenSource Communicator Server"),
        QWebSocketServer::SecureMode,
        this);

    QSslConfiguration sslConfig = m_impl->server->sslConfiguration();
    sslConfig.setLocalCertificate(cert);
    sslConfig.setPrivateKey(key);
    sslConfig.setPeerVerifyMode(QSslSocket::VerifyNone);
    m_impl->server->setSslConfiguration(sslConfig);

    connect(m_impl->server, &QWebSocketServer::newConnection,
            this, &WsServer::onNewConnection);

    if (!m_impl->server->listen(QHostAddress(host), port)) {
        qCCritical(lcWsServer) << "Failed to listen on" << host << ":" << port
                               << m_impl->server->errorString();
        return false;
    }

    qCInfo(lcWsServer) << "Listening on wss://" << host << ":" << port;
    return true;
}

void WsServer::stop()
{
    if (m_impl->server) {
        m_impl->server->close();
        m_impl->server->deleteLater();
        m_impl->server = nullptr;
    }

    for (WsSession *s : m_impl->sessions) {
        s->deleteLater();
    }
    m_impl->sessions.clear();
}

bool WsServer::isListening() const
{
    return m_impl->server && m_impl->server->isListening();
}

void WsServer::onNewConnection()
{
    QWebSocket *socket = m_impl->server->nextPendingConnection();
    if (!socket) {
        return;
    }

    auto *session = new WsSession(socket, this);
    m_impl->sessions.append(session);

    connect(session, &WsSession::disconnected, this, &WsServer::onSocketDisconnected);

    qCInfo(lcWsServer) << "New connection from" << socket->peerAddress().toString();
    emit newSession(session);
}

void WsServer::onSocketDisconnected()
{
    auto *session = qobject_cast<WsSession *>(sender());
    if (!session) {
        return;
    }

    m_impl->sessions.removeOne(session);
    qCInfo(lcWsServer) << "Connection closed:" << session->sid();
    emit sessionClosed(session);
    session->deleteLater();
}

} // namespace itl
