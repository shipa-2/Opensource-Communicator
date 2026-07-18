#pragma once

#include <QObject>
#include <QString>

namespace itl {

class UserSession;

class ConferenceManager : public QObject {
    Q_OBJECT

public:
    explicit ConferenceManager(QObject *parent = nullptr);

    void handleCommand(UserSession *session, int requestId,
                       const QString &command, const QJsonObject &payload);
};

} // namespace itl
