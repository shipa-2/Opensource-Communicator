#pragma once

#include <QColor>
#include <QString>

class QFormLayout;
class QHBoxLayout;
class QMenu;
class QPushButton;
class QWidget;

namespace itl {

QString dialogStyleSheet();
void applyDialogStyle(QWidget *widget);
void refreshDialogStyle(QWidget *widget);
void applyFormDialogStyle(QWidget *widget);
void applyNativeButtons(QWidget *root);
QFormLayout *createDialogForm();
QHBoxLayout *createDialogButtonRow(QPushButton **cancelBtn, QPushButton **acceptBtn, const QString &acceptText);

QString colorToRgba(const QColor &color, int alpha);

/**
 * Tooltips and context menus always use native opaque system chrome.
 * Wallpaper panel opacity must not affect them (and must not restyle them).
 */
void setPopupChromeUiOpacity(int uiOpacityPercent);
int popupChromeOpacityPercent();
int popupChromeAlpha();

void applyApplicationTooltipStyle();
void applyPopupMenuStyle(QMenu *menu);

} // namespace itl
