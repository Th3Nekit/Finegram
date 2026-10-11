/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef NET_DCSCTP_RX_REASSEMBLY_STREAMS_H_
#define NET_DCSCTP_RX_REASSEMBLY_STREAMS_H_

#include <stddef.h>
#include <stdint.h>

#include <functional>
#include <vector>

#include "absl/strings/string_view.h"
#include "api/array_view.h"
#include "net/dcsctp/common/sequence_numbers.h"
#include "net/dcsctp/packet/chunk/forward_tsn_common.h"
#include "net/dcsctp/packet/data.h"
#include "net/dcsctp/public/dcsctp_handover_state.h"
#include "net/dcsctp/public/dcsctp_message.h"

namespace dcsctp {

class ReassemblyStreams {
 public:

  using OnAssembledMessage =
      std::function<void(rtc::ArrayView<const UnwrappedTSN> tsns,
                         DcSctpMessage message)>;

  virtual ~ReassemblyStreams() = default;

  virtual int Add(UnwrappedTSN tsn, Data data) = 0;

  virtual size_t HandleForwardTsn(
      UnwrappedTSN new_cumulative_ack_tsn,
      rtc::ArrayView<const AnyForwardTsnChunk::SkippedStream>
          skipped_streams) = 0;

  virtual void ResetStreams(rtc::ArrayView<const StreamID> stream_ids) = 0;

  virtual HandoverReadinessStatus GetHandoverReadiness() const = 0;
  virtual void AddHandoverState(DcSctpSocketHandoverState& state) = 0;
  virtual void RestoreFromState(const DcSctpSocketHandoverState& state) = 0;
};

}

#endif
