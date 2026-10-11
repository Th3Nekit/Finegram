#ifndef AVCODEC_CODEC_DESC_H
#define AVCODEC_CODEC_DESC_H

#include "libavutil/avutil.h"

#include "codec_id.h"

typedef struct AVCodecDescriptor {
    enum AVCodecID     id;
    enum AVMediaType type;

    const char      *name;

    const char *long_name;

    int             props;

    const char *const *mime_types;

    const struct AVProfile *profiles;
} AVCodecDescriptor;

#define AV_CODEC_PROP_INTRA_ONLY    (1 << 0)

#define AV_CODEC_PROP_LOSSY         (1 << 1)

#define AV_CODEC_PROP_LOSSLESS      (1 << 2)

#define AV_CODEC_PROP_REORDER       (1 << 3)

#define AV_CODEC_PROP_FIELDS        (1 << 4)

#define AV_CODEC_PROP_ENHANCEMENT   (1 << 5)

#define AV_CODEC_PROP_BITMAP_SUB    (1 << 16)

#define AV_CODEC_PROP_TEXT_SUB      (1 << 17)

const AVCodecDescriptor *avcodec_descriptor_get(enum AVCodecID id);

const AVCodecDescriptor *avcodec_descriptor_next(const AVCodecDescriptor *prev);

const AVCodecDescriptor *avcodec_descriptor_get_by_name(const char *name);

#endif
