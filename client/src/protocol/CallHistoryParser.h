#pragma once

#include "settings/UserDataStore.h"

#include <QJsonObject>
#include <QList>
#include <QString>

namespace itl {

struct CallHistoryParseContext {
    QString domain;
    QString selfPeer;
    QString selfLogin;
};

QList<CallHistoryEntry> parseServerCallHistory(const QJsonValue &callsValue,
                                               const CallHistoryParseContext &context);

bool isInternalPeer(const QString &peer, const QString &domain = {});

bool historyEntryIsInternal(const CallHistoryEntry &entry, const QString &domain);

} // namespace itl
