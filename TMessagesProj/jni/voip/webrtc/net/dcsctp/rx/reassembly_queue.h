/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef NET_DCSCTP_RX_REASSEMBLY_QUEUE_H_
#define NET_DCSCTP_RX_REASSEMBLY_QUEUE_H_

#include <stddef.h>

#include <cstdint>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "absl/functional/any_invocable.h"
#include "absl/strings/string_view.h"
#include "api/array_view.h"
#include "net/dcsctp/common/internal_types.h"
#include "net/dcsctp/common/sequence_numbers.h"
#include "net/dcsctp/packet/chunk/forward_tsn_common.h"
#include "net/dcsctp/packet/data.h"
#include "net/dcsctp/packet/parameter/outgoing_ssn_reset_request_parameter.h"
#include "net/dcsctp/packet/parameter/reconfiguration_response_parameter.h"
#include "net/dcsctp/public/dcsctp_handover_state.h"
#include "net/dcsctp/public/dcsctp_message.h"
#include "net/dcsctp/rx/reassembly_streams.h"
#include "rtc_base/containers/flat_set.h"

namespace dcsctp {

class ReassemblyQueue {
 public:

  static constexpr float kHighWatermarkLimit = 0.9;

  ReassemblyQueue(absl::string_view log_prefix,
                  TSN peer_initial_tsn,
                  size_t max_size_bytes,
                  bool use_message_interleaving = false);

  void Add(TSN tsn, Data data);

  bool HasMessages() const { return !reassembled_messages_.empty(); }

  std::vector<DcSctpMessage> FlushMessages();

  void HandleForwardTsn(
      TSN new_cumulative_tsn,
      rtc::ArrayView<const AnyForwardTsnChunk::SkippedStream> skipped_streams);

  void ResetStreamsAndLeaveDeferredReset(
      rtc::ArrayView<const StreamID> stream_ids);

  void EnterDeferredReset(TSN sender_last_assigned_tsn,
                          rtc::ArrayView<const StreamID> streams);

  size_t queued_bytes() const { return queued_bytes_; }

  size_t remaining_bytes() const { return watermark_bytes_ - queued_bytes_; }

  bool is_full() const { return queued_bytes_ >= max_size_bytes_; }

  bool is_above_watermark() const { return queued_bytes_ >= watermark_bytes_; }

  size_t watermark_bytes() const { return watermark_bytes_; }

  HandoverReadinessStatus GetHandoverReadiness() const;

  void AddHandoverState(DcSctpSocketHandoverState& state);
  void RestoreFromState(const DcSctpSocketHandoverState& state);

 private:
  struct DeferredResetStreams {
    DeferredResetStreams(UnwrappedTSN sender_last_assigned_tsn,
                         webrtc::flat_set<StreamID> streams)
        : sender_last_assigned_tsn(sender_last_assigned_tsn),
          streams(std::move(streams)) {}

    UnwrappedTSN sender_last_assigned_tsn;
    webrtc::flat_set<StreamID> streams;
    std::vector<absl::AnyInvocable<void(void)>> deferred_actions;
  };

  bool IsConsistent() const;
  void AddReassembledMessage(rtc::ArrayView<const UnwrappedTSN> tsns,
                             DcSctpMessage message);

  const absl::string_view log_prefix_;
  const size_t max_size_bytes_;
  const size_t watermark_bytes_;
  UnwrappedTSN::Unwrapper tsn_unwrapper_;

  std::vector<DcSctpMessage> reassembled_messages_;

  absl::optional<DeferredResetStreams> deferred_reset_streams_;

  ReconfigRequestSN last_completed_reset_req_seq_nbr_;

  size_t queued_bytes_ = 0;

  std::unique_ptr<ReassemblyStreams> streams_;
};
}

#endif
