#include "AddressBookManager.h"

#include "ProtocolTypes.h"
#include "WsApiClient.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(lcAddressBook, "itl.addressbook")

namespace itl {

namespace {

int countDigits(const QString &value)
{
  int digits = 0;
  for (const QChar ch : value) {
    if (ch.isDigit()) {
      ++digits;
    }
  }
  return digits;
}

} // namespace

AddressBookManager::AddressBookManager(WsApiClient *api, QObject *parent)
    : QObject(parent)
    , m_api(api)
{
}

void AddressBookManager::setDomain(const QString &domain)
{
  m_domain = domain;
}

QList<CustomContact> AddressBookManager::contacts() const
{
  return m_byServerId.values();
}

QString AddressBookManager::serverIdForPeer(const QString &peer) const
{
  return m_peerToServerId.value(peer);
}

QString AddressBookManager::normalizeSubId(const QString &raw)
{
  QString subId = raw;
  if (subId.startsWith(QStringLiteral("ab:"))) {
    subId = subId.mid(3);
  }
  if (subId.contains(QLatin1Char('@')) && !subId.startsWith(QLatin1Char('~'))) {
    subId = QLatin1Char('~') + subId;
  }
  return subId;
}

QString AddressBookManager::normalizePhone(QString phone)
{
  phone = phone.trimmed();
  if (phone.isEmpty()) {
    return phone;
  }

  QString stripped;
  stripped.reserve(phone.size());
  for (const QChar ch : phone) {
    if (ch.isDigit() || ch == QLatin1Char('+') || ch == QLatin1Char('*') || ch == QLatin1Char('#')) {
      stripped.append(ch);
    }
  }
  if (stripped.isEmpty()) {
    return phone;
  }

  const int digits = countDigits(stripped);
  if (digits >= 8 && digits <= 20) {
    if (!stripped.startsWith(QLatin1Char('+')) && stripped.startsWith(QLatin1Char('8')) && digits == 11) {
      return QStringLiteral("+7") + stripped.mid(1);
    }
    if (!stripped.startsWith(QLatin1Char('+')) && stripped.startsWith(QLatin1Char('7')) && digits == 11) {
      return QLatin1Char('+') + stripped;
    }
  }
  return stripped;
}

QString AddressBookManager::peerFromAbObject(const QJsonObject &obj, const QString &domain)
{
  QString phone;
  QString ext;
  const QJsonArray phones = obj.value(QStringLiteral("phones")).toArray();
  for (const QJsonValue &value : phones) {
    const QJsonObject ph = value.toObject();
    const QString num = ph.value(QString::fromUtf8(kEmptyKey)).toString();
    const QString type = ph.value(QStringLiteral("type")).toString();
    if (type == QStringLiteral("ext")) {
      ext = num;
    } else if (phone.isEmpty() && !num.isEmpty()) {
      phone = normalizePhone(num);
    }
  }

  if (!phone.isEmpty()) {
    return phone;
  }
  if (!ext.isEmpty()) {
    return ext.contains(QLatin1Char('@')) ? ext : ext + QLatin1Char('@') + domain;
  }

  const QJsonArray emails = obj.value(QStringLiteral("emails")).toArray();
  if (!emails.isEmpty()) {
    const QString email = emails.first().toString();
    if (!email.isEmpty()) {
      return email.contains(QLatin1Char('@')) ? email : email + QLatin1Char('@') + domain;
    }
  }
  return {};
}

QJsonObject AddressBookManager::contactToServerJson(const CustomContact &contact)
{
  QJsonArray phones;
  if (!contact.phone.isEmpty()) {
    phones.append(QJsonObject{
        {QString::fromUtf8(kEmptyKey), normalizePhone(contact.phone)},
        {QStringLiteral("type"), QStringLiteral("main")},
    });
  }
  if (!contact.ext.isEmpty()) {
    phones.append(QJsonObject{
        {QString::fromUtf8(kEmptyKey), contact.ext},
        {QStringLiteral("type"), QStringLiteral("ext")},
    });
  }

  QJsonObject payload{
      {QStringLiteral("name"), QJsonObject{{QStringLiteral("full"), contact.name}}},
      {QStringLiteral("emails"), QJsonValue(QJsonArray())},
  };
  if (!phones.isEmpty()) {
    payload.insert(QStringLiteral("phones"), phones);
  }
  if (!contact.serverId.isEmpty()) {
    payload.insert(QStringLiteral("contactId"), contact.serverId);
  }
  return payload;
}

CustomContact AddressBookManager::parseAbContact(const QJsonObject &obj, const QString &subId,
                                                 const QString &domain) const
{
  CustomContact contact;
  const QJsonObject nameObj = obj.value(QStringLiteral("name")).toObject();
  contact.name = nameObj.value(QStringLiteral("full")).toString();
  contact.serverId = obj.value(QStringLiteral("id")).toString();
  contact.serverId = buildServerId(contact.serverId, subId);

  const QJsonArray phones = obj.value(QStringLiteral("phones")).toArray();
  for (const QJsonValue &value : phones) {
    const QJsonObject ph = value.toObject();
    const QString num = ph.value(QString::fromUtf8(kEmptyKey)).toString();
    const QString type = ph.value(QStringLiteral("type")).toString();
    if (type == QStringLiteral("ext")) {
      contact.ext = num;
    } else if (contact.phone.isEmpty()) {
      contact.phone = normalizePhone(num);
    }
  }

  contact.peer = peerFromAbObject(obj, domain);
  if (contact.peer.isEmpty() && !contact.serverId.isEmpty()) {
    contact.peer = contact.serverId;
  }
  return contact;
}

QString AddressBookManager::buildServerId(const QString &rawId, const QString &subId)
{
  if (rawId.isEmpty()) {
    return {};
  }
  if (rawId.contains(QLatin1Char('*'))) {
    return rawId;
  }
  if (subId.isEmpty()) {
    return rawId;
  }
  return rawId + QLatin1Char('*') + subId;
}

bool AddressBookManager::peersMatch(const QString &a, const QString &b, const QString &domain)
{
  if (a.isEmpty() || b.isEmpty()) {
    return false;
  }
  if (a.compare(b, Qt::CaseInsensitive) == 0) {
    return true;
  }

  const QString phoneA = normalizePhone(a);
  const QString phoneB = normalizePhone(b);
  if (!phoneA.isEmpty() && phoneA == phoneB) {
    return true;
  }

  const auto withDomain = [&domain](QString peer) {
    peer = peer.trimmed().toLower();
    if (!peer.contains(QLatin1Char('@')) && !domain.isEmpty()) {
      peer += QLatin1Char('@') + domain.toLower();
    }
    return peer;
  };

  return withDomain(a) == withDomain(b);
}

QString AddressBookManager::findServerIdForPeer(const QString &peer) const
{
  if (peer.isEmpty()) {
    return {};
  }

  const QString direct = m_peerToServerId.value(peer);
  if (!direct.isEmpty()) {
    return direct;
  }

  for (auto it = m_peerToServerId.cbegin(); it != m_peerToServerId.cend(); ++it) {
    if (peersMatch(it.key(), peer, m_domain)) {
      return it.value();
    }
  }

  for (const CustomContact &contact : m_byServerId) {
    if (peersMatch(contact.peer, peer, m_domain)) {
      return contact.serverId;
    }
    if (!contact.phone.isEmpty() && peersMatch(contact.phone, peer, m_domain)) {
      return contact.serverId;
    }
    if (!contact.ext.isEmpty()) {
      const QString extPeer = contact.ext.contains(QLatin1Char('@'))
          ? contact.ext
          : contact.ext + QLatin1Char('@') + m_domain;
      if (peersMatch(extPeer, peer, m_domain)) {
        return contact.serverId;
      }
    }
  }

  return {};
}

bool AddressBookManager::removeContactByServerId(const QString &serverId, bool emitChanged)
{
  if (serverId.isEmpty()) {
    return false;
  }

  const CustomContact removed = m_byServerId.take(serverId);
  if (removed.serverId.isEmpty() && removed.peer.isEmpty()) {
    return false;
  }

  QStringList peersToRemove;
  for (auto it = m_peerToServerId.cbegin(); it != m_peerToServerId.cend(); ++it) {
    if (it.value() == serverId) {
      peersToRemove.append(it.key());
    }
  }
  for (const QString &mappedPeer : peersToRemove) {
    m_peerToServerId.remove(mappedPeer);
  }
  if (!removed.peer.isEmpty()) {
    m_peerToServerId.remove(removed.peer);
  }

  qCInfo(lcAddressBook) << "Removed contact locally" << removed.peer << serverId;
  if (emitChanged) {
    emit contactsChanged();
  }
  return true;
}

int AddressBookManager::createContact(const CustomContact &contact)
{
  if (!m_api || contact.name.isEmpty()) {
    return -1;
  }

  const int requestId = m_api->createContact(contactToServerJson(contact));
  if (requestId >= 0) {
    m_pendingCreatePeers.insert(requestId, contact.peer);
  }
  return requestId;
}

int AddressBookManager::deleteContactByPeer(const QString &peer)
{
  const QString serverId = findServerIdForPeer(peer);
  if (!m_api || serverId.isEmpty()) {
    qCWarning(lcAddressBook) << "deleteContactByPeer: no server id for" << peer;
    return -1;
  }

  const int requestId = m_api->deleteContact(serverId);
  if (requestId >= 0) {
    m_pendingDeleteServerIds.insert(requestId, serverId);
  }
  return requestId;
}

int AddressBookManager::uploadLocalContacts(const QList<CustomContact> &local)
{
  if (!m_api || local.isEmpty()) {
    return -1;
  }

  QJsonArray contacts;
  for (const CustomContact &contact : local) {
    contacts.append(contactToServerJson(contact));
  }

  const int requestId = m_api->uploadContacts(contacts);
  if (requestId >= 0) {
    m_uploadRequestId = requestId;
  }
  return requestId;
}

void AddressBookManager::handlePayload(const QJsonObject &payload)
{
  if (payload.value(QStringLiteral("What")).toString() != QStringLiteral("[CONTACTS]")) {
    return;
  }

  const QString subId = normalizeSubId(payload.value(QStringLiteral("subId")).toString());
  bool changed = false;

  const QJsonArray objects = payload.value(QStringLiteral("objects")).toArray();
  for (const QJsonValue &value : objects) {
    if (!value.isObject()) {
      continue;
    }
    const QJsonObject obj = value.toObject();
    if (obj.value(QStringLiteral("deleted")).toBool()) {
      QString rawId = obj.value(QStringLiteral("id")).toString();
      if (rawId.isEmpty()) {
        rawId = obj.value(QStringLiteral("contactId")).toString();
      }
      const QString serverId = buildServerId(rawId, subId);
      if (removeContactByServerId(serverId, false)) {
        changed = true;
      }
      continue;
    }

    const CustomContact contact = parseAbContact(obj, subId, m_domain);
    if (contact.serverId.isEmpty() || contact.peer.isEmpty()) {
      continue;
    }

    const QString oldPeer = m_byServerId.value(contact.serverId).peer;
    if (!oldPeer.isEmpty() && oldPeer != contact.peer) {
      m_peerToServerId.remove(oldPeer);
    }

    m_byServerId.insert(contact.serverId, contact);
    m_peerToServerId.insert(contact.peer, contact.serverId);
    changed = true;
  }

  if (changed) {
    qCInfo(lcAddressBook) << "Address book updated, total" << m_byServerId.size();
    emit contactsChanged();
  }
}

void AddressBookManager::handleResponse(int requestId, const QJsonObject &response)
{
  const QJsonObject inner = response.value(QString::fromUtf8(kEmptyKey)).toObject();
  const QJsonValue responseValue = inner.value(QStringLiteral("response"));

  if (m_pendingDeleteServerIds.contains(requestId)) {
    const QString serverId = m_pendingDeleteServerIds.take(requestId);
    if (inner.value(QStringLiteral("error")).toBool()
        || response.contains(QStringLiteral("error"))) {
      const QString reason = inner.value(QStringLiteral("error")).toVariant().toString();
      qCWarning(lcAddressBook) << "deletecontact failed for" << serverId << response;
      emit deleteFailed(serverId, reason);
      return;
    }
    removeContactByServerId(serverId);
    return;
  }

  if (requestId == m_uploadRequestId) {
    m_uploadRequestId = -1;
    const bool ok = responseValue.isString() && responseValue.toString() == QStringLiteral("ok");
    if (ok) {
      qCInfo(lcAddressBook) << "Local contacts uploaded to server";
    } else {
      qCWarning(lcAddressBook) << "uploadcontacts failed:" << response;
    }
    emit uploadCompleted(ok);
    return;
  }

  if (!m_pendingCreatePeers.contains(requestId)) {
    return;
  }

  const QString peer = m_pendingCreatePeers.take(requestId);
  if (responseValue.isObject()) {
    const QJsonObject obj = responseValue.toObject();
    if (obj.contains(QStringLiteral("contactId"))) {
      qCInfo(lcAddressBook) << "createcontact ok for" << peer << "id" << obj.value(QStringLiteral("contactId")).toString();
      return;
    }
    if (obj.contains(QStringLiteral("error"))) {
      qCWarning(lcAddressBook) << "createcontact error for" << peer << obj.value(QStringLiteral("error")).toString();
      return;
    }
  }
  qCWarning(lcAddressBook) << "createcontact unexpected response for" << peer;
}

void AddressBookManager::clear()
{
  m_byServerId.clear();
  m_peerToServerId.clear();
  m_pendingCreatePeers.clear();
  m_pendingDeleteServerIds.clear();
  m_uploadRequestId = -1;
}

} // namespace itl
