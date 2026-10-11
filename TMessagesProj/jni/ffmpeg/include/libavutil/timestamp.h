#ifndef AVUTIL_TIMESTAMP_H
#define AVUTIL_TIMESTAMP_H

#include "avutil.h"

#if defined(__cplusplus) && !defined(__STDC_FORMAT_MACROS) && !defined(PRId64)
#error missing -D__STDC_FORMAT_MACROS / #define __STDC_FORMAT_MACROS
#endif

#define AV_TS_MAX_STRING_SIZE 32

static inline char *av_ts_make_string(char *buf, int64_t ts)
{
    if (ts == AV_NOPTS_VALUE) snprintf(buf, AV_TS_MAX_STRING_SIZE, "NOPTS");
    else                      snprintf(buf, AV_TS_MAX_STRING_SIZE, "%" PRId64, ts);
    return buf;
}

#define av_ts2str(ts) av_ts_make_string((char[AV_TS_MAX_STRING_SIZE]){0}, ts)

char *av_ts_make_time_string2(char *buf, int64_t ts, AVRational tb);

static inline char *av_ts_make_time_string(char *buf, int64_t ts,
                                           const AVRational *tb)
{
    return av_ts_make_time_string2(buf, ts, *tb);
}

#define av_ts2timestr(ts, tb) av_ts_make_time_string((char[AV_TS_MAX_STRING_SIZE]){0}, ts, tb)

#endif
