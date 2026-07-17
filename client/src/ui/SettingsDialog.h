#pragma once

#include "settings/AppSettings.h"

#include <QDialog>
#include <QHash>
#include <QString>

namespace itl {
class CallManager;
class CommunicatorClient;
class IncomingRingPlayer;
class RingbackPlayer;
}

class ProfileAvatarWidget;

class SettingsDialog : public QDialog {
    Q_OBJECT

public:
    SettingsDialog(itl::CommunicatorClient *client, itl::CallManager *calls,
                   const QString &displayName, const QHash<QString, QString> &sharePeers,
                   const QString &selfPeer, QWidget *parent = nullptr);
    ~SettingsDialog() override;
    QString displayName() const;

private slots:
    void onBrowseRingback();
    void onBrowseIncoming();
    void onAccept();
    void onPreviewRingback();
    void onPreviewIncoming();
    void stopPreview();
    void onShareAvatar();
    void onShareTheme();
    void updateShareProfileButton();

protected:
    void reject() override;

private:
    void loadDevices();
    void loadNetworkInterfaces();
    void loadFromSettings();
    void updatePreviewButtons();
    itl::AppSettings::RingtoneKind ringtoneKindForIndex(int index) const;
    itl::AppSettings::RingtoneKind incomingRingtoneKindForIndex(int index) const;
    void syncPreviewSettings(itl::AppSettings *target) const;
    void updateWallpaperPreview();

    itl::CommunicatorClient *m_client = nullptr;
    itl::CallManager *m_calls = nullptr;
    itl::AppSettings *m_settings = nullptr;
    itl::AppSettings m_previewSettings;
    itl::RingbackPlayer *m_ringbackPreview = nullptr;
    itl::IncomingRingPlayer *m_incomingPreview = nullptr;
    class QTimer *m_previewTimer = nullptr;
    QHash<QString, QString> m_sharePeers;
    QString m_selfPeer;

    class QComboBox *m_inputDevice = nullptr;
    class QComboBox *m_outputDevice = nullptr;
    class QComboBox *m_ringbackTone = nullptr;
    class QComboBox *m_incomingTone = nullptr;
    class QLineEdit *m_ringbackPath = nullptr;
    class QLineEdit *m_incomingPath = nullptr;
    class QPushButton *m_ringbackBrowse = nullptr;
    class QPushButton *m_incomingBrowse = nullptr;
    class QPushButton *m_ringbackPreviewBtn = nullptr;
    class QPushButton *m_incomingPreviewBtn = nullptr;

    ProfileAvatarWidget *m_accountAvatar = nullptr;
    class QPushButton *m_shareProfileBtn = nullptr;
    class QLineEdit *m_displayNameEdit = nullptr;
    class QComboBox *m_networkInterface = nullptr;
    QString m_selfName;

    class QCheckBox *m_recordingEnabledCheck = nullptr;
    class QCheckBox *m_recordingDualTrackCheck = nullptr;
    class QCheckBox *m_recordingCombinedCheck = nullptr;
    class QLineEdit *m_recordingDirEdit = nullptr;
    class QLineEdit *m_recordingTemplateEdit = nullptr;
    class QLabel *m_recordingPreviewLabel = nullptr;
    class QLabel *m_wallpaperPreview = nullptr;
    class QPushButton *m_removeWallpaperBtn = nullptr;
    class QPushButton *m_shareThemeBtn = nullptr;
    class QSlider *m_wallpaperOpacitySlider = nullptr;
    class QLabel *m_wallpaperOpacityValue = nullptr;
    class QWidget *m_wallpaperOpacityRow = nullptr;
    class QSlider *m_wallpaperListOpacitySlider = nullptr;
    class QLabel *m_wallpaperListOpacityValue = nullptr;
    class QWidget *m_wallpaperListOpacityRow = nullptr;
};
