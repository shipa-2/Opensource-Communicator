#include "MainWindow.h"

#include "AddContactDialog.h"
#include "ConferenceDialog.h"
#include "TransferDialog.h"
#include "ContactRowWidget.h"
#include "HelpDialog.h"
#include "LoginDialog.h"
#include "NotePopupDialog.h"
#include "ProfileAvatarWidget.h"
#include "SettingsDialog.h"
#include "CallWindow.h"
#include "ChatDialog.h"
#include "PresenceSelector.h"
#include "calls/CallManager.h"
#include "demo/DemoData.h"
#include "protocol/CommunicatorClient.h"
#include "settings/UserDataStore.h"
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QButtonGroup>
#include <QAbstractButton>
#include <QDialog>
#include <QFrame>
#include <QHBoxLayout>
#include <QTabWidget>
#include <QTimer>
#include <QVBoxLayout>
#include <QDateTime>
#include <QFont>
#include <QApplication>

#include <algorithm>

namespace {
int countDigits(const QString &value)
{
  int digits = 0;
  for (const QChar ch : value) {
    if (ch.isDigit()) {
      ++digits;
    }
  }
  return digits;
}

void applyHistoryListPalette(QListWidget *historyList, const QPalette &sourcePalette)
{
  if (!historyList) {
    return;
  }

  QPalette historyPalette = QApplication::palette(historyList);
  historyPalette.setColor(QPalette::Base, sourcePalette.color(QPalette::Window));
  historyPalette.setColor(QPalette::Text, sourcePalette.color(QPalette::WindowText));
  historyList->setPalette(historyPalette);
  if (QWidget *viewport = historyList->viewport()) {
    viewport->setPalette(historyPalette);
  }
}
} // namespace

MainWindow::MainWindow(itl::CommunicatorClient *client, itl::CallManager *calls, QWidget *parent)
    : QMainWindow(parent)
    , m_client(client)
    , m_calls(calls)
    , m_callWindow(new CallWindow(this))
    , m_chatDialog(new ChatDialog(client, this))
{
  setWindowTitle(tr("OpenSource Communicator"));
  setFixedWidth(390);
  resize(390, 620);
  setMenuBar(nullptr);

  auto *central = new QWidget;
  setCentralWidget(central);
  auto *root = new QVBoxLayout(central);
  root->setContentsMargins(0, 0, 0, 0);
  root->setSpacing(0);

  auto *header = new QWidget;
  header->setObjectName(QStringLiteral("mainHeader"));
  auto *headerOuter = new QVBoxLayout(header);
  headerOuter->setContentsMargins(10, 8, 10, 8);

  auto *linksRow = new QHBoxLayout;
  linksRow->addStretch();
  auto *settingsBtn = new QPushButton(tr("Настройки..."));
  settingsBtn->setObjectName(QStringLiteral("linkButton"));
  auto *helpBtn = new QPushButton(tr("Помощь"));
  helpBtn->setObjectName(QStringLiteral("linkButton"));
  linksRow->addWidget(settingsBtn);
  linksRow->addWidget(helpBtn);
  headerOuter->addLayout(linksRow);

  auto *profileRow = new QHBoxLayout;
  m_headerAvatar = new ProfileAvatarWidget(&m_client->appSettings());
  profileRow->addWidget(m_headerAvatar);

  auto *profileText = new QVBoxLayout;
  profileText->setSpacing(2);
  m_headerName = new QLabel(tr("Не авторизован"));
  m_headerName->setObjectName(QStringLiteral("headerName"));
  QFont headerNameFont = m_headerName->font();
  headerNameFont.setPixelSize(18);
  headerNameFont.setBold(true);
  m_headerName->setFont(headerNameFont);

  auto *nameRow = new QWidget;
  auto *nameRowLayout = new QHBoxLayout(nameRow);
  nameRowLayout->setContentsMargins(16, 0, 0, 0);
  nameRowLayout->setSpacing(0);
  nameRowLayout->addWidget(m_headerName);
  profileText->addWidget(nameRow);

  m_presenceSelector = new PresenceSelector;
  m_presenceSelector->setObjectName(QStringLiteral("presenceSelector"));
  profileText->addWidget(m_presenceSelector);
  profileRow->addLayout(profileText, 1);
  headerOuter->addLayout(profileRow);
  root->addWidget(header);

  m_tabs = new QTabWidget;
  root->addWidget(m_tabs, 1);

  auto *contactsPage = new QWidget;
  auto *contactsLayout = new QVBoxLayout(contactsPage);
  contactsLayout->setContentsMargins(8, 8, 8, 8);

  auto *filterRow = new QHBoxLayout;
  m_filterGroup = new QButtonGroup(this);
  auto *allBtn = new QPushButton(tr("Все"));
  allBtn->setObjectName(QStringLiteral("filterBtn"));
  allBtn->setCheckable(true);
  allBtn->setChecked(true);
  filterRow->addWidget(allBtn, 1);
  auto *recentBtn = new QPushButton(tr("Недавние"));
  recentBtn->setObjectName(QStringLiteral("filterBtn"));
  recentBtn->setCheckable(true);
  filterRow->addWidget(recentBtn, 1);
  auto *externalBtn = new QPushButton(tr("Внешние"));
  externalBtn->setObjectName(QStringLiteral("filterBtn"));
  externalBtn->setCheckable(true);
  filterRow->addWidget(externalBtn, 1);
  m_filterGroup->addButton(allBtn, static_cast<int>(ContactSortMode::All));
  m_filterGroup->addButton(recentBtn, static_cast<int>(ContactSortMode::Recent));
  m_filterGroup->addButton(externalBtn, static_cast<int>(ContactSortMode::External));
  contactsLayout->addLayout(filterRow);
  updateFilterButtonStyles();

  m_searchEdit = new QLineEdit;
  m_searchEdit->setObjectName(QStringLiteral("searchEdit"));
  m_searchEdit->setPlaceholderText(tr("Введите номер или имя контакта"));
  contactsLayout->addWidget(m_searchEdit);

  m_contactsList = new QListWidget;
  m_contactsList->setObjectName(QStringLiteral("contactList"));
  contactsLayout->addWidget(m_contactsList, 1);
  m_tabs->addTab(contactsPage, tr("Контакты"));

  auto *dialPage = new QWidget;
  auto *dialLayout = new QVBoxLayout(dialPage);
  dialLayout->setContentsMargins(16, 24, 16, 16);
  dialLayout->addWidget(new QLabel(tr("Набрать номер или внутренний ext:")));
  m_dialInput = new QLineEdit;
  m_dialInput->setObjectName(QStringLiteral("dialEdit"));
  m_dialInput->setPlaceholderText(tr("702, ivan или +7..."));
  dialLayout->addWidget(m_dialInput);
  m_dialCallBtn = new QPushButton(tr("Позвонить"));
  m_dialCallBtn->setObjectName(QStringLiteral("dialCallBtn"));
  dialLayout->addWidget(m_dialCallBtn);
  dialLayout->addStretch();
  m_tabs->addTab(dialPage, tr("Набрать вручную"));

  auto *historyPage = new QWidget;
  auto *historyLayout = new QVBoxLayout(historyPage);
  historyLayout->setContentsMargins(8, 8, 8, 8);
  historyLayout->setSpacing(0);

  m_historyList = new QListWidget;
  m_historyList->setObjectName(QStringLiteral("historyList"));
  m_historyList->setFrameShape(QFrame::NoFrame);
  applyHistoryListPalette(m_historyList, palette());
  historyLayout->addWidget(m_historyList, 1);
  m_tabs->addTab(historyPage, tr("История"));

  auto *footer = new QWidget;
  footer->setObjectName(QStringLiteral("mainFooter"));
  auto *footerLayout = new QHBoxLayout(footer);
  auto *addBtn = new QPushButton(tr("+ Добавить..."));
  addBtn->setObjectName(QStringLiteral("footerBtn"));
  auto *confBtn = new QPushButton(tr("📞 Конференция"));
  confBtn->setObjectName(QStringLiteral("footerBtn"));
  auto *viewBtn = new QPushButton(tr("Вид ▾"));
  viewBtn->setObjectName(QStringLiteral("footerBtn"));
  footerLayout->addWidget(addBtn);
  footerLayout->addWidget(confBtn);
  footerLayout->addWidget(viewBtn);
  root->addWidget(footer);

  connect(settingsBtn, &QPushButton::clicked, this, &MainWindow::onSettings);
  connect(helpBtn, &QPushButton::clicked, this, &MainWindow::onHelp);
  connect(addBtn, &QPushButton::clicked, this, &MainWindow::onAddContact);
  connect(confBtn, &QPushButton::clicked, this, &MainWindow::onConference);
  connect(m_dialCallBtn, &QPushButton::clicked, this, &MainWindow::onDial);
  connect(m_dialInput, &QLineEdit::returnPressed, this, &MainWindow::onDial);
  connect(m_searchEdit, &QLineEdit::textChanged, this, &MainWindow::onSearchChanged);
  connect(m_contactsList, &QListWidget::currentItemChanged, this, [this]() { onContactSelected(); });
  connect(m_presenceSelector, &PresenceSelector::currentIndexChanged, this, &MainWindow::onPresenceChanged);
  connect(m_filterGroup, &QButtonGroup::idClicked, this, &MainWindow::onFilterChanged);
  connect(m_headerAvatar, &ProfileAvatarWidget::settingsChanged, this, &MainWindow::onProfileAvatarChanged);

  connect(m_callWindow, &CallWindow::hangupRequested, this, &MainWindow::onHangup);
  connect(m_callWindow, &CallWindow::answerRequested, this, &MainWindow::onAnswer);
  connect(m_callWindow, &CallWindow::holdRequested, this, &MainWindow::onHold);
  connect(m_callWindow, &CallWindow::transferRequested, this, &MainWindow::onTransfer);
  connect(m_callWindow, &CallWindow::notesChanged, this, &MainWindow::onCallNotesChanged);

  connect(m_client, &itl::CommunicatorClient::statusMessage, this, &MainWindow::onStatusMessage);
  connect(m_client, &itl::CommunicatorClient::contactUpdated, this, &MainWindow::onContactUpdated);
  connect(m_client, &itl::CommunicatorClient::callEvent, this, &MainWindow::onCallEvent);
  connect(m_client->api(), &itl::WsApiClient::domainContactsLoaded, this, &MainWindow::onContactsLoaded);
  connect(m_calls, &itl::CallManager::callStateChanged, this, &MainWindow::onCallStateChanged);

  m_client->loadSettings();
  mergeCustomContacts();
  updateSelfHeader();
  rebuildHistoryList();
  setOnlineUi(false);
  QTimer::singleShot(0, this, &MainWindow::onLogin);
}

void MainWindow::refreshTheme()
{
  if (m_historyList) {
    applyHistoryListPalette(m_historyList, palette());
    m_historyList->update();
    if (QWidget *viewport = m_historyList->viewport()) {
      viewport->update();
    }
  }

  if (m_headerName) {
    m_headerName->setStyleSheet({});
    m_headerName->setPalette(QApplication::palette(m_headerName));
    m_headerName->update();
  }

  if (m_presenceSelector) {
    m_presenceSelector->refreshAppearance();
  }

  if (m_headerAvatar) {
    m_headerAvatar->refreshAppearance();
  }

  if (m_callWindow) {
    m_callWindow->refreshAppearance();
  }

  for (ContactRowWidget *row : findChildren<ContactRowWidget *>()) {
    row->refreshAppearance();
  }

  updateFilterButtonStyles();
  update();
}

void MainWindow::setOnlineUi(bool online)
{
  m_online = online;
  m_dialInput->setEnabled(online);
  m_dialCallBtn->setEnabled(online);
  m_searchEdit->setEnabled(online);
  m_presenceSelector->setEnabled(online);
  m_contactsList->setEnabled(online);
  if (online) {
    m_presenceSelector->setCurrentStatus(QStringLiteral("online"));
  } else {
    m_presenceSelector->setCurrentStatus(QStringLiteral("offline"));
  }
}

QString MainWindow::selectedPeer() const
{
  if (m_contactsList->currentItem()) {
    return m_contactsList->currentItem()->data(Qt::UserRole).toString();
  }
  return resolvePeer(m_dialInput->text().trimmed());
}

QString MainWindow::resolvePeer(QString input) const
{
  input = input.trimmed();
  if (input.isEmpty()) {
    return {};
  }
  if (input.contains(QLatin1Char('@'))) {
    return input;
  }
  const QString domain = m_client->credentials().domain;
  for (auto it = m_contacts.cbegin(); it != m_contacts.cend(); ++it) {
    if (it.key().startsWith(input + QLatin1Char('@')) || it.value().ext == input
        || it.value().login == input || it.value().phone == input) {
      return it.key();
    }
  }
  bool dialAsNumber = true;
  for (const QChar ch : input) {
    if (!ch.isDigit() && ch != QLatin1Char('+') && ch != QLatin1Char('*') && ch != QLatin1Char('#')) {
      dialAsNumber = false;
      break;
    }
  }
  return dialAsNumber ? input : input + QLatin1Char('@') + domain;
}

QString MainWindow::displayNameForPeer(const QString &peer) const
{
  const QString name = m_contacts.value(peer).name;
  return name.isEmpty() ? peer.section(QLatin1Char('@'), 0, 0) : name;
}

QString MainWindow::detailForPeer(const QString &peer) const
{
  const QString ext = m_contacts.value(peer).ext;
  return ext.isEmpty() ? peer : tr("%1 (Внутренний номер)").arg(ext);
}

bool MainWindow::matchesFilterMode(const QString &peer) const
{
  if (m_sortMode != ContactSortMode::External) {
    return true;
  }

  const ContactEntry entry = m_contacts.value(peer);
  if (entry.isSelf || !entry.ext.isEmpty()) {
    return false;
  }

  if (countDigits(entry.phone) > 5) {
    return true;
  }

  return !peer.contains(QLatin1Char('@')) && countDigits(peer) > 5;
}

bool MainWindow::matchesSearch(const QString &peer) const
{
  const QString q = m_searchEdit->text().trimmed().toLower();
  if (q.isEmpty()) {
    return true;
  }
  const ContactEntry entry = m_contacts.value(peer);
  return peer.toLower().contains(q) || entry.name.toLower().contains(q) || entry.ext.contains(q)
      || entry.phone.contains(q);
}

bool MainWindow::isSamePeer(const QString &a, const QString &b) const
{
  if (a.isEmpty() || b.isEmpty()) {
    return a == b;
  }
  if (a == b) {
    return true;
  }
  const QString ra = resolvePeer(a);
  const QString rb = resolvePeer(b);
  return !ra.isEmpty() && ra == rb;
}

ContactRowWidget *MainWindow::rowWidgetForPeer(const QString &peer) const
{
  QListWidgetItem *item = m_contactItems.value(peer);
  return item ? qobject_cast<ContactRowWidget *>(m_contactsList->itemWidget(item)) : nullptr;
}

void MainWindow::rebuildContactList()
{
  m_contactsList->clear();
  m_contactItems.clear();
  QStringList others;
  QString selfPeer;
  for (auto it = m_contacts.cbegin(); it != m_contacts.cend(); ++it) {
    if (it.value().isSelf) {
      selfPeer = it.key();
    } else if (matchesSearch(it.key()) && matchesFilterMode(it.key())) {
      others.append(it.key());
    }
  }
  std::sort(others.begin(), others.end(), [this](const QString &a, const QString &b) {
    if (m_sortMode == ContactSortMode::Recent) {
      const qint64 ta = m_client->appSettings().recentCallTime(a);
      const qint64 tb = m_client->appSettings().recentCallTime(b);
      if (ta != tb) {
        return ta > tb;
      }
    }
    return displayNameForPeer(a).localeAwareCompare(displayNameForPeer(b)) < 0;
  });
  for (const QString &peer : others) {
    addOrUpdateContactRow(peer);
  }
  if (!selfPeer.isEmpty() && matchesSearch(selfPeer) && matchesFilterMode(selfPeer)) {
    addOrUpdateContactRow(selfPeer);
  }
}

void MainWindow::addOrUpdateContactRow(const QString &peer)
{
  const ContactEntry entry = m_contacts.value(peer);
  auto *row = new ContactRowWidget(peer, entry.name, entry.ext, entry.phone, entry.presence, entry.isSelf);
  auto *item = new QListWidgetItem;
  item->setData(Qt::UserRole, peer);
  item->setSizeHint(QSize(0, entry.ext.isEmpty() && entry.phone.isEmpty() ? 48 : 56));
  m_contactsList->addItem(item);
  m_contactsList->setItemWidget(item, row);
  m_contactItems.insert(peer, item);
  connect(row, &ContactRowWidget::callRequested, this, &MainWindow::onCallFromRow);
  connect(row, &ContactRowWidget::chatRequested, this, &MainWindow::onChatFromRow);
  connect(row, &ContactRowWidget::notesRequested, this, &MainWindow::onNotesFromRow);
}

void MainWindow::updateSelfHeader()
{
  if (!m_selfName.isEmpty()) {
    m_headerName->setText(m_selfName);
    m_headerAvatar->setLetter(m_selfName.left(1).toUpper());
  } else {
    const QString login = m_client->credentials().login.section(QLatin1Char('@'), 0, 0);
    m_headerName->setText(login);
    m_headerAvatar->setLetter(login.left(1).toUpper());
  }
  m_headerAvatar->refreshFromSettings();
}

void MainWindow::onFilterChanged(int id)
{
  m_sortMode = static_cast<ContactSortMode>(id);
  updateFilterButtonStyles();
  rebuildContactList();
}

void MainWindow::updateFilterButtonStyles()
{
  if (!m_filterGroup) {
    return;
  }

  const QPalette appPalette = QApplication::palette();
  const QColor accent = appPalette.color(QPalette::Highlight);
  const QColor accentText = appPalette.color(QPalette::HighlightedText);

  for (QAbstractButton *button : m_filterGroup->buttons()) {
    auto *filterBtn = qobject_cast<QPushButton *>(button);
    if (!filterBtn) {
      continue;
    }

    if (filterBtn->isChecked()) {
      QPalette pal = QApplication::palette(filterBtn);
      pal.setColor(QPalette::Button, accent);
      pal.setColor(QPalette::ButtonText, accentText);
      pal.setColor(QPalette::Light, accent.lighter(115));
      pal.setColor(QPalette::Midlight, accent.lighter(105));
      pal.setColor(QPalette::Mid, accent.darker(105));
      pal.setColor(QPalette::Dark, accent.darker(125));
      pal.setColor(QPalette::Shadow, accent.darker(140));
      filterBtn->setPalette(pal);
    } else {
      filterBtn->setPalette(QApplication::palette(filterBtn));
    }

    filterBtn->style()->unpolish(filterBtn);
    filterBtn->style()->polish(filterBtn);
    filterBtn->update();
  }
}

void MainWindow::onProfileAvatarChanged()
{
  m_client->saveSettings();
  m_headerAvatar->refreshFromSettings();
}

void MainWindow::loadCallNotes(const QString &peer)
{
  m_callWindow->setNotesText(m_client->appSettings().noteForPeer(peer));
}

void MainWindow::recordCallForPeer(const QString &peer)
{
  const QString resolved = resolvePeer(peer);
  if (resolved.isEmpty()) {
    return;
  }
  m_client->appSettings().recordRecentCall(resolved);
  if (m_sortMode == ContactSortMode::Recent) {
    rebuildContactList();
  }
}

QString MainWindow::formatHistoryDuration(int seconds)
{
  if (seconds <= 0) {
    return QStringLiteral("—");
  }
  return QStringLiteral("%1:%2")
      .arg(seconds / 60)
      .arg(seconds % 60, 2, 10, QLatin1Char('0'));
}

QString MainWindow::formatHistoryTime(qint64 ms)
{
  if (ms <= 0) {
    return {};
  }
  return QDateTime::fromMSecsSinceEpoch(ms).toString(QStringLiteral("dd.MM.yy HH:mm"));
}

void MainWindow::rebuildHistoryList()
{
  m_historyList->clear();
  const QList<itl::CallHistoryEntry> history =
      m_demoMode ? m_demoCallHistory : m_client->appSettings().callHistory();
  if (history.isEmpty()) {
    auto *placeholder = new QListWidgetItem(tr("История звонков пуста"));
    placeholder->setFlags(Qt::NoItemFlags);
    m_historyList->addItem(placeholder);
    return;
  }

  for (const itl::CallHistoryEntry &entry : history) {
    const QString arrow = entry.direction == QStringLiteral("incoming") ? QStringLiteral("↓") : QStringLiteral("↑");
    QString status;
    if (entry.result == QStringLiteral("transferred") && !entry.transferTo.isEmpty()) {
      if (entry.answered && entry.durationSec > 0) {
        status = tr("%1 · переведён на %2")
                     .arg(formatHistoryDuration(entry.durationSec), entry.transferTo);
      } else {
        status = tr("переведён на %1").arg(entry.transferTo);
      }
    } else if (entry.answered) {
      status = formatHistoryDuration(entry.durationSec);
    } else if (entry.direction == QStringLiteral("incoming")) {
      status = tr("пропущ.");
    } else {
      status = tr("не отв.");
    }

    const QString name = entry.displayName.isEmpty() ? entry.peer : entry.displayName;
    const QString line = QStringLiteral("%1  %2  %3  %4")
                             .arg(formatHistoryTime(entry.startedAtMs), arrow, name, status);
    auto *item = new QListWidgetItem(line);
    item->setData(Qt::UserRole, entry.peer);
    m_historyList->addItem(item);
  }
}

void MainWindow::beginCallTracking(const QString &leg, const QString &peer, const QString &displayName, bool incoming)
{
  if (leg.isEmpty() || peer.isEmpty()) {
    return;
  }
  CallTracking tracking;
  tracking.peer = peer;
  tracking.displayName = displayName.isEmpty() ? displayNameForPeer(peer) : displayName;
  tracking.incoming = incoming;
  tracking.startedAtMs = QDateTime::currentMSecsSinceEpoch();
  m_callTracking.insert(leg, tracking);
}

void MainWindow::markCallConnected(const QString &leg)
{
  if (!m_callTracking.contains(leg) || m_callTracking[leg].connectedAtMs > 0) {
    return;
  }
  m_callTracking[leg].connectedAtMs = QDateTime::currentMSecsSinceEpoch();
}

void MainWindow::finalizeCallHistory(const QString &leg, const QString &state, const QString &transferTo)
{
  if (!m_callTracking.contains(leg)) {
    return;
  }

  const CallTracking tracking = m_callTracking.take(leg);
  itl::CallHistoryEntry entry;
  entry.peer = tracking.peer;
  entry.displayName = tracking.displayName;
  entry.direction = tracking.incoming ? QStringLiteral("incoming") : QStringLiteral("outgoing");
  entry.startedAtMs = tracking.startedAtMs;
  entry.connectedAtMs = tracking.connectedAtMs;
  entry.endedAtMs = QDateTime::currentMSecsSinceEpoch();
  entry.answered = tracking.connectedAtMs > 0;
  entry.durationSec = entry.answered ? static_cast<int>((entry.endedAtMs - entry.connectedAtMs) / 1000) : 0;
  entry.result = state;

  if (state == QStringLiteral("transferred")) {
    entry.result = QStringLiteral("transferred");
    entry.transferTo = transferTo;
    if (entry.transferTo.isEmpty()) {
      entry.transferTo = tr("контакт");
    }
  } else if (!entry.answered && tracking.incoming) {
    entry.result = QStringLiteral("missed");
  } else if (!entry.answered) {
    entry.result = QStringLiteral("unanswered");
  }

  if (m_demoMode) {
    m_demoCallHistory.prepend(entry);
    while (m_demoCallHistory.size() > 50) {
      m_demoCallHistory.removeLast();
    }
    rebuildHistoryList();
    return;
  }

  m_client->appSettings().addCallHistoryEntry(entry);
  recordCallForPeer(tracking.peer);
  rebuildHistoryList();
}

void MainWindow::onCallNotesChanged(const QString &peer, const QString &text)
{
  m_client->appSettings().setNoteForPeer(peer, text);
}

void MainWindow::onNotesFromRow(const QString &peer)
{
  const bool duringCall = !m_activeLeg.isEmpty();
  const bool activeCallPeer = duringCall && isSamePeer(peer, m_callWindow->peer());

  NotePopupDialog dlg(peer, displayNameForPeer(peer), &m_client->appSettings(), this);
  dlg.setDuringCall(duringCall);
  if (dlg.exec() == QDialog::Accepted) {
    if (activeCallPeer) {
      loadCallNotes(peer);
      m_callWindow->setNotesVisible(true);
    }
  }
}

void MainWindow::onLogin()
{
  if (m_demoMode) {
    exitDemoInterface();
  }

  LoginDialog dlg(m_client, this);
  m_client->loadSettings();
  dlg.loadFromClient();
  if (dlg.exec() != QDialog::Accepted) {
    return;
  }

  m_contacts.clear();
  m_contactItems.clear();
  m_contactsList->clear();

  const itl::LoginCredentials cred = m_client->credentials();
  if (itl::DemoData::isDemoCredentials(cred.login, cred.password)) {
    itl::LoginCredentials demoCred = cred;
    demoCred.login = QStringLiteral("demo@") + itl::DemoData::demoDomain();
    demoCred.password = QStringLiteral("demo");
    demoCred.domain = itl::DemoData::demoDomain();
    demoCred.authDomain.clear();
    m_client->setCredentials(demoCred);
    m_client->enterDemoMode();
    enterDemoInterface();
    return;
  }

  m_client->login();
}

void MainWindow::enterDemoInterface()
{
  m_demoMode = true;
  m_demoCallHistory = itl::DemoData::callHistory();
  m_contacts.clear();

  for (const itl::DemoData::DemoContact &contact : itl::DemoData::contacts()) {
    ContactEntry entry;
    entry.name = contact.name;
    entry.ext = contact.ext;
    entry.phone = contact.phone;
    entry.presence = contact.presence;
    entry.login = contact.peer.section(QLatin1Char('@'), 0, 0);
    entry.isSelf = contact.isSelf;
    if (contact.isSelf) {
      m_selfPeer = contact.peer;
      m_selfName = contact.name;
    }
    m_contacts.insert(contact.peer, entry);
  }

  itl::DemoData::seedChatMessages(m_client->chat());
  updateSelfHeader();
  rebuildContactList();
  rebuildHistoryList();
  setOnlineUi(true);
}

void MainWindow::exitDemoInterface()
{
  stopDemoCallSimulation();
  m_demoMode = false;
  m_demoCallHistory.clear();
  m_contacts.clear();
  m_contactItems.clear();
  m_contactsList->clear();
  m_selfPeer.clear();
  m_selfName.clear();
  m_client->leaveDemoMode();
  m_callWindow->closeCall();
  updateSelfHeader();
  rebuildHistoryList();
  setOnlineUi(false);
}

void MainWindow::stopDemoCallSimulation()
{
  m_demoCallLeg.clear();
}

void MainWindow::startDemoCallSimulation(const QString &peer, const QString &displayName, const QString &detail)
{
  stopDemoCallSimulation();
  m_demoCallLeg = QStringLiteral("demo-call");
  m_activeLeg = m_demoCallLeg;
  m_activeIncomingLeg.clear();
  loadCallNotes(peer);
  m_callWindow->showOutgoing(peer, displayName, detail);
  beginCallTracking(m_demoCallLeg, peer, displayName, false);

  QTimer::singleShot(1200, this, [this, displayName]() {
    if (!m_demoMode || m_activeLeg != m_demoCallLeg) {
      return;
    }
    m_callWindow->updateState(QStringLiteral("ringing"), {});
    QTimer::singleShot(1800, this, [this, displayName]() {
      if (!m_demoMode || m_activeLeg != m_demoCallLeg) {
        return;
      }
      m_callWindow->updateState(QStringLiteral("connected"), displayName);
      markCallConnected(m_demoCallLeg);
    });
  });
}

void MainWindow::onLogout()
{
  if (m_demoMode) {
    exitDemoInterface();
    return;
  }
  m_client->logout();
  setOnlineUi(false);
  m_callWindow->closeCall();
}

void MainWindow::onSettings()
{
  SettingsDialog dlg(m_client, m_calls, this);
  const int result = dlg.exec();
  if (result == 2) {
    onLogin();
  }
}

void MainWindow::onAddContact()
{
  if (!m_online) {
    return;
  }

  AddContactDialog dlg(m_client->credentials().domain, this);
  if (dlg.exec() != QDialog::Accepted) {
    return;
  }

  const itl::CustomContact contact = dlg.contact();
  m_client->appSettings().addCustomContact(contact);
  m_client->saveSettings();

  ContactEntry entry;
  entry.name = contact.name;
  entry.ext = contact.ext;
  entry.phone = contact.phone;
  entry.login = contact.peer.section(QLatin1Char('@'), 0, 0);
  m_contacts.insert(contact.peer, entry);
  rebuildContactList();
}

void MainWindow::onConference()
{
  if (!m_online) {
    return;
  }

  QHash<QString, QString> peerNames;
  for (auto it = m_contacts.cbegin(); it != m_contacts.cend(); ++it) {
    if (it.value().isSelf) {
      continue;
    }
    const QString name = it.value().name.isEmpty() ? displayNameForPeer(it.key()) : it.value().name;
    peerNames.insert(it.key(), name);
  }

  ConferenceDialog dlg(peerNames, m_selfPeer, this);
  if (dlg.exec() != QDialog::Accepted) {
    return;
  }

  if (m_demoMode) {
    const QList<itl::ConferenceParticipant> participants = dlg.participants();
    const QString subject = dlg.subject();
    startDemoCallSimulation(subject.isEmpty() ? tr("Конференция") : subject, subject.isEmpty() ? tr("Конференция") : subject,
                            tr("%1 участников").arg(participants.size()));
    return;
  }

  const QList<itl::ConferenceParticipant> participants = dlg.participants();
  const QString subject = dlg.subject();
  m_activeLeg = m_calls->startConferenceCall(subject, participants);
  if (m_activeLeg.isEmpty()) {
    return;
  }
  m_callWindow->showOutgoing(subject.isEmpty() ? tr("Конференция") : subject, tr("Конференция"),
                             tr("%1 участников").arg(participants.size()));
}
void MainWindow::onHelp()
{
  HelpDialog dlg(this);
  dlg.exec();
}

void MainWindow::onDial()
{
  const QString peer = resolvePeer(m_dialInput->text());
  if (!peer.isEmpty()) {
    onCallFromRow(peer);
  }
}

void MainWindow::onCallFromRow(const QString &peer)
{
  if (!m_online) {
    return;
  }

  if (m_demoMode) {
    startDemoCallSimulation(peer, displayNameForPeer(peer), detailForPeer(peer));
    return;
  }

  m_activeLeg = m_calls->startOutgoingCall(peer);
  m_activeIncomingLeg.clear();
  loadCallNotes(peer);
  m_callWindow->showOutgoing(peer, displayNameForPeer(peer), detailForPeer(peer));
}

void MainWindow::onChatFromRow(const QString &peer)
{
  m_chatDialog->openForPeer(peer, displayNameForPeer(peer));
}

void MainWindow::onHangup()
{
  if (m_demoMode) {
    const QString leg = m_activeLeg;
    stopDemoCallSimulation();
    m_activeLeg.clear();
    m_activeIncomingLeg.clear();
    m_onHold = false;
    if (!leg.isEmpty()) {
      finalizeCallHistory(leg, QStringLiteral("ended"));
    }
    m_callWindow->closeCall();
    return;
  }

  m_calls->hangupAll();
  m_activeLeg.clear();
  m_activeIncomingLeg.clear();
  m_onHold = false;
  m_callWindow->closeCall();
}

void MainWindow::onAnswer()
{
  if (!m_activeIncomingLeg.isEmpty()) {
    m_calls->acceptIncomingCall(m_activeIncomingLeg);
    m_activeLeg = m_activeIncomingLeg;
  }
}

void MainWindow::onHold()
{
  if (m_activeLeg.isEmpty()) {
    return;
  }

  if (m_demoMode) {
    m_onHold = !m_onHold;
    m_callWindow->updateState(m_onHold ? QStringLiteral("hold") : QStringLiteral("resumed"), {});
    return;
  }

  m_onHold = !m_onHold;
  m_calls->setHold(m_activeLeg, m_onHold);
}

void MainWindow::onTransfer()
{
  if (m_activeLeg.isEmpty() || !m_online) {
    return;
  }

  QHash<QString, QString> peerNames;
  for (auto it = m_contacts.cbegin(); it != m_contacts.cend(); ++it) {
    if (it.value().isSelf) {
      continue;
    }
    const QString name = it.value().name.isEmpty() ? displayNameForPeer(it.key()) : it.value().name;
    peerNames.insert(it.key(), name);
  }

  const QString excludePeer = m_callWindow ? m_callWindow->peer() : QString();
  TransferDialog dlg(peerNames, m_selfPeer, excludePeer, this);
  if (dlg.exec() != QDialog::Accepted) {
    return;
  }

  const QString peer = dlg.selectedPeer();
  if (peer.isEmpty()) {
    return;
  }

  const QString transferName = dlg.selectedDisplayName().isEmpty()
                                   ? displayNameForPeer(peer)
                                   : dlg.selectedDisplayName();
  const QString leg = m_activeLeg;

  if (m_demoMode) {
    finalizeCallHistory(leg, QStringLiteral("transferred"), transferName);
    m_activeLeg.clear();
    m_activeIncomingLeg.clear();
    m_onHold = false;
    m_callWindow->closeCall();
    return;
  }

  m_calls->blindTransfer(leg, peer);
}

void MainWindow::onPresenceChanged(int index)
{
  if (!m_online || index < 0 || m_demoMode) {
    return;
  }
  m_client->api()->setOwnPresence(m_presenceSelector->currentStatus());
}

void MainWindow::onSearchChanged(const QString &) { rebuildContactList(); }

void MainWindow::onContactSelected()
{
  for (int i = 0; i < m_contactsList->count(); ++i) {
    auto *item = m_contactsList->item(i);
    if (auto *row = qobject_cast<ContactRowWidget *>(m_contactsList->itemWidget(item))) {
      row->setSelected(item == m_contactsList->currentItem());
    }
  }
}

void MainWindow::onStatusMessage(const QString &message)
{
  if (m_demoMode) {
    return;
  }
  if (message.contains(tr("В сети"))) {
    setOnlineUi(true);
  }
}

void MainWindow::onContactUpdated(const QString &peer, const QString &name, const QString &presence)
{
  ContactEntry &entry = m_contacts[peer];
  if (!name.isEmpty()) {
    entry.name = name;
  }
  if (!presence.isEmpty()) {
    entry.presence = presence;
  }
  if (auto *row = rowWidgetForPeer(peer)) {
    if (!name.isEmpty()) {
      row->updateName(entry.name);
    }
    if (!presence.isEmpty()) {
      row->updatePresence(presence);
    }
  }
}

void MainWindow::onContactsLoaded(const QJsonObject &contacts)
{
  const QJsonObject accList = contacts.value(QStringLiteral("accList")).toObject();
  if (accList.isEmpty()) {
    return;
  }

  const QString domain = m_client->credentials().domain;
  const QString selfLogin = m_client->credentials().login.section(QLatin1Char('@'), 0, 0);

  for (auto it = accList.begin(); it != accList.end(); ++it) {
    if (it.key() == QStringLiteral("pbx")) {
      continue;
    }

    const QJsonObject acc = it.value().toObject();
    ContactEntry entry;
    entry.login = it.key();
    entry.name = acc.value(QStringLiteral("RealName")).toString();
    const QJsonArray ext = acc.value(QStringLiteral("ext")).toArray();
    if (!ext.isEmpty()) {
      entry.ext = ext.first().toString();
    }
    entry.phone = acc.value(QStringLiteral("mobile")).toString();
    if (entry.phone.isEmpty()) {
      const QJsonArray tn = acc.value(QStringLiteral("tn")).toArray();
      if (!tn.isEmpty()) {
        entry.phone = tn.first().toString();
      }
    }
    if (entry.phone.isEmpty()) {
      entry.phone = acc.value(QStringLiteral("sim")).toString();
    }
    entry.isSelf = (entry.login == selfLogin);
    const QString peer = entry.login + QLatin1Char('@') + domain;
    if (entry.isSelf) {
      m_selfPeer = peer;
      m_selfName = entry.name;
    }
    m_contacts.insert(peer, entry);
  }

  updateSelfHeader();
  mergeCustomContacts();
  rebuildContactList();
}

void MainWindow::mergeCustomContacts()
{
  for (const itl::CustomContact &contact : m_client->appSettings().customContacts()) {
    if (contact.peer.isEmpty()) {
      continue;
    }
    ContactEntry entry = m_contacts.value(contact.peer);
    entry.name = contact.name;
    entry.ext = contact.ext;
    entry.phone = contact.phone;
    entry.login = contact.peer.section(QLatin1Char('@'), 0, 0);
    entry.isCustom = true;
    m_contacts.insert(contact.peer, entry);
  }
}

void MainWindow::onCallEvent(const QString &leg, const QString &what, const QJsonObject &payload)
{
  m_calls->handleServerCallEvent(leg, what, payload);
  if (what == QStringLiteral("incomingCall")) {
    m_activeIncomingLeg = leg;
  }
}

void MainWindow::onCallStateChanged(const QString &leg, const QString &state, const QString &detail)
{
  if (state == QStringLiteral("incoming")) {
    m_activeIncomingLeg = leg;
    QString incomingPeer;
    if (itl::CallSession *session = m_calls->call(leg)) {
      incomingPeer = session->peer;
      loadCallNotes(incomingPeer);
    }
    beginCallTracking(leg, incomingPeer, detail, true);
    m_callWindow->showIncoming(incomingPeer, detail, {});
    return;
  }
  if (state == QStringLiteral("connecting") || state == QStringLiteral("dialing")
      || state == QStringLiteral("ringing")) {
    if (state != QStringLiteral("connecting") && state != QStringLiteral("dialing")) {
      m_activeLeg = leg;
    }
    if (state == QStringLiteral("connecting") || state == QStringLiteral("dialing")) {
      const QString peer = m_callWindow->peer().isEmpty() ? detail : m_callWindow->peer();
      beginCallTracking(leg, peer, displayNameForPeer(peer), false);
    }
    m_callWindow->updateState(state, detail);
    return;
  }
  if (state == QStringLiteral("connected")) {
    m_activeLeg = leg;
    m_activeIncomingLeg.clear();
    markCallConnected(leg);
    m_callWindow->updateState(state, detail.isEmpty() ? displayNameForPeer(m_callWindow->peer()) : detail);
    return;
  }
  if (state == QStringLiteral("accepting") || state == QStringLiteral("media")
      || state == QStringLiteral("hold") || state == QStringLiteral("resumed")) {
    m_activeLeg = leg;
    m_callWindow->updateState(state, detail);
    return;
  }
  if (state == QStringLiteral("transferred")) {
    const QString transferName = detail.isEmpty() ? tr("контакт") : displayNameForPeer(detail);
    finalizeCallHistory(leg, QStringLiteral("transferred"),
                        transferName.isEmpty() ? detail : transferName);
    if (m_activeLeg == leg || m_activeIncomingLeg == leg || m_activeLeg.isEmpty()) {
      m_activeLeg.clear();
      m_activeIncomingLeg.clear();
      m_onHold = false;
    }
    m_callWindow->closeCall();
    return;
  }
  if (state == QStringLiteral("ended") || state == QStringLiteral("rejected")
      || state == QStringLiteral("error")) {
    finalizeCallHistory(leg, state);
    if (m_activeLeg == leg || m_activeIncomingLeg == leg) {
      m_activeLeg.clear();
      m_activeIncomingLeg.clear();
      m_onHold = false;
    }
    m_callWindow->updateState(state, detail);
  }
}
