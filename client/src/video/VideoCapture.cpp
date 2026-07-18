#include "VideoCapture.h"

#include <QCamera>
#include <QMediaCaptureSession>
#include <QVideoSink>
#include <QVideoFrame>
#include <QLoggingCategory>
#include <QMediaDevices>

Q_LOGGING_CATEGORY(lcVideoCapture, "itl.video.capture")

namespace itl {

VideoCapture::VideoCapture(QObject *parent)
    : QObject(parent)
{
    connect(&m_timer, &QTimer::timeout, this, &VideoCapture::captureFrame);
}

VideoCapture::~VideoCapture()
{
    stop();
}

bool VideoCapture::start(int width, int height, int fps)
{
    if (m_running) {
        return true;
    }

    const auto cameras = QMediaDevices::videoInputs();
    if (cameras.isEmpty()) {
        qCWarning(lcVideoCapture) << "No video input devices found";
        return false;
    }

    qCInfo(lcVideoCapture) << "Available cameras:" << cameras.size();
    for (const QCameraDevice &cam : cameras) {
        qCInfo(lcVideoCapture) << "  -" << cam.description();
    }

    // TODO: use QCamera + QMediaCaptureSession + QVideoSink
    // For now, start a timer for frame capture
    m_timer.start(1000 / fps);
    m_running = true;

    qCInfo(lcVideoCapture) << "Video capture started:" << width << "x" << height << "@" << fps << "fps";
    return true;
}

void VideoCapture::stop()
{
    if (!m_running) {
        return;
    }

    m_timer.stop();
    m_running = false;
    qCInfo(lcVideoCapture) << "Video capture stopped";
}

QStringList VideoCapture::availableDevices() const
{
    QStringList result;
    const auto cameras = QMediaDevices::videoInputs();
    for (const QCameraDevice &cam : cameras) {
        result.append(cam.description());
    }
    return result;
}

void VideoCapture::captureFrame()
{
    // TODO: implement actual frame capture from QCamera
    // For now, emit a placeholder frame
    // The actual implementation will use QMediaCaptureSession + QVideoSink
}

} // namespace itl
