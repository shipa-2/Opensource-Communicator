#pragma once

#include <QPair>
#include <QVector>
#include <QWidget>

class QLabel;
class QPushButton;

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
    void setChatButtonVisible(bool visible);
    void setSelected(bool selected);
    void refreshAppearance();

protected:
    void changeEvent(QEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

signals:
    void callRequested(const QString &peer);
    void callNumberRequested(const QString &number);
    void chatRequested(const QString &peer);
    void notesRequested(const QString &peer);
    void deleteRequested(const QString &peer);
    void exportRequested(const QString &peer);

private:
    void refreshAvatarStyle();
    void refreshStatusDot();
    void refreshTextLabels();
    void showCallMenu(const QPoint &globalPos);
    bool isInteractiveChild(QWidget *target) const;

    QString m_peer;
    QString m_presence;
    bool m_isSelf = false;
    bool m_canDelete = false;
    QVector<CallNumber> m_numbers;

    QLabel *m_avatar = nullptr;
    QLabel *m_statusDot = nullptr;
    QLabel *m_nameLabel = nullptr;
    QLabel *m_numberLabel = nullptr;
    QPushButton *m_callBtn = nullptr;
    QPushButton *m_chatBtn = nullptr;
};
