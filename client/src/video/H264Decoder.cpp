#include "H264Decoder.h"

#include <QLoggingCategory>

extern "C" {
#include <wels/codec_api.h>
}

Q_LOGGING_CATEGORY(lcH264Dec, "itl.video.h264dec")

namespace itl {

struct H264Decoder::DecoderContext {
    ISVCDecoder *decoder = nullptr;
};

H264Decoder::H264Decoder(QObject *parent)
    : QObject(parent)
    , m_ctx(new DecoderContext)
{
}

H264Decoder::~H264Decoder()
{
    close();
    delete m_ctx;
}

bool H264Decoder::open()
{
    if (m_ctx->decoder) {
        return true;
    }

    if (WelsCreateDecoder(&m_ctx->decoder) != 0 || !m_ctx->decoder) {
        qCCritical(lcH264Dec) << "Failed to create OpenH264 decoder";
        m_ctx->decoder = nullptr;
        return false;
    }

    SDecodingParam parameters{};
    parameters.sVideoProperty.size = sizeof(SVideoProperty);
    parameters.sVideoProperty.eVideoBsType = VIDEO_BITSTREAM_AVC;
    parameters.bParseOnly = false;
    if (m_ctx->decoder->Initialize(&parameters) != 0) {
        qCCritical(lcH264Dec) << "Failed to initialize OpenH264 decoder";
        WelsDestroyDecoder(m_ctx->decoder);
        m_ctx->decoder = nullptr;
        return false;
    }

    qCInfo(lcH264Dec) << "OpenH264 decoder opened";
    return true;
}

void H264Decoder::close()
{
    if (m_ctx->decoder) {
        m_ctx->decoder->Uninitialize();
        WelsDestroyDecoder(m_ctx->decoder);
        m_ctx->decoder = nullptr;
    }
}

bool H264Decoder::isOpen() const
{
    return m_ctx && m_ctx->decoder;
}

QImage H264Decoder::decode(const QByteArray &nalu)
{
    if (!m_ctx->decoder || nalu.isEmpty()) {
        return {};
    }

    unsigned char *planes[3]{};
    SBufferInfo bufferInfo{};
    const DECODING_STATE state = m_ctx->decoder->DecodeFrameNoDelay(
        reinterpret_cast<const unsigned char *>(nalu.constData()),
        nalu.size(),
        planes,
        &bufferInfo);
    if (state != dsErrorFree) {
        qCWarning(lcH264Dec) << "OpenH264 decode state:" << static_cast<int>(state);
    }
    if (bufferInfo.iBufferStatus != 1 || !planes[0] || !planes[1] || !planes[2]) {
        return {};
    }

    const int width = bufferInfo.UsrData.sSystemBuffer.iWidth;
    const int height = bufferInfo.UsrData.sSystemBuffer.iHeight;
    const int yStride = bufferInfo.UsrData.sSystemBuffer.iStride[0];
    const int uvStride = bufferInfo.UsrData.sSystemBuffer.iStride[1];
    if (width <= 0 || height <= 0 || yStride <= 0 || uvStride <= 0) {
        return {};
    }

    QImage image(width, height, QImage::Format_RGB888);
    if (image.isNull()) {
        return {};
    }

    const auto clamp = [](int value) {
        return static_cast<uchar>(value < 0 ? 0 : (value > 255 ? 255 : value));
    };
    for (int y = 0; y < height; ++y) {
        uchar *destination = image.scanLine(y);
        const unsigned char *yRow = planes[0] + y * yStride;
        const unsigned char *uRow = planes[1] + (y / 2) * uvStride;
        const unsigned char *vRow = planes[2] + (y / 2) * uvStride;
        for (int x = 0; x < width; ++x) {
            const int luminance = qMax(0, static_cast<int>(yRow[x]) - 16);
            const int u = static_cast<int>(uRow[x / 2]) - 128;
            const int v = static_cast<int>(vRow[x / 2]) - 128;
            destination[x * 3] = clamp((298 * luminance + 409 * v + 128) >> 8);
            destination[x * 3 + 1] =
                clamp((298 * luminance - 100 * u - 208 * v + 128) >> 8);
            destination[x * 3 + 2] = clamp((298 * luminance + 516 * u + 128) >> 8);
        }
    }

    return image;
}

} // namespace itl
