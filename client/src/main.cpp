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
#ifdef OSC_DEBUG_BUILD
  qSetMessagePattern(QStringLiteral("[%{time HH:mm:ss.zzz}] %{type} %{category}: %{message}"));
  QLoggingCategory::setFilterRules(QStringLiteral(
      "itl.*.debug=true\n"
      "itl.*.info=true\n"
      "itl.*.warning=true\n"
      "itl.*.critical=true"));
#else
  QLoggingCategory::setFilterRules(QStringLiteral(
      "*.debug=false\n"
      "*.info=false\n"
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
  QApplication::setApplicationVersion(QStringLiteral("0.2.0"));
  QApplication::setDesktopFileName(QStringLiteral("opensource-communicator"));
  QApplication::setWindowIcon(QIcon(QStringLiteral(":/logo.png")));

  const QStringList startupArgs = QApplication::arguments().mid(1);
  if (itl::AppInstance::sendToRunningInstance(startupArgs)) {
    return 0;
  }

  itl::NativeScrollBarHelper nativeScrollBars(&app);
  itl::ThemeWatcher themeWatcher(&app);

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

  const QStringList startupTelUrls = itl::AppInstance::extractTelUrls(startupArgs);
  if (!startupTelUrls.isEmpty()) {
    QTimer::singleShot(0, &window, [&window, startupTelUrls]() {
      window.handleIncomingTelUri(startupTelUrls.first());
    });
  }

  return app.exec();
}
