/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_TRANSIENT_FILE_UTILS_H_
#define MODULES_AUDIO_PROCESSING_TRANSIENT_FILE_UTILS_H_

#include <string.h>

#include "rtc_base/system/file_wrapper.h"

namespace webrtc {

template <class Dest, class Source>
inline Dest bit_cast(const Source& source) {

  static_assert(sizeof(Dest) == sizeof(Source),
                "Dest and Source have different sizes");

  Dest dest;
  memcpy(&dest, &source, sizeof(dest));
  return dest;
}

int ConvertByteArrayToFloat(const uint8_t bytes[4], float* out);

int ConvertByteArrayToDouble(const uint8_t bytes[8], double* out);

int ConvertFloatToByteArray(float value, uint8_t out_bytes[4]);

int ConvertDoubleToByteArray(double value, uint8_t out_bytes[8]);

size_t ReadInt16BufferFromFile(FileWrapper* file,
                               size_t length,
                               int16_t* buffer);

size_t ReadInt16FromFileToFloatBuffer(FileWrapper* file,
                                      size_t length,
                                      float* buffer);

size_t ReadInt16FromFileToDoubleBuffer(FileWrapper* file,
                                       size_t length,
                                       double* buffer);

size_t ReadFloatBufferFromFile(FileWrapper* file, size_t length, float* buffer);

size_t ReadDoubleBufferFromFile(FileWrapper* file,
                                size_t length,
                                double* buffer);

size_t WriteInt16BufferToFile(FileWrapper* file,
                              size_t length,
                              const int16_t* buffer);

size_t WriteFloatBufferToFile(FileWrapper* file,
                              size_t length,
                              const float* buffer);

size_t WriteDoubleBufferToFile(FileWrapper* file,
                               size_t length,
                               const double* buffer);

}

#endif
