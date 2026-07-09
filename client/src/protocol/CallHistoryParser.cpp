#include "CallHistoryParser.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonValue>

#include <algorithm>

namespace itl {

namespace {

bool domainsMatch(const QString &peerDomain, const QString &domain)
{
  if (peerDomain.isEmpty() || domain.isEmpty()) {
    return true;
  }
  const QString a = peerDomain.toLower();
  const QString b = domain.toLower();
  return a == b || a.endsWith(QLatin1Char('.') + b) || b.endsWith(QLatin1Char('.') + a);
}

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

QString pickAddress(const QJsonObject &item, const QString &extKey, const QString &addrKey)
{
  const QString ext = item.value(extKey).toString();
  if (!ext.isEmpty()) {
    return ext;
  }
  return item.value(addrKey).toString();
}

QString resolveToAddress(const QJsonObject &item, const QString &domain)
{
  QString to = pickAddress(item, QStringLiteral("ToExt"), QStringLiteral("To"));
  const QJsonObject to2 = item.value(QStringLiteral("To2")).toObject();
  if (!to2.isEmpty()) {
    const QString addrType = to2.value(QStringLiteral("AddrType")).toString();
    const QString name = to2.value(QStringLiteral("Name")).toString();
    if (addrType == QStringLiteral("account") && !name.isEmpty() && !domain.isEmpty()) {
      to = name + QLatin1Char('@') + domain;
    } else if (addrType != QStringLiteral("group") && !name.isEmpty()) {
      to = name;
    }
    const QString realName = to2.value(QStringLiteral("RealName")).toString();
    if (!realName.isEmpty() && !name.isEmpty()) {
      // RealName is handled separately by caller.
    }
  }
  const QString sipTrunk = item.value(QStringLiteral("SipTrunkTelnum")).toString();
  if (!sipTrunk.isEmpty()) {
    to = sipTrunk;
  }
  return to;
}

QString formatEmployee(const QString &addr, const QString &name)
{
  if (!name.isEmpty() && !addr.isEmpty()) {
    return name + QStringLiteral(" <") + addr + QLatin1Char('>');
  }
  if (!name.isEmpty()) {
    return name;
  }
  return addr;
}

bool isSelfAddress(const QString &addr, const CallHistoryParseContext &context)
{
  if (addr.isEmpty()) {
    return false;
  }
  if (!context.selfPeer.isEmpty() && addr == context.selfPeer) {
    return true;
  }
  if (!context.selfLogin.isEmpty()) {
    if (addr == context.selfLogin) {
      return true;
    }
    if (!context.domain.isEmpty() && addr == context.selfLogin + QLatin1Char('@') + context.domain) {
      return true;
    }
  }
  return false;
}

qint64 parseStartMs(const QJsonObject &item)
{
  const QString start = item.value(QStringLiteral("Start")).toString();
  if (start.isEmpty()) {
    return 0;
  }
  QDateTime dt = QDateTime::fromString(start, Qt::ISODateWithMs);
  if (!dt.isValid()) {
    dt = QDateTime::fromString(start, Qt::ISODate);
  }
  return dt.isValid() ? dt.toMSecsSinceEpoch() : 0;
}

QString normalizeBareExt(const QString &addr, const QString &domain)
{
  if (addr.isEmpty() || addr.contains(QLatin1Char('@')) || domain.isEmpty()) {
    return addr;
  }
  if (addr.size() == countDigits(addr) && countDigits(addr) > 0 && countDigits(addr) <= 5) {
    return addr + QLatin1Char('@') + domain;
  }
  return addr;
}

} // namespace

bool isInternalPeer(const QString &peer, const QString &domain)
{
  if (peer.isEmpty()) {
    return false;
  }

  const int at = peer.indexOf(QLatin1Char('@'));
  const QString local = at >= 0 ? peer.left(at) : peer;
  if (local.isEmpty() || local.size() != countDigits(local) || countDigits(local) == 0) {
    return false;
  }

  // Внутренний номер: до 5 цифр, только цифры до «@домен».
  if (countDigits(local) > 5) {
    return false;
  }

  if (at >= 0 && !domain.isEmpty()) {
    const QString peerDomain = peer.mid(at + 1);
    if (!domainsMatch(peerDomain, domain)) {
      return false;
    }
  }

  return true;
}

bool historyEntryIsInternal(const CallHistoryEntry &entry, const QString &domain)
{
  if (entry.isInnerCall) {
    return true;
  }
  if (isInternalPeer(entry.peer, domain)) {
    return true;
  }
  if (isInternalPeer(entry.fromAddr, domain)) {
    return true;
  }
  if (isInternalPeer(entry.toAddr, domain)) {
    return true;
  }
  return false;
}

QList<CallHistoryEntry> parseServerCallHistory(const QJsonValue &callsValue,
                                               const CallHistoryParseContext &context)
{
  QList<CallHistoryEntry> result;

  auto appendItem = [&](const QJsonObject &item) {
    if (item.value(QStringLiteral("ID")).toString().isEmpty()) {
      return;
    }

    const QString callType = item.value(QStringLiteral("CallType")).toString().toLower();
    const QString error = item.value(QStringLiteral("Error")).toString();

    QString from = normalizeBareExt(pickAddress(item, QStringLiteral("FromExt"), QStringLiteral("From")), context.domain);
    QString to = normalizeBareExt(resolveToAddress(item, context.domain), context.domain);
    QString fromName = item.value(QStringLiteral("FromName")).toString();
    QString toName = item.value(QStringLiteral("ToName")).toString();

    const QJsonObject to2 = item.value(QStringLiteral("To2")).toObject();
    const QString to2RealName = to2.value(QStringLiteral("RealName")).toString();
    if (!to2RealName.isEmpty()) {
      toName = to2RealName;
    }

    bool incoming = callType == QStringLiteral("in") || callType == QStringLiteral("missed");
    bool outgoing = callType == QStringLiteral("out") || callType == QStringLiteral("noanswer");
    const bool inner = callType == QStringLiteral("inner");

    if (inner) {
      if (isSelfAddress(to, context)) {
        incoming = true;
      } else {
        outgoing = true;
      }
    } else if (outgoing && isSelfAddress(to, context)) {
      incoming = true;
      outgoing = false;
    }

    CallHistoryEntry entry;
    entry.fromAddr = from;
    entry.toAddr = to;
    entry.isInnerCall = inner;
    entry.direction = incoming ? QStringLiteral("incoming") : QStringLiteral("outgoing");
    entry.answered = error.isEmpty()
        && callType != QStringLiteral("missed")
        && callType != QStringLiteral("noanswer");
    entry.durationSec = item.value(QStringLiteral("Duration")).toInt();
    entry.startedAtMs = parseStartMs(item);

    const int waitSec = item.value(QStringLiteral("Wait")).toInt();
    if (entry.answered && waitSec > 0) {
      entry.connectedAtMs = entry.startedAtMs + static_cast<qint64>(waitSec) * 1000;
    }
    if (entry.durationSec > 0 && entry.connectedAtMs > 0) {
      entry.endedAtMs = entry.connectedAtMs + static_cast<qint64>(entry.durationSec) * 1000;
    } else if (entry.startedAtMs > 0) {
      entry.endedAtMs = entry.startedAtMs + static_cast<qint64>(qMax(waitSec, entry.durationSec)) * 1000;
    }

    if (incoming) {
      entry.peer = from;
      entry.displayName = fromName.isEmpty() ? from : fromName;
      entry.employeeInfo = formatEmployee(to, toName);
    } else {
      entry.peer = to;
      entry.displayName = toName.isEmpty() ? to : toName;
      entry.employeeInfo = formatEmployee(from, fromName);
    }

    if (inner) {
      if (incoming && isInternalPeer(from, context.domain)) {
        entry.peer = from;
      } else if (!incoming && isInternalPeer(to, context.domain)) {
        entry.peer = to;
      }
    }

    if (entry.peer.isEmpty()) {
      return;
    }

    if (entry.answered) {
      entry.result = QStringLiteral("connected");
    } else if (incoming) {
      entry.result = QStringLiteral("missed");
    } else {
      entry.result = QStringLiteral("no-answer");
    }

    result.append(entry);
  };

  if (callsValue.isArray()) {
    for (const QJsonValue &value : callsValue.toArray()) {
      if (value.isObject()) {
        appendItem(value.toObject());
      }
    }
  } else if (callsValue.isObject()) {
    const QJsonObject callsObject = callsValue.toObject();
    for (auto it = callsObject.begin(); it != callsObject.end(); ++it) {
      if (it.value().isObject()) {
        appendItem(it.value().toObject());
      }
    }
  }

  std::sort(result.begin(), result.end(), [](const CallHistoryEntry &a, const CallHistoryEntry &b) {
    return a.startedAtMs > b.startedAtMs;
  });
  return result;
}

} // namespace itl
