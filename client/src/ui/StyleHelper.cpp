#include "StyleHelper.h"

#include "NativeScrollBars.h"

#include <QApplication>
#include <QFile>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QObject>
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
    button->setAttribute(Qt::WA_StyleSheetTarget, false);
    button->setStyleSheet({});
    button->setStyle(QApplication::style());
  }
}

void applyDialogStyle(QWidget *widget)
{
  if (!widget) {
    return;
  }

  const QString sheet = dialogStyleSheet();
  if (!sheet.isEmpty()) {
    widget->setStyleSheet(sheet);
  }
  applyNativeScrollBars(widget);
  applyNativeButtons(widget);
  widget->setProperty("itlDialogStyle", true);
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
