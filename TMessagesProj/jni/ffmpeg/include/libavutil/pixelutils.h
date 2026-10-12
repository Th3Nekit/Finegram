#ifndef AVUTIL_PIXELUTILS_H
#define AVUTIL_PIXELUTILS_H

#include <stddef.h>
#include <stdint.h>

typedef int (*av_pixelutils_sad_fn)(const uint8_t *src1, ptrdiff_t stride1,
                                    const uint8_t *src2, ptrdiff_t stride2);

av_pixelutils_sad_fn av_pixelutils_get_sad_fn(int w_bits, int h_bits,
                                              int aligned, void *log_ctx);

#endif
