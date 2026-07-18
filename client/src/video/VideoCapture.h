#pragma once

#include <QObject>
#include <QImage>
#include <QStringList>

class QCamera;
class QMediaCaptureSession;
class QVideoFrame;
class QVideoSink;

namespace itl {

class VideoCapture : public QObject {
    Q_OBJECT

public:
    explicit VideoCapture(QObject *parent = nullptr);
    ~VideoCapture() override;

    bool start(int width = 640, int height = 480, int fps = 30);
    void stop();
    bool isRunning() const { return m_running; }
    QStringList availableDevices() const;

signals:
    void frameReady(const QImage &frame);
    void error(const QString &message);

private slots:
    void onVideoFrameChanged(const QVideoFrame &frame);

private:
    bool m_running = false;
    QCamera *m_camera = nullptr;
    QMediaCaptureSession *m_captureSession = nullptr;
    QVideoSink *m_videoSink = nullptr;
    int m_width = 640;
    int m_height = 360;
};

} // namespace itl
