#pragma once

#include "protocol/ProtocolTypes.h"

#include <QDateTime>
#include <QHash>
#include <QObject>
#include <QPixmap>
#include <QSet>
#include <QString>

namespace itl {

class UserDataStore;
class WsApiClient;

struct InstantMessage {
    QString id;
    QString origId;
    QString peer;
    QString body;
    bool incoming = false;
    bool seen = false;
    QDateTime timestamp;
};

struct ThemeSharePayload {
    QPixmap wallpaper;
    int uiOpacity = 85;
    int listOpacity = 85;
};

struct ChatFilePayload {
    QString fileName;
    QByteArray data;
};

class ChatManager : public QObject {
    Q_OBJECT

public:
    explicit ChatManager(WsApiClient *api, QObject *parent = nullptr);

    void setDomain(const QString &domain);
    void setSelfLogin(const QString &login);
    void setUserDataStore(UserDataStore *store);
    /// Clear all runtime data that belongs to the previous login/server.
    void resetSessionState();
    void loadStoredPeerColors();
    void loadStoredPeerAvatars();
    void loadStoredOscPeers();

    void handlePayload(const QJsonObject &payload);
    void handleResponse(int requestId, const QJsonObject &response);

    bool isSmsPeer(const QString &peer) const;
    QString normalizedPeer(const QString &peer) const;
    int sendMessage(const QString &peer, const QString &text);
    /// Domain contacts that should receive Openping! on login (not yet confirmed OSC).
    void setOpenpingCandidates(const QStringList &peers);
    void sendOpenpingBroadcast();
    QStringList oscPeers() const;
    bool isOscPeer(const QString &peer) const;
    /// Register a newly discovered OSC peer (Openping / demo simulation).
    bool discoverOscPeer(const QString &peer);
    void seedDemoOscPeers(const QStringList &peers);
    /// Demo: peer shares a theme with the local user (incoming chat notice + preview).
    void demoIncomingThemeShare(const QString &peer, const QPixmap &wallpaper, int uiOpacity, int listOpacity);
    /// Demo: peer sends a file attachment into chat.
    void demoIncomingFileShare(const QString &peer, const QString &fileName, const QByteArray &data);
    /// Color + optional photo used when replying to Openping! / advertising.
    void setSelfShareProfile(const QString &color, const QPixmap &avatarPhoto = {});
    void sendColorAdvertisement(const QString &color);
    /// Share a contact-list / call avatar (scaled to 140×140 PNG) via IM footer — OSC peers only.
    /// Clients downscale for the contact list (~32–36px) and use full size in the call window.
    bool sendAvatarShare(const QString &peer, const QPixmap &pixmap);
    /// Share wallpaper (390×620 JPEG) + dimming sliders via IM — OSC peers only.
    bool sendThemeShare(const QString &peer, const QPixmap &wallpaper, int uiOpacity, int listOpacity);
    /// Acknowledge applied theme back to the sharer (OSC peers only).
    bool sendThemeApplied(const QString &peer);
    /// Share profile color to one OSC peer (when no photo is set).
    bool sendColorShare(const QString &peer);
    /// Share a file in chat (`**fnm=…;enc=b64;cnt=…**`, persist=true).
    bool sendFileShare(const QString &peer, const QString &filePath);
    QString peerColor(const QString &peer) const;
    QPixmap peerAvatar(const QString &peer) const;
    static bool isColorAdvertisement(const QString &body);
    static QString extractColor(const QString &body);
    static bool isFileTransfer(const QString &body);
    static bool isAvatarFileTransfer(const QString &body);
    static bool isChatFileTransfer(const QString &body);
    static bool isThemeShare(const QString &body);
    static bool isThemeShareNotice(const QString &body);
    static QString themeShareNoticeKey(const QString &body);
    static QString themeShareNoticeBody(const QString &key);
    static bool isThemeApplied(const QString &body);
    static bool isThemeAppliedNotice(const QString &body);
    static QString themeAppliedNoticeBody();
    ThemeSharePayload themeShareOffer(const QString &key) const;
    static bool isFileShareNotice(const QString &body);
    static QString fileShareNoticeKey(const QString &body);
    static QString fileShareNoticeBody(const QString &key);
    ChatFilePayload chatFileOffer(const QString &key) const;
    static bool isOpenping(const QString &body);
    static QString extractFileTransferName(const QString &body);
    static QByteArray extractFileTransferData(const QString &body);
    void setDemoMode(bool enabled);
    void clearDemoMessages();
    void addDemoMessage(const QString &peer, const QString &text, bool incoming, bool notify = true);
    void setDemoPeerColor(const QString &peer, const QString &color);
    void sendSeen(const QString &peer, const QStringList &origIds);
    void loadHistory(const QString &lastKnownId = {});

    QList<InstantMessage> messagesForPeer(const QString &peer) const;

    bool hasUnread(const QString &peer) const;
    void markPeerRead(const QString &peer);

signals:
    void messageReceived(const itl::InstantMessage &message);
    void typingReceived(const QString &peer);
    void historyLoaded(const QString &peer);
    void unreadChanged(const QString &peer);
    void peerColorReceived(const QString &peer, const QString &color);
    void peerAvatarReceived(const QString &peer, const QPixmap &avatar);
    void oscPeersChanged();
    /// Emitted once when a contact is newly confirmed as OpenSource Communicator.
    void oscPeerDiscovered(const QString &peer);
    /// Demo only: outbound «Теперь ты …» renames the contact in the UI.
    void demoPeerRenameRequested(const QString &peer, const QString &newName);

private:
    InstantMessage parseMessage(const QJsonObject &msg) const;
    QString normalizePeer(QString peer) const;
    QString canonicalPeer(QString peer) const;
    QString imColorAdPeer(const QJsonObject &payload, const QJsonObject &msg) const;
    static QString messageBody(const QJsonObject &msg);
    static bool isEphemeralColorAdvertisement(const QJsonObject &msg, const QString &body);
    static bool isEphemeralFileTransfer(const QJsonObject &msg, const QString &body);
    void storePeerColor(const QString &peer, const QString &color);
    void storePeerAvatar(const QString &peer, const QPixmap &avatar, const QString &base64Png = {});
    bool ingestFileTransfer(const QString &peer, const QString &body);
    void handleIncomingOpenping(const QString &peer);
    void handleIncomingThemeApplied(const QString &peer, const QString &msgId = {},
                                    const QString &origId = {});
    bool addOscPeer(const QString &peer);
    void sendOpenpingTo(const QString &peer);
    void sendColorToPeer(const QString &peer, const QString &color);
    void sendAvatarToPeer(const QString &peer, const QPixmap &pixmap);
    void sendSelfPresenceTo(const QString &peer);
    bool recentlySentOpenping(const QString &peer) const;
    void markOpenpingSent(const QString &peer);
    bool storeMessage(const InstantMessage &im, bool replaceOptimisticOutgoing);
    static bool looksLikePhone(QString value);
    static bool isPhonePeer(QString peer);
    static QString phoneFromPeer(QString peer);
    static QPixmap scaleAvatarForShare(const QPixmap &pixmap);
    static QString encodeAvatarShareBody(const QPixmap &scaledPngPixmap, QByteArray *outPng = nullptr);
    static QString encodeChatFileBody(const QString &fileName, const QByteArray &data);
    static QString encodeThemeShareBody(const QPixmap &wallpaper, int uiOpacity, int listOpacity,
                                        QByteArray *outJpeg = nullptr);
    QString registerThemeShare(const QString &peer, const ThemeSharePayload &payload, const QString &msgId = {});
    QString registerChatFile(const QString &peer, const QString &body, const QString &msgId = {});
    bool parseThemeShareBody(const QString &body, ThemeSharePayload *out) const;
    void loadSmsTelnums();
    void handleSmsTelnumsResponse(const QJsonObject &response);

    WsApiClient *m_api = nullptr;
    UserDataStore *m_userData = nullptr;
    QString m_domain;
    QString m_selfLogin;
    QString m_smsFromNumber;
    QString m_lastAdvertisedColor;
    QPixmap m_selfShareAvatar;
    QHash<QString, qint64> m_openpingSentAt;
    QHash<QString, QList<InstantMessage>> m_messages;
    QHash<QString, QStringList> m_unreadByPeer;
    QHash<QString, QString> m_peerColors;
    QHash<QString, QPixmap> m_peerAvatars;
    QHash<QString, ThemeSharePayload> m_themeShares;
    QHash<QString, ChatFilePayload> m_chatFiles;
    QStringList m_openpingCandidates;
    QSet<QString> m_oscPeers;
    int m_historyRequestId = -1;
    int m_smsTelnumsRequestId = -1;
    bool m_demoMode = false;
};

} // namespace itl
