#include "VideoSourceDialog.h"

#include "ui/StyleHelper.h"
#include "ui/VideoRenderer.h"

#include <QCamera>
#include <QCameraDevice>
#include <QComboBox>
#include <QFormLayout>
#include <QGuiApplication>
#include <QLabel>
#include <QMediaCaptureSession>
#include <QMediaDevices>
#include <QPushButton>
#include <QScreen>
#include <QTimer>
#include <QVBoxLayout>
#include <QVideoFrame>
#include <QVideoSink>

VideoSourceDialog::VideoSourceDialog(bool screensOnly, QWidget *parent)
    : QDialog(parent)
    , m_screensOnly(screensOnly)
{
  setWindowTitle(tr("Источник видео"));
  setModal(true);
  setFixedWidth(390);

  auto *root = new QVBoxLayout(this);
  root->setContentsMargins(18, 18, 18, 18);
  root->setSpacing(10);

  auto *form = itl::createDialogForm();
#ifdef Q_OS_WIN
  if (!m_screensOnly) {
    m_sourceTypeCombo = new QComboBox;
    m_sourceTypeCombo->addItem(tr("Камера"), static_cast<int>(SourceType::Camera));
    m_sourceTypeCombo->addItem(tr("Экран"), static_cast<int>(SourceType::Screen));
    form->addRow(tr("Источник"), m_sourceTypeCombo);
  }
#endif
  m_deviceLabel = new QLabel(tr("Камера"));
  m_deviceCombo = new QComboBox;
  form->addRow(m_deviceLabel, m_deviceCombo);
  root->addLayout(form);

  m_preview = new itl::VideoRenderer(this);
  m_preview->setFixedSize(352, 198);
  m_preview->setPlaceholderText(tr("Предпросмотр недоступен"));
  root->addWidget(m_preview, 0, Qt::AlignHCenter);

  QPushButton *cancel = nullptr;
  QPushButton *ok = nullptr;
  root->addLayout(itl::createDialogButtonRow(&cancel, &ok, tr("Выбрать")));
  connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
  connect(ok, &QPushButton::clicked, this, &QDialog::accept);
  connect(m_deviceCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, &VideoSourceDialog::restartPreview);
#ifdef Q_OS_WIN
  if (m_sourceTypeCombo) {
    connect(m_sourceTypeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this]() {
              populateSources();
              restartPreview();
            });
  }
#endif

  itl::applyFormDialogStyle(this);
  populateSources();
  restartPreview();
}

VideoSourceDialog::~VideoSourceDialog()
{
  stopPreview();
}

QByteArray VideoSourceDialog::cameraId() const
{
  return screenSelected() ? QByteArray() : m_deviceCombo->currentData().toByteArray();
}

QString VideoSourceDialog::screenName() const
{
  return screenSelected() ? m_deviceCombo->currentData().toString() : QString();
}

bool VideoSourceDialog::screenSelected() const
{
#ifdef Q_OS_WIN
  return m_screensOnly
      || (m_sourceTypeCombo
          && static_cast<SourceType>(m_sourceTypeCombo->currentData().toInt()) == SourceType::Screen);
#else
  return false;
#endif
}

void VideoSourceDialog::done(int result)
{
  stopPreview();
  QDialog::done(result);
}

void VideoSourceDialog::populateSources()
{
  m_deviceCombo->blockSignals(true);
  m_deviceCombo->clear();
  if (screenSelected()) {
    m_deviceLabel->setText(tr("Экран"));
    for (QScreen *screen : QGuiApplication::screens()) {
      if (screen) {
        m_deviceCombo->addItem(screen->name(), screen->name());
      }
    }
  } else {
    m_deviceLabel->setText(tr("Камера"));
    const QList<QCameraDevice> cameras = QMediaDevices::videoInputs();
    for (const QCameraDevice &camera : cameras) {
      m_deviceCombo->addItem(camera.description(), camera.id());
    }
  }
  m_deviceCombo->setEnabled(m_deviceCombo->count() > 0);
  m_deviceCombo->blockSignals(false);
}

void VideoSourceDialog::restartPreview()
{
  stopPreview();
  m_preview->clear();
  if (m_deviceCombo->currentIndex() < 0) {
    m_preview->setPlaceholderText(screenSelected() ? tr("Экраны не найдены")
                                                   : tr("Камеры не найдены"));
    return;
  }
  if (screenSelected()) {
    startScreenPreview();
  } else {
    startCameraPreview();
  }
}

void VideoSourceDialog::stopPreview()
{
  if (m_screenTimer) {
    m_screenTimer->stop();
    delete m_screenTimer;
    m_screenTimer = nullptr;
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
}

void VideoSourceDialog::startCameraPreview()
{
  const QByteArray selectedId = cameraId();
  QCameraDevice selected;
  for (const QCameraDevice &camera : QMediaDevices::videoInputs()) {
    if (camera.id() == selectedId) {
      selected = camera;
      break;
    }
  }
  if (selected.isNull()) {
    return;
  }

  m_camera = new QCamera(selected, this);
  m_captureSession = new QMediaCaptureSession(this);
  m_videoSink = new QVideoSink(this);
  m_captureSession->setCamera(m_camera);
  m_captureSession->setVideoSink(m_videoSink);
  connect(m_videoSink, &QVideoSink::videoFrameChanged,
          this, &VideoSourceDialog::showFrame);
  connect(m_camera, &QCamera::errorOccurred, this,
          [this](QCamera::Error, const QString &message) {
            m_preview->setPlaceholderText(message.isEmpty() ? tr("Камера недоступна") : message);
          });
  m_camera->start();
}

void VideoSourceDialog::startScreenPreview()
{
  QScreen *selected = nullptr;
  const QString selectedName = screenName();
  for (QScreen *screen : QGuiApplication::screens()) {
    if (screen && screen->name() == selectedName) {
      selected = screen;
      break;
    }
  }
  if (!selected) {
    return;
  }

  m_screenTimer = new QTimer(this);
  const auto updatePreview = [this, selected]() {
    const QPixmap frame = selected->grabWindow(0);
    if (!frame.isNull()) {
      m_preview->onFrameReceived(frame.toImage());
    }
  };
  connect(m_screenTimer, &QTimer::timeout, this, updatePreview);
  m_screenTimer->start(250);
  updatePreview();
}

void VideoSourceDialog::showFrame(const QVideoFrame &frame)
{
  if (frame.isValid()) {
    m_preview->onFrameReceived(frame.toImage());
  }
}
