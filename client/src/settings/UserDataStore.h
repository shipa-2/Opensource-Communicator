#pragma once

#include <QHash>
#include <QList>
#include <QObject>
#include <QString>

class QSettings;

namespace itl {

struct CallHistoryEntry {
    QString peer;
    QString displayName;
    QString direction;
    qint64 startedAtMs = 0;
    qint64 connectedAtMs = 0;
    qint64 endedAtMs = 0;
    int durationSec = 0;
    bool answered = false;
    QString result;
};

class UserDataStore : public QObject {
    Q_OBJECT

public:
    explicit UserDataStore(QObject *parent = nullptr);

    void load();
    void save() const;
    void migrateFromSettings(QSettings &settings);

    QString noteForPeer(const QString &peer) const;
    void setNoteForPeer(const QString &peer, const QString &note);

    qint64 recentCallTime(const QString &peer) const;
    void recordRecentCall(const QString &peer);

    void addCallHistoryEntry(const CallHistoryEntry &entry);
    QList<CallHistoryEntry> callHistory() const;

    static QString cacheFilePath();

private:
    void ensureLoaded() const;

    mutable bool m_loaded = false;
    QHash<QString, QString> m_notes;
    QHash<QString, qint64> m_recentCalls;
    QList<CallHistoryEntry> m_callHistory;
};

} // namespace itl
