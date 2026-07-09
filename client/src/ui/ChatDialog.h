#pragma once

#include <QDateTime>
#include <QDialog>

class QLineEdit;
class QPlainTextEdit;

namespace itl {
class CommunicatorClient;
struct InstantMessage;
}

class ChatDialog : public QDialog {
    Q_OBJECT

public:
    explicit ChatDialog(itl::CommunicatorClient *client, QWidget *parent = nullptr);

    void openForPeer(const QString &peer, const QString &peerDisplayName, const QString &selfDisplayName);

private slots:
    void onSend();
    void onChatMessage(const QString &peer, const QString &text, bool incoming, const QDateTime &timestamp);
    void onHistoryLoaded(const QString &peer);

private:
    void reloadMessages();
    void appendMessage(const itl::InstantMessage &im);
    void appendMessage(const QString &text, bool incoming, const QDateTime &timestamp);
    static QString shortDisplayName(const QString &fullName, const QString &fallback);
    static QString formatTimestamp(const QDateTime &timestamp);

    itl::CommunicatorClient *m_client = nullptr;
    QString m_peer;
    QString m_peerDisplayName;
    QString m_selfDisplayName;

    QPlainTextEdit *m_view = nullptr;
    QLineEdit *m_input = nullptr;
};
