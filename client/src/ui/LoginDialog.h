#pragma once

#include <QDialog>

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

private:
    itl::CommunicatorClient *m_client = nullptr;
    QLineEdit *m_loginEdit = nullptr;
    QLineEdit *m_passwordEdit = nullptr;
    QLineEdit *m_domainEdit = nullptr;
    QLineEdit *m_authDomainEdit = nullptr;
    QLineEdit *m_partnerEdit = nullptr;
    QWidget *m_advancedPanel = nullptr;
    QPushButton *m_advancedBtn = nullptr;
};
