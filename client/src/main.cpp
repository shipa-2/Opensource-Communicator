#include "ui/MainWindow.h"
#include "ui/AppInstance.h"

#include "calls/CallManager.h"
#include "protocol/CommunicatorClient.h"

#include <QApplication>
#include <QIcon>
#include <QLoggingCategory>
#include <QTimer>
#include "ui/NativeScrollBars.h"
#include "ui/ThemeHelper.h"

namespace {

void configureLogging()
{
  qSetMessagePattern(QStringLiteral("[%{time HH:mm:ss.zzz}] %{type} %{category}: %{message}"));
#ifdef OSC_DEBUG_BUILD
  QLoggingCategory::setFilterRules(QStringLiteral(
      "itl.*.debug=true\n"
      "itl.*.info=true\n"
      "itl.*.warning=true\n"
      "itl.*.critical=true"));
#else
  // Release: keep call/signaling info visible in journalctl for troubleshooting.
  QLoggingCategory::setFilterRules(QStringLiteral(
      "*.debug=false\n"
      "itl.call.info=true\n"
      "itl.ws.info=true\n"
      "itl.api.info=true\n"
      "itl.client.info=true\n"
      "itl.chat.info=true\n"
      "*.warning=true\n"
      "*.critical=true"));
#endif
}

void bringWindowToFront(MainWindow *window)
{
  if (!window) {
    return;
  }
  window->show();
  window->raise();
  window->activateWindow();
}

} // namespace

int main(int argc, char *argv[])
{
  QApplication app(argc, argv);
  configureLogging();
  QApplication::setApplicationName(QStringLiteral("opensource-communicator"));
  QApplication::setOrganizationName(QStringLiteral("opensource-communicator"));
  QApplication::setApplicationVersion(QStringLiteral("0.3.0"));
  QApplication::setDesktopFileName(QStringLiteral("opensource-communicator"));
  QApplication::setWindowIcon(QIcon(QStringLiteral(":/logo.png")));

  const QStringList startupArgs = QApplication::arguments().mid(1);
  if (itl::AppInstance::sendToRunningInstance(startupArgs)) {
    return 0;
  }

  itl::NativeScrollBarHelper nativeScrollBars(&app);
  itl::ThemeWatcher themeWatcher(&app);
  itl::NoFullscreenGuard noFullscreenGuard(&app);

  itl::CommunicatorClient client;
  itl::CallManager calls(client.api(), &client.appSettings());

  MainWindow window(&client, &calls);

  itl::AppInstance::startServer(&app, [&window](const QStringList &arguments) {
    const QStringList telUrls = itl::AppInstance::extractTelUrls(arguments);
    if (telUrls.isEmpty()) {
      QTimer::singleShot(0, &window, [&window]() { bringWindowToFront(&window); });
      return;
    }

    const QString telUrl = telUrls.first();
    QTimer::singleShot(0, &window, [&window, telUrl]() { window.handleIncomingTelUri(telUrl); });
  });

  window.show();
  QTimer::singleShot(0, &window, [&window]() {
    window.setWindowState(Qt::WindowNoState);
    window.resize(390, 620);
  });

  const QStringList startupTelUrls = itl::AppInstance::extractTelUrls(startupArgs);
  if (!startupTelUrls.isEmpty()) {
    QTimer::singleShot(0, &window, [&window, startupTelUrls]() {
      window.handleIncomingTelUri(startupTelUrls.first());
    });
  }

  return app.exec();
}
