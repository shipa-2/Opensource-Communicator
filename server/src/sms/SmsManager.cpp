#include "SmsManager.h"
#include "../network/WsSession.h"
#include "../session/UserSession.h"

#include <QJsonArray>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(lcSms, "server.sms")

namespace itl {

SmsManager::SmsManager(QObject *parent)
    : QObject(parent)
{
}

void SmsManager::handleGetSmsChannels(UserSession *session, int requestId, const QJsonObject &)
{
    QJsonObject resp;
    resp.insert(QStringLiteral("response"), QJsonArray());
    session->wsSession()->sendResponse(requestId, resp);
}

void SmsManager::handleGetSms(UserSession *session, int requestId, const QJsonObject &)
{
    QJsonObject resp;
    resp.insert(QStringLiteral("response"), QJsonArray());
    session->wsSession()->sendResponse(requestId, resp);
}

void SmsManager::handleSendSms(UserSession *session, int requestId, const QJsonObject &payload)
{
    qCInfo(lcSms) << "SendSms from" << session->login()
                  << "to" << payload.value(QStringLiteral("to")).toString();
    QJsonObject resp;
    resp.insert(QStringLiteral("response"), QStringLiteral("ok"));
    session->wsSession()->sendResponse(requestId, resp);
}

} // namespace itl
