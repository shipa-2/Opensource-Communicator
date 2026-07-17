#include "CallWindow.h"

#include "DialKeypadWidget.h"
#include "ui/StyleHelper.h"

#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QTextEdit>
#include <QTimer>
#include <QVBoxLayout>
#include <QSignalBlocker>

CallWindow::CallWindow(QWidget *parent)
    : QDialog(parent)
{
  setObjectName(QStringLiteral("callWindow"));
  setWindowFlags(Qt::Dialog | Qt::WindowTitleHint | Qt::WindowCloseButtonHint);
  setModal(false);
  resize(kNormalWidth, kNormalHeight);
  buildUi();

  m_durationTimer = new QTimer(this);
  connect(m_durationTimer, &QTimer::timeout, this, &CallWindow::onTimerTick);

  itl::applyDialogStyle(this);
}

namespace {
QString avatarLetter(const QString &displayName)
{
  const QString trimmed = displayName.trimmed();
  if (trimmed.isEmpty()) {
    return QStringLiteral("?");
  }
  return trimmed.left(1).toUpper();
}

class CallAvatarWidget : public QWidget {
public:
  explicit CallAvatarWidget(QWidget *parent = nullptr)
      : QWidget(parent)
  {
    setObjectName(QStringLiteral("callAvatar"));
    setFixedSize(140, 140);
    setAttribute(Qt::WA_StyledBackground, false);
  }

  void setLetter(const QString &letter)
  {
    m_letter = letter.isEmpty() ? QStringLiteral("?") : letter;
    update();
  }

  void setBaseColor(const QString &color)
  {
    m_baseColor = color;
    update();
  }

  void setPhoto(const QPixmap &photo)
  {
    m_photo = photo;
    update();
  }

  void setSpeaking(bool speaking)
  {
    if (m_speaking == speaking) {
      return;
    }
    m_speaking = speaking;
    update();
  }

protected:
  void paintEvent(QPaintEvent *) override
  {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

    const QRectF outer = QRectF(1.0, 1.0, width() - 2.0, height() - 2.0);

    if (!m_photo.isNull()) {
      const QColor bg = m_baseColor.isEmpty() ? palette().color(QPalette::Midlight) : QColor(m_baseColor);
      painter.setPen(Qt::NoPen);
      painter.setBrush(bg);
      painter.drawEllipse(outer);

      QPainterPath clip;
      clip.addEllipse(outer);
      painter.setClipPath(clip);
      const QPixmap scaled =
          m_photo.scaled(outer.size().toSize(), Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
      const QPointF topLeft(outer.center().x() - scaled.width() / 2.0,
                            outer.center().y() - scaled.height() / 2.0);
      painter.drawPixmap(topLeft, scaled);
      painter.setClipping(false);
    } else {
      const QColor bg = m_baseColor.isEmpty() ? palette().color(QPalette::Midlight) : QColor(m_baseColor);
      painter.setPen(Qt::NoPen);
      painter.setBrush(bg);
      painter.drawEllipse(outer);
      painter.setPen(palette().color(QPalette::WindowText));
      QFont font = painter.font();
      font.setPixelSize(64);
      font.setBold(true);
      painter.setFont(font);
      painter.drawText(outer, Qt::AlignCenter, m_letter);
    }

    if (!m_speaking) {
      painter.setPen(QPen(palette().color(QPalette::Mid), 2.0));
      painter.setBrush(Qt::NoBrush);
      painter.drawEllipse(outer);
      return;
    }

    const QColor accent = palette().color(QPalette::Highlight);
    QPen glowPen(accent, 4.0);
    glowPen.setJoinStyle(Qt::RoundJoin);
    painter.setPen(glowPen);
    painter.setBrush(Qt::NoBrush);
    painter.drawEllipse(outer);

    QColor soft = accent;
    soft.setAlpha(70);
    QPen softPen(soft, 8.0);
    softPen.setJoinStyle(Qt::RoundJoin);
    painter.setPen(softPen);
    painter.drawEllipse(outer);
  }

private:
  QString m_letter = QStringLiteral("?");
  QString m_baseColor;
  QPixmap m_photo;
  bool m_speaking = false;
};

CallAvatarWidget *callAvatarWidget(QWidget *widget)
{
  return static_cast<CallAvatarWidget *>(widget);
}
} // namespace

void CallWindow::refreshAppearance()
{
  itl::refreshDialogStyle(this);
  if (m_dtmfKeypad) {
    m_dtmfKeypad->refreshAppearance();
  }
}

void CallWindow::buildUi()
{
  auto *root = new QVBoxLayout(this);
  root->setContentsMargins(16, 16, 16, 16);
  root->setSpacing(12);

  m_nameLabel = new QLabel;
  m_nameLabel->setObjectName(QStringLiteral("callNameLabel"));
  m_nameLabel->setAlignment(Qt::AlignCenter);
  QFont nameFont = m_nameLabel->font();
  nameFont.setPixelSize(18);
  nameFont.setBold(true);
  m_nameLabel->setFont(nameFont);
  root->addWidget(m_nameLabel);

  m_detailLabel = new QLabel;
  m_detailLabel->setObjectName(QStringLiteral("callDetailLabel"));
  m_detailLabel->setAlignment(Qt::AlignCenter);
  m_detailLabel->setWordWrap(true);
  root->addWidget(m_detailLabel);

  auto *avatarRow = new QHBoxLayout;
  avatarRow->addStretch();
  m_avatar = new CallAvatarWidget(this);
  avatarRow->addWidget(m_avatar);
  avatarRow->addStretch();
  root->addLayout(avatarRow);

  m_statusLabel = new QLabel;
  m_statusLabel->setObjectName(QStringLiteral("callStatusLabel"));
  m_statusLabel->setAlignment(Qt::AlignCenter);
  QFont statusFont = m_statusLabel->font();
  statusFont.setPixelSize(14);
  m_statusLabel->setFont(statusFont);
  root->addWidget(m_statusLabel);

  m_timerLabel = new QLabel;
  m_timerLabel->setObjectName(QStringLiteral("callTimerLabel"));
  m_timerLabel->setAlignment(Qt::AlignCenter);
  QFont timerFont = m_timerLabel->font();
  timerFont.setPixelSize(14);
  m_timerLabel->setFont(timerFont);
  m_timerLabel->setFixedHeight(m_timerLabel->fontMetrics().height() + 4);
  root->addWidget(m_timerLabel);

  m_notesEdit = new QTextEdit;
  m_notesEdit->setPlaceholderText(tr("Заметка по этому абоненту..."));
  m_notesEdit->setFixedHeight(120);
  root->addWidget(m_notesEdit);

  auto *buttonRow = new QHBoxLayout;
  buttonRow->setSpacing(8);

  m_transferBtn = new QPushButton(tr("Перевод"));
  m_transferBtn->setObjectName(QStringLiteral("callActionBtn"));
  buttonRow->addWidget(m_transferBtn);

  m_hangupBtn = new QPushButton(tr("Сброс"));
  m_hangupBtn->setObjectName(QStringLiteral("callHangupBtn"));
  buttonRow->addWidget(m_hangupBtn);

  m_holdBtn = new QPushButton(tr("Удержание"));
  m_holdBtn->setObjectName(QStringLiteral("callActionBtn"));
  buttonRow->addWidget(m_holdBtn);

  root->addLayout(buttonRow);

  m_dtmfToggleBtn = new QPushButton(tr("Тон"));
  m_dtmfToggleBtn->setObjectName(QStringLiteral("callActionBtn"));
  m_dtmfToggleBtn->setCheckable(true);
  m_dtmfToggleBtn->setVisible(false);
  root->addWidget(m_dtmfToggleBtn);

  m_answerBtn = new QPushButton(tr("Ответить"));
  m_answerBtn->setObjectName(QStringLiteral("callAnswerBtn"));
  m_answerBtn->setVisible(false);
  root->addWidget(m_answerBtn);

  m_dtmfPanel = new QWidget;
  auto *dtmfLayout = new QVBoxLayout(m_dtmfPanel);
  dtmfLayout->setContentsMargins(0, 0, 0, 0);
  dtmfLayout->setSpacing(6);
  m_dtmfEdit = new QLineEdit;
  m_dtmfEdit->setObjectName(QStringLiteral("callDtmfEdit"));
  m_dtmfEdit->setAlignment(Qt::AlignCenter);
  m_dtmfEdit->setPlaceholderText(tr("Введите тон..."));
  m_dtmfEdit->setMaxLength(128);
  m_dtmfEdit->setValidator(
      new QRegularExpressionValidator(QRegularExpression(QStringLiteral("[0-9*#]*")), m_dtmfEdit));
  dtmfLayout->addWidget(m_dtmfEdit);
  m_dtmfKeypad = new DialKeypadWidget(m_dtmfPanel);
  m_dtmfKeypad->setDtmfMode(true);
  dtmfLayout->addWidget(m_dtmfKeypad);
  m_dtmfPanel->setVisible(false);
  m_dtmfPanel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Ignored);
  m_dtmfPanel->setMaximumHeight(0);
  root->addWidget(m_dtmfPanel);

  connect(m_hangupBtn, &QPushButton::clicked, this, &CallWindow::hangupRequested);
  connect(m_answerBtn, &QPushButton::clicked, this, &CallWindow::answerRequested);
  connect(m_holdBtn, &QPushButton::clicked, this, &CallWindow::holdRequested);
  connect(m_notesEdit, &QTextEdit::textChanged, this, [this]() {
    if (!m_peer.isEmpty()) {
      emit notesChanged(m_peer, m_notesEdit->toPlainText());
    }
  });
  connect(m_transferBtn, &QPushButton::clicked, this, &CallWindow::transferRequested);
  connect(m_dtmfToggleBtn, &QPushButton::toggled, this, &CallWindow::setDtmfPanelVisible);
  connect(m_dtmfKeypad, &DialKeypadWidget::digitPressed, this, &CallWindow::sendDtmfDigit);
  connect(m_dtmfEdit, &QLineEdit::textEdited, this, [this](const QString &text) {
    int commonPrefix = 0;
    while (commonPrefix < m_dtmfSent.size() && commonPrefix < text.size()
           && m_dtmfSent.at(commonPrefix) == text.at(commonPrefix)) {
      ++commonPrefix;
    }
    for (int i = commonPrefix; i < text.size(); ++i) {
      emit dtmfRequested(QString(text.at(i)));
    }
    m_dtmfSent = text;
  });
}

void CallWindow::applyFixedCallWidth()
{
  setMinimumWidth(kNormalWidth);
  setMaximumWidth(kNormalWidth);
}

void CallWindow::updateCollapsedMinimumHeight()
{
  if (m_dtmfExpanded || !layout()) {
    return;
  }

  layout()->activate();
  m_minCollapsedHeight = qMax(kNormalHeight, layout()->minimumSize().height());
  setMinimumHeight(m_minCollapsedHeight);
  if (height() < m_minCollapsedHeight) {
    resize(kNormalWidth, m_minCollapsedHeight);
  }
}

void CallWindow::resetCallWindowLayout()
{
  m_dtmfExpanded = false;
  m_minCollapsedHeight = kNormalHeight;
  m_collapsedHeight = kNormalHeight;
  if (m_dtmfPanel) {
    m_dtmfPanel->hide();
    m_dtmfPanel->setFixedHeight(0);
    m_dtmfPanel->setMaximumHeight(0);
    m_dtmfPanel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Ignored);
  }
  if (m_dtmfToggleBtn) {
    const QSignalBlocker blocker(m_dtmfToggleBtn);
    m_dtmfToggleBtn->setChecked(false);
  }
  if (m_dtmfEdit) {
    m_dtmfEdit->clear();
  }
  setMinimumWidth(0);
  setMaximumWidth(QWIDGETSIZE_MAX);
  setMinimumHeight(0);
  setMaximumHeight(QWIDGETSIZE_MAX);
  resize(kNormalWidth, kNormalHeight);
}

void CallWindow::setMode(Mode mode)
{
  m_mode = mode;
  const bool activeCall = mode == Mode::Active || mode == Mode::IncomingAccepted;
  const bool outgoingCall = mode == Mode::Outgoing;
  const bool showCallControls = activeCall || outgoingCall;

  m_answerBtn->setVisible(mode == Mode::Incoming);
  m_answerBtn->setEnabled(mode == Mode::Incoming);
  m_hangupBtn->setEnabled(mode != Mode::Hidden);
  m_holdBtn->setVisible(showCallControls);
  m_transferBtn->setVisible(showCallControls);
  m_dtmfToggleBtn->setVisible(showCallControls);
  m_timerLabel->setVisible(showCallControls);
  m_holdBtn->setEnabled(activeCall);
  m_transferBtn->setEnabled(activeCall);
  m_dtmfToggleBtn->setEnabled(activeCall);
  m_dtmfEnabled = activeCall;

  if (activeCall) {
    applyFixedCallWidth();
  }

  if (!activeCall) {
    m_dtmfSent.clear();
    resetCallWindowLayout();
  }

  if (showCallControls) {
    updateCollapsedMinimumHeight();
  }
}

void CallWindow::reject()
{
  if (m_mode != Mode::Hidden) {
    emit hangupRequested();
    return;
  }

  QDialog::reject();
}

void CallWindow::keyPressEvent(QKeyEvent *event)
{
  if (event->key() == Qt::Key_Escape) {
    event->accept();
    return;
  }

  if (m_dtmfEnabled) {
    QString digit;
    switch (event->key()) {
    case Qt::Key_0:
    case Qt::Key_1:
    case Qt::Key_2:
    case Qt::Key_3:
    case Qt::Key_4:
    case Qt::Key_5:
    case Qt::Key_6:
    case Qt::Key_7:
    case Qt::Key_8:
    case Qt::Key_9:
      digit = QString::number(event->key() - Qt::Key_0);
      break;
    case Qt::Key_Asterisk:
    case Qt::Key_multiply:
      digit = QStringLiteral("*");
      break;
    case Qt::Key_NumberSign:
      digit = QStringLiteral("#");
      break;
    default:
      break;
    }
    if (!digit.isEmpty()) {
      sendDtmfDigit(digit);
      event->accept();
      return;
    }
  }

  QDialog::keyPressEvent(event);
}

void CallWindow::setDtmfPanelVisible(bool visible)
{
  const bool showPanel = visible && m_dtmfEnabled;
  if (showPanel && !m_dtmfExpanded) {
    updateCollapsedMinimumHeight();
    m_collapsedHeight = qMax(height(), m_minCollapsedHeight);
  }

  if (m_dtmfPanel) {
    if (showPanel) {
      setMinimumHeight(0);
      setMaximumHeight(QWIDGETSIZE_MAX);
      m_dtmfPanel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
      m_dtmfPanel->setMinimumHeight(0);
      m_dtmfPanel->setMaximumHeight(QWIDGETSIZE_MAX);
      m_dtmfPanel->show();
    } else {
      m_dtmfPanel->hide();
      m_dtmfPanel->setFixedHeight(0);
      m_dtmfPanel->setMaximumHeight(0);
      m_dtmfPanel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Ignored);
    }
  }
  if (m_dtmfToggleBtn && m_dtmfToggleBtn->isChecked() != visible) {
    const QSignalBlocker blocker(m_dtmfToggleBtn);
    m_dtmfToggleBtn->setChecked(visible);
  }
  if (showPanel && m_dtmfKeypad) {
    m_dtmfKeypad->refreshAppearance();
  }
  updateWindowHeightForDtmf(showPanel);
  if (showPanel && m_dtmfEdit) {
    m_dtmfEdit->setFocus(Qt::OtherFocusReason);
  }
}

void CallWindow::updateWindowHeightForDtmf(bool expanded)
{
  applyFixedCallWidth();

  if (expanded) {
    m_dtmfExpanded = true;
    setMinimumHeight(0);
    setMaximumHeight(QWIDGETSIZE_MAX);
    if (layout()) {
      layout()->activate();
    }
    QTimer::singleShot(0, this, [this]() {
      if (!m_dtmfExpanded || !m_dtmfPanel) {
        return;
      }
      m_dtmfPanel->adjustSize();
      const int panelHeight = m_dtmfPanel->sizeHint().height();
      resize(kNormalWidth, m_collapsedHeight + panelHeight + layout()->spacing());
    });
    return;
  }

  m_dtmfExpanded = false;
  const int targetHeight = qMax(m_collapsedHeight, m_minCollapsedHeight);
  if (layout()) {
    layout()->activate();
  }
  QTimer::singleShot(0, this, [this, targetHeight]() {
    applyFixedCallWidth();
    setFixedHeight(targetHeight);
    QTimer::singleShot(0, this, [this, targetHeight]() {
      setMinimumHeight(m_minCollapsedHeight);
      setMaximumHeight(QWIDGETSIZE_MAX);
      resize(kNormalWidth, targetHeight);
    });
  });
}

void CallWindow::sendDtmfDigit(const QString &digit)
{
  if (!m_dtmfEnabled || digit.isEmpty()) {
    return;
  }
  appendDtmfDigit(digit);
  emit dtmfRequested(digit);
}

void CallWindow::appendDtmfDigit(const QString &digit)
{
  m_dtmfSent.append(digit);
  if (m_dtmfEdit) {
    const QSignalBlocker blocker(m_dtmfEdit);
    m_dtmfEdit->setText(m_dtmfSent);
    m_dtmfEdit->setCursorPosition(m_dtmfEdit->text().size());
  }
}

void CallWindow::setNotesText(const QString &text)
{
  const QSignalBlocker blocker(m_notesEdit);
  m_notesEdit->setPlainText(text);
}

QString CallWindow::notesText() const
{
  return m_notesEdit->toPlainText();
}

void CallWindow::setNotesVisible(bool visible)
{
  m_notesEdit->setVisible(visible);
}

void CallWindow::setAvatarColor(const QString &color)
{
  m_avatarBaseColor = color;
  resetAudioLevel();
  refreshAvatarBorder();
}

void CallWindow::setAvatarLetter(const QString &displayName)
{
  m_avatarLetter = avatarLetter(displayName);
  refreshAvatarContent();
}

void CallWindow::setAvatarPixmap(const QPixmap &pixmap)
{
  m_avatarPhoto = pixmap;
  refreshAvatarContent();
}

void CallWindow::setRemoteSpeakingIndicator(bool speaking)
{
  m_calibrated = true;
  m_speaking = speaking;
  if (CallAvatarWidget *avatar = callAvatarWidget(m_avatar)) {
    avatar->setSpeaking(speaking);
  }
}

void CallWindow::resetAudioLevel()
{
  m_calibrationSamples = 0;
  m_calibrationSum = 0;
  m_calibrated = false;
  m_speaking = false;
  m_noiseFloor = 0.005f;
  m_speechThreshold = 0.02f;
  if (CallAvatarWidget *avatar = callAvatarWidget(m_avatar)) {
    avatar->setSpeaking(false);
  }
}

void CallWindow::refreshAvatarContent()
{
  CallAvatarWidget *avatar = callAvatarWidget(m_avatar);
  if (!avatar) {
    return;
  }

  avatar->setLetter(m_avatarLetter);
  avatar->setBaseColor(m_avatarBaseColor);
  avatar->setPhoto(m_avatarPhoto);
  avatar->setSpeaking(m_speaking);
}

void CallWindow::refreshAvatarBorder()
{
  if (CallAvatarWidget *avatar = callAvatarWidget(m_avatar)) {
    avatar->setSpeaking(m_speaking);
  }
}

void CallWindow::updateRemoteAudioLevel(float level)
{
  if (m_mode != Mode::Active && m_mode != Mode::IncomingAccepted) {
    return;
  }

  if (!m_calibrated) {
    m_calibrationSamples++;
    m_calibrationSum += level;
    if (m_calibrationSamples >= 50) {
      m_noiseFloor = m_calibrationSum / m_calibrationSamples;
      m_speechThreshold = m_noiseFloor * 4.0f;
      if (m_speechThreshold < 0.01f) {
        m_speechThreshold = 0.01f;
      }
      m_calibrated = true;
    }
    return;
  }

  const bool nowSpeaking = level > m_speechThreshold;
  if (nowSpeaking != m_speaking) {
    m_speaking = nowSpeaking;
    refreshAvatarBorder();
  }
}

void CallWindow::showOutgoing(const QString &peer, const QString &displayName, const QString &detail)
{
  setAttribute(Qt::WA_ShowWithoutActivating, false);
  resetCallWindowLayout();
  m_peer = peer;
  m_displayName = displayName;
  setWindowTitle(tr("%1 — дозвон").arg(displayName));
  m_nameLabel->setText(displayName);
  m_detailLabel->setText(detail);
  m_statusLabel->setText(tr("Дозвон"));
  m_timerLabel->clear();
  setAvatarLetter(displayName);
  setMode(Mode::Outgoing);
  stopTimer();
  applyFixedCallWidth();
  resize(kNormalWidth, kNormalHeight);
  show();
  raise();
  activateWindow();
}

void CallWindow::showIncoming(const QString &peer, const QString &displayName, const QString &detail)
{
  setAttribute(Qt::WA_ShowWithoutActivating, true);
  resetCallWindowLayout();
  m_peer = peer;
  m_displayName = displayName;
  setWindowTitle(tr("Входящий: %1").arg(displayName));
  m_nameLabel->setText(displayName);
  m_detailLabel->setText(detail);
  m_statusLabel->setText(tr("Входящий"));
  m_timerLabel->clear();
  setAvatarLetter(displayName);
  setMode(Mode::Incoming);
  stopTimer();
  applyFixedCallWidth();
  resize(kNormalWidth, height() > 0 ? height() : kNormalHeight);
  show();
  raise();
}

void CallWindow::showActive(const QString &peer, const QString &displayName)
{
  setAttribute(Qt::WA_ShowWithoutActivating, false);
  m_peer = peer;
  m_displayName = displayName;
  setWindowTitle(tr("%1 — разговор").arg(displayName));
  m_nameLabel->setText(displayName);
  m_statusLabel->setText(tr("Разговор"));
  setAvatarLetter(displayName);
  setMode(Mode::Active);
  stopTimer();
  applyFixedCallWidth();
  resize(kNormalWidth, m_dtmfExpanded ? height() : m_collapsedHeight);
  show();
}

void CallWindow::updateState(const QString &state, const QString &detail)
{
  if (state == QStringLiteral("connecting") || state == QStringLiteral("dialing")) {
    showOutgoing(m_peer.isEmpty() ? detail : m_peer, detail, m_peer);
    m_statusLabel->setText(tr("Дозвон"));
    return;
  }
  if (state == QStringLiteral("ringing")) {
    m_statusLabel->setText(tr("Вызов на удалённой стороне..."));
    return;
  }
  if (state == QStringLiteral("incoming")) {
    showIncoming(m_peer.isEmpty() ? detail : m_peer, detail, {});
    return;
  }
  if (state == QStringLiteral("connected") || state == QStringLiteral("accepting")
      || state == QStringLiteral("media")) {
    if (state == QStringLiteral("connected")) {
      if (!detail.isEmpty()) {
        m_displayName = detail;
        m_nameLabel->setText(detail);
        setAvatarLetter(detail);
      }
      m_statusLabel->setText(tr("Разговор"));
      setMode(Mode::Active);
      if (m_timerLabel) {
        m_timerLabel->setText(tr("Соединение..."));
        m_timerLabel->setVisible(true);
      }
      if (!isVisible()) {
        show();
      }
    }
    return;
  }
  if (state == QStringLiteral("hold")) {
    m_statusLabel->setText(tr("Удержание"));
    return;
  }
  if (state == QStringLiteral("resumed")) {
    m_statusLabel->setText(tr("Разговор"));
    return;
  }
  if (state == QStringLiteral("ended") || state == QStringLiteral("rejected")
      || state == QStringLiteral("error")) {
    closeCall();
  }
}

void CallWindow::beginConversationTimer()
{
  if (m_durationTimer->isActive()) {
    return;
  }
  startTimer();
}

void CallWindow::closeCall()
{
  stopTimer();
  setRemoteSpeakingIndicator(false);
  m_avatarPhoto = {};
  m_avatarLetter = QStringLiteral("?");
  refreshAvatarContent();
  m_dtmfSent.clear();
  hide();
  setMode(Mode::Hidden);
  resetCallWindowLayout();
}

void CallWindow::startTimer()
{
  m_elapsedSeconds = 0;
  m_timerLabel->setText(tr("Продолжит: %1").arg(formatDuration(0)));
  m_durationTimer->start(1000);
}

void CallWindow::stopTimer()
{
  m_durationTimer->stop();
  m_elapsedSeconds = 0;
}

void CallWindow::onTimerTick()
{
  ++m_elapsedSeconds;
  m_timerLabel->setText(tr("Продолжит: %1").arg(formatDuration(m_elapsedSeconds)));
}

QString CallWindow::formatDuration(int seconds)
{
  return QStringLiteral("%1:%2")
      .arg(seconds / 60, 2, 10, QLatin1Char('0'))
      .arg(seconds % 60, 2, 10, QLatin1Char('0'));
}
