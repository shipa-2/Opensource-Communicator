#include "HistoryRowWidget.h"

#include "ui/StyleHelper.h"

#include <QApplication>
#include <QContextMenuEvent>
#include <QEnterEvent>
#include <QEvent>
#include <QFont>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPaintEvent>
#include <QPalette>
#include <QPen>
#include <QTimer>
#include <QVBoxLayout>

HistoryRowWidget::HistoryRowWidget(const QString &peer, const QString &displayName,
                                   const QString &firstLine, const QString &secondLine,
                                   const QString &whenText, const QString &arrow,
                                   const QString &arrowColor, bool missed, QWidget *parent)
    : QWidget(parent)
    , m_peer(peer)
    , m_displayName(displayName)
    , m_missed(missed)
    , m_arrowColor(arrowColor)
{
  setObjectName(QStringLiteral("historyRow"));
  setAttribute(Qt::WA_Hover, true);
  setMouseTracking(true);
  setAutoFillBackground(false);
  setMinimumHeight(48);
  setToolTip(tr("Двойной щелчок — заметка\nПКМ — действия"));

  auto *rowLayout = new QHBoxLayout(this);
  rowLayout->setContentsMargins(8, 4, 8, 4);
  rowLayout->setSpacing(8);

  m_arrowLabel = new QLabel(arrow);
  m_arrowLabel->setFixedWidth(18);
  m_arrowLabel->setAlignment(Qt::AlignVCenter | Qt::AlignHCenter);
  m_arrowLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
  rowLayout->addWidget(m_arrowLabel);

  auto *textCol = new QVBoxLayout;
  textCol->setSpacing(0);
  m_nameLabel = new QLabel(firstLine);
  m_nameLabel->setObjectName(QStringLiteral("historyName"));
  QFont nameFont = m_nameLabel->font();
  nameFont.setBold(missed);
  m_nameLabel->setFont(nameFont);
  textCol->addWidget(m_nameLabel);

  m_detailLabel = new QLabel(secondLine);
  m_detailLabel->setObjectName(QStringLiteral("historyDetail"));
  QFont detailFont = m_detailLabel->font();
  detailFont.setPixelSize(12);
  m_detailLabel->setFont(detailFont);
  m_detailLabel->setVisible(!secondLine.trimmed().isEmpty());
  textCol->addWidget(m_detailLabel);
  rowLayout->addLayout(textCol, 1);

  m_dateLabel = new QLabel(whenText);
  m_dateLabel->setAlignment(Qt::AlignVCenter | Qt::AlignRight);
  QFont dateFont = m_dateLabel->font();
  dateFont.setPixelSize(11);
  m_dateLabel->setFont(dateFont);
  rowLayout->addWidget(m_dateLabel);

  refreshAppearance();
}

void HistoryRowWidget::setSelected(bool selected)
{
  if (m_selected == selected) {
    return;
  }
  m_selected = selected;
  setProperty("selected", selected);
  update();
}

void HistoryRowWidget::setChromeAlpha(int alpha)
{
  m_chromeAlpha = qBound(40, alpha, 255);
}

void HistoryRowWidget::refreshAppearance()
{
  if (m_arrowLabel) {
    m_arrowLabel->setStyleSheet(
        QStringLiteral("color:%1; font-size:16px; font-weight:bold; background: transparent;")
            .arg(m_arrowColor));
  }
  refreshBackground();
  refreshTextLabels();
  update();
}

void HistoryRowWidget::refreshTextLabels()
{
  const QPalette app = QApplication::palette();
  const QColor text = app.color(QPalette::WindowText);
  const QColor link = app.color(QPalette::Link);
  const QColor muted = app.color(QPalette::PlaceholderText).isValid()
                           ? app.color(QPalette::PlaceholderText)
                           : app.color(QPalette::Mid);

  // Match ContactRowWidget: title = WindowText, secondary line (number/details) = Link.
  if (m_nameLabel) {
    m_nameLabel->setStyleSheet(
        QStringLiteral("color: %1; background: transparent;").arg(text.name(QColor::HexRgb)));
    m_nameLabel->update();
  }

  if (m_detailLabel) {
    m_detailLabel->setStyleSheet(
        QStringLiteral("color: %1; font-size: 12px; background: transparent;")
            .arg(link.name(QColor::HexRgb)));
    m_detailLabel->update();
  }

  if (m_dateLabel) {
    m_dateLabel->setStyleSheet(
        QStringLiteral("color: %1; font-size: 11px; background: transparent;")
            .arg(muted.name(QColor::HexRgb)));
    m_dateLabel->update();
  }
}

void HistoryRowWidget::refreshBackground()
{
  // Contour/hover is drawn in paintEvent — keep the row transparent for wallpaper.
  setStyleSheet({});
  setAutoFillBackground(false);
  setPalette(QApplication::palette());
}

void HistoryRowWidget::paintEvent(QPaintEvent *event)
{
  QWidget::paintEvent(event);
  if (!m_hovered && !m_selected) {
    return;
  }

  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing, true);
  painter.setClipRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5));

  const QColor accent = QApplication::palette().color(QPalette::Highlight);
  QColor fill = accent;
  fill.setAlpha(m_selected ? 55 : 35);
  const qreal penW = m_selected ? 1.5 : 1.0;
  const qreal margin = 2.0;
  const qreal radius = 6.0;
  const QRectF frame = QRectF(rect()).adjusted(margin, margin, -margin, -margin);

  painter.setPen(Qt::NoPen);
  painter.setBrush(fill);
  const qreal fillInset = penW * 0.5;
  painter.drawRoundedRect(frame.adjusted(fillInset, fillInset, -fillInset, -fillInset),
                          qMax(0.0, radius - fillInset), qMax(0.0, radius - fillInset));

  QPen pen(accent, penW);
  pen.setJoinStyle(Qt::RoundJoin);
  pen.setCapStyle(Qt::RoundCap);
  painter.setBrush(Qt::NoBrush);
  painter.setPen(pen);
  painter.drawRoundedRect(frame, radius, radius);
}

void HistoryRowWidget::contextMenuEvent(QContextMenuEvent *event)
{
  QMenu menu(this);
  itl::applyPopupMenuStyle(&menu);
  menu.addAction(tr("Позвонить"), this, [this]() { emit callRequested(m_peer); });
  menu.addAction(tr("Сообщение"), this, [this]() { emit chatRequested(m_peer); });
  menu.addAction(tr("Заметка"), this, [this]() { emit notesRequested(m_peer); });
  menu.exec(event->globalPos());
  event->accept();
}

void HistoryRowWidget::mouseDoubleClickEvent(QMouseEvent *event)
{
  emit notesRequested(m_peer);
  event->accept();
}

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
void HistoryRowWidget::enterEvent(QEnterEvent *event)
#else
void HistoryRowWidget::enterEvent(QEvent *event)
#endif
{
  m_hovered = true;
  update();
  QWidget::enterEvent(event);
}

void HistoryRowWidget::leaveEvent(QEvent *event)
{
  QWidget::leaveEvent(event);
  if (m_hovered) {
    m_hovered = false;
    update();
  }
  QTimer::singleShot(0, this, [this]() {
    const bool inside = underMouse();
    if (m_hovered != inside) {
      m_hovered = inside;
      update();
    }
  });
}

void HistoryRowWidget::changeEvent(QEvent *event)
{
  QWidget::changeEvent(event);
  if (event->type() == QEvent::PaletteChange) {
    refreshAppearance();
  }
}
