#include "ChatDialog.h"

#include "chat/ChatManager.h"
#include "protocol/CommunicatorClient.h"
#include "ui/StyleHelper.h"

#include <QDate>
#include <QLocale>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollBar>
#include <QVBoxLayout>

ChatDialog::ChatDialog(itl::CommunicatorClient *client, QWidget *parent)
    : QDialog(parent)
    , m_client(client)
{
  setWindowTitle(tr("Сообщение"));
  resize(420, 360);

  auto *root = new QVBoxLayout(this);
  m_view = new QPlainTextEdit;
  m_view->setReadOnly(true);
  root->addWidget(m_view, 1);

  auto *row = new QHBoxLayout;
  m_input = new QLineEdit;
  m_input->setPlaceholderText(tr("Введите сообщение..."));
  auto *sendBtn = new QPushButton(tr("Отправить"));
  row->addWidget(m_input, 1);
  row->addWidget(sendBtn);
  root->addLayout(row);

  connect(sendBtn, &QPushButton::clicked, this, &ChatDialog::onSend);
  connect(m_input, &QLineEdit::returnPressed, this, &ChatDialog::onSend);
  connect(m_client, &itl::CommunicatorClient::chatMessage, this, &ChatDialog::onChatMessage);
  connect(m_client->chat(), &itl::ChatManager::historyLoaded, this, &ChatDialog::onHistoryLoaded);

  itl::applyDialogStyle(this);
}

void ChatDialog::refreshAppearance()
{
  itl::refreshDialogStyle(this);
}

void ChatDialog::openForPeer(const QString &peer, const QString &peerDisplayName, const QString &selfDisplayName)
{
  m_peer = peer;
  m_peerDisplayName = shortDisplayName(peerDisplayName, peer.section(QLatin1Char('@'), 0, 0));
  const QString login = m_client->credentials().login.section(QLatin1Char('@'), 0, 0);
  m_selfDisplayName = shortDisplayName(selfDisplayName, login.isEmpty() ? tr("Я") : login);
  const bool smsPeer = m_client && m_client->chat()->isSmsPeer(peer);
  setWindowTitle(smsPeer ? tr("SMS — %1").arg(peerDisplayName) : tr("Чат — %1").arg(peerDisplayName));
  reloadMessages();
  m_client->chat()->loadHistory();
  show();
  raise();
  activateWindow();
  m_input->setFocus();
  m_client->chat()->markPeerRead(peer);
}

bool ChatDialog::isOpenForPeer(const QString &peer) const
{
  if (!isVisible() || peer.isEmpty() || !m_client) {
    return false;
  }
  return m_client->chat()->normalizedPeer(peer) == m_client->chat()->normalizedPeer(m_peer);
}

QString ChatDialog::shortDisplayName(const QString &fullName, const QString &fallback)
{
  QString name = fullName.trimmed();
  if (name.isEmpty()) {
    name = fallback.trimmed();
  }
  if (name.isEmpty()) {
    return {};
  }
  // "Денис Калинин" → "Денис"
  const int space = name.indexOf(QLatin1Char(' '));
  if (space > 0) {
    return name.left(space);
  }
  return name;
}

QString ChatDialog::formatTimestamp(const QDateTime &timestamp)
{
  if (!timestamp.isValid()) {
    return {};
  }

  const QDateTime local = timestamp.toLocalTime();
  const QString time = local.toString(QStringLiteral("HH:mm"));
  if (local.date() == QDate::currentDate()) {
    return time;
  }

  const QLocale locale(QLocale::Russian);
  // "янв." / "февр." — abbreviated month with trailing period, as in "29 янв."
  const QString dayMonth = locale.toString(local.date(), QStringLiteral("d MMM"));
  return dayMonth + QLatin1Char(' ') + time;
}

void ChatDialog::reloadMessages()
{
  m_view->clear();
  for (const auto &im : m_client->chat()->messagesForPeer(m_peer)) {
    appendMessage(im);
  }
  QScrollBar *bar = m_view->verticalScrollBar();
  bar->setValue(bar->maximum());
}

void ChatDialog::onHistoryLoaded(const QString &peer)
{
  if (!isVisible() || m_client->chat()->normalizedPeer(peer) != m_client->chat()->normalizedPeer(m_peer)) {
    return;
  }
  reloadMessages();
}

void ChatDialog::appendMessage(const itl::InstantMessage &im)
{
  appendMessage(im.body, im.incoming, im.timestamp);
}

void ChatDialog::appendMessage(const QString &text, bool incoming, const QDateTime &timestamp)
{
  const QString name = incoming ? m_peerDisplayName : m_selfDisplayName;
  const QString stamp = formatTimestamp(timestamp);
  if (stamp.isEmpty()) {
    m_view->appendPlainText(QStringLiteral("%1: %2").arg(name, text));
  } else {
    m_view->appendPlainText(QStringLiteral("%1 %2: %3").arg(stamp, name, text));
  }
  QScrollBar *bar = m_view->verticalScrollBar();
  bar->setValue(bar->maximum());
}

void ChatDialog::onSend()
{
  const QString text = m_input->text().trimmed();
  if (text.isEmpty() || m_peer.isEmpty()) {
    return;
  }
  m_client->chat()->sendMessage(m_peer, text);
  m_input->clear();
}

void ChatDialog::onChatMessage(const QString &peer, const QString &text, bool incoming, const QDateTime &timestamp)
{
  if (!isVisible() || m_client->chat()->normalizedPeer(peer) != m_client->chat()->normalizedPeer(m_peer)) {
    return;
  }
  appendMessage(text, incoming, timestamp);
  if (incoming) {
    m_client->chat()->markPeerRead(peer);
  }
}
