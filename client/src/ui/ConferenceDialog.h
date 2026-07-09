#pragma once

#include "calls/CallManager.h"

#include <QDialog>
#include <QHash>
#include <QString>

class QLineEdit;
class QListWidget;
class QListWidgetItem;
class QPushButton;

class ConferenceDialog : public QDialog {
    Q_OBJECT

public:
    ConferenceDialog(const QHash<QString, QString> &peerNames, const QString &selfPeer, QWidget *parent = nullptr);

    QString subject() const;
    QList<itl::ConferenceParticipant> participants() const;

private:
    enum class Target { Available, Speakers, Listeners };

    void onAccepted();
    void moveSelection(QListWidget *from, QListWidget *to);
    void moveAll(QListWidget *from, QListWidget *to);
    void applySearchFilter(const QString &text);
    QListWidgetItem *takeItemCopy(QListWidget *from, int row);

    QString m_selfPeer;
    QHash<QString, QString> m_peerNames;

    QLineEdit *m_subjectEdit = nullptr;
    QLineEdit *m_searchEdit = nullptr;
    QListWidget *m_availableList = nullptr;
    QListWidget *m_speakersList = nullptr;
    QListWidget *m_listenersList = nullptr;
};
