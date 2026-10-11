/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef COMMON_AUDIO_WAV_HEADER_H_
#define COMMON_AUDIO_WAV_HEADER_H_

#include <stddef.h>
#include <stdint.h>
#include <algorithm>

#include "rtc_base/checks.h"

namespace webrtc {

class WavHeaderReader {
 public:

  virtual size_t Read(void* buf, size_t num_bytes) = 0;
  virtual bool SeekForward(uint32_t num_bytes) = 0;
  virtual ~WavHeaderReader() = default;
  virtual int64_t GetPosition() = 0;
};

enum class WavFormat {
  kWavFormatPcm = 1,
  kWavFormatIeeeFloat = 3,
  kWavFormatALaw = 6,
  kWavFormatMuLaw = 7,
};

constexpr size_t kPcmWavHeaderSize = 44;
constexpr size_t kIeeeFloatWavHeaderSize = 58;

constexpr size_t WavHeaderSize(WavFormat format) {
  if (format == WavFormat::kWavFormatPcm) {
    return kPcmWavHeaderSize;
  }
  RTC_CHECK_EQ(format, WavFormat::kWavFormatIeeeFloat);
  return kIeeeFloatWavHeaderSize;
}

constexpr size_t MaxWavHeaderSize() {
  return std::max(WavHeaderSize(WavFormat::kWavFormatPcm),
                  WavHeaderSize(WavFormat::kWavFormatIeeeFloat));
}

bool CheckWavParameters(size_t num_channels,
                        int sample_rate,
                        WavFormat format,
                        size_t num_samples);

void WriteWavHeader(size_t num_channels,
                    int sample_rate,
                    WavFormat format,
                    size_t num_samples,
                    uint8_t* buf,
                    size_t* header_size);

bool ReadWavHeader(WavHeaderReader* readable,
                   size_t* num_channels,
                   int* sample_rate,
                   WavFormat* format,
                   size_t* bytes_per_sample,
                   size_t* num_samples,
                   int64_t* data_start_pos);

}

#endif
