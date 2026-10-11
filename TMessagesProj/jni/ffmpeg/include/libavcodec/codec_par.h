#ifndef AVCODEC_CODEC_PAR_H
#define AVCODEC_CODEC_PAR_H

#include <stdint.h>

#include "libavutil/avutil.h"
#include "libavutil/channel_layout.h"
#include "libavutil/rational.h"
#include "libavutil/pixfmt.h"

#include "codec_id.h"
#include "defs.h"
#include "packet.h"

typedef struct AVCodecParameters {

    enum AVMediaType codec_type;

    enum AVCodecID   codec_id;

    uint32_t         codec_tag;

    uint8_t *extradata;

    int      extradata_size;

    AVPacketSideData *coded_side_data;

    int nb_coded_side_data;

    int format;

    int64_t bit_rate;

    int bits_per_coded_sample;

    int bits_per_raw_sample;

    int profile;
    int level;

    int width;
    int height;

    AVRational sample_aspect_ratio;

    AVRational framerate;

    enum AVFieldOrder                  field_order;

    enum AVColorRange                  color_range;
    enum AVColorPrimaries              color_primaries;
    enum AVColorTransferCharacteristic color_trc;
    enum AVColorSpace                  color_space;
    enum AVChromaLocation              chroma_location;

    int video_delay;

    AVChannelLayout ch_layout;

    int      sample_rate;

    int      block_align;

    int      frame_size;

    int initial_padding;

    int trailing_padding;

    int seek_preroll;

    enum AVAlphaMode alpha_mode;
} AVCodecParameters;

AVCodecParameters *avcodec_parameters_alloc(void);

void avcodec_parameters_free(AVCodecParameters **par);

int avcodec_parameters_copy(AVCodecParameters *dst, const AVCodecParameters *src);

int av_get_audio_frame_duration2(AVCodecParameters *par, int frame_bytes);

#endif
