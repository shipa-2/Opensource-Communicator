#include "AppInstance.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QLocalServer>
#include <QLocalSocket>
#include <QCoreApplication>
#include <QUrl>

namespace itl {

QString AppInstance::serverName()
{
  return QStringLiteral("opensource-communicator-v1");
}

QString AppInstance::isolatedServerName()
{
  return serverName() + QLatin1Char('-') + QString::number(QCoreApplication::applicationPid());
}

bool AppInstance::wantsNewInstance(const QStringList &arguments)
{
  for (const QString &argument : arguments) {
    if (argument == QStringLiteral("--newinstance")) {
      return true;
    }
  }
  return false;
}

QStringList AppInstance::stripInstanceFlags(const QStringList &arguments)
{
  QStringList filtered;
  filtered.reserve(arguments.size());
  for (const QString &argument : arguments) {
    if (argument == QStringLiteral("--newinstance")) {
      continue;
    }
    filtered.append(argument);
  }
  return filtered;
}

QStringList AppInstance::extractTelUrls(const QStringList &arguments)
{
  QStringList urls;
  for (const QString &argument : arguments) {
    const QString trimmed = argument.trimmed();
    if (trimmed.isEmpty()) {
      continue;
    }
    if (trimmed.startsWith(QStringLiteral("tel:"), Qt::CaseInsensitive)) {
      urls.append(trimmed);
      continue;
    }
    const QUrl url(trimmed);
    if (url.scheme() == QStringLiteral("tel")) {
      urls.append(trimmed);
    }
  }
  return urls;
}

bool AppInstance::sendToRunningInstance(const QStringList &arguments)
{
  QLocalSocket socket;
  socket.connectToServer(serverName());
  if (!socket.waitForConnected(500)) {
    return false;
  }

  QJsonArray payload;
  for (const QString &argument : arguments) {
    payload.append(argument);
  }

  QByteArray data = QJsonDocument(payload).toJson(QJsonDocument::Compact);
  data.append('\n');
  socket.write(data);
  socket.waitForBytesWritten(1000);
  socket.disconnectFromServer();
  return true;
}

bool AppInstance::startServer(QObject *owner, const std::function<void(const QStringList &)> &handler,
                              bool newInstance)
{
  if (!owner || !handler) {
    return false;
  }

  const QString name = newInstance ? isolatedServerName() : serverName();

  auto *server = new QLocalServer(owner);
  QLocalServer::removeServer(name);
  if (!server->listen(name)) {
    server->deleteLater();
    return false;
  }

  QObject::connect(server, &QLocalServer::newConnection, owner, [server, handler]() {
    QLocalSocket *client = server->nextPendingConnection();
    if (!client) {
      return;
    }

    QObject::connect(client, &QLocalSocket::readyRead, client, [client, handler]() {
      const QByteArray data = client->readAll();
      const QJsonDocument document = QJsonDocument::fromJson(data);
      if (!document.isArray()) {
        client->disconnectFromServer();
        client->deleteLater();
        return;
      }

      QStringList arguments;
      for (const QJsonValue &value : document.array()) {
        arguments.append(value.toString());
      }
      handler(arguments);
      client->disconnectFromServer();
      client->deleteLater();
    });
  });

  return true;
}

} // namespace itl
