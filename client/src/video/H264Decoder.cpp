#include "H264Decoder.h"

#include <QLoggingCategory>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}

Q_LOGGING_CATEGORY(lcH264Dec, "itl.video.h264dec")

namespace itl {

struct H264Decoder::DecoderContext {
    AVCodecContext *codecCtx = nullptr;
    AVFrame *frame = nullptr;
    AVFrame *rgbFrame = nullptr;
    uint8_t *rgbBuffer = nullptr;
    SwsContext *swsCtx = nullptr;
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
    if (m_ctx->codecCtx) {
        return true;
    }

    const AVCodec *codec = avcodec_find_decoder(AV_CODEC_ID_H264);
    if (!codec) {
        qCCritical(lcH264Dec) << "H264 decoder not found";
        return false;
    }

    m_ctx->codecCtx = avcodec_alloc_context3(codec);
    if (!m_ctx->codecCtx) {
        qCCritical(lcH264Dec) << "Failed to allocate decoder context";
        return false;
    }

    m_ctx->codecCtx->thread_count = 2;

    if (avcodec_open2(m_ctx->codecCtx, codec, nullptr) < 0) {
        qCCritical(lcH264Dec) << "Failed to open H264 decoder";
        avcodec_free_context(&m_ctx->codecCtx);
        return false;
    }

    m_ctx->frame = av_frame_alloc();
    m_ctx->rgbFrame = av_frame_alloc();

    qCInfo(lcH264Dec) << "H264 decoder opened";
    return true;
}

void H264Decoder::close()
{
    if (m_ctx->swsCtx) {
        sws_freeContext(m_ctx->swsCtx);
        m_ctx->swsCtx = nullptr;
    }
    if (m_ctx->rgbBuffer) {
        av_free(m_ctx->rgbBuffer);
        m_ctx->rgbBuffer = nullptr;
    }
    if (m_ctx->rgbFrame) {
        av_frame_free(&m_ctx->rgbFrame);
    }
    if (m_ctx->frame) {
        av_frame_free(&m_ctx->frame);
    }
    if (m_ctx->codecCtx) {
        avcodec_free_context(&m_ctx->codecCtx);
    }
}

bool H264Decoder::isOpen() const
{
    return m_ctx && m_ctx->codecCtx;
}

QImage H264Decoder::decode(const QByteArray &nalu)
{
    if (!m_ctx->codecCtx || nalu.isEmpty()) {
        return {};
    }

    AVPacket *packet = av_packet_alloc();
    packet->data = reinterpret_cast<uint8_t *>(const_cast<char *>(nalu.constData()));
    packet->size = nalu.size();

    int ret = avcodec_send_packet(m_ctx->codecCtx, packet);
    av_packet_free(&packet);

    if (ret < 0) {
        qCWarning(lcH264Dec) << "Error sending packet to decoder:" << ret;
        return {};
    }

    ret = avcodec_receive_frame(m_ctx->codecCtx, m_ctx->frame);
    if (ret < 0) {
        return {};
    }

    AVFrame *frame = m_ctx->frame;
    int width = frame->width;
    int height = frame->height;

    if (width <= 0 || height <= 0) {
        return {};
    }

    // Setup SWS context for YUV420P → RGB conversion
    if (!m_ctx->swsCtx || m_ctx->codecCtx->width != width || m_ctx->codecCtx->height != height) {
        if (m_ctx->swsCtx) {
            sws_freeContext(m_ctx->swsCtx);
        }
        if (m_ctx->rgbBuffer) {
            av_free(m_ctx->rgbBuffer);
            m_ctx->rgbBuffer = nullptr;
        }

        m_ctx->swsCtx = sws_getContext(
            width, height, AV_PIX_FMT_YUV420P,
            width, height, AV_PIX_FMT_RGB24,
            SWS_BILINEAR, nullptr, nullptr, nullptr);

        if (!m_ctx->swsCtx) {
            qCWarning(lcH264Dec) << "Failed to create SWS context";
            return {};
        }

        int numBytes = av_image_get_buffer_size(AV_PIX_FMT_RGB24, width, height, 1);
        m_ctx->rgbBuffer = static_cast<uint8_t *>(av_malloc(numBytes));
        av_image_fill_arrays(m_ctx->rgbFrame->data, m_ctx->rgbFrame->linesize,
                             m_ctx->rgbBuffer, AV_PIX_FMT_RGB24, width, height, 1);
    }

    // Convert YUV → RGB
    sws_scale(m_ctx->swsCtx,
              frame->data, frame->linesize, 0, height,
              m_ctx->rgbFrame->data, m_ctx->rgbFrame->linesize);

    // Create QImage from RGB buffer
    QImage rgbImage(m_ctx->rgbBuffer, width, height, m_ctx->rgbFrame->linesize[0],
                    QImage::Format_RGB888);
    QImage result = rgbImage.copy();  // Deep copy before buffer reuse

    return result;
}

} // namespace itl
