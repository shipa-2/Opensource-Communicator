#include "UserSession.h"

namespace itl {

UserSession::UserSession(const QString &sid, QObject *parent)
    : QObject(parent)
    , m_sid(sid)
    , m_lastActivityMs(QDateTime::currentMSecsSinceEpoch())
{
}

} // namespace itl
