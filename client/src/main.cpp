#include "ui/MainWindow.h"

#include "calls/CallManager.h"
#include "protocol/CommunicatorClient.h"

#include <QApplication>
#include <QIcon>
#include <QLoggingCategory>
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

  itl::NativeScrollBarHelper nativeScrollBars(&app);
  itl::ThemeWatcher themeWatcher(&app);

  itl::CommunicatorClient client;
  itl::CallManager calls(client.api(), &client.appSettings());

  MainWindow window(&client, &calls);
  window.show();

  return app.exec();
}
