#pragma once

#include <QObject>

namespace itl {

void refreshApplicationTheme();

class ThemeWatcher : public QObject {
    Q_OBJECT

public:
    explicit ThemeWatcher(QObject *parent = nullptr);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void connectStyleHints();
};

} // namespace itl
