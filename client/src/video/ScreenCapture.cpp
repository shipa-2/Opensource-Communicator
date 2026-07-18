#include "ScreenCapture.h"

#include <QGuiApplication>
#include <QScreen>
#include <QPixmap>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(lcScreenCap, "itl.video.screencap")

namespace itl {

ScreenCapture::ScreenCapture(QObject *parent)
    : QObject(parent)
{
    connect(&m_timer, &QTimer::timeout, this, &ScreenCapture::captureFrame);
}

ScreenCapture::~ScreenCapture()
{
    stop();
}

bool ScreenCapture::start(int fps)
{
    if (m_running) {
        return true;
    }

    m_screen = QGuiApplication::primaryScreen();
    if (!m_screen) {
        qCWarning(lcScreenCap) << "No screen available for capture";
        return false;
    }

    m_timer.start(1000 / fps);
    m_running = true;

    qCInfo(lcScreenCap) << "Screen capture started:" << m_screen->size() << "@" << fps << "fps";
    return true;
}

void ScreenCapture::stop()
{
    if (!m_running) {
        return;
    }

    m_timer.stop();
    m_running = false;
    qCInfo(lcScreenCap) << "Screen capture stopped";
}

void ScreenCapture::captureFrame()
{
    if (!m_screen) {
        return;
    }

    QPixmap pixmap = m_screen->grabWindow(0);
    if (pixmap.isNull()) {
        return;
    }

    QImage frame = pixmap.toImage().convertToFormat(QImage::Format_RGB888);
    if (!frame.isNull()) {
        emit frameReady(frame);
    }
}

} // namespace itl
