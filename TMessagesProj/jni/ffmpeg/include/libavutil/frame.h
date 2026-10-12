#ifndef AVUTIL_FRAME_H
#define AVUTIL_FRAME_H

#include <stddef.h>
#include <stdint.h>

#include "avutil.h"
#include "buffer.h"
#include "channel_layout.h"
#include "dict.h"
#include "rational.h"
#include "samplefmt.h"
#include "pixfmt.h"
#include "version.h"

enum AVFrameSideDataType {

    AV_FRAME_DATA_PANSCAN,

    AV_FRAME_DATA_A53_CC,

    AV_FRAME_DATA_STEREO3D,

    AV_FRAME_DATA_MATRIXENCODING,

    AV_FRAME_DATA_DOWNMIX_INFO,

    AV_FRAME_DATA_REPLAYGAIN,

    AV_FRAME_DATA_DISPLAYMATRIX,

    AV_FRAME_DATA_AFD,

    AV_FRAME_DATA_MOTION_VECTORS,

    AV_FRAME_DATA_SKIP_SAMPLES,

    AV_FRAME_DATA_AUDIO_SERVICE_TYPE,

    AV_FRAME_DATA_MASTERING_DISPLAY_METADATA,

    AV_FRAME_DATA_GOP_TIMECODE,

    AV_FRAME_DATA_SPHERICAL,

    AV_FRAME_DATA_CONTENT_LIGHT_LEVEL,

    AV_FRAME_DATA_ICC_PROFILE,

    AV_FRAME_DATA_S12M_TIMECODE,

    AV_FRAME_DATA_DYNAMIC_HDR_PLUS,

    AV_FRAME_DATA_REGIONS_OF_INTEREST,

    AV_FRAME_DATA_VIDEO_ENC_PARAMS,

    AV_FRAME_DATA_SEI_UNREGISTERED,

    AV_FRAME_DATA_FILM_GRAIN_PARAMS,

    AV_FRAME_DATA_DETECTION_BBOXES,

    AV_FRAME_DATA_DOVI_RPU_BUFFER,

    AV_FRAME_DATA_DOVI_METADATA,

    AV_FRAME_DATA_DYNAMIC_HDR_VIVID,

    AV_FRAME_DATA_AMBIENT_VIEWING_ENVIRONMENT,

    AV_FRAME_DATA_VIDEO_HINT,

    AV_FRAME_DATA_LCEVC,

    AV_FRAME_DATA_VIEW_ID,

    AV_FRAME_DATA_3D_REFERENCE_DISPLAYS,

     AV_FRAME_DATA_EXIF,
};

enum AVActiveFormatDescription {
    AV_AFD_SAME         = 8,
    AV_AFD_4_3          = 9,
    AV_AFD_16_9         = 10,
    AV_AFD_14_9         = 11,
    AV_AFD_4_3_SP_14_9  = 13,
    AV_AFD_16_9_SP_14_9 = 14,
    AV_AFD_SP_4_3       = 15,
};

typedef struct AVFrameSideData {
    enum AVFrameSideDataType type;
    uint8_t *data;
    size_t   size;
    AVDictionary *metadata;
    AVBufferRef *buf;
} AVFrameSideData;

enum AVSideDataProps {

    AV_SIDE_DATA_PROP_GLOBAL = (1 << 0),

    AV_SIDE_DATA_PROP_MULTI  = (1 << 1),

    AV_SIDE_DATA_PROP_SIZE_DEPENDENT = (1 << 2),

    AV_SIDE_DATA_PROP_COLOR_DEPENDENT = (1 << 3),

    AV_SIDE_DATA_PROP_CHANNEL_DEPENDENT = (1 << 4),
};

typedef struct AVSideDataDescriptor {

    const char      *name;

    unsigned         props;
} AVSideDataDescriptor;

typedef struct AVRegionOfInterest {

    uint32_t self_size;

    int top;
    int bottom;
    int left;
    int right;

    AVRational qoffset;
} AVRegionOfInterest;

typedef struct AVFrame {
#define AV_NUM_DATA_POINTERS 8

    uint8_t *data[AV_NUM_DATA_POINTERS];

    int linesize[AV_NUM_DATA_POINTERS];

    uint8_t **extended_data;

    int width, height;

    int nb_samples;

    int format;

    enum AVPictureType pict_type;

    AVRational sample_aspect_ratio;

    int64_t pts;

    int64_t pkt_dts;

    AVRational time_base;

    int quality;

    void *opaque;

    int repeat_pict;

    int sample_rate;

    AVBufferRef *buf[AV_NUM_DATA_POINTERS];

    AVBufferRef **extended_buf;

    int        nb_extended_buf;

    AVFrameSideData **side_data;
    int            nb_side_data;

#define AV_FRAME_FLAG_CORRUPT       (1 << 0)

#define AV_FRAME_FLAG_KEY (1 << 1)

#define AV_FRAME_FLAG_DISCARD   (1 << 2)

#define AV_FRAME_FLAG_INTERLACED (1 << 3)

#define AV_FRAME_FLAG_TOP_FIELD_FIRST (1 << 4)

#define AV_FRAME_FLAG_LOSSLESS        (1 << 5)

    int flags;

    enum AVColorRange color_range;

    enum AVColorPrimaries color_primaries;

    enum AVColorTransferCharacteristic color_trc;

    enum AVColorSpace colorspace;

    enum AVChromaLocation chroma_location;

    int64_t best_effort_timestamp;

    AVDictionary *metadata;

    int decode_error_flags;
#define FF_DECODE_ERROR_INVALID_BITSTREAM   1
#define FF_DECODE_ERROR_MISSING_REFERENCE   2
#define FF_DECODE_ERROR_CONCEALMENT_ACTIVE  4
#define FF_DECODE_ERROR_DECODE_SLICES       8

    AVBufferRef *hw_frames_ctx;

    AVBufferRef *opaque_ref;

    size_t crop_top;
    size_t crop_bottom;
    size_t crop_left;
    size_t crop_right;

    void *private_ref;

    AVChannelLayout ch_layout;

    int64_t duration;

    enum AVAlphaMode alpha_mode;
} AVFrame;

AVFrame *av_frame_alloc(void);

void av_frame_free(AVFrame **frame);

int av_frame_ref(AVFrame *dst, const AVFrame *src);

int av_frame_replace(AVFrame *dst, const AVFrame *src);

AVFrame *av_frame_clone(const AVFrame *src);

void av_frame_unref(AVFrame *frame);

void av_frame_move_ref(AVFrame *dst, AVFrame *src);

int av_frame_get_buffer(AVFrame *frame, int align);

int av_frame_is_writable(AVFrame *frame);

int av_frame_make_writable(AVFrame *frame);

int av_frame_copy(AVFrame *dst, const AVFrame *src);

int av_frame_copy_props(AVFrame *dst, const AVFrame *src);

AVBufferRef *av_frame_get_plane_buffer(const AVFrame *frame, int plane);

AVFrameSideData *av_frame_new_side_data(AVFrame *frame,
                                        enum AVFrameSideDataType type,
                                        size_t size);

AVFrameSideData *av_frame_new_side_data_from_buf(AVFrame *frame,
                                                 enum AVFrameSideDataType type,
                                                 AVBufferRef *buf);

AVFrameSideData *av_frame_get_side_data(const AVFrame *frame,
                                        enum AVFrameSideDataType type);

void av_frame_remove_side_data(AVFrame *frame, enum AVFrameSideDataType type);

enum {

    AV_FRAME_CROP_UNALIGNED     = 1 << 0,
};

int av_frame_apply_cropping(AVFrame *frame, int flags);

const char *av_frame_side_data_name(enum AVFrameSideDataType type);

const AVSideDataDescriptor *av_frame_side_data_desc(enum AVFrameSideDataType type);

void av_frame_side_data_free(AVFrameSideData ***sd, int *nb_sd);

#define AV_FRAME_SIDE_DATA_FLAG_UNIQUE (1 << 0)

#define AV_FRAME_SIDE_DATA_FLAG_REPLACE (1 << 1)

#define AV_FRAME_SIDE_DATA_FLAG_NEW_REF (1 << 2)

AVFrameSideData *av_frame_side_data_new(AVFrameSideData ***sd, int *nb_sd,
                                        enum AVFrameSideDataType type,
                                        size_t size, unsigned int flags);

AVFrameSideData *av_frame_side_data_add(AVFrameSideData ***sd, int *nb_sd,
                                        enum AVFrameSideDataType type,
                                        AVBufferRef **buf, unsigned int flags);

int av_frame_side_data_clone(AVFrameSideData ***sd, int *nb_sd,
                             const AVFrameSideData *src, unsigned int flags);

const AVFrameSideData *av_frame_side_data_get_c(const AVFrameSideData * const *sd,
                                                const int nb_sd,
                                                enum AVFrameSideDataType type);

static inline
const AVFrameSideData *av_frame_side_data_get(AVFrameSideData * const *sd,
                                              const int nb_sd,
                                              enum AVFrameSideDataType type)
{
    return av_frame_side_data_get_c((const AVFrameSideData * const *)sd,
                                    nb_sd, type);
}

void av_frame_side_data_remove(AVFrameSideData ***sd, int *nb_sd,
                               enum AVFrameSideDataType type);

void av_frame_side_data_remove_by_props(AVFrameSideData ***sd, int *nb_sd,
                                        int props);

#endif
