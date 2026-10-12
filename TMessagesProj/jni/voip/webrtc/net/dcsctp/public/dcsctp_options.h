/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef NET_DCSCTP_PUBLIC_DCSCTP_OPTIONS_H_
#define NET_DCSCTP_PUBLIC_DCSCTP_OPTIONS_H_

#include <stddef.h>
#include <stdint.h>

#include "absl/types/optional.h"
#include "net/dcsctp/public/types.h"

namespace dcsctp {
struct DcSctpOptions {

  static constexpr size_t kMaxSafeMTUSize = 1191;

  int local_port = 5000;

  int remote_port = 5000;

  uint16_t announced_maximum_incoming_streams = 65535;

  uint16_t announced_maximum_outgoing_streams = 65535;

  size_t mtu = kMaxSafeMTUSize;

  size_t max_message_size = 256 * 1024;

  StreamPriority default_stream_priority = StreamPriority(256);

  size_t max_receiver_window_buffer_size = 5 * 1024 * 1024;

  size_t max_send_buffer_size = 2'000'000;

  size_t total_buffered_amount_low_threshold = 1'800'000;

  DurationMs rtt_max = DurationMs(60'000);

  DurationMs rto_initial = DurationMs(500);

  DurationMs rto_max = DurationMs(60'000);

  DurationMs rto_min = DurationMs(400);

  DurationMs t1_init_timeout = DurationMs(1000);

  DurationMs t1_cookie_timeout = DurationMs(1000);

  DurationMs t2_shutdown_timeout = DurationMs(1000);

  absl::optional<DurationMs> max_timer_backoff_duration = absl::nullopt;

  DurationMs heartbeat_interval = DurationMs(30000);

  DurationMs delayed_ack_max_timeout = DurationMs(200);

  DurationMs min_rtt_variance = DurationMs(220);

  size_t cwnd_mtus_initial = 10;

  size_t cwnd_mtus_min = 4;

  size_t avoid_fragmentation_cwnd_mtus = 6;

  int max_burst = 4;

  absl::optional<int> max_retransmissions = 10;

  absl::optional<int> max_init_retransmits = 8;

  bool enable_partial_reliability = true;

  bool enable_message_interleaving = false;

  bool heartbeat_interval_include_rtt = true;

  bool disable_checksum_verification = false;

  ZeroChecksumAlternateErrorDetectionMethod
      zero_checksum_alternate_error_detection_method =
          ZeroChecksumAlternateErrorDetectionMethod::None();
};
}

#endif
