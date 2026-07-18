#pragma once

#include <QObject>

namespace itl {

class UserSession;

class SmsManager : public QObject {
    Q_OBJECT

public:
    explicit SmsManager(QObject *parent = nullptr);

    void handleGetSmsChannels(UserSession *session, int requestId, const QJsonObject &payload);
    void handleGetSms(UserSession *session, int requestId, const QJsonObject &payload);
    void handleSendSms(UserSession *session, int requestId, const QJsonObject &payload);
};

} // namespace itl
