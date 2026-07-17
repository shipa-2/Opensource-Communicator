#include "DialKeypadWidget.h"

#include <QApplication>
#include <QEvent>
#include <QGridLayout>
#include <QLineEdit>
#include <QPalette>
#include <QPushButton>
#include <QSizePolicy>
#include <QTimer>

namespace {

constexpr int kHoldActionMs = 1000;
constexpr int kHoldAccentMs = 250;
constexpr int kHoldProgressMs = kHoldActionMs - kHoldAccentMs;
constexpr int kHoldProgressTickMs = 16;

QColor blendColors(const QColor &from, const QColor &to, qreal amount)
{
  const qreal t = qBound(0.0, amount, 1.0);
  return QColor(
      qRound(from.red() + (to.red() - from.red()) * t),
      qRound(from.green() + (to.green() - from.green()) * t),
      qRound(from.blue() + (to.blue() - from.blue()) * t));
}

struct DialKeyDef {
    const char *label;
    int row;
    int col;
    bool backspace = false;
    bool zero = false;
};

const DialKeyDef kDialKeys[] = {
    {"1", 0, 0},
    {"2", 0, 1},
    {"3", 0, 2},
    {"4", 1, 0},
    {"5", 1, 1},
    {"6", 1, 2},
    {"7", 2, 0},
    {"8", 2, 1},
    {"9", 2, 2},
    {nullptr, 3, 0, true},
    {"0", 3, 1, false, true},
    {"#", 3, 2},
};

} // namespace

DialKeypadWidget::DialKeypadWidget(QWidget *parent)
    : QWidget(parent)
    , m_grid(new QGridLayout(this))
{
  m_grid->setSpacing(8);
  m_grid->setContentsMargins(0, 8, 0, 0);

  for (const DialKeyDef &key : kDialKeys) {
    auto *button = new QPushButton;
    button->setObjectName(QStringLiteral("dialKeyBtn"));
    button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    button->setMinimumHeight(52);
    button->setCursor(Qt::PointingHandCursor);

    if (key.backspace) {
      button->setText(tr("⌫"));
      button->setAccessibleName(tr("Стереть"));
      m_backspaceBtn = button;
      connect(button, &QPushButton::pressed, this, &DialKeypadWidget::onBackspacePressed);
      connect(button, &QPushButton::released, this, &DialKeypadWidget::onBackspaceReleased);
      connect(button, &QPushButton::clicked, this, &DialKeypadWidget::onBackspaceClicked);
      applyButtonStyle(button, true);
    } else if (key.zero) {
      button->setText(QStringLiteral("0"));
      m_zeroBtn = button;
      connect(button, &QPushButton::pressed, this, &DialKeypadWidget::onZeroPressed);
      connect(button, &QPushButton::released, this, &DialKeypadWidget::onZeroReleased);
      connect(button, &QPushButton::clicked, this, &DialKeypadWidget::onZeroClicked);
      applyButtonStyle(button, false);
    } else {
      button->setText(QString::fromUtf8(key.label));
      const QString digit = QString::fromUtf8(key.label);
      connect(button, &QPushButton::clicked, this, [this, digit]() { appendChar(digit); });
      applyButtonStyle(button, false);
    }

    m_grid->addWidget(button, key.row, key.col);
    m_keys.append(button);
  }

  m_holdTimer = new QTimer(this);
  m_holdTimer->setSingleShot(true);
  m_holdTimer->setInterval(kHoldActionMs);
  connect(m_holdTimer, &QTimer::timeout, this, &DialKeypadWidget::onHoldTimeout);

  m_holdPhaseTimer = new QTimer(this);
  m_holdPhaseTimer->setSingleShot(true);
  m_holdPhaseTimer->setInterval(kHoldAccentMs);
  connect(m_holdPhaseTimer, &QTimer::timeout, this, &DialKeypadWidget::onHoldPhaseTimeout);

  m_holdProgressTimer = new QTimer(this);
  m_holdProgressTimer->setInterval(kHoldProgressTickMs);
  connect(m_holdProgressTimer, &QTimer::timeout, this, &DialKeypadWidget::onHoldProgressTick);
}

void DialKeypadWidget::setLineEdit(QLineEdit *edit)
{
  m_edit = edit;
}

void DialKeypadWidget::setDtmfMode(bool enabled)
{
  if (m_dtmfMode == enabled) {
    return;
  }
  m_dtmfMode = enabled;
  if (m_backspaceBtn) {
    if (enabled) {
      m_backspaceBtn->setText(QStringLiteral("*"));
      m_backspaceBtn->setAccessibleName(tr("Звёздочка"));
    } else {
      m_backspaceBtn->setText(tr("⌫"));
      m_backspaceBtn->setAccessibleName(tr("Стереть"));
    }
  }
}

void DialKeypadWidget::setCompact(bool compact)
{
  if (m_compact == compact) {
    return;
  }
  m_compact = compact;
  const int height = compact ? 40 : 52;
  for (QPushButton *button : m_keys) {
    button->setMinimumHeight(height);
  }
}

void DialKeypadWidget::refreshAppearance()
{
  for (QPushButton *button : m_keys) {
    if (button == m_holdBtn && m_holdVisual != HoldVisual::None) {
      updateHoldVisual();
      continue;
    }
    const bool backspace = button == m_backspaceBtn;
    applyButtonStyle(button, backspace);
  }
}

void DialKeypadWidget::setChromeAlpha(int alpha)
{
  m_chromeAlpha = qBound(40, alpha, 255);
}

void DialKeypadWidget::changeEvent(QEvent *event)
{
  QWidget::changeEvent(event);
}

QString DialKeypadWidget::colorCss(const QColor &color) const
{
  if (m_chromeAlpha >= 255) {
    return color.name();
  }
  return QStringLiteral("rgba(%1, %2, %3, %4)")
      .arg(color.red())
      .arg(color.green())
      .arg(color.blue())
      .arg(m_chromeAlpha);
}

void DialKeypadWidget::applyButtonStyle(QPushButton *button, bool backspace) const
{
  const QPalette pal = QApplication::palette(this);
  const QColor accent = pal.color(QPalette::Highlight);
  const QColor accentText = pal.color(QPalette::HighlightedText);
  const QColor base = backspace ? pal.color(QPalette::Midlight) : pal.color(QPalette::Button);
  const QColor border = pal.color(QPalette::Mid);
  const QColor text = pal.color(QPalette::ButtonText);

  button->setStyleSheet(QStringLiteral(
                            "QPushButton#dialKeyBtn {"
                            "  background-color: %1;"
                            "  color: %2;"
                            "  border: 2px solid %3;"
                            "  border-radius: 26px;"
                            "  font-size: %4px;"
                            "  font-weight: bold;"
                            "  padding: 0;"
                            "}"
                            "QPushButton#dialKeyBtn:hover {"
                            "  background-color: %1;"
                            "  color: %2;"
                            "  border: 2px solid %5;"
                            "}"
                            "QPushButton#dialKeyBtn:pressed {"
                            "  background-color: %6;"
                            "  color: %7;"
                            "  border: 2px solid %6;"
                            "}"
                            "QPushButton#dialKeyBtn:disabled {"
                            "  color: %8;"
                            "  background-color: %9;"
                            "  border: 2px solid %3;"
                            "}")
                            .arg(colorCss(base),
                                 text.name(),
                                 border.name(),
                                 backspace ? QStringLiteral("18") : QStringLiteral("24"),
                                 accent.name(),
                                 colorCss(accent),
                                 accentText.name(),
                                 pal.color(QPalette::Disabled, QPalette::ButtonText).name(),
                                 colorCss(pal.color(QPalette::Disabled, QPalette::Button))));
}

void DialKeypadWidget::updateHoldVisual()
{
  if (!m_holdBtn) {
    return;
  }

  if (m_holdVisual == HoldVisual::None) {
    applyButtonStyle(m_holdBtn, m_holdSecondaryStyle);
    return;
  }

  const QPalette pal = QApplication::palette(this);
  const QColor accent = pal.color(QPalette::Highlight);
  const QColor accentText = pal.color(QPalette::HighlightedText);
  const QColor base = m_holdSecondaryStyle ? pal.color(QPalette::Midlight) : pal.color(QPalette::Button);
  const QColor border = pal.color(QPalette::Mid);
  const QColor text = pal.color(QPalette::ButtonText);

  QColor background = base;
  QColor foreground = text;
  QColor borderColor = border;

  if (m_holdVisual == HoldVisual::SolidAccent) {
    background = accent;
    foreground = accentText;
    borderColor = accent;
  } else {
    background = blendColors(base, accent, m_holdFillProgress);
    foreground = blendColors(text, accentText, m_holdFillProgress);
    borderColor = blendColors(border, accent, m_holdFillProgress);
  }

  const int fontSize = m_holdSecondaryStyle ? 18 : 24;
  const QString style = QStringLiteral(
                            "QPushButton#dialKeyBtn {"
                            "  background-color: %1;"
                            "  color: %2;"
                            "  border: 2px solid %3;"
                            "  border-radius: 26px;"
                            "  font-size: %4px;"
                            "  font-weight: bold;"
                            "  padding: 0;"
                            "}"
                            "QPushButton#dialKeyBtn:hover {"
                            "  background-color: %1;"
                            "  color: %2;"
                            "  border: 2px solid %3;"
                            "}"
                            "QPushButton#dialKeyBtn:pressed {"
                            "  background-color: %1;"
                            "  color: %2;"
                            "  border: 2px solid %3;"
                            "}")
                            .arg(colorCss(background), foreground.name(), borderColor.name())
                            .arg(fontSize);
  m_holdBtn->setStyleSheet(style);
}

void DialKeypadWidget::startHold(QPushButton *button, bool secondaryStyle, bool clearOnHold)
{
  m_holdBtn = button;
  m_holdSecondaryStyle = secondaryStyle;
  m_holdClearOnHold = clearOnHold;
  m_holdActionDone = false;
  m_holdVisual = HoldVisual::SolidAccent;
  m_holdFillProgress = 0.0;
  updateHoldVisual();
  m_holdTimer->start();
  m_holdPhaseTimer->start();
}

void DialKeypadWidget::endHold()
{
  m_holdTimer->stop();
  m_holdPhaseTimer->stop();
  m_holdProgressTimer->stop();
  m_holdVisual = HoldVisual::None;
  m_holdFillProgress = 0.0;
  if (m_holdBtn) {
    applyButtonStyle(m_holdBtn, m_holdSecondaryStyle);
  }
  m_holdBtn = nullptr;
}

void DialKeypadWidget::appendChar(const QString &character)
{
  if (character.isEmpty()) {
    return;
  }
  if (m_dtmfMode) {
    emit digitPressed(character);
    return;
  }
  if (!m_edit) {
    return;
  }

  m_edit->insert(character);
  m_edit->setFocus();
}

void DialKeypadWidget::onBackspacePressed()
{
  if (m_dtmfMode || !m_backspaceBtn) {
    return;
  }
  startHold(m_backspaceBtn, true, true);
}

void DialKeypadWidget::onBackspaceReleased()
{
  if (m_dtmfMode) {
    return;
  }
  endHold();
}

void DialKeypadWidget::onBackspaceClicked()
{
  if (m_dtmfMode) {
    emit digitPressed(QStringLiteral("*"));
    return;
  }
  if (m_holdActionDone) {
    m_holdActionDone = false;
    return;
  }
  onBackspace();
}

void DialKeypadWidget::onZeroPressed()
{
  if (m_dtmfMode || !m_zeroBtn) {
    return;
  }
  startHold(m_zeroBtn, false, false);
}

void DialKeypadWidget::onZeroReleased()
{
  if (m_dtmfMode) {
    return;
  }
  endHold();
}

void DialKeypadWidget::onZeroClicked()
{
  if (m_dtmfMode) {
    emit digitPressed(QStringLiteral("0"));
    return;
  }
  if (m_holdActionDone) {
    m_holdActionDone = false;
    return;
  }
  appendChar(QStringLiteral("0"));
}

void DialKeypadWidget::onHoldPhaseTimeout()
{
  m_holdVisual = HoldVisual::Filling;
  m_holdFillProgress = 0.0;
  updateHoldVisual();
  m_holdProgressTimer->start();
}

void DialKeypadWidget::onHoldProgressTick()
{
  m_holdFillProgress += static_cast<qreal>(kHoldProgressTickMs) / static_cast<qreal>(kHoldProgressMs);
  if (m_holdFillProgress >= 1.0) {
    m_holdFillProgress = 1.0;
    m_holdProgressTimer->stop();
  }
  updateHoldVisual();
}

void DialKeypadWidget::onHoldTimeout()
{
  m_holdProgressTimer->stop();
  m_holdVisual = HoldVisual::SolidAccent;
  m_holdFillProgress = 1.0;
  updateHoldVisual();

  if (m_dtmfMode) {
    return;
  }

  if (m_holdClearOnHold) {
    if (!m_edit) {
      return;
    }
    m_edit->clear();
    m_edit->setFocus();
  } else {
    appendChar(QStringLiteral("+"));
  }
  m_holdActionDone = true;
}

void DialKeypadWidget::onBackspace()
{
  if (!m_edit) {
    return;
  }

  const int cursor = m_edit->cursorPosition();
  if (cursor <= 0) {
    return;
  }

  m_edit->setText(m_edit->text().left(cursor - 1) + m_edit->text().mid(cursor));
  m_edit->setCursorPosition(cursor - 1);
  m_edit->setFocus();
}
