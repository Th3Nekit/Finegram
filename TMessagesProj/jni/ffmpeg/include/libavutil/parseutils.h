#ifndef AVUTIL_PARSEUTILS_H
#define AVUTIL_PARSEUTILS_H

#include <time.h>

#include "rational.h"

int av_parse_ratio(AVRational *q, const char *str, int max,
                   int log_offset, void *log_ctx);

#define av_parse_ratio_quiet(rate, str, max) \
    av_parse_ratio(rate, str, max, AV_LOG_MAX_OFFSET, NULL)

int av_parse_video_size(int *width_ptr, int *height_ptr, const char *str);

int av_parse_video_rate(AVRational *rate, const char *str);

int av_parse_color(uint8_t *rgba_color, const char *color_string, int slen,
                   void *log_ctx);

const char *av_get_known_color_name(int color_idx, const uint8_t **rgb);

int av_parse_time(int64_t *timeval, const char *timestr, int duration);

int av_find_info_tag(char *arg, int arg_size, const char *tag1, const char *info);

char *av_small_strptime(const char *p, const char *fmt, struct tm *dt);

time_t av_timegm(struct tm *tm);

#endif
