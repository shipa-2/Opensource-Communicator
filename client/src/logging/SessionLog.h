#pragma once

#include <QString>

class QMessageLogContext;
enum QtMsgType;

namespace itl {

// In-memory capture of application log messages, installed once at startup.
// Backs the diagnostic export buttons in Settings → Информация. The last
// entries are kept in a bounded buffer; export filters them by subsystem.
class SessionLog {
public:
    enum class Scope { All, Media, Network };

    static void install();
    // Sink for libdatachannel's logger so media diagnostics land in the same buffer.
    static void addRtcLine(int severity, const char *message);
    static bool saveLog(Scope scope, const QString &path, QString *error = nullptr);
};

} // namespace itl
