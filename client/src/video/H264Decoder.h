#pragma once

#include <QObject>
#include <QImage>
#include <QByteArray>

namespace itl {

class H264Decoder : public QObject {
    Q_OBJECT

public:
    explicit H264Decoder(QObject *parent = nullptr);
    ~H264Decoder() override;

    bool open();
    void close();
    bool isOpen() const;

    QImage decode(const QByteArray &nalu);

signals:
    void frameDecoded(const QImage &frame);
    void error(const QString &message);

private:
    struct DecoderContext;
    DecoderContext *m_ctx = nullptr;
};

} // namespace itl
