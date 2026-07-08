#pragma once

#include "settings/UserDataStore.h"

#include <QString>
#include <QList>

namespace itl {
class ChatManager;
}

namespace itl::DemoData {

struct DemoContact {
    QString peer;
    QString name;
    QString ext;
    QString phone;
    QString presence;
    bool isSelf = false;
};

bool isDemoCredentials(const QString &login, const QString &password);
QString demoDomain();

QList<DemoContact> contacts();
QList<CallHistoryEntry> callHistory();
void seedChatMessages(ChatManager *chat);

} // namespace itl::DemoData
