#pragma once

#include <QDateTime>
#include <QDialog>

class QFrame;
class QLineEdit;
class QPushButton;
class QTextBrowser;
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
    void updatePeerDisplayName(const QString &peerDisplayName);

    void refreshAppearance();

protected:
    void showEvent(QShowEvent *event) override;

private slots:
    void onSend();
    void onAttachFile();
    void onChatMessage(const QString &peer, const QString &text, bool incoming, const QDateTime &timestamp);
    void onHistoryLoaded(const QString &peer);
    void onAnchorActivated(const QUrl &url);

private:
    void reloadMessages();
    void applyThemeFromPreview(const QString &key);
    void saveChatFile(const QString &key);
    QString buildMessageHtml(const itl::InstantMessage &im) const;
    QString buildPlainMessageHtml(const QString &text, bool incoming, const QDateTime &timestamp) const;
    QString buildThemeShareNoticeHtml(const QString &noticeBody, bool incoming,
                                      const QDateTime &timestamp) const;
    QString buildFileShareNoticeHtml(const QString &noticeBody, bool incoming,
                                     const QDateTime &timestamp) const;
    QString buildThemeAppliedNoticeHtml(bool incoming, const QDateTime &timestamp) const;
    void openThemePreview(const QString &key);
    void refreshViewChrome();
    void updateAttachEnabled();
    static QString htmlEscape(const QString &text);
    static QString linkifyHtml(const QString &text);
    static QString shortDisplayName(const QString &fullName, const QString &fallback);
    static QString formatTimestamp(const QDateTime &timestamp);

    itl::CommunicatorClient *m_client = nullptr;
    QString m_peer;
    QString m_peerDisplayName;
    QString m_selfDisplayName;

    QFrame *m_viewFrame = nullptr;
    QTextBrowser *m_view = nullptr;
    QPushButton *m_attachBtn = nullptr;
    QLineEdit *m_input = nullptr;
};
