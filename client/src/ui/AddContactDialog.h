#pragma once

#include "settings/AppSettings.h"

#include <QDialog>

class QLabel;

class AddContactDialog : public QDialog {
    Q_OBJECT

public:
    explicit AddContactDialog(const QString &domain, QWidget *parent = nullptr);

    itl::CustomContact contact() const;

private:
    void onAccepted();

    QString m_domain;
    class QLineEdit *m_nameEdit = nullptr;
    class QLineEdit *m_phoneEdit = nullptr;
    class QLineEdit *m_extEdit = nullptr;
    itl::CustomContact m_result;
};
