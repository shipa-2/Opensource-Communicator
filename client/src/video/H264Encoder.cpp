#include "H264Encoder.h"

#include <QLoggingCategory>

#include <wels/codec_api.h>

Q_LOGGING_CATEGORY(lcH264Enc, "itl.video.h264enc")

namespace itl {

struct H264Encoder::EncoderData {
    ISVCEncoder *encoder = nullptr;
};

H264Encoder::H264Encoder(QObject *parent)
    : QObject(parent)
    , m_data(new EncoderData)
{
}

H264Encoder::~H264Encoder()
{
    close();
    delete m_data;
}

bool H264Encoder::open(int width, int height, int fps, int bitrate)
{
    if (m_data->encoder) {
        close();
    }

    int ret = WelsCreateSVCEncoder(&m_data->encoder);
    if (ret != cmResultSuccess || !m_data->encoder) {
        qCCritical(lcH264Enc) << "Failed to create OpenH264 encoder:" << ret;
        return false;
    }

    SEncParamExt param;
    memset(&param, 0, sizeof(param));
    ret = m_data->encoder->GetDefaultParams(&param);
    if (ret != cmResultSuccess) {
        qCCritical(lcH264Enc) << "Failed to get default encoder parameters:" << ret;
        WelsDestroySVCEncoder(m_data->encoder);
        m_data->encoder = nullptr;
        return false;
    }
    param.iUsageType = CAMERA_VIDEO_REAL_TIME;
    param.fMaxFrameRate = fps;
    param.iTargetBitrate = bitrate;
    param.iRCMode = RC_BITRATE_MODE;
    param.iPicWidth = width;
    param.iPicHeight = height;
    param.iTemporalLayerNum = 1;
    param.iSpatialLayerNum = 1;
    param.uiIntraPeriod = qMax(1, fps * 2);
    param.bEnableFrameSkip = true;
    param.iMultipleThreadIdc = 1;
    SSpatialLayerConfig &layer = param.sSpatialLayers[0];
    layer.iVideoWidth = width;
    layer.iVideoHeight = height;
    layer.fFrameRate = fps;
    layer.iSpatialBitrate = bitrate;
    layer.iMaxSpatialBitrate = bitrate;
    layer.uiProfileIdc = PRO_BASELINE;
    layer.uiLevelIdc = LEVEL_UNKNOWN;
    layer.sSliceArgument.uiSliceMode = SM_SINGLE_SLICE;

    ret = m_data->encoder->InitializeExt(&param);
    if (ret != cmResultSuccess) {
        qCCritical(lcH264Enc) << "Failed to initialize encoder:" << ret;
        WelsDestroySVCEncoder(m_data->encoder);
        m_data->encoder = nullptr;
        return false;
    }

    m_width = width;
    m_height = height;
    m_frameNum = 0;

    qCInfo(lcH264Enc) << "H264 encoder opened:" << width << "x" << height << "@" << fps << "fps" << bitrate << "bps";
    return true;
}

void H264Encoder::close()
{
    if (m_data->encoder) {
        m_data->encoder->Uninitialize();
        WelsDestroySVCEncoder(m_data->encoder);
        m_data->encoder = nullptr;
    }
    m_frameNum = 0;
}

QByteArray H264Encoder::encode(const QImage &frame)
{
    if (!m_data->encoder) {
        return {};
    }

    QImage converted = frame.convertToFormat(QImage::Format_RGB888);
    if (converted.width() != m_width || converted.height() != m_height) {
        converted = converted.scaled(m_width, m_height, Qt::IgnoreAspectRatio, Qt::FastTransformation);
    }

    SSourcePicture srcPic;
    memset(&srcPic, 0, sizeof(srcPic));
    srcPic.iPicWidth = m_width;
    srcPic.iPicHeight = m_height;
    srcPic.iColorFormat = videoFormatI420;
    srcPic.uiTimeStamp = m_frameNum * 100;

    const int ySize = m_width * m_height;
    const int uvSize = ySize / 4;
    QByteArray yuvStorage(ySize + uvSize * 2, Qt::Uninitialized);
    auto *yuv = reinterpret_cast<uint8_t *>(yuvStorage.data());
    srcPic.pData[0] = yuv;
    srcPic.pData[1] = yuv + ySize;
    srcPic.pData[2] = yuv + ySize + uvSize;
    srcPic.iStride[0] = m_width;
    srcPic.iStride[1] = m_width / 2;
    srcPic.iStride[2] = m_width / 2;

    for (int j = 0; j < m_height; ++j) {
        const uchar *rgb = converted.constScanLine(j);
        for (int i = 0; i < m_width; ++i) {
            int idx = i * 3;
            int r = rgb[idx], g = rgb[idx + 1], b = rgb[idx + 2];
            int y = qBound(0, ((66 * r + 129 * g + 25 * b + 128) >> 8) + 16, 255);
            int u = qBound(0, ((-38 * r - 74 * g + 112 * b + 128) >> 8) + 128, 255);
            int v = qBound(0, ((112 * r - 94 * g - 18 * b + 128) >> 8) + 128, 255);
            srcPic.pData[0][j * m_width + i] = y;
            if (j % 2 == 0 && i % 2 == 0) {
                srcPic.pData[1][(j / 2) * (m_width / 2) + (i / 2)] = u;
                srcPic.pData[2][(j / 2) * (m_width / 2) + (i / 2)] = v;
            }
        }
    }

    SFrameBSInfo bsInfo;
    memset(&bsInfo, 0, sizeof(bsInfo));
    int ret = m_data->encoder->EncodeFrame(&srcPic, &bsInfo);

    if (ret != cmResultSuccess) {
        qCWarning(lcH264Enc) << "EncodeFrame failed:" << ret;
        return {};
    }

    QByteArray nalData;
    for (int layer = 0; layer < bsInfo.iLayerNum; ++layer) {
        const SLayerBSInfo &li = bsInfo.sLayerInfo[layer];
        int offset = 0;
        for (int nal = 0; nal < li.iNalCount; ++nal) {
            nalData.append(reinterpret_cast<const char *>(li.pBsBuf + offset), li.pNalLengthInByte[nal]);
            offset += li.pNalLengthInByte[nal];
        }
    }

    m_frameNum++;
    return nalData;
}

} // namespace itl
