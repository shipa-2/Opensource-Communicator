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
    void refreshAppearance();

protected:
    void changeEvent(QEvent *event) override;

private:
    void appendChar(const QString &character);
    void onBackspace();
    void onBackspacePressed();
    void onBackspaceReleased();
    void onBackspaceClicked();
    void onBackspaceHoldTimeout();
    void onBackspaceHoldPhaseTimeout();
    void onBackspaceHoldProgressTick();
    void applyButtonStyle(QPushButton *button, bool backspace = false) const;
    void updateBackspaceHoldVisual();

    enum class BackspaceHoldVisual {
        None,
        SolidAccent,
        Filling,
    };

    QLineEdit *m_edit = nullptr;
    QPushButton *m_backspaceBtn = nullptr;
    QTimer *m_backspaceHoldTimer = nullptr;
    QTimer *m_backspaceHoldPhaseTimer = nullptr;
    QTimer *m_backspaceHoldProgressTimer = nullptr;
    BackspaceHoldVisual m_backspaceHoldVisual = BackspaceHoldVisual::None;
    qreal m_backspaceFillProgress = 0.0;
    bool m_backspaceHoldClearDone = false;
    QGridLayout *m_grid = nullptr;
    QList<QPushButton *> m_keys;
};
