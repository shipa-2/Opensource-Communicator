#pragma once

#include <QStringList>
#include <functional>

class QObject;

namespace itl {

class AppInstance {
public:
    static QString serverName();
    static QStringList extractTelUrls(const QStringList &arguments);
    static bool sendToRunningInstance(const QStringList &arguments);
    static bool startServer(QObject *owner, const std::function<void(const QStringList &)> &handler);
};

} // namespace itl
