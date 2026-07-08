#pragma once

#include <QWidget>

class QLabel;
class QPushButton;

class ContactRowWidget : public QWidget {
    Q_OBJECT

public:
    explicit ContactRowWidget(const QString &peer, const QString &name, const QString &ext,
                              const QString &phone, const QString &presence, bool isSelf,
                              QWidget *parent = nullptr);

    QString peer() const { return m_peer; }
    void updatePresence(const QString &presence);
    void updateName(const QString &name);
    void setSelected(bool selected);
    void refreshAppearance();

protected:
    void changeEvent(QEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;

signals:
    void callRequested(const QString &peer);
    void chatRequested(const QString &peer);
    void notesRequested(const QString &peer);

private:
    void refreshAvatarStyle();
    void refreshStatusDot();
    void refreshTextLabels();

    QString m_peer;
    QString m_presence;
    bool m_isSelf = false;

    QLabel *m_avatar = nullptr;
    QLabel *m_statusDot = nullptr;
    QLabel *m_nameLabel = nullptr;
    QLabel *m_numberLabel = nullptr;
    QPushButton *m_callBtn = nullptr;
    QPushButton *m_chatBtn = nullptr;
};
