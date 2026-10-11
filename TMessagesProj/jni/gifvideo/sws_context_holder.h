#ifndef SWS_CONTEXT_HOLDER_H
#define SWS_CONTEXT_HOLDER_H

extern "C" {
#include <libswscale/swscale.h>
#include <libavutil/pixfmt.h>
}

struct SwsContextHolder {

    SwsContext*    ctx        = nullptr;
    int            src_width  = 0;
    int            src_height = 0;
    AVPixelFormat  src_format = AV_PIX_FMT_NONE;
    int            dst_width  = 0;
    int            dst_height = 0;
    AVPixelFormat  dst_format = AV_PIX_FMT_NONE;

    SwsContextHolder() = default;

    ~SwsContextHolder() {
        reset();
    }

    SwsContextHolder(const SwsContextHolder&) = delete;

    SwsContextHolder& operator=(const SwsContextHolder&) = delete;

    SwsContextHolder(SwsContextHolder&& other) noexcept
            : ctx(other.ctx),
              src_width(other.src_width),   src_height(other.src_height), src_format(other.src_format),
              dst_width(other.dst_width),   dst_height(other.dst_height), dst_format(other.dst_format) {
        other.ctx = nullptr;
    }

    SwsContextHolder& operator=(SwsContextHolder&& other) noexcept {
        if (this != &other) {
            reset();
            ctx        = other.ctx;
            src_width  = other.src_width;
            src_height = other.src_height;
            src_format = other.src_format;
            dst_width  = other.dst_width;
            dst_height = other.dst_height;
            dst_format = other.dst_format;
            other.ctx  = nullptr;
        }
        return *this;
    }

    SwsContext* get(
            int new_src_width, int new_src_height, AVPixelFormat new_src_format,
            int new_dst_width, int new_dst_height, AVPixelFormat new_dst_format
    ) {

        if (ctx        != nullptr        &&
            src_width  == new_src_width  && src_height == new_src_height && src_format == new_src_format &&
            dst_width  == new_dst_width  && dst_height == new_dst_height && dst_format == new_dst_format) {
            return ctx;
        }

        reset();

        ctx = sws_getContext(
                new_src_width, new_src_height, new_src_format,
                new_dst_width, new_dst_height, new_dst_format,
                SWS_BILINEAR, nullptr, nullptr, nullptr
        );

        if (ctx != nullptr) {
            src_width  = new_src_width;
            src_height = new_src_height;
            src_format = new_src_format;
            dst_width  = new_dst_width;
            dst_height = new_dst_height;
            dst_format = new_dst_format;
        }

        return ctx;
    }

    void reset() {
        if (ctx != nullptr) {
            sws_freeContext(ctx);
            ctx = nullptr;
        }
        src_width  = src_height = dst_width = dst_height = 0;
        src_format = dst_format = AV_PIX_FMT_NONE;
    }
};

#endif