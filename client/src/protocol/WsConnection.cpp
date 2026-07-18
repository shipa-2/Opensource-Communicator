#include "WsConnection.h"

#include <QAbstractSocket>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLoggingCategory>
#include <QSslConfiguration>
#include <QSslError>
#include <QSslSocket>

Q_LOGGING_CATEGORY(lcWs, "itl.ws")

namespace itl {

namespace {
QJsonObject redactPassword(const QJsonObject &obj)
{
  QJsonObject copy = obj;
  if (copy.contains(QString::fromUtf8(kEmptyKey)) && copy.value(QString::fromUtf8(kEmptyKey)).isObject()) {
    QJsonObject inner = copy.value(QString::fromUtf8(kEmptyKey)).toObject();
    if (inner.contains(QStringLiteral("password"))) {
      inner.insert(QStringLiteral("password"), QStringLiteral("***"));
      copy.insert(QString::fromUtf8(kEmptyKey), inner);
    }
  }
  return copy;
}
} // namespace

WsConnection::WsConnection(QObject *parent)
    : QObject(parent)
{
  m_noopTimer.setSingleShot(true);
  connect(&m_noopTimer, &QTimer::timeout, this, &WsConnection::sendNoop);
}

QUrl WsConnection::alternateInsecureScheme(const QUrl &url)
{
  QUrl alternate = url;
  if (url.scheme() == QStringLiteral("wss")) {
    alternate.setScheme(QStringLiteral("ws"));
  } else if (url.scheme() == QStringLiteral("ws")) {
    alternate.setScheme(QStringLiteral("wss"));
  }
  return alternate;
}

void WsConnection::applyInsecureTlsOptions(QWebSocket *socket, const QUrl &url)
{
  if (!m_ignoreInsecureTls || url.scheme() != QStringLiteral("wss") || !socket) {
    return;
  }

  QSslConfiguration sslConfig = socket->sslConfiguration();
  sslConfig.setPeerVerifyMode(QSslSocket::VerifyNone);
  socket->setSslConfiguration(sslConfig);
  connect(socket, &QWebSocket::sslErrors, socket, [socket](const QList<QSslError> &errors) {
    socket->ignoreSslErrors(errors);
  });
}

void WsConnection::openSocket(const QUrl &url)
{
  if (m_socket) {
    m_socket->disconnect(this);
    m_socket->close();
    m_socket->deleteLater();
    m_socket = nullptr;
  }

  m_connectUrl = url;
  m_awaitingHello = true;

  m_socket = new QWebSocket(QString(), QWebSocketProtocol::VersionLatest, this);
  connect(m_socket, &QWebSocket::connected, this, &WsConnection::onSocketConnected);
  connect(m_socket, &QWebSocket::textMessageReceived, this, &WsConnection::onSocketTextMessage);
  connect(m_socket, &QWebSocket::disconnected, this, &WsConnection::onSocketDisconnected);
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
  connect(m_socket, &QWebSocket::errorOccurred, this, &WsConnection::onSocketError);
#else
  connect(m_socket, QOverload<QAbstractSocket::SocketError>::of(&QWebSocket::error),
          this, &WsConnection::onSocketError);
#endif

  applyInsecureTlsOptions(m_socket, url);

  qCInfo(lcWs) << "Connecting to" << url;
  m_socket->open(url);
}

void WsConnection::tryInsecureAlternateScheme()
{
  if (!m_ignoreInsecureTls || m_insecureAlternateTried) {
    return;
  }

  const QUrl alternate = alternateInsecureScheme(m_connectUrl);
  if (alternate.scheme() == m_connectUrl.scheme()) {
    return;
  }

  m_insecureAlternateTried = true;
  qCInfo(lcWs) << "Insecure connect failed, retrying with" << alternate;
  openSocket(alternate);
}

void WsConnection::connectToServer(const QUrl &url, const QString &ssoLogin, bool ignoreInsecureTls)
{
  if (m_socket) {
    return;
  }

  m_ssoLogin = ssoLogin;
  m_ignoreInsecureTls = ignoreInsecureTls;
  m_insecureAlternateTried = false;
  openSocket(url);
}

void WsConnection::disconnectFromServer()
{
  if (!m_socket) {
    return;
  }

  m_socket->disconnect(this);
  m_socket->close();
  m_socket->deleteLater();
  m_socket = nullptr;
  m_noopTimer.stop();
  resetState();
}

void WsConnection::resetState()
{
  m_sid.clear();
  m_ack = 0;
  m_lastSentAck = 0;
  m_seq = 0;
  m_reqSeq = 0;
  m_unackedMessages.clear();
  m_deferredPayloads.clear();
  m_awaitingHello = false;
  m_ignoreInsecureTls = false;
  m_insecureAlternateTried = false;
  m_connectedOnce = false;
  m_connectUrl = QUrl();

  for (auto it = m_pendingRequests.begin(); it != m_pendingRequests.end(); ++it) {
    if (it->timer) {
      it->timer->stop();
      it->timer->deleteLater();
    }
  }
  m_pendingRequests.clear();
}

void WsConnection::sendRaw(const QJsonObject &message)
{
  if (!m_socket) {
    return;
  }

  qCInfo(lcWs) << "Send:" << QJsonDocument(redactPassword(message)).toJson(QJsonDocument::Compact);
  m_socket->sendTextMessage(QJsonDocument(message).toJson(QJsonDocument::Compact));

  m_noopTimer.stop();
  m_noopTimer.start(57000);
}

void WsConnection::sendNoop()
{
  QJsonObject payload;
  payload.insert(QString::fromUtf8(kEmptyKey), QStringLiteral("noop"));
  sendMessage(payload);
}

void WsConnection::failInitialConnect(const QString &error)
{
  if (m_socket) {
    m_socket->disconnect(this);
    m_socket->deleteLater();
    m_socket = nullptr;
  }
  resetState();
  emit connectionFailed(error);
}

void WsConnection::onSocketConnected()
{
  m_connectedOnce = true;
  QJsonObject hello;
  if (!m_sid.isEmpty()) {
    hello.insert(QStringLiteral("sid"), m_sid);
    hello.insert(QStringLiteral("ack"), m_ack);
    hello.insert(QStringLiteral("seq"), 0);

    if (!m_unackedMessages.isEmpty()) {
      hello.insert(QStringLiteral("seq"), m_unackedMessages.first().value(QStringLiteral("seq")).toInt());
      QJsonArray payloads;
      for (const auto &msg : m_unackedMessages) {
        payloads.append(msg.value(QStringLiteral("payload")));
      }
      hello.insert(QStringLiteral("payloads"), payloads);
    }
  } else {
    QJsonObject params{
        {QStringLiteral("apiVersion"), 2},
        {QStringLiteral("platform"), QStringLiteral("Linux")},
        {QStringLiteral("appId"), QStringLiteral("Communicator")},
    };
    for (auto it = m_extParams.begin(); it != m_extParams.end(); ++it) {
      params.insert(it.key(), it.value());
    }

    hello.insert(QStringLiteral("task"), QStringLiteral("Main"));
    hello.insert(QStringLiteral("useragent"), QStringLiteral("opensource-communicator/0.2 Qt6"));
    hello.insert(QStringLiteral("params"), params);
  }

  sendRaw(hello);
}

void WsConnection::handleHelloResponse(const QJsonObject &data)
{
  qCInfo(lcWs) << "Received:" << QJsonDocument(data).toJson(QJsonDocument::Compact);

  const QString status = data.value(QStringLiteral("status")).toString();
  if (status == QStringLiteral("ok")) {
    if (data.contains(QStringLiteral("sid"))) {
      m_sid = data.value(QStringLiteral("sid")).toString();
      m_awaitingHello = false;
      if (!m_ssoLogin.isEmpty()) {
        resolveUser(m_ssoLogin);
        return;
      }
      emit connected();
    } else {
      receivePayload(data);
      m_awaitingHello = false;
    }
    return;
  }

  const QJsonObject payload = data.value(QStringLiteral("payload")).toObject();
  const QJsonObject inner = payload.value(QString::fromUtf8(kEmptyKey)).toObject();
  if (inner.contains(QStringLiteral("user"))) {
    emit connectionFailed(QStringLiteral("ssoReconnect"), inner.value(QStringLiteral("user")).toString());
  } else if (inner.contains(QStringLiteral("error"))) {
    emit connectionFailed(inner.value(QStringLiteral("error")).toString());
  } else {
    emit connectionFailed(QStringLiteral("ssoError"));
  }
}

void WsConnection::handleRuntimeMessage(const QJsonObject &data)
{
  qCInfo(lcWs) << "Received:" << QJsonDocument(data).toJson(QJsonDocument::Compact);

  if (data.contains(QStringLiteral("bye"))) {
    resetState();
    emit disconnected(QStringLiteral("bye"));
    return;
  }

  receivePayload(data);
}

void WsConnection::onSocketTextMessage(const QString &message)
{
  const QJsonObject data = QJsonDocument::fromJson(message.toUtf8()).object();
  if (data.isEmpty()) {
    qCWarning(lcWs) << "Invalid JSON message";
    return;
  }

  if (m_awaitingHello) {
    handleHelloResponse(data);
    return;
  }

  handleRuntimeMessage(data);
}

void WsConnection::removeAcked(int ack)
{
  if (m_unackedMessages.isEmpty()) {
    return;
  }

  if (m_unackedMessages.last().value(QStringLiteral("seq")).toInt() <= ack) {
    m_unackedMessages.clear();
    return;
  }

  while (!m_unackedMessages.isEmpty() && m_unackedMessages.first().value(QStringLiteral("seq")).toInt() <= ack) {
    m_unackedMessages.removeFirst();
  }
}

bool WsConnection::processPayload(const QJsonObject &payload)
{
  const QString what = payload.value(QStringLiteral("What")).toString();

  if (what == QStringLiteral("request")) {
  return false;
  }

  if (what == QStringLiteral("response")) {
    const int id = payload.value(QStringLiteral("id")).toInt();
    if (m_pendingRequests.contains(id)) {
      PendingRequest pending = m_pendingRequests.take(id);
      if (pending.timer) {
        pending.timer->stop();
        pending.timer->deleteLater();
      }

      const QJsonObject inner = payload.value(QString::fromUtf8(kEmptyKey)).toObject();
      if (inner.value(QStringLiteral("error")).toBool()) {
        if (pending.onError) {
          pending.onError(inner.value(QStringLiteral("error")).toString());
        }
      } else if (pending.onSuccess) {
        pending.onSuccess(payload);
      }
      emit responseReceived(id, payload);
    }
    return false;
  }

  emit payloadReceived(payload);
  return false;
}

void WsConnection::receivePayload(const QJsonObject &message)
{
  const int ack = message.value(QStringLiteral("ack")).toInt();
  removeAcked(ack);

  QList<QJsonObject> deferred;

  if (message.contains(QStringLiteral("payloads"))) {
    int curSeq = message.value(QStringLiteral("seq")).toInt();
    const QJsonArray payloads = message.value(QStringLiteral("payloads")).toArray();
    for (const auto &value : payloads) {
      const QJsonObject payload = value.toObject();
      if (!payload.isEmpty()) {
        if (processPayload(payload)) {
          deferred.append(payload);
        }
      }
      if (curSeq > m_ack) {
        m_ack = curSeq;
      }
      ++curSeq;
    }
  } else {
    const int seq = message.value(QStringLiteral("seq")).toInt();
    const QJsonObject payload = message.value(QStringLiteral("payload")).toObject();
    if (!payload.isEmpty()) {
      if (processPayload(payload)) {
        deferred.append(payload);
      }
    }
    if (seq > m_ack) {
      m_ack = seq;
    }
  }

  if (m_ack - m_lastSentAck > 4) {
    sendRaw(QJsonObject{{QStringLiteral("ack"), m_ack}});
    m_lastSentAck = m_ack;
  }

  for (const auto &payload : m_deferredPayloads) {
    if (processPayload(payload)) {
      deferred.append(payload);
    }
  }
  m_deferredPayloads = deferred;
}

void WsConnection::sendMessage(const QJsonObject &payload)
{
  if (!m_socket) {
    return;
  }

  ++m_seq;
  QJsonObject msg{
      {QStringLiteral("seq"), m_seq},
      {QStringLiteral("ack"), m_ack},
      {QStringLiteral("payload"), payload},
  };

  m_lastSentAck = m_ack;
  m_unackedMessages.append(msg);
  sendRaw(msg);
}

void WsConnection::sendRequest(const QJsonObject &request,
                               std::function<void(const QJsonObject &)> onSuccess,
                               std::function<void(const QString &)> onError)
{
  const int reqId = m_reqSeq++;
  QJsonObject payload{
      {QStringLiteral("What"), QStringLiteral("request")},
      {QStringLiteral("id"), reqId},
      {QString::fromUtf8(kEmptyKey), request},
  };

  auto *timer = new QTimer(this);
  timer->setSingleShot(true);
  timer->start(requestTimeoutMs);

  PendingRequest pending{timer, std::move(onSuccess), std::move(onError)};
  connect(timer, &QTimer::timeout, this, [this, reqId]() {
    if (!m_pendingRequests.contains(reqId)) {
      return;
    }
    PendingRequest p = m_pendingRequests.take(reqId);
    if (p.onError) {
      p.onError(QStringLiteral("timeout"));
    }
    emit responseReceived(reqId, QJsonObject{{QStringLiteral("error"), QStringLiteral("timeout")}});
  });

  m_pendingRequests.insert(reqId, pending);
  sendMessage(payload);
}

int WsConnection::sendRequestWithResponse(const QJsonObject &request)
{
  const int reqId = m_reqSeq++;
  QJsonObject payload{
      {QStringLiteral("What"), QStringLiteral("request")},
      {QStringLiteral("id"), reqId},
      {QString::fromUtf8(kEmptyKey), request},
  };

  auto *timer = new QTimer(this);
  timer->setSingleShot(true);
  timer->start(requestTimeoutMs);

  PendingRequest pending;
  pending.timer = timer;
  connect(timer, &QTimer::timeout, this, [this, reqId]() {
    if (!m_pendingRequests.contains(reqId)) {
      return;
    }
    m_pendingRequests.remove(reqId);
    emit responseReceived(reqId, QJsonObject{{QStringLiteral("error"), QStringLiteral("timeout")}});
  });

  m_pendingRequests.insert(reqId, pending);
  sendMessage(payload);
  return reqId;
}

int WsConnection::resolveUser(const QString &login)
{
  QJsonObject request{
      {QString::fromUtf8(kEmptyKey), QStringLiteral("resolveuser")},
      {QStringLiteral("login"), login},
  };
  return sendRequestWithResponse(request);
}

void WsConnection::onSocketDisconnected()
{
  if (!m_socket || sender() != m_socket) {
    return;
  }

  m_noopTimer.stop();
  if (m_connectedOnce) {
    resetState();
    emit disconnected(QStringLiteral("socket_closed"));
    return;
  }

  if (m_ignoreInsecureTls && !m_insecureAlternateTried) {
    tryInsecureAlternateScheme();
    return;
  }

  failInitialConnect(m_socket->errorString().isEmpty() ? QStringLiteral("socket_closed")
                                                       : m_socket->errorString());
}

void WsConnection::onSocketError(QAbstractSocket::SocketError)
{
  if (!m_socket || sender() != m_socket) {
    return;
  }

  if (m_connectedOnce) {
    emit connectionFailed(m_socket->errorString());
    return;
  }

  if (m_ignoreInsecureTls && !m_insecureAlternateTried) {
    tryInsecureAlternateScheme();
    return;
  }

  failInitialConnect(m_socket->errorString());
}

} // namespace itl
