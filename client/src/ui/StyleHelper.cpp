#include "StyleHelper.h"

#include "NativeScrollBars.h"

#include <QApplication>
#include <QFile>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QObject>
#include <QPlainTextEdit>
#include <QPointer>
#include <QPushButton>
#include <QWidget>

namespace itl {

QString dialogStyleSheet()
{
  static QString cached;
  if (!cached.isEmpty()) {
    return cached;
  }

  QFile styleFile(QStringLiteral(":/communicator-dialogs.qss"));
  if (styleFile.open(QIODevice::ReadOnly)) {
    cached = QString::fromUtf8(styleFile.readAll());
  }
  return cached;
}

void applyNativeButtons(QWidget *root)
{
  if (!root) {
    return;
  }

  const auto buttons = root->findChildren<QPushButton *>();
  for (QPushButton *button : buttons) {
    QPointer<QPushButton> guard(button);
    if (guard) {
      guard->setAttribute(Qt::WA_StyleSheetTarget, false);
      guard->setStyleSheet({});
      // Inherit the application style; pinning QApplication::style() breaks when
      // KDEPlasmaPlatformTheme6 calls QApplication::setStyle() on theme change.
      guard->setStyle(nullptr);
      guard->update();
    }
  }
}

void applyDialogStyle(QWidget *widget)
{
  if (!widget) {
    return;
  }

  const QString sheet = dialogStyleSheet();
  if (!sheet.isEmpty() && widget->styleSheet() != sheet) {
    widget->setStyleSheet(sheet);
  }
  applyNativeScrollBars(widget);
  applyNativeButtons(widget);
  widget->setProperty("itlDialogStyle", true);
}

void refreshDialogStyle(QWidget *widget)
{
  if (!widget) {
    return;
  }

  // QSS uses palette(); the sheet text is unchanged on OS theme switch, so
  // re-apply forcibly or colors stay on the old palette.
  const QString sheet = dialogStyleSheet();
  widget->setStyleSheet(QString());
  if (!sheet.isEmpty()) {
    widget->setStyleSheet(sheet);
  }

  const auto polish = [](QWidget *child) {
    if (!child) {
      return;
    }
    child->setStyleSheet({});
    child->setPalette(QApplication::palette(child));
    child->update();
  };

  polish(widget);
  for (QPlainTextEdit *edit : widget->findChildren<QPlainTextEdit *>()) {
    if (!edit->testAttribute(Qt::WA_StyleSheetTarget)) {
      continue;
    }
    polish(edit);
    if (QWidget *viewport = edit->viewport()) {
      polish(viewport);
    }
  }
  for (QLineEdit *edit : widget->findChildren<QLineEdit *>()) {
    polish(edit);
  }

  applyNativeScrollBars(widget);
  applyNativeButtons(widget);
  widget->update();
}

void applyFormDialogStyle(QWidget *widget)
{
  if (!widget) {
    return;
  }

  applyNativeScrollBars(widget);
}

QFormLayout *createDialogForm()
{
  auto *form = new QFormLayout;
  form->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
  form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
  form->setFormAlignment(Qt::AlignLeft | Qt::AlignTop);
  form->setHorizontalSpacing(12);
  form->setVerticalSpacing(8);
  return form;
}

QHBoxLayout *createDialogButtonRow(QPushButton **cancelBtn, QPushButton **acceptBtn, const QString &acceptText)
{
  auto *buttons = new QHBoxLayout;
  buttons->setContentsMargins(0, 4, 0, 0);
  buttons->addStretch();
  *cancelBtn = new QPushButton(QObject::tr("Отмена"));
  *acceptBtn = new QPushButton(acceptText);
  (*acceptBtn)->setDefault(true);
  buttons->addWidget(*cancelBtn);
  buttons->addWidget(*acceptBtn);
  return buttons;
}

} // namespace itl
