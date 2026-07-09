#include "MainWindow.h"

#include "AddContactDialog.h"
#include "ConferenceDialog.h"
#include "DialKeypadWidget.h"
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
#include "chat/ChatManager.h"
#include "audio/MessageNotifyPlayer.h"
#include "demo/DemoData.h"
#include "protocol/CommunicatorClient.h"
#include "protocol/CallHistoryParser.h"
#include "protocol/ProtocolTypes.h"
#include "settings/UserDataStore.h"
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QAction>
#include <QMenu>
#include <QMenuBar>
#include <QPushButton>
#include <QRegularExpression>
#include <QButtonGroup>
#include <QAbstractButton>
#include <QDialog>
#include <QFile>
#include <QFileDialog>
#include <QFrame>
#include <QHBoxLayout>
#include <QMenu>
#include <QMessageBox>
#include <QTabWidget>
#include <QTextStream>
#include <QTimer>
#include <QVBoxLayout>
#include <QDateTime>
#include <QTime>
#include <QFont>
#include <QApplication>
#include <QBuffer>
#include <QClipboard>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QLoggingCategory>
#include <QMimeData>
#include <QUrl>
#include <QEvent>

#include <algorithm>

Q_LOGGING_CATEGORY(lcHistory, "itl.history")

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

QColor textOnAccentBackground(const QColor &background)
{
  return background.lightness() > 140 ? QColor(0x20, 0x20, 0x20) : QColor(Qt::white);
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
    , m_messageNotify(new itl::MessageNotifyPlayer(this))
    , m_callWindow(new CallWindow(this))
    , m_chatDialog(new ChatDialog(client, this))
{
  setWindowTitle(tr("OpenSource Communicator"));
  setFixedWidth(390);
  resize(390, 620);

  auto *menuBar = new QMenuBar(this);
  menuBar->setObjectName(QStringLiteral("windowMenuBar"));
  menuBar->setNativeMenuBar(false);
  auto *headerLinks = new QWidget(menuBar);
  auto *linksRow = new QHBoxLayout(headerLinks);
  linksRow->setContentsMargins(0, 0, 4, 0);
  linksRow->setSpacing(8);
  auto *settingsBtn = new QPushButton(tr("Настройки"), headerLinks);
  settingsBtn->setObjectName(QStringLiteral("linkButton"));
  settingsBtn->setFlat(true);
  settingsBtn->setCursor(Qt::PointingHandCursor);
  auto *helpBtn = new QPushButton(tr("Помощь"), headerLinks);
  helpBtn->setObjectName(QStringLiteral("linkButton"));
  helpBtn->setFlat(true);
  helpBtn->setCursor(Qt::PointingHandCursor);
  linksRow->addWidget(settingsBtn);
  linksRow->addWidget(helpBtn);
  menuBar->setCornerWidget(headerLinks, Qt::TopLeftCorner);
  setMenuBar(menuBar);

  auto *central = new QWidget;
  setCentralWidget(central);
  auto *root = new QVBoxLayout(central);
  root->setContentsMargins(0, 0, 0, 0);
  root->setSpacing(0);

  auto *header = new QWidget;
  header->setObjectName(QStringLiteral("mainHeader"));
  auto *headerOuter = new QVBoxLayout(header);
  headerOuter->setContentsMargins(10, 8, 10, 8);

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

  m_dialPage = new QWidget;
  auto *dialLayout = new QVBoxLayout(m_dialPage);
  dialLayout->setContentsMargins(16, 24, 16, 16);
  dialLayout->addWidget(new QLabel(tr("Набрать номер или внутренний код:")));
  m_dialInput = new QLineEdit;
  m_dialInput->setObjectName(QStringLiteral("dialEdit"));
  m_dialInput->setPlaceholderText(tr("702, ivan или +7..."));
  m_dialInput->setAlignment(Qt::AlignCenter);
  dialLayout->addWidget(m_dialInput);

  m_dialKeypad = new DialKeypadWidget(m_dialPage);
  m_dialKeypad->setLineEdit(m_dialInput);
  dialLayout->addWidget(m_dialKeypad);

  m_dialCallBtn = new QPushButton(tr("Позвонить"));
  m_dialCallBtn->setObjectName(QStringLiteral("dialCallBtn"));
  dialLayout->addWidget(m_dialCallBtn);
  m_tabs->addTab(m_dialPage, tr("Набрать вручную"));

  auto *historyPage = new QWidget;
  auto *historyLayout = new QVBoxLayout(historyPage);
  historyLayout->setContentsMargins(8, 8, 8, 8);
  historyLayout->setSpacing(6);

  auto *periodRow = new QHBoxLayout;
  periodRow->addWidget(new QLabel(tr("Показать за:")));
  m_historyPeriodBtn = new QPushButton;
  m_historyPeriodBtn->setObjectName(QStringLiteral("linkButton"));
  m_historyPeriodBtn->setFlat(true);
  m_historyPeriodBtn->setCursor(Qt::PointingHandCursor);
  applyLinkButtonStyle(m_historyPeriodBtn);
  periodRow->addWidget(m_historyPeriodBtn);
  periodRow->addStretch();
  historyLayout->addLayout(periodRow);

  auto *historyFilterRow = new QHBoxLayout;
  m_historyDirGroup = new QButtonGroup(this);
  const struct {
    const char *label;
    HistoryDir mode;
  } dirButtons[] = {
      {QT_TR_NOOP("Все"), HistoryDir::All},
      {QT_TR_NOOP("Входящие"), HistoryDir::Incoming},
      {QT_TR_NOOP("Без ответа"), HistoryDir::Missed},
      {QT_TR_NOOP("Исходящие"), HistoryDir::Outgoing},
  };
  for (const auto &def : dirButtons) {
    auto *btn = new QPushButton(tr(def.label));
    btn->setObjectName(QStringLiteral("filterBtn"));
    btn->setCheckable(true);
    btn->setChecked(def.mode == HistoryDir::All);
    m_historyDirGroup->addButton(btn, static_cast<int>(def.mode));
    historyFilterRow->addWidget(btn, 1);
  }
  historyLayout->addLayout(historyFilterRow);

  m_historySearchEdit = new QLineEdit;
  m_historySearchEdit->setObjectName(QStringLiteral("searchEdit"));
  m_historySearchEdit->setPlaceholderText(tr("Поиск в истории звонков"));
  m_historySearchEdit->setClearButtonEnabled(true);
  historyLayout->addWidget(m_historySearchEdit);

  m_historyList = new QListWidget;
  m_historyList->setObjectName(QStringLiteral("historyList"));
  m_historyList->setFrameShape(QFrame::NoFrame);
  applyHistoryListPalette(m_historyList, palette());
  historyLayout->addWidget(m_historyList, 1);

  auto *historyScopeRow = new QHBoxLayout;
  m_historyScopeGroup = new QButtonGroup(this);
  const struct {
    const char *label;
    HistoryScope mode;
  } scopeButtons[] = {
      {QT_TR_NOOP("Мои звонки"), HistoryScope::Mine},
      {QT_TR_NOOP("Звонки компании"), HistoryScope::Company},
      {QT_TR_NOOP("Внутренние звонки"), HistoryScope::Internal},
  };
  for (const auto &def : scopeButtons) {
    auto *btn = new QPushButton(tr(def.label));
    btn->setObjectName(QStringLiteral("filterBtn"));
    btn->setCheckable(true);
    btn->setChecked(def.mode == HistoryScope::Mine);
    m_historyScopeGroup->addButton(btn, static_cast<int>(def.mode));
    historyScopeRow->addWidget(btn, 1);
  }
  historyLayout->addLayout(historyScopeRow);

  m_tabs->addTab(historyPage, tr("История"));

  auto *footer = new QWidget;
  footer->setObjectName(QStringLiteral("mainFooter"));
  auto *footerLayout = new QHBoxLayout(footer);
  auto *addBtn = new QPushButton(tr("Добавить"));
  addBtn->setObjectName(QStringLiteral("footerBtn"));
  auto *addMenu = new QMenu(addBtn);
  addMenu->addAction(tr("Контакт"), this, &MainWindow::onAddContact);
  addMenu->addAction(tr("Импорт"), this, &MainWindow::onImportContacts);
  addBtn->setMenu(addMenu);
  auto *confBtn = new QPushButton(tr("Конференция"));
  confBtn->setObjectName(QStringLiteral("footerBtn"));
  auto *viewBtn = new QPushButton(tr("Вид"));
  viewBtn->setObjectName(QStringLiteral("footerBtn"));
  m_viewMenu = new QMenu(viewBtn);
  m_viewChatAction = m_viewMenu->addAction(tr("Кнопка сообщений"));
  m_viewChatAction->setCheckable(true);
  connect(m_viewMenu, &QMenu::aboutToShow, this, [this]() {
    if (m_viewChatAction) {
      m_viewChatAction->setChecked(m_client->appSettings().showChatButtons());
    }
  });
  connect(m_viewChatAction, &QAction::triggered, this, [this](bool checked) {
    m_client->appSettings().setShowChatButtons(checked);
    m_client->saveSettings();
    applyContactViewSettings();
  });
  viewBtn->setMenu(m_viewMenu);
  footerLayout->addWidget(addBtn);
  footerLayout->addWidget(confBtn);
  footerLayout->addWidget(viewBtn);
  root->addWidget(footer);

  connect(settingsBtn, &QPushButton::clicked, this, &MainWindow::onSettings);
  connect(helpBtn, &QPushButton::clicked, this, &MainWindow::onHelp);
  connect(confBtn, &QPushButton::clicked, this, &MainWindow::onConference);
  connect(m_dialCallBtn, &QPushButton::clicked, this, &MainWindow::onDial);
  connect(m_dialInput, &QLineEdit::returnPressed, this, &MainWindow::onDial);
  connect(m_searchEdit, &QLineEdit::textChanged, this, &MainWindow::onSearchChanged);
  connect(m_contactsList, &QListWidget::currentItemChanged, this, [this]() { onContactSelected(); });
  connect(m_presenceSelector, &PresenceSelector::currentIndexChanged, this, &MainWindow::onPresenceChanged);
  connect(m_filterGroup, &QButtonGroup::idClicked, this, &MainWindow::onFilterChanged);
  connect(m_historyDirGroup, &QButtonGroup::idClicked, this, &MainWindow::onHistoryDirChanged);
  connect(m_historyScopeGroup, &QButtonGroup::idClicked, this, &MainWindow::onHistoryScopeChanged);
  connect(m_historyPeriodBtn, &QPushButton::clicked, this, &MainWindow::onHistoryPeriodClicked);
  connect(m_historySearchEdit, &QLineEdit::textChanged, this, &MainWindow::onHistorySearchChanged);
  connect(m_historyList, &QListWidget::itemClicked, this, &MainWindow::onHistoryItemActivated);
  connect(m_headerAvatar, &ProfileAvatarWidget::settingsChanged, this, &MainWindow::onProfileAvatarChanged);

  connect(m_callWindow, &CallWindow::hangupRequested, this, &MainWindow::onHangup);
  connect(m_callWindow, &CallWindow::answerRequested, this, &MainWindow::onAnswer);
  connect(m_callWindow, &CallWindow::holdRequested, this, &MainWindow::onHold);
  connect(m_callWindow, &CallWindow::transferRequested, this, &MainWindow::onTransfer);
  connect(m_callWindow, &CallWindow::notesChanged, this, &MainWindow::onCallNotesChanged);

  connect(m_client, &itl::CommunicatorClient::statusMessage, this, &MainWindow::onStatusMessage);
  connect(m_client, &itl::CommunicatorClient::contactUpdated, this, &MainWindow::onContactUpdated);
  connect(m_client, &itl::CommunicatorClient::addressBookChanged, this, &MainWindow::onAddressBookChanged);
  connect(m_client->addressBook(), &itl::AddressBookManager::deleteFailed, this,
          [this](const QString &, const QString &reason) {
            const QString message = reason.isEmpty()
                ? tr("Не удалось удалить контакт на сервере.")
                : tr("Не удалось удалить контакт на сервере: %1").arg(reason);
            QMessageBox::warning(this, tr("Удалить контакт"), message);
          });
  connect(m_client, &itl::CommunicatorClient::chatMessage, this, &MainWindow::onIncomingChatMessage);
  connect(m_client->chat(), &itl::ChatManager::unreadChanged, this, [this](const QString &) {
    updateUnreadIndicators();
  });
  connect(m_client, &itl::CommunicatorClient::callEvent, this, &MainWindow::onCallEvent);
  connect(m_client->api(), &itl::WsApiClient::domainContactsLoaded, this, &MainWindow::onContactsLoaded);
  connect(m_client->api(), &itl::WsApiClient::historyLoaded, this, &MainWindow::onServerHistoryLoaded);
  connect(m_calls, &itl::CallManager::callStateChanged, this, &MainWindow::onCallStateChanged);
  connect(m_calls, &itl::CallManager::callRecordingFinished, this, [this](const QString &path) {
    onStatusMessage(tr("Запись сохранена: %1").arg(path));
  });
  connect(m_calls, &itl::CallManager::remoteAudioStarted, this, [this](const QString &leg) {
    markCallConnected(leg);
    m_callWindow->beginConversationTimer();
  });

  m_client->loadSettings();
  m_messageNotify->applySettings(&m_client->appSettings());
  mergeCustomContacts();
  updateSelfHeader();
  updateHistoryPeriodLabel();
  updateHistoryButtonStyles();
  updateDialCallButtonStyle();
  rebuildHistoryList();
  setOnlineUi(false);
  setupDragDrop();
  QTimer::singleShot(0, this, &MainWindow::startSession);
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

  if (m_dialKeypad) {
    m_dialKeypad->refreshAppearance();
  }

  updateDialCallButtonStyle();
  updateFilterButtonStyles();
  updateHistoryButtonStyles();
  for (QPushButton *linkBtn : findChildren<QPushButton *>(QStringLiteral("linkButton"))) {
    applyLinkButtonStyle(linkBtn);
  }
  update();
}

void MainWindow::setOnlineUi(bool online)
{
  m_online = online;
  m_dialInput->setEnabled(online);
  m_dialCallBtn->setEnabled(online);
  if (m_dialKeypad) {
    m_dialKeypad->setEnabled(online);
  }
  m_searchEdit->setEnabled(online);
  m_presenceSelector->setEnabled(online);
  m_contactsList->setEnabled(online);
  if (online) {
    m_presenceSelector->setCurrentStatus(QStringLiteral("online"));
    refreshServerHistory();
  } else {
    m_presenceSelector->setCurrentStatus(QStringLiteral("offline"));
    m_serverHistory.clear();
    m_companyHistory.clear();
    m_internalHistory.clear();
    m_historyRequestId = -1;
    m_companyHistoryRequestId = -1;
    m_internalHistoryRequestId = -1;
    m_historyLoading = false;
    m_companyHistoryLoading = false;
    m_internalHistoryLoading = false;
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

QString MainWindow::recordingNameForPeer(const QString &peer, const QString &fallbackDisplayName) const
{
  const QString resolved = resolvePeer(peer);
  const ContactEntry entry = m_contacts.value(resolved.isEmpty() ? peer : resolved);

  if (!entry.name.isEmpty()) {
    return entry.name;
  }
  if (!fallbackDisplayName.isEmpty() && !fallbackDisplayName.contains(QLatin1Char('@'))) {
    return fallbackDisplayName;
  }

  if (!entry.ext.isEmpty()) {
    return entry.ext;
  }
  if (!entry.phone.isEmpty()) {
    return entry.phone;
  }
  if (!entry.personalPhone.isEmpty()) {
    return entry.personalPhone;
  }

  const QString target = resolved.isEmpty() ? peer : resolved;
  if (!target.contains(QLatin1Char('@'))) {
    return target;
  }
  return target.section(QLatin1Char('@'), 0, 0);
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
  auto *row = new ContactRowWidget(peer, entry.name, entry.ext, entry.phone, entry.presence, entry.isSelf,
                                  entry.isCustom);
  if (!entry.isSelf) {
    row->setCallNumbers(callNumbersForPeer(peer));
  }
  row->setChatButtonVisible(m_client->appSettings().showChatButtons());
  row->setUnreadBlink(m_client->appSettings().showChatButtons() && m_client->chat()->hasUnread(peer));
  auto *item = new QListWidgetItem;
  item->setData(Qt::UserRole, peer);
  item->setSizeHint(QSize(0, entry.ext.isEmpty() && entry.phone.isEmpty() ? 48 : 56));
  m_contactsList->addItem(item);
  m_contactsList->setItemWidget(item, row);
  m_contactItems.insert(peer, item);
  connect(row, &ContactRowWidget::callRequested, this, &MainWindow::onCallFromRow);
  connect(row, &ContactRowWidget::callNumberRequested, this, [this](const QString &number) {
    const QString target = resolvePeer(number);
    if (!target.isEmpty()) {
      onCallFromRow(target);
    }
  });
  connect(row, &ContactRowWidget::chatRequested, this, &MainWindow::onChatFromRow);
  connect(row, &ContactRowWidget::notesRequested, this, &MainWindow::onNotesFromRow);
  connect(row, &ContactRowWidget::deleteRequested, this, &MainWindow::onDeleteContactFromRow);
  connect(row, &ContactRowWidget::exportRequested, this, &MainWindow::onExportContactFromRow);
}

QVector<ContactRowWidget::CallNumber> MainWindow::callNumbersForPeer(const QString &peer) const
{
  const ContactEntry entry = m_contacts.value(peer);
  QVector<ContactRowWidget::CallNumber> numbers;
  if (!entry.ext.isEmpty()) {
    numbers.append({tr("Внутренний номер"), entry.ext});
  }
  if (!entry.phone.isEmpty()) {
    numbers.append({tr("Мобильный номер"), entry.phone});
  }
  if (!entry.personalPhone.isEmpty() && entry.personalPhone != entry.phone) {
    numbers.append({tr("Личный мобильный"), entry.personalPhone});
  }
  return numbers;
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

void MainWindow::updateDialCallButtonStyle()
{
  if (!m_dialCallBtn) {
    return;
  }

  const QPalette appPalette = QApplication::palette(m_dialCallBtn);
  const QColor accent = appPalette.color(QPalette::Highlight);
  const QColor labelColor = textOnAccentBackground(accent);
  const QColor pressed = accent.darker(108);
  const QColor pressedLabel = textOnAccentBackground(pressed);
  const QColor hoverBorder = accent.lighter(125);
  const QColor disabledBg = appPalette.color(QPalette::Disabled, QPalette::Button);
  const QColor disabledText = appPalette.color(QPalette::Disabled, QPalette::ButtonText);

  m_dialCallBtn->setAutoFillBackground(false);
  m_dialCallBtn->setPalette(QApplication::palette(m_dialCallBtn));
  m_dialCallBtn->setStyleSheet(QStringLiteral(
      "QPushButton#dialCallBtn {"
      "  background-color: %1;"
      "  color: %2;"
      "  border: 2px solid %1;"
      "  border-radius: 8px;"
      "  min-height: 40px;"
      "  font-size: 15px;"
      "  font-weight: bold;"
      "}"
      "QPushButton#dialCallBtn:hover {"
      "  background-color: %1;"
      "  color: %2;"
      "  border: 2px solid %6;"
      "}"
      "QPushButton#dialCallBtn:pressed {"
      "  background-color: %3;"
      "  color: %4;"
      "  border: 2px solid %3;"
      "}"
      "QPushButton#dialCallBtn:disabled {"
      "  background-color: %5;"
      "  color: %7;"
      "  border: 2px solid %5;"
      "}")
      .arg(accent.name(), labelColor.name(), pressed.name(), pressedLabel.name(), disabledBg.name(),
           hoverBorder.name(), disabledText.name()));

  m_dialCallBtn->style()->unpolish(m_dialCallBtn);
  m_dialCallBtn->style()->polish(m_dialCallBtn);
  m_dialCallBtn->update();
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

QString MainWindow::formatHistoryWhen(qint64 ms)
{
  if (ms <= 0) {
    return {};
  }
  const QDateTime dt = QDateTime::fromMSecsSinceEpoch(ms);
  const QDate today = QDate::currentDate();
  const QString time = dt.toString(QStringLiteral("HH:mm"));
  if (dt.date() == today) {
    return tr("сегодня, %1").arg(time);
  }
  if (dt.date() == today.addDays(-1)) {
    return tr("вчера, %1").arg(time);
  }
  return dt.toString(QStringLiteral("dd.MM.yy, HH:mm"));
}

void MainWindow::onHistoryDirChanged(int id)
{
  m_historyDir = static_cast<HistoryDir>(id);
  updateHistoryButtonStyles();
  refreshServerHistory();
  rebuildHistoryList();
}

void MainWindow::onHistoryScopeChanged(int id)
{
  m_historyScope = static_cast<HistoryScope>(id);
  updateHistoryButtonStyles();
  refreshServerHistory();
  rebuildHistoryList();
}

void MainWindow::onHistorySearchChanged(const QString &text)
{
  m_historySearch = text.trimmed();
  refreshServerHistory();
  rebuildHistoryList();
}

void MainWindow::applyLinkButtonStyle(QPushButton *button) const
{
  if (!button) {
    return;
  }
  QPalette pal = QApplication::palette(button);
  pal.setColor(QPalette::Button, Qt::transparent);
  pal.setColor(QPalette::ButtonText, pal.color(QPalette::Link));
  button->setPalette(pal);
  button->setStyleSheet({});
  button->style()->unpolish(button);
  button->style()->polish(button);
}

void MainWindow::onHistoryPeriodClicked()
{
  QMenu menu(this);
  const struct {
    const char *label;
    HistoryPeriod period;
  } items[] = {
      {QT_TR_NOOP("сегодня"), HistoryPeriod::Today},
      {QT_TR_NOOP("текущую неделю"), HistoryPeriod::Week},
      {QT_TR_NOOP("текущий месяц"), HistoryPeriod::Month},
      {QT_TR_NOOP("всё время"), HistoryPeriod::AllTime},
  };
  for (const auto &item : items) {
    QAction *action = menu.addAction(tr(item.label));
    QFont font = action->font();
    font.setBold(item.period == m_historyPeriod);
    action->setFont(font);
    const HistoryPeriod period = item.period;
    connect(action, &QAction::triggered, this, [this, period]() {
      m_historyPeriod = period;
      updateHistoryPeriodLabel();
      refreshServerHistory();
      rebuildHistoryList();
    });
  }
  menu.exec(m_historyPeriodBtn->mapToGlobal(QPoint(0, m_historyPeriodBtn->height())));
}

void MainWindow::updateHistoryPeriodLabel()
{
  if (!m_historyPeriodBtn) {
    return;
  }
  QString text;
  switch (m_historyPeriod) {
  case HistoryPeriod::Today:
    text = tr("сегодня");
    break;
  case HistoryPeriod::Week:
    text = tr("текущую неделю");
    break;
  case HistoryPeriod::Month:
    text = tr("текущий месяц");
    break;
  case HistoryPeriod::AllTime:
    text = tr("всё время");
    break;
  }
  m_historyPeriodBtn->setText(text);
  applyLinkButtonStyle(m_historyPeriodBtn);
}

QJsonObject MainWindow::buildHistoryRequest(HistoryScope scope) const
{
  QJsonObject request{
      {QString::fromUtf8(itl::kEmptyKey), QStringLiteral("gethistory")},
      {QStringLiteral("CallType"), QStringLiteral("all")},
      {QStringLiteral("splitout"), 1},
      {QStringLiteral("Limit"), 100},
  };

  switch (m_historyDir) {
  case HistoryDir::Incoming:
    request.insert(QStringLiteral("CallType"), QStringLiteral("in"));
    break;
  case HistoryDir::Outgoing:
    request.insert(QStringLiteral("CallType"), QStringLiteral("out"));
    break;
  case HistoryDir::Missed:
    request.insert(QStringLiteral("CallType"), QStringLiteral("missed"));
    break;
  case HistoryDir::All:
    break;
  }

  switch (scope) {
  case HistoryScope::Mine:
    // Только звонки текущего пользователя.
    request.insert(QStringLiteral("owner"), QStringLiteral("my"));
    break;
  case HistoryScope::Company:
    // Все звонки сотрудников компании.
    break;
  case HistoryScope::Internal:
    // Внутренние звонки — отдельный запрос, как в официальном клиенте.
    request.insert(QStringLiteral("inner"), true);
    request.insert(QStringLiteral("owner"), QStringLiteral("my"));
    break;
  }

  const QDateTime now = QDateTime::currentDateTimeUtc();
  QDateTime start = now;
  switch (m_historyPeriod) {
  case HistoryPeriod::Today:
    start = QDateTime(QDate(now.date()), QTime(0, 0), Qt::UTC);
    break;
  case HistoryPeriod::Week:
    start = QDateTime(now.date().addDays(-(now.date().dayOfWeek() - 1)), QTime(0, 0), Qt::UTC);
    break;
  case HistoryPeriod::Month:
    start = QDateTime(QDate(now.date().year(), now.date().month(), 1), QTime(0, 0), Qt::UTC);
    break;
  case HistoryPeriod::AllTime:
    start = QDateTime(QDate(2000, 1, 1), QTime(0, 0), Qt::UTC);
    break;
  }
  request.insert(QStringLiteral("start"), start.toString(Qt::ISODateWithMs));
  request.insert(QStringLiteral("end"), now.toString(Qt::ISODateWithMs));

  if (!m_historySearch.isEmpty()) {
    request.insert(QStringLiteral("search"), QJsonObject{{QStringLiteral("query"), m_historySearch}});
  }

  return request;
}

void MainWindow::refreshServerHistory()
{
  if (m_demoMode || !m_online || m_client->api()->appState() != itl::AppState::Online) {
    return;
  }

  if (m_historyScope == HistoryScope::Company && !m_companyHistory.isEmpty()) {
    m_historyLoading = false;
    rebuildHistoryList();
    return;
  }

  if (m_historyScope == HistoryScope::Internal && !m_internalHistory.isEmpty()) {
    m_historyLoading = false;
    rebuildHistoryList();
    return;
  }

  m_historyRequestScope = m_historyScope;
  const int requestId = m_client->api()->getHistory(buildHistoryRequest(m_historyRequestScope));
  if (requestId >= 0) {
    m_historyRequestId = requestId;
    m_historyLoading = true;
    rebuildHistoryList();
  } else {
    m_historyLoading = false;
  }
}

void MainWindow::prefetchCompanyHistory()
{
  if (m_demoMode || !m_online || m_client->api()->appState() != itl::AppState::Online) {
    return;
  }
  if (!m_companyHistory.isEmpty() || m_companyHistoryLoading) {
    return;
  }

  const int requestId = m_client->api()->getHistory(buildHistoryRequest(HistoryScope::Company));
  if (requestId >= 0) {
    m_companyHistoryRequestId = requestId;
    m_companyHistoryLoading = true;
  }
}

void MainWindow::prefetchInternalHistory()
{
  if (m_demoMode || !m_online || m_client->api()->appState() != itl::AppState::Online) {
    return;
  }
  if (!m_internalHistory.isEmpty() || m_internalHistoryLoading) {
    return;
  }

  const int requestId = m_client->api()->getHistory(buildHistoryRequest(HistoryScope::Internal));
  if (requestId >= 0) {
    m_internalHistoryRequestId = requestId;
    m_internalHistoryLoading = true;
  }
}

void MainWindow::onServerHistoryLoaded(int requestId, const QJsonObject &response)
{
  enum class Bucket { None, Mine, Company, Internal };
  Bucket bucket = Bucket::None;
  if (requestId == m_companyHistoryRequestId) {
    bucket = Bucket::Company;
  } else if (requestId == m_internalHistoryRequestId) {
    bucket = Bucket::Internal;
  } else if (requestId == m_historyRequestId) {
    if (m_historyRequestScope == HistoryScope::Company) {
      bucket = Bucket::Company;
    } else if (m_historyRequestScope == HistoryScope::Internal) {
      bucket = Bucket::Internal;
    } else {
      bucket = Bucket::Mine;
    }
  }
  if (bucket == Bucket::None) {
    return;
  }

  const QJsonObject inner = response.value(QString::fromUtf8(itl::kEmptyKey)).toObject();
  const QJsonObject payload = inner.value(QStringLiteral("response")).toObject();

  auto finishLoading = [&](Bucket target) {
    switch (target) {
    case Bucket::Company:
      m_companyHistoryLoading = false;
      m_companyHistoryRequestId = -1;
      break;
    case Bucket::Internal:
      m_internalHistoryLoading = false;
      m_internalHistoryRequestId = -1;
      break;
    case Bucket::Mine:
      m_historyLoading = false;
      m_historyRequestId = -1;
      break;
    default:
      break;
    }
  };

  if (payload.contains(QStringLiteral("error"))) {
    qCWarning(lcHistory) << "gethistory error:" << payload.value(QStringLiteral("error")).toString()
                         << "bucket" << static_cast<int>(bucket);
    if (bucket == Bucket::Company) {
      m_companyHistory.clear();
    } else if (bucket == Bucket::Internal) {
      m_internalHistory.clear();
    } else {
      m_serverHistory.clear();
    }
    finishLoading(bucket);
    rebuildHistoryList();
    return;
  }

  const QJsonObject result = payload.value(QStringLiteral("result")).toObject();
  itl::CallHistoryParseContext context;
  context.domain = m_client->credentials().domain;
  context.selfPeer = m_selfPeer;
  context.selfLogin = m_client->credentials().login.section(QLatin1Char('@'), 0, 0);
  const QList<itl::CallHistoryEntry> parsed =
      itl::parseServerCallHistory(result.value(QStringLiteral("Calls")), context);

  if (bucket == Bucket::Company) {
    m_companyHistory = parsed;
    qCInfo(lcHistory) << "Company history loaded:" << parsed.size() << "entries";
  } else if (bucket == Bucket::Internal) {
    m_internalHistory = parsed;
    qCInfo(lcHistory) << "Internal history loaded:" << parsed.size() << "entries";
  } else {
    m_serverHistory = parsed;
    qCInfo(lcHistory) << "Mine history loaded:" << parsed.size() << "entries";
  }

  finishLoading(bucket);
  rebuildHistoryList();
}

QList<itl::CallHistoryEntry> MainWindow::currentHistoryEntries() const
{
  if (m_demoMode) {
    return m_demoCallHistory;
  }
  if (m_historyScope == HistoryScope::Company) {
    return m_companyHistory;
  }
  if (m_historyScope == HistoryScope::Internal) {
    if (!m_internalHistory.isEmpty()) {
      return m_internalHistory;
    }
    // Запасной вариант: отбор коротких номеров из истории компании.
    return m_companyHistory;
  }
  if (m_online && m_client->api()->appState() == itl::AppState::Online && !m_serverHistory.isEmpty()) {
    return m_serverHistory;
  }
  if (m_historyScope == HistoryScope::Mine) {
    const QList<itl::CallHistoryEntry> local = m_client->appSettings().callHistory();
    if (!local.isEmpty()) {
      return local;
    }
  }
  return m_serverHistory;
}

bool MainWindow::historyEntryMatches(const itl::CallHistoryEntry &entry) const
{
  const bool incoming = entry.direction == QStringLiteral("incoming");
  switch (m_historyDir) {
  case HistoryDir::All:
    break;
  case HistoryDir::Incoming:
    if (!incoming) {
      return false;
    }
    break;
  case HistoryDir::Outgoing:
    if (incoming) {
      return false;
    }
    break;
  case HistoryDir::Missed:
    if (!(incoming && !entry.answered)) {
      return false;
    }
    break;
  }

  const QString domain = m_client->credentials().domain;
  switch (m_historyScope) {
  case HistoryScope::Mine:
    break;
  case HistoryScope::Company:
    break;
  case HistoryScope::Internal:
    // Данные уже с сервера (inner:true); дополнительно отсеиваем не-короткие номера.
    if (!entry.isInnerCall && !itl::historyEntryIsInternal(entry, domain)) {
      return false;
    }
    break;
  }

  const QDateTime now = QDateTime::currentDateTime();
  const QDateTime started = QDateTime::fromMSecsSinceEpoch(entry.startedAtMs);
  switch (m_historyPeriod) {
  case HistoryPeriod::Today:
    if (started.date() != now.date()) {
      return false;
    }
    break;
  case HistoryPeriod::Week: {
    QDate weekStart = now.date().addDays(-(now.date().dayOfWeek() - 1));
    if (started.date() < weekStart) {
      return false;
    }
    break;
  }
  case HistoryPeriod::Month:
    if (started.date().year() != now.date().year()
        || started.date().month() != now.date().month()) {
      return false;
    }
    break;
  case HistoryPeriod::AllTime:
    break;
  }

  if (!m_historySearch.isEmpty()) {
    const QString name = entry.displayName.isEmpty() ? entry.peer : entry.displayName;
    if (!name.contains(m_historySearch, Qt::CaseInsensitive)
        && !entry.peer.contains(m_historySearch, Qt::CaseInsensitive)) {
      return false;
    }
  }

  return true;
}

void MainWindow::rebuildHistoryList()
{
  m_historyList->clear();
  const QList<itl::CallHistoryEntry> history = currentHistoryEntries();

  if ((m_historyLoading || m_companyHistoryLoading || m_internalHistoryLoading) && history.isEmpty()
      && m_online && !m_demoMode) {
    auto *placeholder = new QListWidgetItem(tr("Загрузка истории..."));
    placeholder->setFlags(Qt::NoItemFlags);
    m_historyList->addItem(placeholder);
    return;
  }

  int shown = 0;
  for (const itl::CallHistoryEntry &entry : history) {
    if (!historyEntryMatches(entry)) {
      continue;
    }
    ++shown;

    const bool incoming = entry.direction == QStringLiteral("incoming");
    const bool missed = incoming && !entry.answered;
    const QString arrow = incoming ? QStringLiteral("↙") : QStringLiteral("↗");
    QString arrowColor;
    if (missed) {
      arrowColor = QStringLiteral("#c0392b");
    } else if (incoming) {
      arrowColor = QStringLiteral("#27ae60");
    } else {
      arrowColor = QStringLiteral("#2b7bd6");
    }

    // Время ожидания ответа (гудки).
    qint64 waitMs = 0;
    if (entry.answered && entry.connectedAtMs > 0) {
      waitMs = entry.connectedAtMs - entry.startedAtMs;
    } else if (entry.endedAtMs > 0) {
      waitMs = entry.endedAtMs - entry.startedAtMs;
    }
    const int waitSec = waitMs > 0 ? static_cast<int>((waitMs + 500) / 1000) : 0;

    QString status;
    if (entry.result == QStringLiteral("transferred") && !entry.transferTo.isEmpty()) {
      status = tr("переведён на %1").arg(entry.transferTo);
    } else if (entry.answered && entry.durationSec > 0) {
      status = tr("%1 сек.").arg(entry.durationSec);
    } else if (missed) {
      status = tr("пропущенный");
    } else if (!incoming && !entry.answered) {
      status = tr("не отвечено");
    }

    const QString name = entry.displayName.isEmpty() ? entry.peer : entry.displayName;

    auto *rowWidget = new QWidget;
    auto *rowLayout = new QHBoxLayout(rowWidget);
    rowLayout->setContentsMargins(6, 4, 6, 4);
    rowLayout->setSpacing(8);

    auto *arrowLabel = new QLabel(arrow);
    arrowLabel->setStyleSheet(QStringLiteral("color:%1; font-size:16px; font-weight:bold;").arg(arrowColor));
    arrowLabel->setFixedWidth(18);
    arrowLabel->setAlignment(Qt::AlignTop | Qt::AlignHCenter);
    rowLayout->addWidget(arrowLabel);

    auto *textCol = new QVBoxLayout;
    textCol->setSpacing(1);
    QString firstLine = name;
    if (waitSec > 0) {
      firstLine += tr("; ждал: %1 сек.").arg(waitSec);
    }
    auto *nameLabel = new QLabel(firstLine);
    QFont nameFont = nameLabel->font();
    nameFont.setBold(missed);
    nameLabel->setFont(nameFont);
    textCol->addWidget(nameLabel);

    QString secondLine;
    if (m_historyScope == HistoryScope::Company && !entry.employeeInfo.isEmpty()) {
      secondLine = entry.employeeInfo;
    } else if (m_historyScope == HistoryScope::Internal) {
      secondLine = entry.peer;
    } else {
      secondLine = entry.peer;
    }
    if (!status.isEmpty()) {
      secondLine += QStringLiteral("; ") + status;
    }
    auto *detailLabel = new QLabel(secondLine);
    QFont detailFont = detailLabel->font();
    detailFont.setPixelSize(11);
    detailLabel->setFont(detailFont);
    QPalette detailPalette = detailLabel->palette();
    detailPalette.setColor(QPalette::WindowText, palette().color(QPalette::Link));
    detailLabel->setPalette(detailPalette);
    textCol->addWidget(detailLabel);
    rowLayout->addLayout(textCol, 1);

    auto *dateLabel = new QLabel(formatHistoryWhen(entry.startedAtMs));
    dateLabel->setAlignment(Qt::AlignTop | Qt::AlignRight);
    QFont dateFont = dateLabel->font();
    dateFont.setPixelSize(11);
    dateLabel->setFont(dateFont);
    rowLayout->addWidget(dateLabel);

    auto *item = new QListWidgetItem;
    item->setData(Qt::UserRole, entry.peer);
    item->setSizeHint(QSize(0, 46));
    m_historyList->addItem(item);
    m_historyList->setItemWidget(item, rowWidget);
  }

  if (shown == 0) {
    auto *placeholder = new QListWidgetItem(history.isEmpty()
                                                ? tr("История звонков пуста")
                                                : tr("Нет звонков по выбранному фильтру"));
    placeholder->setFlags(Qt::NoItemFlags);
    m_historyList->addItem(placeholder);
  }
}

void MainWindow::runHistorySelfTest()
{
  auto countForScope = [this](HistoryScope scope) {
    const HistoryScope saved = m_historyScope;
    m_historyScope = scope;
    int shown = 0;
    for (const itl::CallHistoryEntry &entry : currentHistoryEntries()) {
      if (historyEntryMatches(entry)) {
        ++shown;
      }
    }
    m_historyScope = saved;
    return shown;
  };

  const int mine = countForScope(HistoryScope::Mine);
  const int company = countForScope(HistoryScope::Company);
  const int internal = countForScope(HistoryScope::Internal);

  qCInfo(lcHistory) << "SELFTEST demo=" << m_demoMode << "mine=" << mine << "company=" << company
                    << "internal=" << internal << "demoEntries=" << m_demoCallHistory.size();

  const bool ok = m_demoMode && mine == 4 && company == 4 && internal == 1;
  if (!ok) {
    qCCritical(lcHistory) << "SELFTEST FAILED";
    QApplication::exit(1);
    return;
  }

  qCInfo(lcHistory) << "SELFTEST OK";
  QApplication::exit(0);
}

void MainWindow::updateHistoryButtonStyles()
{
  const QPalette appPalette = QApplication::palette();
  const QColor accent = appPalette.color(QPalette::Highlight);
  const QColor accentText = appPalette.color(QPalette::HighlightedText);

  for (QButtonGroup *group : {m_historyDirGroup, m_historyScopeGroup}) {
    if (!group) {
      continue;
    }
    for (QAbstractButton *button : group->buttons()) {
      auto *btn = qobject_cast<QPushButton *>(button);
      if (!btn) {
        continue;
      }
      if (btn->isChecked()) {
        QPalette pal = QApplication::palette(btn);
        pal.setColor(QPalette::Button, accent);
        pal.setColor(QPalette::ButtonText, accentText);
        pal.setColor(QPalette::Light, accent.lighter(115));
        pal.setColor(QPalette::Midlight, accent.lighter(105));
        pal.setColor(QPalette::Mid, accent.darker(105));
        pal.setColor(QPalette::Dark, accent.darker(125));
        btn->setPalette(pal);
      } else {
        btn->setPalette(QApplication::palette(btn));
      }
      btn->style()->unpolish(btn);
      btn->style()->polish(btn);
      btn->update();
    }
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
  m_calls->setRecordingName(leg, recordingNameForPeer(peer, displayName));
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
  dlg.setShowCallAction(true);
  dlg.setDuringCall(activeCallPeer);
  const int result = dlg.exec();
  if (result == NotePopupDialog::CallResult) {
    onCallFromRow(peer);
    return;
  }
  if (activeCallPeer) {
    loadCallNotes(peer);
    m_callWindow->setNotesVisible(true);
  }
}

void MainWindow::onHistoryItemActivated(QListWidgetItem *item)
{
  if (!item || !(item->flags() & Qt::ItemIsEnabled)) {
    return;
  }

  const QString peer = item->data(Qt::UserRole).toString();
  if (peer.isEmpty()) {
    return;
  }

  QString displayName = displayNameForPeer(peer);
  if (displayName == peer.section(QLatin1Char('@'), 0, 0) || displayName == peer) {
    // Prefer the name stored with the history entry when contact book has no RealName.
    for (const itl::CallHistoryEntry &entry : currentHistoryEntries()) {
      if (entry.peer == peer && !entry.displayName.isEmpty()) {
        displayName = entry.displayName;
        break;
      }
    }
  }

  NotePopupDialog dlg(peer, displayName, &m_client->appSettings(), this);
  dlg.setShowCallAction(false);
  dlg.exec();
}

void MainWindow::onDeleteContactFromRow(const QString &peer)
{
  const ContactEntry entry = m_contacts.value(peer);
  if (!entry.isCustom || entry.isSelf) {
    return;
  }

  const QString name = entry.name.isEmpty() ? peer.section(QLatin1Char('@'), 0, 0) : entry.name;
  if (QMessageBox::question(this, tr("Удалить контакт"),
                            tr("Удалить контакт «%1»?").arg(name))
      != QMessageBox::Yes) {
    return;
  }

  if (useServerContacts()) {
    if (m_client->addressBook()->deleteContactByPeer(peer) < 0) {
      QMessageBox::warning(this, tr("Удалить контакт"), tr("Не удалось удалить контакт на сервере."));
      return;
    }
    return;
  }

  m_client->appSettings().removeCustomContact(peer);
  m_client->saveSettings();
  m_contacts.remove(peer);
  rebuildContactList();
}

void MainWindow::onExportContactFromRow(const QString &peer)
{
  const ContactEntry entry = m_contacts.value(peer);
  const QString displayName = entry.name.isEmpty() ? peer.section(QLatin1Char('@'), 0, 0) : entry.name;
  QString defaultBase = displayName.trimmed();
  defaultBase.replace(QRegularExpression(QStringLiteral("\\s+")), QStringLiteral("_"));

  const QString path = QFileDialog::getSaveFileName(
      this, tr("Экспорт контакта"), defaultBase,
      tr("vCard (*.vcf);;CSV (*.csv)"));
  if (path.isEmpty()) {
    return;
  }

  QFile file(path);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
    QMessageBox::warning(this, tr("Экспорт"), tr("Не удалось создать файл."));
    return;
  }

  QTextStream out(&file);
  if (path.endsWith(QStringLiteral(".csv"), Qt::CaseInsensitive)) {
    out << displayName << QLatin1Char(',') << entry.phone << QLatin1Char(',') << entry.ext << QLatin1Char('\n');
  } else {
    out << QStringLiteral("BEGIN:VCARD\n");
    out << QStringLiteral("VERSION:3.0\n");
    out << QStringLiteral("FN:") << displayName << QLatin1Char('\n');
    if (!entry.ext.isEmpty()) {
      out << QStringLiteral("TEL;TYPE=WORK:") << entry.ext << QLatin1Char('\n');
    }
    if (!entry.phone.isEmpty()) {
      out << QStringLiteral("TEL;TYPE=CELL:") << entry.phone << QLatin1Char('\n');
    }
    if (!entry.personalPhone.isEmpty()) {
      out << QStringLiteral("TEL;TYPE=HOME:") << entry.personalPhone << QLatin1Char('\n');
    }
    out << QStringLiteral("END:VCARD\n");
  }

  file.close();
  onStatusMessage(tr("Контакт экспортирован: %1").arg(path));
}

void MainWindow::startSession()
{
  m_client->loadSettings();

  if (qEnvironmentVariableIsSet("OSC_SELFTEST")) {
    itl::LoginCredentials demoCred;
    demoCred.login = QStringLiteral("demo");
    demoCred.password = QStringLiteral("demo");
    m_client->setCredentials(demoCred);
    beginSessionWithCurrentCredentials();
    QTimer::singleShot(300, this, &MainWindow::runHistorySelfTest);
    return;
  }

  if (m_client->rememberMe()) {
    const itl::LoginCredentials cred = m_client->credentials();
    const bool hasAccount = !cred.login.trimmed().isEmpty() && !cred.password.isEmpty()
        && !itl::DemoData::isDemoCredentials(cred.login, cred.password);
    if (hasAccount) {
      beginSessionWithCurrentCredentials();
      return;
    }
  }

  onLogin();
}

void MainWindow::beginSessionWithCurrentCredentials()
{
  if (m_demoMode) {
    exitDemoInterface();
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

  beginSessionWithCurrentCredentials();
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
      m_callWindow->beginConversationTimer();
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
  m_messageNotify->applySettings(&m_client->appSettings());
  if (result == 2) {
    onLogin();
  }
}

void MainWindow::applyContactViewSettings()
{
  const bool show = m_client->appSettings().showChatButtons();
  for (ContactRowWidget *row : findChildren<ContactRowWidget *>()) {
    row->setChatButtonVisible(show);
    if (!show) {
      row->setUnreadBlink(false);
    }
  }
  if (show) {
    updateUnreadIndicators();
  }
}

bool MainWindow::shouldNotifyForChatMessage(const QString &peer) const
{
  if (!m_client->appSettings().showChatButtons()) {
    return false;
  }
  if (m_chatDialog && m_chatDialog->isOpenForPeer(peer)) {
    return false;
  }
  return true;
}

void MainWindow::updateUnreadIndicators()
{
  if (!m_client->appSettings().showChatButtons()) {
    for (ContactRowWidget *row : findChildren<ContactRowWidget *>()) {
      row->setUnreadBlink(false);
    }
    return;
  }

  for (auto it = m_contactItems.cbegin(); it != m_contactItems.cend(); ++it) {
    ContactRowWidget *row = rowWidgetForPeer(it.key());
    if (!row) {
      continue;
    }
    row->setUnreadBlink(m_client->chat()->hasUnread(it.key()));
  }
}

void MainWindow::onIncomingChatMessage(const QString &peer, const QString &text, bool incoming,
                                     const QDateTime &timestamp)
{
  Q_UNUSED(text)
  Q_UNUSED(timestamp)

  if (!incoming) {
    return;
  }

  if (m_chatDialog && m_chatDialog->isOpenForPeer(peer)) {
    m_client->chat()->markPeerRead(peer);
    updateUnreadIndicators();
    return;
  }

  updateUnreadIndicators();

  if (shouldNotifyForChatMessage(peer)) {
    m_messageNotify->play();
  }
}

void MainWindow::setupDragDrop()
{
  setAcceptDrops(true);
  if (QWidget *central = centralWidget()) {
    registerDropTarget(central);
  }
  if (m_tabs) {
    registerDropTarget(m_tabs);
    for (int i = 0; i < m_tabs->count(); ++i) {
      registerDropTarget(m_tabs->widget(i));
    }
  }
  if (m_dialPage) {
    registerDropTarget(m_dialPage);
  }
  if (m_contactsList) {
    registerDropTarget(m_contactsList);
  }
  qApp->installEventFilter(this);
}

void MainWindow::registerDropTarget(QWidget *widget)
{
  if (!widget) {
    return;
  }
  widget->setAcceptDrops(true);
  widget->installEventFilter(this);
}

bool MainWindow::isTelUri(const QString &text) const
{
  const QString trimmed = text.trimmed();
  if (trimmed.isEmpty() || trimmed.contains(QLatin1Char('\n'))) {
    return false;
  }
  if (trimmed.startsWith(QStringLiteral("tel:"), Qt::CaseInsensitive)) {
    return true;
  }
  const QUrl url(trimmed);
  return url.scheme() == QStringLiteral("tel");
}

bool MainWindow::shouldInterceptTelPaste(QObject *focusWidget) const
{
  if (QApplication::activeModalWidget()) {
    return false;
  }

  QWidget *widget = qobject_cast<QWidget *>(focusWidget);
  if (!widget) {
    widget = QApplication::focusWidget();
  }

  for (QWidget *current = widget; current; current = current->parentWidget()) {
    if (current == m_chatDialog) {
      return false;
    }
    if (qobject_cast<QDialog *>(current) && current != this) {
      return false;
    }
  }
  return true;
}

void MainWindow::handleIncomingTelUri(const QString &uri)
{
  applyTelUriToDial(uri);
  show();
  raise();
  activateWindow();
}

void MainWindow::applyTelUriToDial(const QString &raw)
{
  QString value = raw.trimmed();
  if (value.startsWith(QStringLiteral("tel:"), Qt::CaseInsensitive)) {
    value = value.mid(4);
  }

  const QUrl asUrl(raw.trimmed());
  if (asUrl.scheme() == QStringLiteral("tel")) {
    value = asUrl.path();
    if (value.isEmpty()) {
      value = asUrl.toString(QUrl::RemoveScheme).trimmed();
    }
  }

  value = QUrl::fromPercentEncoding(value.trimmed().toUtf8());
  const int semicolon = value.indexOf(QLatin1Char(';'));
  if (semicolon >= 0) {
    value = value.left(semicolon);
  }
  value = value.trimmed();
  if (value.isEmpty() || !m_dialInput) {
    return;
  }

  if (m_tabs && m_dialPage) {
    m_tabs->setCurrentWidget(m_dialPage);
  }
  m_dialInput->setText(value);
  m_dialInput->setFocus();
  m_dialInput->selectAll();
}

int MainWindow::addImportedContact(const QString &name, const QString &phone, const QString &ext)
{
  const QString cleanPhone = phone.trimmed();
  const QString cleanExt = ext.trimmed();
  QString cleanName = name.trimmed();
  if (cleanPhone.isEmpty() && cleanExt.isEmpty()) {
    return 0;
  }
  if (cleanName.isEmpty()) {
    cleanName = cleanPhone.isEmpty() ? cleanExt : cleanPhone;
  }

  const QString domain = m_client->credentials().domain;
  const QString handle = !cleanExt.isEmpty() ? cleanExt : cleanPhone;
  itl::CustomContact contact;
  contact.peer = handle.contains(QLatin1Char('@')) ? handle
                                                 : handle + QLatin1Char('@') + domain;
  contact.name = cleanName;
  contact.phone = cleanPhone;
  contact.ext = cleanExt;

  if (useServerContacts()) {
    return m_client->addressBook()->createContact(contact) >= 0 ? 1 : 0;
  }

  m_client->appSettings().addCustomContact(contact);

  ContactEntry entry;
  entry.name = contact.name;
  entry.ext = contact.ext;
  entry.phone = contact.phone;
  entry.login = contact.peer.section(QLatin1Char('@'), 0, 0);
  entry.isCustom = true;
  m_contacts.insert(contact.peer, entry);
  return 1;
}

int MainWindow::importContactsFromText(const QString &text, bool isVcard, bool notify, bool fromDrop)
{
  QByteArray data = text.toUtf8();
  QBuffer buffer(&data);
  if (!buffer.open(QIODevice::ReadOnly | QIODevice::Text)) {
    return 0;
  }

  QTextStream in(&buffer);
  int imported = 0;
  QString vcardName;
  QString vcardTel;

  while (!in.atEnd()) {
    const QString line = in.readLine().trimmed();
    if (line.isEmpty()) {
      continue;
    }

    if (isVcard) {
      if (line.startsWith(QStringLiteral("BEGIN:VCARD"), Qt::CaseInsensitive)) {
        vcardName.clear();
        vcardTel.clear();
      } else if (line.startsWith(QStringLiteral("FN:"), Qt::CaseInsensitive)) {
        vcardName = line.mid(3).trimmed();
      } else if (line.startsWith(QStringLiteral("TEL"), Qt::CaseInsensitive)) {
        vcardTel = line.section(QLatin1Char(':'), 1).trimmed();
      } else if (line.startsWith(QStringLiteral("END:VCARD"), Qt::CaseInsensitive)) {
        imported += addImportedContact(vcardName, vcardTel, {});
      }
      continue;
    }

    const QStringList parts = line.split(QLatin1Char(','));
    imported += addImportedContact(parts.value(0), parts.value(1), parts.value(2));
  }

    if (imported > 0) {
    if (!useServerContacts()) {
      m_client->saveSettings();
      rebuildContactList();
    }
    if (notify) {
      const QString title = fromDrop ? tr("Контакт") : tr("Импорт");
      const QString message = fromDrop ? tr("Добавлено контактов: %1").arg(imported)
                                     : tr("Импортировано контактов: %1").arg(imported);
      QMessageBox::information(this, title, message);
    } else {
      onStatusMessage(tr("Добавлено контактов: %1").arg(imported));
    }
  } else if (notify) {
    QMessageBox::warning(this, tr("Импорт"),
                         tr("В файле не найдено контактов.\n\nФормат CSV: имя,номер,внутренний"));
  }

  return imported;
}

int MainWindow::importContactsFromPath(const QString &path, bool notify, bool fromDrop)
{
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    if (notify) {
      QMessageBox::warning(this, tr("Импорт"), tr("Не удалось открыть файл."));
    }
    return 0;
  }

  const bool isVcard = path.endsWith(QStringLiteral(".vcf"), Qt::CaseInsensitive);
  const int imported = importContactsFromText(QString::fromUtf8(file.readAll()), isVcard, notify, fromDrop);
  file.close();
  return imported;
}

bool MainWindow::canAcceptDrag(const QMimeData *mimeData) const
{
  if (!mimeData || !m_online) {
    return false;
  }

  if (mimeData->hasFormat(QStringLiteral("text/x-vcard"))) {
    return true;
  }

  if (mimeData->hasUrls()) {
    for (const QUrl &url : mimeData->urls()) {
      if (url.scheme() == QStringLiteral("tel")) {
        return true;
      }
      if (url.isLocalFile()) {
        const QString path = url.toLocalFile().toLower();
        if (path.endsWith(QStringLiteral(".vcf")) || path.endsWith(QStringLiteral(".csv"))
            || path.endsWith(QStringLiteral(".txt"))) {
          return true;
        }
      }
    }
  }

  const QString text = mimeData->text().trimmed();
  if (text.isEmpty()) {
    return false;
  }
  if (isTelUri(text)) {
    return true;
  }
  if (text.contains(QStringLiteral("BEGIN:VCARD"), Qt::CaseInsensitive)) {
    return true;
  }
  if (text.contains(QLatin1Char(','))) {
    return true;
  }

  if (mimeData->hasFormat(QStringLiteral("text/uri-list"))) {
    const QString uriList = QString::fromUtf8(mimeData->data(QStringLiteral("text/uri-list")));
    for (const QString &line : uriList.split(QLatin1Char('\n'))) {
      const QString trimmed = line.trimmed();
      if (trimmed.isEmpty() || trimmed.startsWith(QLatin1Char('#'))) {
        continue;
      }
      if (trimmed.startsWith(QStringLiteral("tel:"), Qt::CaseInsensitive)
          || QUrl(trimmed).scheme() == QStringLiteral("tel")) {
        return true;
      }
    }
  }

  return false;
}

bool MainWindow::handleDroppedMimeData(const QMimeData *mimeData, bool notify)
{
  if (!mimeData || !m_online) {
    return false;
  }

  auto firstUri = [&]() -> QString {
    if (mimeData->hasFormat(QStringLiteral("text/uri-list"))) {
      const QString uriList = QString::fromUtf8(mimeData->data(QStringLiteral("text/uri-list")));
      for (const QString &line : uriList.split(QLatin1Char('\n'))) {
        const QString trimmed = line.trimmed();
        if (!trimmed.isEmpty() && !trimmed.startsWith(QLatin1Char('#'))) {
          return trimmed;
        }
      }
    }
    if (mimeData->hasUrls() && !mimeData->urls().isEmpty()) {
      return mimeData->urls().first().toString();
    }
    return {};
  };

  const QString uri = firstUri();
  if (!uri.isEmpty()) {
    const QUrl url(uri);
    if (url.scheme() == QStringLiteral("tel") || uri.startsWith(QStringLiteral("tel:"), Qt::CaseInsensitive)) {
      applyTelUriToDial(uri);
      onStatusMessage(tr("Номер вставлен в «Набрать вручную»"));
      return true;
    }
    if (url.isLocalFile()) {
      const QString path = url.toLocalFile();
      const QString lower = path.toLower();
      if (lower.endsWith(QStringLiteral(".vcf")) || lower.endsWith(QStringLiteral(".csv"))
          || lower.endsWith(QStringLiteral(".txt"))) {
        return importContactsFromPath(path, notify, true) > 0;
      }
    }
  }

  if (mimeData->hasUrls()) {
    for (const QUrl &url : mimeData->urls()) {
      if (!url.isLocalFile()) {
        continue;
      }
      const QString path = url.toLocalFile();
      const QString lower = path.toLower();
      if (lower.endsWith(QStringLiteral(".vcf")) || lower.endsWith(QStringLiteral(".csv"))
          || lower.endsWith(QStringLiteral(".txt"))) {
        return importContactsFromPath(path, notify, true) > 0;
      }
    }
  }

  QString text;
  if (mimeData->hasFormat(QStringLiteral("text/x-vcard"))) {
    text = QString::fromUtf8(mimeData->data(QStringLiteral("text/x-vcard")));
  } else {
    text = mimeData->text();
  }

  const QString trimmed = text.trimmed();
  if (isTelUri(trimmed)) {
    applyTelUriToDial(trimmed);
    onStatusMessage(tr("Номер вставлен в «Набрать вручную»"));
    return true;
  }

  if (trimmed.contains(QStringLiteral("BEGIN:VCARD"), Qt::CaseInsensitive)) {
    return importContactsFromText(trimmed, true, notify, true) > 0;
  }

  if (trimmed.contains(QLatin1Char(','))) {
    return importContactsFromText(trimmed, false, notify, true) > 0;
  }

  return false;
}

void MainWindow::dragEnterEvent(QDragEnterEvent *event)
{
  if (canAcceptDrag(event->mimeData())) {
    event->acceptProposedAction();
    return;
  }
  QMainWindow::dragEnterEvent(event);
}

void MainWindow::dropEvent(QDropEvent *event)
{
  if (handleDroppedMimeData(event->mimeData(), true)) {
    event->acceptProposedAction();
    return;
  }
  QMainWindow::dropEvent(event);
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
  if (event->type() == QEvent::DragEnter) {
    auto *dragEvent = static_cast<QDragEnterEvent *>(event);
    if (canAcceptDrag(dragEvent->mimeData())) {
      dragEvent->acceptProposedAction();
      return true;
    }
  }

  if (event->type() == QEvent::Drop) {
    auto *dropEvent = static_cast<QDropEvent *>(event);
    if (handleDroppedMimeData(dropEvent->mimeData(), true)) {
      dropEvent->acceptProposedAction();
      return true;
    }
  }

  if (event->type() == QEvent::KeyPress && shouldInterceptTelPaste(QApplication::focusWidget())) {
    auto *keyEvent = static_cast<QKeyEvent *>(event);
    if (keyEvent->matches(QKeySequence::Paste)) {
      const QString clipboardText = QGuiApplication::clipboard()->text().trimmed();
      if (isTelUri(clipboardText)) {
        applyTelUriToDial(clipboardText);
        return true;
      }
    }
  }

  return QObject::eventFilter(watched, event);
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
  if (useServerContacts()) {
    if (m_client->addressBook()->createContact(contact) < 0) {
      QMessageBox::warning(this, tr("Контакт"), tr("Не удалось отправить контакт на сервер."));
      return;
    }
    onStatusMessage(tr("Контакт «%1» отправлен на сервер").arg(contact.name));
    return;
  }

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

void MainWindow::onImportContacts()
{
  if (!m_online) {
    return;
  }

  const QString path = QFileDialog::getOpenFileName(
      this, tr("Импорт контактов"), QString(),
      tr("Контакты (*.csv *.vcf *.txt);;Все файлы (*)"));
  if (path.isEmpty()) {
    return;
  }

  importContactsFromPath(path, true);
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
  m_chatDialog->openForPeer(peer, displayNameForPeer(peer), m_selfName);
  updateUnreadIndicators();
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
    entry.personalPhone = acc.value(QStringLiteral("sim")).toString();
    if (entry.phone.isEmpty()) {
      entry.phone = entry.personalPhone;
      entry.personalPhone.clear();
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
  refreshServerHistory();
  prefetchCompanyHistory();
  prefetchInternalHistory();
}

void MainWindow::onAddressBookChanged()
{
  mergeCustomContacts();
  rebuildContactList();
}

bool MainWindow::useServerContacts() const
{
  return m_online && !m_demoMode;
}

void MainWindow::mergeCustomContacts()
{
  for (auto it = m_contacts.begin(); it != m_contacts.end();) {
    if (it->isCustom && !it->isSelf) {
      it = m_contacts.erase(it);
    } else {
      ++it;
    }
  }

  const QList<itl::CustomContact> personal = useServerContacts()
      ? m_client->addressBook()->contacts()
      : m_client->appSettings().customContacts();

  for (const itl::CustomContact &contact : personal) {
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
    // Conversation duration / history "answered" start when remote audio arrives.
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
