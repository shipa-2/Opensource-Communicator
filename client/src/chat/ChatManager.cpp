#include "ChatManager.h"

#include "protocol/WsApiClient.h"

#include <QJsonArray>
#include <QLoggingCategory>
#include <QRegularExpression>

#include <algorithm>

Q_LOGGING_CATEGORY(lcChat, "itl.chat")

namespace itl {

namespace {
bool charIsDialable(QChar ch)
{
  return ch.isDigit() || ch == QLatin1Char('+') || ch == QLatin1Char('*') || ch == QLatin1Char('#');
}
} // namespace

ChatManager::ChatManager(WsApiClient *api, QObject *parent)
    : QObject(parent)
    , m_api(api)
{
}

void ChatManager::setDomain(const QString &domain)
{
  m_domain = domain;
  loadSmsTelnums();
}

bool ChatManager::looksLikePhone(QString value)
{
  value = value.trimmed();
  if (value.isEmpty()) {
    return false;
  }

  const int atPos = value.indexOf(QLatin1Char('@'));
  if (atPos > 0) {
    value = value.left(atPos);
  }

  for (const QChar ch : value) {
    if (!charIsDialable(ch) && ch != QLatin1Char('-') && ch != QLatin1Char(' ')
        && ch != QLatin1Char('(') && ch != QLatin1Char(')')) {
      return false;
    }
  }

  QString digitsOnly = value;
  digitsOnly.remove(QLatin1Char('+'));
  digitsOnly.remove(QLatin1Char('-'));
  digitsOnly.remove(QLatin1Char(' '));
  digitsOnly.remove(QLatin1Char('('));
  digitsOnly.remove(QLatin1Char(')'));
  return !digitsOnly.isEmpty() && digitsOnly.length() <= 21;
}

bool ChatManager::isPhonePeer(QString peer)
{
  return looksLikePhone(peer);
}

QString ChatManager::phoneFromPeer(QString peer)
{
  const int atPos = peer.indexOf(QLatin1Char('@'));
  if (atPos > 0 && looksLikePhone(peer.left(atPos))) {
    return peer.left(atPos);
  }
  return peer;
}

QString ChatManager::normalizePeer(QString peer) const
{
  if (isPhonePeer(peer)) {
    return phoneFromPeer(peer);
  }
  if (!peer.contains(QLatin1Char('@')) && !m_domain.isEmpty()) {
    peer += QLatin1Char('@') + m_domain;
  }
  return peer;
}

bool ChatManager::isSmsPeer(const QString &peer) const
{
  return isPhonePeer(peer);
}

QString ChatManager::normalizedPeer(const QString &peer) const
{
  return normalizePeer(peer);
}

void ChatManager::loadSmsTelnums()
{
  if (!m_api) {
    return;
  }
  m_smsTelnumsRequestId = m_api->getSmsTelnums();
}

void ChatManager::handleSmsTelnumsResponse(const QJsonObject &response)
{
  m_smsFromNumber.clear();
  const QJsonObject inner = response.value(QString::fromUtf8(kEmptyKey)).toObject();
  const QJsonValue responseValue = inner.value(QStringLiteral("response"));
  if (!responseValue.isArray()) {
    return;
  }

  for (const QJsonValue &value : responseValue.toArray()) {
    const QString tn = value.toObject().value(QStringLiteral("tn")).toString();
    if (!tn.isEmpty()) {
      m_smsFromNumber = tn;
      break;
    }
  }
}

InstantMessage ChatManager::parseMessage(const QJsonObject &msg) const
{
  InstantMessage im;
  im.id = msg.value(QStringLiteral("id")).toString();
  im.origId = msg.value(QStringLiteral("origId")).toString(msg.value(QStringLiteral("remoteId")).toString());
  if (im.origId.isEmpty()) {
    im.origId = im.id;
  }

  im.peer = normalizePeer(msg.value(QStringLiteral("from")).toString());
  im.incoming = true;

  if (msg.contains(QStringLiteral("to"))) {
    im.peer = normalizePeer(msg.value(QStringLiteral("to")).toString());
    im.incoming = false;
  }

  const QString type = msg.value(QStringLiteral("type")).toString();
  if (type == QStringLiteral("typing")) {
    return im;
  }
  if (type == QStringLiteral("seen") || type == QStringLiteral("state")) {
    return {};
  }

  if (msg.value(QStringLiteral("body")).isObject()) {
    im.body = msg.value(QStringLiteral("body")).toObject().value(QString::fromUtf8(kEmptyKey)).toString();
  } else {
    im.body = msg.value(QStringLiteral("body")).toString();
  }

  if (msg.contains(QStringLiteral("timestamp"))) {
    qint64 raw = msg.value(QStringLiteral("timestamp")).toVariant().toLongLong();
    // Protocol may send seconds or milliseconds since epoch.
    if (raw > 0 && raw < 100000000000LL) {
      raw *= 1000;
    }
    im.timestamp = QDateTime::fromMSecsSinceEpoch(raw);
  } else {
    im.timestamp = QDateTime::currentDateTime();
  }

  return im;
}

bool ChatManager::storeMessage(const InstantMessage &im, bool replaceOptimisticOutgoing)
{
  if (im.body.isEmpty() || im.peer.isEmpty()) {
    return false;
  }

  auto &list = m_messages[im.peer];

  if (!im.id.isEmpty() && im.id != QStringLiteral("0")) {
    for (const InstantMessage &existing : list) {
      if (existing.id == im.id) {
        return false;
      }
    }
  }

  if (replaceOptimisticOutgoing && !im.incoming) {
    for (int i = list.size() - 1; i >= 0; --i) {
      InstantMessage &existing = list[i];
      if (!existing.incoming && existing.id.isEmpty() && existing.body == im.body) {
        existing = im;
        return false;
      }
    }
  }

  list.append(im);
  return true;
}

void ChatManager::handlePayload(const QJsonObject &payload)
{
  const QString what = payload.value(QStringLiteral("What")).toString();

  if (what == QStringLiteral("[IM]")) {
    const QJsonObject raw = payload.value(QStringLiteral("message")).toObject();
    if (raw.value(QStringLiteral("type")).toString() == QStringLiteral("typing")) {
      emit typingReceived(normalizePeer(raw.value(QStringLiteral("from")).toString()));
      return;
    }

    const InstantMessage im = parseMessage(raw);
    if (im.body.isEmpty() || im.id == QStringLiteral("0")) {
      return;
    }

    if (im.incoming && isColorAdvertisement(im.body)) {
      const QString color = extractColor(im.body);
      if (!color.isEmpty()) {
        m_peerColors[im.peer] = color;
        qCDebug(lcChat) << "Color advertisement from" << im.peer << ":" << color;
        emit peerColorReceived(im.peer, color);
      }
      return;
    }

    // Outgoing copyToSelf echoes the optimistic local insert — merge, don't re-emit.
    if (!storeMessage(im, /*replaceOptimisticOutgoing=*/true)) {
      return;
    }

    if (im.incoming && !im.origId.isEmpty()) {
      m_unreadByPeer[im.peer].append(im.origId);
      emit unreadChanged(im.peer);
    }
    emit messageReceived(im);
    return;
  }

  if (what == QStringLiteral("[IM_HIST]")) {
    if (payload.contains(QStringLiteral("error"))) {
      qCWarning(lcChat) << "IM history error:" << payload.value(QStringLiteral("error")).toString();
      return;
    }

    const QJsonArray messages = payload.value(QStringLiteral("messages")).toArray();
    QString chatPeer = normalizePeer(payload.value(QStringLiteral("chatId")).toString());
    for (const auto &value : messages) {
      const InstantMessage im = parseMessage(value.toObject());
      if (im.body.isEmpty()) {
        continue;
      }
      chatPeer = im.peer;
      if (isColorAdvertisement(im.body)) {
        const QString color = extractColor(im.body);
        if (!color.isEmpty()) {
          m_peerColors[im.peer] = color;
          qCDebug(lcChat) << "Color advertisement from" << im.peer << ":" << color;
          emit peerColorReceived(im.peer, color);
        }
        continue;
      }
      // History is applied to the store only; UI reloads via historyLoaded.
      storeMessage(im, /*replaceOptimisticOutgoing=*/true);
    }
    if (!chatPeer.isEmpty()) {
      emit historyLoaded(chatPeer);
    }
    return;
  }

  if (what == QStringLiteral("[IM_CONTACTS]")) {
    const QJsonObject contacts = payload.value(QStringLiteral("contacts")).toObject();
    for (auto it = contacts.begin(); it != contacts.end(); ++it) {
      const QString peer = normalizePeer(it.key());
      const QJsonArray unseen = it.value().toObject().value(QStringLiteral("unseen")).toArray();
      QStringList ids;
      for (const auto &id : unseen) {
        if (id.toString() != QStringLiteral("0")) {
          ids.append(id.toString());
        }
      }
      if (!ids.isEmpty()) {
        m_unreadByPeer.insert(peer, ids);
        emit unreadChanged(peer);
      }
    }
  }
}

void ChatManager::handleResponse(int requestId, const QJsonObject &response)
{
  if (requestId == m_smsTelnumsRequestId) {
    m_smsTelnumsRequestId = -1;
    handleSmsTelnumsResponse(response);
    return;
  }

  if (requestId != m_historyRequestId) {
    return;
  }
  m_historyRequestId = -1;

  const QJsonObject inner = response.value(QString::fromUtf8(kEmptyKey)).toObject();
  QJsonObject hist = inner.value(QStringLiteral("response")).toObject();
  hist.insert(QStringLiteral("What"), QStringLiteral("[IM_HIST]"));
  if (!hist.isEmpty()) {
    handlePayload(hist);
  }
}

int ChatManager::sendMessage(const QString &peer, const QString &text)
{
  if (!m_api && !m_demoMode) {
    return -1;
  }

  const QString normalized = normalizePeer(peer);
  int reqId = -1;

  if (!m_demoMode) {
    if (isPhonePeer(normalized)) {
      if (m_smsFromNumber.isEmpty()) {
        qCWarning(lcChat) << "SMS sender number is not configured";
        return -1;
      }
      const QString to = phoneFromPeer(normalized);
      m_api->sendSms(m_smsFromNumber, to, text);
    } else {
      reqId = m_api->sendIm(normalized, text);
    }
  }

  InstantMessage im;
  im.peer = normalized;
  im.body = text;
  im.incoming = false;
  im.timestamp = QDateTime::currentDateTime();
  m_messages[normalized].append(im);
  emit messageReceived(im);
  return reqId;
}

void ChatManager::setDemoMode(bool enabled)
{
  m_demoMode = enabled;
  if (!enabled) {
    clearDemoMessages();
  }
}

void ChatManager::clearDemoMessages()
{
  m_messages.clear();
  m_unreadByPeer.clear();
}

void ChatManager::addDemoMessage(const QString &peer, const QString &text, bool incoming, bool notify)
{
  const QString normalized = normalizePeer(peer);
  InstantMessage im;
  im.peer = normalized;
  im.body = text;
  im.incoming = incoming;
  im.timestamp = QDateTime::currentDateTime();
  m_messages[normalized].append(im);
  if (!notify) {
    return;
  }
  if (incoming) {
    m_unreadByPeer[normalized].append(QStringLiteral("demo-%1").arg(im.timestamp.toMSecsSinceEpoch()));
    emit unreadChanged(normalized);
  }
  emit messageReceived(im);
}

void ChatManager::setDemoPeerColor(const QString &peer, const QString &color)
{
  if (color.isEmpty()) {
    return;
  }
  const QString normalized = normalizePeer(peer);
  m_peerColors[normalized] = color;
  emit peerColorReceived(normalized, color);
}

bool ChatManager::hasUnread(const QString &peer) const
{
  return !m_unreadByPeer.value(normalizePeer(peer)).isEmpty();
}

void ChatManager::markPeerRead(const QString &peer)
{
  const QString normalized = normalizePeer(peer);
  const QStringList ids = m_unreadByPeer.value(normalized);
  if (ids.isEmpty()) {
    return;
  }
  if (m_demoMode || !m_api) {
    m_unreadByPeer.remove(normalized);
  } else {
    sendSeen(normalized, ids);
  }
  emit unreadChanged(normalized);
}

void ChatManager::sendSeen(const QString &peer, const QStringList &origIds)
{
  if (!m_api || origIds.isEmpty()) {
    return;
  }
  m_api->sendIm(normalizePeer(peer), origIds, QStringLiteral("seen"));
  m_unreadByPeer.remove(normalizePeer(peer));
}

void ChatManager::loadHistory(const QString &lastKnownId)
{
  if (m_demoMode || !m_api) {
    return;
  }
  m_historyRequestId = m_api->loadImHistory(lastKnownId);
}

QList<InstantMessage> ChatManager::messagesForPeer(const QString &peer) const
{
  QList<InstantMessage> list = m_messages.value(normalizePeer(peer));
  std::stable_sort(list.begin(), list.end(), [](const InstantMessage &a, const InstantMessage &b) {
    if (a.timestamp.isValid() != b.timestamp.isValid()) {
      return a.timestamp.isValid();
    }
    if (a.timestamp != b.timestamp) {
      return a.timestamp < b.timestamp;
    }
    return a.id < b.id;
  });
  return list;
}

bool ChatManager::isColorAdvertisement(const QString &body)
{
  static const QRegularExpression re(QStringLiteral("^\\*\\*#[0-9a-fA-F]{6}\\*\\*$"));
  return re.match(body.trimmed()).hasMatch();
}

QString ChatManager::extractColor(const QString &body)
{
  const QString trimmed = body.trimmed();
  if (trimmed.length() == 10 && trimmed.startsWith(QStringLiteral("**#")) && trimmed.endsWith(QStringLiteral("**"))) {
    return trimmed.mid(2, 7);
  }
  return {};
}

void ChatManager::sendColorAdvertisement(const QString &color)
{
  if (color.isEmpty()) {
    return;
  }
  if (m_demoMode || !m_api) {
    qCDebug(lcChat) << "Color advertisement (demo/local):" << color;
    return;
  }
  const QString body = QStringLiteral("**%1**").arg(color);
  m_api->sendIm(normalizePeer(QStringLiteral("__broadcast__")), body);
}

QString ChatManager::peerColor(const QString &peer) const
{
  return m_peerColors.value(normalizePeer(peer));
}

} // namespace itl
