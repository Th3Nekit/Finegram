#ifndef AVUTIL_CONTAINER_FIFO_H
#define AVUTIL_CONTAINER_FIFO_H

#include <stddef.h>

typedef struct AVContainerFifo AVContainerFifo;

enum AVContainerFifoFlags {

    AV_CONTAINER_FIFO_FLAG_REF  = (1 << 0),

    AV_CONTAINER_FIFO_FLAG_USER = (1 << 16),
};

AVContainerFifo*
av_container_fifo_alloc(void *opaque,
                        void* (*container_alloc)(void *opaque),
                        void  (*container_reset)(void *opaque, void *obj),
                        void  (*container_free) (void *opaque, void *obj),
                        int   (*fifo_transfer)  (void *opaque, void *dst, void *src, unsigned flags),
                        unsigned flags);

AVContainerFifo *av_container_fifo_alloc_avframe(unsigned flags);

void av_container_fifo_free(AVContainerFifo **cf);

int av_container_fifo_write(AVContainerFifo *cf, void *obj, unsigned flags);

int av_container_fifo_read(AVContainerFifo *cf, void *obj, unsigned flags);

int av_container_fifo_peek(AVContainerFifo *cf, void **pobj, size_t offset);

void av_container_fifo_drain(AVContainerFifo *cf, size_t nb_elems);

size_t av_container_fifo_can_read(const AVContainerFifo *cf);

#endif
