#pragma once

#include "settings/AppSettings.h"

#include <QDialog>

namespace itl {
class CallManager;
class CommunicatorClient;
class IncomingRingPlayer;
class RingbackPlayer;
}

class SettingsDialog : public QDialog {
    Q_OBJECT

public:
    SettingsDialog(itl::CommunicatorClient *client, itl::CallManager *calls, QWidget *parent = nullptr);
    ~SettingsDialog() override;

private slots:
    void onBrowseRingback();
    void onBrowseIncoming();
    void onAccountSettings();
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
};
