#pragma once

#include <QWidget>
#include <QImage>
#include <QPixmap>

namespace itl {

class VideoRenderer : public QWidget {
    Q_OBJECT

public:
    explicit VideoRenderer(QWidget *parent = nullptr);

    void setPlaceholderText(const QString &text);

public slots:
    void onFrameReceived(const QImage &frame);
    void clear();

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QPixmap m_currentFrame;
    QString m_placeholderText = QStringLiteral("Video");
};

} // namespace itl
