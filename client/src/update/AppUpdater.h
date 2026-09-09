#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QList>
#include <QObject>
#include <QString>

#include <QUrl>

class QNetworkAccessManager;

namespace itl {

class AppUpdater : public QObject {
    Q_OBJECT

public:
    enum class Phase {
        Idle,
        Checking,
        Downloading,
        Unpacking,
        Installing,
        Finalizing,
        Restarting,
        Done,
        Failed,
    };
    Q_ENUM(Phase)

    enum class Channel {
        Release,
        PreRelease,
    };
    Q_ENUM(Channel)

    struct ChannelOffer {
        QString tag;
        QString packageVersion;
        QString pageUrl;
        QString assetUrl;
        QString fileName;
        bool available = false;
        bool updateAvailable = false;
        bool localBuildAhead = false;
    };

    struct ScanResult {
        ChannelOffer release;
        ChannelOffer preRelease;
        bool onPreReleaseBuild = false;
    };

    explicit AppUpdater(QObject *parent = nullptr);

    static QList<int> parseVersion(const QString &raw);
    static QString baseVersion(const QString &appVersion);
    static bool isPreReleaseBuild(const QString &appVersion);
    static QString preReleaseStampFromVersion(const QString &appVersion);
    static QString preReleaseStampFromTag(const QString &tag);
    static QString packageVersionFromFileName(const QString &fileName);
    static bool isRemoteNewer(const QString &current, const QString &remoteTag);
    static bool isPreReleaseTagNewer(const QString &currentAppVersion, const QString &remoteTag);
    static ScanResult scanReleases(const QJsonArray &releases, const QString &currentAppVersion);
    static QString platformAssetToken();
    static QString findAssetUrl(const QJsonObject &releaseObject);
    static QString phaseTitle(Phase phase);

    void downloadAndInstall(const QUrl &url, const QString &fileName);

signals:
    void progressChanged(Phase phase, const QString &detail);
    void finished(bool success, const QString &message);

private:
    bool extractArchive(const QString &archivePath, const QString &destDir, QString *errorOut);
    bool applyPayload(const QString &payloadRoot, const QString &installRoot, QString *relaunchPathOut,
                      QString *errorOut);
    bool installRootWritable(const QString &installRoot) const;
    bool elevateAndApply(const QString &payloadRoot, const QString &installRoot, QString *errorOut);
    bool relaunchApplication(QString *errorOut) const;
    QString detectInstallRoot() const;
    QString detectPayloadRoot(const QString &extractDir) const;

    QString relaunchPathForInstall(const QString &installRoot) const;

    QNetworkAccessManager *m_network = nullptr;
    QString m_relaunchPath;
};

} // namespace itl
