/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef NET_DCSCTP_TX_RETRANSMISSION_TIMEOUT_H_
#define NET_DCSCTP_TX_RETRANSMISSION_TIMEOUT_H_

#include <cstdint>
#include <functional>

#include "net/dcsctp/public/dcsctp_options.h"

namespace dcsctp {

class RetransmissionTimeout {
 public:
  static constexpr int kRttShift = 3;
  static constexpr int kRttVarShift = 2;
  explicit RetransmissionTimeout(const DcSctpOptions& options);

  void ObserveRTT(webrtc::TimeDelta measured_rtt);

  webrtc::TimeDelta rto() const { return webrtc::TimeDelta::Millis(rto_); }

  webrtc::TimeDelta srtt() const {
    return webrtc::TimeDelta::Millis(scaled_srtt_ >> kRttShift);
  }

 private:
  const webrtc::TimeDelta min_rto_;
  const webrtc::TimeDelta max_rto_;
  const webrtc::TimeDelta max_rtt_;
  const int64_t min_rtt_variance_;

  bool first_measurement_ = true;

  int64_t scaled_srtt_;

  int64_t scaled_rtt_var_ = 0;

  int64_t rto_;
};
}

#endif
