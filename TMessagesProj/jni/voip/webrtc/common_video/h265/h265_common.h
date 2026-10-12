/*
 *  Copyright (c) 2023 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef COMMON_VIDEO_H265_H265_COMMON_H_
#define COMMON_VIDEO_H265_H265_COMMON_H_

#include <memory>
#include <vector>

#include "common_video/h265/h265_inline.h"
#include "rtc_base/buffer.h"

namespace webrtc {

namespace H265 {

constexpr size_t kNaluLongStartSequenceSize = 4;

constexpr size_t kNaluShortStartSequenceSize = 3;

constexpr size_t kNaluHeaderSize = 2;

const size_t kNaluTypeSize = 2;

enum NaluType : uint8_t {
  kTrailN = 0,
  kTrailR = 1,
  kTsaN = 2,
  kTsaR = 3,
  kStsaN = 4,
  kStsaR = 5,
  kRadlN = 6,
  kRadlR = 7,
  kBlaWLp = 16,
  kBlaWRadl = 17,
  kBlaNLp = 18,
  kIdrWRadl = 19,
  kIdrNLp = 20,
  kCra = 21,
  kRsvIrapVcl23 = 23,
  kVps = 32,
  kSps = 33,
  kPps = 34,
  kAud = 35,
  kPrefixSei = 39,
  kSuffixSei = 40,

  kAp = 48,

  kFu = 49,

  kPaci = 50
};

enum SliceType : uint8_t { kB = 0, kP = 1, kI = 2 };

struct NaluIndex {

  size_t start_offset = 0;

  size_t payload_start_offset = 0;

  size_t payload_size = 0;
};

std::vector<NaluIndex> FindNaluIndices(const uint8_t* buffer,
                                       size_t buffer_size);

NaluType ParseNaluType(uint8_t data);

std::vector<uint8_t> ParseRbsp(const uint8_t* data, size_t length);

void WriteRbsp(const uint8_t* bytes, size_t length, rtc::Buffer* destination);

uint32_t Log2Ceiling(uint32_t value);
uint32_t Log2(uint32_t value);

}
}

#endif
