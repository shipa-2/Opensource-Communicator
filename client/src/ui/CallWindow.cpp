#include "CallWindow.h"

#include "ui/StyleHelper.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QTextEdit>
#include <QTimer>
#include <QVBoxLayout>
#include <QSignalBlocker>

namespace {
constexpr int kCompactHeight = 200;
} // namespace

CallWindow::CallWindow(QWidget *parent)
    : QDialog(parent)
{
  setObjectName(QStringLiteral("callWindow"));
  setWindowFlags(Qt::Dialog | Qt::WindowTitleHint | Qt::WindowCloseButtonHint);
  setModal(false);
  resize(520, kCompactHeight);
  buildUi();

  m_durationTimer = new QTimer(this);
  connect(m_durationTimer, &QTimer::timeout, this, &CallWindow::onTimerTick);

  itl::applyDialogStyle(this);
}

void CallWindow::refreshAppearance()
{
  itl::applyDialogStyle(this);
  update();
}

void CallWindow::buildUi()
{
  auto *root = new QVBoxLayout(this);
  root->setContentsMargins(12, 12, 12, 0);
  root->setSpacing(8);

  auto *infoRow = new QHBoxLayout;
  m_avatar = new QLabel(QStringLiteral("👤"));
  m_avatar->setObjectName(QStringLiteral("callAvatar"));
  m_avatar->setFixedSize(72, 72);
  m_avatar->setAlignment(Qt::AlignCenter);
  infoRow->addWidget(m_avatar);

  auto *textCol = new QVBoxLayout;
  m_nameLabel = new QLabel;
  m_nameLabel->setObjectName(QStringLiteral("callNameLabel"));
  textCol->addWidget(m_nameLabel);

  m_detailLabel = new QLabel;
  m_detailLabel->setObjectName(QStringLiteral("callDetailLabel"));
  m_detailLabel->setWordWrap(true);
  textCol->addWidget(m_detailLabel);
  textCol->addStretch();
  infoRow->addLayout(textCol, 1);

  auto *statusCol = new QVBoxLayout;
  m_statusLabel = new QLabel;
  m_statusLabel->setObjectName(QStringLiteral("callStatusIncoming"));
  statusCol->addWidget(m_statusLabel, 0, Qt::AlignRight);

  m_timerLabel = new QLabel;
  m_timerLabel->setObjectName(QStringLiteral("callStatusActive"));
  m_timerLabel->setAlignment(Qt::AlignRight);
  statusCol->addWidget(m_timerLabel);
  statusCol->addStretch();
  infoRow->addLayout(statusCol);
  root->addLayout(infoRow);

  m_notesEdit = new QTextEdit;
  m_notesEdit->setPlaceholderText(tr("Заметка по этому абоненту..."));
  m_notesEdit->setVisible(false);
  m_notesEdit->setMaximumHeight(120);
  root->addWidget(m_notesEdit);

  m_toolbar = new QWidget;
  m_toolbar->setObjectName(QStringLiteral("callToolbar"));
  auto *tb = new QHBoxLayout(m_toolbar);
  tb->setContentsMargins(12, 10, 12, 10);

  m_notesBtn = new QPushButton(QStringLiteral("📝"));
  m_notesBtn->setObjectName(QStringLiteral("callCtrlBtn"));
  m_notesBtn->setToolTip(tr("Заметки"));
  m_notesBtn->setCheckable(true);
  tb->addWidget(m_notesBtn);

  m_holdBtn = new QPushButton(QStringLiteral("⏸"));
  m_holdBtn->setObjectName(QStringLiteral("callCtrlBtn"));
  m_holdBtn->setToolTip(tr("Удержание"));
  tb->addWidget(m_holdBtn);

  m_transferBtn = new QPushButton(QStringLiteral("↪"));
  m_transferBtn->setObjectName(QStringLiteral("callCtrlBtn"));
  m_transferBtn->setToolTip(tr("Перевод"));
  tb->addWidget(m_transferBtn);

  tb->addStretch();

  m_answerBtn = new QPushButton(QStringLiteral("📞"));
  m_answerBtn->setObjectName(QStringLiteral("callAnswerBtn"));
  m_answerBtn->setToolTip(tr("Ответить"));
  tb->addWidget(m_answerBtn);

  m_hangupBtn = new QPushButton(QStringLiteral("📴"));
  m_hangupBtn->setObjectName(QStringLiteral("callHangupBtn"));
  m_hangupBtn->setToolTip(tr("Завершить"));
  tb->addWidget(m_hangupBtn);

  root->addWidget(m_toolbar);
  root->setSizeConstraint(QLayout::SetFixedSize);

  connect(m_hangupBtn, &QPushButton::clicked, this, &CallWindow::hangupRequested);
  connect(m_answerBtn, &QPushButton::clicked, this, &CallWindow::answerRequested);
  connect(m_holdBtn, &QPushButton::clicked, this, &CallWindow::holdRequested);
  connect(m_notesBtn, &QPushButton::clicked, this, [this](bool checked) { setNotesVisible(checked); });
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
  m_notesBtn->setVisible(mode != Mode::Hidden && mode != Mode::Incoming);
}

void CallWindow::setNotesVisible(bool visible)
{
  if (m_notesVisible == visible) {
    return;
  }

  m_notesVisible = visible;
  m_notesBtn->setChecked(visible);
  m_notesEdit->setVisible(visible);
  m_notesEdit->setMaximumHeight(visible ? 120 : 0);

  if (QLayout *rootLayout = layout()) {
    rootLayout->invalidate();
    rootLayout->activate();
  }

  const int windowWidth = width() > 0 ? width() : 520;
  adjustSize();
  resize(windowWidth, height());

  if (visible) {
    m_notesEdit->setFocus();
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

void CallWindow::setNotesText(const QString &text)
{
  const QSignalBlocker blocker(m_notesEdit);
  m_notesEdit->setPlainText(text);
}

QString CallWindow::notesText() const
{
  return m_notesEdit->toPlainText();
}

void CallWindow::showOutgoing(const QString &peer, const QString &displayName, const QString &detail)
{
  m_peer = peer;
  m_displayName = displayName;
  setWindowTitle(tr("%1 — дозвон").arg(displayName));
  m_nameLabel->setText(displayName);
  m_detailLabel->setText(detail);
  m_statusLabel->setText(tr("Дозвон"));
  m_statusLabel->setObjectName(QStringLiteral("callStatusIncoming"));
  m_timerLabel->clear();
  setNotesVisible(false);
  setMode(Mode::Outgoing);
  stopTimer();
  show();
  raise();
  activateWindow();
}

void CallWindow::showIncoming(const QString &peer, const QString &displayName, const QString &detail)
{
  m_peer = peer;
  m_displayName = displayName;
  setWindowTitle(tr("Входящий: %1").arg(displayName));
  m_nameLabel->setText(displayName);
  m_detailLabel->setText(detail);
  m_statusLabel->setText(tr("Входящий"));
  m_statusLabel->setObjectName(QStringLiteral("callStatusIncoming"));
  m_timerLabel->clear();
  setNotesVisible(false);
  setMode(Mode::Incoming);
  stopTimer();
  show();
  raise();
  activateWindow();
}

void CallWindow::showActive(const QString &peer, const QString &displayName)
{
  m_peer = peer;
  m_displayName = displayName;
  setWindowTitle(tr("%1 — разговор").arg(displayName));
  m_nameLabel->setText(displayName);
  m_statusLabel->setText(tr("Разговор"));
  m_statusLabel->setObjectName(QStringLiteral("callStatusActive"));
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
      }
      m_statusLabel->setText(tr("Разговор"));
      m_statusLabel->setObjectName(QStringLiteral("callStatusActive"));
      setMode(Mode::Active);
      // Duration timer starts later, when remote audio actually arrives.
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
  setNotesVisible(false);
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
