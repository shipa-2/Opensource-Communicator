#pragma once

#include <QDialog>

namespace itl {
class AppSettings;
class CommunicatorClient;
}

class RecordingSettingsDialog : public QDialog {
    Q_OBJECT

public:
    RecordingSettingsDialog(itl::CommunicatorClient *client, QWidget *parent = nullptr);

private slots:
    void onAccepted();
    void updatePreview();
    void onBrowseDirectory();

private:
    itl::CommunicatorClient *m_client = nullptr;
    class QCheckBox *m_enabledCheck = nullptr;
    class QCheckBox *m_dualTrackCheck = nullptr;
    class QLineEdit *m_directoryEdit = nullptr;
    class QLineEdit *m_filenameEdit = nullptr;
    class QLabel *m_previewLabel = nullptr;
};
