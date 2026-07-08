#pragma once

#include <QHash>
#include <QMainWindow>
#include <QString>

class PresenceSelector;
class QLabel;
class QButtonGroup;
class QLineEdit;
class QListWidget;
class QListWidgetItem;
class QPushButton;
class QTabWidget;
class QJsonObject;

class CallWindow;
class ChatDialog;
class ContactRowWidget;
class ProfileAvatarWidget;

namespace itl {
class CommunicatorClient;
class CallManager;
}

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(itl::CommunicatorClient *client, itl::CallManager *calls, QWidget *parent = nullptr);
    void refreshTheme();

private slots:
    void onLogin();
    void onLogout();
    void onSettings();
    void onHelp();
    void onAddContact();
    void onConference();
    void onDial();
    void onCallFromRow(const QString &peer);
    void onChatFromRow(const QString &peer);
    void onNotesFromRow(const QString &peer);
    void onHangup();
    void onHold();
    void onTransfer(const QString &target);
    void onAnswer();
    void onPresenceChanged(int index);
    void onSearchChanged(const QString &text);
    void onContactSelected();
    void onStatusMessage(const QString &message);
    void onContactUpdated(const QString &peer, const QString &name, const QString &presence);
    void onCallEvent(const QString &leg, const QString &what, const QJsonObject &payload);
    void onCallStateChanged(const QString &leg, const QString &state, const QString &detail);
    void onContactsLoaded(const QJsonObject &contacts);
    void onFilterChanged(int id);
    void onCallNotesChanged(const QString &peer, const QString &text);
    void onProfileAvatarChanged();

private:
    enum class ContactSortMode { All, Recent, External };

    struct ContactEntry {
        QString name;
        QString ext;
        QString presence;
        QString login;
        QString phone;
        bool isSelf = false;
        bool isCustom = false;
    };

    void setOnlineUi(bool online);
    void rebuildContactList();
    void addOrUpdateContactRow(const QString &peer);
    void updateSelfHeader();
    void mergeCustomContacts();
    QString selectedPeer() const;
    QString resolvePeer(QString input) const;
    QString displayNameForPeer(const QString &peer) const;
    QString detailForPeer(const QString &peer) const;
    bool matchesSearch(const QString &peer) const;
    bool matchesFilterMode(const QString &peer) const;
    bool isSamePeer(const QString &a, const QString &b) const;
    ContactRowWidget *rowWidgetForPeer(const QString &peer) const;

    void loadCallNotes(const QString &peer);
    void recordCallForPeer(const QString &peer);
    void rebuildHistoryList();
    void updateFilterButtonStyles();
    void beginCallTracking(const QString &leg, const QString &peer, const QString &displayName, bool incoming);
    void markCallConnected(const QString &leg);
    void finalizeCallHistory(const QString &leg, const QString &state);
    static QString formatHistoryDuration(int seconds);
    static QString formatHistoryTime(qint64 ms);

    struct CallTracking {
        QString peer;
        QString displayName;
        bool incoming = false;
        qint64 startedAtMs = 0;
        qint64 connectedAtMs = 0;
    };

    itl::CommunicatorClient *m_client = nullptr;
    itl::CallManager *m_calls = nullptr;
    CallWindow *m_callWindow = nullptr;
    ChatDialog *m_chatDialog = nullptr;

    QLabel *m_headerName = nullptr;
    ProfileAvatarWidget *m_headerAvatar = nullptr;
    PresenceSelector *m_presenceSelector = nullptr;
    QTabWidget *m_tabs = nullptr;
    QListWidget *m_contactsList = nullptr;
    QListWidget *m_historyList = nullptr;
    QLineEdit *m_searchEdit = nullptr;
    QLineEdit *m_dialInput = nullptr;
    QPushButton *m_dialCallBtn = nullptr;
    QButtonGroup *m_filterGroup = nullptr;

    QHash<QString, ContactEntry> m_contacts;
    QHash<QString, QListWidgetItem *> m_contactItems;
    QString m_selfPeer;
    QString m_selfName;
    QString m_activeIncomingLeg;
    QString m_activeLeg;
    bool m_onHold = false;
    bool m_online = false;
    ContactSortMode m_sortMode = ContactSortMode::All;
    QHash<QString, CallTracking> m_callTracking;
};
