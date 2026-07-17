#pragma once

#include <QWidget>

class QGridLayout;
class QLineEdit;
class QPushButton;
class QTimer;

class DialKeypadWidget : public QWidget {
    Q_OBJECT

public:
    explicit DialKeypadWidget(QWidget *parent = nullptr);

    void setLineEdit(QLineEdit *edit);
    void setDtmfMode(bool enabled);
    void setCompact(bool compact);
    void setChromeAlpha(int alpha);
    void refreshAppearance();

signals:
    void digitPressed(const QString &digit);

protected:
    void changeEvent(QEvent *event) override;

private:
    void appendChar(const QString &character);
    void onBackspace();
    void onBackspacePressed();
    void onBackspaceReleased();
    void onBackspaceClicked();
    void onZeroPressed();
    void onZeroReleased();
    void onZeroClicked();
    void onHoldTimeout();
    void onHoldPhaseTimeout();
    void onHoldProgressTick();
    void startHold(QPushButton *button, bool secondaryStyle, bool clearOnHold);
    void endHold();
    void applyButtonStyle(QPushButton *button, bool backspace = false) const;
    void updateHoldVisual();
    QString colorCss(const QColor &color) const;

    enum class HoldVisual {
        None,
        SolidAccent,
        Filling,
    };

    QLineEdit *m_edit = nullptr;
    QPushButton *m_backspaceBtn = nullptr;
    QPushButton *m_zeroBtn = nullptr;
    QPushButton *m_holdBtn = nullptr;
    QTimer *m_holdTimer = nullptr;
    QTimer *m_holdPhaseTimer = nullptr;
    QTimer *m_holdProgressTimer = nullptr;
    HoldVisual m_holdVisual = HoldVisual::None;
    qreal m_holdFillProgress = 0.0;
    bool m_holdSecondaryStyle = false;
    bool m_holdClearOnHold = false;
    bool m_holdActionDone = false;
    QGridLayout *m_grid = nullptr;
    QList<QPushButton *> m_keys;
    bool m_dtmfMode = false;
    bool m_compact = false;
    int m_chromeAlpha = 255;
};
