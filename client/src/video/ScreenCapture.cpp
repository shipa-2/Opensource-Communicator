#include "ScreenCapture.h"

#include <QGuiApplication>
#include <QMediaCaptureSession>
#include <QScreen>
#include <QScreenCapture>
#include <QPainter>
#include <QVideoFrame>
#include <QVideoSink>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(lcScreenCap, "itl.video.screencap")

namespace itl {

ScreenCapture::ScreenCapture(QObject *parent)
    : QObject(parent)
{
}

ScreenCapture::~ScreenCapture()
{
    stop();
}

bool ScreenCapture::start(int width, int height, int fps)
{
    if (m_running) {
        return true;
    }

    QScreen *screen = QGuiApplication::primaryScreen();
    if (!screen) {
        qCWarning(lcScreenCap) << "No screen available for capture";
        return false;
    }

    m_width = qMax(2, width & ~1);
    m_height = qMax(2, height & ~1);
    m_screenCapture = new QScreenCapture(this);
    m_captureSession = new QMediaCaptureSession(this);
    m_videoSink = new QVideoSink(this);
    m_screenCapture->setScreen(screen);
    m_captureSession->setScreenCapture(m_screenCapture);
    m_captureSession->setVideoSink(m_videoSink);
    connect(m_videoSink, &QVideoSink::videoFrameChanged,
            this, &ScreenCapture::onVideoFrameChanged);
    connect(m_screenCapture, &QScreenCapture::errorOccurred, this,
            [this](QScreenCapture::Error, const QString &message) {
                qCWarning(lcScreenCap) << "Screen capture error:" << message;
                emit error(message);
            }, Qt::QueuedConnection);

    m_running = true;
    m_screenCapture->start();

    qCInfo(lcScreenCap) << "Screen capture requested:" << screen->name()
                        << m_width << "x" << m_height << "target" << fps << "fps";
    return true;
}

void ScreenCapture::stop()
{
    if (!m_running) {
        return;
    }

    if (m_screenCapture) {
        m_screenCapture->stop();
    }
    if (m_captureSession) {
        m_captureSession->setVideoSink(nullptr);
        m_captureSession->setScreenCapture(nullptr);
    }
    delete m_videoSink;
    m_videoSink = nullptr;
    delete m_captureSession;
    m_captureSession = nullptr;
    delete m_screenCapture;
    m_screenCapture = nullptr;
    m_running = false;
    qCInfo(lcScreenCap) << "Screen capture stopped";
}

void ScreenCapture::onVideoFrameChanged(const QVideoFrame &frame)
{
    if (!m_running || !frame.isValid()) {
        return;
    }
    QImage image = frame.toImage();
    if (image.isNull()) {
        return;
    }
    image = image.scaled(m_width, m_height, Qt::KeepAspectRatio,
                         Qt::SmoothTransformation);
    if (image.width() != m_width || image.height() != m_height) {
        QImage canvas(m_width, m_height, QImage::Format_RGB888);
        canvas.fill(Qt::black);
        const int x = (m_width - image.width()) / 2;
        const int y = (m_height - image.height()) / 2;
        QPainter painter(&canvas);
        painter.drawImage(x, y, image);
        image = canvas;
    }
    emit frameReady(image);
}

} // namespace itl
