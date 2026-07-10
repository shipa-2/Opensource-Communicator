#include "NativeScrollBars.h"

#include <QAbstractScrollArea>
#include <QApplication>
#include <QChildEvent>
#include <QEvent>
#include <QPointer>
#include <QScrollBar>
#include <QShowEvent>
#include <QTimer>
#include <QWidget>

namespace itl {

void applyNativeScrollBarStyle(QScrollBar *scrollBar)
{
  if (!scrollBar) {
    return;
  }

  scrollBar->setAttribute(Qt::WA_StyleSheetTarget, false);
  scrollBar->setStyleSheet({});
  scrollBar->setStyle(nullptr);
  scrollBar->update();
}

void applyNativeScrollBars(QWidget *root)
{
  if (!root) {
    return;
  }

  // Defer: re-styling scroll bars during PaletteChange/ThemeChange can re-enter
  // QAbstractScrollArea::event and crash in QScrollBar::sizeHint().
  QPointer<QWidget> guard(root);
  QTimer::singleShot(0, root, [guard]() {
    if (!guard) {
      return;
    }

    const auto scrollBars = guard->findChildren<QScrollBar *>();
    for (QScrollBar *scrollBar : scrollBars) {
      QPointer<QScrollBar> bar(scrollBar);
      if (bar) {
        applyNativeScrollBarStyle(bar);
      }
    }

    const auto scrollAreas = guard->findChildren<QAbstractScrollArea *>();
    for (QAbstractScrollArea *area : scrollAreas) {
      QPointer<QAbstractScrollArea> areaGuard(area);
      if (areaGuard) {
        applyNativeScrollBarStyle(areaGuard->verticalScrollBar());
        applyNativeScrollBarStyle(areaGuard->horizontalScrollBar());
      }
    }
  });
}

NativeScrollBarHelper::NativeScrollBarHelper(QObject *parent)
    : QObject(parent)
{
  qApp->installEventFilter(this);
}

bool NativeScrollBarHelper::eventFilter(QObject *watched, QEvent *event)
{
  if (event->type() == QEvent::Show) {
    if (auto *widget = qobject_cast<QWidget *>(watched)) {
      applyNativeScrollBars(widget);
    }
  } else if (event->type() == QEvent::ChildAdded) {
    auto *childEvent = static_cast<QChildEvent *>(event);
    if (auto *child = qobject_cast<QWidget *>(childEvent->child())) {
      QTimer::singleShot(0, child, [child]() { applyNativeScrollBars(child); });
    }
  }

  return QObject::eventFilter(watched, event);
}

} // namespace itl
