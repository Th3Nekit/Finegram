/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/audio_codecs/audio_codec_pair_id.h"

#include <atomic>
#include <cstdint>

#include "rtc_base/checks.h"

namespace webrtc {

namespace {

uint64_t GetNextId() {
  static std::atomic<uint64_t> next_id(0);

  const uint64_t new_id = next_id.fetch_add(1, std::memory_order_relaxed);

  RTC_DCHECK_LT(new_id, uint64_t{1} << 63) << "Used up all ID values";

  return new_id;
}

constexpr uint64_t ObfuscateId(uint64_t id) {

  return uint64_t{0x85fdb20e1294309a} + uint64_t{0xc516ef5c37462469} * id;
}

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4307)
#endif
static_assert(ObfuscateId(0) == uint64_t{0x85fdb20e1294309a}, "");
static_assert(ObfuscateId(1) == uint64_t{0x4b14a16a49da5503}, "");
static_assert(ObfuscateId(2) == uint64_t{0x102b90c68120796c}, "");
static_assert(ObfuscateId(3) == uint64_t{0xd5428022b8669dd5}, "");
static_assert(ObfuscateId(4) == uint64_t{0x9a596f7eefacc23e}, "");
static_assert(ObfuscateId(5) == uint64_t{0x5f705edb26f2e6a7}, "");
static_assert(ObfuscateId(6) == uint64_t{0x24874e375e390b10}, "");
static_assert(ObfuscateId(7) == uint64_t{0xe99e3d93957f2f79}, "");
static_assert(ObfuscateId(8) == uint64_t{0xaeb52cefccc553e2}, "");
static_assert(ObfuscateId(9) == uint64_t{0x73cc1c4c040b784b}, "");
#ifdef _MSC_VER
#pragma warning(pop)
#endif

}

AudioCodecPairId AudioCodecPairId::Create() {
  return AudioCodecPairId(ObfuscateId(GetNextId()));
}

}
