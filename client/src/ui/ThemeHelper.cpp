#include "ThemeHelper.h"

#include "MainWindow.h"
#include "StyleHelper.h"

#include <QApplication>
#include <QEvent>
#include <QStyleHints>
#include <QTimer>
#include <QWidget>

namespace itl {

namespace {

void refreshApplicationThemeNow()
{
  for (QWidget *widget : QApplication::allWidgets()) {
    if (widget->property("itlDialogStyle").toBool()) {
      applyDialogStyle(widget);
    }
  }

  for (QWidget *widget : QApplication::topLevelWidgets()) {
    if (auto *mainWindow = qobject_cast<MainWindow *>(widget)) {
      mainWindow->refreshTheme();
    }
  }
}

} // namespace

void refreshApplicationTheme()
{
  QTimer::singleShot(0, qApp, []() { refreshApplicationThemeNow(); });
}

ThemeWatcher::ThemeWatcher(QObject *parent)
    : QObject(parent)
{
  qApp->installEventFilter(this);
  connectStyleHints();
}

void ThemeWatcher::connectStyleHints()
{
  if (QStyleHints *hints = QApplication::styleHints()) {
    connect(hints, &QStyleHints::colorSchemeChanged, this, []() { refreshApplicationTheme(); });
  }
}

bool ThemeWatcher::eventFilter(QObject *watched, QEvent *event)
{
  if (watched == qApp
      && (event->type() == QEvent::ApplicationPaletteChange || event->type() == QEvent::ThemeChange)) {
    refreshApplicationTheme();
  }

  return QObject::eventFilter(watched, event);
}

} // namespace itl
