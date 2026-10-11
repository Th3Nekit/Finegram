/*
 *  Copyright 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef PC_TRANSCEIVER_LIST_H_
#define PC_TRANSCEIVER_LIST_H_

#include <stddef.h>

#include <algorithm>
#include <map>
#include <string>
#include <vector>

#include "absl/types/optional.h"
#include "api/media_types.h"
#include "api/rtc_error.h"
#include "api/rtp_parameters.h"
#include "api/rtp_sender_interface.h"
#include "api/scoped_refptr.h"
#include "api/sequence_checker.h"
#include "pc/rtp_transceiver.h"
#include "rtc_base/checks.h"
#include "rtc_base/system/no_unique_address.h"
#include "rtc_base/thread_annotations.h"

namespace webrtc {

typedef rtc::scoped_refptr<RtpTransceiverProxyWithInternal<RtpTransceiver>>
    RtpTransceiverProxyRefPtr;

class TransceiverStableState {
 public:
  TransceiverStableState() {}
  void set_newly_created();
  void SetMSectionIfUnset(absl::optional<std::string> mid,
                          absl::optional<size_t> mline_index);
  void SetRemoteStreamIds(const std::vector<std::string>& ids);
  void SetInitSendEncodings(
      const std::vector<RtpEncodingParameters>& encodings);
  void SetFiredDirection(
      absl::optional<RtpTransceiverDirection> fired_direction) {
    fired_direction_ = fired_direction;
  }
  absl::optional<std::string> mid() const { return mid_; }
  absl::optional<size_t> mline_index() const { return mline_index_; }
  absl::optional<std::vector<std::string>> remote_stream_ids() const {
    return remote_stream_ids_;
  }
  absl::optional<std::vector<RtpEncodingParameters>> init_send_encodings()
      const {
    return init_send_encodings_;
  }
  bool has_m_section() const { return has_m_section_; }
  bool newly_created() const { return newly_created_; }
  bool did_set_fired_direction() const { return fired_direction_.has_value(); }

  absl::optional<RtpTransceiverDirection> fired_direction() const {
    RTC_DCHECK(did_set_fired_direction());
    return fired_direction_.value();
  }

 private:
  absl::optional<std::string> mid_;
  absl::optional<size_t> mline_index_;
  absl::optional<std::vector<std::string>> remote_stream_ids_;
  absl::optional<std::vector<RtpEncodingParameters>> init_send_encodings_;

  bool has_m_section_ = false;

  bool newly_created_ = false;

  absl::optional<absl::optional<RtpTransceiverDirection>> fired_direction_;
};

class TransceiverList {
 public:

  std::vector<RtpTransceiverProxyRefPtr> List() const {
    RTC_DCHECK_RUN_ON(&sequence_checker_);
    return transceivers_;
  }

  std::vector<RtpTransceiverProxyRefPtr> UnsafeList() const {
    return transceivers_;
  }

  std::vector<RtpTransceiver*> ListInternal() const;

  void Add(RtpTransceiverProxyRefPtr transceiver) {
    RTC_DCHECK_RUN_ON(&sequence_checker_);
    transceivers_.push_back(transceiver);
  }
  void Remove(RtpTransceiverProxyRefPtr transceiver) {
    RTC_DCHECK_RUN_ON(&sequence_checker_);
    transceivers_.erase(
        std::remove(transceivers_.begin(), transceivers_.end(), transceiver),
        transceivers_.end());
  }
  RtpTransceiverProxyRefPtr FindBySender(
      rtc::scoped_refptr<RtpSenderInterface> sender) const;
  RtpTransceiverProxyRefPtr FindByMid(const std::string& mid) const;
  RtpTransceiverProxyRefPtr FindByMLineIndex(size_t mline_index) const;

  TransceiverStableState* StableState(RtpTransceiverProxyRefPtr transceiver) {
    RTC_DCHECK_RUN_ON(&sequence_checker_);
    return &(transceiver_stable_states_by_transceivers_[transceiver]);
  }

  void DiscardStableStates() {
    RTC_DCHECK_RUN_ON(&sequence_checker_);
    transceiver_stable_states_by_transceivers_.clear();
  }

  std::map<RtpTransceiverProxyRefPtr, TransceiverStableState>& StableStates() {
    RTC_DCHECK_RUN_ON(&sequence_checker_);
    return transceiver_stable_states_by_transceivers_;
  }

 private:
  RTC_NO_UNIQUE_ADDRESS SequenceChecker sequence_checker_;
  std::vector<RtpTransceiverProxyRefPtr> transceivers_;

  std::map<RtpTransceiverProxyRefPtr, TransceiverStableState>
      transceiver_stable_states_by_transceivers_
          RTC_GUARDED_BY(sequence_checker_);

  std::map<RtpTransceiverProxyRefPtr, std::vector<std::string>>
      remote_stream_ids_by_transceivers_ RTC_GUARDED_BY(sequence_checker_);
};

}

#endif
