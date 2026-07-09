#include "AppSettings.h"

#include <QJsonObject>
#include <QSettings>
#include <QStandardPaths>
#include <QRegularExpression>

namespace {
constexpr auto kInputDevice = "audio/inputDevice";
constexpr auto kOutputDevice = "audio/outputDevice";
constexpr auto kRingbackKind = "audio/ringbackKind";
constexpr auto kRingbackPath = "audio/ringbackPath";
constexpr auto kIncomingKind = "audio/incomingKind";
constexpr auto kIncomingPath = "audio/incomingPath";
constexpr auto kCustomContacts = "contacts/custom";
constexpr auto kProfileAvatarPath = "profile/avatarPath";
constexpr auto kProfileAvatarColor = "profile/avatarColor";
constexpr auto kShowChatButtons = "ui/showChatButtons";
constexpr auto kRecordingDualTrack = "recording/dualTrack";
constexpr auto kRecordingFilenameTemplate = "recording/filenameTemplate";
constexpr auto kRecordingEnabled = "recording/enabled";
constexpr auto kRecordingDirectory = "recording/directory";
} // namespace

namespace itl {

AppSettings::AppSettings(QObject *parent)
    : QObject(parent)
{
}

void AppSettings::load(QSettings &settings)
{
  m_inputDeviceId = settings.value(QString::fromUtf8(kInputDevice)).toString();
  m_outputDeviceId = settings.value(QString::fromUtf8(kOutputDevice)).toString();
  m_ringbackKind = ringtoneKindFromString(settings.value(QString::fromUtf8(kRingbackKind), QStringLiteral("builtin_ru")).toString());
  m_ringbackCustomPath = settings.value(QString::fromUtf8(kRingbackPath)).toString();
  m_incomingRingKind = ringtoneKindFromString(settings.value(QString::fromUtf8(kIncomingKind), QStringLiteral("builtin_classic")).toString());
  m_incomingRingCustomPath = settings.value(QString::fromUtf8(kIncomingPath)).toString();
  customContactsFromJson(settings.value(QString::fromUtf8(kCustomContacts)).toJsonArray());
  m_profileAvatarPath = settings.value(QString::fromUtf8(kProfileAvatarPath)).toString();
  m_profileAvatarColor = settings.value(QString::fromUtf8(kProfileAvatarColor), QStringLiteral("#5a9e2f")).toString();
  m_showChatButtons = settings.value(QString::fromUtf8(kShowChatButtons), true).toBool();
  m_recordingDualTrack = settings.value(QString::fromUtf8(kRecordingDualTrack), false).toBool();
  m_recordingFilenameTemplate =
      settings.value(QString::fromUtf8(kRecordingFilenameTemplate), QStringLiteral("%dmy_%h-%m-%s_%name")).toString();
  m_recordingEnabled = settings.value(QString::fromUtf8(kRecordingEnabled), true).toBool();
  m_recordingDirectory = settings.value(QString::fromUtf8(kRecordingDirectory)).toString();
}

void AppSettings::save(QSettings &settings) const
{
  settings.setValue(QString::fromUtf8(kInputDevice), m_inputDeviceId);
  settings.setValue(QString::fromUtf8(kOutputDevice), m_outputDeviceId);
  settings.setValue(QString::fromUtf8(kRingbackKind), QString::number(static_cast<int>(m_ringbackKind)));
  settings.setValue(QString::fromUtf8(kRingbackPath), m_ringbackCustomPath);
  settings.setValue(QString::fromUtf8(kIncomingKind), QString::number(static_cast<int>(m_incomingRingKind)));
  settings.setValue(QString::fromUtf8(kIncomingPath), m_incomingRingCustomPath);
  settings.setValue(QString::fromUtf8(kCustomContacts), customContactsToJson());
  settings.setValue(QString::fromUtf8(kProfileAvatarPath), m_profileAvatarPath);
  settings.setValue(QString::fromUtf8(kProfileAvatarColor), m_profileAvatarColor);
  settings.setValue(QString::fromUtf8(kShowChatButtons), m_showChatButtons);
  settings.setValue(QString::fromUtf8(kRecordingDualTrack), m_recordingDualTrack);
  settings.setValue(QString::fromUtf8(kRecordingFilenameTemplate), m_recordingFilenameTemplate);
  settings.setValue(QString::fromUtf8(kRecordingEnabled), m_recordingEnabled);
  settings.setValue(QString::fromUtf8(kRecordingDirectory), m_recordingDirectory);
}

void AppSettings::loadUserData(QSettings &settings)
{
  m_userData.load();
  m_userData.migrateFromSettings(settings);
}

void AppSettings::saveUserData() const
{
  m_userData.save();
}

QString AppSettings::ringtoneLabel(RingtoneKind kind)
{
  switch (kind) {
  case RingtoneKind::BuiltinRussian:
    return QObject::tr("Российский гудок (425 Гц)");
  case RingtoneKind::BuiltinClassic:
    return QObject::tr("Классический звонок");
  case RingtoneKind::BuiltinShort:
    return QObject::tr("Короткий сигнал");
  case RingtoneKind::CustomFile:
    return QObject::tr("Свой файл...");
  }
  return {};
}

AppSettings::RingtoneKind AppSettings::ringtoneKindFromString(const QString &value)
{
  bool ok = false;
  const int asInt = value.toInt(&ok);
  if (ok && asInt >= static_cast<int>(RingtoneKind::BuiltinRussian)
      && asInt <= static_cast<int>(RingtoneKind::CustomFile)) {
    return static_cast<RingtoneKind>(asInt);
  }
  if (value == QStringLiteral("builtin_ru")) {
    return RingtoneKind::BuiltinRussian;
  }
  if (value == QStringLiteral("builtin_short")) {
    return RingtoneKind::BuiltinShort;
  }
  if (value == QStringLiteral("custom")) {
    return RingtoneKind::CustomFile;
  }
  return RingtoneKind::BuiltinClassic;
}

void AppSettings::setInputDeviceId(const QString &id)
{
  if (m_inputDeviceId == id) {
    return;
  }
  m_inputDeviceId = id;
  emit settingsChanged();
}

void AppSettings::setOutputDeviceId(const QString &id)
{
  if (m_outputDeviceId == id) {
    return;
  }
  m_outputDeviceId = id;
  emit settingsChanged();
}

void AppSettings::setRingbackKind(RingtoneKind kind)
{
  if (m_ringbackKind == kind) {
    return;
  }
  m_ringbackKind = kind;
  emit settingsChanged();
}

void AppSettings::setRingbackCustomPath(const QString &path)
{
  if (m_ringbackCustomPath == path) {
    return;
  }
  m_ringbackCustomPath = path;
  emit settingsChanged();
}

void AppSettings::setIncomingRingKind(RingtoneKind kind)
{
  if (m_incomingRingKind == kind) {
    return;
  }
  m_incomingRingKind = kind;
  emit settingsChanged();
}

void AppSettings::setIncomingRingCustomPath(const QString &path)
{
  if (m_incomingRingCustomPath == path) {
    return;
  }
  m_incomingRingCustomPath = path;
  emit settingsChanged();
}

void AppSettings::setCustomContacts(const QList<CustomContact> &contacts)
{
  m_customContacts = contacts;
  emit settingsChanged();
}

void AppSettings::addCustomContact(const CustomContact &contact)
{
  for (int i = 0; i < m_customContacts.size(); ++i) {
    if (m_customContacts[i].peer == contact.peer) {
      m_customContacts[i] = contact;
      emit settingsChanged();
      return;
    }
  }
  m_customContacts.append(contact);
  emit settingsChanged();
}

bool AppSettings::removeCustomContact(const QString &peer)
{
  for (int i = 0; i < m_customContacts.size(); ++i) {
    if (m_customContacts[i].peer == peer) {
      m_customContacts.removeAt(i);
      emit settingsChanged();
      return true;
    }
  }
  return false;
}

void AppSettings::setProfileAvatarPath(const QString &path)
{
  if (m_profileAvatarPath == path) {
    return;
  }
  m_profileAvatarPath = path;
  emit settingsChanged();
}

void AppSettings::setProfileAvatarColor(const QString &color)
{
  if (m_profileAvatarColor == color) {
    return;
  }
  m_profileAvatarColor = color;
  emit settingsChanged();
}

void AppSettings::setShowChatButtons(bool show)
{
  if (m_showChatButtons == show) {
    return;
  }
  m_showChatButtons = show;
  emit settingsChanged();
}

void AppSettings::setRecordingDualTrack(bool dual)
{
  if (m_recordingDualTrack == dual) {
    return;
  }
  m_recordingDualTrack = dual;
  emit settingsChanged();
}

void AppSettings::setRecordingFilenameTemplate(const QString &templateText)
{
  const QString trimmed = templateText.trimmed();
  if (m_recordingFilenameTemplate == trimmed) {
    return;
  }
  m_recordingFilenameTemplate = trimmed.isEmpty() ? QStringLiteral("%dmy_%h-%m-%s_%name") : trimmed;
  emit settingsChanged();
}

void AppSettings::setRecordingEnabled(bool enabled)
{
  if (m_recordingEnabled == enabled) {
    return;
  }
  m_recordingEnabled = enabled;
  emit settingsChanged();
}

void AppSettings::setRecordingDirectory(const QString &directory)
{
  const QString trimmed = directory.trimmed();
  if (m_recordingDirectory == trimmed) {
    return;
  }
  m_recordingDirectory = trimmed;
  emit settingsChanged();
}

QString AppSettings::recordingFilenameSyntaxHelp()
{
  return QObject::tr(
      "Шаблон имени файла записи:\n"
      "%dmy — день, месяц, год (дд.ММ.гггг)\n"
      "%h — час (00–23)\n"
      "%m — минута\n"
      "%s — секунда\n"
      "%name — ФИО абонента или номер телефона");
}

QString AppSettings::defaultRecordingDirectory()
{
  return QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)
         + QStringLiteral("/opensource-communicator/recordings");
}

QString AppSettings::expandRecordingFilenameTemplate(const QString &templateText, const QString &contactName,
                                                     const QDateTime &when)
{
  const QDateTime dt = when.isValid() ? when : QDateTime::currentDateTime();
  QString result = templateText.trimmed();
  if (result.isEmpty()) {
    result = QStringLiteral("%dmy_%h-%m-%s_%name");
  }

  result.replace(QStringLiteral("%dmy"), dt.toString(QStringLiteral("dd.MM.yyyy")));
  result.replace(QStringLiteral("%h"), dt.toString(QStringLiteral("hh")));
  result.replace(QStringLiteral("%m"), dt.toString(QStringLiteral("mm")));
  result.replace(QStringLiteral("%s"), dt.toString(QStringLiteral("ss")));
  QString safeName = contactName.trimmed();
  safeName.replace(QRegularExpression(QStringLiteral("\\s+")), QStringLiteral("_"));
  if (safeName.isEmpty()) {
    safeName = QStringLiteral("call");
  }
  result.replace(QStringLiteral("%name"), safeName);
  return result;
}

QString AppSettings::noteForPeer(const QString &peer) const
{
  return m_userData.noteForPeer(peer);
}

void AppSettings::setNoteForPeer(const QString &peer, const QString &note)
{
  const QString trimmed = note.trimmed();
  if (m_userData.noteForPeer(peer) == trimmed) {
    return;
  }
  m_userData.setNoteForPeer(peer, note);
  emit settingsChanged();
}

qint64 AppSettings::recentCallTime(const QString &peer) const
{
  return m_userData.recentCallTime(peer);
}

void AppSettings::recordRecentCall(const QString &peer)
{
  m_userData.recordRecentCall(peer);
  emit settingsChanged();
}

void AppSettings::addCallHistoryEntry(const CallHistoryEntry &entry)
{
  m_userData.addCallHistoryEntry(entry);
  emit settingsChanged();
}

QList<CallHistoryEntry> AppSettings::callHistory() const
{
  return m_userData.callHistory();
}

QJsonArray AppSettings::customContactsToJson() const
{
  QJsonArray array;
  for (const CustomContact &contact : m_customContacts) {
    QJsonObject obj{
        {QStringLiteral("peer"), contact.peer},
        {QStringLiteral("name"), contact.name},
        {QStringLiteral("phone"), contact.phone},
        {QStringLiteral("ext"), contact.ext},
    };
    if (!contact.serverId.isEmpty()) {
      obj.insert(QStringLiteral("serverId"), contact.serverId);
    }
    array.append(obj);
  }
  return array;
}

void AppSettings::customContactsFromJson(const QJsonArray &array)
{
  m_customContacts.clear();
  for (const QJsonValue &value : array) {
    const QJsonObject obj = value.toObject();
    CustomContact contact;
    contact.peer = obj.value(QStringLiteral("peer")).toString();
    contact.name = obj.value(QStringLiteral("name")).toString();
    contact.phone = obj.value(QStringLiteral("phone")).toString();
    contact.ext = obj.value(QStringLiteral("ext")).toString();
    contact.serverId = obj.value(QStringLiteral("serverId")).toString();
    if (!contact.peer.isEmpty()) {
      m_customContacts.append(contact);
    }
  }
}

} // namespace itl
