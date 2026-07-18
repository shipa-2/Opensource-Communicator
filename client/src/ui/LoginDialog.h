#pragma once

#include <QDialog>

class QCheckBox;
class QComboBox;
class QLineEdit;
class QPushButton;
class QWidget;

namespace itl {
class CommunicatorClient;
struct LoginCredentials;
}

class LoginDialog : public QDialog {
    Q_OBJECT

public:
    explicit LoginDialog(itl::CommunicatorClient *client, QWidget *parent = nullptr);

    void loadFromClient();

private slots:
    void onAccepted();
    void toggleAdvanced(bool expanded);
    void onAccountActivated(int index);

private:
    void applyCredentials(const itl::LoginCredentials &cred);
    void refreshAccountCombo(const QString &selectedLogin = {});
    itl::LoginCredentials credentialsFromForm() const;
    itl::LoginCredentials accountAt(int index) const;

    itl::CommunicatorClient *m_client = nullptr;
    QComboBox *m_loginCombo = nullptr;
    QLineEdit *m_passwordEdit = nullptr;
    QLineEdit *m_domainEdit = nullptr;
    QLineEdit *m_authDomainEdit = nullptr;
    QLineEdit *m_serverPortEdit = nullptr;
    QLineEdit *m_partnerEdit = nullptr;
    QCheckBox *m_rememberCheck = nullptr;
    QWidget *m_advancedPanel = nullptr;
    QPushButton *m_advancedBtn = nullptr;
    QCheckBox *m_ignoreInsecureTlsCheck = nullptr;
};
