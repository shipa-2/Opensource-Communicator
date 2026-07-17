#include "ChatDialog.h"

#include "chat/ChatManager.h"
#include "protocol/CommunicatorClient.h"
#include "ui/NativeScrollBars.h"
#include "ui/StyleHelper.h"

#include <QApplication>
#include <QDate>
#include <QFrame>
#include <QLocale>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollBar>
#include <QShowEvent>
#include <QVBoxLayout>

namespace {

QString chatInputStyleSheet()
{
  return QStringLiteral(
      "QLineEdit {"
      "  color: palette(text);"
      "  background: palette(base);"
      "  border: 1px solid palette(mid);"
      "  border-radius: 4px;"
      "  padding: 4px 8px;"
      "  selection-color: palette(highlightedText);"
      "  selection-background-color: palette(highlight);"
      "}"
      "QLineEdit:focus {"
      "  border: 2px solid palette(highlight);"
      "  padding: 3px 7px;"
      "}");
}

} // namespace

ChatDialog::ChatDialog(itl::CommunicatorClient *client, QWidget *parent)
    : QDialog(parent)
    , m_client(client)
{
  setWindowTitle(tr("Сообщение"));
  setObjectName(QStringLiteral("chatDialog"));
  resize(420, 360);

  auto *root = new QVBoxLayout(this);
  m_viewFrame = new QFrame;
  m_viewFrame->setObjectName(QStringLiteral("chatViewFrame"));
  m_viewFrame->setFrameShape(QFrame::StyledPanel);
  m_viewFrame->setFrameShadow(QFrame::Sunken);
  m_viewFrame->setAttribute(Qt::WA_StyleSheetTarget, false);
  auto *viewLayout = new QVBoxLayout(m_viewFrame);
  viewLayout->setContentsMargins(4, 4, 4, 4);

  m_view = new QPlainTextEdit;
  m_view->setObjectName(QStringLiteral("chatMessageView"));
  m_view->setReadOnly(true);
  m_view->setFrameShape(QFrame::NoFrame);
  m_view->setAttribute(Qt::WA_StyleSheetTarget, false);
  viewLayout->addWidget(m_view);
  root->addWidget(m_viewFrame, 1);

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

  refreshAppearance();
}

void ChatDialog::showEvent(QShowEvent *event)
{
  QDialog::showEvent(event);
  refreshAppearance();
}

void ChatDialog::refreshViewChrome()
{
  if (!m_view) {
    return;
  }

  m_view->setAttribute(Qt::WA_StyleSheetTarget, false);
  m_view->setStyleSheet({});
  m_view->setPalette(QApplication::palette(m_view));
  if (QWidget *viewport = m_view->viewport()) {
    viewport->setAttribute(Qt::WA_StyleSheetTarget, false);
    viewport->setStyleSheet({});
    viewport->setAutoFillBackground(true);
    viewport->setPalette(QApplication::palette(viewport));
    viewport->update();
  }
  m_view->update();
}

void ChatDialog::refreshAppearance()
{
  setAutoFillBackground(true);
  setPalette(QApplication::palette(this));

  if (m_viewFrame) {
    m_viewFrame->setAttribute(Qt::WA_StyleSheetTarget, false);
    m_viewFrame->setStyleSheet({});
    m_viewFrame->setPalette(QApplication::palette(m_viewFrame));
    m_viewFrame->update();
  }

  refreshViewChrome();

  if (m_input) {
    m_input->setStyleSheet(chatInputStyleSheet());
  }

  itl::applyNativeButtons(this);
  itl::applyNativeScrollBars(m_view);
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
  refreshAppearance();
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
