#pragma once

#include <QObject>
#include <QImage>
#include <QTimer>
#include <QScreen>

namespace itl {

class ScreenCapture : public QObject {
    Q_OBJECT

public:
    explicit ScreenCapture(QObject *parent = nullptr);
    ~ScreenCapture() override;

    bool start(int fps = 15);
    void stop();
    bool isRunning() const { return m_running; }

signals:
    void frameReady(const QImage &frame);

private slots:
    void captureFrame();

private:
    bool m_running = false;
    QTimer m_timer;
    QScreen *m_screen = nullptr;
};

} // namespace itl
