#include "LoginDialog.h"

#include "protocol/CommunicatorClient.h"
#include "ui/StyleHelper.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <QIntValidator>
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

  m_loginCombo = new QComboBox;
  m_loginCombo->setEditable(true);
  m_loginCombo->setInsertPolicy(QComboBox::NoInsert);
  m_loginCombo->lineEdit()->setPlaceholderText(QStringLiteral("demo"));
  m_loginCombo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

  m_passwordEdit = new QLineEdit;
  m_passwordEdit->setEchoMode(QLineEdit::Password);
  m_passwordEdit->setPlaceholderText(QStringLiteral("demo"));

  form->addRow(tr("Логин"), m_loginCombo);
  form->addRow(tr("Пароль"), m_passwordEdit);
  layout->addLayout(form);

  m_rememberCheck = new QCheckBox(tr("Запомнить меня"));
  m_rememberCheck->setChecked(true);
  layout->addWidget(m_rememberCheck);

  m_advancedBtn = new QPushButton(tr("Расширенные"));
  m_advancedBtn->setFlat(true);
  m_advancedBtn->setCheckable(true);
  layout->addWidget(m_advancedBtn, 0, Qt::AlignLeft);

  m_advancedPanel = new QWidget;
  auto *advancedForm = itl::createDialogForm();
  advancedForm->setContentsMargins(0, 0, 0, 0);
  m_domainEdit = new QLineEdit;
  m_authDomainEdit = new QLineEdit;
  m_serverPortEdit = new QLineEdit;
  m_serverPortEdit->setPlaceholderText(QStringLiteral("443"));
  m_serverPortEdit->setValidator(new QIntValidator(1, 65535, m_serverPortEdit));
  m_partnerEdit = new QLineEdit(QStringLiteral("megafon"));
  advancedForm->addRow(tr("Домен"), m_domainEdit);
  advancedForm->addRow(tr("Auth-домен"), m_authDomainEdit);
  advancedForm->addRow(tr("Порт сервера"), m_serverPortEdit);
  advancedForm->addRow(tr("Partner"), m_partnerEdit);
#ifdef OSC_DEBUG_BUILD
  m_ignoreInsecureTlsCheck = new QCheckBox(tr("Игнорировать небезопасный TLS"));
  m_ignoreInsecureTlsCheck->setToolTip(tr("Для отладки: ws:// без TLS или wss:// с самоподписанным сертификатом."));
  advancedForm->addRow(QString(), m_ignoreInsecureTlsCheck);
#endif
  m_advancedPanel->setLayout(advancedForm);
  m_advancedPanel->setVisible(false);
  layout->addWidget(m_advancedPanel);

  QPushButton *cancel = nullptr;
  QPushButton *ok = nullptr;
  layout->addLayout(itl::createDialogButtonRow(&cancel, &ok, tr("Войти")));

  connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
  connect(ok, &QPushButton::clicked, this, &LoginDialog::onAccepted);
  connect(m_advancedBtn, &QPushButton::toggled, this, &LoginDialog::toggleAdvanced);
  connect(m_loginCombo, QOverload<int>::of(&QComboBox::activated), this, &LoginDialog::onAccountActivated);

  itl::applyFormDialogStyle(this);
}

void LoginDialog::toggleAdvanced(bool expanded)
{
  m_advancedPanel->setVisible(expanded);
  m_advancedBtn->setText(tr("Расширенные"));
  adjustSize();
}

void LoginDialog::applyCredentials(const itl::LoginCredentials &cred)
{
  {
    const QSignalBlocker blocker(m_loginCombo);
    m_loginCombo->setEditText(cred.login);
  }
  m_passwordEdit->setText(cred.password);
  m_domainEdit->setText(cred.domain);
  m_authDomainEdit->setText(cred.authDomain);
  m_serverPortEdit->setText(cred.serverPort > 0 && cred.serverPort != 443 ? QString::number(cred.serverPort) : QString());
  m_partnerEdit->setText(cred.partner.isEmpty() ? QStringLiteral("megafon") : cred.partner);
#ifdef OSC_DEBUG_BUILD
  if (m_ignoreInsecureTlsCheck) {
    m_ignoreInsecureTlsCheck->setChecked(cred.ignoreInsecureTls);
  }
#endif
}

itl::LoginCredentials LoginDialog::accountAt(int index) const
{
  if (index < 0 || index >= m_loginCombo->count()) {
    return {};
  }
  return m_loginCombo->itemData(index).value<itl::LoginCredentials>();
}

void LoginDialog::refreshAccountCombo(const QString &selectedLogin)
{
  const QSignalBlocker blocker(m_loginCombo);
  m_loginCombo->clear();

  const auto accounts = m_client->savedAccounts();
  int selectedIndex = -1;
  for (const itl::LoginCredentials &account : accounts) {
    QString label = account.login;
    if (!account.domain.isEmpty() && !label.contains(QLatin1Char('@'))) {
      label += QLatin1Char('@') + account.domain;
    }
    m_loginCombo->addItem(label, QVariant::fromValue(account));
    if (!selectedLogin.isEmpty()
        && (account.login.compare(selectedLogin, Qt::CaseInsensitive) == 0
            || label.compare(selectedLogin, Qt::CaseInsensitive) == 0)) {
      selectedIndex = m_loginCombo->count() - 1;
    }
  }

  if (selectedIndex >= 0) {
    m_loginCombo->setCurrentIndex(selectedIndex);
  } else if (!selectedLogin.isEmpty()) {
    m_loginCombo->setEditText(selectedLogin);
  }
}

void LoginDialog::loadFromClient()
{
  m_rememberCheck->setChecked(m_client->rememberMe());
  refreshAccountCombo(m_client->credentials().login);

  const int index = m_loginCombo->currentIndex();
  if (index >= 0 && m_loginCombo->count() > 0) {
    applyCredentials(accountAt(index));
  } else {
    applyCredentials(m_client->credentials());
  }

  m_advancedBtn->setChecked(false);
  toggleAdvanced(false);
}

itl::LoginCredentials LoginDialog::credentialsFromForm() const
{
  itl::LoginCredentials cred;
  cred.login = m_loginCombo->currentText().trimmed();
  cred.password = m_passwordEdit->text();
  cred.domain = m_domainEdit->text().trimmed();
  cred.authDomain = m_authDomainEdit->text().trimmed();
  cred.partner = m_partnerEdit->text().trimmed();
  const QString portText = m_serverPortEdit->text().trimmed();
  if (!portText.isEmpty()) {
    bool ok = false;
    const int port = portText.toInt(&ok);
    if (ok) {
      cred.serverPort = port;
    }
  }
#ifdef OSC_DEBUG_BUILD
  if (m_ignoreInsecureTlsCheck) {
    cred.ignoreInsecureTls = m_ignoreInsecureTlsCheck->isChecked();
  }
#endif
  return cred;
}

void LoginDialog::onAccountActivated(int index)
{
  const itl::LoginCredentials account = accountAt(index);
  if (account.login.isEmpty()) {
    return;
  }
  applyCredentials(account);
}

void LoginDialog::onAccepted()
{
  itl::LoginCredentials cred = credentialsFromForm();

  // Пустые логин и пароль — вход в demo-режим (подсказки в полях не меняем).
  if (cred.login.isEmpty() && cred.password.isEmpty()) {
    cred.login = QStringLiteral("demo");
    cred.password = QStringLiteral("demo");
  }

  if (cred.partner.isEmpty()) {
    cred.partner = QStringLiteral("megafon");
  }

  if (cred.domain.isEmpty() && cred.login.contains(QLatin1Char('@'))) {
    cred.domain = cred.login.section(QLatin1Char('@'), 1);
  }

  const bool remember = m_rememberCheck->isChecked();
  m_client->setRememberMe(remember);
  m_client->setCredentials(cred);
  m_client->saveSettings();
  accept();
}
