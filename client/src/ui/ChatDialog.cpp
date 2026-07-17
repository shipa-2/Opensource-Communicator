#include "ChatDialog.h"

#include "chat/ChatManager.h"
#include "protocol/CommunicatorClient.h"
#include "settings/AppSettings.h"
#include "ui/NativeScrollBars.h"
#include "ui/StyleHelper.h"
#include "ui/ThemePreviewDialog.h"

#include <QApplication>
#include <QDate>
#include <QFrame>
#include <QLocale>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollBar>
#include <QShowEvent>
#include <QTextBrowser>
#include <QUrl>
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

  m_view = new QTextBrowser;
  m_view->setObjectName(QStringLiteral("chatMessageView"));
  m_view->setReadOnly(true);
  m_view->setFrameShape(QFrame::NoFrame);
  m_view->setAttribute(Qt::WA_StyleSheetTarget, false);
  m_view->setOpenExternalLinks(false);
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
  connect(m_view, &QTextBrowser::anchorClicked, this, &ChatDialog::onThemeLinkActivated);

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

void ChatDialog::updatePeerDisplayName(const QString &peerDisplayName)
{
  if (m_peer.isEmpty()) {
    return;
  }
  m_peerDisplayName = shortDisplayName(peerDisplayName, m_peer.section(QLatin1Char('@'), 0, 0));
  const bool smsPeer = m_client && m_client->chat()->isSmsPeer(m_peer);
  setWindowTitle(smsPeer ? tr("SMS — %1").arg(peerDisplayName) : tr("Чат — %1").arg(peerDisplayName));
  reloadMessages();
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
  const QString dayMonth = locale.toString(local.date(), QStringLiteral("d MMM"));
  return dayMonth + QLatin1Char(' ') + time;
}

QString ChatDialog::htmlEscape(const QString &text)
{
  QString escaped = text;
  escaped.replace(QLatin1Char('&'), QStringLiteral("&amp;"));
  escaped.replace(QLatin1Char('<'), QStringLiteral("&lt;"));
  escaped.replace(QLatin1Char('>'), QStringLiteral("&gt;"));
  return escaped;
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
  if (itl::ChatManager::isThemeShareNotice(im.body)) {
    appendThemeShareNotice(im.body, im.incoming, im.timestamp);
    return;
  }
  if (itl::ChatManager::isThemeAppliedNotice(im.body)) {
    appendThemeAppliedNotice(im.incoming, im.timestamp);
    return;
  }
  appendMessage(im.body, im.incoming, im.timestamp);
}

void ChatDialog::appendMessage(const QString &text, bool incoming, const QDateTime &timestamp)
{
  const QString name = incoming ? m_peerDisplayName : m_selfDisplayName;
  const QString stamp = formatTimestamp(timestamp);
  const QString line = stamp.isEmpty()
      ? QStringLiteral("%1: %2").arg(htmlEscape(name), htmlEscape(text))
      : QStringLiteral("%1 %2: %3").arg(htmlEscape(stamp), htmlEscape(name), htmlEscape(text));
  m_view->append(line);
  QScrollBar *bar = m_view->verticalScrollBar();
  bar->setValue(bar->maximum());
}

void ChatDialog::appendThemeShareNotice(const QString &noticeBody, bool incoming,
                                        const QDateTime &timestamp)
{
  const QString name = incoming ? m_peerDisplayName : m_selfDisplayName;
  const QString stamp = formatTimestamp(timestamp);
  const QString key = itl::ChatManager::themeShareNoticeKey(noticeBody);
  const QString text =
      incoming ? tr("%1 поделился с вами темой").arg(name) : tr("Вы поделились темой");
  QString line;
  if (incoming && !key.isEmpty()) {
    const QString encodedKey = QString::fromLatin1(QUrl::toPercentEncoding(key));
    const QString link = QStringLiteral("<a href=\"osc-theme:%1\">%2</a>")
                             .arg(encodedKey, htmlEscape(text));
    line = stamp.isEmpty() ? link : QStringLiteral("%1 %2").arg(htmlEscape(stamp), link);
  } else {
    line = stamp.isEmpty() ? htmlEscape(text)
                           : QStringLiteral("%1 %2").arg(htmlEscape(stamp), htmlEscape(text));
  }
  m_view->append(line);
  QScrollBar *bar = m_view->verticalScrollBar();
  bar->setValue(bar->maximum());
}

void ChatDialog::appendThemeAppliedNotice(bool incoming, const QDateTime &timestamp)
{
  if (!incoming) {
    return;
  }
  const QString text = tr("%1 применил вашу тему").arg(m_peerDisplayName);
  const QString stamp = formatTimestamp(timestamp);
  const QString line = stamp.isEmpty() ? htmlEscape(text)
                                       : QStringLiteral("%1 %2").arg(htmlEscape(stamp), htmlEscape(text));
  m_view->append(line);
  QScrollBar *bar = m_view->verticalScrollBar();
  bar->setValue(bar->maximum());
}

void ChatDialog::onThemeLinkActivated(const QUrl &url)
{
  if (url.scheme() != QStringLiteral("osc-theme")) {
    return;
  }
  QString encoded = url.toString(QUrl::FullyEncoded);
  const int colon = encoded.indexOf(QLatin1Char(':'));
  if (colon >= 0) {
    encoded = encoded.mid(colon + 1);
  }
  openThemePreview(QUrl::fromPercentEncoding(encoded.toUtf8()));
}

void ChatDialog::openThemePreview(const QString &key)
{
  if (!m_client || key.isEmpty()) {
    return;
  }
  const itl::ThemeSharePayload payload = m_client->chat()->themeShareOffer(key);
  if (payload.wallpaper.isNull()) {
    QMessageBox::information(this, tr("Тема"), tr("Предложение темы больше недоступно."));
    return;
  }

  ThemePreviewDialog preview(payload.wallpaper, payload.uiOpacity, payload.listOpacity, this);
  if (preview.exec() != QDialog::Accepted) {
    return;
  }

  itl::AppSettings &settings = m_client->appSettings();
  const QString path = itl::AppSettings::saveAppWallpaperImage(payload.wallpaper);
  if (path.isEmpty()) {
    QMessageBox::warning(this, tr("Тема"), tr("Не удалось сохранить обои."));
    return;
  }
  settings.setAppWallpaperPath(path);
  settings.setAppWallpaperOpacity(payload.uiOpacity);
  settings.setAppWallpaperListOpacity(payload.listOpacity);
  m_client->saveSettings();
  m_client->chat()->sendThemeApplied(m_peer);
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
  if (itl::ChatManager::isThemeShareNotice(text)) {
    appendThemeShareNotice(text, incoming, timestamp);
  } else if (itl::ChatManager::isThemeAppliedNotice(text)) {
    appendThemeAppliedNotice(incoming, timestamp);
  } else {
    appendMessage(text, incoming, timestamp);
  }
  if (incoming) {
    m_client->chat()->markPeerRead(peer);
  }
}
