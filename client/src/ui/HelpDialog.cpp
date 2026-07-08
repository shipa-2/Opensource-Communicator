#include "HelpDialog.h"

#include "ui/StyleHelper.h"

#include <QLabel>
#include <QPixmap>
#include <QPushButton>
#include <QVBoxLayout>

namespace {
constexpr int kMaxImageWidth = 420;
}

HelpDialog::HelpDialog(QWidget *parent)
    : QDialog(parent)
{
  setWindowTitle(tr("Помощь"));
  setObjectName(QStringLiteral("helpDialog"));

  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(12, 12, 12, 12);
  layout->setSpacing(10);

  auto *image = new QLabel;
  QPixmap pixmap(QStringLiteral(":/help.jpg"));
  if (!pixmap.isNull() && pixmap.width() > kMaxImageWidth) {
    pixmap = pixmap.scaledToWidth(kMaxImageWidth, Qt::SmoothTransformation);
  }
  image->setPixmap(pixmap);
  image->setAlignment(Qt::AlignCenter);
  layout->addWidget(image);

  auto *closeBtn = new QPushButton(tr("Закрыть"));
  connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);
  layout->addWidget(closeBtn, 0, Qt::AlignCenter);

  layout->setSizeConstraint(QLayout::SetFixedSize);

  itl::applyDialogStyle(this);
}
