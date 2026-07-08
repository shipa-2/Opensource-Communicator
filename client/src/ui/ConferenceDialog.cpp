#include "ConferenceDialog.h"

#include "ui/StyleHelper.h"

#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

ConferenceDialog::ConferenceDialog(const QHash<QString, QString> &peerNames, const QString &selfPeer,
                                     QWidget *parent)
    : QDialog(parent)
    , m_selfPeer(selfPeer)
    , m_peerNames(peerNames)
{
  setWindowTitle(tr("Конференция"));
  setObjectName(QStringLiteral("conferenceDialog"));
  resize(380, 420);

  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(16, 16, 16, 16);
  layout->setSpacing(10);

  auto *form = itl::createDialogForm();
  m_subjectEdit = new QLineEdit;
  m_subjectEdit->setPlaceholderText(tr("Совещание"));
  form->addRow(tr("Тема"), m_subjectEdit);
  layout->addLayout(form);

  layout->addWidget(new QLabel(tr("Выберите участников:")));

  m_participantsList = new QListWidget;
  m_participantsList->setSelectionMode(QAbstractItemView::NoSelection);
  for (auto it = m_peerNames.cbegin(); it != m_peerNames.cend(); ++it) {
    if (it.key() == m_selfPeer) {
      continue;
    }
    auto *item = new QListWidgetItem(it.value());
    item->setData(Qt::UserRole, it.key());
    item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
    item->setCheckState(Qt::Unchecked);
    m_participantsList->addItem(item);
  }
  layout->addWidget(m_participantsList, 1);

  QPushButton *cancelBtn = nullptr;
  QPushButton *acceptBtn = nullptr;
  layout->addLayout(itl::createDialogButtonRow(&cancelBtn, &acceptBtn, tr("Создать")));
  connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
  connect(acceptBtn, &QPushButton::clicked, this, &ConferenceDialog::onAccepted);

  itl::applyFormDialogStyle(this);
}

QString ConferenceDialog::subject() const
{
  return m_subjectEdit->text().trimmed();
}

QList<itl::ConferenceParticipant> ConferenceDialog::participants() const
{
  QList<itl::ConferenceParticipant> result;

  itl::ConferenceParticipant self;
  self.peer = m_selfPeer;
  self.name = m_peerNames.value(m_selfPeer);
  self.owner = true;
  self.operatorPeer = true;
  result.append(self);

  for (int i = 0; i < m_participantsList->count(); ++i) {
    auto *item = m_participantsList->item(i);
    if (item->checkState() != Qt::Checked) {
      continue;
    }
    itl::ConferenceParticipant participant;
    participant.peer = item->data(Qt::UserRole).toString();
    participant.name = item->text();
    participant.owner = false;
    participant.operatorPeer = false;
    result.append(participant);
  }

  return result;
}

void ConferenceDialog::onAccepted()
{
  int checked = 0;
  for (int i = 0; i < m_participantsList->count(); ++i) {
    if (m_participantsList->item(i)->checkState() == Qt::Checked) {
      ++checked;
    }
  }
  if (checked == 0) {
    QMessageBox::warning(this, tr("Конференция"), tr("Выберите хотя бы одного участника."));
    return;
  }
  accept();
}
