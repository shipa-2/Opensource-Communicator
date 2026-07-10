#include "ThemeHelper.h"

#include "MainWindow.h"
#include "StyleHelper.h"

#include <QApplication>
#include <QEvent>
#include <QKeyEvent>
#include <QStyleHints>
#include <QTimer>
#include <QWindow>
#include <QWidget>

namespace itl {

namespace {

void stripForbiddenWindowState(QWidget *widget)
{
  if (!widget || !widget->isWindow()) {
    return;
  }

  Qt::WindowStates state = widget->windowState();
  Qt::WindowStates forbidden = Qt::WindowFullScreen;
  if (widget->property("itlNoMaximize").toBool()) {
    forbidden |= Qt::WindowMaximized;
  }
  if (!(state & forbidden)) {
    return;
  }

  state &= ~forbidden;
  widget->setWindowState(state ? state : Qt::WindowNoState);
}

void attachWindowStateGuard(QWidget *widget)
{
  if (!widget || widget->property("itlNoFullscreenHook").toBool()) {
    return;
  }
  widget->setProperty("itlNoFullscreenHook", true);

  const auto hookWindow = [widget]() {
    if (QWindow *window = widget->windowHandle()) {
      QObject::connect(window, &QWindow::windowStateChanged, widget, [widget](Qt::WindowState) {
        stripForbiddenWindowState(widget);
      }, Qt::UniqueConnection);
      stripForbiddenWindowState(widget);
    }
  };

  hookWindow();
  QTimer::singleShot(0, widget, hookWindow);
  widget->setProperty("itlNoFullscreenHook", true);
}

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

void preventFullscreen(QWidget *widget)
{
  if (!widget) {
    return;
  }
  attachWindowStateGuard(widget);
  stripForbiddenWindowState(widget);
}

ThemeWatcher::ThemeWatcher(QObject *parent)
    : QObject(parent)
{
  qApp->installEventFilter(this);
  connectStyleHints();
}

void ThemeWatcher::connectStyleHints()
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
  if (QStyleHints *hints = QApplication::styleHints()) {
    connect(hints, &QStyleHints::colorSchemeChanged, this, []() { refreshApplicationTheme(); });
  }
#endif
}

bool ThemeWatcher::eventFilter(QObject *watched, QEvent *event)
{
  if (watched == qApp
      && (event->type() == QEvent::ApplicationPaletteChange || event->type() == QEvent::ThemeChange)) {
    refreshApplicationTheme();
  }

  return QObject::eventFilter(watched, event);
}

NoFullscreenGuard::NoFullscreenGuard(QObject *parent)
    : QObject(parent)
{
  qApp->installEventFilter(this);
}

bool NoFullscreenGuard::eventFilter(QObject *watched, QEvent *event)
{
  if (event->type() == QEvent::KeyPress) {
    const auto *keyEvent = static_cast<const QKeyEvent *>(event);
    if (keyEvent->key() == Qt::Key_F11 && !keyEvent->isAutoRepeat()) {
      return true;
    }
  }

  if (event->type() == QEvent::Show || event->type() == QEvent::WindowStateChange) {
    if (auto *widget = qobject_cast<QWidget *>(watched)) {
      if (widget->isWindow()) {
        attachWindowStateGuard(widget);
        stripForbiddenWindowState(widget);
        if (event->type() == QEvent::Show) {
          QTimer::singleShot(0, widget, [widget]() { stripForbiddenWindowState(widget); });
        }
      }
    }
  }

  return QObject::eventFilter(watched, event);
}

} // namespace itl
