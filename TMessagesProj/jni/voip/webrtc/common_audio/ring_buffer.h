/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// A ring buffer to hold arbitrary data. Provides no thread safety. Unless
// otherwise specified, functions return 0 on success and -1 on error.

#ifndef COMMON_AUDIO_RING_BUFFER_H_
#define COMMON_AUDIO_RING_BUFFER_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>

enum Wrap { SAME_WRAP, DIFF_WRAP };

typedef struct RingBuffer {
  size_t read_pos;
  size_t write_pos;
  size_t element_count;
  size_t element_size;
  enum Wrap rw_wrap;
  char* data;
} RingBuffer;

RingBuffer* WebRtc_CreateBuffer(size_t element_count, size_t element_size);
void WebRtc_InitBuffer(RingBuffer* handle);
void WebRtc_FreeBuffer(void* handle);

size_t WebRtc_ReadBuffer(RingBuffer* handle,
                         void** data_ptr,
                         void* data,
                         size_t element_count);

size_t WebRtc_WriteBuffer(RingBuffer* handle,
                          const void* data,
                          size_t element_count);

int WebRtc_MoveReadPtr(RingBuffer* handle, int element_count);

size_t WebRtc_available_read(const RingBuffer* handle);

size_t WebRtc_available_write(const RingBuffer* handle);

#ifdef __cplusplus
}
#endif

#endif
