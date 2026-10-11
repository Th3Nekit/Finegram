#ifndef AVCODEC_AVDCT_H
#define AVCODEC_AVDCT_H

#include "libavutil/opt.h"

typedef struct AVDCT {
    const AVClass *av_class;

    void (*idct)(int16_t *block               );

    uint8_t idct_permutation[64];

    void (*fdct)(int16_t *block               );

    int dct_algo;

    int idct_algo;

    void (*get_pixels)(int16_t *block               ,
                       const uint8_t *pixels              ,
                       ptrdiff_t line_size);

    int bits_per_sample;

    void (*get_pixels_unaligned)(int16_t *block               ,
                       const uint8_t *pixels,
                       ptrdiff_t line_size);
} AVDCT;

AVDCT *avcodec_dct_alloc(void);
int avcodec_dct_init(AVDCT *);

const AVClass *avcodec_dct_get_class(void);

#endif
