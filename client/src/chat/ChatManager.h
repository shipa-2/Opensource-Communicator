#pragma once

#include "protocol/ProtocolTypes.h"

#include <QDateTime>
#include <QHash>
#include <QObject>
#include <QString>

namespace itl {

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

class ChatManager : public QObject {
    Q_OBJECT

public:
    explicit ChatManager(WsApiClient *api, QObject *parent = nullptr);

    void setDomain(const QString &domain);

    void handlePayload(const QJsonObject &payload);
    void handleResponse(int requestId, const QJsonObject &response);

    bool isSmsPeer(const QString &peer) const;
    int sendMessage(const QString &peer, const QString &text);
    void setDemoMode(bool enabled);
    void clearDemoMessages();
    void addDemoMessage(const QString &peer, const QString &text, bool incoming);
    void sendSeen(const QString &peer, const QStringList &origIds);
    void loadHistory(const QString &lastKnownId = {});

    QList<InstantMessage> messagesForPeer(const QString &peer) const;

signals:
    void messageReceived(const itl::InstantMessage &message);
    void typingReceived(const QString &peer);
    void historyLoaded(const QString &peer);

private:
    InstantMessage parseMessage(const QJsonObject &msg) const;
    QString normalizePeer(QString peer) const;
    static bool looksLikePhone(QString value);
    static bool isPhonePeer(QString peer);
    static QString phoneFromPeer(QString peer);
    void loadSmsTelnums();
    void handleSmsTelnumsResponse(const QJsonObject &response);

    WsApiClient *m_api = nullptr;
    QString m_domain;
    QString m_smsFromNumber;
    QHash<QString, QList<InstantMessage>> m_messages;
    QHash<QString, QStringList> m_unreadByPeer;
    int m_historyRequestId = -1;
    int m_smsTelnumsRequestId = -1;
    bool m_demoMode = false;
};

} // namespace itl
