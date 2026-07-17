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
#include <QDialog>
#include <QLocale>
#include <QFileDialog>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollBar>
#include <QShowEvent>
#include <QTextBrowser>
#include <QTextCursor>
#include <QUrl>
#include <QVBoxLayout>
#include <QDesktopServices>
#include <QFile>

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

QString themeShareKeyFromUrl(const QUrl &url)
{
  if (url.scheme() != QStringLiteral("osc-theme")) {
    return {};
  }
  QString encoded = url.toString(QUrl::FullyEncoded);
  const int colon = encoded.indexOf(QLatin1Char(':'));
  if (colon >= 0) {
    encoded = encoded.mid(colon + 1);
  }
  return QUrl::fromPercentEncoding(encoded.toUtf8());
}

QString fileShareKeyFromUrl(const QUrl &url)
{
  if (url.scheme() != QStringLiteral("osc-file")) {
    return {};
  }
  QString encoded = url.toString(QUrl::FullyEncoded);
  const int colon = encoded.indexOf(QLatin1Char(':'));
  if (colon >= 0) {
    encoded = encoded.mid(colon + 1);
  }
  return QUrl::fromPercentEncoding(encoded.toUtf8());
}

void configureAttachButton(QPushButton *button)
{
  if (!button) {
    return;
  }
  const QFontMetrics metrics(button->font());
  const int side = qMax(metrics.height() + 8, metrics.horizontalAdvance(QStringLiteral("+")) + 14);
  button->setFixedSize(side, side);
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
  m_attachBtn = new QPushButton(QStringLiteral("+"));
  m_attachBtn->setToolTip(tr("Приложить файл"));
  configureAttachButton(m_attachBtn);
  m_input = new QLineEdit;
  m_input->setPlaceholderText(tr("Введите сообщение..."));
  auto *sendBtn = new QPushButton(tr("Отправить"));
  row->addWidget(m_attachBtn);
  row->addWidget(m_input, 1);
  row->addWidget(sendBtn);
  root->addLayout(row);

  connect(m_attachBtn, &QPushButton::clicked, this, &ChatDialog::onAttachFile);
  connect(sendBtn, &QPushButton::clicked, this, &ChatDialog::onSend);
  connect(m_input, &QLineEdit::returnPressed, this, &ChatDialog::onSend);
  connect(m_client, &itl::CommunicatorClient::chatMessage, this, &ChatDialog::onChatMessage);
  connect(m_client->chat(), &itl::ChatManager::historyLoaded, this, &ChatDialog::onHistoryLoaded);
  connect(m_view, &QTextBrowser::anchorClicked, this, &ChatDialog::onAnchorActivated);

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
  updateAttachEnabled();
  refreshAppearance();
}

void ChatDialog::updateAttachEnabled()
{
  if (!m_attachBtn || !m_client) {
    return;
  }
  const bool smsPeer = !m_peer.isEmpty() && m_client->chat()->isSmsPeer(m_peer);
  m_attachBtn->setEnabled(!m_peer.isEmpty() && !smsPeer);
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
  escaped.replace(QLatin1Char('"'), QStringLiteral("&quot;"));
  return escaped;
}

QString ChatDialog::linkifyHtml(const QString &text)
{
  static const QRegularExpression urlRe(
      R"((?:https?://|www\.)[^\s<&]+)", QRegularExpression::CaseInsensitiveOption);

  QString html;
  int last = 0;
  auto it = urlRe.globalMatch(text);
  while (it.hasNext()) {
    const QRegularExpressionMatch match = it.next();
    html += htmlEscape(text.mid(last, match.capturedStart() - last));

    QString url = match.captured(0);
    while (url.endsWith(QLatin1Char('.')) || url.endsWith(QLatin1Char(',')) || url.endsWith(QLatin1Char(')'))
           || url.endsWith(QLatin1Char(']'))) {
      url.chop(1);
    }
    QString href = url;
    if (href.startsWith(QStringLiteral("www."), Qt::CaseInsensitive)) {
      href.prepend(QStringLiteral("https://"));
    }
    const QString safeHref = QString::fromLatin1(QUrl(href).toEncoded());
    html += QStringLiteral("<a href=\"%1\">%2</a>").arg(safeHref, htmlEscape(url));
    last = match.capturedStart() + url.size();
  }
  html += htmlEscape(text.mid(last));
  return html;
}

void ChatDialog::reloadMessages()
{
  if (!m_client || m_peer.isEmpty()) {
    m_view->clear();
    return;
  }

  QStringList lines;
  for (const auto &im : m_client->chat()->messagesForPeer(m_peer)) {
    lines.append(buildMessageHtml(im));
  }
  m_view->setHtml(lines.join(QStringLiteral("<br/>")));
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

QString ChatDialog::buildMessageHtml(const itl::InstantMessage &im) const
{
  if (itl::ChatManager::isThemeShareNotice(im.body)) {
    return buildThemeShareNoticeHtml(im.body, im.incoming, im.timestamp);
  }
  if (itl::ChatManager::isFileShareNotice(im.body)) {
    return buildFileShareNoticeHtml(im.body, im.incoming, im.timestamp);
  }
  if (itl::ChatManager::isThemeAppliedNotice(im.body)) {
    return buildThemeAppliedNoticeHtml(im.incoming, im.timestamp);
  }
  return buildPlainMessageHtml(im.body, im.incoming, im.timestamp);
}

QString ChatDialog::buildPlainMessageHtml(const QString &text, bool incoming,
                                          const QDateTime &timestamp) const
{
  const QString name = incoming ? m_peerDisplayName : m_selfDisplayName;
  const QString stamp = formatTimestamp(timestamp);
  const QString linkedText = linkifyHtml(text);
  if (stamp.isEmpty()) {
    return QStringLiteral("%1: %2").arg(htmlEscape(name), linkedText);
  }
  return QStringLiteral("%1 %2: %3").arg(htmlEscape(stamp), htmlEscape(name), linkedText);
}

QString ChatDialog::buildThemeShareNoticeHtml(const QString &noticeBody, bool incoming,
                                              const QDateTime &timestamp) const
{
  const QString name = incoming ? m_peerDisplayName : m_selfDisplayName;
  const QString stamp = formatTimestamp(timestamp);
  const QString key = itl::ChatManager::themeShareNoticeKey(noticeBody);
  const QString text =
      incoming ? tr("%1 поделился с вами темой").arg(name) : tr("Вы поделились темой");
  if (incoming && !key.isEmpty()) {
    const QString encodedKey = QString::fromLatin1(QUrl::toPercentEncoding(key));
    const QString link = QStringLiteral("<a href=\"osc-theme:%1\">%2</a>")
                             .arg(encodedKey, htmlEscape(text));
    if (stamp.isEmpty()) {
      return link;
    }
    return QStringLiteral("%1 %2").arg(htmlEscape(stamp), link);
  }
  if (stamp.isEmpty()) {
    return htmlEscape(text);
  }
  return QStringLiteral("%1 %2").arg(htmlEscape(stamp), htmlEscape(text));
}

QString ChatDialog::buildFileShareNoticeHtml(const QString &noticeBody, bool incoming,
                                             const QDateTime &timestamp) const
{
  const QString name = incoming ? m_peerDisplayName : m_selfDisplayName;
  const QString stamp = formatTimestamp(timestamp);
  const QString key = itl::ChatManager::fileShareNoticeKey(noticeBody);
  const itl::ChatFilePayload payload =
      key.isEmpty() ? itl::ChatFilePayload{} : m_client->chat()->chatFileOffer(key);
  const QString fileName =
      payload.fileName.isEmpty() ? tr("файл") : payload.fileName;
  const QString text = incoming ? tr("%1 отправил файл: %2").arg(name, fileName)
                                : tr("Вы отправили файл: %1").arg(fileName);
  if (!key.isEmpty()) {
    const QString encodedKey = QString::fromLatin1(QUrl::toPercentEncoding(key));
    const QString link = QStringLiteral("<a href=\"osc-file:%1\">%2</a>")
                             .arg(encodedKey, htmlEscape(text));
    if (stamp.isEmpty()) {
      return link;
    }
    return QStringLiteral("%1 %2").arg(htmlEscape(stamp), link);
  }
  if (stamp.isEmpty()) {
    return htmlEscape(text);
  }
  return QStringLiteral("%1 %2").arg(htmlEscape(stamp), htmlEscape(text));
}

QString ChatDialog::buildThemeAppliedNoticeHtml(bool incoming, const QDateTime &timestamp) const
{
  if (!incoming) {
    return {};
  }
  const QString text = tr("%1 применил вашу тему").arg(m_peerDisplayName);
  const QString stamp = formatTimestamp(timestamp);
  if (stamp.isEmpty()) {
    return htmlEscape(text);
  }
  return QStringLiteral("%1 %2").arg(htmlEscape(stamp), htmlEscape(text));
}

void ChatDialog::onAnchorActivated(const QUrl &url)
{
  QTextCursor cursor = m_view->textCursor();
  cursor.clearSelection();
  QTextCharFormat plainFormat;
  plainFormat.setAnchor(false);
  cursor.setCharFormat(plainFormat);
  m_view->setTextCursor(cursor);

  const QString scheme = url.scheme().toLower();
  if (scheme == QStringLiteral("osc-theme")) {
    openThemePreview(themeShareKeyFromUrl(url));
    return;
  }
  if (scheme == QStringLiteral("osc-file")) {
    saveChatFile(fileShareKeyFromUrl(url));
    reloadMessages();
    return;
  }
  if (scheme == QStringLiteral("http") || scheme == QStringLiteral("https")
      || scheme == QStringLiteral("ftp")) {
    QDesktopServices::openUrl(url);
  }
}

void ChatDialog::saveChatFile(const QString &key)
{
  if (!m_client || key.isEmpty()) {
    return;
  }
  const itl::ChatFilePayload payload = m_client->chat()->chatFileOffer(key);
  if (payload.data.isEmpty()) {
    QMessageBox::information(this, tr("Файл"), tr("Файл больше недоступен."));
    return;
  }

  const QString path = QFileDialog::getSaveFileName(this, tr("Сохранить файл"), payload.fileName);
  if (path.isEmpty()) {
    return;
  }
  QFile file(path);
  if (!file.open(QIODevice::WriteOnly)) {
    QMessageBox::warning(this, tr("Файл"), tr("Не удалось сохранить файл."));
    return;
  }
  if (file.write(payload.data) != payload.data.size()) {
    QMessageBox::warning(this, tr("Файл"), tr("Не удалось сохранить файл."));
    return;
  }
}

void ChatDialog::onAttachFile()
{
  if (!m_client || m_peer.isEmpty() || m_client->chat()->isSmsPeer(m_peer)) {
    return;
  }

  const QString path = QFileDialog::getOpenFileName(this, tr("Приложить файл"));
  if (path.isEmpty()) {
    return;
  }

  if (!m_client->chat()->sendFileShare(m_peer, path)) {
    QMessageBox::warning(this, tr("Приложить файл"),
                         tr("Не удалось отправить файл (лимит %1 КБ).")
                             .arg(96));
    return;
  }
  reloadMessages();
}

void ChatDialog::applyThemeFromPreview(const QString &key)
{
  if (!m_client || key.isEmpty()) {
    return;
  }
  const itl::ThemeSharePayload payload = m_client->chat()->themeShareOffer(key);
  if (payload.wallpaper.isNull()) {
    QMessageBox::information(this, tr("Тема"), tr("Предложение темы больше недоступно."));
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

  auto *preview =
      new ThemePreviewDialog(payload.wallpaper, payload.uiOpacity, payload.listOpacity, this);
  preview->setAttribute(Qt::WA_DeleteOnClose);
  preview->setWindowModality(Qt::ApplicationModal);
  connect(preview, &QDialog::finished, this, [this, preview, key](int result) {
    if (result == QDialog::Accepted) {
      applyThemeFromPreview(key);
    }
    reloadMessages();
  });
  preview->open();
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
  Q_UNUSED(text)
  Q_UNUSED(incoming)
  Q_UNUSED(timestamp)

  if (!isVisible() || m_client->chat()->normalizedPeer(peer) != m_client->chat()->normalizedPeer(m_peer)) {
    return;
  }
  reloadMessages();
  if (incoming) {
    m_client->chat()->markPeerRead(peer);
  }
}
