#include "DarwinFFMpeg.h"

extern "C" {
#include <libavutil/frame.h>
#include <libavutil/pixfmt.h>
#include <libavcodec/avcodec.h>
}

#import "ExtractCVPixelBuffer.h"

namespace tgcalls {

void setupDarwinVideoDecoding(AVCodecContext *codecContext) {
    return;

}

webrtc::scoped_refptr<webrtc::VideoFrameBuffer> createDarwinPlatformFrameFromData(AVFrame const *frame) {
    if (!frame) {
        return nullptr;
    }
    if (frame->format == AV_PIX_FMT_VIDEOTOOLBOX && frame->data[3]) {
        return extractCVPixelBuffer((void *)frame->data[3]);
    } else {
        return nullptr;
    }
}

}
