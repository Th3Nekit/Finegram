#ifndef AVCODEC_CODEC_H
#define AVCODEC_CODEC_H

#include <stdint.h>

#include "libavutil/avutil.h"
#include "libavutil/hwcontext.h"
#include "libavutil/log.h"
#include "libavutil/pixfmt.h"
#include "libavutil/rational.h"
#include "libavutil/samplefmt.h"

#include "libavcodec/codec_id.h"
#include "libavcodec/version_major.h"

#define AV_CODEC_CAP_DRAW_HORIZ_BAND     (1 <<  0)

#define AV_CODEC_CAP_DR1                 (1 <<  1)

#define AV_CODEC_CAP_DELAY               (1 <<  5)

#define AV_CODEC_CAP_SMALL_LAST_FRAME    (1 <<  6)

#define AV_CODEC_CAP_EXPERIMENTAL        (1 <<  9)

#define AV_CODEC_CAP_CHANNEL_CONF        (1 << 10)

#define AV_CODEC_CAP_FRAME_THREADS       (1 << 12)

#define AV_CODEC_CAP_SLICE_THREADS       (1 << 13)

#define AV_CODEC_CAP_PARAM_CHANGE        (1 << 14)

#define AV_CODEC_CAP_OTHER_THREADS       (1 << 15)

#define AV_CODEC_CAP_VARIABLE_FRAME_SIZE (1 << 16)

#define AV_CODEC_CAP_AVOID_PROBING       (1 << 17)

#define AV_CODEC_CAP_HARDWARE            (1 << 18)

#define AV_CODEC_CAP_HYBRID              (1 << 19)

#define AV_CODEC_CAP_ENCODER_REORDERED_OPAQUE (1 << 20)

#define AV_CODEC_CAP_ENCODER_FLUSH   (1 << 21)

#define AV_CODEC_CAP_ENCODER_RECON_FRAME (1 << 22)

typedef struct AVProfile {
    int profile;
    const char *name;
} AVProfile;

typedef struct AVCodec {

    const char *name;

    const char *long_name;
    enum AVMediaType type;
    enum AVCodecID id;

    int capabilities;
    uint8_t max_lowres;

    attribute_deprecated
    const AVRational *supported_framerates;
    attribute_deprecated
    const enum AVPixelFormat *pix_fmts;
    attribute_deprecated
    const int *supported_samplerates;
    attribute_deprecated
    const enum AVSampleFormat *sample_fmts;

    const AVClass *priv_class;
    const AVProfile *profiles;

    const char *wrapper_name;

    attribute_deprecated
    const AVChannelLayout *ch_layouts;
} AVCodec;

const AVCodec *av_codec_iterate(void **opaque);

const AVCodec *avcodec_find_decoder(enum AVCodecID id);

const AVCodec *avcodec_find_decoder_by_name(const char *name);

const AVCodec *avcodec_find_encoder(enum AVCodecID id);

const AVCodec *avcodec_find_encoder_by_name(const char *name);

int av_codec_is_encoder(const AVCodec *codec);

int av_codec_is_decoder(const AVCodec *codec);

const char *av_get_profile_name(const AVCodec *codec, int profile);

enum {

    AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX = 0x01,

    AV_CODEC_HW_CONFIG_METHOD_HW_FRAMES_CTX = 0x02,

    AV_CODEC_HW_CONFIG_METHOD_INTERNAL      = 0x04,

    AV_CODEC_HW_CONFIG_METHOD_AD_HOC        = 0x08,
};

typedef struct AVCodecHWConfig {

    enum AVPixelFormat pix_fmt;

    int methods;

    enum AVHWDeviceType device_type;
} AVCodecHWConfig;

const AVCodecHWConfig *avcodec_get_hw_config(const AVCodec *codec, int index);

#endif
