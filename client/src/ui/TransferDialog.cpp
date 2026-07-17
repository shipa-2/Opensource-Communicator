#include "TransferDialog.h"

#include "ui/StyleHelper.h"

#include <QAbstractItemView>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

#include <algorithm>

TransferDialog::TransferDialog(const QHash<QString, QString> &peerNames, const QString &selfPeer,
                               const QString &excludePeer, QWidget *parent, const QString &title,
                               const QString &prompt, const QString &acceptLabel)
    : QDialog(parent)
    , m_selfPeer(selfPeer)
    , m_excludePeer(excludePeer)
    , m_acceptWarningTitle(title.isEmpty() ? tr("Перевод") : title)
    , m_peerNames(peerNames)
{
  setWindowTitle(title.isEmpty() ? tr("Перевод звонка") : title);
  setObjectName(QStringLiteral("transferDialog"));
  resize(380, 460);

  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(16, 16, 16, 16);
  layout->setSpacing(10);

  layout->addWidget(new QLabel(prompt.isEmpty() ? tr("Выберите контакт для перевода:") : prompt));

  m_searchEdit = new QLineEdit;
  m_searchEdit->setPlaceholderText(tr("Поиск..."));
  m_searchEdit->setClearButtonEnabled(true);
  layout->addWidget(m_searchEdit);

  m_contactsList = new QListWidget;
  m_contactsList->setObjectName(QStringLiteral("transferContactList"));
  m_contactsList->setSelectionMode(QAbstractItemView::SingleSelection);
  layout->addWidget(m_contactsList, 1);

  QPushButton *cancelBtn = nullptr;
  layout->addLayout(itl::createDialogButtonRow(
      &cancelBtn, &m_acceptBtn, acceptLabel.isEmpty() ? tr("Перевести") : acceptLabel));
  m_acceptBtn->setEnabled(false);

  connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
  connect(m_acceptBtn, &QPushButton::clicked, this, &TransferDialog::onAccepted);
  connect(m_searchEdit, &QLineEdit::textChanged, this, [this](const QString &) { rebuildList(); });
  connect(m_contactsList, &QListWidget::itemSelectionChanged, this, &TransferDialog::updateAcceptEnabled);
  connect(m_contactsList, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem *) { onAccepted(); });

  rebuildList();
  itl::applyFormDialogStyle(this);
}

void TransferDialog::rebuildList()
{
  const QString selected = selectedPeer();
  const QString query = m_searchEdit->text().trimmed().toLower();

  m_contactsList->clear();

  QStringList peers = m_peerNames.keys();
  std::sort(peers.begin(), peers.end(), [this](const QString &a, const QString &b) {
    return m_peerNames.value(a).localeAwareCompare(m_peerNames.value(b)) < 0;
  });

  for (const QString &peer : peers) {
    if (peer == m_selfPeer || peer == m_excludePeer) {
      continue;
    }
    const QString name = m_peerNames.value(peer);
    if (!query.isEmpty() && !name.toLower().contains(query) && !peer.toLower().contains(query)) {
      continue;
    }
    auto *item = new QListWidgetItem(name);
    item->setData(Qt::UserRole, peer);
    item->setToolTip(peer);
    m_contactsList->addItem(item);
    if (peer == selected) {
      item->setSelected(true);
      m_contactsList->setCurrentItem(item);
    }
  }

  updateAcceptEnabled();
}

QString TransferDialog::selectedPeer() const
{
  const QListWidgetItem *item = m_contactsList->currentItem();
  return item ? item->data(Qt::UserRole).toString() : QString();
}

QString TransferDialog::selectedDisplayName() const
{
  const QListWidgetItem *item = m_contactsList->currentItem();
  return item ? item->text() : QString();
}

void TransferDialog::updateAcceptEnabled()
{
  m_acceptBtn->setEnabled(!selectedPeer().isEmpty());
}

void TransferDialog::onAccepted()
{
  if (selectedPeer().isEmpty()) {
    QMessageBox::warning(this, m_acceptWarningTitle, tr("Выберите контакт."));
    return;
  }
  accept();
}
