#pragma once

#include <QDialog>
#include <QPixmap>

class QLabel;
class QKeyEvent;
class QLineEdit;
class QPushButton;
class QTextEdit;
class QTimer;
class QHBoxLayout;
class QVBoxLayout;
class QWidget;

class DialKeypadWidget;
namespace itl {
class VideoRenderer;
}

class CallWindow : public QDialog {
    Q_OBJECT

public:
    enum class Mode { Hidden, Outgoing, Active, Incoming, IncomingAccepted };

    explicit CallWindow(QWidget *parent = nullptr);

    void showOutgoing(const QString &peer, const QString &displayName, const QString &detail);
    void showIncoming(const QString &peer, const QString &displayName, const QString &detail);
    void showActive(const QString &peer, const QString &displayName);
    void updateState(const QString &state, const QString &detail);
    void beginConversationTimer();
    void closeCall();
    QString peer() const { return m_peer; }
    void setNotesText(const QString &text);
    QString notesText() const;
    void setNotesVisible(bool visible);
    void setAvatarColor(const QString &color);
    void setAvatarLetter(const QString &displayName);
    void setAvatarPixmap(const QPixmap &pixmap);
    void setRemoteSpeakingIndicator(bool speaking);
    void updateRemoteAudioLevel(float level);
    void resetAudioLevel();
    void refreshAppearance();
    void appendDtmfDigit(const QString &digit);
    void setVideoCall(bool enabled);
    void setVideoSending(bool enabled);
    void setScreenSharing(bool enabled);
    void setVideoBlur(bool enabled);

public slots:
    void setRemoteVideoFrame(const QImage &frame);
    void setLocalVideoFrame(const QImage &frame);

signals:
    void answerRequested();
    void hangupRequested();
    void holdRequested();
    void transferRequested();
    void dtmfRequested(const QString &digit);
    void notesChanged(const QString &peer, const QString &text);
    void videoSendingRequested(bool enabled);
    void screenSharingRequested(bool enabled);
    void videoBlurRequested(bool enabled);

protected:
    void reject() override;
    void keyPressEvent(QKeyEvent *event) override;

private:
    void buildUi();
    void setMode(Mode mode);
    void refreshAvatarBorder();
    void refreshAvatarContent();
    void startTimer();
    void stopTimer();
    void onTimerTick();
    void setDtmfPanelVisible(bool visible);
    void sendDtmfDigit(const QString &digit);
    void updateWindowHeightForDtmf(bool expanded);
    void resetCallWindowLayout();
    void applyFixedCallWidth();
    void updateCollapsedMinimumHeight();
    void updateHoldButtonEnabled();
    static QString formatDuration(int seconds);

    static constexpr int kNormalWidth = 320;
    static constexpr int kNormalHeight = 480;

    Mode m_mode = Mode::Hidden;
    QString m_peer;
    QString m_displayName;

    QWidget *m_avatar = nullptr;
    QWidget *m_avatarPanel = nullptr;
    QVBoxLayout *m_rootLayout = nullptr;
    QWidget *m_videoInfoRow = nullptr;
    QHBoxLayout *m_videoInfoLayout = nullptr;
    QWidget *m_videoPanel = nullptr;
    QVBoxLayout *m_videoSideLayout = nullptr;
    itl::VideoRenderer *m_remoteVideo = nullptr;
    itl::VideoRenderer *m_localVideo = nullptr;
    QLabel *m_nameLabel = nullptr;
    QLabel *m_detailLabel = nullptr;
    QLabel *m_statusLabel = nullptr;
    QLabel *m_timerLabel = nullptr;
    QPushButton *m_answerBtn = nullptr;
    QPushButton *m_hangupBtn = nullptr;
    QPushButton *m_holdBtn = nullptr;
    QPushButton *m_transferBtn = nullptr;
    QPushButton *m_dtmfToggleBtn = nullptr;
    QPushButton *m_videoToggleBtn = nullptr;
    QPushButton *m_screenShareBtn = nullptr;
    QPushButton *m_videoBlurBtn = nullptr;
    QWidget *m_dtmfPanel = nullptr;
    QLineEdit *m_dtmfEdit = nullptr;
    DialKeypadWidget *m_dtmfKeypad = nullptr;
    QTextEdit *m_notesEdit = nullptr;
    QTimer *m_durationTimer = nullptr;
    int m_elapsedSeconds = 0;
    QString m_avatarBaseColor;
    QString m_avatarLetter = QStringLiteral("?");
    QPixmap m_avatarPhoto;
    float m_noiseFloor = 0.005f;
    float m_speechThreshold = 0.02f;
    int m_calibrationSamples = 0;
    float m_calibrationSum = 0;
    bool m_calibrated = false;
    bool m_speaking = false;
    bool m_dtmfEnabled = false;
    bool m_dtmfExpanded = false;
    int m_minCollapsedHeight = kNormalHeight;
    int m_collapsedHeight = kNormalHeight;
    QString m_dtmfSent;
    bool m_remoteOnHold = false;
    bool m_videoCall = false;
    bool m_videoBlur = false;
};
