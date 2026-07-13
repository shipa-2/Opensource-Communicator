#pragma once

#include <QList>
#include <QString>

namespace itl::NetworkUtils {

struct NetworkInterfaceEntry {
    QString systemName;
    QString displayName;
    QString ipv4;
};

QList<NetworkInterfaceEntry> ipv4Interfaces();
QString ipv4ForBinding(const QString &interfaceSystemName);
QString interfaceLabel(const NetworkInterfaceEntry &entry);

} // namespace itl::NetworkUtils
