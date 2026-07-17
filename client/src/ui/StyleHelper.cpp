#include "StyleHelper.h"

#include "NativeScrollBars.h"

#include <QApplication>
#include <QColor>
#include <QFile>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QMenu>
#include <QObject>
#include <QPalette>
#include <QPlainTextEdit>
#include <QPointer>
#include <QPushButton>
#include <QStyle>
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

QString colorToRgba(const QColor &color, int alpha)
{
  return QStringLiteral("rgba(%1, %2, %3, %4)")
      .arg(color.red())
      .arg(color.green())
      .arg(color.blue())
      .arg(alpha);
}

namespace {

} // namespace

void setPopupChromeUiOpacity(int uiOpacityPercent)
{
  Q_UNUSED(uiOpacityPercent);
  applyApplicationTooltipStyle();
}

int popupChromeOpacityPercent()
{
  return 100;
}

int popupChromeAlpha()
{
  return 255;
}

void applyApplicationTooltipStyle()
{
  QApplication *app = qApp;
  if (!app) {
    return;
  }

  // Native system tooltips: no app-wide QToolTip QSS (that overrides Breeze/Fusion
  // and previously fought with wallpaper translucency).
  if (!app->styleSheet().isEmpty()) {
    app->setStyleSheet({});
  }
}

void applyPopupMenuStyle(QMenu *menu)
{
  if (!menu) {
    return;
  }

  // Opt out of ancestor QSS. Dim-lists used to set unscoped "background: transparent"
  // on the viewport, which cascaded into child QMenus and made them fully see-through.
  menu->setAttribute(Qt::WA_StyleSheetTarget, false);
  menu->setStyleSheet({});
  menu->setAttribute(Qt::WA_TranslucentBackground, false);
  menu->setAutoFillBackground(true);
  menu->setPalette(QApplication::palette(menu));
  if (QStyle *appStyle = QApplication::style()) {
    menu->setStyle(appStyle);
  }
  menu->style()->unpolish(menu);
  menu->style()->polish(menu);
}

} // namespace itl
