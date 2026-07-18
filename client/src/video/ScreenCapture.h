#pragma once

#include <QObject>
#include <QImage>
#include <QtGlobal>

class QMediaCaptureSession;
class QScreen;
class QScreenCapture;
class QTimer;
class QVideoFrame;
class QVideoSink;

namespace itl {

class ScreenCapture : public QObject {
    Q_OBJECT

public:
    explicit ScreenCapture(QObject *parent = nullptr);
    ~ScreenCapture() override;

    bool start(int width = 1280, int height = 720, int fps = 10,
               const QString &screenName = {});
    void stop();
    bool isRunning() const { return m_running; }

signals:
    void frameReady(const QImage &frame);
    void error(const QString &message);

private slots:
    void onVideoFrameChanged(const QVideoFrame &frame);

private:
    bool m_running = false;
    QScreenCapture *m_screenCapture = nullptr;
    QMediaCaptureSession *m_captureSession = nullptr;
    QVideoSink *m_videoSink = nullptr;
    QScreen *m_screen = nullptr;
    QTimer *m_fallbackTimer = nullptr;
    int m_width = 1280;
    int m_height = 720;
};

} // namespace itl
