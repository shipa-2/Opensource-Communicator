#include "NativeScrollBars.h"

#include <QAbstractScrollArea>
#include <QApplication>
#include <QChildEvent>
#include <QEvent>
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
  scrollBar->setStyle(QApplication::style());
}

void applyNativeScrollBars(QWidget *root)
{
  if (!root) {
    return;
  }

  const auto scrollBars = root->findChildren<QScrollBar *>();
  for (QScrollBar *scrollBar : scrollBars) {
    applyNativeScrollBarStyle(scrollBar);
  }

  const auto scrollAreas = root->findChildren<QAbstractScrollArea *>();
  for (QAbstractScrollArea *area : scrollAreas) {
    applyNativeScrollBarStyle(area->verticalScrollBar());
    applyNativeScrollBarStyle(area->horizontalScrollBar());
  }
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
