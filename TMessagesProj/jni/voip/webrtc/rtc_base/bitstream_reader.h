/*
 *  Copyright 2021 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_BITSTREAM_READER_H_
#define RTC_BASE_BITSTREAM_READER_H_

#include <stdint.h>

#include "absl/base/attributes.h"
#include "absl/strings/string_view.h"
#include "api/array_view.h"
#include "rtc_base/checks.h"
#include "rtc_base/numerics/safe_conversions.h"

namespace webrtc {

class BitstreamReader {
 public:
  explicit BitstreamReader(
      rtc::ArrayView<const uint8_t> bytes ABSL_ATTRIBUTE_LIFETIME_BOUND);
  explicit BitstreamReader(
      absl::string_view bytes ABSL_ATTRIBUTE_LIFETIME_BOUND);
  BitstreamReader(const BitstreamReader&) = default;
  BitstreamReader& operator=(const BitstreamReader&) = default;
  ~BitstreamReader();

  int RemainingBitCount() const;

  bool Ok() const { return RemainingBitCount() >= 0; }

  void Invalidate() { remaining_bits_ = -1; }

  void ConsumeBits(int bits);

  ABSL_MUST_USE_RESULT int ReadBit();

  ABSL_MUST_USE_RESULT uint64_t ReadBits(int bits);

  template <typename T,
            typename std::enable_if<std::is_unsigned<T>::value &&
                                    !std::is_same<T, bool>::value &&
                                    sizeof(T) <= 8>::type* = nullptr>
  ABSL_MUST_USE_RESULT T Read() {
    return rtc::dchecked_cast<T>(ReadBits(sizeof(T) * 8));
  }

  template <
      typename T,
      typename std::enable_if<std::is_same<T, bool>::value>::type* = nullptr>
  ABSL_MUST_USE_RESULT bool Read() {
    return ReadBit() != 0;
  }

  uint32_t ReadNonSymmetric(uint32_t num_values);

  uint32_t ReadExponentialGolomb();

  int ReadSignedExponentialGolomb();

  uint64_t ReadLeb128();

  std::string ReadString(int num_bytes);

 private:
  void set_last_read_is_verified(bool value) const;

  const uint8_t* bytes_;

  int remaining_bits_;

  mutable bool last_read_is_verified_ = true;
};

inline BitstreamReader::BitstreamReader(rtc::ArrayView<const uint8_t> bytes)
    : bytes_(bytes.data()),
      remaining_bits_(rtc::checked_cast<int>(bytes.size() * 8)) {}

inline BitstreamReader::BitstreamReader(absl::string_view bytes)
    : bytes_(reinterpret_cast<const uint8_t*>(bytes.data())),
      remaining_bits_(rtc::checked_cast<int>(bytes.size() * 8)) {}

inline BitstreamReader::~BitstreamReader() {
  RTC_DCHECK(last_read_is_verified_) << "Latest calls to Read or ConsumeBit "
                                        "were not checked with Ok function.";
}

inline void BitstreamReader::set_last_read_is_verified(bool value) const {
#ifdef RTC_DCHECK_IS_ON
  last_read_is_verified_ = value;
#endif
}

inline int BitstreamReader::RemainingBitCount() const {
  set_last_read_is_verified(true);
  return remaining_bits_;
}

}

#endif
