/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef COMMON_VIDEO_H264_H264_COMMON_H_
#define COMMON_VIDEO_H264_H264_COMMON_H_

#include <stddef.h>
#include <stdint.h>

#include <vector>

#include "rtc_base/buffer.h"
#include "rtc_base/system/rtc_export.h"

namespace webrtc {

namespace H264 {

const size_t kNaluLongStartSequenceSize = 4;

const size_t kNaluShortStartSequenceSize = 3;

const size_t kNaluTypeSize = 1;

enum NaluType : uint8_t {
  kSlice = 1,
  kIdr = 5,
  kSei = 6,
  kSps = 7,
  kPps = 8,
  kAud = 9,
  kEndOfSequence = 10,
  kEndOfStream = 11,
  kFiller = 12,
  kPrefix = 14,
  kStapA = 24,
  kFuA = 28
};

enum SliceType : uint8_t { kP = 0, kB = 1, kI = 2, kSp = 3, kSi = 4 };

struct NaluIndex {

  size_t start_offset;

  size_t payload_start_offset;

  size_t payload_size;
};

RTC_EXPORT std::vector<NaluIndex> FindNaluIndices(const uint8_t* buffer,
                                                  size_t buffer_size);

RTC_EXPORT NaluType ParseNaluType(uint8_t data);

std::vector<uint8_t> ParseRbsp(const uint8_t* data, size_t length);

void WriteRbsp(const uint8_t* bytes, size_t length, rtc::Buffer* destination);
}
}

#endif
