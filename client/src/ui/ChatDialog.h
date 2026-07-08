#pragma once

#include <QDialog>

class QLineEdit;
class QPlainTextEdit;

namespace itl {
class CommunicatorClient;
}

class ChatDialog : public QDialog {
    Q_OBJECT

public:
    explicit ChatDialog(itl::CommunicatorClient *client, QWidget *parent = nullptr);

    void openForPeer(const QString &peer, const QString &displayName);

private slots:
    void onSend();
    void onChatMessage(const QString &peer, const QString &text, bool incoming);

private:
    void loadHistory();
    void appendMessage(const QString &text, bool incoming);

    itl::CommunicatorClient *m_client = nullptr;
    QString m_peer;
    QString m_displayName;

    QPlainTextEdit *m_view = nullptr;
    QLineEdit *m_input = nullptr;
};
