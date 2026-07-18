#include "ScreenCapture.h"

#include <QGuiApplication>
#include <QMediaCaptureSession>
#include <QPainter>
#include <QPixmap>
#include <QScreen>
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
#include <QScreenCapture>
#endif
#include <QTimer>
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

bool ScreenCapture::start(int width, int height, int fps, const QString &screenName)
{
    if (m_running) {
        return true;
    }

    m_screen = nullptr;
    if (!screenName.isEmpty()) {
        for (QScreen *screen : QGuiApplication::screens()) {
            if (screen && screen->name() == screenName) {
                m_screen = screen;
                break;
            }
        }
    }
    if (!m_screen) {
        m_screen = QGuiApplication::primaryScreen();
    }
    if (!m_screen) {
        qCWarning(lcScreenCap) << "No screen available for capture";
        return false;
    }

    m_width = qMax(2, width & ~1);
    m_height = qMax(2, height & ~1);
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    m_screenCapture = new QScreenCapture(this);
    m_captureSession = new QMediaCaptureSession(this);
    m_videoSink = new QVideoSink(this);
    m_screenCapture->setScreen(m_screen);
    m_captureSession->setScreenCapture(m_screenCapture);
    m_captureSession->setVideoSink(m_videoSink);
    connect(m_videoSink, &QVideoSink::videoFrameChanged,
            this, &ScreenCapture::onVideoFrameChanged);
    connect(m_screenCapture, &QScreenCapture::errorOccurred, this,
            [this](QScreenCapture::Error, const QString &message) {
                qCWarning(lcScreenCap) << "Screen capture error:" << message;
                emit error(message);
            }, Qt::QueuedConnection);
#else
    // QScreenCapture was introduced in Qt 6.5. Ubuntu 24.04 CI ships Qt 6.4,
    // so keep an X11-compatible fallback. On modern Wayland installations the
    // Qt >= 6.5 path above uses PipeWire/xdg-desktop-portal instead.
    m_fallbackTimer = new QTimer(this);
    connect(m_fallbackTimer, &QTimer::timeout, this, [this]() {
        if (!m_running || !m_screen) {
            return;
        }
        const QPixmap pixmap = m_screen->grabWindow(0);
        if (pixmap.isNull()) {
            return;
        }
        QImage image = pixmap.toImage();
        if (!image.isNull()) {
            image = image.scaled(m_width, m_height, Qt::KeepAspectRatio,
                                 Qt::SmoothTransformation);
            emit frameReady(image);
        }
    });
#endif

    m_running = true;
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    m_screenCapture->start();
#else
    m_fallbackTimer->start(qMax(1, 1000 / qMax(1, fps)));
#endif

    qCInfo(lcScreenCap) << "Screen capture requested:" << m_screen->name()
                        << m_width << "x" << m_height << "target" << fps << "fps";
    return true;
}

void ScreenCapture::stop()
{
    if (!m_running) {
        return;
    }

#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
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
#else
    if (m_fallbackTimer) {
        m_fallbackTimer->stop();
        delete m_fallbackTimer;
        m_fallbackTimer = nullptr;
    }
#endif
    m_screen = nullptr;
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
