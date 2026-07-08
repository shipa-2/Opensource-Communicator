#pragma once

#include <QWidget>

class QComboBox;
class QLabel;

class PresenceSelector : public QWidget {
    Q_OBJECT

public:
    explicit PresenceSelector(QWidget *parent = nullptr);

    void setEnabled(bool enabled);
    int currentIndex() const;
    QString currentStatus() const;
    void setCurrentStatus(const QString &status);
    void refreshAppearance();

signals:
    void currentIndexChanged(int index);

private:
    void refreshDot();
    static QString colorForStatus(const QString &status);
    static QString labelForStatus(const QString &status);

    QLabel *m_dot = nullptr;
    QComboBox *m_combo = nullptr;
};
