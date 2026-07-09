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
                               const QString &excludePeer, QWidget *parent)
    : QDialog(parent)
    , m_selfPeer(selfPeer)
    , m_excludePeer(excludePeer)
    , m_peerNames(peerNames)
{
  setWindowTitle(tr("Перевод звонка"));
  setObjectName(QStringLiteral("transferDialog"));
  resize(380, 460);

  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(16, 16, 16, 16);
  layout->setSpacing(10);

  layout->addWidget(new QLabel(tr("Выберите контакт для перевода:")));

  m_searchEdit = new QLineEdit;
  m_searchEdit->setPlaceholderText(tr("Поиск..."));
  m_searchEdit->setClearButtonEnabled(true);
  layout->addWidget(m_searchEdit);

  m_contactsList = new QListWidget;
  m_contactsList->setObjectName(QStringLiteral("transferContactList"));
  m_contactsList->setSelectionMode(QAbstractItemView::SingleSelection);
  layout->addWidget(m_contactsList, 1);

  QPushButton *cancelBtn = nullptr;
  layout->addLayout(itl::createDialogButtonRow(&cancelBtn, &m_transferBtn, tr("Перевести")));
  m_transferBtn->setEnabled(false);

  connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
  connect(m_transferBtn, &QPushButton::clicked, this, &TransferDialog::onAccepted);
  connect(m_searchEdit, &QLineEdit::textChanged, this, [this](const QString &) { rebuildList(); });
  connect(m_contactsList, &QListWidget::itemSelectionChanged, this, &TransferDialog::updateTransferEnabled);
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

  updateTransferEnabled();
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

void TransferDialog::updateTransferEnabled()
{
  m_transferBtn->setEnabled(!selectedPeer().isEmpty());
}

void TransferDialog::onAccepted()
{
  if (selectedPeer().isEmpty()) {
    QMessageBox::warning(this, tr("Перевод"), tr("Выберите контакт."));
    return;
  }
  accept();
}
