/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_VIDEO_CODING_DEPRECATED_JITTER_BUFFER_H_
#define MODULES_VIDEO_CODING_DEPRECATED_JITTER_BUFFER_H_

#include <list>
#include <map>
#include <memory>
#include <set>
#include <vector>

#include "api/field_trials_view.h"
#include "modules/include/module_common_types.h"
#include "modules/include/module_common_types_public.h"
#include "modules/video_coding/deprecated/decoding_state.h"
#include "modules/video_coding/deprecated/event_wrapper.h"
#include "modules/video_coding/deprecated/jitter_buffer_common.h"
#include "modules/video_coding/include/video_coding_defines.h"
#include "modules/video_coding/timing/inter_frame_delay_variation_calculator.h"
#include "modules/video_coding/timing/jitter_estimator.h"
#include "rtc_base/synchronization/mutex.h"
#include "rtc_base/thread_annotations.h"

namespace webrtc {

class Clock;
class VCMFrameBuffer;
class VCMPacket;
class VCMEncodedFrame;

typedef std::list<VCMFrameBuffer*> UnorderedFrameList;

struct VCMJitterSample {
  VCMJitterSample() : timestamp(0), frame_size(0), latest_packet_time(-1) {}
  uint32_t timestamp;
  uint32_t frame_size;
  int64_t latest_packet_time;
};

class TimestampLessThan {
 public:
  bool operator()(uint32_t timestamp1, uint32_t timestamp2) const {
    return IsNewerTimestamp(timestamp2, timestamp1);
  }
};

class FrameList
    : public std::map<uint32_t, VCMFrameBuffer*, TimestampLessThan> {
 public:
  void InsertFrame(VCMFrameBuffer* frame);
  VCMFrameBuffer* PopFrame(uint32_t timestamp);
  VCMFrameBuffer* Front() const;
  VCMFrameBuffer* Back() const;
  int RecycleFramesUntilKeyFrame(FrameList::iterator* key_frame_it,
                                 UnorderedFrameList* free_frames);
  void CleanUpOldOrEmptyFrames(VCMDecodingState* decoding_state,
                               UnorderedFrameList* free_frames);
  void Reset(UnorderedFrameList* free_frames);
};

class VCMJitterBuffer {
 public:
  VCMJitterBuffer(Clock* clock,
                  std::unique_ptr<EventWrapper> event,
                  const FieldTrialsView& field_trials);

  ~VCMJitterBuffer();

  VCMJitterBuffer(const VCMJitterBuffer&) = delete;
  VCMJitterBuffer& operator=(const VCMJitterBuffer&) = delete;

  void Start() RTC_LOCKS_EXCLUDED(mutex_);

  void Stop() RTC_LOCKS_EXCLUDED(mutex_);

  bool Running() const RTC_LOCKS_EXCLUDED(mutex_);

  void Flush() RTC_LOCKS_EXCLUDED(mutex_);

  int num_packets() const RTC_LOCKS_EXCLUDED(mutex_);

  int num_duplicated_packets() const RTC_LOCKS_EXCLUDED(mutex_);

  VCMEncodedFrame* NextCompleteFrame(uint32_t max_wait_time_ms)
      RTC_LOCKS_EXCLUDED(mutex_);

  VCMEncodedFrame* ExtractAndSetDecode(uint32_t timestamp)
      RTC_LOCKS_EXCLUDED(mutex_);

  void ReleaseFrame(VCMEncodedFrame* frame) RTC_LOCKS_EXCLUDED(mutex_);

  int64_t LastPacketTime(const VCMEncodedFrame* frame,
                         bool* retransmitted) const RTC_LOCKS_EXCLUDED(mutex_);

  VCMFrameBufferEnum InsertPacket(const VCMPacket& packet, bool* retransmitted)
      RTC_LOCKS_EXCLUDED(mutex_);

  uint32_t EstimatedJitterMs() RTC_LOCKS_EXCLUDED(mutex_);

  void SetNackSettings(size_t max_nack_list_size,
                       int max_packet_age_to_nack,
                       int max_incomplete_time_ms) RTC_LOCKS_EXCLUDED(mutex_);

  std::vector<uint16_t> GetNackList(bool* request_key_frame)
      RTC_LOCKS_EXCLUDED(mutex_);

 private:
  class SequenceNumberLessThan {
   public:
    bool operator()(const uint16_t& sequence_number1,
                    const uint16_t& sequence_number2) const {
      return IsNewerSequenceNumber(sequence_number2, sequence_number1);
    }
  };
  typedef std::set<uint16_t, SequenceNumberLessThan> SequenceNumberSet;

  VCMFrameBufferEnum GetFrame(const VCMPacket& packet,
                              VCMFrameBuffer** frame,
                              FrameList** frame_list)
      RTC_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  bool IsContinuousInState(const VCMFrameBuffer& frame,
                           const VCMDecodingState& decoding_state) const
      RTC_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  bool IsContinuous(const VCMFrameBuffer& frame) const
      RTC_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  void FindAndInsertContinuousFramesWithState(
      const VCMDecodingState& decoded_state)
      RTC_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  void FindAndInsertContinuousFrames(const VCMFrameBuffer& new_frame)
      RTC_EXCLUSIVE_LOCKS_REQUIRED(mutex_);
  VCMFrameBuffer* NextFrame() const RTC_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  bool UpdateNackList(uint16_t sequence_number)
      RTC_EXCLUSIVE_LOCKS_REQUIRED(mutex_);
  bool TooLargeNackList() const;

  bool HandleTooLargeNackList() RTC_EXCLUSIVE_LOCKS_REQUIRED(mutex_);
  bool MissingTooOldPacket(uint16_t latest_sequence_number) const
      RTC_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  bool HandleTooOldPackets(uint16_t latest_sequence_number)
      RTC_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  void DropPacketsFromNackList(uint16_t last_decoded_sequence_number);

  VCMFrameBuffer* GetEmptyFrame() RTC_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  bool TryToIncreaseJitterBufferSize() RTC_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  bool RecycleFramesUntilKeyFrame() RTC_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  void UpdateAveragePacketsPerFrame(int current_number_packets_)
      RTC_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  void CleanUpOldOrEmptyFrames() RTC_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  bool IsPacketRetransmitted(const VCMPacket& packet) const;

  void UpdateJitterEstimate(const VCMJitterSample& sample,
                            bool incomplete_frame);
  void UpdateJitterEstimate(const VCMFrameBuffer& frame, bool incomplete_frame);
  void UpdateJitterEstimate(int64_t latest_packet_time_ms,
                            uint32_t timestamp,
                            unsigned int frame_size,
                            bool incomplete_frame);

  int NonContinuousOrIncompleteDuration() RTC_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  uint16_t EstimatedLowSequenceNumber(const VCMFrameBuffer& frame) const;

  void RecycleFrameBuffer(VCMFrameBuffer* frame)
      RTC_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  void FlushInternal() RTC_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  Clock* clock_;

  bool running_;
  mutable Mutex mutex_;

  std::unique_ptr<EventWrapper> frame_event_;

  int max_number_of_frames_;
  UnorderedFrameList free_frames_ RTC_GUARDED_BY(mutex_);
  FrameList decodable_frames_ RTC_GUARDED_BY(mutex_);
  FrameList incomplete_frames_ RTC_GUARDED_BY(mutex_);
  VCMDecodingState last_decoded_state_ RTC_GUARDED_BY(mutex_);
  bool first_packet_since_reset_;

  int num_consecutive_old_packets_;

  int num_packets_ RTC_GUARDED_BY(mutex_);

  int num_duplicated_packets_ RTC_GUARDED_BY(mutex_);

  JitterEstimator jitter_estimate_;

  InterFrameDelayVariationCalculator inter_frame_delay_;
  VCMJitterSample waiting_for_completion_;

  SequenceNumberSet missing_sequence_numbers_;
  uint16_t latest_received_sequence_number_;
  size_t max_nack_list_size_;
  int max_packet_age_to_nack_;
  int max_incomplete_time_ms_;

  float average_packets_per_frame_;

  int frame_counter_;
};
}

#endif
