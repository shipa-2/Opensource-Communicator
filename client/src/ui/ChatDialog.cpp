#include "ChatDialog.h"

#include "chat/ChatManager.h"
#include "protocol/CommunicatorClient.h"
#include "ui/StyleHelper.h"

#include <QHBoxLayout>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
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

  itl::applyDialogStyle(this);
}

void ChatDialog::openForPeer(const QString &peer, const QString &displayName)
{
  m_peer = peer;
  m_displayName = displayName;
  const bool smsPeer = m_client && m_client->chat()->isSmsPeer(peer);
  setWindowTitle(smsPeer ? tr("SMS — %1").arg(displayName) : tr("Чат — %1").arg(displayName));
  m_view->clear();
  loadHistory();
  show();
  raise();
  activateWindow();
  m_input->setFocus();
}

void ChatDialog::loadHistory()
{
  for (const auto &im : m_client->chat()->messagesForPeer(m_peer)) {
    appendMessage(im.body, im.incoming);
  }
  m_client->chat()->loadHistory();
}

void ChatDialog::appendMessage(const QString &text, bool incoming)
{
  const QString prefix = incoming ? tr("← ") : tr("→ ");
  m_view->appendPlainText(prefix + text);
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

void ChatDialog::onChatMessage(const QString &peer, const QString &text, bool incoming)
{
  if (peer != m_peer || !isVisible()) {
    return;
  }
  appendMessage(text, incoming);
}
