#pragma once

#include <QDialog>

class QLabel;
class QPushButton;
class QTextEdit;
class QTimer;
class QWidget;

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
    void updateRemoteAudioLevel(float level);
    void resetAudioLevel();
    void refreshAppearance();

signals:
    void answerRequested();
    void hangupRequested();
    void holdRequested();
    void transferRequested();
    void notesChanged(const QString &peer, const QString &text);

protected:
    void reject() override;

private:
    void buildUi();
    void setMode(Mode mode);
    void refreshAvatarBorder();
    void startTimer();
    void stopTimer();
    void onTimerTick();
    static QString formatDuration(int seconds);

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
};
