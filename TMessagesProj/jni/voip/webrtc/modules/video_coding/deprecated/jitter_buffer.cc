/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "modules/video_coding/deprecated/jitter_buffer.h"

#include <algorithm>
#include <limits>
#include <utility>

#include "api/units/timestamp.h"
#include "modules/video_coding/deprecated/frame_buffer.h"
#include "modules/video_coding/deprecated/jitter_buffer_common.h"
#include "modules/video_coding/deprecated/packet.h"
#include "modules/video_coding/timing/inter_frame_delay_variation_calculator.h"
#include "modules/video_coding/timing/jitter_estimator.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "system_wrappers/include/clock.h"

namespace webrtc {

static const int64_t kDefaultRtt = 200;

typedef std::pair<uint32_t, VCMFrameBuffer*> FrameListPair;

bool IsKeyFrame(FrameListPair pair) {
  return pair.second->FrameType() == VideoFrameType::kVideoFrameKey;
}

bool HasNonEmptyState(FrameListPair pair) {
  return pair.second->GetState() != kStateEmpty;
}

void FrameList::InsertFrame(VCMFrameBuffer* frame) {
  insert(rbegin().base(), FrameListPair(frame->RtpTimestamp(), frame));
}

VCMFrameBuffer* FrameList::PopFrame(uint32_t timestamp) {
  FrameList::iterator it = find(timestamp);
  if (it == end())
    return NULL;
  VCMFrameBuffer* frame = it->second;
  erase(it);
  return frame;
}

VCMFrameBuffer* FrameList::Front() const {
  return begin()->second;
}

VCMFrameBuffer* FrameList::Back() const {
  return rbegin()->second;
}

int FrameList::RecycleFramesUntilKeyFrame(FrameList::iterator* key_frame_it,
                                          UnorderedFrameList* free_frames) {
  int drop_count = 0;
  FrameList::iterator it = begin();
  while (!empty()) {

    it->second->Reset();
    free_frames->push_back(it->second);
    erase(it++);
    ++drop_count;
    if (it != end() &&
        it->second->FrameType() == VideoFrameType::kVideoFrameKey) {
      *key_frame_it = it;
      return drop_count;
    }
  }
  *key_frame_it = end();
  return drop_count;
}

void FrameList::CleanUpOldOrEmptyFrames(VCMDecodingState* decoding_state,
                                        UnorderedFrameList* free_frames) {
  while (!empty()) {
    VCMFrameBuffer* oldest_frame = Front();
    bool remove_frame = false;
    if (oldest_frame->GetState() == kStateEmpty && size() > 1) {

      remove_frame = decoding_state->UpdateEmptyFrame(oldest_frame);
    } else {
      remove_frame = decoding_state->IsOldFrame(oldest_frame);
    }
    if (!remove_frame) {
      break;
    }
    free_frames->push_back(oldest_frame);
    erase(begin());
  }
}

void FrameList::Reset(UnorderedFrameList* free_frames) {
  while (!empty()) {
    begin()->second->Reset();
    free_frames->push_back(begin()->second);
    erase(begin());
  }
}

VCMJitterBuffer::VCMJitterBuffer(Clock* clock,
                                 std::unique_ptr<EventWrapper> event,
                                 const FieldTrialsView& field_trials)
    : clock_(clock),
      running_(false),
      frame_event_(std::move(event)),
      max_number_of_frames_(kStartNumberOfFrames),
      free_frames_(),
      decodable_frames_(),
      incomplete_frames_(),
      last_decoded_state_(),
      first_packet_since_reset_(true),
      num_consecutive_old_packets_(0),
      num_packets_(0),
      num_duplicated_packets_(0),
      jitter_estimate_(clock, field_trials),
      missing_sequence_numbers_(SequenceNumberLessThan()),
      latest_received_sequence_number_(0),
      max_nack_list_size_(0),
      max_packet_age_to_nack_(0),
      max_incomplete_time_ms_(0),
      average_packets_per_frame_(0.0f),
      frame_counter_(0) {
  for (int i = 0; i < kStartNumberOfFrames; i++)
    free_frames_.push_back(new VCMFrameBuffer());
}

VCMJitterBuffer::~VCMJitterBuffer() {
  Stop();
  for (UnorderedFrameList::iterator it = free_frames_.begin();
       it != free_frames_.end(); ++it) {
    delete *it;
  }
  for (FrameList::iterator it = incomplete_frames_.begin();
       it != incomplete_frames_.end(); ++it) {
    delete it->second;
  }
  for (FrameList::iterator it = decodable_frames_.begin();
       it != decodable_frames_.end(); ++it) {
    delete it->second;
  }
}

void VCMJitterBuffer::Start() {
  MutexLock lock(&mutex_);
  running_ = true;

  num_consecutive_old_packets_ = 0;
  num_packets_ = 0;
  num_duplicated_packets_ = 0;

  waiting_for_completion_.frame_size = 0;
  waiting_for_completion_.timestamp = 0;
  waiting_for_completion_.latest_packet_time = -1;
  first_packet_since_reset_ = true;
  last_decoded_state_.Reset();

  decodable_frames_.Reset(&free_frames_);
  incomplete_frames_.Reset(&free_frames_);
}

void VCMJitterBuffer::Stop() {
  MutexLock lock(&mutex_);
  running_ = false;
  last_decoded_state_.Reset();

  frame_event_->Set();
}

bool VCMJitterBuffer::Running() const {
  MutexLock lock(&mutex_);
  return running_;
}

void VCMJitterBuffer::Flush() {
  MutexLock lock(&mutex_);
  FlushInternal();
}

void VCMJitterBuffer::FlushInternal() {
  decodable_frames_.Reset(&free_frames_);
  incomplete_frames_.Reset(&free_frames_);
  last_decoded_state_.Reset();
  num_consecutive_old_packets_ = 0;

  jitter_estimate_.Reset();
  inter_frame_delay_.Reset();
  waiting_for_completion_.frame_size = 0;
  waiting_for_completion_.timestamp = 0;
  waiting_for_completion_.latest_packet_time = -1;
  first_packet_since_reset_ = true;
  missing_sequence_numbers_.clear();
}

int VCMJitterBuffer::num_packets() const {
  MutexLock lock(&mutex_);
  return num_packets_;
}

int VCMJitterBuffer::num_duplicated_packets() const {
  MutexLock lock(&mutex_);
  return num_duplicated_packets_;
}

VCMEncodedFrame* VCMJitterBuffer::NextCompleteFrame(uint32_t max_wait_time_ms) {
  MutexLock lock(&mutex_);
  if (!running_) {
    return nullptr;
  }
  CleanUpOldOrEmptyFrames();

  if (decodable_frames_.empty() ||
      decodable_frames_.Front()->GetState() != kStateComplete) {
    const int64_t end_wait_time_ms =
        clock_->TimeInMilliseconds() + max_wait_time_ms;
    int64_t wait_time_ms = max_wait_time_ms;
    while (wait_time_ms > 0) {
      mutex_.Unlock();
      const EventTypeWrapper ret =
          frame_event_->Wait(static_cast<uint32_t>(wait_time_ms));
      mutex_.Lock();
      if (ret == kEventSignaled) {

        if (!running_) {
          return nullptr;
        }

        CleanUpOldOrEmptyFrames();
        if (decodable_frames_.empty() ||
            decodable_frames_.Front()->GetState() != kStateComplete) {
          wait_time_ms = end_wait_time_ms - clock_->TimeInMilliseconds();
        } else {
          break;
        }
      } else {
        break;
      }
    }
  }
  if (decodable_frames_.empty() ||
      decodable_frames_.Front()->GetState() != kStateComplete) {
    return nullptr;
  }
  return decodable_frames_.Front();
}

VCMEncodedFrame* VCMJitterBuffer::ExtractAndSetDecode(uint32_t timestamp) {
  MutexLock lock(&mutex_);
  if (!running_) {
    return NULL;
  }

  VCMFrameBuffer* frame = decodable_frames_.PopFrame(timestamp);
  bool continuous = true;
  if (!frame) {
    frame = incomplete_frames_.PopFrame(timestamp);
    if (frame)
      continuous = last_decoded_state_.ContinuousFrame(frame);
    else
      return NULL;
  }

  const bool retransmitted = (frame->GetNackCount() > 0);
  if (retransmitted) {
    jitter_estimate_.FrameNacked();
  } else if (frame->size() > 0) {

    if (waiting_for_completion_.latest_packet_time >= 0) {
      UpdateJitterEstimate(waiting_for_completion_, true);
    }
    if (frame->GetState() == kStateComplete) {
      UpdateJitterEstimate(*frame, false);
    } else {

      waiting_for_completion_.frame_size = frame->size();
      waiting_for_completion_.latest_packet_time = frame->LatestPacketTimeMs();
      waiting_for_completion_.timestamp = frame->RtpTimestamp();
    }
  }

  frame->PrepareForDecode(continuous);

  last_decoded_state_.SetState(frame);
  DropPacketsFromNackList(last_decoded_state_.sequence_num());

  UpdateAveragePacketsPerFrame(frame->NumPackets());

  return frame;
}

void VCMJitterBuffer::ReleaseFrame(VCMEncodedFrame* frame) {
  RTC_CHECK(frame != nullptr);
  MutexLock lock(&mutex_);
  VCMFrameBuffer* frame_buffer = static_cast<VCMFrameBuffer*>(frame);
  RecycleFrameBuffer(frame_buffer);
}

VCMFrameBufferEnum VCMJitterBuffer::GetFrame(const VCMPacket& packet,
                                             VCMFrameBuffer** frame,
                                             FrameList** frame_list) {
  *frame = incomplete_frames_.PopFrame(packet.timestamp);
  if (*frame != NULL) {
    *frame_list = &incomplete_frames_;
    return kNoError;
  }
  *frame = decodable_frames_.PopFrame(packet.timestamp);
  if (*frame != NULL) {
    *frame_list = &decodable_frames_;
    return kNoError;
  }

  *frame_list = NULL;

  *frame = GetEmptyFrame();
  if (*frame == NULL) {

    RTC_LOG(LS_WARNING) << "Unable to get empty frame; Recycling.";
    bool found_key_frame = RecycleFramesUntilKeyFrame();
    *frame = GetEmptyFrame();
    RTC_CHECK(*frame);
    if (!found_key_frame) {
      RecycleFrameBuffer(*frame);
      return kFlushIndicator;
    }
  }
  (*frame)->Reset();
  return kNoError;
}

int64_t VCMJitterBuffer::LastPacketTime(const VCMEncodedFrame* frame,
                                        bool* retransmitted) const {
  RTC_DCHECK(retransmitted);
  MutexLock lock(&mutex_);
  const VCMFrameBuffer* frame_buffer =
      static_cast<const VCMFrameBuffer*>(frame);
  *retransmitted = (frame_buffer->GetNackCount() > 0);
  return frame_buffer->LatestPacketTimeMs();
}

VCMFrameBufferEnum VCMJitterBuffer::InsertPacket(const VCMPacket& packet,
                                                 bool* retransmitted) {
  MutexLock lock(&mutex_);

  ++num_packets_;

  if (last_decoded_state_.IsOldPacket(&packet)) {

    if (packet.sizeBytes > 0) {
      num_consecutive_old_packets_++;
    }

    last_decoded_state_.UpdateOldPacket(&packet);
    DropPacketsFromNackList(last_decoded_state_.sequence_num());

    FindAndInsertContinuousFramesWithState(last_decoded_state_);

    if (num_consecutive_old_packets_ > kMaxConsecutiveOldPackets) {
      RTC_LOG(LS_WARNING)
          << num_consecutive_old_packets_
          << " consecutive old packets received. Flushing the jitter buffer.";
      FlushInternal();
      return kFlushIndicator;
    }
    return kOldPacket;
  }

  num_consecutive_old_packets_ = 0;

  VCMFrameBuffer* frame;
  FrameList* frame_list;
  const VCMFrameBufferEnum error = GetFrame(packet, &frame, &frame_list);
  if (error != kNoError)
    return error;

  Timestamp now = clock_->CurrentTime();

  if (first_packet_since_reset_) {

    inter_frame_delay_.Reset();
  }

  if (packet.video_header.frame_type != VideoFrameType::kEmptyFrame) {
    if (waiting_for_completion_.timestamp == packet.timestamp) {

      waiting_for_completion_.frame_size += packet.sizeBytes;
      waiting_for_completion_.latest_packet_time = now.ms();
    } else if (waiting_for_completion_.latest_packet_time >= 0 &&
               waiting_for_completion_.latest_packet_time + 2000 <= now.ms()) {

      UpdateJitterEstimate(waiting_for_completion_, true);
      waiting_for_completion_.latest_packet_time = -1;
      waiting_for_completion_.frame_size = 0;
      waiting_for_completion_.timestamp = 0;
    }
  }

  VCMFrameBufferStateEnum previous_state = frame->GetState();

  FrameData frame_data;
  frame_data.rtt_ms = kDefaultRtt;
  frame_data.rolling_average_packets_per_frame = average_packets_per_frame_;
  VCMFrameBufferEnum buffer_state =
      frame->InsertPacket(packet, now.ms(), frame_data);

  if (buffer_state > 0) {
    if (first_packet_since_reset_) {
      latest_received_sequence_number_ = packet.seqNum;
      first_packet_since_reset_ = false;
    } else {
      if (IsPacketRetransmitted(packet)) {
        frame->IncrementNackCount();
      }
      if (!UpdateNackList(packet.seqNum) &&
          packet.video_header.frame_type != VideoFrameType::kVideoFrameKey) {
        buffer_state = kFlushIndicator;
      }

      latest_received_sequence_number_ =
          LatestSequenceNumber(latest_received_sequence_number_, packet.seqNum);
    }
  }

  bool continuous = IsContinuous(*frame);
  switch (buffer_state) {
    case kGeneralError:
    case kTimeStampError:
    case kSizeError: {
      RecycleFrameBuffer(frame);
      break;
    }
    case kCompleteSession: {
      if (previous_state != kStateComplete) {
        if (continuous) {

          frame_event_->Set();
        }
      }

      *retransmitted = (frame->GetNackCount() > 0);
      if (continuous) {
        decodable_frames_.InsertFrame(frame);
        FindAndInsertContinuousFrames(*frame);
      } else {
        incomplete_frames_.InsertFrame(frame);
      }
      break;
    }
    case kIncomplete: {
      if (frame->GetState() == kStateEmpty &&
          last_decoded_state_.UpdateEmptyFrame(frame)) {
        RecycleFrameBuffer(frame);
        return kNoError;
      } else {
        incomplete_frames_.InsertFrame(frame);
      }
      break;
    }
    case kNoError:
    case kOutOfBoundsPacket:
    case kDuplicatePacket: {

      if (frame_list != NULL) {
        frame_list->InsertFrame(frame);
      } else {
        RecycleFrameBuffer(frame);
      }
      ++num_duplicated_packets_;
      break;
    }
    case kFlushIndicator:
      RecycleFrameBuffer(frame);
      return kFlushIndicator;
    default:
      RTC_DCHECK_NOTREACHED();
  }
  return buffer_state;
}

bool VCMJitterBuffer::IsContinuousInState(
    const VCMFrameBuffer& frame,
    const VCMDecodingState& decoding_state) const {

  return (frame.GetState() == kStateComplete) &&
         decoding_state.ContinuousFrame(&frame);
}

bool VCMJitterBuffer::IsContinuous(const VCMFrameBuffer& frame) const {
  if (IsContinuousInState(frame, last_decoded_state_)) {
    return true;
  }
  VCMDecodingState decoding_state;
  decoding_state.CopyFrom(last_decoded_state_);
  for (FrameList::const_iterator it = decodable_frames_.begin();
       it != decodable_frames_.end(); ++it) {
    VCMFrameBuffer* decodable_frame = it->second;
    if (IsNewerTimestamp(decodable_frame->RtpTimestamp(),
                         frame.RtpTimestamp())) {
      break;
    }
    decoding_state.SetState(decodable_frame);
    if (IsContinuousInState(frame, decoding_state)) {
      return true;
    }
  }
  return false;
}

void VCMJitterBuffer::FindAndInsertContinuousFrames(
    const VCMFrameBuffer& new_frame) {
  VCMDecodingState decoding_state;
  decoding_state.CopyFrom(last_decoded_state_);
  decoding_state.SetState(&new_frame);
  FindAndInsertContinuousFramesWithState(decoding_state);
}

void VCMJitterBuffer::FindAndInsertContinuousFramesWithState(
    const VCMDecodingState& original_decoded_state) {

  VCMDecodingState decoding_state;
  decoding_state.CopyFrom(original_decoded_state);

  for (FrameList::iterator it = incomplete_frames_.begin();
       it != incomplete_frames_.end();) {
    VCMFrameBuffer* frame = it->second;
    if (IsNewerTimestamp(original_decoded_state.time_stamp(),
                         frame->RtpTimestamp())) {
      ++it;
      continue;
    }
    if (IsContinuousInState(*frame, decoding_state)) {
      decodable_frames_.InsertFrame(frame);
      incomplete_frames_.erase(it++);
      decoding_state.SetState(frame);
    } else if (frame->TemporalId() <= 0) {
      break;
    } else {
      ++it;
    }
  }
}

uint32_t VCMJitterBuffer::EstimatedJitterMs() {
  MutexLock lock(&mutex_);
  const double rtt_mult = 1.0f;
  return jitter_estimate_.GetJitterEstimate(rtt_mult, absl::nullopt).ms();
}

void VCMJitterBuffer::SetNackSettings(size_t max_nack_list_size,
                                      int max_packet_age_to_nack,
                                      int max_incomplete_time_ms) {
  MutexLock lock(&mutex_);
  RTC_DCHECK_GE(max_packet_age_to_nack, 0);
  RTC_DCHECK_GE(max_incomplete_time_ms_, 0);
  max_nack_list_size_ = max_nack_list_size;
  max_packet_age_to_nack_ = max_packet_age_to_nack;
  max_incomplete_time_ms_ = max_incomplete_time_ms;
}

int VCMJitterBuffer::NonContinuousOrIncompleteDuration() {
  if (incomplete_frames_.empty()) {
    return 0;
  }
  uint32_t start_timestamp = incomplete_frames_.Front()->RtpTimestamp();
  if (!decodable_frames_.empty()) {
    start_timestamp = decodable_frames_.Back()->RtpTimestamp();
  }
  return incomplete_frames_.Back()->RtpTimestamp() - start_timestamp;
}

uint16_t VCMJitterBuffer::EstimatedLowSequenceNumber(
    const VCMFrameBuffer& frame) const {
  RTC_DCHECK_GE(frame.GetLowSeqNum(), 0);
  if (frame.HaveFirstPacket())
    return frame.GetLowSeqNum();

  return frame.GetLowSeqNum() - 1;
}

std::vector<uint16_t> VCMJitterBuffer::GetNackList(bool* request_key_frame) {
  MutexLock lock(&mutex_);
  *request_key_frame = false;
  if (last_decoded_state_.in_initial_state()) {
    VCMFrameBuffer* next_frame = NextFrame();
    const bool first_frame_is_key =
        next_frame &&
        next_frame->FrameType() == VideoFrameType::kVideoFrameKey &&
        next_frame->HaveFirstPacket();
    if (!first_frame_is_key) {
      bool have_non_empty_frame =
          decodable_frames_.end() != find_if(decodable_frames_.begin(),
                                             decodable_frames_.end(),
                                             HasNonEmptyState);
      if (!have_non_empty_frame) {
        have_non_empty_frame =
            incomplete_frames_.end() != find_if(incomplete_frames_.begin(),
                                                incomplete_frames_.end(),
                                                HasNonEmptyState);
      }
      bool found_key_frame = RecycleFramesUntilKeyFrame();
      if (!found_key_frame) {
        *request_key_frame = have_non_empty_frame;
        return std::vector<uint16_t>();
      }
    }
  }
  if (TooLargeNackList()) {
    *request_key_frame = !HandleTooLargeNackList();
  }
  if (max_incomplete_time_ms_ > 0) {
    int non_continuous_incomplete_duration =
        NonContinuousOrIncompleteDuration();
    if (non_continuous_incomplete_duration > 90 * max_incomplete_time_ms_) {
      RTC_LOG_F(LS_WARNING) << "Too long non-decodable duration: "
                            << non_continuous_incomplete_duration << " > "
                            << 90 * max_incomplete_time_ms_;
      FrameList::reverse_iterator rit = find_if(
          incomplete_frames_.rbegin(), incomplete_frames_.rend(), IsKeyFrame);
      if (rit == incomplete_frames_.rend()) {

        *request_key_frame = true;
        return std::vector<uint16_t>();
      } else {

        last_decoded_state_.Reset();
        DropPacketsFromNackList(EstimatedLowSequenceNumber(*rit->second));
      }
    }
  }
  std::vector<uint16_t> nack_list(missing_sequence_numbers_.begin(),
                                  missing_sequence_numbers_.end());
  return nack_list;
}

VCMFrameBuffer* VCMJitterBuffer::NextFrame() const {
  if (!decodable_frames_.empty())
    return decodable_frames_.Front();
  if (!incomplete_frames_.empty())
    return incomplete_frames_.Front();
  return NULL;
}

bool VCMJitterBuffer::UpdateNackList(uint16_t sequence_number) {

  if (!last_decoded_state_.in_initial_state()) {
    latest_received_sequence_number_ = LatestSequenceNumber(
        latest_received_sequence_number_, last_decoded_state_.sequence_num());
  }
  if (IsNewerSequenceNumber(sequence_number,
                            latest_received_sequence_number_)) {

    for (uint16_t i = latest_received_sequence_number_ + 1;
         IsNewerSequenceNumber(sequence_number, i); ++i) {
      missing_sequence_numbers_.insert(missing_sequence_numbers_.end(), i);
    }
    if (TooLargeNackList() && !HandleTooLargeNackList()) {
      RTC_LOG(LS_WARNING) << "Requesting key frame due to too large NACK list.";
      return false;
    }
    if (MissingTooOldPacket(sequence_number) &&
        !HandleTooOldPackets(sequence_number)) {
      RTC_LOG(LS_WARNING)
          << "Requesting key frame due to missing too old packets";
      return false;
    }
  } else {
    missing_sequence_numbers_.erase(sequence_number);
  }
  return true;
}

bool VCMJitterBuffer::TooLargeNackList() const {
  return missing_sequence_numbers_.size() > max_nack_list_size_;
}

bool VCMJitterBuffer::HandleTooLargeNackList() {

  RTC_LOG_F(LS_WARNING) << "NACK list has grown too large: "
                        << missing_sequence_numbers_.size() << " > "
                        << max_nack_list_size_;
  bool key_frame_found = false;
  while (TooLargeNackList()) {
    key_frame_found = RecycleFramesUntilKeyFrame();
  }
  return key_frame_found;
}

bool VCMJitterBuffer::MissingTooOldPacket(
    uint16_t latest_sequence_number) const {
  if (missing_sequence_numbers_.empty()) {
    return false;
  }
  const uint16_t age_of_oldest_missing_packet =
      latest_sequence_number - *missing_sequence_numbers_.begin();

  return age_of_oldest_missing_packet > max_packet_age_to_nack_;
}

bool VCMJitterBuffer::HandleTooOldPackets(uint16_t latest_sequence_number) {
  bool key_frame_found = false;
  const uint16_t age_of_oldest_missing_packet =
      latest_sequence_number - *missing_sequence_numbers_.begin();
  RTC_LOG_F(LS_WARNING) << "NACK list contains too old sequence numbers: "
                        << age_of_oldest_missing_packet << " > "
                        << max_packet_age_to_nack_;
  while (MissingTooOldPacket(latest_sequence_number)) {
    key_frame_found = RecycleFramesUntilKeyFrame();
  }
  return key_frame_found;
}

void VCMJitterBuffer::DropPacketsFromNackList(
    uint16_t last_decoded_sequence_number) {

  missing_sequence_numbers_.erase(
      missing_sequence_numbers_.begin(),
      missing_sequence_numbers_.upper_bound(last_decoded_sequence_number));
}

VCMFrameBuffer* VCMJitterBuffer::GetEmptyFrame() {
  if (free_frames_.empty()) {
    if (!TryToIncreaseJitterBufferSize()) {
      return NULL;
    }
  }
  VCMFrameBuffer* frame = free_frames_.front();
  free_frames_.pop_front();
  return frame;
}

bool VCMJitterBuffer::TryToIncreaseJitterBufferSize() {
  if (max_number_of_frames_ >= kMaxNumberOfFrames)
    return false;
  free_frames_.push_back(new VCMFrameBuffer());
  ++max_number_of_frames_;
  return true;
}

bool VCMJitterBuffer::RecycleFramesUntilKeyFrame() {

  FrameList::iterator key_frame_it;
  bool key_frame_found = false;
  int dropped_frames = 0;
  dropped_frames += incomplete_frames_.RecycleFramesUntilKeyFrame(
      &key_frame_it, &free_frames_);
  key_frame_found = key_frame_it != incomplete_frames_.end();
  if (dropped_frames == 0) {
    dropped_frames += decodable_frames_.RecycleFramesUntilKeyFrame(
        &key_frame_it, &free_frames_);
    key_frame_found = key_frame_it != decodable_frames_.end();
  }
  if (key_frame_found) {
    RTC_LOG(LS_INFO) << "Found key frame while dropping frames.";

    last_decoded_state_.Reset();
    DropPacketsFromNackList(EstimatedLowSequenceNumber(*key_frame_it->second));
  } else if (decodable_frames_.empty()) {

    last_decoded_state_.Reset();
    missing_sequence_numbers_.clear();
  }
  return key_frame_found;
}

void VCMJitterBuffer::UpdateAveragePacketsPerFrame(int current_number_packets) {
  if (frame_counter_ > kFastConvergeThreshold) {
    average_packets_per_frame_ =
        average_packets_per_frame_ * (1 - kNormalConvergeMultiplier) +
        current_number_packets * kNormalConvergeMultiplier;
  } else if (frame_counter_ > 0) {
    average_packets_per_frame_ =
        average_packets_per_frame_ * (1 - kFastConvergeMultiplier) +
        current_number_packets * kFastConvergeMultiplier;
    frame_counter_++;
  } else {
    average_packets_per_frame_ = current_number_packets;
    frame_counter_++;
  }
}

void VCMJitterBuffer::CleanUpOldOrEmptyFrames() {
  decodable_frames_.CleanUpOldOrEmptyFrames(&last_decoded_state_,
                                            &free_frames_);
  incomplete_frames_.CleanUpOldOrEmptyFrames(&last_decoded_state_,
                                             &free_frames_);
  if (!last_decoded_state_.in_initial_state()) {
    DropPacketsFromNackList(last_decoded_state_.sequence_num());
  }
}

bool VCMJitterBuffer::IsPacketRetransmitted(const VCMPacket& packet) const {
  return missing_sequence_numbers_.find(packet.seqNum) !=
         missing_sequence_numbers_.end();
}

void VCMJitterBuffer::UpdateJitterEstimate(const VCMJitterSample& sample,
                                           bool incomplete_frame) {
  if (sample.latest_packet_time == -1) {
    return;
  }
  UpdateJitterEstimate(sample.latest_packet_time, sample.timestamp,
                       sample.frame_size, incomplete_frame);
}

void VCMJitterBuffer::UpdateJitterEstimate(const VCMFrameBuffer& frame,
                                           bool incomplete_frame) {
  if (frame.LatestPacketTimeMs() == -1) {
    return;
  }

  UpdateJitterEstimate(frame.LatestPacketTimeMs(), frame.RtpTimestamp(),
                       frame.size(), incomplete_frame);
}

void VCMJitterBuffer::UpdateJitterEstimate(int64_t latest_packet_time_ms,
                                           uint32_t timestamp,
                                           unsigned int frame_size,
                                           bool                     ) {
  if (latest_packet_time_ms == -1) {
    return;
  }
  auto frame_delay = inter_frame_delay_.Calculate(
      timestamp, Timestamp::Millis(latest_packet_time_ms));

  bool not_reordered = frame_delay.has_value();

  if (not_reordered) {

    jitter_estimate_.UpdateEstimate(*frame_delay, DataSize::Bytes(frame_size));
  }
}

void VCMJitterBuffer::RecycleFrameBuffer(VCMFrameBuffer* frame) {
  frame->Reset();
  free_frames_.push_back(frame);
}

}
