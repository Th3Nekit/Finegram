#ifndef AVUTIL_REFSTRUCT_H
#define AVUTIL_REFSTRUCT_H

#include <stddef.h>

typedef union {
    void *nc;
    const void *c;
} AVRefStructOpaque;

#define AV_REFSTRUCT_FLAG_NO_ZEROING (1 << 0)

void *av_refstruct_alloc_ext_c(size_t size, unsigned flags, AVRefStructOpaque opaque,
                               void (*free_cb)(AVRefStructOpaque opaque, void *obj));

static inline
void *av_refstruct_alloc_ext(size_t size, unsigned flags, void *opaque,
                             void (*free_cb)(AVRefStructOpaque opaque, void *obj))
{
    return av_refstruct_alloc_ext_c(size, flags, (AVRefStructOpaque){.nc = opaque},
                                    free_cb);
}

static inline
void *av_refstruct_allocz(size_t size)
{
    return av_refstruct_alloc_ext(size, 0, NULL, NULL);
}

void av_refstruct_unref(void *objp);

void *av_refstruct_ref(void *obj);

const void *av_refstruct_ref_c(const void *obj);

void av_refstruct_replace(void *dstp, const void *src);

int av_refstruct_exclusive(const void *obj);

typedef struct AVRefStructPool AVRefStructPool;

#define AV_REFSTRUCT_POOL_FLAG_NO_ZEROING         AV_REFSTRUCT_FLAG_NO_ZEROING

#define AV_REFSTRUCT_POOL_FLAG_RESET_ON_INIT_ERROR                   (1 << 16)

#define AV_REFSTRUCT_POOL_FLAG_FREE_ON_INIT_ERROR                    (1 << 17)

#define AV_REFSTRUCT_POOL_FLAG_ZERO_EVERY_TIME                       (1 << 18)

AVRefStructPool *av_refstruct_pool_alloc(size_t size, unsigned flags);

AVRefStructPool *av_refstruct_pool_alloc_ext_c(size_t size, unsigned flags,
                                               AVRefStructOpaque opaque,
                                               int  (*init_cb)(AVRefStructOpaque opaque, void *obj),
                                               void (*reset_cb)(AVRefStructOpaque opaque, void *obj),
                                               void (*free_entry_cb)(AVRefStructOpaque opaque, void *obj),
                                               void (*free_cb)(AVRefStructOpaque opaque));

static inline
AVRefStructPool *av_refstruct_pool_alloc_ext(size_t size, unsigned flags,
                                             void *opaque,
                                             int  (*init_cb)(AVRefStructOpaque opaque, void *obj),
                                             void (*reset_cb)(AVRefStructOpaque opaque, void *obj),
                                             void (*free_entry_cb)(AVRefStructOpaque opaque, void *obj),
                                             void (*free_cb)(AVRefStructOpaque opaque))
{
    return av_refstruct_pool_alloc_ext_c(size, flags, (AVRefStructOpaque){.nc = opaque},
                                         init_cb, reset_cb, free_entry_cb, free_cb);
}

void *av_refstruct_pool_get(AVRefStructPool *pool);

static inline void av_refstruct_pool_uninit(AVRefStructPool **poolp)
{
    av_refstruct_unref(poolp);
}

#endif
