#include "update/AppUpdater.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTextStream>
#include <QTimer>
#include <QUrl>

namespace itl {
namespace {

bool copyRecursively(const QString &srcDir, const QString &dstDir, QString *errorOut)
{
  QDir src(srcDir);
  if (!src.exists()) {
    return true;
  }
  if (!QDir().mkpath(dstDir)) {
    if (errorOut) {
      *errorOut = QObject::tr("Не удалось создать каталог %1").arg(dstDir);
    }
    return false;
  }

  const QFileInfoList entries =
      src.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden);
  for (const QFileInfo &entry : entries) {
    const QString targetPath = dstDir + QLatin1Char('/') + entry.fileName();
    if (entry.isDir()) {
      if (!copyRecursively(entry.absoluteFilePath(), targetPath, errorOut)) {
        return false;
      }
      continue;
    }
    if (QFile::exists(targetPath) && !QFile::remove(targetPath)) {
      if (errorOut) {
        *errorOut = QObject::tr("Не удалось заменить файл %1").arg(targetPath);
      }
      return false;
    }
    if (!QFile::copy(entry.absoluteFilePath(), targetPath)) {
      if (errorOut) {
        *errorOut = QObject::tr("Не удалось скопировать %1").arg(entry.fileName());
      }
      return false;
    }
  }
  return true;
}

QString shellQuote(const QString &value)
{
  QString escaped = value;
  escaped.replace(QLatin1Char('\''), QStringLiteral("'\\''"));
  return QStringLiteral("'") + escaped + QStringLiteral("'");
}

bool removeRecursively(const QString &path)
{
  if (!QFileInfo::exists(path)) {
    return true;
  }
  return QDir(path).removeRecursively();
}

} // namespace

AppUpdater::AppUpdater(QObject *parent)
    : QObject(parent)
{
}

QList<int> AppUpdater::parseVersion(const QString &raw)
{
  QString tag = raw;
  if (tag.startsWith(QLatin1Char('v')) || tag.startsWith(QLatin1Char('V'))) {
    tag.remove(0, 1);
  }
  const QStringList parts = tag.split(QLatin1Char('.'));
  QList<int> numbers;
  for (const QString &part : parts) {
    QString digits;
    for (QChar ch : part) {
      if (ch.isDigit()) {
        digits.append(ch);
      } else {
        break;
      }
    }
    numbers.append(digits.isEmpty() ? 0 : digits.toInt());
  }
  while (numbers.size() < 3) {
    numbers.append(0);
  }
  return numbers.mid(0, 3);
}

bool AppUpdater::isRemoteNewer(const QString &current, const QString &remoteTag)
{
  const QList<int> latest = parseVersion(remoteTag);
  const QList<int> currentParts = parseVersion(baseVersion(current));
  for (int i = 0; i < 3; ++i) {
    if (latest.at(i) != currentParts.at(i)) {
      return latest.at(i) > currentParts.at(i);
    }
  }
  return false;
}

QString AppUpdater::baseVersion(const QString &appVersion)
{
  const int dash = appVersion.indexOf(QLatin1Char('-'));
  if (dash < 0) {
    return appVersion;
  }
  return appVersion.left(dash);
}

bool AppUpdater::isPreReleaseBuild(const QString &appVersion)
{
  return appVersion.contains(QStringLiteral("-videotest-"), Qt::CaseInsensitive);
}

QString AppUpdater::preReleaseStampFromVersion(const QString &appVersion)
{
  const int marker = appVersion.indexOf(QStringLiteral("-videotest-"), 0, Qt::CaseInsensitive);
  if (marker < 0) {
    return {};
  }
  return appVersion.mid(marker + QStringLiteral("-videotest-").size());
}

QString AppUpdater::preReleaseStampFromTag(const QString &tag)
{
  if (tag.startsWith(QStringLiteral("videotest-"), Qt::CaseInsensitive)) {
    return tag.mid(QStringLiteral("videotest-").size());
  }
  return {};
}

QString AppUpdater::packageVersionFromFileName(const QString &fileName)
{
  const QString prefix = QStringLiteral("OpenSource-Communicator-");
  if (!fileName.startsWith(prefix, Qt::CaseInsensitive)) {
    return {};
  }
  const QString rest = fileName.mid(prefix.size());
  const int nextDash = rest.indexOf(QLatin1Char('-'));
  if (nextDash <= 0) {
    return {};
  }
  return rest.left(nextDash);
}

bool AppUpdater::isPreReleaseTagNewer(const QString &currentAppVersion, const QString &remoteTag)
{
  const QString currentStamp = preReleaseStampFromVersion(currentAppVersion);
  const QString remoteStamp = preReleaseStampFromTag(remoteTag);
  if (remoteStamp.isEmpty()) {
    return false;
  }
  if (currentStamp.isEmpty()) {
    return true;
  }
  return remoteStamp > currentStamp;
}

AppUpdater::ScanResult AppUpdater::scanReleases(const QJsonArray &releases,
                                                const QString &currentAppVersion)
{
  ScanResult result;
  result.onPreReleaseBuild = isPreReleaseBuild(currentAppVersion);

  QDateTime latestPreReleaseDate;
  for (const QJsonValue &value : releases) {
    const QJsonObject release = value.toObject();
    if (release.value(QStringLiteral("draft")).toBool()) {
      continue;
    }

    const bool preRelease = release.value(QStringLiteral("prerelease")).toBool();
    const QString tag = release.value(QStringLiteral("tag_name")).toString();
    const QString assetUrl = findAssetUrl(release);
    if (tag.isEmpty() || assetUrl.isEmpty()) {
      continue;
    }

    ChannelOffer offer;
    offer.tag = tag;
    offer.pageUrl = release.value(QStringLiteral("html_url")).toString();
    offer.assetUrl = assetUrl;
    offer.fileName = QUrl(assetUrl).fileName();
    offer.packageVersion = packageVersionFromFileName(offer.fileName);
    offer.available = true;

    if (preRelease) {
      const QDateTime published =
          QDateTime::fromString(release.value(QStringLiteral("published_at")).toString(), Qt::ISODate);
      if (!latestPreReleaseDate.isValid() || published > latestPreReleaseDate) {
        latestPreReleaseDate = published;
        result.preRelease = offer;
      }
      continue;
    }

    if (!result.release.available) {
      result.release = offer;
    }
  }

  if (result.release.available) {
    if (result.onPreReleaseBuild) {
      result.release.updateAvailable = true;
    } else {
      result.release.updateAvailable = isRemoteNewer(currentAppVersion, result.release.tag);
    }
  }

  if (result.preRelease.available) {
    const QString remoteVersion =
        result.preRelease.packageVersion.isEmpty() ? result.preRelease.tag
                                                   : result.preRelease.packageVersion;
    if (!result.onPreReleaseBuild) {
      result.preRelease.updateAvailable = true;
    } else {
      const bool stampNewer =
          isPreReleaseTagNewer(currentAppVersion, result.preRelease.tag);
      const bool packageNewer = isRemoteNewer(currentAppVersion, remoteVersion);
      result.preRelease.updateAvailable = stampNewer || packageNewer;

      const QString currentStamp = preReleaseStampFromVersion(currentAppVersion);
      const QString remoteStamp = preReleaseStampFromTag(result.preRelease.tag);
      result.preRelease.localBuildAhead =
          !result.preRelease.updateAvailable && !currentStamp.isEmpty() &&
          !remoteStamp.isEmpty() && currentStamp > remoteStamp;
    }
  }

  return result;
}

QString AppUpdater::platformAssetToken()
{
#if defined(Q_OS_WIN)
  return QStringLiteral("win64");
#elif defined(Q_OS_LINUX)
  return QStringLiteral("linux-x86_64");
#else
  return {};
#endif
}

QString AppUpdater::phaseTitle(Phase phase)
{
  switch (phase) {
  case Phase::Checking:
    return tr("Проверка обновлений");
  case Phase::Downloading:
    return tr("Загрузка");
  case Phase::Unpacking:
    return tr("Распаковка");
  case Phase::Installing:
    return tr("Установка");
  case Phase::Finalizing:
    return tr("Финальные штрихи");
  case Phase::Restarting:
    return tr("Перезапуск");
  case Phase::Done:
    return tr("Готово");
  case Phase::Failed:
    return tr("Ошибка");
  case Phase::Idle:
    break;
  }
  return {};
}

QString AppUpdater::findAssetUrl(const QJsonObject &releaseObject)
{
  const QString token = platformAssetToken();
  if (token.isEmpty()) {
    return {};
  }

  const QJsonArray assets = releaseObject.value(QStringLiteral("assets")).toArray();
  for (const QJsonValue &value : assets) {
    const QJsonObject asset = value.toObject();
    const QString name = asset.value(QStringLiteral("name")).toString();
    if (!name.contains(token, Qt::CaseInsensitive)) {
      continue;
    }
    if (name.contains(QStringLiteral("debug"), Qt::CaseInsensitive)) {
      continue;
    }
    return asset.value(QStringLiteral("browser_download_url")).toString();
  }
  return {};
}

void AppUpdater::downloadAndInstall(const QUrl &url, const QString &fileName)
{
  m_relaunchPath.clear();
  if (!m_network) {
    m_network = new QNetworkAccessManager(this);
  }

  const QString cacheDir =
      QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + QStringLiteral("/updates");
  QDir().mkpath(cacheDir);
  const QString downloadPath = cacheDir + QLatin1Char('/') + fileName;

  if (QFile::exists(downloadPath)) {
    QFile::remove(downloadPath);
  }

  emit progressChanged(Phase::Downloading, {});

  QNetworkRequest request(url);
  request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
  QNetworkReply *reply = m_network->get(request);
  connect(reply, &QNetworkReply::downloadProgress, this, [this](qint64 received, qint64 total) {
    if (total <= 0) {
      return;
    }
    const int percent = static_cast<int>((received * 100) / total);
    emit progressChanged(Phase::Downloading, tr("%1%").arg(percent));
  });

  connect(reply, &QNetworkReply::finished, this, [this, reply, downloadPath, fileName, cacheDir] {
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
      emit finished(false, tr("Ошибка загрузки: %1").arg(reply->errorString()));
      return;
    }

    QFile out(downloadPath);
    if (!out.open(QIODevice::WriteOnly)) {
      emit finished(false, tr("Не удалось сохранить файл обновления"));
      return;
    }
    out.write(reply->readAll());
    out.close();

    emit progressChanged(Phase::Unpacking, {});

    QTemporaryDir extractDir;
    if (!extractDir.isValid()) {
      emit finished(false, tr("Не удалось создать временный каталог"));
      return;
    }

    QString error;
    if (!extractArchive(downloadPath, extractDir.path(), &error)) {
      emit finished(false, error);
      return;
    }

    const QString payloadRoot = detectPayloadRoot(extractDir.path());
    const QString stagingDir = cacheDir + QStringLiteral("/staging-") +
                               QFileInfo(fileName).completeBaseName();
    emit progressChanged(Phase::Finalizing, tr("Подготовка файлов"));
    removeRecursively(stagingDir);
    if (!copyRecursively(payloadRoot, stagingDir, &error)) {
      emit finished(false, error);
      return;
    }

    const QString installRoot = detectInstallRoot();
    if (installRoot.isEmpty()) {
      emit finished(false, tr("Не удалось определить каталог установки"));
      return;
    }

#if defined(Q_OS_WIN)
    emit progressChanged(Phase::Finalizing, tr("Подготовка перезапуска"));
    const QString appDir = QCoreApplication::applicationDirPath();
    const QString exePath = QCoreApplication::applicationFilePath();
    const QString helperPath =
        QStandardPaths::writableLocation(QStandardPaths::TempLocation) +
        QStringLiteral("/osc-update-helper.bat");

    QFile helper(helperPath);
    if (!helper.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
      emit finished(false, tr("Не удалось создать скрипт перезапуска"));
      return;
    }

    QTextStream bat(&helper);
    bat << "@echo off\r\n";
    bat << "timeout /t 2 /nobreak >nul\r\n";
    bat << "xcopy /y /e /i \"" << stagingDir << "\\*\" \"" << appDir << "\\\"\r\n";
    bat << "start \"\" \"" << exePath << "\"\r\n";
    bat << "del \"%~f0\"\r\n";
    helper.close();

    if (!QProcess::startDetached(QStringLiteral("cmd.exe"), {QStringLiteral("/c"), helperPath})) {
      emit finished(false, tr("Не удалось перезапустить приложение"));
      return;
    }

    emit progressChanged(Phase::Restarting, {});
    emit finished(true, tr("Обновление будет установлено после перезапуска"));
    QTimer::singleShot(0, qApp, &QCoreApplication::quit);
    return;
#endif

    emit progressChanged(Phase::Installing, {});

    if (installRootWritable(installRoot)) {
      if (!applyPayload(stagingDir, installRoot, &m_relaunchPath, &error)) {
        emit finished(false, error);
        return;
      }
    } else {
      emit progressChanged(Phase::Finalizing, tr("Запрос прав администратора"));
      if (!elevateAndApply(stagingDir, installRoot, &error)) {
        emit finished(false, error);
        return;
      }
      emit progressChanged(Phase::Restarting, {});
      emit finished(true, tr("Запрошены права администратора. Приложение закроется после установки."));
      QTimer::singleShot(0, qApp, &QCoreApplication::quit);
      return;
    }

    emit progressChanged(Phase::Restarting, {});
    if (!relaunchApplication(&error)) {
      emit finished(false, error);
      return;
    }

    emit finished(true, tr("Обновление установлено. Перезапуск..."));
    QTimer::singleShot(0, qApp, &QCoreApplication::quit);
  });
}

bool AppUpdater::extractArchive(const QString &archivePath, const QString &destDir, QString *errorOut)
{
  QProcess tar;
  tar.setProgram(QStringLiteral("tar"));
  tar.setArguments({QStringLiteral("-xf"), archivePath, QStringLiteral("-C"), destDir});
  tar.start();
  if (!tar.waitForStarted(5000)) {
    if (errorOut) {
      *errorOut = tr("Не найден tar для распаковки");
    }
    return false;
  }
  if (!tar.waitForFinished(120000) || tar.exitStatus() != QProcess::NormalExit || tar.exitCode() != 0) {
    if (errorOut) {
      *errorOut = tr("Не удалось распаковать архив");
    }
    return false;
  }
  return true;
}

QString AppUpdater::detectInstallRoot() const
{
  const QString appDir = QCoreApplication::applicationDirPath();
#if defined(Q_OS_WIN)
  return appDir;
#else
  const QDir dir(appDir);
  if (dir.dirName() == QStringLiteral("bin") &&
      (QDir(appDir + QStringLiteral("/../lib")).exists() ||
       QDir(appDir + QStringLiteral("/../share")).exists())) {
    return QDir(appDir).absoluteFilePath(QStringLiteral(".."));
  }
  if (appDir == QStringLiteral("/usr/bin") || appDir.endsWith(QStringLiteral("/usr/bin"))) {
    return QStringLiteral("/usr");
  }
  return appDir;
#endif
}

QString AppUpdater::detectPayloadRoot(const QString &extractDir) const
{
  QDir root(extractDir);
  if (root.exists(QStringLiteral("bin")) || root.exists(QStringLiteral("lib"))) {
    return extractDir;
  }
  if (root.exists(QStringLiteral("usr"))) {
    return extractDir + QStringLiteral("/usr");
  }

  const QStringList entries = root.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
  if (entries.size() == 1) {
    const QString nested = extractDir + QLatin1Char('/') + entries.first();
    const QDir nestedDir(nested);
    if (nestedDir.exists(QStringLiteral("bin")) ||
        nestedDir.exists(QStringLiteral("opensource-communicator.exe"))) {
      return nested;
    }
  }
  return extractDir;
}

bool AppUpdater::applyPayload(const QString &payloadRoot, const QString &installRoot, QString *relaunchPathOut,
                              QString *errorOut)
{
  const QString exeName = QFileInfo(QCoreApplication::applicationFilePath()).fileName();

  if (QDir(payloadRoot + QStringLiteral("/bin")).exists()) {
    const QString stagedExe = payloadRoot + QStringLiteral("/bin/") + exeName;
    if (!QFile::exists(stagedExe)) {
      if (errorOut) {
        *errorOut = tr("В архиве нет исполняемого файла");
      }
      return false;
    }

    const QString binDir = installRoot + QStringLiteral("/bin");
    const QString libDir = installRoot + QStringLiteral("/lib");
    if (!QDir().mkpath(binDir) || !QDir().mkpath(libDir)) {
      if (errorOut) {
        *errorOut = tr("Не удалось создать каталоги bin/lib");
      }
      return false;
    }

    if (!copyRecursively(payloadRoot + QStringLiteral("/bin"), binDir, errorOut)) {
      return false;
    }
    if (QDir(payloadRoot + QStringLiteral("/lib")).exists() &&
        !copyRecursively(payloadRoot + QStringLiteral("/lib"), libDir, errorOut)) {
      return false;
    }
    if (QDir(payloadRoot + QStringLiteral("/share")).exists() &&
        !copyRecursively(payloadRoot + QStringLiteral("/share"), installRoot + QStringLiteral("/share"), errorOut)) {
      return false;
    }

    const QString newExePath = binDir + QLatin1Char('/') + exeName;
    const QString legacyFlatExe = installRoot + QLatin1Char('/') + exeName;
    if (legacyFlatExe != newExePath && QFile::exists(legacyFlatExe)) {
      QFile::remove(legacyFlatExe);
    }

    if (relaunchPathOut) {
      *relaunchPathOut = newExePath;
    }
    return true;
  }

  if (relaunchPathOut) {
    *relaunchPathOut = QCoreApplication::applicationFilePath();
  }
  return copyRecursively(payloadRoot, installRoot, errorOut);
}

QString AppUpdater::relaunchPathForInstall(const QString &installRoot) const
{
  if (!m_relaunchPath.isEmpty()) {
    return m_relaunchPath;
  }
  const QString exeName = QFileInfo(QCoreApplication::applicationFilePath()).fileName();
  const QString releaseLayoutExe = installRoot + QStringLiteral("/bin/") + exeName;
  if (QFile::exists(releaseLayoutExe)) {
    return releaseLayoutExe;
  }
  return QCoreApplication::applicationFilePath();
}

bool AppUpdater::installRootWritable(const QString &installRoot) const
{
  const QString probePath = installRoot + QStringLiteral("/.osc_update_probe");
  QFile probe(probePath);
  if (!probe.open(QIODevice::WriteOnly)) {
    return false;
  }
  probe.close();
  probe.remove();
  return true;
}

bool AppUpdater::elevateAndApply(const QString &payloadRoot, const QString &installRoot, QString *errorOut)
{
#if !defined(Q_OS_LINUX)
  Q_UNUSED(payloadRoot);
  Q_UNUSED(installRoot);
  if (errorOut) {
    *errorOut = tr("Повышение прав не поддерживается на этой платформе");
  }
  return false;
#else
  const QString exePath = QCoreApplication::applicationFilePath();
  const QString scriptPath =
      QStandardPaths::writableLocation(QStandardPaths::TempLocation) +
      QStringLiteral("/osc-apply-update.sh");

  QFile script(scriptPath);
  if (!script.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
    if (errorOut) {
      *errorOut = tr("Не удалось создать скрипт обновления");
    }
    return false;
  }

  const QString exeName = QFileInfo(exePath).fileName();
  const QString relaunchPath = installRoot + QStringLiteral("/bin/") + exeName;
  QTextStream out(&script);
  out << "#!/bin/sh\n";
  out << "set -e\n";
  if (QDir(payloadRoot + QStringLiteral("/bin")).exists()) {
    out << "mkdir -p " << shellQuote(installRoot + QStringLiteral("/bin")) << " "
        << shellQuote(installRoot + QStringLiteral("/lib")) << "\n";
    out << "cp -a " << shellQuote(payloadRoot + QStringLiteral("/bin/.")) << " "
        << shellQuote(installRoot + QStringLiteral("/bin/")) << "\n";
    if (QDir(payloadRoot + QStringLiteral("/lib")).exists()) {
      out << "cp -a " << shellQuote(payloadRoot + QStringLiteral("/lib/.")) << " "
          << shellQuote(installRoot + QStringLiteral("/lib/")) << "\n";
    }
    if (QDir(payloadRoot + QStringLiteral("/share")).exists()) {
      out << "mkdir -p " << shellQuote(installRoot + QStringLiteral("/share")) << "\n";
      out << "cp -a " << shellQuote(payloadRoot + QStringLiteral("/share/.")) << " "
          << shellQuote(installRoot + QStringLiteral("/share/")) << "\n";
    }
    out << "rm -f " << shellQuote(installRoot + QLatin1Char('/') + exeName) << "\n";
  } else {
    out << "cp -a " << shellQuote(payloadRoot + QStringLiteral("/.")) << " "
        << shellQuote(installRoot + QStringLiteral("/")) << "\n";
  }
  out << "exec " << shellQuote(QFile::exists(relaunchPath) ? relaunchPath : exePath) << " \"$@\" &\n";
  script.close();
  script.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);

  if (!QProcess::startDetached(QStringLiteral("pkexec"), {QStringLiteral("bash"), scriptPath})) {
    if (errorOut) {
      *errorOut = tr("Не удалось запустить pkexec");
    }
    return false;
  }
  return true;
#endif
}

bool AppUpdater::relaunchApplication(QString *errorOut) const
{
  const QString exePath = relaunchPathForInstall(detectInstallRoot());
  if (!QProcess::startDetached(exePath, QCoreApplication::arguments().mid(1))) {
    if (errorOut) {
      *errorOut = tr("Не удалось перезапустить приложение");
    }
    return false;
  }
  return true;
}

} // namespace itl
