#include "HistoryRowWidget.h"

#include <QApplication>
#include <QContextMenuEvent>
#include <QEnterEvent>
#include <QEvent>
#include <QFont>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QMouseEvent>
#include <QPalette>
#include <QStyle>
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
  setAutoFillBackground(true);
  setToolTip(tr("Двойной щелчок — заметка\nПКМ — действия"));

  auto *rowLayout = new QHBoxLayout(this);
  rowLayout->setContentsMargins(6, 4, 6, 4);
  rowLayout->setSpacing(8);

  m_arrowLabel = new QLabel(arrow);
  m_arrowLabel->setFixedWidth(18);
  m_arrowLabel->setAlignment(Qt::AlignTop | Qt::AlignHCenter);
  rowLayout->addWidget(m_arrowLabel);

  auto *textCol = new QVBoxLayout;
  textCol->setSpacing(1);
  m_nameLabel = new QLabel(firstLine);
  QFont nameFont = m_nameLabel->font();
  nameFont.setBold(missed);
  m_nameLabel->setFont(nameFont);
  textCol->addWidget(m_nameLabel);

  m_detailLabel = new QLabel(secondLine);
  QFont detailFont = m_detailLabel->font();
  detailFont.setPixelSize(11);
  m_detailLabel->setFont(detailFont);
  textCol->addWidget(m_detailLabel);
  rowLayout->addLayout(textCol, 1);

  m_dateLabel = new QLabel(whenText);
  m_dateLabel->setAlignment(Qt::AlignTop | Qt::AlignRight);
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
  style()->unpolish(this);
  style()->polish(this);
  refreshBackground();
}

void HistoryRowWidget::setChromeAlpha(int alpha)
{
  m_chromeAlpha = qBound(40, alpha, 255);
}

void HistoryRowWidget::refreshAppearance()
{
  if (m_arrowLabel) {
    m_arrowLabel->setStyleSheet(
        QStringLiteral("color:%1; font-size:16px; font-weight:bold;").arg(m_arrowColor));
  }
  refreshTextLabels();
  refreshBackground();
}

void HistoryRowWidget::refreshTextLabels()
{
  if (m_detailLabel) {
    QPalette detailPalette = m_detailLabel->palette();
    detailPalette.setColor(QPalette::WindowText, palette().color(QPalette::Link));
    m_detailLabel->setPalette(detailPalette);
  }
}

void HistoryRowWidget::refreshBackground()
{
  const QPalette app = QApplication::palette();
  setStyleSheet({});

  if (m_selected || m_hovered) {
    QColor highlight = app.color(QPalette::Highlight).lighter(170);
    if (m_chromeAlpha < 255) {
      highlight.setAlpha(m_chromeAlpha);
    }
    QPalette pal = app;
    pal.setColor(QPalette::Window, highlight);
    setAutoFillBackground(true);
    setPalette(pal);
  } else if (m_chromeAlpha < 255) {
    // Let the dimmed history page show through (wallpaper visible).
    setAutoFillBackground(false);
    setPalette(app);
  } else {
    QPalette pal = app;
    pal.setColor(QPalette::Window, app.color(QPalette::Base));
    setAutoFillBackground(true);
    setPalette(pal);
  }
  update();
}

void HistoryRowWidget::contextMenuEvent(QContextMenuEvent *event)
{
  QMenu menu(this);
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
  refreshBackground();
  QWidget::enterEvent(event);
}

void HistoryRowWidget::leaveEvent(QEvent *event)
{
  m_hovered = false;
  refreshBackground();
  QWidget::leaveEvent(event);
}

void HistoryRowWidget::changeEvent(QEvent *event)
{
  QWidget::changeEvent(event);
  if (event->type() == QEvent::PaletteChange) {
    refreshAppearance();
  }
}
