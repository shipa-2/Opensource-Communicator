#pragma once

#include "settings/AppSettings.h"

#include <QDialog>

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
                   const QString &displayName, QWidget *parent = nullptr);
    ~SettingsDialog() override;
    QString displayName() const;

private slots:
    void onBrowseRingback();
    void onBrowseIncoming();
    void onAccept();
    void onPreviewRingback();
    void onPreviewIncoming();
    void stopPreview();

protected:
    void reject() override;

private:
    void loadDevices();
    void loadFromSettings();
    void updatePreviewButtons();
    itl::AppSettings::RingtoneKind ringtoneKindForIndex(int index) const;
    itl::AppSettings::RingtoneKind incomingRingtoneKindForIndex(int index) const;
    void syncPreviewSettings(itl::AppSettings *target) const;

    itl::CommunicatorClient *m_client = nullptr;
    itl::CallManager *m_calls = nullptr;
    itl::AppSettings *m_settings = nullptr;
    itl::AppSettings m_previewSettings;
    itl::RingbackPlayer *m_ringbackPreview = nullptr;
    itl::IncomingRingPlayer *m_incomingPreview = nullptr;
    class QTimer *m_previewTimer = nullptr;

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
    class QLineEdit *m_displayNameEdit = nullptr;
    QString m_selfName;

    class QCheckBox *m_recordingEnabledCheck = nullptr;
    class QCheckBox *m_recordingDualTrackCheck = nullptr;
    class QCheckBox *m_recordingCombinedCheck = nullptr;
    class QLineEdit *m_recordingDirEdit = nullptr;
    class QLineEdit *m_recordingTemplateEdit = nullptr;
    class QLabel *m_recordingPreviewLabel = nullptr;
};
