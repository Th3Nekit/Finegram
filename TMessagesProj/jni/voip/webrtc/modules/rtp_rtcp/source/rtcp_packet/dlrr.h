/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 *
 */

#ifndef MODULES_RTP_RTCP_SOURCE_RTCP_PACKET_DLRR_H_
#define MODULES_RTP_RTCP_SOURCE_RTCP_PACKET_DLRR_H_

#include <stddef.h>
#include <stdint.h>

#include <vector>

namespace webrtc {
namespace rtcp {
struct ReceiveTimeInfo {

  ReceiveTimeInfo() : ssrc(0), last_rr(0), delay_since_last_rr(0) {}
  ReceiveTimeInfo(uint32_t ssrc, uint32_t last_rr, uint32_t delay)
      : ssrc(ssrc), last_rr(last_rr), delay_since_last_rr(delay) {}

  uint32_t ssrc;
  uint32_t last_rr;
  uint32_t delay_since_last_rr;
};

inline bool operator==(const ReceiveTimeInfo& lhs, const ReceiveTimeInfo& rhs) {
  return lhs.ssrc == rhs.ssrc && lhs.last_rr == rhs.last_rr &&
         lhs.delay_since_last_rr == rhs.delay_since_last_rr;
}

inline bool operator!=(const ReceiveTimeInfo& lhs, const ReceiveTimeInfo& rhs) {
  return !(lhs == rhs);
}

class Dlrr {
 public:
  static const uint8_t kBlockType = 5;

  Dlrr();
  Dlrr(const Dlrr& other);
  ~Dlrr();

  Dlrr& operator=(const Dlrr& other) = default;

  explicit operator bool() const { return !sub_blocks_.empty(); }

  bool Parse(const uint8_t* buffer, uint16_t block_length_32bits);

  size_t BlockLength() const;

  void Create(uint8_t* buffer) const;

  void ClearItems() { sub_blocks_.clear(); }
  void AddDlrrItem(const ReceiveTimeInfo& time_info) {
    sub_blocks_.push_back(time_info);
  }

  const std::vector<ReceiveTimeInfo>& sub_blocks() const { return sub_blocks_; }

 private:
  static const size_t kBlockHeaderLength = 4;
  static const size_t kSubBlockLength = 12;

  std::vector<ReceiveTimeInfo> sub_blocks_;
};
}
}
#endif
