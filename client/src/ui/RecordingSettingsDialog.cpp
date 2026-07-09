#include "RecordingSettingsDialog.h"

#include "protocol/CommunicatorClient.h"
#include "settings/AppSettings.h"
#include "ui/StyleHelper.h"

#include <QCheckBox>
#include <QEvent>
#include <QFileDialog>
#include <QFormLayout>
#include <QHelpEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QToolTip>
#include <QVBoxLayout>

namespace {

class TemplateHintLineEdit : public QLineEdit {
public:
    using QLineEdit::QLineEdit;

protected:
    bool event(QEvent *event) override
    {
        if (event->type() == QEvent::ToolTip) {
            const auto *help = static_cast<const QHelpEvent *>(event);
            QToolTip::showText(help->globalPos(), itl::AppSettings::recordingFilenameSyntaxHelp(), this);
            return true;
        }
        return QLineEdit::event(event);
    }
};

} // namespace

RecordingSettingsDialog::RecordingSettingsDialog(itl::CommunicatorClient *client, QWidget *parent)
    : QDialog(parent)
    , m_client(client)
{
  setWindowTitle(tr("Запись разговоров"));
  setObjectName(QStringLiteral("recordingSettingsDialog"));
  resize(520, 300);

  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(16, 16, 16, 16);
  layout->setSpacing(10);

  m_enabledCheck = new QCheckBox(tr("Записывать разговоры"));
  layout->addWidget(m_enabledCheck);

  m_dualTrackCheck = new QCheckBox(tr("Запись в две дорожки (абонент и оператор отдельно)"));
  layout->addWidget(m_dualTrackCheck);

  auto *form = itl::createDialogForm();

  auto *directoryRow = new QHBoxLayout;
  m_directoryEdit = new QLineEdit;
  m_directoryEdit->setPlaceholderText(itl::AppSettings::defaultRecordingDirectory());
  auto *browseBtn = new QPushButton(tr("Обзор..."));
  directoryRow->addWidget(m_directoryEdit, 1);
  directoryRow->addWidget(browseBtn);
  form->addRow(tr("Папка"), directoryRow);

  m_filenameEdit = new TemplateHintLineEdit;
  m_filenameEdit->setPlaceholderText(QStringLiteral("%dmy_%h-%m-%s_%name"));
  m_filenameEdit->setClearButtonEnabled(true);
  form->addRow(tr("Имя файла"), m_filenameEdit);
  layout->addLayout(form);

  m_previewLabel = new QLabel;
  m_previewLabel->setObjectName(QStringLiteral("recordingPreviewLabel"));
  m_previewLabel->setWordWrap(true);
  layout->addWidget(m_previewLabel);

  QPushButton *cancelBtn = nullptr;
  QPushButton *acceptBtn = nullptr;
  layout->addLayout(itl::createDialogButtonRow(&cancelBtn, &acceptBtn, tr("Сохранить")));
  connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
  connect(acceptBtn, &QPushButton::clicked, this, &RecordingSettingsDialog::onAccepted);
  connect(browseBtn, &QPushButton::clicked, this, &RecordingSettingsDialog::onBrowseDirectory);
  connect(m_filenameEdit, &QLineEdit::textChanged, this, &RecordingSettingsDialog::updatePreview);
  connect(m_dualTrackCheck, &QCheckBox::toggled, this, &RecordingSettingsDialog::updatePreview);

  itl::AppSettings &settings = m_client->appSettings();
  m_enabledCheck->setChecked(settings.recordingEnabled());
  m_dualTrackCheck->setChecked(settings.recordingDualTrack());
  m_directoryEdit->setText(settings.recordingDirectory());
  m_filenameEdit->setText(settings.recordingFilenameTemplate());
  updatePreview();

  itl::applyFormDialogStyle(this);
}

void RecordingSettingsDialog::onBrowseDirectory()
{
  const QString path = QFileDialog::getExistingDirectory(
      this, tr("Папка для записей"),
      m_directoryEdit->text().trimmed().isEmpty() ? itl::AppSettings::defaultRecordingDirectory()
                                                   : m_directoryEdit->text().trimmed());
  if (!path.isEmpty()) {
    m_directoryEdit->setText(path);
  }
}

void RecordingSettingsDialog::updatePreview()
{
  const QString example = itl::AppSettings::expandRecordingFilenameTemplate(
      m_filenameEdit->text(), tr("Иван Иванов"));
  if (m_dualTrackCheck->isChecked()) {
    m_previewLabel->setText(tr("Пример: %1.wav, %1_manager.wav, %1_caller.wav").arg(example));
  } else {
    m_previewLabel->setText(tr("Пример: %1.wav").arg(example));
  }
}

void RecordingSettingsDialog::onAccepted()
{
  itl::AppSettings &settings = m_client->appSettings();
  settings.setRecordingEnabled(m_enabledCheck->isChecked());
  settings.setRecordingDualTrack(m_dualTrackCheck->isChecked());
  settings.setRecordingDirectory(m_directoryEdit->text());
  settings.setRecordingFilenameTemplate(m_filenameEdit->text());
  m_client->saveSettings();
  accept();
}
