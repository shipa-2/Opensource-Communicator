#include "WsApiClient.h"

#include "AddressBookManager.h"

#include <QJsonDocument>
#include <QJsonArray>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(lcApi, "itl.api")

namespace itl {

WsApiClient::WsApiClient(QObject *parent)
    : QObject(parent)
{
}

QString WsApiClient::sid() const
{
  return m_connection ? m_connection->sid() : QString();
}

bool WsApiClient::isConnected() const
{
  return m_connection && m_connection->isConnected();
}

void WsApiClient::initialize(const QUrl &url, const QString &ssoLogin)
{
  disconnect();

  m_connection = new WsConnection(this);
  connect(m_connection, &WsConnection::connected, this, &WsApiClient::onConnected);
  connect(m_connection, &WsConnection::disconnected, this, &WsApiClient::onDisconnected);
  connect(m_connection, &WsConnection::connectionFailed, this, &WsApiClient::onConnectionFailed);
  connect(m_connection, &WsConnection::payloadReceived, this, &WsApiClient::onPayload);
  connect(m_connection, &WsConnection::responseReceived, this, &WsApiClient::onResponse);

  m_connection->connectToServer(url, ssoLogin);
}

void WsApiClient::disconnect()
{
  if (!m_connection) {
    return;
  }

  m_connection->disconnectFromServer();
  m_connection->deleteLater();
  m_connection = nullptr;
  m_isConnected = false;
  m_appState = AppState::Offline;
}

bool WsApiClient::ensureOnline() const
{
  return m_connection && m_isConnected && m_appState == AppState::Online;
}

void WsApiClient::onConnected()
{
  m_isConnected = true;
  emit connectionEstablished();
}

void WsApiClient::onDisconnected(const QString &reason)
{
  m_isConnected = false;
  m_appState = AppState::Offline;
  emit connectionClosed(reason);
}

void WsApiClient::onConnectionFailed(const QString &error, const QString &effectiveLogin)
{
  Q_UNUSED(effectiveLogin)
  m_isConnected = false;
  m_appState = AppState::Offline;
  emit connectionFailed(error);
}

void WsApiClient::onPayload(const QJsonObject &payload)
{
  emit serverPayload(payload);
}

void WsApiClient::onResponse(int requestId, const QJsonObject &response)
{
  const QJsonObject inner = response.value(QString::fromUtf8(kEmptyKey)).toObject();
  if (inner.contains(QStringLiteral("response"))) {
    const QJsonObject data = inner.value(QStringLiteral("response")).toObject();
    if (data.contains(QStringLiteral("accList")) || data.contains(QStringLiteral("accounts"))) {
      emit domainContactsLoaded(data);
    }
  }

  emit responseReceived(requestId, response);
  emit historyLoaded(requestId, response);
  emit smsChannelsLoaded(requestId, response);
}

void WsApiClient::login(const QString &username, const QString &password, const QString &partner)
{
  if (!m_connection || !m_isConnected || m_appState != AppState::Offline) {
    return;
  }

  m_appState = AppState::Connecting;

  QJsonObject request{
      {QString::fromUtf8(kEmptyKey), QStringLiteral("login")},
      {QStringLiteral("username"), username},
      {QStringLiteral("password"), password},
  };

  if (!partner.isEmpty()) {
    request.insert(QStringLiteral("partner"), partner.toLower());
    request.insert(QStringLiteral("os"), QStringLiteral("Linux"));
    request.insert(QStringLiteral("appVersion"), QStringLiteral("3.13.11"));
  }

  m_connection->sendRequest(
      request,
      [this](const QJsonObject &msg) {
        const QJsonObject inner = msg.value(QString::fromUtf8(kEmptyKey)).toObject();
        const bool success = inner.contains(QStringLiteral("userRole")) || inner.contains(QStringLiteral("partner"))
                             || (!inner.contains(QStringLiteral("error")) && !inner.value(QStringLiteral("error")).toBool());
        if (success) {
          m_appState = AppState::Online;
        } else {
          m_appState = AppState::Offline;
          qCWarning(lcApi) << "Login failed:" << inner;
        }
        emit authResult(success, inner);
      },
      [this](const QString &error) {
        m_appState = AppState::Offline;
        emit authResult(false, QJsonObject{{QStringLiteral("error"), error}});
      });
}

void WsApiClient::sendBye()
{
  if (!ensureOnline()) {
    return;
  }
  QJsonObject payload;
  payload.insert(QString::fromUtf8(kEmptyKey), QStringLiteral("bye"));
  m_connection->sendMessage(payload);
}

void WsApiClient::bind()
{
  if (!ensureOnline()) {
    return;
  }

  m_connection->sendRequest(
      QJsonObject{{QString::fromUtf8(kEmptyKey), QStringLiteral("Bind")}},
      [this](const QJsonObject &) { emit bindResult(true); },
      [this](const QString &) { emit bindResult(false); });
}

void WsApiClient::bindIm(const QString &lastKnownId, int maxHistSize, bool loadContacts)
{
  if (!ensureOnline()) {
    return;
  }

  QJsonObject request{
      {QString::fromUtf8(kEmptyKey), QStringLiteral("BindIM")},
      {QStringLiteral("maxHistSize"), maxHistSize},
      {QStringLiteral("loadContacts"), loadContacts},
  };
  if (!lastKnownId.isEmpty()) {
    request.insert(QStringLiteral("lastKnownId"), lastKnownId);
  }
  m_connection->sendRequest(request, [](const QJsonObject &) {}, [](const QString &) {});
}

void WsApiClient::subscribeToAddressBook()
{
  if (!ensureOnline()) {
    return;
  }
  m_connection->sendRequestWithResponse(QJsonObject{{QString::fromUtf8(kEmptyKey), QStringLiteral("subscribetoaddressbook")}});
}

int WsApiClient::createContact(const QJsonObject &contact)
{
  if (!ensureOnline()) {
    return -1;
  }
  return m_connection->sendRequestWithResponse(QJsonObject{
      {QString::fromUtf8(kEmptyKey), QStringLiteral("createcontact")},
      {QStringLiteral("contact"), contact},
  });
}

int WsApiClient::deleteContact(const QString &contactId)
{
  if (!ensureOnline() || contactId.isEmpty()) {
    return -1;
  }
  return m_connection->sendRequestWithResponse(QJsonObject{
      {QString::fromUtf8(kEmptyKey), QStringLiteral("deletecontact")},
      {QStringLiteral("contact"), QJsonObject{{QStringLiteral("contactId"), contactId}}},
  });
}

int WsApiClient::uploadContacts(const QJsonArray &contacts)
{
  if (!ensureOnline() || contacts.isEmpty()) {
    return -1;
  }
  return m_connection->sendRequestWithResponse(QJsonObject{
      {QString::fromUtf8(kEmptyKey), QStringLiteral("uploadcontacts")},
      {QStringLiteral("contacts"), contacts},
  });
}

void WsApiClient::provisionCall(const QString &leg)
{
  if (!ensureOnline()) {
    return;
  }
  m_connection->sendRequest(QJsonObject{
      {QString::fromUtf8(kEmptyKey), QStringLiteral("ProvisionCall")},
      {QStringLiteral("leg"), leg},
  });
}

void WsApiClient::startCall(const QString &leg, const QString &peer, const QString &localSdp,
                            const QString &callerId, const QJsonObject &conference)
{
  if (!ensureOnline()) {
    return;
  }

  QJsonObject addr{
      {QString::fromUtf8(kEmptyKey), AddressBookManager::formatCallAddress(peer)},
      {QStringLiteral("wsapi"), QStringLiteral("communicator")}};
  if (!callerId.isEmpty()) {
    addr.insert(QStringLiteral("P-Preferred-Identity"), callerId);
  }

  QJsonObject request{
      {QString::fromUtf8(kEmptyKey), QStringLiteral("StartCall")},
      {QStringLiteral("leg"), leg},
      {QStringLiteral("addr"), addr},
      {QStringLiteral("sdp"), localSdp},
  };
  if (!conference.isEmpty()) {
    request.insert(QStringLiteral("conference"), conference);
  }

  m_connection->sendRequest(request);
}

void WsApiClient::acceptCall(const QString &leg, const QString &localSdp)
{
  if (!ensureOnline()) {
    return;
  }

  m_connection->sendRequest(QJsonObject{
      {QString::fromUtf8(kEmptyKey), QStringLiteral("AcceptCall")},
      {QStringLiteral("leg"), leg},
      {QStringLiteral("sdp"), localSdp},
      {QStringLiteral("headers"), QJsonObject{{QStringLiteral("wsapi"), QStringLiteral("communicator")}}},
  });
}

void WsApiClient::rejectCall(const QString &leg, int code, const QString &reason)
{
  if (!ensureOnline()) {
    return;
  }
  m_connection->sendRequest(QJsonObject{
      {QString::fromUtf8(kEmptyKey), QStringLiteral("RejectCall")},
      {QStringLiteral("leg"), leg},
      {QStringLiteral("code"), code},
      {QStringLiteral("reason"), reason},
  });
}

void WsApiClient::cancelCall(const QString &leg, int code, const QString &reason)
{
  if (!ensureOnline()) {
    return;
  }
  m_connection->sendRequest(QJsonObject{
      {QString::fromUtf8(kEmptyKey), QStringLiteral("CancelCall")},
      {QStringLiteral("leg"), leg},
      {QStringLiteral("code"), code},
      {QStringLiteral("reason"), reason},
  });
}

void WsApiClient::disconnectCall(const QString &leg, int code)
{
  if (!ensureOnline()) {
    return;
  }
  m_connection->sendRequest(QJsonObject{
      {QString::fromUtf8(kEmptyKey), QStringLiteral("DisconnectCall")},
      {QStringLiteral("leg"), leg},
      {QStringLiteral("code"), code},
  });
}

void WsApiClient::ackAccept(const QString &leg)
{
  if (!ensureOnline()) {
    return;
  }
  m_connection->sendRequest(QJsonObject{
      {QString::fromUtf8(kEmptyKey), QStringLiteral("AckAccept")},
      {QStringLiteral("leg"), leg},
  });
}

void WsApiClient::updateCall(const QString &leg, const QString &localSdp)
{
  if (!ensureOnline()) {
    return;
  }
  m_connection->sendRequest(QJsonObject{
      {QString::fromUtf8(kEmptyKey), QStringLiteral("UpdateCall")},
      {QStringLiteral("leg"), leg},
      {QStringLiteral("sdp"), localSdp},
  });
}

void WsApiClient::blindTransfer(const QString &leg, const QString &peer)
{
  if (!ensureOnline()) {
    return;
  }
  m_connection->sendRequest(QJsonObject{
      {QString::fromUtf8(kEmptyKey), QStringLiteral("Transfer")},
      {QStringLiteral("leg"), leg},
      {QStringLiteral("address"), AddressBookManager::formatCallAddress(peer)},
  });
}

void WsApiClient::setOwnPresence(const QString &status, bool manual)
{
  if (!ensureOnline()) {
    return;
  }
  m_connection->sendRequest(QJsonObject{
      {QString::fromUtf8(kEmptyKey), QStringLiteral("SetPresence")},
      {QStringLiteral("presence"),
       QJsonObject{{QStringLiteral("status"), status.toLower()}, {QStringLiteral("manual"), manual}}},
  });
}

void WsApiClient::sendSms(const QString &from, const QString &to, const QString &text)
{
  if (!ensureOnline()) {
    return;
  }
  m_connection->sendRequestWithResponse(QJsonObject{
      {QString::fromUtf8(kEmptyKey), QStringLiteral("sendsms")},
      {QStringLiteral("from"), from},
      {QStringLiteral("to"), to},
      {QStringLiteral("text"), text},
  });
}

void WsApiClient::getDomainContacts()
{
  if (!ensureOnline()) {
    return;
  }

  m_connection->sendRequest(
      QJsonObject{{QString::fromUtf8(kEmptyKey), QStringLiteral("listaccounts")}},
      [](const QJsonObject &) {},
      [](const QString &) {});
}

int WsApiClient::getHistory(const QJsonObject &params)
{
  if (!ensureOnline()) {
    return -1;
  }

  QJsonObject request = params;
  if (!request.contains(QString::fromUtf8(kEmptyKey))) {
    request.insert(QString::fromUtf8(kEmptyKey), QStringLiteral("gethistory"));
  }
  return m_connection->sendRequestWithResponse(request);
}

int WsApiClient::getSmsTelnums()
{
  if (!ensureOnline()) {
    return -1;
  }
  return m_connection->sendRequestWithResponse(
      QJsonObject{{QString::fromUtf8(kEmptyKey), QStringLiteral("smstelnums")}});
}

int WsApiClient::sendIm(const QString &peer, const QString &body, const QString &type, bool persist, bool copyToSelf)
{
  if (!ensureOnline() || peer.isEmpty()) {
    return -1;
  }

  QJsonObject request{
      {QString::fromUtf8(kEmptyKey), QStringLiteral("SendIM")},
      {QStringLiteral("to"), peer},
      {QStringLiteral("body"), body},
  };
  if (!type.isEmpty()) {
    request.insert(QStringLiteral("type"), type);
  }
  if (!persist) {
    request.insert(QStringLiteral("persist"), false);
  }
  if (!copyToSelf) {
    request.insert(QStringLiteral("copyToSelf"), false);
  }
  return m_connection->sendRequestWithResponse(request);
}

int WsApiClient::sendIm(const QString &peer, const QStringList &body, const QString &type)
{
  if (!ensureOnline() || peer.isEmpty()) {
    return -1;
  }

  QJsonArray arr;
  for (const QString &item : body) {
    arr.append(item);
  }

  QJsonObject request{
      {QString::fromUtf8(kEmptyKey), QStringLiteral("SendIM")},
      {QStringLiteral("to"), peer},
      {QStringLiteral("body"), arr},
      {QStringLiteral("type"), type},
      {QStringLiteral("persist"), true},
      {QStringLiteral("copyToSelf"), true},
  };
  return m_connection->sendRequestWithResponse(request);
}

int WsApiClient::loadImHistory(const QString &lastLoadedId, int loadSize)
{
  if (!ensureOnline()) {
    return -1;
  }

  QJsonObject request{
      {QString::fromUtf8(kEmptyKey), QStringLiteral("loadimhistory")},
      {QStringLiteral("maxSize"), loadSize},
  };
  if (!lastLoadedId.isEmpty()) {
    request.insert(QStringLiteral("lastLoadedId"), lastLoadedId);
  }
  return m_connection->sendRequestWithResponse(request);
}

} // namespace itl
