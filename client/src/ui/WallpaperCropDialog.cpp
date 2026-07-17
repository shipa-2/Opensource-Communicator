#include "WallpaperCropDialog.h"

#include "ui/StyleHelper.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWheelEvent>

namespace {

class CropCanvas : public QWidget {
public:
  explicit CropCanvas(const QPixmap &source, const QSize &targetSize, QWidget *parent = nullptr)
      : QWidget(parent)
      , m_source(source)
      , m_targetSize(targetSize)
  {
    setMinimumSize(360, 420);
    setMouseTracking(true);
    resetView();
  }

  QPixmap croppedPixmap() const
  {
    if (m_source.isNull() || m_targetSize.isEmpty()) {
      return {};
    }

    const QRectF crop = cropRect();
    const QPointF topLeft = imageTopLeft(crop);
    QRectF sourceRect((crop.left() - topLeft.x()) / m_scale, (crop.top() - topLeft.y()) / m_scale,
                      crop.width() / m_scale, crop.height() / m_scale);
    sourceRect = sourceRect.intersected(QRectF(m_source.rect()));
    if (sourceRect.isEmpty()) {
      return {};
    }

    return m_source.copy(sourceRect.toRect()).scaled(m_targetSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
  }

protected:
  void paintEvent(QPaintEvent *) override
  {
    QPainter painter(this);
    painter.fillRect(rect(), palette().color(QPalette::Mid));

    if (m_source.isNull()) {
      painter.setPen(palette().color(QPalette::Text));
      painter.drawText(rect(), Qt::AlignCenter, QStringLiteral("Нет изображения"));
      return;
    }

    const QRectF crop = cropRect();
    const QPointF topLeft = imageTopLeft(crop);
    const QRectF imageRect(topLeft, QSizeF(m_source.width() * m_scale, m_source.height() * m_scale));

    painter.setClipRect(crop.toRect());
    painter.drawPixmap(imageRect, m_source, m_source.rect());
    painter.setClipping(false);

    painter.fillRect(QRectF(rect().topLeft(), QPointF(rect().right(), crop.top())), QColor(0, 0, 0, 140));
    painter.fillRect(QRectF(rect().bottomLeft(), QPointF(rect().right(), crop.bottom())), QColor(0, 0, 0, 140));
    painter.fillRect(QRectF(crop.topLeft(), QPointF(crop.left(), crop.bottom())), QColor(0, 0, 0, 140));
    painter.fillRect(QRectF(QPointF(crop.right(), crop.top()), crop.bottomRight()), QColor(0, 0, 0, 140));

    QPen framePen(palette().color(QPalette::Highlight), 2);
    painter.setPen(framePen);
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(crop);

    painter.setPen(palette().color(QPalette::Text));
    painter.drawText(crop.adjusted(0, 6, 0, 0), Qt::AlignHCenter | Qt::AlignTop,
                     QStringLiteral("%1×%2").arg(m_targetSize.width()).arg(m_targetSize.height()));
  }

  void mousePressEvent(QMouseEvent *event) override
  {
    if (event->button() == Qt::LeftButton) {
      m_dragging = true;
      m_lastPos = event->pos();
      event->accept();
    }
  }

  void mouseMoveEvent(QMouseEvent *event) override
  {
    if (!m_dragging) {
      return;
    }
    m_offset += event->pos() - m_lastPos;
    m_lastPos = event->pos();
    update();
  }

  void mouseReleaseEvent(QMouseEvent *event) override
  {
    if (event->button() == Qt::LeftButton) {
      m_dragging = false;
    }
  }

  void wheelEvent(QWheelEvent *event) override
  {
    if (m_source.isNull()) {
      return;
    }

    const qreal factor = event->angleDelta().y() > 0 ? 1.08 : 1.0 / 1.08;
    const qreal minScale = minimumCoverScale();
    m_scale = qBound(minScale, m_scale * factor, minScale * 4.0);
    update();
    event->accept();
  }

private:
  QRectF cropRect() const
  {
    const qreal aspect = static_cast<qreal>(m_targetSize.width()) / m_targetSize.height();
    qreal width = this->width() * 0.88;
    qreal height = width / aspect;
    if (height > this->height() * 0.82) {
      height = this->height() * 0.82;
      width = height * aspect;
    }
    return QRectF((this->width() - width) / 2.0, (this->height() - height) / 2.0, width, height);
  }

  qreal minimumCoverScale() const
  {
    const QRectF crop = cropRect();
    if (m_source.isNull() || crop.isEmpty()) {
      return 1.0;
    }
    return qMax(crop.width() / m_source.width(), crop.height() / m_source.height());
  }

  QPointF imageTopLeft(const QRectF &crop) const
  {
    const QSizeF scaled(m_source.width() * m_scale, m_source.height() * m_scale);
    const QPointF center = crop.center();
    return center - QPointF(scaled.width() / 2.0, scaled.height() / 2.0) + m_offset;
  }

  void resetView()
  {
    m_scale = minimumCoverScale();
    m_offset = QPointF();
    update();
  }

  QPixmap m_source;
  QSize m_targetSize;
  qreal m_scale = 1.0;
  QPointF m_offset;
  bool m_dragging = false;
  QPoint m_lastPos;
};

} // namespace

WallpaperCropDialog::WallpaperCropDialog(const QPixmap &source, const QSize &targetSize, QWidget *parent,
                                         const QString &dialogTitle)
    : QDialog(parent)
    , m_targetSize(targetSize)
{
  setWindowTitle(dialogTitle.isEmpty() ? tr("Обрезка изображения") : dialogTitle);
  resize(420, 520);

  auto *layout = new QVBoxLayout(this);
  layout->addWidget(new QLabel(tr("Перетащите изображение и прокрутите колёсико для масштаба.")));

  m_canvas = new CropCanvas(source, targetSize, this);
  layout->addWidget(m_canvas, 1);

  auto *row = new QHBoxLayout;
  row->addStretch();
  auto *cancelBtn = new QPushButton(tr("Отмена"));
  auto *acceptBtn = new QPushButton(tr("Применить"));
  acceptBtn->setDefault(true);
  row->addWidget(cancelBtn);
  row->addWidget(acceptBtn);
  layout->addLayout(row);

  connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
  connect(acceptBtn, &QPushButton::clicked, this, &QDialog::accept);

  itl::applyDialogStyle(this);
}

QPixmap WallpaperCropDialog::croppedPixmap() const
{
  if (!m_canvas) {
    return {};
  }
  return static_cast<CropCanvas *>(m_canvas)->croppedPixmap();
}

QPixmap WallpaperCropDialog::cropImage(const QPixmap &source, const QSize &targetSize, QWidget *parent,
                                       const QString &dialogTitle)
{
  if (source.isNull() || targetSize.isEmpty()) {
    return {};
  }

  WallpaperCropDialog dialog(source, targetSize, parent, dialogTitle);
  if (dialog.exec() != QDialog::Accepted) {
    return {};
  }
  return dialog.croppedPixmap();
}
