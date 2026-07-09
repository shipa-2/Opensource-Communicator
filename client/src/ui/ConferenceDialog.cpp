#include "ConferenceDialog.h"

#include "ui/StyleHelper.h"

#include <QFormLayout>
#include <QGridLayout>
#include <QHBoxLayout>
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
  setWindowTitle(tr("Начать конференцию"));
  setObjectName(QStringLiteral("conferenceDialog"));
  resize(620, 520);

  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(16, 16, 16, 16);
  layout->setSpacing(10);

  auto *form = itl::createDialogForm();
  m_subjectEdit = new QLineEdit;
  m_subjectEdit->setPlaceholderText(tr("Введите тему конференции"));
  form->addRow(tr("Тема"), m_subjectEdit);
  layout->addLayout(form);

  auto *columns = new QGridLayout;
  columns->setHorizontalSpacing(10);
  columns->setVerticalSpacing(6);

  // Левая колонка — доступные контакты.
  columns->addWidget(new QLabel(tr("Контакты:")), 0, 0);
  m_availableList = new QListWidget;
  m_availableList->setSelectionMode(QAbstractItemView::ExtendedSelection);
  for (auto it = m_peerNames.cbegin(); it != m_peerNames.cend(); ++it) {
    if (it.key() == m_selfPeer) {
      continue;
    }
    auto *item = new QListWidgetItem(it.value());
    item->setData(Qt::UserRole, it.key());
    m_availableList->addItem(item);
  }
  m_availableList->sortItems();
  columns->addWidget(m_availableList, 1, 0, 3, 1);

  // Средняя колонка — кнопки перемещения.
  auto *arrowsSpeakers = new QVBoxLayout;
  auto *toSpeakers = new QPushButton(QStringLiteral(">"));
  auto *fromSpeakers = new QPushButton(QStringLiteral("<"));
  auto *toListeners = new QPushButton(QStringLiteral(">>"));
  auto *fromListeners = new QPushButton(QStringLiteral("<<"));
  for (QPushButton *btn : {toSpeakers, fromSpeakers, toListeners, fromListeners}) {
    btn->setObjectName(QStringLiteral("footerBtn"));
    btn->setFixedWidth(44);
  }
  toSpeakers->setToolTip(tr("В собеседники"));
  fromSpeakers->setToolTip(tr("Убрать из собеседников"));
  toListeners->setToolTip(tr("В слушатели"));
  fromListeners->setToolTip(tr("Убрать из слушателей"));
  arrowsSpeakers->addStretch();
  arrowsSpeakers->addWidget(toSpeakers);
  arrowsSpeakers->addWidget(fromSpeakers);
  arrowsSpeakers->addSpacing(20);
  arrowsSpeakers->addWidget(toListeners);
  arrowsSpeakers->addWidget(fromListeners);
  arrowsSpeakers->addStretch();
  auto *arrowsWrap = new QWidget;
  arrowsWrap->setLayout(arrowsSpeakers);
  columns->addWidget(arrowsWrap, 1, 1, 3, 1);

  // Правая колонка — собеседники и слушатели.
  columns->addWidget(new QLabel(tr("Участвуют как собеседники:")), 0, 2);
  m_speakersList = new QListWidget;
  m_speakersList->setSelectionMode(QAbstractItemView::ExtendedSelection);
  columns->addWidget(m_speakersList, 1, 2);

  columns->addWidget(new QLabel(tr("Участвуют как слушатели:")), 2, 2);
  m_listenersList = new QListWidget;
  m_listenersList->setSelectionMode(QAbstractItemView::ExtendedSelection);
  columns->addWidget(m_listenersList, 3, 2);

  columns->setColumnStretch(0, 3);
  columns->setColumnStretch(1, 0);
  columns->setColumnStretch(2, 3);
  columns->setRowStretch(1, 1);
  columns->setRowStretch(3, 1);
  layout->addLayout(columns, 1);

  m_searchEdit = new QLineEdit;
  m_searchEdit->setPlaceholderText(tr("Поиск / добавление номера"));
  m_searchEdit->setClearButtonEnabled(true);
  layout->addWidget(m_searchEdit);

  QPushButton *cancelBtn = nullptr;
  QPushButton *acceptBtn = nullptr;
  layout->addLayout(itl::createDialogButtonRow(&cancelBtn, &acceptBtn, tr("Начать")));
  connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
  connect(acceptBtn, &QPushButton::clicked, this, &ConferenceDialog::onAccepted);

  connect(toSpeakers, &QPushButton::clicked, this, [this]() { moveSelection(m_availableList, m_speakersList); });
  connect(fromSpeakers, &QPushButton::clicked, this, [this]() { moveSelection(m_speakersList, m_availableList); });
  connect(toListeners, &QPushButton::clicked, this, [this]() { moveSelection(m_availableList, m_listenersList); });
  connect(fromListeners, &QPushButton::clicked, this, [this]() { moveSelection(m_listenersList, m_availableList); });
  connect(m_availableList, &QListWidget::itemDoubleClicked, this,
          [this](QListWidgetItem *) { moveSelection(m_availableList, m_speakersList); });
  connect(m_speakersList, &QListWidget::itemDoubleClicked, this,
          [this](QListWidgetItem *) { moveSelection(m_speakersList, m_availableList); });
  connect(m_listenersList, &QListWidget::itemDoubleClicked, this,
          [this](QListWidgetItem *) { moveSelection(m_listenersList, m_availableList); });
  connect(m_searchEdit, &QLineEdit::textChanged, this, &ConferenceDialog::applySearchFilter);

  itl::applyFormDialogStyle(this);
}

QListWidgetItem *ConferenceDialog::takeItemCopy(QListWidget *from, int row)
{
  return from->takeItem(row);
}

void ConferenceDialog::moveSelection(QListWidget *from, QListWidget *to)
{
  if (!from || !to) {
    return;
  }
  const QList<QListWidgetItem *> selected = from->selectedItems();
  for (QListWidgetItem *item : selected) {
    const int row = from->row(item);
    QListWidgetItem *taken = from->takeItem(row);
    to->addItem(taken);
  }
  to->sortItems();
}

void ConferenceDialog::moveAll(QListWidget *from, QListWidget *to)
{
  if (!from || !to) {
    return;
  }
  while (from->count() > 0) {
    to->addItem(from->takeItem(0));
  }
  to->sortItems();
}

void ConferenceDialog::applySearchFilter(const QString &text)
{
  const QString needle = text.trimmed();
  for (int i = 0; i < m_availableList->count(); ++i) {
    QListWidgetItem *item = m_availableList->item(i);
    const bool match = needle.isEmpty()
                       || item->text().contains(needle, Qt::CaseInsensitive)
                       || item->data(Qt::UserRole).toString().contains(needle, Qt::CaseInsensitive);
    item->setHidden(!match);
  }
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

  for (int i = 0; i < m_speakersList->count(); ++i) {
    QListWidgetItem *item = m_speakersList->item(i);
    itl::ConferenceParticipant participant;
    participant.peer = item->data(Qt::UserRole).toString();
    participant.name = item->text();
    participant.listener = false;
    result.append(participant);
  }

  for (int i = 0; i < m_listenersList->count(); ++i) {
    QListWidgetItem *item = m_listenersList->item(i);
    itl::ConferenceParticipant participant;
    participant.peer = item->data(Qt::UserRole).toString();
    participant.name = item->text();
    participant.listener = true;
    result.append(participant);
  }

  return result;
}

void ConferenceDialog::onAccepted()
{
  if (m_speakersList->count() + m_listenersList->count() == 0) {
    QMessageBox::warning(this, tr("Конференция"), tr("Выберите хотя бы одного участника."));
    return;
  }
  accept();
}
