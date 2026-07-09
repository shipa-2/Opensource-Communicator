#pragma once

#include <QDialog>

class QLabel;
class QPushButton;
class QTextEdit;

namespace itl {
class AppSettings;
}

class NotePopupDialog : public QDialog {
    Q_OBJECT

public:
    static constexpr int CallResult = 2;

    NotePopupDialog(const QString &peer, const QString &displayName, itl::AppSettings *settings,
                    QWidget *parent = nullptr);

    void setDuringCall(bool duringCall);
    void setShowCallAction(bool show);

private:
    void saveNote();
    void onSave();
    void onCall();
    void onClose();

    QString m_peer;
    itl::AppSettings *m_settings = nullptr;
    QLabel *m_hintLabel = nullptr;
    QTextEdit *m_editor = nullptr;
    QPushButton *m_primaryBtn = nullptr;
    bool m_showCallAction = false;
};
