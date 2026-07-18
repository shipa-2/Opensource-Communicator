#pragma once

#include <QStringList>
#include <functional>

class QObject;

namespace itl {

class AppInstance {
public:
    static QString serverName();
    static QString isolatedServerName();
    static bool wantsNewInstance(const QStringList &arguments);
    static QStringList stripInstanceFlags(const QStringList &arguments);
    static QStringList extractTelUrls(const QStringList &arguments);
    static bool sendToRunningInstance(const QStringList &arguments);
    static bool startServer(QObject *owner, const std::function<void(const QStringList &)> &handler,
                            bool newInstance = false);
};

} // namespace itl
