#ifndef AVUTIL_TX_H
#define AVUTIL_TX_H

#include <stdint.h>
#include <stddef.h>

typedef struct AVTXContext AVTXContext;

typedef struct AVComplexFloat {
    float re, im;
} AVComplexFloat;

typedef struct AVComplexDouble {
    double re, im;
} AVComplexDouble;

typedef struct AVComplexInt32 {
    int32_t re, im;
} AVComplexInt32;

enum AVTXType {

    AV_TX_FLOAT_FFT  = 0,
    AV_TX_DOUBLE_FFT = 2,
    AV_TX_INT32_FFT  = 4,

    AV_TX_FLOAT_MDCT  = 1,
    AV_TX_DOUBLE_MDCT = 3,
    AV_TX_INT32_MDCT  = 5,

    AV_TX_FLOAT_RDFT  = 6,
    AV_TX_DOUBLE_RDFT = 7,
    AV_TX_INT32_RDFT  = 8,

    AV_TX_FLOAT_DCT  = 9,
    AV_TX_DOUBLE_DCT = 10,
    AV_TX_INT32_DCT  = 11,

    AV_TX_FLOAT_DCT_I  = 12,
    AV_TX_DOUBLE_DCT_I = 13,
    AV_TX_INT32_DCT_I  = 14,

    AV_TX_FLOAT_DST_I  = 15,
    AV_TX_DOUBLE_DST_I = 16,
    AV_TX_INT32_DST_I  = 17,

    AV_TX_NB,
};

typedef void (*av_tx_fn)(AVTXContext *s, void *out, void *in, ptrdiff_t stride);

enum AVTXFlags {

    AV_TX_INPLACE = 1ULL << 0,

    AV_TX_UNALIGNED = 1ULL << 1,

    AV_TX_FULL_IMDCT = 1ULL << 2,

    AV_TX_REAL_TO_REAL      = 1ULL << 3,
    AV_TX_REAL_TO_IMAGINARY = 1ULL << 4,
};

int av_tx_init(AVTXContext **ctx, av_tx_fn *tx, enum AVTXType type,
               int inv, int len, const void *scale, uint64_t flags);

void av_tx_uninit(AVTXContext **ctx);

#endif
