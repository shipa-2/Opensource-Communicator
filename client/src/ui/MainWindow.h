#pragma once

#include "ContactRowWidget.h"

#include <QHash>
#include <QMainWindow>
#include <QString>
#include <QVector>

namespace itl {
struct CallHistoryEntry;
}

class PresenceSelector;
class QLabel;
class QButtonGroup;
class QDragEnterEvent;
class QDropEvent;
class QShowEvent;
class QLineEdit;
class QListWidget;
class QListWidgetItem;
class QMimeData;
class QPushButton;
class QTabWidget;
class QJsonObject;
class QAction;
class QMenu;
class QTimer;

class CallWindow;
class ChatDialog;
class ContactRowWidget;
class DialKeypadWidget;
class ProfileAvatarWidget;

namespace itl {
class CommunicatorClient;
class CallManager;
class MessageNotifyPlayer;
}

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(itl::CommunicatorClient *client, itl::CallManager *calls, QWidget *parent = nullptr);
    void refreshTheme();
    void handleIncomingTelUri(const QString &uri);

protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void changeEvent(QEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
    void onLogin();
    void onLogout();
    void startSession();
    void beginSessionWithCurrentCredentials();
    void onSettings();
    void onHelp();
    void onAddContact();
    void onImportContacts();
    void onConference();
    void onDial();
    void onCallFromRow(const QString &peer);
    void onChatFromRow(const QString &peer);
    void onIncomingChatMessage(const QString &peer, const QString &text, bool incoming, const QDateTime &timestamp);
    void onNotesFromRow(const QString &peer);
    void onHistoryItemActivated(QListWidgetItem *item);
    void onDeleteContactFromRow(const QString &peer);
    void onExportContactFromRow(const QString &peer);
    void onHangup();
    void onHold();
    void onTransfer();
    void onAnswer();
    void onPresenceChanged(int index);
    void onSearchChanged(const QString &text);
    void onContactSelected();
    void onStatusMessage(const QString &message);
    void onContactUpdated(const QString &peer, const QString &name, const QString &presence);
    void onCallEvent(const QString &leg, const QString &what, const QJsonObject &payload);
    void onCallStateChanged(const QString &leg, const QString &state, const QString &detail);
    void onContactsLoaded(const QJsonObject &contacts);
    void onAddressBookChanged();
    void onServerHistoryLoaded(int requestId, const QJsonObject &response);
    void onFilterChanged(int id);
    void onHistoryDirChanged(int id);
    void onHistoryScopeChanged(int id);
    void onHistoryPeriodClicked();
    void onHistorySearchChanged(const QString &text);
    void onCallNotesChanged(const QString &peer, const QString &text);
    void onProfileAvatarChanged();

private:
    enum class ContactSortMode { All, Recent, External };
    enum class HistoryDir { All, Incoming, Missed, Outgoing };
    enum class HistoryPeriod { Today, Week, Month, AllTime };
    enum class HistoryScope { Mine, Company, Internal };

    struct ContactEntry {
        QString name;
        QString ext;
        QString presence;
        QString login;
        QString phone;
        QString personalPhone;
        bool isSelf = false;
        bool isCustom = false;
    };

    void setOnlineUi(bool online);
    void rebuildContactList();
    void addOrUpdateContactRow(const QString &peer);
    QVector<ContactRowWidget::CallNumber> callNumbersForPeer(const QString &peer) const;
    void updateSelfHeader();
    void mergeCustomContacts();
    bool useServerContacts() const;
    void setupDragDrop();
    void registerDropTarget(QWidget *widget);
    bool canAcceptDrag(const QMimeData *mimeData) const;
    bool handleDroppedMimeData(const QMimeData *mimeData, bool notify);
    void applyTelUriToDial(const QString &raw);
    int importContactsFromPath(const QString &path, bool notify, bool fromDrop = false);
    int importContactsFromText(const QString &text, bool isVcard, bool notify, bool fromDrop = false);
    int addImportedContact(const QString &name, const QString &phone, const QString &ext);
    bool shouldInterceptTelPaste(QObject *focusWidget) const;
    bool isTelUri(const QString &text) const;
    QString selectedPeer() const;
    QString resolvePeer(QString input) const;
    QString displayNameForPeer(const QString &peer) const;
    QString detailForPeer(const QString &peer) const;
    bool matchesSearch(const QString &peer) const;
    bool matchesFilterMode(const QString &peer) const;
    bool isSamePeer(const QString &a, const QString &b) const;
    ContactRowWidget *rowWidgetForPeer(const QString &peer) const;

    QString recordingNameForPeer(const QString &peer, const QString &fallbackDisplayName = {}) const;
    void loadCallNotes(const QString &peer);
    void recordCallForPeer(const QString &peer);
    void rebuildHistoryList();
    void refreshServerHistory();
    void prefetchCompanyHistory();
    void prefetchInternalHistory();
    void runHistorySelfTest();
    QJsonObject buildHistoryRequest(HistoryScope scope) const;
    QList<itl::CallHistoryEntry> currentHistoryEntries() const;
    bool historyEntryMatches(const itl::CallHistoryEntry &entry) const;
    void updateHistoryPeriodLabel();
    void applyLinkButtonStyle(QPushButton *button) const;
    void updateFilterButtonStyles();
    void updateHistoryButtonStyles();
    void updateDialCallButtonStyle();
    void applyContactViewSettings();
    void updateUnreadIndicators();
    bool shouldNotifyForChatMessage(const QString &peer) const;
    void enterDemoInterface();
    void exitDemoInterface();
    void stopDemoCallSimulation();
    void startDemoVoiceSimulation();
    void startDemoCallSimulation(const QString &peer, const QString &displayName, const QString &detail);
    void beginCallTracking(const QString &leg, const QString &peer, const QString &displayName, bool incoming);
    void markCallConnected(const QString &leg);
    void finalizeCallHistory(const QString &leg, const QString &state, const QString &transferTo = {});
    void resumeExternalMediaIfIdle();
    static QString formatHistoryDuration(int seconds);
    static QString formatHistoryTime(qint64 ms);
    static QString formatHistoryWhen(qint64 ms);

    struct CallTracking {
        QString peer;
        QString displayName;
        bool incoming = false;
        qint64 startedAtMs = 0;
        qint64 connectedAtMs = 0;
    };

    itl::CommunicatorClient *m_client = nullptr;
    itl::CallManager *m_calls = nullptr;
    itl::MessageNotifyPlayer *m_messageNotify = nullptr;
    CallWindow *m_callWindow = nullptr;
    ChatDialog *m_chatDialog = nullptr;
    QMenu *m_viewMenu = nullptr;
    QAction *m_viewChatAction = nullptr;
    QAction *m_viewCallAction = nullptr;

    QLabel *m_headerName = nullptr;
    ProfileAvatarWidget *m_headerAvatar = nullptr;
    PresenceSelector *m_presenceSelector = nullptr;
    QTabWidget *m_tabs = nullptr;
    QWidget *m_dialPage = nullptr;
    QListWidget *m_contactsList = nullptr;
    QListWidget *m_historyList = nullptr;
    QLineEdit *m_searchEdit = nullptr;
    QLineEdit *m_dialInput = nullptr;
    DialKeypadWidget *m_dialKeypad = nullptr;
    QLineEdit *m_historySearchEdit = nullptr;
    QPushButton *m_dialCallBtn = nullptr;
    QPushButton *m_historyPeriodBtn = nullptr;
    QButtonGroup *m_filterGroup = nullptr;
    QButtonGroup *m_historyDirGroup = nullptr;
    QButtonGroup *m_historyScopeGroup = nullptr;

    QHash<QString, ContactEntry> m_contacts;
    QHash<QString, QListWidgetItem *> m_contactItems;
    QString m_selfPeer;
    QString m_selfName;
    QString m_activeIncomingLeg;
    QString m_activeLeg;
    bool m_onHold = false;
    bool m_online = false;
    bool m_demoMode = false;
    ContactSortMode m_sortMode = ContactSortMode::All;
    HistoryDir m_historyDir = HistoryDir::All;
    HistoryPeriod m_historyPeriod = HistoryPeriod::Week;
    HistoryScope m_historyScope = HistoryScope::Mine;
    QString m_historySearch;
    QHash<QString, CallTracking> m_callTracking;
    QString m_demoCallLeg;
    QTimer *m_demoVoiceTimer = nullptr;
    bool m_demoVoiceActive = false;
    QList<itl::CallHistoryEntry> m_demoCallHistory;
    QList<itl::CallHistoryEntry> m_serverHistory;
    QList<itl::CallHistoryEntry> m_companyHistory;
    QList<itl::CallHistoryEntry> m_internalHistory;
    int m_historyRequestId = -1;
    int m_companyHistoryRequestId = -1;
    int m_internalHistoryRequestId = -1;
    HistoryScope m_historyRequestScope = HistoryScope::Mine;
    bool m_historyLoading = false;
    bool m_companyHistoryLoading = false;
    bool m_internalHistoryLoading = false;
};
