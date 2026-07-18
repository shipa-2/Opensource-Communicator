#pragma once

#include <QObject>
#include <QImage>
#include <QTimer>
#include <QStringList>

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

private slots:
    void captureFrame();

private:
    bool m_running = false;
    QTimer m_timer;
    QImage m_currentFrame;
};

} // namespace itl
