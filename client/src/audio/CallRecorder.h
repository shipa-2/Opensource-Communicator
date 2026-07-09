#pragma once

#include <QByteArray>
#include <QObject>
#include <QString>
#include <QStringList>

class QProcess;

namespace itl {

class AppSettings;

class CallRecorder : public QObject {
    Q_OBJECT

public:
    explicit CallRecorder(QObject *parent = nullptr);
    ~CallRecorder() override;

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
    void enqueueAndMix(bool localSide, const QByteArray &pcm);
    void drainMixedQueues(bool padWithSilence);
    void closeWriters();
    void beginMp3Conversion(const QStringList &wavPaths);
    void convertNext();
    void finishConversion(bool success, const QString &message);
    static QByteArray silenceFrame();
    static QString findEncoder();

    bool m_active = false;
    bool m_dualTrack = false;
    QString m_lastSavedPath;
    QString m_pendingMixedPath;
    QStringList m_pendingWavPaths;
    int m_convertIndex = 0;
    QProcess *m_convertProcess = nullptr;
    QList<QByteArray> m_localQueue;
    QList<QByteArray> m_remoteQueue;

    void *m_managerWriter = nullptr;
    void *m_callerWriter = nullptr;
    void *m_mixedWriter = nullptr;
};

} // namespace itl
