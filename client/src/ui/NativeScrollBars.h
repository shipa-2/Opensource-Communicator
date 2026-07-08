#pragma once

#include <QObject>

class QScrollBar;
class QWidget;
class QEvent;

namespace itl {

void applyNativeScrollBarStyle(QScrollBar *scrollBar);
void applyNativeScrollBars(QWidget *root);

class NativeScrollBarHelper : public QObject {
    Q_OBJECT

public:
    explicit NativeScrollBarHelper(QObject *parent = nullptr);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
};

} // namespace itl
