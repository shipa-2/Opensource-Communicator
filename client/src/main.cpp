#include "ui/MainWindow.h"

#include "calls/CallManager.h"
#include "protocol/CommunicatorClient.h"

#include <QApplication>
#include <QIcon>
#include "ui/NativeScrollBars.h"
#include "ui/ThemeHelper.h"

int main(int argc, char *argv[])
{
  QApplication app(argc, argv);
  QApplication::setApplicationName(QStringLiteral("opensource-communicator"));
  QApplication::setOrganizationName(QStringLiteral("opensource-communicator"));
  QApplication::setApplicationVersion(QStringLiteral("0.1.0"));
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
