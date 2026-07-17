#include "SettingsDialog.h"

#include "ProfileAvatarWidget.h"
#include "WallpaperCropDialog.h"
#include "audio/AudioDeviceUtils.h"
#include "audio/IncomingRingPlayer.h"
#include "audio/RingbackPlayer.h"
#include "network/NetworkUtils.h"
#include "calls/CallManager.h"
#include "protocol/CommunicatorClient.h"
#include "settings/AppSettings.h"
#include "ui/StyleHelper.h"

#include <QAudioDevice>
#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QFile>
#include <QFileDialog>
#include <QFrame>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPixmap>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QSlider>
#include <QTabWidget>
#include <QTimer>
#include <QVBoxLayout>

#include "chat/ChatManager.h"
#include "ui/TransferDialog.h"

#include <QPixmap>

SettingsDialog::SettingsDialog(itl::CommunicatorClient *client, itl::CallManager *calls,
                               const QString &displayName, const QHash<QString, QString> &sharePeers,
                               const QString &selfPeer, QWidget *parent)
    : QDialog(parent)
    , m_client(client)
    , m_calls(calls)
    , m_settings(&client->appSettings())
    , m_ringbackPreview(new itl::RingbackPlayer(this))
    , m_incomingPreview(new itl::IncomingRingPlayer(this))
    , m_previewTimer(new QTimer(this))
    , m_sharePeers(sharePeers)
    , m_selfPeer(selfPeer)
{
  setWindowTitle(tr("Настройки"));
  setObjectName(QStringLiteral("settingsDialog"));
  setSizeGripEnabled(false);
  // Fixed size so live wallpaper opacity updates (parent MainWindow restyle) cannot grow the dialog.
  setFixedSize(460, 680);
  m_selfName = displayName;

  auto *mainLayout = new QVBoxLayout(this);
  mainLayout->setContentsMargins(16, 16, 16, 16);

  auto *tabs = new QTabWidget;

  auto *soundTab = new QWidget;
  auto *soundLayout = new QVBoxLayout(soundTab);
  soundLayout->setContentsMargins(8, 8, 8, 8);
  soundLayout->setSpacing(8);

  auto *form = new QFormLayout;
  form->setSpacing(8);

  m_inputDevice = new QComboBox;
  m_outputDevice = new QComboBox;
  m_ringbackTone = new QComboBox;
  m_incomingTone = new QComboBox;

  m_ringbackTone->addItem(itl::AppSettings::ringtoneLabel(itl::AppSettings::RingtoneKind::BuiltinRussian),
                          static_cast<int>(itl::AppSettings::RingtoneKind::BuiltinRussian));
  m_ringbackTone->addItem(itl::AppSettings::ringtoneLabel(itl::AppSettings::RingtoneKind::BuiltinClassic),
                          static_cast<int>(itl::AppSettings::RingtoneKind::BuiltinClassic));
  m_ringbackTone->addItem(itl::AppSettings::ringtoneLabel(itl::AppSettings::RingtoneKind::BuiltinShort),
                          static_cast<int>(itl::AppSettings::RingtoneKind::BuiltinShort));
  m_ringbackTone->addItem(itl::AppSettings::ringtoneLabel(itl::AppSettings::RingtoneKind::CustomFile),
                          static_cast<int>(itl::AppSettings::RingtoneKind::CustomFile));

  m_incomingTone->addItem(itl::AppSettings::ringtoneLabel(itl::AppSettings::RingtoneKind::BuiltinClassic),
                          static_cast<int>(itl::AppSettings::RingtoneKind::BuiltinClassic));
  m_incomingTone->addItem(itl::AppSettings::ringtoneLabel(itl::AppSettings::RingtoneKind::BuiltinRussian),
                          static_cast<int>(itl::AppSettings::RingtoneKind::BuiltinRussian));
  m_incomingTone->addItem(itl::AppSettings::ringtoneLabel(itl::AppSettings::RingtoneKind::BuiltinShort),
                          static_cast<int>(itl::AppSettings::RingtoneKind::BuiltinShort));
  m_incomingTone->addItem(itl::AppSettings::ringtoneLabel(itl::AppSettings::RingtoneKind::CustomFile),
                          static_cast<int>(itl::AppSettings::RingtoneKind::CustomFile));

  form->addRow(tr("Микрофон:"), m_inputDevice);
  form->addRow(tr("Динамики:"), m_outputDevice);

  auto *ringbackToneRow = new QHBoxLayout;
  m_ringbackPreviewBtn = new QPushButton(QStringLiteral("▶"));
  m_ringbackPreviewBtn->setToolTip(tr("Прослушать"));
  m_ringbackPreviewBtn->setFixedWidth(m_ringbackPreviewBtn->sizeHint().height());
  ringbackToneRow->addWidget(m_ringbackTone, 1);
  ringbackToneRow->addWidget(m_ringbackPreviewBtn);
  form->addRow(tr("Звук дозвона ДО клиента:"), ringbackToneRow);

  auto *ringbackRow = new QHBoxLayout;
  m_ringbackPath = new QLineEdit;
  m_ringbackPath->setPlaceholderText(tr("Путь к файлу (wav, mp3, ogg)"));
  m_ringbackBrowse = new QPushButton(tr("Обзор..."));
  ringbackRow->addWidget(m_ringbackPath, 1);
  ringbackRow->addWidget(m_ringbackBrowse);
  form->addRow(tr("Файл гудка:"), ringbackRow);

  auto *incomingToneRow = new QHBoxLayout;
  m_incomingPreviewBtn = new QPushButton(QStringLiteral("▶"));
  m_incomingPreviewBtn->setToolTip(tr("Прослушать"));
  m_incomingPreviewBtn->setFixedWidth(m_incomingPreviewBtn->sizeHint().height());
  incomingToneRow->addWidget(m_incomingTone, 1);
  incomingToneRow->addWidget(m_incomingPreviewBtn);
  form->addRow(tr("Звук дозвона ОТ клиента:"), incomingToneRow);
  auto *incomingRow = new QHBoxLayout;
  m_incomingPath = new QLineEdit;
  m_incomingPath->setPlaceholderText(tr("Путь к файлу (wav, mp3, ogg)"));
  m_incomingBrowse = new QPushButton(tr("Обзор..."));
  incomingRow->addWidget(m_incomingPath, 1);
  incomingRow->addWidget(m_incomingBrowse);
  form->addRow(tr("Файл звонка:"), incomingRow);

  soundLayout->addLayout(form);
  soundLayout->addStretch();
  tabs->addTab(soundTab, tr("Звук"));

  auto *accountTab = new QWidget;
  auto *accountLayout = new QVBoxLayout(accountTab);
  accountLayout->setContentsMargins(8, 8, 8, 8);

  auto *avatarRow = new QHBoxLayout;
  m_accountAvatar = new ProfileAvatarWidget(&m_client->appSettings());
  m_accountAvatar->setMenuEnabled(false);
  avatarRow->addWidget(m_accountAvatar);

  auto *avatarBtns = new QVBoxLayout;
  avatarBtns->setSpacing(4);
  auto *photoBtn = new QPushButton(tr("Загрузить фото"));
  photoBtn->setObjectName(QStringLiteral("avatarMenuBtn"));
  auto *colorBtn = new QPushButton(tr("Цвет фона"));
  colorBtn->setObjectName(QStringLiteral("avatarMenuBtn"));
  auto *removePhotoBtn = new QPushButton(tr("Убрать фото"));
  removePhotoBtn->setObjectName(QStringLiteral("avatarMenuBtn"));
  m_shareProfileBtn = new QPushButton;
  m_shareProfileBtn->setObjectName(QStringLiteral("avatarMenuBtn"));
  avatarBtns->addWidget(photoBtn);
  avatarBtns->addWidget(colorBtn);
  avatarBtns->addWidget(removePhotoBtn);
  avatarBtns->addWidget(m_shareProfileBtn);
  const auto syncPhotoButtons = [this, colorBtn, removePhotoBtn]() {
    const bool hasPhoto = m_settings && !m_settings->profileAvatarPath().isEmpty();
    colorBtn->setVisible(!hasPhoto);
    removePhotoBtn->setVisible(hasPhoto);
    updateShareProfileButton();
  };
  syncPhotoButtons();
  avatarBtns->addStretch();
  avatarRow->addLayout(avatarBtns);
  accountLayout->addLayout(avatarRow);

  connect(photoBtn, &QPushButton::clicked, this, [this, syncPhotoButtons]() {
    const QString path = QFileDialog::getOpenFileName(this, tr("Выберите фото"), {},
                                                       tr("Изображения (*.png *.jpg *.jpeg *.bmp *.webp);;Все файлы (*)"));
    if (!path.isEmpty()) {
      m_settings->setProfileAvatarPath(path);
      m_accountAvatar->refreshFromSettings();
      syncPhotoButtons();
    }
  });
  connect(colorBtn, &QPushButton::clicked, this, [this, syncPhotoButtons]() {
    const QColor color = QColorDialog::getColor(m_settings->profileAvatarColor(), this, tr("Выберите цвет фона"));
    if (color.isValid()) {
      m_settings->setProfileAvatarColor(color.name());
      m_accountAvatar->refreshFromSettings();
      syncPhotoButtons();
    }
  });
  connect(removePhotoBtn, &QPushButton::clicked, this, [this, syncPhotoButtons]() {
    m_settings->setProfileAvatarPath({});
    m_accountAvatar->refreshFromSettings();
    syncPhotoButtons();
  });
  connect(m_shareProfileBtn, &QPushButton::clicked, this, &SettingsDialog::onShareAvatar);

  auto *accountForm = new QFormLayout;
  accountForm->setSpacing(8);

  m_displayNameEdit = new QLineEdit;
  m_displayNameEdit->setPlaceholderText(tr("Имя для отображения"));
  m_displayNameEdit->setText(m_selfName);

  auto *nameRow = new QHBoxLayout;
  nameRow->addWidget(m_displayNameEdit, 1);
  auto *resetNameBtn = new QPushButton(tr("Сбросить"));
  resetNameBtn->setToolTip(tr("Восстановить имя по умолчанию"));
  connect(resetNameBtn, &QPushButton::clicked, this, [this]() {
    m_displayNameEdit->setText(m_selfName);
  });
  nameRow->addWidget(resetNameBtn);
  accountForm->addRow(tr("Имя:"), nameRow);

  m_networkInterface = new QComboBox;
  m_networkInterface->setToolTip(tr("Интерфейс для WebRTC-звонков (SDP и ICE). Не влияет на вход в систему."));
  accountForm->addRow(tr("Сеть для звонков:"), m_networkInterface);

  accountLayout->addLayout(accountForm);

  accountLayout->addSpacing(12);

  auto *wallpaperRow = new QHBoxLayout;
  m_wallpaperPreview = new QLabel;
  m_wallpaperPreview->setFixedSize(78, 124);
  m_wallpaperPreview->setAlignment(Qt::AlignCenter);
  m_wallpaperPreview->setFrameShape(QFrame::StyledPanel);
  m_wallpaperPreview->setScaledContents(true);
  wallpaperRow->addWidget(m_wallpaperPreview);

  auto *wallpaperBtns = new QVBoxLayout;
  wallpaperBtns->setSpacing(4);
  auto *wallpaperBtn = new QPushButton(tr("Выбрать обои..."));
  wallpaperBtn->setObjectName(QStringLiteral("avatarMenuBtn"));
  m_removeWallpaperBtn = new QPushButton(tr("Убрать обои"));
  m_removeWallpaperBtn->setObjectName(QStringLiteral("avatarMenuBtn"));
  m_shareThemeBtn = new QPushButton(tr("Поделиться темой"));
  m_shareThemeBtn->setObjectName(QStringLiteral("avatarMenuBtn"));
  m_shareThemeBtn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  m_shareThemeBtn->setToolTip(
      tr("Отправить обои и параметры затемнения выбранному пользователю OpenSource Communicator"));

  m_wallpaperOpacityRow = new QWidget;
  auto *opacityLayout = new QVBoxLayout(m_wallpaperOpacityRow);
  opacityLayout->setContentsMargins(0, 0, 0, 0);
  opacityLayout->setSpacing(2);
  opacityLayout->addWidget(new QLabel(tr("Затемнение интерфейса:")));
  auto *opacitySliderRow = new QHBoxLayout;
  opacitySliderRow->setContentsMargins(0, 0, 0, 0);
  m_wallpaperOpacitySlider = new QSlider(Qt::Horizontal);
  m_wallpaperOpacitySlider->setRange(20, 100);
  m_wallpaperOpacitySlider->setValue(m_settings->appWallpaperOpacity());
  m_wallpaperOpacitySlider->setToolTip(
      tr("100%% — обычный непрозрачный интерфейс. Меньше — панели становятся прозрачнее и показывают обои."));
  opacitySliderRow->addWidget(m_wallpaperOpacitySlider, 1);
  m_wallpaperOpacityValue = new QLabel(QStringLiteral("%1%").arg(m_wallpaperOpacitySlider->value()));
  m_wallpaperOpacityValue->setMinimumWidth(40);
  m_wallpaperOpacityValue->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
  opacitySliderRow->addWidget(m_wallpaperOpacityValue);
  opacityLayout->addLayout(opacitySliderRow);
  m_wallpaperOpacityRow->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

  m_wallpaperListOpacityRow = new QWidget;
  auto *listOpacityLayout = new QVBoxLayout(m_wallpaperListOpacityRow);
  listOpacityLayout->setContentsMargins(0, 0, 0, 0);
  listOpacityLayout->setSpacing(2);
  listOpacityLayout->addWidget(new QLabel(tr("Затемнение списков:")));
  auto *listOpacitySliderRow = new QHBoxLayout;
  listOpacitySliderRow->setContentsMargins(0, 0, 0, 0);
  m_wallpaperListOpacitySlider = new QSlider(Qt::Horizontal);
  m_wallpaperListOpacitySlider->setRange(20, 100);
  m_wallpaperListOpacitySlider->setValue(m_settings->appWallpaperListOpacity());
  m_wallpaperListOpacitySlider->setToolTip(
      tr("Непрозрачность списков контактов и истории поверх обоев."));
  listOpacitySliderRow->addWidget(m_wallpaperListOpacitySlider, 1);
  m_wallpaperListOpacityValue =
      new QLabel(QStringLiteral("%1%").arg(m_wallpaperListOpacitySlider->value()));
  m_wallpaperListOpacityValue->setMinimumWidth(40);
  m_wallpaperListOpacityValue->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
  listOpacitySliderRow->addWidget(m_wallpaperListOpacityValue);
  listOpacityLayout->addLayout(listOpacitySliderRow);
  m_wallpaperListOpacityRow->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

  wallpaperBtns->addWidget(wallpaperBtn);
  wallpaperBtns->addWidget(m_wallpaperOpacityRow);
  wallpaperBtns->addWidget(m_wallpaperListOpacityRow);
  wallpaperBtns->addWidget(m_removeWallpaperBtn);
  wallpaperRow->addLayout(wallpaperBtns, 1);
  accountLayout->addLayout(wallpaperRow);

  m_shareThemeBtn->setMinimumHeight(32);
  accountLayout->addWidget(m_shareThemeBtn);

  connect(m_wallpaperOpacitySlider, &QSlider::valueChanged, this, [this](int value) {
    if (m_wallpaperOpacityValue) {
      m_wallpaperOpacityValue->setText(QStringLiteral("%1%").arg(value));
    }
    m_settings->setAppWallpaperOpacity(value);
    m_client->saveSettings();
  });
  connect(m_wallpaperListOpacitySlider, &QSlider::valueChanged, this, [this](int value) {
    if (m_wallpaperListOpacityValue) {
      m_wallpaperListOpacityValue->setText(QStringLiteral("%1%").arg(value));
    }
    m_settings->setAppWallpaperListOpacity(value);
    m_client->saveSettings();
  });

  connect(wallpaperBtn, &QPushButton::clicked, this, [this]() {
    const QString path = QFileDialog::getOpenFileName(
        this, tr("Выберите изображение"), {},
        tr("Изображения (*.png *.jpg *.jpeg *.bmp *.webp);;Все файлы (*)"));
    if (path.isEmpty()) {
      return;
    }

    const QPixmap source(path);
    if (source.isNull()) {
      QMessageBox::warning(this, tr("Обои"), tr("Не удалось открыть изображение."));
      return;
    }

    const QPixmap cropped =
        WallpaperCropDialog::cropImage(source, itl::AppSettings::appWallpaperTargetSize(), this);
    if (cropped.isNull()) {
      return;
    }

    const QString saved = itl::AppSettings::saveAppWallpaperImage(cropped);
    if (saved.isEmpty()) {
      QMessageBox::warning(this, tr("Обои"), tr("Не удалось сохранить обои."));
      return;
    }

    m_settings->setAppWallpaperPath(saved);
    m_client->saveSettings();
    updateWallpaperPreview();
  });
  connect(m_removeWallpaperBtn, &QPushButton::clicked, this, [this]() {
    m_settings->clearAppWallpaper();
    m_client->saveSettings();
    updateWallpaperPreview();
  });
  connect(m_shareThemeBtn, &QPushButton::clicked, this, &SettingsDialog::onShareTheme);

  updateWallpaperPreview();

  accountLayout->addStretch();

  auto *logoutBtn = new QPushButton(tr("Выйти из аккаунта"));
  logoutBtn->setObjectName(QStringLiteral("logoutBtn"));
  connect(logoutBtn, &QPushButton::clicked, this, [this]() {
    accept();
    done(3);
  });
  accountLayout->addWidget(logoutBtn);

  tabs->addTab(accountTab, tr("Аккаунт"));

  auto *recordingTab = new QWidget;
  auto *recordingLayout = new QVBoxLayout(recordingTab);
  recordingLayout->setContentsMargins(8, 8, 8, 8);

  auto *recordingForm = new QFormLayout;
  recordingForm->setSpacing(8);

  m_recordingEnabledCheck = new QCheckBox(tr("Записывать разговоры"));
  m_recordingEnabledCheck->setChecked(m_settings->recordingEnabled());
  recordingForm->addRow(m_recordingEnabledCheck);

  m_recordingDualTrackCheck = new QCheckBox(tr("Запись в две дорожки"));
  m_recordingDualTrackCheck->setChecked(m_settings->recordingDualTrack());
  recordingForm->addRow(m_recordingDualTrackCheck);

  auto *combinedContainer = new QWidget;
  auto *combinedLayout = new QHBoxLayout(combinedContainer);
  combinedLayout->setContentsMargins(0, 0, 0, 0);
  m_recordingCombinedCheck = new QCheckBox(tr("Записывать объединённую дорожку"));
  m_recordingCombinedCheck->setChecked(m_settings->recordingCombinedTrack());
  combinedLayout->addWidget(m_recordingCombinedCheck);
  combinedLayout->addStretch();
  recordingForm->addRow(combinedContainer);

  auto *dirRow = new QHBoxLayout;
  m_recordingDirEdit = new QLineEdit;
  m_recordingDirEdit->setText(m_settings->recordingDirectory());
  m_recordingDirEdit->setPlaceholderText(itl::AppSettings::defaultRecordingDirectory());
  dirRow->addWidget(m_recordingDirEdit, 1);
  auto *dirBtn = new QPushButton(tr("Обзор..."));
  connect(dirBtn, &QPushButton::clicked, this, [this]() {
    const QString dir = QFileDialog::getExistingDirectory(this, tr("Выберите папку"), m_recordingDirEdit->text());
    if (!dir.isEmpty()) {
      m_recordingDirEdit->setText(dir);
    }
  });
  dirRow->addWidget(dirBtn);
  recordingForm->addRow(tr("Директория:"), dirRow);

  m_recordingTemplateEdit = new QLineEdit;
  m_recordingTemplateEdit->setText(m_settings->recordingFilenameTemplate());
  m_recordingTemplateEdit->setToolTip(itl::AppSettings::recordingFilenameSyntaxHelp());
  recordingForm->addRow(tr("Шаблон имени файла:"), m_recordingTemplateEdit);

  recordingLayout->addLayout(recordingForm);

  m_recordingPreviewLabel = new QLabel;
  m_recordingPreviewLabel->setObjectName(QStringLiteral("recordingPreview"));
  m_recordingPreviewLabel->setWordWrap(true);
  recordingLayout->addWidget(m_recordingPreviewLabel);

  auto updatePreview = [this]() {
    const QString name = m_recordingTemplateEdit->text();
    const QString base = itl::AppSettings::expandRecordingFilenameTemplate(name, m_selfName);
    if (m_recordingDualTrackCheck->isChecked()) {
      QString text = tr("Дорожки:\n  %1_manager.mp3\n  %1_caller.mp3").arg(base);
      if (m_recordingCombinedCheck->isChecked()) {
        text += tr("\nОбъединённая:\n  %1.mp3").arg(base);
      }
      m_recordingPreviewLabel->setText(text);
    } else {
      m_recordingPreviewLabel->setText(tr("Пример: %1.mp3").arg(base));
    }
  };
  connect(m_recordingTemplateEdit, &QLineEdit::textChanged, this, updatePreview);
  connect(m_recordingDualTrackCheck, &QCheckBox::toggled, this, [this, updatePreview](bool checked) {
    m_recordingCombinedCheck->setEnabled(checked);
    if (!checked) {
      m_recordingCombinedCheck->setChecked(false);
    }
    updatePreview();
  });
  m_recordingCombinedCheck->setEnabled(m_settings->recordingDualTrack());
  connect(m_recordingCombinedCheck, &QCheckBox::toggled, this, updatePreview);
  updatePreview();

  recordingLayout->addStretch();
  tabs->addTab(recordingTab, tr("Запись"));

  mainLayout->addWidget(tabs, 1);

  QPushButton *cancel = nullptr;
  QPushButton *ok = nullptr;
  mainLayout->addLayout(itl::createDialogButtonRow(&cancel, &ok, tr("Сохранить")));

  connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
  connect(ok, &QPushButton::clicked, this, &SettingsDialog::onAccept);
  connect(m_ringbackBrowse, &QPushButton::clicked, this, &SettingsDialog::onBrowseRingback);
  connect(m_incomingBrowse, &QPushButton::clicked, this, &SettingsDialog::onBrowseIncoming);
  connect(m_ringbackPreviewBtn, &QPushButton::clicked, this, &SettingsDialog::onPreviewRingback);
  connect(m_incomingPreviewBtn, &QPushButton::clicked, this, &SettingsDialog::onPreviewIncoming);
  connect(m_previewTimer, &QTimer::timeout, this, &SettingsDialog::stopPreview);
  m_previewTimer->setSingleShot(true);
  connect(m_ringbackTone, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int index) {
    const bool custom = ringtoneKindForIndex(index) == itl::AppSettings::RingtoneKind::CustomFile;
    m_ringbackPath->setEnabled(custom);
    m_ringbackBrowse->setEnabled(custom);
    updatePreviewButtons();
  });
  connect(m_incomingTone, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int index) {
    const bool custom = incomingRingtoneKindForIndex(index) == itl::AppSettings::RingtoneKind::CustomFile;
    m_incomingPath->setEnabled(custom);
    m_incomingBrowse->setEnabled(custom);
    updatePreviewButtons();
  });
  connect(m_ringbackPath, &QLineEdit::textChanged, this, &SettingsDialog::updatePreviewButtons);
  connect(m_incomingPath, &QLineEdit::textChanged, this, &SettingsDialog::updatePreviewButtons);

  loadDevices();
  loadNetworkInterfaces();
  loadFromSettings();

  itl::applyFormDialogStyle(this);
}

SettingsDialog::~SettingsDialog()
{
  stopPreview();
}

QString SettingsDialog::displayName() const
{
  return m_displayNameEdit ? m_displayNameEdit->text().trimmed() : QString();
}

void SettingsDialog::reject()
{
  stopPreview();
  QDialog::reject();
}

void SettingsDialog::loadDevices()
{
  m_inputDevice->clear();
  m_outputDevice->clear();

  for (const QAudioDevice &device : itl::AudioDeviceUtils::inputDevices()) {
    m_inputDevice->addItem(device.description(), itl::AudioDeviceUtils::deviceId(device));
  }
  for (const QAudioDevice &device : itl::AudioDeviceUtils::audioOutputDevices()) {
    m_outputDevice->addItem(device.description(), itl::AudioDeviceUtils::deviceId(device));
  }
}

void SettingsDialog::loadNetworkInterfaces()
{
  m_networkInterface->clear();
  m_networkInterface->addItem(tr("Автоматически (первая доступная)"), QString());

  for (const itl::NetworkUtils::NetworkInterfaceEntry &entry : itl::NetworkUtils::ipv4Interfaces()) {
    m_networkInterface->addItem(itl::NetworkUtils::interfaceLabel(entry), entry.systemName);
  }
}

void SettingsDialog::loadFromSettings()
{
  const int inputIndex = m_inputDevice->findData(m_settings->inputDeviceId());
  m_inputDevice->setCurrentIndex(inputIndex >= 0 ? inputIndex : 0);

  const int outputIndex = m_outputDevice->findData(m_settings->outputDeviceId());
  m_outputDevice->setCurrentIndex(outputIndex >= 0 ? outputIndex : 0);

  const int ringbackIndex = m_ringbackTone->findData(static_cast<int>(m_settings->ringbackKind()));
  m_ringbackTone->setCurrentIndex(ringbackIndex >= 0 ? ringbackIndex : 0);

  const int incomingIndex = m_incomingTone->findData(static_cast<int>(m_settings->incomingRingKind()));
  m_incomingTone->setCurrentIndex(incomingIndex >= 0 ? incomingIndex : 0);

  m_ringbackPath->setText(m_settings->ringbackCustomPath());
  m_incomingPath->setText(m_settings->incomingRingCustomPath());

  const bool ringbackCustom = m_settings->ringbackKind() == itl::AppSettings::RingtoneKind::CustomFile;
  m_ringbackPath->setEnabled(ringbackCustom);
  m_ringbackBrowse->setEnabled(ringbackCustom);

  const bool incomingCustom = m_settings->incomingRingKind() == itl::AppSettings::RingtoneKind::CustomFile;
  m_incomingPath->setEnabled(incomingCustom);
  m_incomingBrowse->setEnabled(incomingCustom);
  updatePreviewButtons();

  const int networkIndex = m_networkInterface->findData(m_settings->networkInterfaceName());
  m_networkInterface->setCurrentIndex(networkIndex >= 0 ? networkIndex : 0);
}

itl::AppSettings::RingtoneKind SettingsDialog::ringtoneKindForIndex(int index) const
{
  return static_cast<itl::AppSettings::RingtoneKind>(m_ringbackTone->itemData(index).toInt());
}

itl::AppSettings::RingtoneKind SettingsDialog::incomingRingtoneKindForIndex(int index) const
{
  return static_cast<itl::AppSettings::RingtoneKind>(m_incomingTone->itemData(index).toInt());
}

void SettingsDialog::syncPreviewSettings(itl::AppSettings *target) const
{
  if (!target) {
    return;
  }
  target->setOutputDeviceId(m_outputDevice->currentData().toString());
  target->setRingbackKind(static_cast<itl::AppSettings::RingtoneKind>(m_ringbackTone->currentData().toInt()));
  target->setRingbackCustomPath(m_ringbackPath->text().trimmed());
  target->setIncomingRingKind(static_cast<itl::AppSettings::RingtoneKind>(m_incomingTone->currentData().toInt()));
  target->setIncomingRingCustomPath(m_incomingPath->text().trimmed());
}

void SettingsDialog::updatePreviewButtons()
{
  const bool ringbackCustom =
      ringtoneKindForIndex(m_ringbackTone->currentIndex()) == itl::AppSettings::RingtoneKind::CustomFile;
  const bool incomingCustom =
      incomingRingtoneKindForIndex(m_incomingTone->currentIndex()) == itl::AppSettings::RingtoneKind::CustomFile;
  m_ringbackPreviewBtn->setEnabled(!ringbackCustom || !m_ringbackPath->text().trimmed().isEmpty());
  m_incomingPreviewBtn->setEnabled(!incomingCustom || !m_incomingPath->text().trimmed().isEmpty());
}

void SettingsDialog::stopPreview()
{
  m_previewTimer->stop();
  m_ringbackPreview->stop();
  m_incomingPreview->stop();
}

void SettingsDialog::onPreviewRingback()
{
  if (m_ringbackPreview->isPlaying()) {
    stopPreview();
    return;
  }

  stopPreview();
  syncPreviewSettings(&m_previewSettings);
  m_ringbackPreview->applySettings(&m_previewSettings);
  m_ringbackPreview->start();
  m_previewTimer->start(5000);
}

void SettingsDialog::onPreviewIncoming()
{
  if (m_incomingPreview->isPlaying()) {
    stopPreview();
    return;
  }

  stopPreview();
  syncPreviewSettings(&m_previewSettings);
  m_incomingPreview->applySettings(&m_previewSettings);
  m_incomingPreview->start();
  m_previewTimer->start(5000);
}

void SettingsDialog::updateWallpaperPreview()
{
  if (!m_wallpaperPreview || !m_removeWallpaperBtn) {
    return;
  }

  const auto setOpacityRowsEnabled = [this](bool enabled) {
    if (m_wallpaperOpacityRow) {
      m_wallpaperOpacityRow->setEnabled(enabled);
    }
    if (m_wallpaperListOpacityRow) {
      m_wallpaperListOpacityRow->setEnabled(enabled);
    }
    if (m_shareThemeBtn) {
      m_shareThemeBtn->setEnabled(enabled);
    }
  };

  const QString path = m_settings->appWallpaperPath();
  const bool hasWallpaper = !path.isEmpty() && QFile::exists(path);
  if (!hasWallpaper) {
    m_wallpaperPreview->clear();
    m_wallpaperPreview->setText(tr("Нет"));
    m_removeWallpaperBtn->setEnabled(false);
    setOpacityRowsEnabled(false);
    return;
  }

  const QPixmap pixmap(path);
  if (pixmap.isNull()) {
    m_wallpaperPreview->clear();
    m_wallpaperPreview->setText(tr("Нет"));
    m_removeWallpaperBtn->setEnabled(false);
    setOpacityRowsEnabled(false);
    return;
  }

  m_wallpaperPreview->setPixmap(
      pixmap.scaled(m_wallpaperPreview->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
  m_wallpaperPreview->setText({});
  m_removeWallpaperBtn->setEnabled(true);
  setOpacityRowsEnabled(true);
  if (m_wallpaperOpacitySlider) {
    const QSignalBlocker blocker(m_wallpaperOpacitySlider);
    m_wallpaperOpacitySlider->setValue(m_settings->appWallpaperOpacity());
  }
  if (m_wallpaperOpacityValue) {
    m_wallpaperOpacityValue->setText(QStringLiteral("%1%").arg(m_settings->appWallpaperOpacity()));
  }
  if (m_wallpaperListOpacitySlider) {
    const QSignalBlocker blocker(m_wallpaperListOpacitySlider);
    m_wallpaperListOpacitySlider->setValue(m_settings->appWallpaperListOpacity());
  }
  if (m_wallpaperListOpacityValue) {
    m_wallpaperListOpacityValue->setText(
        QStringLiteral("%1%").arg(m_settings->appWallpaperListOpacity()));
  }
}

void SettingsDialog::onBrowseRingback()
{
  const QString path = QFileDialog::getOpenFileName(this, tr("Выберите файл гудка"), {},
                                                     tr("Аудио (*.wav *.mp3 *.ogg);;Все файлы (*)"));
  if (!path.isEmpty()) {
    m_ringbackPath->setText(path);
  }
}

void SettingsDialog::onBrowseIncoming()
{
  const QString path = QFileDialog::getOpenFileName(this, tr("Выберите файл звонка"), {},
                                                     tr("Аудио (*.wav *.mp3 *.ogg);;Все файлы (*)"));
  if (!path.isEmpty()) {
    m_incomingPath->setText(path);
  }
}

void SettingsDialog::updateShareProfileButton()
{
  if (!m_shareProfileBtn || !m_settings) {
    return;
  }
  const bool hasPhoto = !m_settings->profileAvatarPath().isEmpty();
  if (hasPhoto) {
    m_shareProfileBtn->setText(tr("Поделиться аватаркой"));
    m_shareProfileBtn->setToolTip(
        tr("Отправить аватарку (140×140) выбранному пользователю OpenSource Communicator"));
  } else {
    m_shareProfileBtn->setText(tr("Поделиться цветом"));
    m_shareProfileBtn->setToolTip(
        tr("Отправить цвет аватарки выбранному пользователю OpenSource Communicator"));
  }
}

void SettingsDialog::onShareAvatar()
{
  if (!m_client || !m_client->chat() || !m_settings) {
    return;
  }
  const bool hasPhoto = !m_settings->profileAvatarPath().isEmpty();
  const QString title = hasPhoto ? tr("Поделиться аватаркой") : tr("Поделиться цветом");
  if (m_sharePeers.isEmpty()) {
    QMessageBox::information(this, title,
                             tr("Пока нет других пользователей OpenSource Communicator.\n"
                                "Они появятся после обмена Openping! при входе в сеть."));
    return;
  }

  if (hasPhoto) {
    QPixmap photo(m_settings->profileAvatarPath());
    if (photo.isNull()) {
      QMessageBox::warning(this, title, tr("Не удалось открыть фото аватарки."));
      return;
    }
    TransferDialog dlg(m_sharePeers, m_selfPeer, {}, this, title,
                       tr("Выберите контакт, которому отправить аватарку:"), tr("Отправить"));
    if (dlg.exec() != QDialog::Accepted) {
      return;
    }
    const QString peer = dlg.selectedPeer();
    if (peer.isEmpty()) {
      return;
    }
    if (!m_client->chat()->sendAvatarShare(peer, photo)) {
      QMessageBox::warning(this, title, tr("Не удалось отправить аватарку."));
      return;
    }
    QMessageBox::information(this, title, tr("Аватарка отправлена: %1").arg(dlg.selectedDisplayName()));
    return;
  }

  const QString color = m_settings->profileAvatarColor();
  if (color.isEmpty()) {
    QMessageBox::information(this, title, tr("Цвет аватарки не задан."));
    return;
  }
  m_client->chat()->setSelfShareProfile(color, {});
  TransferDialog dlg(m_sharePeers, m_selfPeer, {}, this, title,
                     tr("Выберите контакт, которому отправить цвет:"), tr("Отправить"));
  if (dlg.exec() != QDialog::Accepted) {
    return;
  }
  const QString peer = dlg.selectedPeer();
  if (peer.isEmpty()) {
    return;
  }
  if (!m_client->chat()->sendColorShare(peer)) {
    QMessageBox::warning(this, title, tr("Не удалось отправить цвет."));
    return;
  }
  QMessageBox::information(this, title,
                           tr("Цвет %1 отправлен: %2").arg(color, dlg.selectedDisplayName()));
}

void SettingsDialog::onShareTheme()
{
  if (!m_client || !m_client->chat() || !m_settings) {
    return;
  }
  const QString title = tr("Поделиться темой");
  const QString path = m_settings->appWallpaperPath();
  if (path.isEmpty() || !QFile::exists(path)) {
    QMessageBox::information(this, title, tr("Сначала выберите обои."));
    return;
  }
  const QPixmap wallpaper(path);
  if (wallpaper.isNull()) {
    QMessageBox::warning(this, title, tr("Не удалось открыть файл обоев."));
    return;
  }
  if (m_sharePeers.isEmpty()) {
    QMessageBox::information(this, title,
                             tr("Пока нет других пользователей OpenSource Communicator.\n"
                                "Они появятся после обмена Openping! при входе в сеть."));
    return;
  }

  TransferDialog dlg(m_sharePeers, m_selfPeer, {}, this, title,
                     tr("Выберите контакт, которому отправить тему:"), tr("Отправить"));
  if (dlg.exec() != QDialog::Accepted) {
    return;
  }
  const QString peer = dlg.selectedPeer();
  if (peer.isEmpty()) {
    return;
  }
  if (!m_client->chat()->sendThemeShare(peer, wallpaper, m_settings->appWallpaperOpacity(),
                                        m_settings->appWallpaperListOpacity())) {
    QMessageBox::warning(this, title, tr("Не удалось отправить тему."));
    return;
  }
  QMessageBox::information(this, title, tr("Тема отправлена: %1").arg(dlg.selectedDisplayName()));
}

void SettingsDialog::onAccept()
{
  stopPreview();
  m_settings->setInputDeviceId(m_inputDevice->currentData().toString());
  m_settings->setOutputDeviceId(m_outputDevice->currentData().toString());
  m_settings->setRingbackKind(static_cast<itl::AppSettings::RingtoneKind>(m_ringbackTone->currentData().toInt()));
  m_settings->setRingbackCustomPath(m_ringbackPath->text().trimmed());
  m_settings->setIncomingRingKind(static_cast<itl::AppSettings::RingtoneKind>(m_incomingTone->currentData().toInt()));
  m_settings->setIncomingRingCustomPath(m_incomingPath->text().trimmed());
  m_settings->setNetworkInterfaceName(m_networkInterface->currentData().toString());

  m_settings->setRecordingEnabled(m_recordingEnabledCheck->isChecked());
  m_settings->setRecordingDualTrack(m_recordingDualTrackCheck->isChecked());
  m_settings->setRecordingCombinedTrack(m_recordingCombinedCheck->isChecked());
  m_settings->setRecordingDirectory(m_recordingDirEdit->text());
  m_settings->setRecordingFilenameTemplate(m_recordingTemplateEdit->text());

  m_client->saveSettings();
  m_calls->applySettings();
  accept();
}
