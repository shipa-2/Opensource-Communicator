#include "CommunicatorClient.h"

#include "chat/ChatManager.h"

#include <QJsonArray>
#include <QLoggingCategory>
#include <QUrl>

Q_LOGGING_CATEGORY(lcClient, "itl.client")

namespace itl {

CommunicatorClient::CommunicatorClient(QObject *parent)
    : QObject(parent)
    , m_settings(QStringLiteral("opensource-communicator"), QStringLiteral("opensource-communicator"))
    , m_chat(new ChatManager(&m_api, this))
{
  connect(&m_api, &WsApiClient::connectionEstablished, this, &CommunicatorClient::onConnectionEstablished);
  connect(&m_api, &WsApiClient::authResult, this, &CommunicatorClient::onAuthResult);
  connect(&m_api, &WsApiClient::serverPayload, this, &CommunicatorClient::onServerPayload);
  connect(&m_api, &WsApiClient::connectionFailed, this, &CommunicatorClient::onConnectionFailed);
  connect(&m_api, &WsApiClient::connectionClosed, this, &CommunicatorClient::onConnectionClosed);
  connect(&m_api, &WsApiClient::responseReceived, this, &CommunicatorClient::onApiResponse);

  connect(m_chat, &ChatManager::messageReceived, this, [this](const InstantMessage &im) {
    if (!im.body.isEmpty()) {
      emit chatMessage(im.peer, im.body, im.incoming);
    }
  });
}

CommunicatorClient::~CommunicatorClient() = default;

void CommunicatorClient::loadSettings()
{
  m_credentials.login = m_settings.value(QStringLiteral("login")).toString();
  m_credentials.password = m_settings.value(QStringLiteral("password")).toString();
  m_credentials.domain = m_settings.value(QStringLiteral("domain")).toString();
  m_credentials.authDomain = m_settings.value(QStringLiteral("authDomain")).toString();
  m_credentials.partner = m_settings.value(QStringLiteral("partner"), QStringLiteral("megafon")).toString();
  m_appSettings.load(m_settings);
  m_appSettings.loadUserData(m_settings);
}

void CommunicatorClient::saveSettings()
{
  m_settings.setValue(QStringLiteral("login"), m_credentials.login);
  m_settings.setValue(QStringLiteral("password"), m_credentials.password);
  m_settings.setValue(QStringLiteral("domain"), m_credentials.domain);
  m_settings.setValue(QStringLiteral("authDomain"), m_credentials.authDomain);
  m_settings.setValue(QStringLiteral("partner"), m_credentials.partner);
  m_appSettings.save(m_settings);
  m_appSettings.saveUserData();
}

void CommunicatorClient::setCredentials(const LoginCredentials &credentials)
{
  m_credentials = credentials;
}

QString CommunicatorClient::buildWebSocketUrl() const
{
  const QString domain = m_credentials.domain.isEmpty() ? m_credentials.login.section(QLatin1Char('@'), 1) : m_credentials.domain;
  const QString authDomain = m_credentials.authDomain.isEmpty() ? domain : m_credentials.authDomain;

  if (!m_credentials.authDomain.isEmpty() || m_credentials.partner == QStringLiteral("megafon")) {
    return QStringLiteral("wss://%1/ws/?_domain=%2").arg(authDomain, domain);
  }
  return QStringLiteral("wss://%1/ws").arg(domain);
}

void CommunicatorClient::login()
{
  if (m_credentials.login.size() <= 2) {
    emit statusMessage(tr("Укажите логин и домен"));
    return;
  }

  if (m_credentials.domain.isEmpty()) {
    m_credentials.domain = m_credentials.login.section(QLatin1Char('@'), 1);
  }

  emit stateChanged(AppState::Connecting);
  emit statusMessage(tr("Подключение к %1...").arg(buildWebSocketUrl()));

  m_api.initialize(QUrl(buildWebSocketUrl()));
}

void CommunicatorClient::logout()
{
  if (m_api.appState() == AppState::Online) {
    m_api.sendBye();
  }
  m_api.disconnect();
  emit stateChanged(AppState::Offline);
  emit statusMessage(tr("Отключено"));
}

void CommunicatorClient::onConnectionEstablished()
{
  emit statusMessage(tr("Соединение установлено, авторизация..."));

  QString partner = m_credentials.partner;
  if (partner == QStringLiteral("Exolve")) {
    partner = QStringLiteral("mtt");
  }

  m_api.login(m_credentials.login, m_credentials.password, partner);
}

void CommunicatorClient::onAuthResult(bool success, const QJsonObject &payload)
{
  if (!success) {
    const QString error = payload.value(QStringLiteral("error")).toString();
    emit statusMessage(tr("Ошибка входа: %1").arg(error.isEmpty() ? tr("неизвестная ошибка") : error));
    emit stateChanged(AppState::Offline);
    m_api.disconnect();
    return;
  }

  saveSettings();
  m_chat->setDomain(m_credentials.domain);
  emit stateChanged(AppState::Online);
  emit statusMessage(tr("В сети"));

  m_api.bind();
  m_api.bindIm();
  m_api.subscribeToAddressBook();
  m_api.getDomainContacts();
  m_api.setOwnPresence(QStringLiteral("online"));
}

void CommunicatorClient::onServerPayload(const QJsonObject &payload)
{
  const QString what = payload.value(QStringLiteral("What")).toString();

  if (what == QStringLiteral("[IM]") || what == QStringLiteral("[IM_HIST]") || what == QStringLiteral("[IM_CONTACTS]")) {
    m_chat->handlePayload(payload);
    return;
  }

  if (what == QStringLiteral("[PRESENCE]")) {
    handlePresencePayload(payload);
    return;
  }

  if (payload.contains(QStringLiteral("leg")) && !what.isEmpty()) {
    const QString leg = payload.value(QStringLiteral("leg")).toString();
    emit callEvent(leg, what, payload);

    if (what == QStringLiteral("incomingCall")) {
      const QJsonObject dest = payload.value(QStringLiteral("dest")).toObject();
      QString peer = dest.value(QStringLiteral("From")).toObject().value(QString::fromUtf8(kEmptyKey)).toString();
      if (peer.isEmpty()) {
        peer = dest.value(QStringLiteral("From")).toObject().value(QString::fromUtf8(kEmptyKey)).toObject().value(QString::fromUtf8(kEmptyKey)).toString();
      }
      emit statusMessage(tr("Входящий вызов %1").arg(peer));
    }
  }
}

void CommunicatorClient::handlePresencePayload(const QJsonObject &payload)
{
  const QJsonObject batch = payload.value(QStringLiteral("batch")).toObject();
  for (auto it = batch.begin(); it != batch.end(); ++it) {
    if (it.value().isNull() || !it.value().isObject()) {
      continue;
    }

    QString peer = it.key();
    if (!peer.contains(QLatin1Char('@'))) {
      peer += QLatin1Char('@') + m_credentials.domain;
    }

    const QJsonObject entry = it.value().toObject();
    QString presence = entry.value(QStringLiteral("voice")).toString();
    if (presence.isEmpty()) {
      presence = entry.value(QStringLiteral("im")).toObject().value(QStringLiteral("status")).toString();
    }

    emit contactUpdated(peer, {}, presence);
  }
}

void CommunicatorClient::onApiResponse(int requestId, const QJsonObject &response)
{
  m_chat->handleResponse(requestId, response);
}

void CommunicatorClient::onConnectionFailed(const QString &error)
{
  emit statusMessage(tr("Ошибка соединения: %1").arg(error));
  emit stateChanged(AppState::Offline);
}

void CommunicatorClient::onConnectionClosed(const QString &reason)
{
  emit statusMessage(tr("Соединение закрыто: %1").arg(reason));
  emit stateChanged(AppState::Offline);
}

} // namespace itl
