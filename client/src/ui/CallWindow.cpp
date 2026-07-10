#include "CallWindow.h"

#include "ui/StyleHelper.h"

#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QPushButton>
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
  resize(320, 480);
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
} // namespace

void CallWindow::refreshAppearance()
{
  itl::refreshDialogStyle(this);
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
  m_avatar = new QLabel(QStringLiteral("?"));
  m_avatar->setObjectName(QStringLiteral("callAvatar"));
  m_avatar->setFixedSize(140, 140);
  m_avatar->setAlignment(Qt::AlignCenter);
  QFont avatarFont = m_avatar->font();
  avatarFont.setPixelSize(64);
  m_avatar->setFont(avatarFont);
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
  root->addWidget(m_timerLabel);

  m_notesEdit = new QTextEdit;
  m_notesEdit->setPlaceholderText(tr("Заметка по этому абоненту..."));
  m_notesEdit->setMinimumHeight(80);
  m_notesEdit->setMaximumHeight(120);
  root->addWidget(m_notesEdit, 1);

  auto *buttonRow = new QHBoxLayout;
  buttonRow->setSpacing(8);

  m_transferBtn = new QPushButton(tr("Перевод"));
  m_transferBtn->setObjectName(QStringLiteral("callActionBtn"));
  m_transferBtn->setCheckable(false);
  buttonRow->addWidget(m_transferBtn);

  m_hangupBtn = new QPushButton(tr("Сброс"));
  m_hangupBtn->setObjectName(QStringLiteral("callHangupBtn"));
  buttonRow->addWidget(m_hangupBtn);

  m_holdBtn = new QPushButton(tr("Удержание"));
  m_holdBtn->setObjectName(QStringLiteral("callActionBtn"));
  buttonRow->addWidget(m_holdBtn);

  root->addLayout(buttonRow);

  m_answerBtn = new QPushButton(tr("Ответить"));
  m_answerBtn->setObjectName(QStringLiteral("callAnswerBtn"));
  m_answerBtn->setVisible(false);
  root->addWidget(m_answerBtn);

  connect(m_hangupBtn, &QPushButton::clicked, this, &CallWindow::hangupRequested);
  connect(m_answerBtn, &QPushButton::clicked, this, &CallWindow::answerRequested);
  connect(m_holdBtn, &QPushButton::clicked, this, &CallWindow::holdRequested);
  connect(m_notesEdit, &QTextEdit::textChanged, this, [this]() {
    if (!m_peer.isEmpty()) {
      emit notesChanged(m_peer, m_notesEdit->toPlainText());
    }
  });
  connect(m_transferBtn, &QPushButton::clicked, this, &CallWindow::transferRequested);
}

void CallWindow::setMode(Mode mode)
{
  m_mode = mode;
  m_answerBtn->setVisible(mode == Mode::Incoming);
  m_holdBtn->setVisible(mode == Mode::Active || mode == Mode::IncomingAccepted);
  m_transferBtn->setVisible(mode == Mode::Active || mode == Mode::IncomingAccepted);
  m_timerLabel->setVisible(mode == Mode::Active || mode == Mode::IncomingAccepted);
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
  // Escape may have been intended for the window that was active before an
  // incoming call appeared. Ending a call must require an explicit action.
  if (event->key() == Qt::Key_Escape) {
    event->accept();
    return;
  }

  QDialog::keyPressEvent(event);
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
  if (!m_avatar) {
    return;
  }
  m_avatar->setText(avatarLetter(displayName));
}

void CallWindow::setRemoteSpeakingIndicator(bool speaking)
{
  m_calibrated = true;
  if (m_speaking == speaking) {
    return;
  }
  m_speaking = speaking;
  refreshAvatarBorder();
}

void CallWindow::resetAudioLevel()
{
  m_calibrationSamples = 0;
  m_calibrationSum = 0;
  m_calibrated = false;
  m_speaking = false;
  m_noiseFloor = 0.005f;
  m_speechThreshold = 0.02f;
}

void CallWindow::refreshAvatarBorder()
{
  if (!m_avatar) {
    return;
  }
  const QColor borderColor = m_speaking ? palette().color(QPalette::Highlight) : palette().color(QPalette::Mid);
  const QString bg = m_avatarBaseColor.isEmpty() ? QStringLiteral("palette(midlight)") : m_avatarBaseColor;
  const QString border = m_speaking ? borderColor.name() : borderColor.name();
  m_avatar->setStyleSheet(
      QStringLiteral("QLabel {"
                     "  background-color: %1;"
                     "  color: palette(window-text);"
                     "  border-radius: 70px;"
                     "  border: 3px solid %2;"
                     "  font-size: 64px;"
                     "  font-weight: bold;"
                     "}")
          .arg(bg, border));
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
  show();
  raise();
  activateWindow();
}

void CallWindow::showIncoming(const QString &peer, const QString &displayName, const QString &detail)
{
  // Show the incoming call prominently, but preserve keyboard focus in the
  // application the user is currently working in.
  setAttribute(Qt::WA_ShowWithoutActivating, true);
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
  hide();
  setMode(Mode::Hidden);
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
