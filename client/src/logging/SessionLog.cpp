#include "logging/SessionLog.h"

#include <QApplication>
#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QSysInfo>
#include <QTextStream>
#include <QtGlobal>

#include <cstdio>
#include <deque>
#include <mutex>

namespace itl {
namespace {

struct Entry {
    QString stamp;
    QString level;
    QString category;
    QString message;
};

constexpr int kMaxEntries = 12000;
constexpr int kMaxMessageLength = 4000;

std::mutex g_mutex;
std::deque<Entry> g_entries;
QtMessageHandler g_defaultHandler = nullptr;

QString levelName(QtMsgType type)
{
    switch (type) {
    case QtDebugMsg: return QStringLiteral("debug");
    case QtInfoMsg: return QStringLiteral("info");
    case QtWarningMsg: return QStringLiteral("warning");
    case QtCriticalMsg: return QStringLiteral("critical");
    case QtFatalMsg: return QStringLiteral("fatal");
    }
    return QStringLiteral("info");
}

bool scopeMatches(SessionLog::Scope scope, const QString &category)
{
    if (scope == SessionLog::Scope::All) {
        return true;
    }
    if (scope == SessionLog::Scope::Media) {
        return category == QStringLiteral("rtc") || category.startsWith(QStringLiteral("itl.call"))
            || category.startsWith(QStringLiteral("itl.audio")) || category == QStringLiteral("itl.media")
            || category.startsWith(QStringLiteral("itl.record"));
    }
    return category.startsWith(QStringLiteral("itl.ws")) || category.startsWith(QStringLiteral("itl.client"));
}

void appendEntry(const QString &level, const QString &category, const QString &message)
{
    Entry entry;
    entry.stamp = QDateTime::currentDateTime().toString(QStringLiteral("dd.MM.yyyy hh:mm:ss.zzz"));
    entry.level = level;
    entry.category = category;
    entry.message = message.size() > kMaxMessageLength ? message.left(kMaxMessageLength) : message;

    std::lock_guard<std::mutex> lock(g_mutex);
    g_entries.push_back(std::move(entry));
    while (g_entries.size() > kMaxEntries) {
        g_entries.pop_front();
    }
}

void qtMessageHandler(QtMsgType type, const QMessageLogContext &context, const QString &message)
{
    appendEntry(levelName(type), QString::fromLatin1(context.category ? context.category : "default"),
                message);
    if (g_defaultHandler) {
        g_defaultHandler(type, context, message);
    }
}

QString scopeTitle(SessionLog::Scope scope)
{
    switch (scope) {
    case SessionLog::Scope::Media: return QStringLiteral("calls and media (itl.call, itl.audio, itl.media, itl.record, rtc)");
    case SessionLog::Scope::Network: return QStringLiteral("network and login (itl.ws, itl.client)");
    case SessionLog::Scope::All: break;
    }
    return QStringLiteral("all subsystems");
}

} // namespace

void SessionLog::install()
{
    g_defaultHandler = qInstallMessageHandler(qtMessageHandler);
}

void SessionLog::addRtcLine(int severity, const char *message)
{
    if (!message) {
        return;
    }
    // rtc::LogLevel: None=0 Verbose=1 Debug=2 Info=3 Warning=4 Error=5 Fatal=6
    const QString level = severity >= 6 ? QStringLiteral("fatal")
        : severity == 5                 ? QStringLiteral("critical")
        : severity == 4                 ? QStringLiteral("warning")
        : severity <= 2                 ? QStringLiteral("debug")
                                        : QStringLiteral("info");
    appendEntry(level, QStringLiteral("rtc"), QString::fromUtf8(message));
    // libdatachannel's default logger wrote to stderr; keep that behavior.
    std::fprintf(stderr, "%s\n", message);
}

bool SessionLog::saveLog(Scope scope, const QString &path, QString *error)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        if (error) {
            *error = file.errorString();
        }
        return false;
    }

    std::deque<Entry> snapshot;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        snapshot = g_entries;
    }

    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8);
    stream << "# OpenSource Communicator " << QApplication::applicationVersion() << "\n";
    stream << "# Qt " << qVersion() << " | " << QSysInfo::prettyProductName() << "\n";
    stream << "# Scope: " << scopeTitle(scope) << "\n";
    stream << "# Exported: " << QDateTime::currentDateTime().toString(QStringLiteral("dd.MM.yyyy hh:mm:ss"))
           << "\n";
    stream << "# Entries: " << snapshot.size() << "\n\n";

    int matched = 0;
    for (const Entry &entry : snapshot) {
        if (!scopeMatches(scope, entry.category)) {
            continue;
        }
        ++matched;
        stream << entry.stamp << ' ' << entry.level << ' ' << entry.category << ": " << entry.message
               << '\n';
    }

    if (matched == 0) {
        stream << "(no matching entries captured)\n";
    }
    if (!file.flush() && error) {
        *error = file.errorString();
        return false;
    }
    return true;
}

} // namespace itl
