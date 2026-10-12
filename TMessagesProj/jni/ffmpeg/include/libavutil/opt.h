/*
 * AVOptions
 * copyright (c) 2005 Michael Niedermayer <michaelni@gmx.at>
 *
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef AVUTIL_OPT_H
#define AVUTIL_OPT_H

#include "rational.h"
#include "avutil.h"
#include "channel_layout.h"
#include "dict.h"
#include "log.h"
#include "pixfmt.h"
#include "samplefmt.h"

enum AVOptionType{

    AV_OPT_TYPE_FLAGS = 1,

    AV_OPT_TYPE_INT,

    AV_OPT_TYPE_INT64,

    AV_OPT_TYPE_DOUBLE,

    AV_OPT_TYPE_FLOAT,

    AV_OPT_TYPE_STRING,

    AV_OPT_TYPE_RATIONAL,

    AV_OPT_TYPE_BINARY,

    AV_OPT_TYPE_DICT,

    AV_OPT_TYPE_UINT64,

    AV_OPT_TYPE_CONST,

    AV_OPT_TYPE_IMAGE_SIZE,

    AV_OPT_TYPE_PIXEL_FMT,

    AV_OPT_TYPE_SAMPLE_FMT,

    AV_OPT_TYPE_VIDEO_RATE,

    AV_OPT_TYPE_DURATION,

    AV_OPT_TYPE_COLOR,

    AV_OPT_TYPE_BOOL,

    AV_OPT_TYPE_CHLAYOUT,

    AV_OPT_TYPE_UINT,

    AV_OPT_TYPE_FLAG_ARRAY = (1 << 16),
};

#define AV_OPT_FLAG_ENCODING_PARAM  (1 << 0)

#define AV_OPT_FLAG_DECODING_PARAM  (1 << 1)
#define AV_OPT_FLAG_AUDIO_PARAM     (1 << 3)
#define AV_OPT_FLAG_VIDEO_PARAM     (1 << 4)
#define AV_OPT_FLAG_SUBTITLE_PARAM  (1 << 5)

#define AV_OPT_FLAG_EXPORT          (1 << 6)

#define AV_OPT_FLAG_READONLY        (1 << 7)

#define AV_OPT_FLAG_BSF_PARAM       (1 << 8)

#define AV_OPT_FLAG_RUNTIME_PARAM   (1 << 15)

#define AV_OPT_FLAG_FILTERING_PARAM (1 << 16)

#define AV_OPT_FLAG_DEPRECATED      (1 << 17)

#define AV_OPT_FLAG_CHILD_CONSTS    (1 << 18)

typedef struct AVOptionArrayDef {

    const char         *def;

    unsigned            size_min;

    unsigned            size_max;

    char                sep;
} AVOptionArrayDef;

typedef struct AVOption {
    const char *name;

    const char *help;

    int offset;
    enum AVOptionType type;

    union {
        int64_t i64;
        double dbl;
        const char *str;

        AVRational q;

        const AVOptionArrayDef *arr;
    } default_val;
    double min;
    double max;

    int flags;

    const char *unit;
} AVOption;

typedef struct AVOptionRange {
    const char *str;

    double value_min, value_max;

    double component_min, component_max;

    int is_range;
} AVOptionRange;

typedef struct AVOptionRanges {

    AVOptionRange **range;

    int nb_ranges;

    int nb_components;
} AVOptionRanges;

void av_opt_set_defaults(void *s);

void av_opt_set_defaults2(void *s, int mask, int flags);

void av_opt_free(void *obj);

const AVOption *av_opt_next(const void *obj, const AVOption *prev);

void *av_opt_child_next(void *obj, void *prev);

const AVClass *av_opt_child_class_iterate(const AVClass *parent, void **iter);

#define AV_OPT_SEARCH_CHILDREN   (1 << 0)

#define AV_OPT_SEARCH_FAKE_OBJ   (1 << 1)

#define AV_OPT_ALLOW_NULL (1 << 2)

#define AV_OPT_ARRAY_REPLACE (1 << 3)

#define AV_OPT_MULTI_COMPONENT_RANGE (1 << 12)

const AVOption *av_opt_find(void *obj, const char *name, const char *unit,
                            int opt_flags, int search_flags);

const AVOption *av_opt_find2(void *obj, const char *name, const char *unit,
                             int opt_flags, int search_flags, void **target_obj);

int av_opt_show2(void *obj, void *av_log_obj, int req_flags, int rej_flags);

int av_opt_get_key_value(const char **ropts,
                         const char *key_val_sep, const char *pairs_sep,
                         unsigned flags,
                         char **rkey, char **rval);

enum {

    AV_OPT_FLAG_IMPLICIT_KEY = 1,
};

int av_set_options_string(void *ctx, const char *opts,
                          const char *key_val_sep, const char *pairs_sep);

int av_opt_set_from_string(void *ctx, const char *opts,
                           const char *const *shorthand,
                           const char *key_val_sep, const char *pairs_sep);

int av_opt_set_dict(void *obj, struct AVDictionary **options);

int av_opt_set_dict2(void *obj, struct AVDictionary **options, int search_flags);

int av_opt_copy(void *dest, const void *src);

int av_opt_set         (void *obj, const char *name, const char *val, int search_flags);
int av_opt_set_int     (void *obj, const char *name, int64_t     val, int search_flags);
int av_opt_set_double  (void *obj, const char *name, double      val, int search_flags);
int av_opt_set_q       (void *obj, const char *name, AVRational  val, int search_flags);
int av_opt_set_bin     (void *obj, const char *name, const uint8_t *val, int size, int search_flags);
int av_opt_set_image_size(void *obj, const char *name, int w, int h, int search_flags);
int av_opt_set_pixel_fmt (void *obj, const char *name, enum AVPixelFormat fmt, int search_flags);
int av_opt_set_sample_fmt(void *obj, const char *name, enum AVSampleFormat fmt, int search_flags);
int av_opt_set_video_rate(void *obj, const char *name, AVRational val, int search_flags);

int av_opt_set_chlayout(void *obj, const char *name, const AVChannelLayout *layout, int search_flags);

int av_opt_set_dict_val(void *obj, const char *name, const AVDictionary *val, int search_flags);

#if FF_API_OPT_INT_LIST

#define av_opt_set_int_list(obj, name, val, term, flags) \
    (av_int_list_length(val, term) > INT_MAX / sizeof(*(val)) ? \
     AVERROR(EINVAL) : \
     av_opt_set_bin(obj, name, (const uint8_t *)(val), \
                    av_int_list_length(val, term) * sizeof(*(val)), flags))
#endif

int av_opt_set_array(void *obj, const char *name, int search_flags,
                     unsigned int start_elem, unsigned int nb_elems,
                     enum AVOptionType val_type, const void *val);

int av_opt_get         (void *obj, const char *name, int search_flags, uint8_t   **out_val);
int av_opt_get_int     (void *obj, const char *name, int search_flags, int64_t    *out_val);
int av_opt_get_double  (void *obj, const char *name, int search_flags, double     *out_val);
int av_opt_get_q       (void *obj, const char *name, int search_flags, AVRational *out_val);
int av_opt_get_image_size(void *obj, const char *name, int search_flags, int *w_out, int *h_out);
int av_opt_get_pixel_fmt (void *obj, const char *name, int search_flags, enum AVPixelFormat *out_fmt);
int av_opt_get_sample_fmt(void *obj, const char *name, int search_flags, enum AVSampleFormat *out_fmt);
int av_opt_get_video_rate(void *obj, const char *name, int search_flags, AVRational *out_val);

int av_opt_get_chlayout(void *obj, const char *name, int search_flags, AVChannelLayout *layout);

int av_opt_get_dict_val(void *obj, const char *name, int search_flags, AVDictionary **out_val);

int av_opt_get_array_size(void *obj, const char *name, int search_flags,
                          unsigned int *out_val);

int av_opt_get_array(void *obj, const char *name, int search_flags,
                     unsigned int start_elem, unsigned int nb_elems,
                     enum AVOptionType out_type, void *out_val);

int av_opt_eval_flags (void *obj, const AVOption *o, const char *val, int        *flags_out);
int av_opt_eval_int   (void *obj, const AVOption *o, const char *val, int        *int_out);
int av_opt_eval_uint  (void *obj, const AVOption *o, const char *val, unsigned   *uint_out);
int av_opt_eval_int64 (void *obj, const AVOption *o, const char *val, int64_t    *int64_out);
int av_opt_eval_float (void *obj, const AVOption *o, const char *val, float      *float_out);
int av_opt_eval_double(void *obj, const AVOption *o, const char *val, double     *double_out);
int av_opt_eval_q     (void *obj, const AVOption *o, const char *val, AVRational *q_out);

#if FF_API_OPT_PTR

attribute_deprecated
void *av_opt_ptr(const AVClass *avclass, void *obj, const char *name);
#endif

int av_opt_is_set_to_default(void *obj, const AVOption *o);

int av_opt_is_set_to_default_by_name(void *obj, const char *name, int search_flags);

int av_opt_flag_is_set(void *obj, const char *field_name, const char *flag_name);

#define AV_OPT_SERIALIZE_SKIP_DEFAULTS              0x00000001  ///< Serialize options that are not set to default values only.
#define AV_OPT_SERIALIZE_OPT_FLAGS_EXACT            0x00000002  ///< Serialize options that exactly match opt_flags only.
#define AV_OPT_SERIALIZE_SEARCH_CHILDREN            0x00000004  ///< Serialize options in possible children of the given object.

int av_opt_serialize(void *obj, int opt_flags, int flags, char **buffer,
                     const char key_val_sep, const char pairs_sep);

void av_opt_freep_ranges(AVOptionRanges **ranges);

int av_opt_query_ranges(AVOptionRanges **, void *obj, const char *key, int flags);

int av_opt_query_ranges_default(AVOptionRanges **, void *obj, const char *key, int flags);

#endif
