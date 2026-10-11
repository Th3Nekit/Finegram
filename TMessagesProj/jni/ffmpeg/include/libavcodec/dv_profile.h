#ifndef AVCODEC_DV_PROFILE_H
#define AVCODEC_DV_PROFILE_H

#include <stdint.h>

#include "libavutil/pixfmt.h"
#include "libavutil/rational.h"

#define DV_PROFILE_BYTES (6 * 80)

typedef struct AVDVProfile {
    int              dsf;
    int              video_stype;
    int              frame_size;
    int              difseg_size;
    int              n_difchan;
    AVRational       time_base;
    int              ltc_divisor;
    int              height;
    int              width;
    AVRational       sar[2];
    enum AVPixelFormat pix_fmt;
    int              bpm;
    const uint8_t   *block_sizes;
    int              audio_stride;
    int              audio_min_samples[3];

    int              audio_samples_dist[5];

    const uint8_t  (*audio_shuffle)[9];
} AVDVProfile;

const AVDVProfile *av_dv_frame_profile(const AVDVProfile *sys,
                                       const uint8_t *frame, unsigned buf_size);

const AVDVProfile *av_dv_codec_profile(int width, int height, enum AVPixelFormat pix_fmt);

const AVDVProfile *av_dv_codec_profile2(int width, int height, enum AVPixelFormat pix_fmt, AVRational frame_rate);

#endif
