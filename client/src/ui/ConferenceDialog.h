#pragma once

#include "calls/CallManager.h"

#include <QDialog>
#include <QHash>
#include <QString>

class QListWidget;

class ConferenceDialog : public QDialog {
    Q_OBJECT

public:
    ConferenceDialog(const QHash<QString, QString> &peerNames, const QString &selfPeer, QWidget *parent = nullptr);

    QString subject() const;
    QList<itl::ConferenceParticipant> participants() const;

private:
    void onAccepted();

    QString m_selfPeer;
    QHash<QString, QString> m_peerNames;
    class QLineEdit *m_subjectEdit = nullptr;
    QListWidget *m_participantsList = nullptr;
};
