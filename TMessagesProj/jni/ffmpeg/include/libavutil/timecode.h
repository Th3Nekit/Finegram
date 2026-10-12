/*
 * Copyright (c) 2006 Smartjog S.A.S, Baptiste Coudurier <baptiste.coudurier@gmail.com>
 * Copyright (c) 2011-2012 Smartjog S.A.S, Clément Bœsch <clement.boesch@smartjog.com>
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

/**
 * @file
 * Timecode helpers header
 */

#ifndef AVUTIL_TIMECODE_H
#define AVUTIL_TIMECODE_H

#include <stdint.h>
#include "rational.h"

#define AV_TIMECODE_STR_SIZE 23

enum AVTimecodeFlag {
    AV_TIMECODE_FLAG_DROPFRAME      = 1<<0,
    AV_TIMECODE_FLAG_24HOURSMAX     = 1<<1,
    AV_TIMECODE_FLAG_ALLOWNEGATIVE  = 1<<2,
};

typedef struct {
    int start;
    uint32_t flags;
    AVRational rate;
    unsigned fps;
} AVTimecode;

int av_timecode_adjust_ntsc_framenum2(int framenum, int fps);

uint32_t av_timecode_get_smpte_from_framenum(const AVTimecode *tc, int framenum);

uint32_t av_timecode_get_smpte(AVRational rate, int drop, int hh, int mm, int ss, int ff);

char *av_timecode_make_string(const AVTimecode *tc, char *buf, int framenum);

char *av_timecode_make_smpte_tc_string2(char *buf, AVRational rate, uint32_t tcsmpte, int prevent_df, int skip_field);

char *av_timecode_make_smpte_tc_string(char *buf, uint32_t tcsmpte, int prevent_df);

char *av_timecode_make_mpeg_tc_string(char *buf, uint32_t tc25bit);

int av_timecode_init(AVTimecode *tc, AVRational rate, int flags, int frame_start, void *log_ctx);

int av_timecode_init_from_components(AVTimecode *tc, AVRational rate, int flags, int hh, int mm, int ss, int ff, void *log_ctx);

int av_timecode_init_from_string(AVTimecode *tc, AVRational rate, const char *str, void *log_ctx);

int av_timecode_check_frame_rate(AVRational rate);

#endif
