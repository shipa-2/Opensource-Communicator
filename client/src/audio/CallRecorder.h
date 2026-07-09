#pragma once

#include <QByteArray>
#include <QObject>
#include <QString>

namespace itl {

class AppSettings;

class CallRecorder : public QObject {
    Q_OBJECT

public:
    explicit CallRecorder(QObject *parent = nullptr);

    bool isActive() const { return m_active; }
    QString lastSavedPath() const { return m_lastSavedPath; }

    bool start(const AppSettings *settings, const QString &contactName);
    void stop();

    void appendLocal(const QByteArray &pcm);
    void appendRemote(const QByteArray &pcm);

signals:
    void recordingFinished(const QString &path);

private:
    void flushMixedFrame(const QByteArray &local, const QByteArray &remote);
    static QByteArray silenceFrame();

    bool m_active = false;
    bool m_dualTrack = false;
    QString m_lastSavedPath;

    void *m_managerWriter = nullptr;
    void *m_callerWriter = nullptr;
    void *m_mixedWriter = nullptr;
};

} // namespace itl
