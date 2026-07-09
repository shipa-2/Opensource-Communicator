#include "SettingsDialog.h"

#include "ProfileAvatarWidget.h"
#include "audio/AudioDeviceUtils.h"
#include "audio/IncomingRingPlayer.h"
#include "audio/RingbackPlayer.h"
#include "calls/CallManager.h"
#include "protocol/CommunicatorClient.h"
#include "settings/AppSettings.h"
#include "ui/StyleHelper.h"

#include <QAudioDevice>
#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTabWidget>
#include <QTimer>
#include <QVBoxLayout>

SettingsDialog::SettingsDialog(itl::CommunicatorClient *client, itl::CallManager *calls,
                               const QString &displayName, QWidget *parent)
    : QDialog(parent)
    , m_client(client)
    , m_calls(calls)
    , m_settings(&client->appSettings())
    , m_ringbackPreview(new itl::RingbackPlayer(this))
    , m_incomingPreview(new itl::IncomingRingPlayer(this))
    , m_previewTimer(new QTimer(this))
{
  setWindowTitle(tr("Настройки"));
  setObjectName(QStringLiteral("settingsDialog"));
  resize(460, 420);
  m_selfName = displayName;

  auto *mainLayout = new QVBoxLayout(this);
  mainLayout->setContentsMargins(16, 16, 16, 16);

  auto *tabs = new QTabWidget;

  auto *soundTab = new QWidget;
  auto *soundLayout = new QVBoxLayout(soundTab);
  soundLayout->setContentsMargins(8, 8, 8, 8);

  auto *audioGroup = new QGroupBox(tr("Звук"));
  auto *form = new QFormLayout(audioGroup);

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

  form->addRow(tr("Микрофон"), m_inputDevice);
  form->addRow(tr("Динамики"), m_outputDevice);

  auto *ringbackToneRow = new QHBoxLayout;
  m_ringbackPreviewBtn = new QPushButton(QStringLiteral("▶"));
  m_ringbackPreviewBtn->setToolTip(tr("Прослушать"));
  m_ringbackPreviewBtn->setFixedWidth(m_ringbackPreviewBtn->sizeHint().height());
  ringbackToneRow->addWidget(m_ringbackTone, 1);
  ringbackToneRow->addWidget(m_ringbackPreviewBtn);
  form->addRow(tr("Звук дозвона ДО клиента"), ringbackToneRow);

  auto *ringbackRow = new QHBoxLayout;
  m_ringbackPath = new QLineEdit;
  m_ringbackPath->setPlaceholderText(tr("Путь к файлу (wav, mp3, ogg)"));
  m_ringbackBrowse = new QPushButton(tr("Обзор..."));
  ringbackRow->addWidget(m_ringbackPath, 1);
  ringbackRow->addWidget(m_ringbackBrowse);
  form->addRow(tr("Файл гудка"), ringbackRow);

  auto *incomingToneRow = new QHBoxLayout;
  m_incomingPreviewBtn = new QPushButton(QStringLiteral("▶"));
  m_incomingPreviewBtn->setToolTip(tr("Прослушать"));
  m_incomingPreviewBtn->setFixedWidth(m_incomingPreviewBtn->sizeHint().height());
  incomingToneRow->addWidget(m_incomingTone, 1);
  incomingToneRow->addWidget(m_incomingPreviewBtn);
  form->addRow(tr("Звук дозвона ОТ клиента"), incomingToneRow);
  auto *incomingRow = new QHBoxLayout;
  m_incomingPath = new QLineEdit;
  m_incomingPath->setPlaceholderText(tr("Путь к файлу (wav, mp3, ogg)"));
  m_incomingBrowse = new QPushButton(tr("Обзор..."));
  incomingRow->addWidget(m_incomingPath, 1);
  incomingRow->addWidget(m_incomingBrowse);
  form->addRow(tr("Файл звонка"), incomingRow);

  soundLayout->addWidget(audioGroup);
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
  removePhotoBtn->setVisible(!m_settings->profileAvatarPath().isEmpty());
  avatarBtns->addWidget(photoBtn);
  avatarBtns->addWidget(colorBtn);
  avatarBtns->addWidget(removePhotoBtn);
  avatarBtns->addStretch();
  avatarRow->addLayout(avatarBtns);
  accountLayout->addLayout(avatarRow);

  connect(photoBtn, &QPushButton::clicked, this, [this, removePhotoBtn]() {
    const QString path = QFileDialog::getOpenFileName(this, tr("Выберите фото"), {},
                                                       tr("Изображения (*.png *.jpg *.jpeg *.bmp *.webp);;Все файлы (*)"));
    if (!path.isEmpty()) {
      m_settings->setProfileAvatarPath(path);
      m_accountAvatar->refreshFromSettings();
      removePhotoBtn->setVisible(true);
    }
  });
  connect(colorBtn, &QPushButton::clicked, this, [this]() {
    const QColor color = QColorDialog::getColor(m_settings->profileAvatarColor(), this, tr("Выберите цвет фона"));
    if (color.isValid()) {
      m_settings->setProfileAvatarColor(color.name());
      m_accountAvatar->refreshFromSettings();
    }
  });
  connect(removePhotoBtn, &QPushButton::clicked, this, [this, removePhotoBtn]() {
    m_settings->setProfileAvatarPath({});
    m_accountAvatar->refreshFromSettings();
    removePhotoBtn->setVisible(false);
  });

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
  accountLayout->addLayout(accountForm);

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

void SettingsDialog::onAccept()
{
  stopPreview();
  m_settings->setInputDeviceId(m_inputDevice->currentData().toString());
  m_settings->setOutputDeviceId(m_outputDevice->currentData().toString());
  m_settings->setRingbackKind(static_cast<itl::AppSettings::RingtoneKind>(m_ringbackTone->currentData().toInt()));
  m_settings->setRingbackCustomPath(m_ringbackPath->text().trimmed());
  m_settings->setIncomingRingKind(static_cast<itl::AppSettings::RingtoneKind>(m_incomingTone->currentData().toInt()));
  m_settings->setIncomingRingCustomPath(m_incomingPath->text().trimmed());

  m_settings->setRecordingEnabled(m_recordingEnabledCheck->isChecked());
  m_settings->setRecordingDualTrack(m_recordingDualTrackCheck->isChecked());
  m_settings->setRecordingCombinedTrack(m_recordingCombinedCheck->isChecked());
  m_settings->setRecordingDirectory(m_recordingDirEdit->text());
  m_settings->setRecordingFilenameTemplate(m_recordingTemplateEdit->text());

  m_client->saveSettings();
  m_calls->applySettings();
  accept();
}
