#include "AddContactDialog.h"

#include "ui/StyleHelper.h"

#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

namespace {
bool looksLikePhone(const QString &value)
{
  if (value.isEmpty()) {
    return false;
  }
  for (const QChar ch : value) {
    if (!ch.isDigit() && ch != QLatin1Char('+') && ch != QLatin1Char('*') && ch != QLatin1Char('#')) {
      return false;
    }
  }
  return true;
}
} // namespace

AddContactDialog::AddContactDialog(const QString &domain, QWidget *parent)
    : QDialog(parent)
    , m_domain(domain)
{
  setWindowTitle(tr("Добавить контакт"));
  setObjectName(QStringLiteral("addContactDialog"));
  resize(380, 200);

  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(16, 16, 16, 16);
  layout->setSpacing(10);

  auto *form = itl::createDialogForm();
  m_nameEdit = new QLineEdit;
  m_nameEdit->setPlaceholderText(tr("Змей Шипа"));
  m_phoneEdit = new QLineEdit;
  m_phoneEdit->setPlaceholderText(tr("+79991234567"));
  m_extEdit = new QLineEdit;
  m_extEdit->setPlaceholderText(QStringLiteral("shipa_2"));

  form->addRow(tr("Имя"), m_nameEdit);
  form->addRow(tr("Телефон"), m_phoneEdit);
  form->addRow(tr("Внутренний номер"), m_extEdit);
  layout->addLayout(form);

  auto *hint = new QLabel(tr("Укажите телефон или внутренний номер."));
  hint->setWordWrap(true);
  layout->addWidget(hint);

  QPushButton *cancelBtn = nullptr;
  QPushButton *acceptBtn = nullptr;
  layout->addLayout(itl::createDialogButtonRow(&cancelBtn, &acceptBtn, tr("Добавить")));
  connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
  connect(acceptBtn, &QPushButton::clicked, this, &AddContactDialog::onAccepted);

  itl::applyFormDialogStyle(this);
}

itl::CustomContact AddContactDialog::contact() const
{
  return m_result;
}

void AddContactDialog::onAccepted()
{
  const QString name = m_nameEdit->text().trimmed();
  const QString phone = m_phoneEdit->text().trimmed();
  const QString ext = m_extEdit->text().trimmed();

  if (name.isEmpty()) {
    QMessageBox::warning(this, tr("Контакт"), tr("Введите имя контакта."));
    return;
  }
  if (phone.isEmpty() && ext.isEmpty()) {
    QMessageBox::warning(this, tr("Контакт"), tr("Введите телефон или внутренний номер."));
    return;
  }

  m_result.name = name;
  m_result.phone = phone;
  m_result.ext = ext;

  if (!phone.isEmpty()) {
    m_result.peer = looksLikePhone(phone) ? phone : phone + QLatin1Char('@') + m_domain;
  } else {
    m_result.peer = ext + QLatin1Char('@') + m_domain;
  }

  accept();
}
