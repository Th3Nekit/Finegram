#ifndef AVUTIL_HWCONTEXT_VIDEOTOOLBOX_H
#define AVUTIL_HWCONTEXT_VIDEOTOOLBOX_H

#include <stdint.h>

#include <VideoToolbox/VideoToolbox.h>

#include "frame.h"
#include "pixfmt.h"

typedef struct AVVTFramesContext {
    enum AVColorRange color_range;
} AVVTFramesContext;

enum AVPixelFormat av_map_videotoolbox_format_to_pixfmt(uint32_t cv_fmt);

uint32_t av_map_videotoolbox_format_from_pixfmt(enum AVPixelFormat pix_fmt);

uint32_t av_map_videotoolbox_format_from_pixfmt2(enum AVPixelFormat pix_fmt, bool full_range);

CFStringRef av_map_videotoolbox_chroma_loc_from_av(enum AVChromaLocation loc);

CFStringRef av_map_videotoolbox_color_matrix_from_av(enum AVColorSpace space);

CFStringRef av_map_videotoolbox_color_primaries_from_av(enum AVColorPrimaries pri);

CFStringRef av_map_videotoolbox_color_trc_from_av(enum AVColorTransferCharacteristic trc);

int av_vt_pixbuf_set_attachments(void *log_ctx,
                                 CVPixelBufferRef pixbuf, const struct AVFrame *src);

#endif
