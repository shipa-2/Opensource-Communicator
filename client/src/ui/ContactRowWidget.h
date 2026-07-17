#pragma once

#include <QPair>
#include <QPixmap>
#include <QVector>
#include <QWidget>

class QLabel;
class QPushButton;
class QTimer;
class QEvent;
class QContextMenuEvent;
class QEnterEvent;
class QMouseEvent;
class QPaintEvent;

class ContactRowWidget : public QWidget {
    Q_OBJECT

public:
    // Each call target is a (label, dialNumber) pair shown in the "Позвонить по:" menu.
    using CallNumber = QPair<QString, QString>;

    explicit ContactRowWidget(const QString &peer, const QString &name, const QString &ext,
                              const QString &phone, const QString &presence, bool isSelf,
                              bool canDelete = false, QWidget *parent = nullptr);

    QString peer() const { return m_peer; }
    void updatePresence(const QString &presence);
    void updateName(const QString &name);
    void setCallNumbers(const QVector<CallNumber> &numbers);
    void setPhones(const QString &phone, const QString &personalPhone);
    void setChatButtonVisible(bool visible);
    void setCallButtonVisible(bool visible);
    void setPeerColor(const QString &color);
    void setPeerAvatar(const QPixmap &avatar);
    void setUnreadBlink(bool enabled);
    void setSelected(bool selected);
    void refreshAppearance();

protected:
    void changeEvent(QEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    void enterEvent(QEnterEvent *event) override;
#else
    void enterEvent(QEvent *event) override;
#endif
    void leaveEvent(QEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

signals:
    void callRequested(const QString &peer);
    void callNumberRequested(const QString &number);
    void chatRequested(const QString &peer);
    void notesRequested(const QString &peer);
    void deleteRequested(const QString &peer);
    void exportRequested(const QString &peer);
    void copyNumberRequested(const QString &number);

private:
    void refreshAvatarStyle();
    void refreshStatusDot();
    void refreshTextLabels();
    void refreshChatButtonStyle();
    void refreshBackground();
    void showCallMenu(const QPoint &globalPos);
    bool isInteractiveChild(QWidget *target) const;

    QString m_peer;
    QString m_presence;
    QString m_phone;
    QString m_personalPhone;
    QString m_peerColor;
    QPixmap m_peerAvatar;
    bool m_isSelf = false;
    bool m_canDelete = false;
    bool m_selected = false;
    bool m_hovered = false;
    QVector<CallNumber> m_numbers;

    QWidget *m_avatar = nullptr;
    QLabel *m_statusDot = nullptr;
    QLabel *m_nameLabel = nullptr;
    QLabel *m_numberLabel = nullptr;
    QPushButton *m_callBtn = nullptr;
    QPushButton *m_chatBtn = nullptr;
    QTimer *m_blinkTimer = nullptr;
    bool m_unreadBlink = false;
    bool m_blinkAccentOn = false;
};
