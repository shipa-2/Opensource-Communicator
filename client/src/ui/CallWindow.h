#pragma once

#include <QDialog>

class QLabel;
class QKeyEvent;
class QLineEdit;
class QPushButton;
class QTextEdit;
class QTimer;
class QWidget;

class DialKeypadWidget;

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
    void setRemoteSpeakingIndicator(bool speaking);
    void updateRemoteAudioLevel(float level);
    void resetAudioLevel();
    void refreshAppearance();
    void appendDtmfDigit(const QString &digit);

signals:
    void answerRequested();
    void hangupRequested();
    void holdRequested();
    void transferRequested();
    void dtmfRequested(const QString &digit);
    void notesChanged(const QString &peer, const QString &text);

protected:
    void reject() override;
    void keyPressEvent(QKeyEvent *event) override;

private:
    void buildUi();
    void setMode(Mode mode);
    void refreshAvatarBorder();
    void startTimer();
    void stopTimer();
    void onTimerTick();
    void setDtmfPanelVisible(bool visible);
    void sendDtmfDigit(const QString &digit);
    void updateWindowHeightForDtmf(bool expanded);
    void resetCallWindowLayout();
    void applyFixedCallWidth();
    void updateCollapsedMinimumHeight();
    static QString formatDuration(int seconds);

    static constexpr int kNormalWidth = 320;
    static constexpr int kNormalHeight = 480;

    Mode m_mode = Mode::Hidden;
    QString m_peer;
    QString m_displayName;

    QLabel *m_avatar = nullptr;
    QLabel *m_nameLabel = nullptr;
    QLabel *m_detailLabel = nullptr;
    QLabel *m_statusLabel = nullptr;
    QLabel *m_timerLabel = nullptr;
    QPushButton *m_answerBtn = nullptr;
    QPushButton *m_hangupBtn = nullptr;
    QPushButton *m_holdBtn = nullptr;
    QPushButton *m_transferBtn = nullptr;
    QPushButton *m_dtmfToggleBtn = nullptr;
    QWidget *m_dtmfPanel = nullptr;
    QLineEdit *m_dtmfEdit = nullptr;
    DialKeypadWidget *m_dtmfKeypad = nullptr;
    QTextEdit *m_notesEdit = nullptr;
    QTimer *m_durationTimer = nullptr;
    int m_elapsedSeconds = 0;
    QString m_avatarBaseColor;
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
};
