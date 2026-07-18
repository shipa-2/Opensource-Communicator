#pragma once

#include <QObject>
#include <QImage>
#include <QByteArray>
#include <cstdint>

namespace itl {

class H264Encoder : public QObject {
    Q_OBJECT

public:
    explicit H264Encoder(QObject *parent = nullptr);
    ~H264Encoder() override;

    bool open(int width, int height, int fps = 30, int bitrate = 500000);
    void close();
    bool isOpen() const;

    QByteArray encode(const QImage &frame);

signals:
    void error(const QString &message);

private:
    struct EncoderData;
    EncoderData *m_data = nullptr;
    int m_width = 0;
    int m_height = 0;
    int m_frameNum = 0;
};

} // namespace itl
