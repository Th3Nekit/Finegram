/*
 * Copyright (c) 2006 Michael Niedermayer <michaelni@gmx.at>
 * Copyright (c) 2008 Peter Ross
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

#ifndef AVUTIL_CHANNEL_LAYOUT_H
#define AVUTIL_CHANNEL_LAYOUT_H

#include <stdint.h>
#include <stdlib.h>

#include "version.h"
#include "attributes.h"

enum AVChannel {

    AV_CHAN_NONE = -1,
    AV_CHAN_FRONT_LEFT,
    AV_CHAN_FRONT_RIGHT,
    AV_CHAN_FRONT_CENTER,
    AV_CHAN_LOW_FREQUENCY,
    AV_CHAN_BACK_LEFT,
    AV_CHAN_BACK_RIGHT,
    AV_CHAN_FRONT_LEFT_OF_CENTER,
    AV_CHAN_FRONT_RIGHT_OF_CENTER,
    AV_CHAN_BACK_CENTER,
    AV_CHAN_SIDE_LEFT,
    AV_CHAN_SIDE_RIGHT,
    AV_CHAN_TOP_CENTER,
    AV_CHAN_TOP_FRONT_LEFT,
    AV_CHAN_TOP_FRONT_CENTER,
    AV_CHAN_TOP_FRONT_RIGHT,
    AV_CHAN_TOP_BACK_LEFT,
    AV_CHAN_TOP_BACK_CENTER,
    AV_CHAN_TOP_BACK_RIGHT,

    AV_CHAN_STEREO_LEFT = 29,

    AV_CHAN_STEREO_RIGHT,
    AV_CHAN_WIDE_LEFT,
    AV_CHAN_WIDE_RIGHT,
    AV_CHAN_SURROUND_DIRECT_LEFT,
    AV_CHAN_SURROUND_DIRECT_RIGHT,
    AV_CHAN_LOW_FREQUENCY_2,
    AV_CHAN_TOP_SIDE_LEFT,
    AV_CHAN_TOP_SIDE_RIGHT,
    AV_CHAN_BOTTOM_FRONT_CENTER,
    AV_CHAN_BOTTOM_FRONT_LEFT,
    AV_CHAN_BOTTOM_FRONT_RIGHT,
    AV_CHAN_SIDE_SURROUND_LEFT,
    AV_CHAN_SIDE_SURROUND_RIGHT,
    AV_CHAN_TOP_SURROUND_LEFT,
    AV_CHAN_TOP_SURROUND_RIGHT,

    AV_CHAN_BINAURAL_LEFT = 61,
    AV_CHAN_BINAURAL_RIGHT,

    AV_CHAN_UNUSED = 0x200,

    AV_CHAN_UNKNOWN = 0x300,

    AV_CHAN_AMBISONIC_BASE = 0x400,

    AV_CHAN_AMBISONIC_END  = 0x7ff,
};

enum AVChannelOrder {

    AV_CHANNEL_ORDER_UNSPEC,

    AV_CHANNEL_ORDER_NATIVE,

    AV_CHANNEL_ORDER_CUSTOM,

    AV_CHANNEL_ORDER_AMBISONIC,

    FF_CHANNEL_ORDER_NB
};

#define AV_CH_FRONT_LEFT             (1ULL << AV_CHAN_FRONT_LEFT           )
#define AV_CH_FRONT_RIGHT            (1ULL << AV_CHAN_FRONT_RIGHT          )
#define AV_CH_FRONT_CENTER           (1ULL << AV_CHAN_FRONT_CENTER         )
#define AV_CH_LOW_FREQUENCY          (1ULL << AV_CHAN_LOW_FREQUENCY        )
#define AV_CH_BACK_LEFT              (1ULL << AV_CHAN_BACK_LEFT            )
#define AV_CH_BACK_RIGHT             (1ULL << AV_CHAN_BACK_RIGHT           )
#define AV_CH_FRONT_LEFT_OF_CENTER   (1ULL << AV_CHAN_FRONT_LEFT_OF_CENTER )
#define AV_CH_FRONT_RIGHT_OF_CENTER  (1ULL << AV_CHAN_FRONT_RIGHT_OF_CENTER)
#define AV_CH_BACK_CENTER            (1ULL << AV_CHAN_BACK_CENTER          )
#define AV_CH_SIDE_LEFT              (1ULL << AV_CHAN_SIDE_LEFT            )
#define AV_CH_SIDE_RIGHT             (1ULL << AV_CHAN_SIDE_RIGHT           )
#define AV_CH_TOP_CENTER             (1ULL << AV_CHAN_TOP_CENTER           )
#define AV_CH_TOP_FRONT_LEFT         (1ULL << AV_CHAN_TOP_FRONT_LEFT       )
#define AV_CH_TOP_FRONT_CENTER       (1ULL << AV_CHAN_TOP_FRONT_CENTER     )
#define AV_CH_TOP_FRONT_RIGHT        (1ULL << AV_CHAN_TOP_FRONT_RIGHT      )
#define AV_CH_TOP_BACK_LEFT          (1ULL << AV_CHAN_TOP_BACK_LEFT        )
#define AV_CH_TOP_BACK_CENTER        (1ULL << AV_CHAN_TOP_BACK_CENTER      )
#define AV_CH_TOP_BACK_RIGHT         (1ULL << AV_CHAN_TOP_BACK_RIGHT       )
#define AV_CH_STEREO_LEFT            (1ULL << AV_CHAN_STEREO_LEFT          )
#define AV_CH_STEREO_RIGHT           (1ULL << AV_CHAN_STEREO_RIGHT         )
#define AV_CH_WIDE_LEFT              (1ULL << AV_CHAN_WIDE_LEFT            )
#define AV_CH_WIDE_RIGHT             (1ULL << AV_CHAN_WIDE_RIGHT           )
#define AV_CH_SURROUND_DIRECT_LEFT   (1ULL << AV_CHAN_SURROUND_DIRECT_LEFT )
#define AV_CH_SURROUND_DIRECT_RIGHT  (1ULL << AV_CHAN_SURROUND_DIRECT_RIGHT)
#define AV_CH_LOW_FREQUENCY_2        (1ULL << AV_CHAN_LOW_FREQUENCY_2      )
#define AV_CH_TOP_SIDE_LEFT          (1ULL << AV_CHAN_TOP_SIDE_LEFT        )
#define AV_CH_TOP_SIDE_RIGHT         (1ULL << AV_CHAN_TOP_SIDE_RIGHT       )
#define AV_CH_BOTTOM_FRONT_CENTER    (1ULL << AV_CHAN_BOTTOM_FRONT_CENTER  )
#define AV_CH_BOTTOM_FRONT_LEFT      (1ULL << AV_CHAN_BOTTOM_FRONT_LEFT    )
#define AV_CH_BOTTOM_FRONT_RIGHT     (1ULL << AV_CHAN_BOTTOM_FRONT_RIGHT   )
#define AV_CH_SIDE_SURROUND_LEFT     (1ULL << AV_CHAN_SIDE_SURROUND_LEFT   )
#define AV_CH_SIDE_SURROUND_RIGHT    (1ULL << AV_CHAN_SIDE_SURROUND_RIGHT  )
#define AV_CH_TOP_SURROUND_LEFT      (1ULL << AV_CHAN_TOP_SURROUND_LEFT    )
#define AV_CH_TOP_SURROUND_RIGHT     (1ULL << AV_CHAN_TOP_SURROUND_RIGHT   )
#define AV_CH_BINAURAL_LEFT          (1ULL << AV_CHAN_BINAURAL_LEFT        )
#define AV_CH_BINAURAL_RIGHT         (1ULL << AV_CHAN_BINAURAL_RIGHT       )

#define AV_CH_LAYOUT_MONO              (AV_CH_FRONT_CENTER)
#define AV_CH_LAYOUT_STEREO            (AV_CH_FRONT_LEFT|AV_CH_FRONT_RIGHT)
#define AV_CH_LAYOUT_2POINT1           (AV_CH_LAYOUT_STEREO|AV_CH_LOW_FREQUENCY)
#define AV_CH_LAYOUT_2_1               (AV_CH_LAYOUT_STEREO|AV_CH_BACK_CENTER)
#define AV_CH_LAYOUT_SURROUND          (AV_CH_LAYOUT_STEREO|AV_CH_FRONT_CENTER)
#define AV_CH_LAYOUT_3POINT1           (AV_CH_LAYOUT_SURROUND|AV_CH_LOW_FREQUENCY)
#define AV_CH_LAYOUT_4POINT0           (AV_CH_LAYOUT_SURROUND|AV_CH_BACK_CENTER)
#define AV_CH_LAYOUT_4POINT1           (AV_CH_LAYOUT_4POINT0|AV_CH_LOW_FREQUENCY)
#define AV_CH_LAYOUT_2_2               (AV_CH_LAYOUT_STEREO|AV_CH_SIDE_LEFT|AV_CH_SIDE_RIGHT)
#define AV_CH_LAYOUT_QUAD              (AV_CH_LAYOUT_STEREO|AV_CH_BACK_LEFT|AV_CH_BACK_RIGHT)
#define AV_CH_LAYOUT_5POINT0           (AV_CH_LAYOUT_SURROUND|AV_CH_SIDE_LEFT|AV_CH_SIDE_RIGHT)
#define AV_CH_LAYOUT_5POINT1           (AV_CH_LAYOUT_5POINT0|AV_CH_LOW_FREQUENCY)
#define AV_CH_LAYOUT_5POINT0_BACK      (AV_CH_LAYOUT_SURROUND|AV_CH_BACK_LEFT|AV_CH_BACK_RIGHT)
#define AV_CH_LAYOUT_5POINT1_BACK      (AV_CH_LAYOUT_5POINT0_BACK|AV_CH_LOW_FREQUENCY)
#define AV_CH_LAYOUT_6POINT0           (AV_CH_LAYOUT_5POINT0|AV_CH_BACK_CENTER)
#define AV_CH_LAYOUT_6POINT0_FRONT     (AV_CH_LAYOUT_2_2|AV_CH_FRONT_LEFT_OF_CENTER|AV_CH_FRONT_RIGHT_OF_CENTER)
#define AV_CH_LAYOUT_HEXAGONAL         (AV_CH_LAYOUT_5POINT0_BACK|AV_CH_BACK_CENTER)
#define AV_CH_LAYOUT_3POINT1POINT2     (AV_CH_LAYOUT_3POINT1|AV_CH_TOP_FRONT_LEFT|AV_CH_TOP_FRONT_RIGHT)
#define AV_CH_LAYOUT_6POINT1           (AV_CH_LAYOUT_5POINT1|AV_CH_BACK_CENTER)
#define AV_CH_LAYOUT_6POINT1_BACK      (AV_CH_LAYOUT_5POINT1_BACK|AV_CH_BACK_CENTER)
#define AV_CH_LAYOUT_6POINT1_FRONT     (AV_CH_LAYOUT_6POINT0_FRONT|AV_CH_LOW_FREQUENCY)
#define AV_CH_LAYOUT_7POINT0           (AV_CH_LAYOUT_5POINT0|AV_CH_BACK_LEFT|AV_CH_BACK_RIGHT)
#define AV_CH_LAYOUT_7POINT0_FRONT     (AV_CH_LAYOUT_5POINT0|AV_CH_FRONT_LEFT_OF_CENTER|AV_CH_FRONT_RIGHT_OF_CENTER)
#define AV_CH_LAYOUT_7POINT1           (AV_CH_LAYOUT_5POINT1|AV_CH_BACK_LEFT|AV_CH_BACK_RIGHT)
#define AV_CH_LAYOUT_7POINT1_WIDE      (AV_CH_LAYOUT_5POINT1|AV_CH_FRONT_LEFT_OF_CENTER|AV_CH_FRONT_RIGHT_OF_CENTER)
#define AV_CH_LAYOUT_7POINT1_WIDE_BACK (AV_CH_LAYOUT_5POINT1_BACK|AV_CH_FRONT_LEFT_OF_CENTER|AV_CH_FRONT_RIGHT_OF_CENTER)
#define AV_CH_LAYOUT_5POINT1POINT2     (AV_CH_LAYOUT_5POINT1|AV_CH_TOP_FRONT_LEFT|AV_CH_TOP_FRONT_RIGHT)
#define AV_CH_LAYOUT_5POINT1POINT2_BACK (AV_CH_LAYOUT_5POINT1_BACK|AV_CH_TOP_FRONT_LEFT|AV_CH_TOP_FRONT_RIGHT)
#define AV_CH_LAYOUT_OCTAGONAL         (AV_CH_LAYOUT_5POINT0|AV_CH_BACK_LEFT|AV_CH_BACK_CENTER|AV_CH_BACK_RIGHT)
#define AV_CH_LAYOUT_CUBE              (AV_CH_LAYOUT_QUAD|AV_CH_TOP_FRONT_LEFT|AV_CH_TOP_FRONT_RIGHT|AV_CH_TOP_BACK_LEFT|AV_CH_TOP_BACK_RIGHT)
#define AV_CH_LAYOUT_5POINT1POINT4_BACK (AV_CH_LAYOUT_5POINT1POINT2|AV_CH_TOP_BACK_LEFT|AV_CH_TOP_BACK_RIGHT)
#define AV_CH_LAYOUT_7POINT1POINT2     (AV_CH_LAYOUT_7POINT1|AV_CH_TOP_FRONT_LEFT|AV_CH_TOP_FRONT_RIGHT)
#define AV_CH_LAYOUT_7POINT1POINT4_BACK (AV_CH_LAYOUT_7POINT1POINT2|AV_CH_TOP_BACK_LEFT|AV_CH_TOP_BACK_RIGHT)
#define AV_CH_LAYOUT_7POINT2POINT3     (AV_CH_LAYOUT_7POINT1POINT2|AV_CH_TOP_BACK_CENTER|AV_CH_LOW_FREQUENCY_2)
#define AV_CH_LAYOUT_9POINT1POINT4_BACK (AV_CH_LAYOUT_7POINT1POINT4_BACK|AV_CH_FRONT_LEFT_OF_CENTER|AV_CH_FRONT_RIGHT_OF_CENTER)
#define AV_CH_LAYOUT_9POINT1POINT6     (AV_CH_LAYOUT_9POINT1POINT4_BACK|AV_CH_TOP_SIDE_LEFT|AV_CH_TOP_SIDE_RIGHT)
#define AV_CH_LAYOUT_HEXADECAGONAL     (AV_CH_LAYOUT_OCTAGONAL|AV_CH_WIDE_LEFT|AV_CH_WIDE_RIGHT|AV_CH_TOP_BACK_LEFT|AV_CH_TOP_BACK_RIGHT|AV_CH_TOP_BACK_CENTER|AV_CH_TOP_FRONT_CENTER|AV_CH_TOP_FRONT_LEFT|AV_CH_TOP_FRONT_RIGHT)
#define AV_CH_LAYOUT_BINAURAL          (AV_CH_BINAURAL_LEFT|AV_CH_BINAURAL_RIGHT)
#define AV_CH_LAYOUT_STEREO_DOWNMIX    (AV_CH_STEREO_LEFT|AV_CH_STEREO_RIGHT)
#define AV_CH_LAYOUT_22POINT2          (AV_CH_LAYOUT_9POINT1POINT6|AV_CH_BACK_CENTER|AV_CH_LOW_FREQUENCY_2|AV_CH_TOP_FRONT_CENTER|AV_CH_TOP_CENTER|AV_CH_TOP_BACK_CENTER|AV_CH_BOTTOM_FRONT_CENTER|AV_CH_BOTTOM_FRONT_LEFT|AV_CH_BOTTOM_FRONT_RIGHT)

#define AV_CH_LAYOUT_7POINT1_TOP_BACK AV_CH_LAYOUT_5POINT1POINT2_BACK

enum AVMatrixEncoding {
    AV_MATRIX_ENCODING_NONE,
    AV_MATRIX_ENCODING_DOLBY,
    AV_MATRIX_ENCODING_DPLII,
    AV_MATRIX_ENCODING_DPLIIX,
    AV_MATRIX_ENCODING_DPLIIZ,
    AV_MATRIX_ENCODING_DOLBYEX,
    AV_MATRIX_ENCODING_DOLBYHEADPHONE,
    AV_MATRIX_ENCODING_NB
};

typedef struct AVChannelCustom {
    enum AVChannel id;
    char name[16];
    void *opaque;
} AVChannelCustom;

typedef struct AVChannelLayout {

    enum AVChannelOrder order;

    int nb_channels;

    union {

        uint64_t mask;

        AVChannelCustom *map;
    } u;

    void *opaque;
} AVChannelLayout;

#define AV_CHANNEL_LAYOUT_MASK(nb, m) \
    {              AV_CHANNEL_ORDER_NATIVE, \
                          (nb), \
                    { m }, \
                    NULL }

#define AV_CHANNEL_LAYOUT_MONO              AV_CHANNEL_LAYOUT_MASK(1,  AV_CH_LAYOUT_MONO)
#define AV_CHANNEL_LAYOUT_STEREO            AV_CHANNEL_LAYOUT_MASK(2,  AV_CH_LAYOUT_STEREO)
#define AV_CHANNEL_LAYOUT_2POINT1           AV_CHANNEL_LAYOUT_MASK(3,  AV_CH_LAYOUT_2POINT1)
#define AV_CHANNEL_LAYOUT_2_1               AV_CHANNEL_LAYOUT_MASK(3,  AV_CH_LAYOUT_2_1)
#define AV_CHANNEL_LAYOUT_SURROUND          AV_CHANNEL_LAYOUT_MASK(3,  AV_CH_LAYOUT_SURROUND)
#define AV_CHANNEL_LAYOUT_3POINT1           AV_CHANNEL_LAYOUT_MASK(4,  AV_CH_LAYOUT_3POINT1)
#define AV_CHANNEL_LAYOUT_4POINT0           AV_CHANNEL_LAYOUT_MASK(4,  AV_CH_LAYOUT_4POINT0)
#define AV_CHANNEL_LAYOUT_4POINT1           AV_CHANNEL_LAYOUT_MASK(5,  AV_CH_LAYOUT_4POINT1)
#define AV_CHANNEL_LAYOUT_2_2               AV_CHANNEL_LAYOUT_MASK(4,  AV_CH_LAYOUT_2_2)
#define AV_CHANNEL_LAYOUT_QUAD              AV_CHANNEL_LAYOUT_MASK(4,  AV_CH_LAYOUT_QUAD)
#define AV_CHANNEL_LAYOUT_5POINT0           AV_CHANNEL_LAYOUT_MASK(5,  AV_CH_LAYOUT_5POINT0)
#define AV_CHANNEL_LAYOUT_5POINT1           AV_CHANNEL_LAYOUT_MASK(6,  AV_CH_LAYOUT_5POINT1)
#define AV_CHANNEL_LAYOUT_5POINT0_BACK      AV_CHANNEL_LAYOUT_MASK(5,  AV_CH_LAYOUT_5POINT0_BACK)
#define AV_CHANNEL_LAYOUT_5POINT1_BACK      AV_CHANNEL_LAYOUT_MASK(6,  AV_CH_LAYOUT_5POINT1_BACK)
#define AV_CHANNEL_LAYOUT_6POINT0           AV_CHANNEL_LAYOUT_MASK(6,  AV_CH_LAYOUT_6POINT0)
#define AV_CHANNEL_LAYOUT_6POINT0_FRONT     AV_CHANNEL_LAYOUT_MASK(6,  AV_CH_LAYOUT_6POINT0_FRONT)
#define AV_CHANNEL_LAYOUT_3POINT1POINT2     AV_CHANNEL_LAYOUT_MASK(6,  AV_CH_LAYOUT_3POINT1POINT2)
#define AV_CHANNEL_LAYOUT_HEXAGONAL         AV_CHANNEL_LAYOUT_MASK(6,  AV_CH_LAYOUT_HEXAGONAL)
#define AV_CHANNEL_LAYOUT_6POINT1           AV_CHANNEL_LAYOUT_MASK(7,  AV_CH_LAYOUT_6POINT1)
#define AV_CHANNEL_LAYOUT_6POINT1_BACK      AV_CHANNEL_LAYOUT_MASK(7,  AV_CH_LAYOUT_6POINT1_BACK)
#define AV_CHANNEL_LAYOUT_6POINT1_FRONT     AV_CHANNEL_LAYOUT_MASK(7,  AV_CH_LAYOUT_6POINT1_FRONT)
#define AV_CHANNEL_LAYOUT_7POINT0           AV_CHANNEL_LAYOUT_MASK(7,  AV_CH_LAYOUT_7POINT0)
#define AV_CHANNEL_LAYOUT_7POINT0_FRONT     AV_CHANNEL_LAYOUT_MASK(7,  AV_CH_LAYOUT_7POINT0_FRONT)
#define AV_CHANNEL_LAYOUT_7POINT1           AV_CHANNEL_LAYOUT_MASK(8,  AV_CH_LAYOUT_7POINT1)
#define AV_CHANNEL_LAYOUT_7POINT1_WIDE      AV_CHANNEL_LAYOUT_MASK(8,  AV_CH_LAYOUT_7POINT1_WIDE)
#define AV_CHANNEL_LAYOUT_7POINT1_WIDE_BACK AV_CHANNEL_LAYOUT_MASK(8,  AV_CH_LAYOUT_7POINT1_WIDE_BACK)
#define AV_CHANNEL_LAYOUT_5POINT1POINT2     AV_CHANNEL_LAYOUT_MASK(8,  AV_CH_LAYOUT_5POINT1POINT2)
#define AV_CHANNEL_LAYOUT_5POINT1POINT2_BACK AV_CHANNEL_LAYOUT_MASK(8, AV_CH_LAYOUT_5POINT1POINT2_BACK)
#define AV_CHANNEL_LAYOUT_OCTAGONAL         AV_CHANNEL_LAYOUT_MASK(8,  AV_CH_LAYOUT_OCTAGONAL)
#define AV_CHANNEL_LAYOUT_CUBE              AV_CHANNEL_LAYOUT_MASK(8,  AV_CH_LAYOUT_CUBE)
#define AV_CHANNEL_LAYOUT_5POINT1POINT4_BACK AV_CHANNEL_LAYOUT_MASK(10, AV_CH_LAYOUT_5POINT1POINT4_BACK)
#define AV_CHANNEL_LAYOUT_7POINT1POINT2     AV_CHANNEL_LAYOUT_MASK(10, AV_CH_LAYOUT_7POINT1POINT2)
#define AV_CHANNEL_LAYOUT_7POINT1POINT4_BACK AV_CHANNEL_LAYOUT_MASK(12, AV_CH_LAYOUT_7POINT1POINT4_BACK)
#define AV_CHANNEL_LAYOUT_7POINT2POINT3     AV_CHANNEL_LAYOUT_MASK(12, AV_CH_LAYOUT_7POINT2POINT3)
#define AV_CHANNEL_LAYOUT_9POINT1POINT4_BACK AV_CHANNEL_LAYOUT_MASK(14, AV_CH_LAYOUT_9POINT1POINT4_BACK)
#define AV_CHANNEL_LAYOUT_9POINT1POINT6     AV_CHANNEL_LAYOUT_MASK(16, AV_CH_LAYOUT_9POINT1POINT6)
#define AV_CHANNEL_LAYOUT_HEXADECAGONAL     AV_CHANNEL_LAYOUT_MASK(16, AV_CH_LAYOUT_HEXADECAGONAL)
#define AV_CHANNEL_LAYOUT_BINAURAL          AV_CHANNEL_LAYOUT_MASK(2,  AV_CH_LAYOUT_BINAURAL)
#define AV_CHANNEL_LAYOUT_STEREO_DOWNMIX    AV_CHANNEL_LAYOUT_MASK(2,  AV_CH_LAYOUT_STEREO_DOWNMIX)
#define AV_CHANNEL_LAYOUT_22POINT2          AV_CHANNEL_LAYOUT_MASK(24, AV_CH_LAYOUT_22POINT2)

#define AV_CHANNEL_LAYOUT_7POINT1_TOP_BACK  AV_CHANNEL_LAYOUT_5POINT1POINT2_BACK

#define AV_CHANNEL_LAYOUT_AMBISONIC_FIRST_ORDER \
    {              AV_CHANNEL_ORDER_AMBISONIC, \
                         4, \
                    { 0 }, \
                    NULL }

struct AVBPrint;

int av_channel_name(char *buf, size_t buf_size, enum AVChannel channel);

void av_channel_name_bprint(struct AVBPrint *bp, enum AVChannel channel_id);

int av_channel_description(char *buf, size_t buf_size, enum AVChannel channel);

void av_channel_description_bprint(struct AVBPrint *bp, enum AVChannel channel_id);

enum AVChannel av_channel_from_string(const char *name);

int av_channel_layout_custom_init(AVChannelLayout *channel_layout, int nb_channels);

int av_channel_layout_from_mask(AVChannelLayout *channel_layout, uint64_t mask);

int av_channel_layout_from_string(AVChannelLayout *channel_layout,
                                  const char *str);

void av_channel_layout_default(AVChannelLayout *ch_layout, int nb_channels);

const AVChannelLayout *av_channel_layout_standard(void **opaque);

void av_channel_layout_uninit(AVChannelLayout *channel_layout);

int av_channel_layout_copy(AVChannelLayout *dst, const AVChannelLayout *src);

int av_channel_layout_describe(const AVChannelLayout *channel_layout,
                               char *buf, size_t buf_size);

int av_channel_layout_describe_bprint(const AVChannelLayout *channel_layout,
                                      struct AVBPrint *bp);

enum AVChannel
av_channel_layout_channel_from_index(const AVChannelLayout *channel_layout, unsigned int idx);

int av_channel_layout_index_from_channel(const AVChannelLayout *channel_layout,
                                         enum AVChannel channel);

int av_channel_layout_index_from_string(const AVChannelLayout *channel_layout,
                                        const char *name);

enum AVChannel
av_channel_layout_channel_from_string(const AVChannelLayout *channel_layout,
                                      const char *name);

uint64_t av_channel_layout_subset(const AVChannelLayout *channel_layout,
                                  uint64_t mask);

int av_channel_layout_check(const AVChannelLayout *channel_layout);

int av_channel_layout_compare(const AVChannelLayout *chl, const AVChannelLayout *chl1);

int av_channel_layout_ambisonic_order(const AVChannelLayout *channel_layout);

#define AV_CHANNEL_LAYOUT_RETYPE_FLAG_LOSSLESS (1 << 0)

#define AV_CHANNEL_LAYOUT_RETYPE_FLAG_CANONICAL (1 << 1)

int av_channel_layout_retype(AVChannelLayout *channel_layout, enum AVChannelOrder order, int flags);

#endif
