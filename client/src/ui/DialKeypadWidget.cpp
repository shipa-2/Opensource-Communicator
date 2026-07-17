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

constexpr int kBackspaceHoldClearMs = 1000;
constexpr int kBackspaceHoldAccentMs = 500;
constexpr int kBackspaceHoldProgressMs = kBackspaceHoldClearMs - kBackspaceHoldAccentMs;
constexpr int kBackspaceHoldProgressTickMs = 16;

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
    {"0", 3, 1},
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
    } else {
      button->setText(QString::fromUtf8(key.label));
      const QString digit = QString::fromUtf8(key.label);
      connect(button, &QPushButton::clicked, this, [this, digit]() { appendChar(digit); });
      applyButtonStyle(button, false);
    }

    m_grid->addWidget(button, key.row, key.col);
    m_keys.append(button);
  }

  m_backspaceHoldTimer = new QTimer(this);
  m_backspaceHoldTimer->setSingleShot(true);
  m_backspaceHoldTimer->setInterval(kBackspaceHoldClearMs);
  connect(m_backspaceHoldTimer, &QTimer::timeout, this, &DialKeypadWidget::onBackspaceHoldTimeout);

  m_backspaceHoldPhaseTimer = new QTimer(this);
  m_backspaceHoldPhaseTimer->setSingleShot(true);
  m_backspaceHoldPhaseTimer->setInterval(kBackspaceHoldAccentMs);
  connect(m_backspaceHoldPhaseTimer, &QTimer::timeout, this, &DialKeypadWidget::onBackspaceHoldPhaseTimeout);

  m_backspaceHoldProgressTimer = new QTimer(this);
  m_backspaceHoldProgressTimer->setInterval(kBackspaceHoldProgressTickMs);
  connect(m_backspaceHoldProgressTimer, &QTimer::timeout, this, &DialKeypadWidget::onBackspaceHoldProgressTick);
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
    if (button == m_backspaceBtn && m_backspaceHoldVisual != BackspaceHoldVisual::None) {
      updateBackspaceHoldVisual();
      continue;
    }
    const bool backspace = button == m_backspaceBtn;
    applyButtonStyle(button, backspace);
  }
}

void DialKeypadWidget::changeEvent(QEvent *event)
{
  QWidget::changeEvent(event);
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
                            .arg(base.name(),
                                 text.name(),
                                 border.name(),
                                 backspace ? QStringLiteral("18") : QStringLiteral("24"),
                                 accent.name(),
                                 accent.name(),
                                 accentText.name(),
                                 pal.color(QPalette::Disabled, QPalette::ButtonText).name(),
                                 pal.color(QPalette::Disabled, QPalette::Button).name()));
}

void DialKeypadWidget::updateBackspaceHoldVisual()
{
  if (!m_backspaceBtn) {
    return;
  }

  if (m_backspaceHoldVisual == BackspaceHoldVisual::None) {
    applyButtonStyle(m_backspaceBtn, true);
    return;
  }

  const QPalette pal = QApplication::palette(this);
  const QColor accent = pal.color(QPalette::Highlight);
  const QColor accentText = pal.color(QPalette::HighlightedText);
  const QColor base = pal.color(QPalette::Midlight);
  const QColor border = pal.color(QPalette::Mid);
  const QColor text = pal.color(QPalette::ButtonText);

  QColor background = base;
  QColor foreground = text;
  QColor borderColor = border;

  if (m_backspaceHoldVisual == BackspaceHoldVisual::SolidAccent) {
    background = accent;
    foreground = accentText;
    borderColor = accent;
  } else {
    background = blendColors(base, accent, m_backspaceFillProgress);
    foreground = blendColors(text, accentText, m_backspaceFillProgress);
    borderColor = blendColors(border, accent, m_backspaceFillProgress);
  }

  const QString style = QStringLiteral(
                            "QPushButton#dialKeyBtn {"
                            "  background-color: %1;"
                            "  color: %2;"
                            "  border: 2px solid %3;"
                            "  border-radius: 26px;"
                            "  font-size: 18px;"
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
                            .arg(background.name(), foreground.name(), borderColor.name());
  m_backspaceBtn->setStyleSheet(style);
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
  if (m_dtmfMode) {
    return;
  }
  m_backspaceHoldClearDone = false;
  m_backspaceHoldVisual = BackspaceHoldVisual::SolidAccent;
  m_backspaceFillProgress = 0.0;
  updateBackspaceHoldVisual();
  m_backspaceHoldTimer->start();
  m_backspaceHoldPhaseTimer->start();
}

void DialKeypadWidget::onBackspaceReleased()
{
  if (m_dtmfMode) {
    return;
  }
  m_backspaceHoldTimer->stop();
  m_backspaceHoldPhaseTimer->stop();
  m_backspaceHoldProgressTimer->stop();
  m_backspaceHoldVisual = BackspaceHoldVisual::None;
  m_backspaceFillProgress = 0.0;
  if (m_backspaceBtn) {
    applyButtonStyle(m_backspaceBtn, true);
  }
}

void DialKeypadWidget::onBackspaceClicked()
{
  if (m_dtmfMode) {
    emit digitPressed(QStringLiteral("*"));
    return;
  }
  if (m_backspaceHoldClearDone) {
    m_backspaceHoldClearDone = false;
    return;
  }
  onBackspace();
}

void DialKeypadWidget::onBackspaceHoldPhaseTimeout()
{
  m_backspaceHoldVisual = BackspaceHoldVisual::Filling;
  m_backspaceFillProgress = 0.0;
  updateBackspaceHoldVisual();
  m_backspaceHoldProgressTimer->start();
}

void DialKeypadWidget::onBackspaceHoldProgressTick()
{
  m_backspaceFillProgress += static_cast<qreal>(kBackspaceHoldProgressTickMs)
                             / static_cast<qreal>(kBackspaceHoldProgressMs);
  if (m_backspaceFillProgress >= 1.0) {
    m_backspaceFillProgress = 1.0;
    m_backspaceHoldProgressTimer->stop();
  }
  updateBackspaceHoldVisual();
}

void DialKeypadWidget::onBackspaceHoldTimeout()
{
  m_backspaceHoldProgressTimer->stop();
  m_backspaceHoldVisual = BackspaceHoldVisual::SolidAccent;
  m_backspaceFillProgress = 1.0;
  updateBackspaceHoldVisual();

  if (!m_edit) {
    return;
  }

  m_edit->clear();
  m_edit->setFocus();
  m_backspaceHoldClearDone = true;
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
