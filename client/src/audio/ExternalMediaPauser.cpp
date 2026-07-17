#include "ExternalMediaPauser.h"

#include <QLoggingCategory>

#if defined(Q_OS_LINUX) || defined(Q_OS_FREEBSD)
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusInterface>
#include <QDBusMessage>
#include <QDBusReply>
#include <QDBusVariant>
#endif

Q_LOGGING_CATEGORY(lcMediaPause, "itl.media")

namespace itl {

namespace {

#if defined(Q_OS_LINUX) || defined(Q_OS_FREEBSD)
QString playbackStatus(const QString &service)
{
  QDBusInterface props(service, QStringLiteral("/org/mpris/MediaPlayer2"),
                       QStringLiteral("org.freedesktop.DBus.Properties"),
                       QDBusConnection::sessionBus());
  if (!props.isValid()) {
    return {};
  }

  const QDBusMessage reply = props.call(
      QStringLiteral("Get"), QStringLiteral("org.mpris.MediaPlayer2"), QStringLiteral("PlaybackStatus"));
  if (reply.type() == QDBusMessage::ErrorMessage || reply.arguments().isEmpty()) {
    return {};
  }
  return reply.arguments().constFirst().value<QDBusVariant>().variant().toString();
}

void callPlayerMethod(const QString &service, const char *method)
{
  QDBusInterface player(service, QStringLiteral("/org/mpris/MediaPlayer2"),
                        QStringLiteral("org.mpris.MediaPlayer2.Player"), QDBusConnection::sessionBus());
  if (!player.isValid()) {
    return;
  }
  player.call(method);
}
#endif

} // namespace

QStringList ExternalMediaPauser::mprisServiceNames()
{
#if defined(Q_OS_LINUX) || defined(Q_OS_FREEBSD)
  QDBusConnection bus = QDBusConnection::sessionBus();
  if (!bus.isConnected()) {
    return {};
  }

  QDBusReply<QStringList> reply = bus.interface()->registeredServiceNames();
  if (!reply.isValid()) {
    return {};
  }

  QStringList services;
  for (const QString &name : reply.value()) {
    if (name.startsWith(QStringLiteral("org.mpris.MediaPlayer2."))) {
      services.append(name);
    }
  }
  return services;
#else
  return {};
#endif
}

void ExternalMediaPauser::pause()
{
  if (m_pauseDepth++ > 0) {
    return;
  }

#if defined(Q_OS_LINUX) || defined(Q_OS_FREEBSD)
  m_playingServices.clear();
  for (const QString &service : mprisServiceNames()) {
    const QString status = playbackStatus(service);
    if (status == QStringLiteral("Playing")) {
      m_playingServices.append(service);
    }
    // Pause every MPRIS player: browsers often omit or delay PlaybackStatus.
    callPlayerMethod(service, "Pause");
  }
  if (!m_playingServices.isEmpty()) {
    qCInfo(lcMediaPause) << "Paused MPRIS players:" << m_playingServices;
  }
#endif
}

void ExternalMediaPauser::resume()
{
  if (m_pauseDepth <= 0) {
    m_pauseDepth = 0;
    return;
  }
  if (--m_pauseDepth > 0) {
    return;
  }

#if defined(Q_OS_LINUX) || defined(Q_OS_FREEBSD)
  for (const QString &service : m_playingServices) {
    callPlayerMethod(service, "Play");
  }
  if (!m_playingServices.isEmpty()) {
    qCInfo(lcMediaPause) << "Resumed MPRIS players:" << m_playingServices;
  }
  m_playingServices.clear();
#endif
}

} // namespace itl
