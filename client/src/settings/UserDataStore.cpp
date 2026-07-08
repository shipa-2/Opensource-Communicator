#include "UserDataStore.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <QStandardPaths>
#include <QDateTime>

namespace {
constexpr int kMaxHistoryEntries = 500;
constexpr auto kLegacyNotes = "contacts/notes";
constexpr auto kLegacyRecent = "contacts/recentCalls";
} // namespace

namespace itl {

UserDataStore::UserDataStore(QObject *parent)
    : QObject(parent)
{
}

QString UserDataStore::cacheFilePath()
{
  const QString dir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
  QDir().mkpath(dir);
  return dir + QStringLiteral("/user-data.json");
}

void UserDataStore::ensureLoaded() const
{
  if (m_loaded) {
    return;
  }
  const_cast<UserDataStore *>(this)->load();
}

void UserDataStore::load()
{
  m_notes.clear();
  m_recentCalls.clear();
  m_callHistory.clear();

  QFile file(cacheFilePath());
  if (!file.open(QIODevice::ReadOnly)) {
    m_loaded = true;
    return;
  }

  const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
  const QJsonObject root = doc.object();

  const QJsonObject notes = root.value(QStringLiteral("notes")).toObject();
  for (auto it = notes.begin(); it != notes.end(); ++it) {
    m_notes.insert(it.key(), it.value().toString());
  }

  const QJsonObject recent = root.value(QStringLiteral("recentCalls")).toObject();
  for (auto it = recent.begin(); it != recent.end(); ++it) {
    m_recentCalls.insert(it.key(), static_cast<qint64>(it.value().toDouble()));
  }

  const QJsonArray history = root.value(QStringLiteral("callHistory")).toArray();
  for (const QJsonValue &value : history) {
    const QJsonObject obj = value.toObject();
    CallHistoryEntry entry;
    entry.peer = obj.value(QStringLiteral("peer")).toString();
    entry.displayName = obj.value(QStringLiteral("displayName")).toString();
    entry.direction = obj.value(QStringLiteral("direction")).toString();
    entry.startedAtMs = static_cast<qint64>(obj.value(QStringLiteral("startedAt")).toDouble());
    entry.connectedAtMs = static_cast<qint64>(obj.value(QStringLiteral("connectedAt")).toDouble());
    entry.endedAtMs = static_cast<qint64>(obj.value(QStringLiteral("endedAt")).toDouble());
    entry.durationSec = obj.value(QStringLiteral("durationSec")).toInt();
    entry.answered = obj.value(QStringLiteral("answered")).toBool();
    entry.result = obj.value(QStringLiteral("result")).toString();
    if (!entry.peer.isEmpty()) {
      m_callHistory.append(entry);
    }
  }

  m_loaded = true;
}

void UserDataStore::save() const
{
  ensureLoaded();

  QJsonObject notes;
  for (auto it = m_notes.cbegin(); it != m_notes.cend(); ++it) {
    notes.insert(it.key(), it.value());
  }

  QJsonObject recent;
  for (auto it = m_recentCalls.cbegin(); it != m_recentCalls.cend(); ++it) {
    recent.insert(it.key(), it.value());
  }

  QJsonArray history;
  for (const CallHistoryEntry &entry : m_callHistory) {
    history.append(QJsonObject{
        {QStringLiteral("peer"), entry.peer},
        {QStringLiteral("displayName"), entry.displayName},
        {QStringLiteral("direction"), entry.direction},
        {QStringLiteral("startedAt"), entry.startedAtMs},
        {QStringLiteral("connectedAt"), entry.connectedAtMs},
        {QStringLiteral("endedAt"), entry.endedAtMs},
        {QStringLiteral("durationSec"), entry.durationSec},
        {QStringLiteral("answered"), entry.answered},
        {QStringLiteral("result"), entry.result},
    });
  }

  const QJsonObject root{
      {QStringLiteral("version"), 1},
      {QStringLiteral("notes"), notes},
      {QStringLiteral("recentCalls"), recent},
      {QStringLiteral("callHistory"), history},
  };

  QFile file(cacheFilePath());
  if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
    return;
  }
  file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
}

void UserDataStore::migrateFromSettings(QSettings &settings)
{
  ensureLoaded();
  if (!m_notes.isEmpty() || !m_recentCalls.isEmpty() || !m_callHistory.isEmpty()) {
    return;
  }

  const QJsonObject legacyNotes = QJsonDocument::fromVariant(settings.value(QString::fromUtf8(kLegacyNotes))).object();
  for (auto it = legacyNotes.begin(); it != legacyNotes.end(); ++it) {
    m_notes.insert(it.key(), it.value().toString());
  }

  const QJsonObject legacyRecent = QJsonDocument::fromVariant(settings.value(QString::fromUtf8(kLegacyRecent))).object();
  for (auto it = legacyRecent.begin(); it != legacyRecent.end(); ++it) {
    m_recentCalls.insert(it.key(), static_cast<qint64>(it.value().toDouble()));
  }

  if (!m_notes.isEmpty() || !m_recentCalls.isEmpty()) {
    save();
  }
}

QString UserDataStore::noteForPeer(const QString &peer) const
{
  ensureLoaded();
  return m_notes.value(peer);
}

void UserDataStore::setNoteForPeer(const QString &peer, const QString &note)
{
  ensureLoaded();
  const QString trimmed = note.trimmed();
  if (trimmed.isEmpty()) {
    m_notes.remove(peer);
  } else {
    m_notes.insert(peer, trimmed);
  }
  save();
}

qint64 UserDataStore::recentCallTime(const QString &peer) const
{
  ensureLoaded();
  return m_recentCalls.value(peer, 0);
}

void UserDataStore::recordRecentCall(const QString &peer)
{
  if (peer.isEmpty()) {
    return;
  }
  ensureLoaded();
  m_recentCalls.insert(peer, QDateTime::currentMSecsSinceEpoch());
  save();
}

void UserDataStore::addCallHistoryEntry(const CallHistoryEntry &entry)
{
  ensureLoaded();
  m_callHistory.prepend(entry);
  while (m_callHistory.size() > kMaxHistoryEntries) {
    m_callHistory.removeLast();
  }
  save();
}

QList<CallHistoryEntry> UserDataStore::callHistory() const
{
  ensureLoaded();
  return m_callHistory;
}

} // namespace itl
