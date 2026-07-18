#include "ChatManager.h"

#include "protocol/WsApiClient.h"
#include "settings/UserDataStore.h"

#include <QBuffer>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QJsonArray>
#include <QLoggingCategory>
#include <QRegularExpression>

#include <algorithm>

Q_LOGGING_CATEGORY(lcChat, "itl.chat")

namespace itl {

namespace {
constexpr int kAvatarShareSide = 140;
constexpr int kMaxFileTransferBytes = 96 * 1024;
constexpr int kMaxThemeShareBytes = 384 * 1024;
const QString kThemeShareNoticePrefix = QStringLiteral("__osc_theme__:");
const QString kFileShareNoticePrefix = QStringLiteral("__osc_file__:");
const QString kThemeAppliedNoticeBody = QStringLiteral("__osc_theme_applied__");

QString sanitizeTransferFileName(QString name)
{
  name = name.trimmed();
  if (name.isEmpty()) {
    name = QStringLiteral("file.bin");
  }
  name.replace(QLatin1Char(';'), QLatin1Char('_'));
  name.replace(QLatin1Char('*'), QLatin1Char('_'));
  return name;
}

bool charIsDialable(QChar ch)
{
  return ch.isDigit() || ch == QLatin1Char('+') || ch == QLatin1Char('*') || ch == QLatin1Char('#');
}

bool isLoopbackHost(const QString &host)
{
  const QString normalized = host.trimmed().toLower();
  return normalized == QStringLiteral("localhost") || normalized == QStringLiteral("127.0.0.1")
         || normalized == QStringLiteral("::1");
}

QString normalizeLoopbackHost(const QString &host, const QString &sessionDomain)
{
  if (!isLoopbackHost(host)) {
    return host.trimmed().toLower();
  }
  if (!sessionDomain.isEmpty()) {
    return sessionDomain.trimmed().toLower();
  }
  return QStringLiteral("127.0.0.1");
}

QRegularExpression fileTransferRe()
{
  return QRegularExpression(
      QStringLiteral("^\\*\\*fnm=([^;]+);enc=b64;cnt=([A-Za-z0-9+/=]+)\\*\\*$"));
}

QRegularExpression themeShareRe()
{
  return QRegularExpression(
      QStringLiteral("^\\*\\*fnm=theme\\.(?:jpg|jpeg|png);enc=b64;ui=(\\d+);list=(\\d+);cnt=([A-Za-z0-9+/=]+)\\*\\*$"));
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

void ChatManager::setSelfLogin(const QString &login)
{
  m_selfLogin = login.section(QLatin1Char('@'), 0, 0).trimmed().toLower();
}

void ChatManager::setUserDataStore(UserDataStore *store)
{
  m_userData = store;
}

void ChatManager::loadStoredPeerColors()
{
  if (!m_userData) {
    return;
  }

  for (auto it = m_userData->peerColors().cbegin(); it != m_userData->peerColors().cend(); ++it) {
    m_peerColors.insert(it.key(), it.value());
    const QString login = it.key().section(QLatin1Char('@'), 0, 0);
    if (!login.isEmpty()) {
      m_peerColors.insert(login, it.value());
    }
  }
}

void ChatManager::loadStoredPeerAvatars()
{
  if (!m_userData) {
    return;
  }

  for (auto it = m_userData->peerAvatars().cbegin(); it != m_userData->peerAvatars().cend(); ++it) {
    const QByteArray raw = QByteArray::fromBase64(it.value().toLatin1());
    QPixmap pixmap;
    if (raw.isEmpty() || !pixmap.loadFromData(raw, "PNG")) {
      continue;
    }
    const QString key = canonicalPeer(it.key());
    m_peerAvatars.insert(key, pixmap);
    const QString login = key.section(QLatin1Char('@'), 0, 0);
    if (!login.isEmpty()) {
      m_peerAvatars.insert(login, pixmap);
    }
  }
}

void ChatManager::loadStoredOscPeers()
{
  if (!m_userData) {
    return;
  }
  for (const QString &peer : m_userData->oscPeers()) {
    const QString key = canonicalPeer(peer);
    if (!key.isEmpty() && !isPhonePeer(key)) {
      m_oscPeers.insert(key);
    }
  }
  qCInfo(lcChat) << "Loaded OSC peers:" << m_oscPeers.size();
}

QString ChatManager::canonicalPeer(QString peer) const
{
  peer = normalizePeer(std::move(peer));
  const int at = peer.indexOf(QLatin1Char('@'));
  if (at > 0) {
    return peer.left(at).toLower() + peer.mid(at).toLower();
  }
  return peer.toLower();
}

QString ChatManager::imColorAdPeer(const QJsonObject &payload, const QJsonObject &msg) const
{
  const QString from = msg.value(QStringLiteral("from")).toString().trimmed();
  if (!from.isEmpty()) {
    const QString fromLogin = from.section(QLatin1Char('@'), 0, 0).toLower();
    if (m_selfLogin.isEmpty() || fromLogin != m_selfLogin) {
      return normalizePeer(from);
    }
  }

  const QString chatId = payload.value(QStringLiteral("chatId")).toString().trimmed();
  if (!chatId.isEmpty()) {
    const QString chatLogin = chatId.section(QLatin1Char('@'), 0, 0).toLower();
    if (m_selfLogin.isEmpty() || chatLogin != m_selfLogin) {
      return normalizePeer(chatId);
    }
  }

  return normalizePeer(from);
}

QString ChatManager::messageBody(const QJsonObject &msg)
{
  const QJsonValue bodyValue = msg.value(QStringLiteral("body"));
  if (bodyValue.isObject()) {
    return bodyValue.toObject().value(QString::fromUtf8(kEmptyKey)).toString();
  }
  if (bodyValue.isArray()) {
    return {};
  }
  return bodyValue.toString();
}

bool ChatManager::isEphemeralColorAdvertisement(const QJsonObject &msg, const QString &body)
{
  if (!isColorAdvertisement(body)) {
    return false;
  }

  // Color broadcasts use persist=false and arrive without a normal chat message id.
  // A persisted chat line like "**#123456**" must stay plain text, not override avatar color.
  const QString id = msg.value(QStringLiteral("id")).toString().trimmed();
  return id.isEmpty() || id == QStringLiteral("0");
}

bool ChatManager::isEphemeralFileTransfer(const QJsonObject &msg, const QString &body)
{
  if (!isFileTransfer(body)) {
    return false;
  }
  const QString id = msg.value(QStringLiteral("id")).toString().trimmed();
  return id.isEmpty() || id == QStringLiteral("0");
}

void ChatManager::storePeerColor(const QString &peer, const QString &color)
{
  if (peer.isEmpty() || color.isEmpty()) {
    return;
  }
  const QString key = canonicalPeer(peer);
  m_peerColors[key] = color;
  const QString login = key.section(QLatin1Char('@'), 0, 0);
  if (!login.isEmpty()) {
    m_peerColors[login] = color;
  }
  if (m_userData && !m_demoMode) {
    m_userData->setPeerColorForPeer(key, color);
  }
  qCInfo(lcChat) << "Color advertisement received from" << key << ":" << color;
  emit peerColorReceived(key, color);
}

void ChatManager::storePeerAvatar(const QString &peer, const QPixmap &avatar, const QString &base64Png)
{
  if (peer.isEmpty() || avatar.isNull()) {
    return;
  }
  const QString key = canonicalPeer(peer);
  m_peerAvatars[key] = avatar;
  const QString login = key.section(QLatin1Char('@'), 0, 0);
  if (!login.isEmpty()) {
    m_peerAvatars[login] = avatar;
  }

  QString b64 = base64Png.trimmed();
  if (b64.isEmpty()) {
    QByteArray png;
    QBuffer buffer(&png);
    buffer.open(QIODevice::WriteOnly);
    avatar.save(&buffer, "PNG");
    b64 = QString::fromLatin1(png.toBase64());
  }
  if (m_userData && !m_demoMode && !b64.isEmpty()) {
    m_userData->setPeerAvatarForPeer(key, b64);
  }
  qCInfo(lcChat) << "Avatar share received from" << key << "size" << avatar.size();
  emit peerAvatarReceived(key, avatar);
}

bool ChatManager::ingestFileTransfer(const QString &peer, const QString &body)
{
  const QByteArray raw = extractFileTransferData(body);
  if (raw.isEmpty() || raw.size() > kMaxFileTransferBytes) {
    return false;
  }
  QPixmap pixmap;
  if (!pixmap.loadFromData(raw, "PNG") && !pixmap.loadFromData(raw)) {
    return false;
  }
  storePeerAvatar(peer, pixmap, QString::fromLatin1(raw.toBase64()));
  return true;
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

  const int at = peer.indexOf(QLatin1Char('@'));
  if (at > 0) {
    const QString login = peer.left(at).trimmed();
    const QString host = normalizeLoopbackHost(peer.mid(at + 1), m_domain);
    return login + QLatin1Char('@') + host;
  }

  if (!m_domain.isEmpty()) {
    peer = peer.trimmed() + QLatin1Char('@') + normalizeLoopbackHost(m_domain, m_domain);
  }
  return peer.trimmed();
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

  const QString fromLogin = msg.value(QStringLiteral("from")).toString().section(QLatin1Char('@'), 0, 0).toLower();
  const QString toField = msg.value(QStringLiteral("to")).toString();
  if (!toField.isEmpty()) {
    if (!m_selfLogin.isEmpty() && fromLogin == m_selfLogin) {
      im.peer = normalizePeer(toField);
      im.incoming = false;
    } else if (!fromLogin.isEmpty()) {
      im.peer = normalizePeer(msg.value(QStringLiteral("from")).toString());
      im.incoming = true;
    } else {
      im.peer = normalizePeer(toField);
      im.incoming = false;
    }
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
  } else if (!msg.value(QStringLiteral("body")).isArray()) {
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

    const QString body = messageBody(raw);
    if (!body.isEmpty() && isOpenping(body)) {
      const QString sender = imColorAdPeer(payload, raw);
      if (!sender.isEmpty()) {
        handleIncomingOpenping(sender);
      }
      return;
    }
    if (!body.isEmpty() && isThemeApplied(body)) {
      const QString sender = imColorAdPeer(payload, raw);
      if (!sender.isEmpty()) {
        handleIncomingThemeApplied(sender);
      }
      return;
    }
    if (!body.isEmpty() && isEphemeralFileTransfer(raw, body) && isAvatarFileTransfer(body)) {
      const QString sender = imColorAdPeer(payload, raw);
      if (!sender.isEmpty()) {
        addOscPeer(sender);
        ingestFileTransfer(sender, body);
      }
      return;
    }
    if (!body.isEmpty() && isEphemeralColorAdvertisement(raw, body)) {
      const QString sender = imColorAdPeer(payload, raw);
      if (!sender.isEmpty()) {
        addOscPeer(sender);
        storePeerColor(sender, extractColor(body));
      }
      return;
    }

    const InstantMessage im = parseMessage(raw);
    if (im.body.isEmpty() || im.id == QStringLiteral("0")) {
      return;
    }
    if (isOpenping(im.body)) {
      if (im.incoming) {
        handleIncomingOpenping(im.peer);
      }
      return;
    }
    if (isThemeApplied(im.body)) {
      if (im.incoming) {
        handleIncomingThemeApplied(im.peer, im.id, im.origId);
      }
      return;
    }
    if (isThemeShare(im.body)) {
      InstantMessage notice = im;
      ThemeSharePayload payload;
      if (!parseThemeShareBody(im.body, &payload)) {
        return;
      }
      if (im.incoming) {
        addOscPeer(im.peer);
      }
      const QString key = registerThemeShare(im.peer, payload, im.id);
      notice.body = themeShareNoticeBody(key);
      if (!storeMessage(notice, /*replaceOptimisticOutgoing=*/true)) {
        return;
      }
      if (notice.incoming && !notice.origId.isEmpty()) {
        m_unreadByPeer[notice.peer].append(notice.origId);
        emit unreadChanged(notice.peer);
      }
      emit messageReceived(notice);
      return;
    }
    if (isChatFileTransfer(im.body)) {
      InstantMessage notice = im;
      const QString key = registerChatFile(im.peer, im.body, im.id);
      if (key.isEmpty()) {
        return;
      }
      if (im.incoming) {
        addOscPeer(im.peer);
      }
      notice.body = fileShareNoticeBody(key);
      if (!storeMessage(notice, /*replaceOptimisticOutgoing=*/true)) {
        return;
      }
      if (notice.incoming && !notice.origId.isEmpty()) {
        m_unreadByPeer[notice.peer].append(notice.origId);
        emit unreadChanged(notice.peer);
      }
      emit messageReceived(notice);
      return;
    }
    if (isFileTransfer(im.body) || isColorAdvertisement(im.body)) {
      // Persisted system footers must not appear as chat lines.
      if (isAvatarFileTransfer(im.body) && im.incoming) {
        ingestFileTransfer(im.peer, im.body);
      } else if (isColorAdvertisement(im.body) && im.incoming) {
        storePeerColor(im.peer, extractColor(im.body));
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
      const QJsonObject msgObj = value.toObject();
      const InstantMessage im = parseMessage(msgObj);
      if (im.body.isEmpty()) {
        continue;
      }
      if (isOpenping(im.body)) {
        continue;
      }
      if (isThemeApplied(im.body)) {
        if (im.incoming) {
          InstantMessage notice = im;
          notice.body = themeAppliedNoticeBody();
          chatPeer = notice.peer;
          storeMessage(notice, /*replaceOptimisticOutgoing=*/true);
        }
        continue;
      }
      if (isThemeShare(im.body)) {
        ThemeSharePayload payload;
        if (!parseThemeShareBody(im.body, &payload)) {
          continue;
        }
        if (im.incoming) {
          addOscPeer(im.peer);
        }
        const QString key = registerThemeShare(im.peer, payload, im.id);
        InstantMessage notice = im;
        notice.body = themeShareNoticeBody(key);
        chatPeer = notice.peer;
        storeMessage(notice, /*replaceOptimisticOutgoing=*/true);
        continue;
      }
      if (isChatFileTransfer(im.body)) {
        const QString key = registerChatFile(im.peer, im.body, im.id);
        if (key.isEmpty()) {
          continue;
        }
        if (im.incoming) {
          addOscPeer(im.peer);
        }
        InstantMessage notice = im;
        notice.body = fileShareNoticeBody(key);
        chatPeer = notice.peer;
        storeMessage(notice, /*replaceOptimisticOutgoing=*/true);
        continue;
      }
      if (isAvatarFileTransfer(im.body)) {
        if (im.incoming) {
          ingestFileTransfer(im.peer, im.body);
        }
        continue;
      }
      if (isFileTransfer(im.body)) {
        continue;
      }
      if (isColorAdvertisement(im.body)) {
        continue;
      }
      chatPeer = im.peer;
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
  const QString trimmed = text.trimmed();
  int reqId = -1;

  if (m_demoMode) {
    // Assign avatar color to this contact (hidden from chat history, like real color ads).
    if (isColorAdvertisement(trimmed)) {
      const QString color = extractColor(trimmed);
      if (!color.isEmpty()) {
        storePeerColor(normalized, color);
      }
      return -1;
    }
    if (isFileTransfer(trimmed)) {
      if (isThemeShare(trimmed)) {
        ThemeSharePayload payload;
        if (parseThemeShareBody(trimmed, &payload)) {
          const QString key = registerThemeShare(normalized, payload, {});
          addDemoMessage(normalized, themeShareNoticeBody(key), true, true);
        }
      } else if (isAvatarFileTransfer(trimmed)) {
        ingestFileTransfer(normalized, trimmed);
      } else if (isChatFileTransfer(trimmed)) {
        const QString key = registerChatFile(normalized, trimmed, {});
        if (!key.isEmpty()) {
          addDemoMessage(normalized, fileShareNoticeBody(key), true, true);
        }
      }
      return -1;
    }
    // «Теперь ты Имя» — rename contact in the demo address book.
    static const QString kRenamePrefix = QStringLiteral("Теперь ты ");
    if (trimmed.startsWith(kRenamePrefix)) {
      const QString newName = trimmed.mid(kRenamePrefix.size()).trimmed();
      if (!newName.isEmpty()) {
        emit demoPeerRenameRequested(normalized, newName);
      }
    }
  } else {
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
  storePeerColor(peer, color);
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
  static const QRegularExpression re(QStringLiteral("^\\*\\*(#[0-9a-fA-F]{6})\\*\\*$"));
  const QRegularExpressionMatch match = re.match(body.trimmed());
  if (match.hasMatch()) {
    return match.captured(1);
  }
  return {};
}

bool ChatManager::isFileTransfer(const QString &body)
{
  return fileTransferRe().match(body.trimmed()).hasMatch() && !isThemeShare(body);
}

bool ChatManager::isAvatarFileTransfer(const QString &body)
{
  return isFileTransfer(body)
      && extractFileTransferName(body).compare(QStringLiteral("avatar.png"), Qt::CaseInsensitive) == 0;
}

bool ChatManager::isChatFileTransfer(const QString &body)
{
  return isFileTransfer(body) && !isAvatarFileTransfer(body);
}

bool ChatManager::isThemeShare(const QString &body)
{
  return themeShareRe().match(body.trimmed()).hasMatch();
}

bool ChatManager::isThemeShareNotice(const QString &body)
{
  return body.startsWith(kThemeShareNoticePrefix);
}

QString ChatManager::themeShareNoticeKey(const QString &body)
{
  if (!isThemeShareNotice(body)) {
    return {};
  }
  return body.mid(kThemeShareNoticePrefix.size());
}

QString ChatManager::themeShareNoticeBody(const QString &key)
{
  return kThemeShareNoticePrefix + key;
}

bool ChatManager::isThemeApplied(const QString &body)
{
  return body.trimmed() == QStringLiteral("Themeapplied!");
}

bool ChatManager::isThemeAppliedNotice(const QString &body)
{
  return body == kThemeAppliedNoticeBody;
}

QString ChatManager::themeAppliedNoticeBody()
{
  return kThemeAppliedNoticeBody;
}

ThemeSharePayload ChatManager::themeShareOffer(const QString &key) const
{
  return m_themeShares.value(key);
}

bool ChatManager::isFileShareNotice(const QString &body)
{
  return body.startsWith(kFileShareNoticePrefix);
}

QString ChatManager::fileShareNoticeKey(const QString &body)
{
  if (!isFileShareNotice(body)) {
    return {};
  }
  return body.mid(kFileShareNoticePrefix.size());
}

QString ChatManager::fileShareNoticeBody(const QString &key)
{
  return kFileShareNoticePrefix + key;
}

ChatFilePayload ChatManager::chatFileOffer(const QString &key) const
{
  return m_chatFiles.value(key);
}

bool ChatManager::parseThemeShareBody(const QString &body, ThemeSharePayload *out) const
{
  if (!out) {
    return false;
  }
  const QRegularExpressionMatch match = themeShareRe().match(body.trimmed());
  if (!match.hasMatch()) {
    return false;
  }

  const QByteArray raw = QByteArray::fromBase64(match.captured(3).toLatin1());
  if (raw.isEmpty() || raw.size() > kMaxThemeShareBytes) {
    return false;
  }

  QPixmap pixmap;
  if (!pixmap.loadFromData(raw, "JPEG") && !pixmap.loadFromData(raw, "PNG")) {
    return false;
  }

  out->wallpaper = pixmap;
  out->uiOpacity = qBound(0, match.captured(1).toInt(), 100);
  out->listOpacity = qBound(0, match.captured(2).toInt(), 100);
  return true;
}

QString ChatManager::registerThemeShare(const QString &peer, const ThemeSharePayload &payload,
                                          const QString &msgId)
{
  QString key = msgId.trimmed();
  if (key.isEmpty()) {
    key = QStringLiteral("%1:%2")
              .arg(canonicalPeer(peer))
              .arg(QDateTime::currentMSecsSinceEpoch());
  }
  m_themeShares.insert(key, payload);
  return key;
}

QString ChatManager::registerChatFile(const QString &peer, const QString &body, const QString &msgId)
{
  const QByteArray raw = extractFileTransferData(body);
  if (raw.isEmpty() || raw.size() > kMaxFileTransferBytes) {
    return {};
  }
  ChatFilePayload payload;
  payload.fileName = sanitizeTransferFileName(extractFileTransferName(body));
  payload.data = raw;

  QString key = msgId.trimmed();
  if (key.isEmpty()) {
    key = QStringLiteral("%1:%2")
              .arg(canonicalPeer(peer))
              .arg(QDateTime::currentMSecsSinceEpoch());
  }
  m_chatFiles.insert(key, payload);
  return key;
}

QString ChatManager::extractFileTransferName(const QString &body)
{
  const QRegularExpressionMatch match = fileTransferRe().match(body.trimmed());
  return match.hasMatch() ? match.captured(1) : QString();
}

QByteArray ChatManager::extractFileTransferData(const QString &body)
{
  const QRegularExpressionMatch match = fileTransferRe().match(body.trimmed());
  if (!match.hasMatch()) {
    return {};
  }
  return QByteArray::fromBase64(match.captured(2).toLatin1());
}

QPixmap ChatManager::scaleAvatarForShare(const QPixmap &pixmap)
{
  if (pixmap.isNull()) {
    return {};
  }
  const QPixmap scaled =
      pixmap.scaled(kAvatarShareSide, kAvatarShareSide, Qt::KeepAspectRatioByExpanding,
                    Qt::SmoothTransformation);
  if (scaled.width() == kAvatarShareSide && scaled.height() == kAvatarShareSide) {
    return scaled;
  }
  const int x = qMax(0, (scaled.width() - kAvatarShareSide) / 2);
  const int y = qMax(0, (scaled.height() - kAvatarShareSide) / 2);
  return scaled.copy(x, y, kAvatarShareSide, kAvatarShareSide);
}

QString ChatManager::encodeAvatarShareBody(const QPixmap &scaledPngPixmap, QByteArray *outPng)
{
  if (scaledPngPixmap.isNull()) {
    return {};
  }
  QByteArray png;
  QBuffer buffer(&png);
  buffer.open(QIODevice::WriteOnly);
  if (!scaledPngPixmap.save(&buffer, "PNG") || png.isEmpty() || png.size() > kMaxFileTransferBytes) {
    return {};
  }
  if (outPng) {
    *outPng = png;
  }
  return QStringLiteral("**fnm=avatar.png;enc=b64;cnt=%1**").arg(QString::fromLatin1(png.toBase64()));
}

QString ChatManager::encodeChatFileBody(const QString &fileName, const QByteArray &data)
{
  if (data.isEmpty() || data.size() > kMaxFileTransferBytes) {
    return {};
  }
  return QStringLiteral("**fnm=%1;enc=b64;cnt=%2**")
      .arg(sanitizeTransferFileName(fileName), QString::fromLatin1(data.toBase64()));
}

QString ChatManager::encodeThemeShareBody(const QPixmap &wallpaper, int uiOpacity, int listOpacity,
                                          QByteArray *outJpeg)
{
  if (wallpaper.isNull()) {
    return {};
  }
  QByteArray jpeg;
  QBuffer buffer(&jpeg);
  buffer.open(QIODevice::WriteOnly);
  if (!wallpaper.save(&buffer, "JPEG", 85) || jpeg.isEmpty() || jpeg.size() > kMaxThemeShareBytes) {
    return {};
  }
  if (outJpeg) {
    *outJpeg = jpeg;
  }
  const int ui = qBound(0, uiOpacity, 100);
  const int list = qBound(0, listOpacity, 100);
  return QStringLiteral("**fnm=theme.jpg;enc=b64;ui=%1;list=%2;cnt=%3**")
      .arg(ui)
      .arg(list)
      .arg(QString::fromLatin1(jpeg.toBase64()));
}

bool ChatManager::sendThemeShare(const QString &peer, const QPixmap &wallpaper, int uiOpacity,
                                 int listOpacity)
{
  const QString normalized = normalizePeer(peer);
  if (normalized.isEmpty() || isPhonePeer(normalized) || wallpaper.isNull()) {
    return false;
  }
  if (!m_demoMode && !isOscPeer(normalized)) {
    qCWarning(lcChat) << "Theme share rejected: peer is not an OSC client" << normalized;
    return false;
  }

  QByteArray jpeg;
  const QString body = encodeThemeShareBody(wallpaper, uiOpacity, listOpacity, &jpeg);
  if (body.isEmpty()) {
    return false;
  }

  if (m_demoMode) {
    if (!isOscPeer(normalized)) {
      return false;
    }
    ThemeSharePayload payload;
    payload.wallpaper = wallpaper;
    payload.uiOpacity = qBound(0, uiOpacity, 100);
    payload.listOpacity = qBound(0, listOpacity, 100);
    const QString key = registerThemeShare(normalized, payload, {});
    InstantMessage outgoing;
    outgoing.peer = normalized;
    outgoing.body = tr("Вы поделились темой");
    outgoing.incoming = false;
    outgoing.timestamp = QDateTime::currentDateTime();
    m_messages[normalized].append(outgoing);
    emit messageReceived(outgoing);
    return true;
  }
  if (!m_api) {
    return false;
  }

  m_api->sendIm(normalized, body, {}, true, false);
  qCInfo(lcChat) << "Theme share sent to" << normalized << "bytes" << jpeg.size();

  InstantMessage outgoing;
  outgoing.peer = normalized;
  outgoing.body = tr("Вы поделились темой");
  outgoing.incoming = false;
  outgoing.timestamp = QDateTime::currentDateTime();
  m_messages[normalized].append(outgoing);
  emit messageReceived(outgoing);
  return true;
}

bool ChatManager::sendFileShare(const QString &peer, const QString &filePath)
{
  const QString normalized = normalizePeer(peer);
  if (normalized.isEmpty() || isPhonePeer(normalized) || filePath.isEmpty()) {
    return false;
  }

  QFile file(filePath);
  if (!file.open(QIODevice::ReadOnly)) {
    return false;
  }
  const QByteArray data = file.readAll();
  if (data.isEmpty() || data.size() > kMaxFileTransferBytes) {
    return false;
  }

  const QString fileName = sanitizeTransferFileName(QFileInfo(filePath).fileName());
  const QString body = encodeChatFileBody(fileName, data);
  if (body.isEmpty()) {
    return false;
  }

  if (m_demoMode) {
    const QString key = registerChatFile(normalized, body, {});
    if (key.isEmpty()) {
      return false;
    }
    InstantMessage outgoing;
    outgoing.peer = normalized;
    outgoing.body = fileShareNoticeBody(key);
    outgoing.incoming = false;
    outgoing.timestamp = QDateTime::currentDateTime();
    m_messages[normalized].append(outgoing);
    emit messageReceived(outgoing);
    return true;
  }
  if (!m_api) {
    return false;
  }

  const QString key = registerChatFile(normalized, body, {});
  if (key.isEmpty()) {
    return false;
  }

  m_api->sendIm(normalized, body, {}, true, false);
  qCInfo(lcChat) << "File share sent to" << normalized << "name" << fileName << "bytes" << data.size();

  InstantMessage outgoing;
  outgoing.peer = normalized;
  outgoing.body = fileShareNoticeBody(key);
  outgoing.incoming = false;
  outgoing.timestamp = QDateTime::currentDateTime();
  m_messages[normalized].append(outgoing);
  emit messageReceived(outgoing);
  return true;
}

bool ChatManager::sendThemeApplied(const QString &peer)
{
  const QString normalized = normalizePeer(peer);
  if (normalized.isEmpty() || isPhonePeer(normalized)) {
    return false;
  }
  if (!m_demoMode && !isOscPeer(normalized)) {
    qCWarning(lcChat) << "Themeapplied rejected: peer is not an OSC client" << normalized;
    return false;
  }
  if (m_demoMode) {
    if (!isOscPeer(normalized)) {
      return false;
    }
    return true;
  }
  if (!m_api) {
    return false;
  }
  m_api->sendIm(normalized, QStringLiteral("Themeapplied!"), {}, true, false);
  qCInfo(lcChat) << "Themeapplied sent to" << normalized;
  return true;
}

void ChatManager::handleIncomingThemeApplied(const QString &peer, const QString &msgId,
                                             const QString &origId)
{
  InstantMessage notice;
  notice.peer = normalizePeer(peer);
  notice.id = msgId;
  notice.origId = origId;
  notice.body = themeAppliedNoticeBody();
  notice.incoming = true;
  notice.timestamp = QDateTime::currentDateTime();
  if (!storeMessage(notice, /*replaceOptimisticOutgoing=*/false)) {
    return;
  }
  if (!notice.origId.isEmpty()) {
    m_unreadByPeer[notice.peer].append(notice.origId);
    emit unreadChanged(notice.peer);
  }
  emit messageReceived(notice);
}

bool ChatManager::sendAvatarShare(const QString &peer, const QPixmap &pixmap)
{
  const QString normalized = normalizePeer(peer);
  if (normalized.isEmpty() || isPhonePeer(normalized)) {
    return false;
  }
  if (!m_demoMode && !isOscPeer(normalized)) {
    qCWarning(lcChat) << "Avatar share rejected: peer is not an OSC client" << normalized;
    return false;
  }
  const QPixmap scaled = scaleAvatarForShare(pixmap);
  QByteArray png;
  const QString body = encodeAvatarShareBody(scaled, &png);
  if (body.isEmpty()) {
    return false;
  }

  if (m_demoMode) {
    if (!isOscPeer(normalized)) {
      return false;
    }
    // Solo demo: apply to the selected contact so the list preview is visible.
    storePeerAvatar(normalized, scaled, QString::fromLatin1(png.toBase64()));
    return true;
  }
  if (!m_api) {
    return false;
  }
  m_api->sendIm(normalized, body, {}, false, false);
  qCInfo(lcChat) << "Avatar share sent to" << normalized << "bytes" << png.size();
  return true;
}

bool ChatManager::sendColorShare(const QString &peer)
{
  const QString normalized = normalizePeer(peer);
  if (normalized.isEmpty() || isPhonePeer(normalized)) {
    return false;
  }
  if (m_lastAdvertisedColor.isEmpty()) {
    return false;
  }
  if (m_demoMode) {
    if (!isOscPeer(normalized)) {
      return false;
    }
    storePeerColor(normalized, m_lastAdvertisedColor);
    return true;
  }
  if (!m_api || !isOscPeer(normalized)) {
    return false;
  }
  sendColorToPeer(normalized, m_lastAdvertisedColor);
  return true;
}

void ChatManager::setOpenpingCandidates(const QStringList &peers)
{
  m_openpingCandidates.clear();
  for (const QString &peer : peers) {
    const QString normalized = normalizePeer(peer);
    if (normalized.isEmpty() || isPhonePeer(normalized)) {
      continue;
    }
    const QString login = normalized.section(QLatin1Char('@'), 0, 0).toLower();
    if (!m_selfLogin.isEmpty() && login == m_selfLogin) {
      continue;
    }
    if (!m_openpingCandidates.contains(normalized)) {
      m_openpingCandidates.append(normalized);
    }
  }
  qCInfo(lcChat) << "Openping candidates:" << m_openpingCandidates.size();
}

void ChatManager::sendOpenpingTo(const QString &peer)
{
  if (m_demoMode || !m_api) {
    return;
  }
  const QString normalized = normalizePeer(peer);
  if (normalized.isEmpty() || isPhonePeer(normalized)) {
    return;
  }
  m_api->sendIm(normalized, QStringLiteral("Openping!"), {}, false, false);
  markOpenpingSent(normalized);
}

void ChatManager::sendOpenpingBroadcast()
{
  if (m_demoMode) {
    qCInfo(lcChat) << "Openping broadcast skipped (demo)";
    return;
  }
  if (!m_api) {
    return;
  }
  qCInfo(lcChat) << "Openping broadcast to" << m_openpingCandidates.size() << "contacts";
  for (const QString &peer : m_openpingCandidates) {
    sendOpenpingTo(peer);
  }
}

void ChatManager::markOpenpingSent(const QString &peer)
{
  const QString key = canonicalPeer(peer);
  if (!key.isEmpty()) {
    m_openpingSentAt.insert(key, QDateTime::currentMSecsSinceEpoch());
  }
}

bool ChatManager::recentlySentOpenping(const QString &peer) const
{
  const QString key = canonicalPeer(peer);
  const qint64 sentAt = m_openpingSentAt.value(key, 0);
  if (sentAt <= 0) {
    return false;
  }
  return (QDateTime::currentMSecsSinceEpoch() - sentAt) < 15000;
}

bool ChatManager::discoverOscPeer(const QString &peer)
{
  return addOscPeer(peer);
}

bool ChatManager::addOscPeer(const QString &peer)
{
  const QString key = canonicalPeer(peer);
  if (key.isEmpty() || isPhonePeer(key)) {
    return false;
  }
  const QString login = key.section(QLatin1Char('@'), 0, 0).toLower();
  if (!m_selfLogin.isEmpty() && login == m_selfLogin) {
    return false;
  }
  if (m_oscPeers.contains(key)) {
    return false;
  }
  m_oscPeers.insert(key);
  if (m_userData && !m_demoMode) {
    m_userData->addOscPeer(key);
  }
  qCInfo(lcChat) << "OSC peer discovered:" << key;
  emit oscPeerDiscovered(key);
  emit oscPeersChanged();
  return true;
}

void ChatManager::handleIncomingOpenping(const QString &peer)
{
  addOscPeer(peer);
  const QString key = canonicalPeer(peer);
  if (key.isEmpty()) {
    return;
  }

  // Reply Openping! unless this is an echo to our own recent broadcast/reply (avoids storms).
  if (!recentlySentOpenping(key)) {
    sendOpenpingTo(key);
  }
  // Then advertise ourselves: color and/or avatar.
  sendSelfPresenceTo(key);
}

void ChatManager::sendAvatarToPeer(const QString &peer, const QPixmap &pixmap)
{
  if (pixmap.isNull() || m_demoMode || !m_api) {
    return;
  }
  const QString normalized = normalizePeer(peer);
  if (normalized.isEmpty() || isPhonePeer(normalized) || !isOscPeer(normalized)) {
    return;
  }
  const QPixmap scaled = scaleAvatarForShare(pixmap);
  QByteArray png;
  const QString body = encodeAvatarShareBody(scaled, &png);
  if (body.isEmpty()) {
    return;
  }
  m_api->sendIm(normalized, body, {}, false, false);
  qCInfo(lcChat) << "Avatar auto-share to" << normalized << "bytes" << png.size();
}

void ChatManager::sendSelfPresenceTo(const QString &peer)
{
  if (m_demoMode) {
    return;
  }
  // Photo takes priority; without a photo only the color is sent.
  if (!m_selfShareAvatar.isNull()) {
    sendAvatarToPeer(peer, m_selfShareAvatar);
  } else if (!m_lastAdvertisedColor.isEmpty()) {
    sendColorToPeer(peer, m_lastAdvertisedColor);
  }
}

void ChatManager::setSelfShareProfile(const QString &color, const QPixmap &avatarPhoto)
{
  m_lastAdvertisedColor = color.trimmed();
  if (avatarPhoto.isNull()) {
    m_selfShareAvatar = {};
  } else {
    m_selfShareAvatar = scaleAvatarForShare(avatarPhoto);
  }
}

void ChatManager::sendColorToPeer(const QString &peer, const QString &color)
{
  if (color.isEmpty() || m_demoMode || !m_api) {
    return;
  }
  const QString normalized = normalizePeer(peer);
  if (normalized.isEmpty() || isPhonePeer(normalized) || !isOscPeer(normalized)) {
    return;
  }
  m_api->sendIm(normalized, QStringLiteral("**%1**").arg(color), {}, false, false);
}

QStringList ChatManager::oscPeers() const
{
  QStringList list = m_oscPeers.values();
  std::sort(list.begin(), list.end());
  return list;
}

bool ChatManager::isOscPeer(const QString &peer) const
{
  const QString key = canonicalPeer(peer);
  if (m_oscPeers.contains(key)) {
    return true;
  }
  const QString login = key.section(QLatin1Char('@'), 0, 0);
  for (const QString &known : m_oscPeers) {
    if (known.section(QLatin1Char('@'), 0, 0).compare(login, Qt::CaseInsensitive) == 0) {
      return true;
    }
  }
  return false;
}

void ChatManager::seedDemoOscPeers(const QStringList &peers)
{
  m_oscPeers.clear();
  for (const QString &peer : peers) {
    const QString key = canonicalPeer(peer);
    if (!key.isEmpty() && !isPhonePeer(key)) {
      m_oscPeers.insert(key);
    }
  }
  emit oscPeersChanged();
}

void ChatManager::demoIncomingThemeShare(const QString &peer, const QPixmap &wallpaper, int uiOpacity,
                                         int listOpacity)
{
  if (!m_demoMode || wallpaper.isNull()) {
    return;
  }
  const QString normalized = normalizePeer(peer);
  if (normalized.isEmpty()) {
    return;
  }
  ThemeSharePayload payload;
  payload.wallpaper = wallpaper;
  payload.uiOpacity = qBound(0, uiOpacity, 100);
  payload.listOpacity = qBound(0, listOpacity, 100);
  const QString key = registerThemeShare(normalized, payload, {});
  addDemoMessage(normalized, themeShareNoticeBody(key), true, true);
}

void ChatManager::demoIncomingFileShare(const QString &peer, const QString &fileName, const QByteArray &data)
{
  if (!m_demoMode || data.isEmpty()) {
    return;
  }
  const QString normalized = normalizePeer(peer);
  if (normalized.isEmpty()) {
    return;
  }
  const QString body = encodeChatFileBody(fileName, data);
  if (body.isEmpty()) {
    return;
  }
  const QString key = registerChatFile(normalized, body, {});
  if (key.isEmpty()) {
    return;
  }
  addDemoMessage(normalized, fileShareNoticeBody(key), true, true);
}

void ChatManager::sendColorAdvertisement(const QString &color)
{
  if (!color.isEmpty()) {
    m_lastAdvertisedColor = color;
  }
  if (m_demoMode || !m_api) {
    qCInfo(lcChat) << "Color advertisement (demo/local):" << m_lastAdvertisedColor << "oscPeers"
                   << m_oscPeers.size();
    return;
  }
  if (m_oscPeers.isEmpty()) {
    qCInfo(lcChat) << "Color advertisement skipped: no OSC peers yet";
    return;
  }

  qCInfo(lcChat) << "Color advertisement sent:" << m_lastAdvertisedColor << "to" << m_oscPeers.size()
                 << "OSC peers";
  for (const QString &peer : m_oscPeers) {
    sendSelfPresenceTo(peer);
  }
}

bool ChatManager::isOpenping(const QString &body)
{
  return body.trimmed() == QStringLiteral("Openping!");
}

QString ChatManager::peerColor(const QString &peer) const
{
  const QString key = canonicalPeer(peer);
  if (m_peerColors.contains(key)) {
    return m_peerColors.value(key);
  }

  const QString login = key.section(QLatin1Char('@'), 0, 0);
  if (m_peerColors.contains(login)) {
    return m_peerColors.value(login);
  }

  for (auto it = m_peerColors.cbegin(); it != m_peerColors.cend(); ++it) {
    if (it.key().section(QLatin1Char('@'), 0, 0).compare(login, Qt::CaseInsensitive) == 0) {
      return it.value();
    }
  }
  return {};
}

QPixmap ChatManager::peerAvatar(const QString &peer) const
{
  const QString key = canonicalPeer(peer);
  if (m_peerAvatars.contains(key)) {
    return m_peerAvatars.value(key);
  }
  const QString login = key.section(QLatin1Char('@'), 0, 0);
  if (m_peerAvatars.contains(login)) {
    return m_peerAvatars.value(login);
  }
  for (auto it = m_peerAvatars.cbegin(); it != m_peerAvatars.cend(); ++it) {
    if (it.key().section(QLatin1Char('@'), 0, 0).compare(login, Qt::CaseInsensitive) == 0) {
      return it.value();
    }
  }
  return {};
}

} // namespace itl
