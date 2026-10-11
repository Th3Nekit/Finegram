/*
 * Audio FIFO
 * Copyright (c) 2012 Justin Ruggles <justin.ruggles@gmail.com>
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
 * Audio FIFO Buffer
 */

#ifndef AVUTIL_AUDIO_FIFO_H
#define AVUTIL_AUDIO_FIFO_H

#include "attributes.h"
#include "samplefmt.h"

typedef struct AVAudioFifo AVAudioFifo;

void av_audio_fifo_free(AVAudioFifo *af);

AVAudioFifo *av_audio_fifo_alloc(enum AVSampleFormat sample_fmt, int channels,
                                 int nb_samples);

av_warn_unused_result
int av_audio_fifo_realloc(AVAudioFifo *af, int nb_samples);

int av_audio_fifo_write(AVAudioFifo *af, void * const *data, int nb_samples);

int av_audio_fifo_peek(const AVAudioFifo *af, void * const *data, int nb_samples);

int av_audio_fifo_peek_at(const AVAudioFifo *af, void * const *data,
                          int nb_samples, int offset);

int av_audio_fifo_read(AVAudioFifo *af, void * const *data, int nb_samples);

int av_audio_fifo_drain(AVAudioFifo *af, int nb_samples);

void av_audio_fifo_reset(AVAudioFifo *af);

int av_audio_fifo_size(AVAudioFifo *af);

int av_audio_fifo_space(AVAudioFifo *af);

#endif
