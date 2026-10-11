/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/deprecated/decoding_state.h"

#include "common_video/h264/h264_common.h"
#include "modules/include/module_common_types_public.h"
#include "modules/video_coding/deprecated/frame_buffer.h"
#include "modules/video_coding/deprecated/jitter_buffer_common.h"
#include "modules/video_coding/deprecated/packet.h"
#include "rtc_base/logging.h"

namespace webrtc {

VCMDecodingState::VCMDecodingState()
    : sequence_num_(0),
      time_stamp_(0),
      picture_id_(kNoPictureId),
      temporal_id_(kNoTemporalIdx),
      tl0_pic_id_(kNoTl0PicIdx),
      full_sync_(true),
      in_initial_state_(true) {
  memset(frame_decoded_, 0, sizeof(frame_decoded_));
}

VCMDecodingState::~VCMDecodingState() {}

void VCMDecodingState::Reset() {

  sequence_num_ = 0;
  time_stamp_ = 0;
  picture_id_ = kNoPictureId;
  temporal_id_ = kNoTemporalIdx;
  tl0_pic_id_ = kNoTl0PicIdx;
  full_sync_ = true;
  in_initial_state_ = true;
  memset(frame_decoded_, 0, sizeof(frame_decoded_));
  received_sps_.clear();
  received_pps_.clear();
}

uint32_t VCMDecodingState::time_stamp() const {
  return time_stamp_;
}

uint16_t VCMDecodingState::sequence_num() const {
  return sequence_num_;
}

bool VCMDecodingState::IsOldFrame(const VCMFrameBuffer* frame) const {
  RTC_DCHECK(frame);
  if (in_initial_state_)
    return false;
  return !IsNewerTimestamp(frame->RtpTimestamp(), time_stamp_);
}

bool VCMDecodingState::IsOldPacket(const VCMPacket* packet) const {
  RTC_DCHECK(packet);
  if (in_initial_state_)
    return false;
  return !IsNewerTimestamp(packet->timestamp, time_stamp_);
}

void VCMDecodingState::SetState(const VCMFrameBuffer* frame) {
  RTC_DCHECK(frame);
  RTC_CHECK_GE(frame->GetHighSeqNum(), 0);
  if (!UsingFlexibleMode(frame))
    UpdateSyncState(frame);
  sequence_num_ = static_cast<uint16_t>(frame->GetHighSeqNum());
  time_stamp_ = frame->RtpTimestamp();
  picture_id_ = frame->PictureId();
  temporal_id_ = frame->TemporalId();
  tl0_pic_id_ = frame->Tl0PicId();

  for (const NaluInfo& nalu : frame->GetNaluInfos()) {
    if (nalu.type == H264::NaluType::kPps) {
      if (nalu.pps_id < 0) {
        RTC_LOG(LS_WARNING) << "Received pps without pps id.";
      } else if (nalu.sps_id < 0) {
        RTC_LOG(LS_WARNING) << "Received pps without sps id.";
      } else {
        received_pps_[nalu.pps_id] = nalu.sps_id;
      }
    } else if (nalu.type == H264::NaluType::kSps) {
      if (nalu.sps_id < 0) {
        RTC_LOG(LS_WARNING) << "Received sps without sps id.";
      } else {
        received_sps_.insert(nalu.sps_id);
      }
    }
  }

  if (UsingFlexibleMode(frame)) {
    uint16_t frame_index = picture_id_ % kFrameDecodedLength;
    if (in_initial_state_) {
      frame_decoded_cleared_to_ = frame_index;
    } else if (frame->FrameType() == VideoFrameType::kVideoFrameKey) {
      memset(frame_decoded_, 0, sizeof(frame_decoded_));
      frame_decoded_cleared_to_ = frame_index;
    } else {
      if (AheadOfFramesDecodedClearedTo(frame_index)) {
        while (frame_decoded_cleared_to_ != frame_index) {
          frame_decoded_cleared_to_ =
              (frame_decoded_cleared_to_ + 1) % kFrameDecodedLength;
          frame_decoded_[frame_decoded_cleared_to_] = false;
        }
      }
    }
    frame_decoded_[frame_index] = true;
  }

  in_initial_state_ = false;
}

void VCMDecodingState::CopyFrom(const VCMDecodingState& state) {
  sequence_num_ = state.sequence_num_;
  time_stamp_ = state.time_stamp_;
  picture_id_ = state.picture_id_;
  temporal_id_ = state.temporal_id_;
  tl0_pic_id_ = state.tl0_pic_id_;
  full_sync_ = state.full_sync_;
  in_initial_state_ = state.in_initial_state_;
  frame_decoded_cleared_to_ = state.frame_decoded_cleared_to_;
  memcpy(frame_decoded_, state.frame_decoded_, sizeof(frame_decoded_));
  received_sps_ = state.received_sps_;
  received_pps_ = state.received_pps_;
}

bool VCMDecodingState::UpdateEmptyFrame(const VCMFrameBuffer* frame) {
  bool empty_packet = frame->GetHighSeqNum() == frame->GetLowSeqNum();
  if (in_initial_state_ && empty_packet) {

    return true;
  }
  if ((empty_packet && ContinuousSeqNum(frame->GetHighSeqNum())) ||
      ContinuousFrame(frame)) {

    sequence_num_ = frame->GetHighSeqNum();
    time_stamp_ = frame->RtpTimestamp();
    return true;
  }
  return false;
}

void VCMDecodingState::UpdateOldPacket(const VCMPacket* packet) {
  RTC_DCHECK(packet);
  if (packet->timestamp == time_stamp_) {

    sequence_num_ = LatestSequenceNumber(packet->seqNum, sequence_num_);
  }
}

void VCMDecodingState::SetSeqNum(uint16_t new_seq_num) {
  sequence_num_ = new_seq_num;
}

bool VCMDecodingState::in_initial_state() const {
  return in_initial_state_;
}

bool VCMDecodingState::full_sync() const {
  return full_sync_;
}

void VCMDecodingState::UpdateSyncState(const VCMFrameBuffer* frame) {
  if (in_initial_state_)
    return;
  if (frame->TemporalId() == kNoTemporalIdx ||
      frame->Tl0PicId() == kNoTl0PicIdx) {
    full_sync_ = true;
  } else if (frame->FrameType() == VideoFrameType::kVideoFrameKey ||
             frame->LayerSync()) {
    full_sync_ = true;
  } else if (full_sync_) {

    if (UsingPictureId(frame)) {

      if (frame->Tl0PicId() - tl0_pic_id_ > 1) {
        full_sync_ = false;
      } else {
        full_sync_ = ContinuousPictureId(frame->PictureId());
      }
    } else {
      full_sync_ =
          ContinuousSeqNum(static_cast<uint16_t>(frame->GetLowSeqNum()));
    }
  }
}

bool VCMDecodingState::ContinuousFrame(const VCMFrameBuffer* frame) const {

  RTC_DCHECK(frame);

  if (frame->FrameType() == VideoFrameType::kVideoFrameKey &&
      HaveSpsAndPps(frame->GetNaluInfos())) {
    return true;
  }

  if (in_initial_state_)
    return false;
  if (ContinuousLayer(frame->TemporalId(), frame->Tl0PicId()))
    return true;

  if (frame->Tl0PicId() != tl0_pic_id_)
    return false;

  if (!full_sync_ && !frame->LayerSync())
    return false;
  if (UsingPictureId(frame)) {
    if (UsingFlexibleMode(frame)) {
      return ContinuousFrameRefs(frame);
    } else {
      return ContinuousPictureId(frame->PictureId());
    }
  } else {
    return ContinuousSeqNum(static_cast<uint16_t>(frame->GetLowSeqNum())) &&
           HaveSpsAndPps(frame->GetNaluInfos());
  }
}

bool VCMDecodingState::ContinuousPictureId(int picture_id) const {
  int next_picture_id = picture_id_ + 1;
  if (picture_id < picture_id_) {

    if (picture_id_ >= 0x80) {

      return ((next_picture_id & 0x7FFF) == picture_id);
    } else {

      return ((next_picture_id & 0x7F) == picture_id);
    }
  }

  return (next_picture_id == picture_id);
}

bool VCMDecodingState::ContinuousSeqNum(uint16_t seq_num) const {
  return seq_num == static_cast<uint16_t>(sequence_num_ + 1);
}

bool VCMDecodingState::ContinuousLayer(int temporal_id, int tl0_pic_id) const {

  if (temporal_id == kNoTemporalIdx || tl0_pic_id == kNoTl0PicIdx)
    return false;

  else if (tl0_pic_id_ == kNoTl0PicIdx && temporal_id_ == kNoTemporalIdx &&
           temporal_id == 0)
    return true;

  if (temporal_id != 0)
    return false;
  return (static_cast<uint8_t>(tl0_pic_id_ + 1) == tl0_pic_id);
}

bool VCMDecodingState::ContinuousFrameRefs(const VCMFrameBuffer* frame) const {
  uint8_t num_refs = frame->CodecSpecific()->codecSpecific.VP9.num_ref_pics;
  for (uint8_t r = 0; r < num_refs; ++r) {
    uint16_t frame_ref = frame->PictureId() -
                         frame->CodecSpecific()->codecSpecific.VP9.p_diff[r];
    uint16_t frame_index = frame_ref % kFrameDecodedLength;
    if (AheadOfFramesDecodedClearedTo(frame_index) ||
        !frame_decoded_[frame_index]) {
      return false;
    }
  }
  return true;
}

bool VCMDecodingState::UsingPictureId(const VCMFrameBuffer* frame) const {
  return (frame->PictureId() != kNoPictureId && picture_id_ != kNoPictureId);
}

bool VCMDecodingState::UsingFlexibleMode(const VCMFrameBuffer* frame) const {
  bool is_flexible_mode =
      frame->CodecSpecific()->codecType == kVideoCodecVP9 &&
      frame->CodecSpecific()->codecSpecific.VP9.flexible_mode;
  if (is_flexible_mode && frame->PictureId() == kNoPictureId) {
    RTC_LOG(LS_WARNING) << "Frame is marked as using flexible mode but no"
                           "picture id is set.";
    return false;
  }
  return is_flexible_mode;
}

bool VCMDecodingState::AheadOfFramesDecodedClearedTo(uint16_t index) const {

  uint16_t diff =
      index > frame_decoded_cleared_to_
          ? kFrameDecodedLength - (index - frame_decoded_cleared_to_)
          : frame_decoded_cleared_to_ - index;
  return diff > kFrameDecodedLength / 2;
}

bool VCMDecodingState::HaveSpsAndPps(const std::vector<NaluInfo>& nalus) const {
  std::set<int> new_sps;
  std::map<int, int> new_pps;
  for (const NaluInfo& nalu : nalus) {

    if (nalu.sps_id == -1 && nalu.pps_id == -1)
      continue;
    switch (nalu.type) {
      case H264::NaluType::kPps:
        if (nalu.pps_id < 0) {
          RTC_LOG(LS_WARNING) << "Received pps without pps id.";
        } else if (nalu.sps_id < 0) {
          RTC_LOG(LS_WARNING) << "Received pps without sps id.";
        } else {
          new_pps[nalu.pps_id] = nalu.sps_id;
        }
        break;
      case H264::NaluType::kSps:
        if (nalu.sps_id < 0) {
          RTC_LOG(LS_WARNING) << "Received sps without sps id.";
        } else {
          new_sps.insert(nalu.sps_id);
        }
        break;
      default: {
        int needed_sps = -1;
        auto pps_it = new_pps.find(nalu.pps_id);
        if (pps_it != new_pps.end()) {
          needed_sps = pps_it->second;
        } else {
          auto pps_it2 = received_pps_.find(nalu.pps_id);
          if (pps_it2 == received_pps_.end()) {
            return false;
          }
          needed_sps = pps_it2->second;
        }
        if (new_sps.find(needed_sps) == new_sps.end() &&
            received_sps_.find(needed_sps) == received_sps_.end()) {
          return false;
        }
        break;
      }
    }
  }
  return true;
}

}
