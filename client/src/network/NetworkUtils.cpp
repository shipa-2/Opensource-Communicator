#include "NetworkUtils.h"

#include <algorithm>
#include <QAbstractSocket>
#include <QHostAddress>
#include <QNetworkInterface>

namespace itl::NetworkUtils {

namespace {

bool isUsableInterface(const QNetworkInterface &iface)
{
  return (iface.flags() & QNetworkInterface::IsUp) && !(iface.flags() & QNetworkInterface::IsLoopBack);
}

QString primaryIpv4(const QNetworkInterface &iface)
{
  for (const QNetworkAddressEntry &entry : iface.addressEntries()) {
    const QHostAddress ip = entry.ip();
    if (ip.protocol() == QAbstractSocket::IPv4Protocol && !ip.isLoopback() && !ip.isLinkLocal()) {
      return ip.toString();
    }
  }
  return {};
}

} // namespace

QList<NetworkInterfaceEntry> ipv4Interfaces()
{
  QList<NetworkInterfaceEntry> result;
  for (const QNetworkInterface &iface : QNetworkInterface::allInterfaces()) {
    if (!isUsableInterface(iface)) {
      continue;
    }
    const QString ip = primaryIpv4(iface);
    if (ip.isEmpty()) {
      continue;
    }
    NetworkInterfaceEntry entry;
    entry.systemName = iface.name();
    entry.displayName = iface.humanReadableName();
    entry.ipv4 = ip;
    result.append(entry);
  }

  std::sort(result.begin(), result.end(), [](const NetworkInterfaceEntry &a, const NetworkInterfaceEntry &b) {
    return a.systemName.localeAwareCompare(b.systemName) < 0;
  });
  return result;
}

QString ipv4ForBinding(const QString &interfaceSystemName)
{
  if (!interfaceSystemName.isEmpty()) {
    for (const QNetworkInterface &iface : QNetworkInterface::allInterfaces()) {
      if (iface.name() != interfaceSystemName || !isUsableInterface(iface)) {
        continue;
      }
      const QString ip = primaryIpv4(iface);
      if (!ip.isEmpty()) {
        return ip;
      }
    }
    return {};
  }

  for (const QNetworkInterface &iface : QNetworkInterface::allInterfaces()) {
    if (!isUsableInterface(iface)) {
      continue;
    }
    const QString ip = primaryIpv4(iface);
    if (!ip.isEmpty()) {
      return ip;
    }
  }
  return {};
}

QString interfaceLabel(const NetworkInterfaceEntry &entry)
{
  if (!entry.displayName.isEmpty() && entry.displayName != entry.systemName) {
    return QStringLiteral("%1 (%2) — %3").arg(entry.displayName, entry.systemName, entry.ipv4);
  }
  return QStringLiteral("%1 — %2").arg(entry.systemName, entry.ipv4);
}

} // namespace itl::NetworkUtils
