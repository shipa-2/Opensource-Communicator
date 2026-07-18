#include "VideoRenderer.h"

#include <QPainter>
#include <QPaintEvent>

namespace itl {

VideoRenderer::VideoRenderer(QWidget *parent)
    : QWidget(parent)
{
    setMinimumSize(160, 120);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

void VideoRenderer::setPlaceholderText(const QString &text)
{
    m_placeholderText = text;
    update();
}

void VideoRenderer::onFrameReceived(const QImage &frame)
{
    if (frame.isNull()) {
        return;
    }
    m_currentFrame = QPixmap::fromImage(frame.scaled(size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
    update();
}

void VideoRenderer::clear()
{
    m_currentFrame = QPixmap();
    update();
}

void VideoRenderer::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)
    QPainter painter(this);

    if (m_currentFrame.isNull()) {
        // Draw placeholder
        painter.fillRect(rect(), Qt::black);
        painter.setPen(Qt::white);
        painter.setFont(QFont(QStringLiteral("Sans"), 14));
        painter.drawText(rect(), Qt::AlignCenter, m_placeholderText);
    } else {
        // Draw video frame centered
        painter.fillRect(rect(), Qt::black);
        QPoint target((width() - m_currentFrame.width()) / 2,
                      (height() - m_currentFrame.height()) / 2);
        painter.drawPixmap(target, m_currentFrame);
    }
}

} // namespace itl
