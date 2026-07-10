#pragma once

#include <QObject>

namespace itl {

void refreshApplicationTheme();

void preventFullscreen(QWidget *widget);

class ThemeWatcher : public QObject {
    Q_OBJECT

public:
    explicit ThemeWatcher(QObject *parent = nullptr);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void connectStyleHints();
};

class NoFullscreenGuard : public QObject {
    Q_OBJECT

public:
    explicit NoFullscreenGuard(QObject *parent = nullptr);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
};

} // namespace itl
