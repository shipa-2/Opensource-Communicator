#include "CommunicatorClient.h"

#include "chat/ChatManager.h"
#include "demo/DemoData.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QLoggingCategory>
#include <QUrl>

#include <algorithm>

Q_LOGGING_CATEGORY(lcClient, "itl.client")

namespace itl {

CommunicatorClient::CommunicatorClient(QObject *parent)
    : QObject(parent)
    , m_settings(QStringLiteral("opensource-communicator"), QStringLiteral("opensource-communicator"))
    , m_chat(new ChatManager(&m_api, this))
    , m_addressBook(new AddressBookManager(&m_api, this))
{
  qRegisterMetaType<itl::LoginCredentials>("itl::LoginCredentials");

  connect(&m_api, &WsApiClient::connectionEstablished, this, &CommunicatorClient::onConnectionEstablished);
  connect(&m_api, &WsApiClient::authResult, this, &CommunicatorClient::onAuthResult);
  connect(&m_api, &WsApiClient::serverPayload, this, &CommunicatorClient::onServerPayload);
  connect(&m_api, &WsApiClient::connectionFailed, this, &CommunicatorClient::onConnectionFailed);
  connect(&m_api, &WsApiClient::connectionClosed, this, &CommunicatorClient::onConnectionClosed);
  connect(&m_api, &WsApiClient::responseReceived, this, &CommunicatorClient::onApiResponse);

  connect(m_addressBook, &AddressBookManager::contactsChanged, this, &CommunicatorClient::addressBookChanged);
  connect(m_addressBook, &AddressBookManager::uploadCompleted, this, [this](bool success) {
    if (success && !m_appSettings.customContacts().isEmpty()) {
      m_appSettings.setCustomContacts({});
      saveSettings();
    }
  });

  connect(m_chat, &ChatManager::messageReceived, this, [this](const InstantMessage &im) {
    if (!im.body.isEmpty()) {
      emit chatMessage(im.peer, im.body, im.incoming, im.timestamp);
    }
  });
}

CommunicatorClient::~CommunicatorClient() = default;

QString CommunicatorClient::accountKey(const LoginCredentials &credentials)
{
  const QString login = credentials.login.trimmed().toLower();
  const QString domain = credentials.domain.trimmed().toLower();
  if (domain.isEmpty()) {
    return login;
  }
  if (login.contains(QLatin1Char('@'))) {
    return login;
  }
  return login + QLatin1Char('@') + domain;
}

void CommunicatorClient::loadSavedAccounts()
{
  m_savedAccounts.clear();
  const QJsonArray array = m_settings.value(QStringLiteral("savedAccounts")).toJsonArray();
  for (const QJsonValue &value : array) {
    const QJsonObject obj = value.toObject();
    LoginCredentials account;
    account.login = obj.value(QStringLiteral("login")).toString();
    account.password = obj.value(QStringLiteral("password")).toString();
    account.domain = obj.value(QStringLiteral("domain")).toString();
    account.authDomain = obj.value(QStringLiteral("authDomain")).toString();
    account.partner = obj.value(QStringLiteral("partner")).toString(QStringLiteral("megafon"));
    if (account.login.isEmpty() || DemoData::isDemoCredentials(account.login, account.password)) {
      continue;
    }
    m_savedAccounts.append(account);
  }

  // Migrate a single previously saved login into the account list.
  if (m_savedAccounts.isEmpty() && !m_credentials.login.isEmpty()
      && !DemoData::isDemoCredentials(m_credentials.login, m_credentials.password)) {
    m_savedAccounts.append(m_credentials);
  }
}

void CommunicatorClient::saveSavedAccounts()
{
  QJsonArray array;
  for (const LoginCredentials &account : m_savedAccounts) {
    if (DemoData::isDemoCredentials(account.login, account.password)) {
      continue;
    }
    array.append(QJsonObject{
        {QStringLiteral("login"), account.login},
        {QStringLiteral("password"), account.password},
        {QStringLiteral("domain"), account.domain},
        {QStringLiteral("authDomain"), account.authDomain},
        {QStringLiteral("partner"), account.partner},
    });
  }
  m_settings.setValue(QStringLiteral("savedAccounts"), array);
}

void CommunicatorClient::loadSettings()
{
  m_rememberMe = m_settings.value(QStringLiteral("rememberMe"), true).toBool();
  m_credentials.login = m_settings.value(QStringLiteral("login")).toString();
  m_credentials.password = m_settings.value(QStringLiteral("password")).toString();
  m_credentials.domain = m_settings.value(QStringLiteral("domain")).toString();
  m_credentials.authDomain = m_settings.value(QStringLiteral("authDomain")).toString();
  m_credentials.partner = m_settings.value(QStringLiteral("partner"), QStringLiteral("megafon")).toString();
  if (!m_rememberMe) {
    m_credentials.password.clear();
  }
  loadSavedAccounts();
  m_appSettings.load(m_settings);
  m_appSettings.loadUserData(m_settings);
}

void CommunicatorClient::saveSettings()
{
  m_settings.setValue(QStringLiteral("rememberMe"), m_rememberMe);
  if (!m_rememberMe) {
    m_settings.setValue(QStringLiteral("login"), QString());
    m_settings.setValue(QStringLiteral("password"), QString());
    m_settings.setValue(QStringLiteral("domain"), QString());
    m_settings.setValue(QStringLiteral("authDomain"), QString());
    m_settings.setValue(QStringLiteral("partner"), QStringLiteral("megafon"));
  } else if (!DemoData::isDemoCredentials(m_credentials.login, m_credentials.password)) {
    m_settings.setValue(QStringLiteral("login"), m_credentials.login);
    m_settings.setValue(QStringLiteral("password"), m_credentials.password);
    m_settings.setValue(QStringLiteral("domain"), m_credentials.domain);
    m_settings.setValue(QStringLiteral("authDomain"), m_credentials.authDomain);
    m_settings.setValue(QStringLiteral("partner"), m_credentials.partner);
  }
  saveSavedAccounts();
  m_appSettings.save(m_settings);
  m_appSettings.saveUserData();
}

void CommunicatorClient::setCredentials(const LoginCredentials &credentials)
{
  m_credentials = credentials;
}

void CommunicatorClient::setRememberMe(bool remember)
{
  m_rememberMe = remember;
}

void CommunicatorClient::rememberAccount(const LoginCredentials &credentials)
{
  if (credentials.login.trimmed().isEmpty()
      || DemoData::isDemoCredentials(credentials.login, credentials.password)) {
    return;
  }

  const QString key = accountKey(credentials);
  for (int i = 0; i < m_savedAccounts.size(); ++i) {
    if (accountKey(m_savedAccounts.at(i)) == key) {
      m_savedAccounts[i] = credentials;
      // Keep the most recently used account first.
      m_savedAccounts.move(i, 0);
      return;
    }
  }
  m_savedAccounts.prepend(credentials);
}

void CommunicatorClient::removeSavedAccount(const QString &login)
{
  const QString needle = login.trimmed().toLower();
  if (needle.isEmpty()) {
    return;
  }
  m_savedAccounts.erase(std::remove_if(m_savedAccounts.begin(), m_savedAccounts.end(),
                                       [&](const LoginCredentials &account) {
                                         return accountKey(account) == needle
                                             || account.login.trimmed().toLower() == needle;
                                       }),
                        m_savedAccounts.end());
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
  if (m_demoMode) {
    return;
  }

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
  if (m_demoMode) {
    leaveDemoMode();
    return;
  }

  if (m_api.appState() == AppState::Online) {
    m_api.sendBye();
  }
  m_addressBook->clear();
  m_api.disconnect();
  emit stateChanged(AppState::Offline);
  emit statusMessage(tr("Отключено"));
}

void CommunicatorClient::enterDemoMode()
{
  if (m_demoMode) {
    leaveDemoMode();
  }

  if (m_credentials.domain.isEmpty()) {
    m_credentials.domain = m_credentials.login.section(QLatin1Char('@'), 1);
  }

  m_demoMode = true;
  m_chat->setDomain(m_credentials.domain);
  m_chat->setDemoMode(true);
  emit stateChanged(AppState::Online);
  emit statusMessage(tr("В сети"));
}

void CommunicatorClient::leaveDemoMode()
{
  if (!m_demoMode) {
    return;
  }

  m_demoMode = false;
  m_chat->setDemoMode(false);
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
  if (m_rememberMe) {
    rememberAccount(m_credentials);
    saveSavedAccounts();
  }
  m_chat->setDomain(m_credentials.domain);
  m_chat->setSelfLogin(m_credentials.login);
  m_addressBook->setDomain(m_credentials.domain);
  m_addressBook->clear();

  const QList<CustomContact> localContacts = m_appSettings.customContacts();
  if (!localContacts.isEmpty()) {
    m_addressBook->uploadLocalContacts(localContacts);
  }

  emit stateChanged(AppState::Online);
  emit statusMessage(tr("В сети"));

  m_api.bind();
  m_api.bindIm();
  m_chat->loadHistory();
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

  if (what == QStringLiteral("[CONTACTS]")) {
    m_addressBook->handlePayload(payload);
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
  m_addressBook->handleResponse(requestId, response);
}

void CommunicatorClient::onConnectionFailed(const QString &error)
{
  m_addressBook->clear();
  emit statusMessage(tr("Ошибка соединения: %1").arg(error));
  emit stateChanged(AppState::Offline);
}

void CommunicatorClient::onConnectionClosed(const QString &reason)
{
  m_addressBook->clear();
  emit statusMessage(tr("Соединение закрыто: %1").arg(reason));
  emit stateChanged(AppState::Offline);
}

} // namespace itl
