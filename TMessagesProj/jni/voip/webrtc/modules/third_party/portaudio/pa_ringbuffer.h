#ifndef MODULES_THIRD_PARTY_PORTAUDIO_PA_RINGBUFFER_H_
#define MODULES_THIRD_PARTY_PORTAUDIO_PA_RINGBUFFER_H_
/*
 * $Id$
 * Portable Audio I/O Library
 * Ring Buffer utility.
 *
 * Author: Phil Burk, http://www.softsynth.com
 * modified for SMP safety on OS X by Bjorn Roche.
 * also allowed for const where possible.
 * modified for multiple-byte-sized data elements by Sven Fischer
 *
 * Note that this is safe only for a single-thread reader
 * and a single-thread writer.
 *
 * This program is distributed with the PortAudio Portable Audio Library.
 * For more information see: http://www.portaudio.com
 * Copyright (c) 1999-2000 Ross Bencina and Phil Burk
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files
 * (the "Software"), to deal in the Software without restriction,
 * including without limitation the rights to use, copy, modify, merge,
 * publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR
 * ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
 * CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/*
 * The text above constitutes the entire PortAudio license; however,
 * the PortAudio community also makes the following non-binding requests:
 *
 * Any person wishing to distribute modifications to the Software is
 * requested to send the modifications to the original developer so that
 * they can be incorporated into the canonical version. It is also
 * requested that these non-binding requests be included along with the
 * license above.
 */

/** @file
 @ingroup common_src
 @brief Single-reader single-writer lock-free ring buffer

 PaUtilRingBuffer is a ring buffer used to transport samples between
 different execution contexts (threads, OS callbacks, interrupt handlers)
 without requiring the use of any locks. This only works when there is
 a single reader and a single writer (ie. one thread or callback writes
 to the ring buffer, another thread or callback reads from it).

 The PaUtilRingBuffer structure manages a ring buffer containing N
 elements, where N must be a power of two. An element may be any size
 (specified in bytes).

 The memory area used to store the buffer elements must be allocated by
 the client prior to calling PaUtil_InitializeRingBuffer() and must outlive
 the use of the ring buffer.

 @note The ring buffer functions are not normally exposed in the PortAudio
 libraries. If you want to call them then you will need to add pa_ringbuffer.c
 to your application source code.
*/

#if defined(__APPLE__)
#include <sys/types.h>
typedef int32_t ring_buffer_size_t;
#elif defined(__GNUC__)
typedef long ring_buffer_size_t;
#elif (_MSC_VER >= 1400)
typedef long ring_buffer_size_t;
#elif defined(_MSC_VER) || defined(__BORLANDC__)
typedef long ring_buffer_size_t;
#else
typedef long ring_buffer_size_t;
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct PaUtilRingBuffer {
  ring_buffer_size_t bufferSize;

  volatile ring_buffer_size_t
      writeIndex;

  volatile ring_buffer_size_t
      readIndex;

  ring_buffer_size_t bigMask;

  ring_buffer_size_t smallMask;
  ring_buffer_size_t elementSizeBytes;
  char* buffer;
} PaUtilRingBuffer;

ring_buffer_size_t PaUtil_InitializeRingBuffer(
    PaUtilRingBuffer* rbuf,
    ring_buffer_size_t elementSizeBytes,
    ring_buffer_size_t elementCount,
    void* dataPtr);

void PaUtil_FlushRingBuffer(PaUtilRingBuffer* rbuf);

ring_buffer_size_t PaUtil_GetRingBufferWriteAvailable(
    const PaUtilRingBuffer* rbuf);

ring_buffer_size_t PaUtil_GetRingBufferReadAvailable(
    const PaUtilRingBuffer* rbuf);

ring_buffer_size_t PaUtil_WriteRingBuffer(PaUtilRingBuffer* rbuf,
                                          const void* data,
                                          ring_buffer_size_t elementCount);

ring_buffer_size_t PaUtil_ReadRingBuffer(PaUtilRingBuffer* rbuf,
                                         void* data,
                                         ring_buffer_size_t elementCount);

ring_buffer_size_t PaUtil_GetRingBufferWriteRegions(
    PaUtilRingBuffer* rbuf,
    ring_buffer_size_t elementCount,
    void** dataPtr1,
    ring_buffer_size_t* sizePtr1,
    void** dataPtr2,
    ring_buffer_size_t* sizePtr2);

ring_buffer_size_t PaUtil_AdvanceRingBufferWriteIndex(
    PaUtilRingBuffer* rbuf,
    ring_buffer_size_t elementCount);

ring_buffer_size_t PaUtil_GetRingBufferReadRegions(
    PaUtilRingBuffer* rbuf,
    ring_buffer_size_t elementCount,
    void** dataPtr1,
    ring_buffer_size_t* sizePtr1,
    void** dataPtr2,
    ring_buffer_size_t* sizePtr2);

ring_buffer_size_t PaUtil_AdvanceRingBufferReadIndex(
    PaUtilRingBuffer* rbuf,
    ring_buffer_size_t elementCount);

#ifdef __cplusplus
}
#endif
#endif
