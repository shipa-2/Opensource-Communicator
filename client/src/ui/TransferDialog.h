#pragma once

#include <QDialog>
#include <QHash>
#include <QString>

class QLineEdit;
class QListWidget;
class QPushButton;

class TransferDialog : public QDialog {
    Q_OBJECT

public:
    TransferDialog(const QHash<QString, QString> &peerNames, const QString &selfPeer,
                   const QString &excludePeer = {}, QWidget *parent = nullptr);

    QString selectedPeer() const;
    QString selectedDisplayName() const;

private:
    void rebuildList();
    void updateTransferEnabled();
    void onAccepted();

    QString m_selfPeer;
    QString m_excludePeer;
    QHash<QString, QString> m_peerNames;
    QLineEdit *m_searchEdit = nullptr;
    QListWidget *m_contactsList = nullptr;
    QPushButton *m_transferBtn = nullptr;
};
