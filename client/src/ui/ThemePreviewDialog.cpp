#include "ThemePreviewDialog.h"

#include "ui/StyleHelper.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPushButton>
#include <QVBoxLayout>

namespace {

constexpr int kPreviewWidth = 260;
constexpr int kPreviewHeight = 414;

int wallpaperAlphaFromOpacity(int opacityPercent)
{
  return qBound(40, qRound(255.0 * qBound(0, opacityPercent, 100) / 100.0), 255);
}

} // namespace

ThemePreviewDialog::ThemePreviewDialog(const QPixmap &wallpaper, int uiOpacity, int listOpacity,
                                       QWidget *parent)
    : QDialog(parent)
{
  setWindowTitle(tr("Предпросмотр темы"));
  setModal(true);
  resize(340, 560);

  auto *root = new QVBoxLayout(this);
  root->setSpacing(10);

  m_previewLabel = new QLabel;
  m_previewLabel->setFixedSize(kPreviewWidth, kPreviewHeight);
  m_previewLabel->setAlignment(Qt::AlignCenter);
  m_previewLabel->setFrameShape(QFrame::StyledPanel);
  m_previewLabel->setPixmap(buildPreviewPixmap(wallpaper, uiOpacity, listOpacity));
  root->addWidget(m_previewLabel, 0, Qt::AlignHCenter);

  auto *info = new QLabel(
      tr("Затемнение интерфейса: %1%\nЗатемнение списков: %2%").arg(uiOpacity).arg(listOpacity));
  info->setAlignment(Qt::AlignCenter);
  info->setWordWrap(true);
  root->addWidget(info);

  auto *buttons = new QHBoxLayout;
  auto *applyBtn = new QPushButton(tr("Применить"));
  auto *cancelBtn = new QPushButton(tr("Отменить"));
  applyBtn->setDefault(true);
  buttons->addStretch();
  buttons->addWidget(applyBtn);
  buttons->addWidget(cancelBtn);
  root->addLayout(buttons);

  connect(applyBtn, &QPushButton::clicked, this, &QDialog::accept);
  connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);

  itl::applyNativeButtons(this);
}

QPixmap ThemePreviewDialog::buildPreviewPixmap(const QPixmap &wallpaper, int uiOpacity, int listOpacity)
{
  QPixmap canvas(kPreviewWidth, kPreviewHeight);
  canvas.fill(Qt::black);

  if (wallpaper.isNull()) {
    return canvas;
  }

  QPixmap scaled =
      wallpaper.scaled(kPreviewWidth, kPreviewHeight, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
  const int x = qMax(0, (scaled.width() - kPreviewWidth) / 2);
  const int y = qMax(0, (scaled.height() - kPreviewHeight) / 2);
  scaled = scaled.copy(x, y, kPreviewWidth, kPreviewHeight);

  QPainter painter(&canvas);
  painter.drawPixmap(0, 0, scaled);

  const int uiAlpha = wallpaperAlphaFromOpacity(uiOpacity);
  const int listAlpha = wallpaperAlphaFromOpacity(listOpacity);
  painter.fillRect(0, 0, kPreviewWidth, kPreviewHeight * 2 / 5,
                   QColor(0, 0, 0, 255 - uiAlpha));
  painter.fillRect(16, kPreviewHeight * 2 / 5 + 12, kPreviewWidth - 32, kPreviewHeight / 3,
                   QColor(0, 0, 0, 255 - listAlpha));
  painter.end();
  return canvas;
}
