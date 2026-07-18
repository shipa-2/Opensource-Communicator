#pragma once

#include <QObject>
#include <QImage>

class QMediaCaptureSession;
class QScreenCapture;
class QVideoFrame;
class QVideoSink;

namespace itl {

class ScreenCapture : public QObject {
    Q_OBJECT

public:
    explicit ScreenCapture(QObject *parent = nullptr);
    ~ScreenCapture() override;

    bool start(int width = 1280, int height = 720, int fps = 10);
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
    int m_width = 1280;
    int m_height = 720;
};

} // namespace itl
