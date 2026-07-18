#pragma once

#include <QObject>

namespace itl {

class UserSession;
class Database;

class HistoryManager : public QObject {
    Q_OBJECT

public:
    explicit HistoryManager(Database *db, QObject *parent = nullptr);

    void handleGetHistory(UserSession *session, int requestId, const QJsonObject &payload);
    void handleCreateOrUpdateNote(UserSession *session, int requestId, const QJsonObject &payload);
    void handleDeleteNote(UserSession *session, int requestId, const QJsonObject &payload);

private:
    Database *m_db;
};

} // namespace itl
