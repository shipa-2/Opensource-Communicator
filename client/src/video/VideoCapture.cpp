#include "VideoCapture.h"

#include <QCamera>
#include <QCameraDevice>
#include <QCameraFormat>
#include <QMediaCaptureSession>
#include <QVideoSink>
#include <QVideoFrame>
#include <QLoggingCategory>
#include <QMediaDevices>
#include <limits>

Q_LOGGING_CATEGORY(lcVideoCapture, "itl.video.capture")

namespace itl {

VideoCapture::VideoCapture(QObject *parent)
    : QObject(parent)
{
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

    m_width = qMax(2, width & ~1);
    m_height = qMax(2, height & ~1);

    const QCameraDevice device = QMediaDevices::defaultVideoInput().isNull()
                                     ? cameras.constFirst()
                                     : QMediaDevices::defaultVideoInput();
    m_camera = new QCamera(device, this);
    m_captureSession = new QMediaCaptureSession(this);
    m_videoSink = new QVideoSink(this);

    QCameraFormat bestFormat;
    qint64 bestScore = std::numeric_limits<qint64>::max();
    for (const QCameraFormat &format : device.videoFormats()) {
        const QSize resolution = format.resolution();
        const qint64 sizeDelta = qAbs(resolution.width() - m_width)
                                 + qAbs(resolution.height() - m_height);
        const qint64 fpsPenalty = format.maxFrameRate() + 0.01 < fps ? 100000 : 0;
        const qint64 score = fpsPenalty + sizeDelta;
        if (score < bestScore) {
            bestScore = score;
            bestFormat = format;
        }
    }
    if (!bestFormat.isNull()) {
        m_camera->setCameraFormat(bestFormat);
    }

    m_captureSession->setCamera(m_camera);
    m_captureSession->setVideoSink(m_videoSink);
    connect(m_videoSink, &QVideoSink::videoFrameChanged,
            this, &VideoCapture::onVideoFrameChanged);
    connect(m_camera, &QCamera::errorOccurred, this,
            [this](QCamera::Error, const QString &message) {
                qCWarning(lcVideoCapture) << "Camera error:" << message;
                emit error(message);
            }, Qt::QueuedConnection);

    m_running = true;
    m_camera->start();

    qCInfo(lcVideoCapture) << "Video capture started:" << device.description()
                           << m_width << "x" << m_height << "target" << fps << "fps";
    return true;
}

void VideoCapture::stop()
{
    if (!m_running) {
        return;
    }

    if (m_camera) {
        m_camera->stop();
    }
    if (m_captureSession) {
        m_captureSession->setVideoSink(nullptr);
        m_captureSession->setCamera(nullptr);
    }
    delete m_videoSink;
    m_videoSink = nullptr;
    delete m_captureSession;
    m_captureSession = nullptr;
    delete m_camera;
    m_camera = nullptr;
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

void VideoCapture::onVideoFrameChanged(const QVideoFrame &frame)
{
    if (!m_running || !frame.isValid()) {
        return;
    }
    QImage image = frame.toImage();
    if (image.isNull()) {
        return;
    }
    if (image.width() != m_width || image.height() != m_height) {
        image = image.scaled(m_width, m_height, Qt::KeepAspectRatioByExpanding,
                             Qt::FastTransformation);
        const int x = qMax(0, (image.width() - m_width) / 2);
        const int y = qMax(0, (image.height() - m_height) / 2);
        image = image.copy(x, y, m_width, m_height);
    }
    emit frameReady(image);
}

} // namespace itl
