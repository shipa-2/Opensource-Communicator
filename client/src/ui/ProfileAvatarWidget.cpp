#include "ProfileAvatarWidget.h"

#include "settings/AppSettings.h"

#include <QColorDialog>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>
#include <QEvent>
#include <QPushButton>
#include <QWidgetAction>

namespace {
constexpr int kSize = 72;
constexpr qreal kBorderWidth = 1.0;

QPen avatarBorderPen(const QWidget *widget)
{
  QPen borderPen(widget->palette().color(QPalette::WindowText), kBorderWidth);
  borderPen.setCosmetic(false);
  borderPen.setJoinStyle(Qt::RoundJoin);
  return borderPen;
}
} // namespace

ProfileAvatarWidget::ProfileAvatarWidget(itl::AppSettings *settings, QWidget *parent)
    : QWidget(parent)
    , m_settings(settings)
{
  setObjectName(QStringLiteral("profileAvatar"));
  setFixedSize(kSize, kSize);
  setCursor(Qt::PointingHandCursor);
  setAutoFillBackground(false);
  reloadFromSettings();
}

QRectF ProfileAvatarWidget::circleRect() const
{
  return QRectF(1.0, 1.0, width() - 2.0, height() - 2.0);
}

void ProfileAvatarWidget::setLetter(const QString &letter)
{
  m_letter = letter.trimmed().isEmpty() ? QStringLiteral("?") : letter.left(1).toUpper();
  update();
}

void ProfileAvatarWidget::refreshFromSettings()
{
  reloadFromSettings();
}

void ProfileAvatarWidget::reloadFromSettings()
{
  m_photo = QPixmap();
  if (m_settings) {
    m_bgColor = QColor(m_settings->profileAvatarColor());
    if (!m_bgColor.isValid()) {
      m_bgColor = QColor(QStringLiteral("#5a9e2f"));
    }
    const QString imagePath = m_settings->profileAvatarPath();
    if (!imagePath.isEmpty()) {
      const QPixmap loaded(imagePath);
      if (!loaded.isNull()) {
        m_photo = loaded;
      }
    }
  }
  update();
}

void ProfileAvatarWidget::refreshAppearance()
{
  update();
}

void ProfileAvatarWidget::changeEvent(QEvent *event)
{
  QWidget::changeEvent(event);
  if (event->type() == QEvent::PaletteChange || event->type() == QEvent::ApplicationPaletteChange) {
    refreshAppearance();
  }
}

void ProfileAvatarWidget::paintEvent(QPaintEvent *)
{
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing, true);
  painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

  const QRectF circle = circleRect();
  const QPen borderPen = avatarBorderPen(this);

  if (!m_photo.isNull()) {
    QPainterPath clipPath;
    clipPath.addEllipse(circle);
    painter.setClipPath(clipPath);
    const QPixmap scaled = m_photo.scaled(
        circle.size().toSize(), Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
    const QPointF topLeft(
        circle.center().x() - scaled.width() / 2.0,
        circle.center().y() - scaled.height() / 2.0);
    painter.drawPixmap(topLeft, scaled);
    painter.setClipping(false);
    painter.setPen(borderPen);
    painter.setBrush(Qt::NoBrush);
    painter.drawEllipse(circle);
    return;
  }

  painter.setPen(borderPen);
  painter.setBrush(m_bgColor);
  painter.drawEllipse(circle);

  QFont font = painter.font();
  font.setPixelSize(34);
  font.setBold(true);
  painter.setFont(font);
  painter.setPen(palette().color(QPalette::WindowText));
  painter.setBrush(Qt::NoBrush);
  painter.drawText(circle, Qt::AlignCenter, m_letter);
}

void ProfileAvatarWidget::mousePressEvent(QMouseEvent *event)
{
  if (event->button() == Qt::LeftButton) {
    showAvatarMenu();
    event->accept();
    return;
  }
  QWidget::mousePressEvent(event);
}

void ProfileAvatarWidget::showAvatarMenu()
{
  QMenu menu(this);
  menu.setObjectName(QStringLiteral("avatarMenu"));

  auto *row = new QWidget;
  row->setObjectName(QStringLiteral("avatarMenuRow"));
  row->setAutoFillBackground(true);
  auto *rowLayout = new QHBoxLayout(row);
  rowLayout->setContentsMargins(8, 4, 8, 4);
  rowLayout->setSpacing(6);

  auto *photoBtn = new QPushButton(tr("Загрузить фото"));
  photoBtn->setObjectName(QStringLiteral("avatarMenuBtn"));
  auto *colorBtn = new QPushButton(QStringLiteral("🎨"));
  colorBtn->setObjectName(QStringLiteral("avatarMenuBtn"));
  colorBtn->setFixedWidth(36);
  colorBtn->setToolTip(tr("Цвет фона"));
  rowLayout->addWidget(photoBtn, 1);
  rowLayout->addWidget(colorBtn);

  auto *widgetAction = new QWidgetAction(&menu);
  widgetAction->setDefaultWidget(row);
  menu.addAction(widgetAction);

  if (m_settings && !m_settings->profileAvatarPath().isEmpty()) {
    menu.addAction(tr("Убрать фото"), this, [this]() {
      if (!m_settings) {
        return;
      }
      m_settings->setProfileAvatarPath({});
      reloadFromSettings();
      emit settingsChanged();
    });
  }

  connect(photoBtn, &QPushButton::clicked, &menu, &QMenu::close);
  connect(colorBtn, &QPushButton::clicked, &menu, &QMenu::close);
  connect(photoBtn, &QPushButton::clicked, this, &ProfileAvatarWidget::pickPhoto);
  connect(colorBtn, &QPushButton::clicked, this, &ProfileAvatarWidget::pickColor);

  menu.exec(mapToGlobal(rect().bottomLeft()));
}

void ProfileAvatarWidget::pickPhoto()
{
  const QString path = QFileDialog::getOpenFileName(
      this, tr("Выберите фото"), {}, tr("Изображения (*.png *.jpg *.jpeg *.bmp *.webp)"));
  if (path.isEmpty() || !m_settings) {
    return;
  }
  m_settings->setProfileAvatarPath(path);
  reloadFromSettings();
  emit settingsChanged();
}

void ProfileAvatarWidget::pickColor()
{
  const QColor initial = m_settings ? QColor(m_settings->profileAvatarColor()) : QColor(QStringLiteral("#5a9e2f"));
  const QColor chosen = QColorDialog::getColor(initial, this, tr("Цвет фона аватара"));
  if (!chosen.isValid() || !m_settings) {
    return;
  }
  m_settings->setProfileAvatarColor(chosen.name());
  reloadFromSettings();
  emit settingsChanged();
}
