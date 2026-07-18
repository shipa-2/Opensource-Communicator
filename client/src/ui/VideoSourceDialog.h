#pragma once

#include <QByteArray>
#include <QDialog>
#include <QString>

class QCamera;
class QComboBox;
class QLabel;
class QMediaCaptureSession;
class QTimer;
class QVideoFrame;
class QVideoSink;

namespace itl {
class VideoRenderer;
}

class VideoSourceDialog : public QDialog {
    Q_OBJECT

public:
    explicit VideoSourceDialog(bool screensOnly = false, QWidget *parent = nullptr);
    ~VideoSourceDialog() override;

    QByteArray cameraId() const;
    QString screenName() const;
    bool screenSelected() const;

protected:
    void done(int result) override;

private:
    enum class SourceType {
        Camera,
        Screen,
    };

    void populateSources();
    void restartPreview();
    void stopPreview();
    void startCameraPreview();
    void startScreenPreview();
    void showFrame(const QVideoFrame &frame);

    QComboBox *m_sourceTypeCombo = nullptr;
    QComboBox *m_deviceCombo = nullptr;
    QLabel *m_deviceLabel = nullptr;
    itl::VideoRenderer *m_preview = nullptr;
    QCamera *m_camera = nullptr;
    QMediaCaptureSession *m_captureSession = nullptr;
    QVideoSink *m_videoSink = nullptr;
    QTimer *m_screenTimer = nullptr;
    bool m_screensOnly = false;
};
