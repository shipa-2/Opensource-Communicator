#pragma once

#include "UserDataStore.h"

#include <QHash>
#include <QJsonArray>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QDateTime>

class QSettings;

namespace itl {

struct CustomContact {
    QString peer;
    QString name;
    QString phone;
    QString ext;
};

class AppSettings : public QObject {
    Q_OBJECT

public:
    enum class RingtoneKind {
        BuiltinRussian,
        BuiltinClassic,
        BuiltinShort,
        CustomFile,
    };
    Q_ENUM(RingtoneKind)

    explicit AppSettings(QObject *parent = nullptr);

    void load(QSettings &settings);
    void save(QSettings &settings) const;
    void loadUserData(QSettings &settings);
    void saveUserData() const;

    UserDataStore &userData() { return m_userData; }
    const UserDataStore &userData() const { return m_userData; }

    QString inputDeviceId() const { return m_inputDeviceId; }
    QString outputDeviceId() const { return m_outputDeviceId; }
    RingtoneKind ringbackKind() const { return m_ringbackKind; }
    QString ringbackCustomPath() const { return m_ringbackCustomPath; }
    RingtoneKind incomingRingKind() const { return m_incomingRingKind; }
    QString incomingRingCustomPath() const { return m_incomingRingCustomPath; }
    QList<CustomContact> customContacts() const { return m_customContacts; }

    void setInputDeviceId(const QString &id);
    void setOutputDeviceId(const QString &id);
    void setRingbackKind(RingtoneKind kind);
    void setRingbackCustomPath(const QString &path);
    void setIncomingRingKind(RingtoneKind kind);
    void setIncomingRingCustomPath(const QString &path);
    void setCustomContacts(const QList<CustomContact> &contacts);

    void addCustomContact(const CustomContact &contact);
    bool removeCustomContact(const QString &peer);

    QString profileAvatarPath() const { return m_profileAvatarPath; }
    QString profileAvatarColor() const { return m_profileAvatarColor; }
    void setProfileAvatarPath(const QString &path);
    void setProfileAvatarColor(const QString &color);

    bool showChatButtons() const { return m_showChatButtons; }
    void setShowChatButtons(bool show);

    bool recordingDualTrack() const { return m_recordingDualTrack; }
    QString recordingFilenameTemplate() const { return m_recordingFilenameTemplate; }
    bool recordingEnabled() const { return m_recordingEnabled; }
    QString recordingDirectory() const { return m_recordingDirectory; }
    void setRecordingDualTrack(bool dual);
    void setRecordingFilenameTemplate(const QString &templateText);
    void setRecordingEnabled(bool enabled);
    void setRecordingDirectory(const QString &directory);

    QString noteForPeer(const QString &peer) const;
    void setNoteForPeer(const QString &peer, const QString &note);

    qint64 recentCallTime(const QString &peer) const;
    void recordRecentCall(const QString &peer);

    void addCallHistoryEntry(const CallHistoryEntry &entry);
    QList<CallHistoryEntry> callHistory() const;

    static QString ringtoneLabel(RingtoneKind kind);
    static RingtoneKind ringtoneKindFromString(const QString &value);
    static QString recordingFilenameSyntaxHelp();
    static QString defaultRecordingDirectory();
    static QString expandRecordingFilenameTemplate(const QString &templateText, const QString &contactName,
                                                   const QDateTime &when = QDateTime());

signals:
    void settingsChanged();

private:
    QJsonArray customContactsToJson() const;
    void customContactsFromJson(const QJsonArray &array);

    UserDataStore m_userData;
    QString m_inputDeviceId;
    QString m_outputDeviceId;
    RingtoneKind m_ringbackKind = RingtoneKind::BuiltinRussian;
    QString m_ringbackCustomPath;
    RingtoneKind m_incomingRingKind = RingtoneKind::BuiltinClassic;
    QString m_incomingRingCustomPath;
    QList<CustomContact> m_customContacts;
    QString m_profileAvatarPath;
    QString m_profileAvatarColor = QStringLiteral("#5a9e2f");
    bool m_showChatButtons = true;
    bool m_recordingDualTrack = false;
    bool m_recordingEnabled = true;
    QString m_recordingFilenameTemplate = QStringLiteral("%dmy_%h-%m-%s_%name");
    QString m_recordingDirectory;
};

} // namespace itl
