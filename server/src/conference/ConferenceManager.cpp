#include "ConferenceManager.h"
#include "../network/WsSession.h"
#include "../session/UserSession.h"

#include <QLoggingCategory>

Q_LOGGING_CATEGORY(lcConf, "server.conference")

namespace itl {

ConferenceManager::ConferenceManager(QObject *parent)
    : QObject(parent)
{
}

void ConferenceManager::handleCommand(UserSession *session, int requestId,
                                      const QString &command, const QJsonObject &)
{
    qCInfo(lcConf) << "Conference command (stub):" << command << "from" << session->login();
    QJsonObject resp;
    resp.insert(QStringLiteral("response"), QStringLiteral("ok"));
    session->wsSession()->sendResponse(requestId, resp);
}

} // namespace itl
