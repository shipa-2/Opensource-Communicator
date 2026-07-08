#pragma once

#include <QString>

class QFormLayout;
class QHBoxLayout;
class QPushButton;
class QWidget;

namespace itl {

QString dialogStyleSheet();
void applyDialogStyle(QWidget *widget);
void applyFormDialogStyle(QWidget *widget);
void applyNativeButtons(QWidget *root);
QFormLayout *createDialogForm();
QHBoxLayout *createDialogButtonRow(QPushButton **cancelBtn, QPushButton **acceptBtn, const QString &acceptText);

} // namespace itl
