#pragma once

#include <QObject>
#include <QSslCertificate>
#include <QSslKey>
#include <QString>

namespace itl {

class WsSession;

class WsServer : public QObject {
    Q_OBJECT

public:
    explicit WsServer(QObject *parent = nullptr);
    ~WsServer() override;

    bool start(const QString &host, quint16 port);
    bool startTls(const QString &host, quint16 port,
                  const QString &certPath, const QString &keyPath);
    void stop();
    bool isListening() const;

signals:
    void newSession(WsSession *session);
    void sessionClosed(WsSession *session);

private slots:
    void onNewConnection();
    void onSocketDisconnected();

private:
    class Impl;
    Impl *m_impl;
};

} // namespace itl
