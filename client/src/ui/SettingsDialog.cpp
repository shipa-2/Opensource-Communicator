#include "SettingsDialog.h"

#include "audio/AudioDeviceUtils.h"
#include "audio/IncomingRingPlayer.h"
#include "audio/RingbackPlayer.h"
#include "calls/CallManager.h"
#include "protocol/CommunicatorClient.h"
#include "settings/AppSettings.h"
#include "ui/StyleHelper.h"

#include <QAudioDevice>
#include <QComboBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>

SettingsDialog::SettingsDialog(itl::CommunicatorClient *client, itl::CallManager *calls, QWidget *parent)
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
  resize(460, 360);

  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(16, 16, 16, 16);

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

  layout->addWidget(audioGroup);

  auto *accountBtn = new QPushButton(tr("Учётная запись..."));
  layout->addWidget(accountBtn);

  QPushButton *cancel = nullptr;
  QPushButton *ok = nullptr;
  layout->addLayout(itl::createDialogButtonRow(&cancel, &ok, tr("Сохранить")));

  connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
  connect(ok, &QPushButton::clicked, this, &SettingsDialog::onAccept);
  connect(accountBtn, &QPushButton::clicked, this, &SettingsDialog::onAccountSettings);
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

void SettingsDialog::onAccountSettings()
{
  done(2);
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
  m_client->saveSettings();
  m_calls->applySettings();
  accept();
}
