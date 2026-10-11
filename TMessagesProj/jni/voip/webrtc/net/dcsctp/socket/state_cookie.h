/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef NET_DCSCTP_SOCKET_STATE_COOKIE_H_
#define NET_DCSCTP_SOCKET_STATE_COOKIE_H_

#include <cstdint>
#include <vector>

#include "absl/types/optional.h"
#include "api/array_view.h"
#include "net/dcsctp/common/internal_types.h"
#include "net/dcsctp/socket/capabilities.h"

namespace dcsctp {

class StateCookie {
 public:
  static constexpr size_t kCookieSize = 45;

  StateCookie(VerificationTag peer_tag,
              VerificationTag my_tag,
              TSN peer_initial_tsn,
              TSN my_initial_tsn,
              uint32_t a_rwnd,
              TieTag tie_tag,
              Capabilities capabilities)
      : peer_tag_(peer_tag),
        my_tag_(my_tag),
        peer_initial_tsn_(peer_initial_tsn),
        my_initial_tsn_(my_initial_tsn),
        a_rwnd_(a_rwnd),
        tie_tag_(tie_tag),
        capabilities_(capabilities) {}

  std::vector<uint8_t> Serialize();

  static absl::optional<StateCookie> Deserialize(
      rtc::ArrayView<const uint8_t> cookie);

  VerificationTag peer_tag() const { return peer_tag_; }
  VerificationTag my_tag() const { return my_tag_; }
  TSN peer_initial_tsn() const { return peer_initial_tsn_; }
  TSN my_initial_tsn() const { return my_initial_tsn_; }
  uint32_t a_rwnd() const { return a_rwnd_; }
  TieTag tie_tag() const { return tie_tag_; }
  const Capabilities& capabilities() const { return capabilities_; }

 private:

  const VerificationTag peer_tag_;

  const VerificationTag my_tag_;
  const TSN peer_initial_tsn_;
  const TSN my_initial_tsn_;
  const uint32_t a_rwnd_;
  const TieTag tie_tag_;
  const Capabilities capabilities_;
};
}

#endif
