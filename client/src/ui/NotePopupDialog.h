#pragma once

#include <QDialog>

class QLabel;
class QTextEdit;

namespace itl {
class AppSettings;
}

class NotePopupDialog : public QDialog {
    Q_OBJECT

public:
    NotePopupDialog(const QString &peer, const QString &displayName, itl::AppSettings *settings,
                    QWidget *parent = nullptr);

    void setDuringCall(bool duringCall);

private:
    void onSave();

    QString m_peer;
    itl::AppSettings *m_settings = nullptr;
    QLabel *m_hintLabel = nullptr;
    QTextEdit *m_editor = nullptr;
};
