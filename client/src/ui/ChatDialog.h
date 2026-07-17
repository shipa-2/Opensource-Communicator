#pragma once

#include <QDateTime>
#include <QDialog>

class QFrame;
class QLineEdit;
class QPlainTextEdit;
class QShowEvent;

namespace itl {
class CommunicatorClient;
struct InstantMessage;
}

class ChatDialog : public QDialog {
    Q_OBJECT

public:
    explicit ChatDialog(itl::CommunicatorClient *client, QWidget *parent = nullptr);

    void openForPeer(const QString &peer, const QString &peerDisplayName, const QString &selfDisplayName);
    bool isOpenForPeer(const QString &peer) const;

    void refreshAppearance();

protected:
    void showEvent(QShowEvent *event) override;

private slots:
    void onSend();
    void onChatMessage(const QString &peer, const QString &text, bool incoming, const QDateTime &timestamp);
    void onHistoryLoaded(const QString &peer);

private:
    void reloadMessages();
    void appendMessage(const itl::InstantMessage &im);
    void appendMessage(const QString &text, bool incoming, const QDateTime &timestamp);
    void refreshViewChrome();
    static QString shortDisplayName(const QString &fullName, const QString &fallback);
    static QString formatTimestamp(const QDateTime &timestamp);

    itl::CommunicatorClient *m_client = nullptr;
    QString m_peer;
    QString m_peerDisplayName;
    QString m_selfDisplayName;

    QFrame *m_viewFrame = nullptr;
    QPlainTextEdit *m_view = nullptr;
    QLineEdit *m_input = nullptr;
};
