#include "LoginDialog.h"

#include "protocol/CommunicatorClient.h"
#include "ui/StyleHelper.h"

#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

LoginDialog::LoginDialog(itl::CommunicatorClient *client, QWidget *parent)
    : QDialog(parent)
    , m_client(client)
{
  setObjectName(QStringLiteral("loginDialog"));
  setWindowTitle(tr("Вход — Communicator"));
  setFixedWidth(420);

  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(20, 20, 20, 20);

  auto *title = new QLabel(tr("OpenSource Communicator"));
  title->setObjectName(QStringLiteral("loginTitle"));
  layout->addWidget(title);

  auto *subtitle = new QLabel(tr("Клиент для многих ВАТС на основе ITooLabs"));
  subtitle->setObjectName(QStringLiteral("loginSubtitle"));
  layout->addWidget(subtitle);

  auto *form = itl::createDialogForm();
  m_loginEdit = new QLineEdit;
  m_passwordEdit = new QLineEdit;
  m_passwordEdit->setEchoMode(QLineEdit::Password);
  form->addRow(tr("Логин"), m_loginEdit);
  form->addRow(tr("Пароль"), m_passwordEdit);
  layout->addLayout(form);

  m_advancedBtn = new QPushButton(tr("Расширенные ▾"));
  m_advancedBtn->setFlat(true);
  m_advancedBtn->setCheckable(true);
  layout->addWidget(m_advancedBtn, 0, Qt::AlignLeft);

  m_advancedPanel = new QWidget;
  auto *advancedForm = itl::createDialogForm();
  advancedForm->setContentsMargins(0, 0, 0, 0);
  m_domainEdit = new QLineEdit;
  m_authDomainEdit = new QLineEdit;
  m_partnerEdit = new QLineEdit(QStringLiteral("megafon"));
  advancedForm->addRow(tr("Домен"), m_domainEdit);
  advancedForm->addRow(tr("Auth-домен"), m_authDomainEdit);
  advancedForm->addRow(tr("Partner"), m_partnerEdit);
  m_advancedPanel->setLayout(advancedForm);
  m_advancedPanel->setVisible(false);
  layout->addWidget(m_advancedPanel);

  auto *buttons = new QHBoxLayout;
  QPushButton *cancel = nullptr;
  QPushButton *ok = nullptr;
  layout->addLayout(itl::createDialogButtonRow(&cancel, &ok, tr("Войти")));

  connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
  connect(ok, &QPushButton::clicked, this, &LoginDialog::onAccepted);
  connect(m_advancedBtn, &QPushButton::toggled, this, &LoginDialog::toggleAdvanced);

  itl::applyFormDialogStyle(this);
}

void LoginDialog::toggleAdvanced(bool expanded)
{
  m_advancedPanel->setVisible(expanded);
  m_advancedBtn->setText(expanded ? tr("Расширенные ▴") : tr("Расширенные ▾"));
  adjustSize();
}

void LoginDialog::loadFromClient()
{
  const auto cred = m_client->credentials();
  m_loginEdit->setText(cred.login);
  m_passwordEdit->setText(cred.password);
  m_domainEdit->setText(cred.domain);
  m_authDomainEdit->setText(cred.authDomain);
  m_partnerEdit->setText(cred.partner);

  m_advancedBtn->setChecked(false);
  toggleAdvanced(false);
}

void LoginDialog::onAccepted()
{
  itl::LoginCredentials cred;
  cred.login = m_loginEdit->text().trimmed();
  cred.password = m_passwordEdit->text();
  cred.domain = m_domainEdit->text().trimmed();
  cred.authDomain = m_authDomainEdit->text().trimmed();
  cred.partner = m_partnerEdit->text().trimmed();

  if (cred.partner.isEmpty()) {
    cred.partner = QStringLiteral("megafon");
  }

  if (cred.domain.isEmpty() && cred.login.contains(QLatin1Char('@'))) {
    cred.domain = cred.login.section(QLatin1Char('@'), 1);
  }

  m_client->setCredentials(cred);
  accept();
}
