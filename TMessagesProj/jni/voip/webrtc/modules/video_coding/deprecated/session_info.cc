/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/deprecated/session_info.h"

#include <string.h>

#include <vector>

#include "absl/types/variant.h"
#include "modules/include/module_common_types.h"
#include "modules/include/module_common_types_public.h"
#include "modules/video_coding/codecs/interface/common_constants.h"
#include "modules/video_coding/codecs/vp8/include/vp8_globals.h"
#include "modules/video_coding/deprecated/jitter_buffer_common.h"
#include "modules/video_coding/deprecated/packet.h"
#include "rtc_base/logging.h"

namespace webrtc {

namespace {

uint16_t BufferToUWord16(const uint8_t* dataBuffer) {
  return (dataBuffer[0] << 8) | dataBuffer[1];
}

}

VCMSessionInfo::VCMSessionInfo()
    : complete_(false),
      frame_type_(VideoFrameType::kVideoFrameDelta),
      packets_(),
      empty_seq_num_low_(-1),
      empty_seq_num_high_(-1),
      first_packet_seq_num_(-1),
      last_packet_seq_num_(-1) {}

VCMSessionInfo::~VCMSessionInfo() {}

void VCMSessionInfo::UpdateDataPointers(const uint8_t* old_base_ptr,
                                        const uint8_t* new_base_ptr) {
  for (PacketIterator it = packets_.begin(); it != packets_.end(); ++it)
    if ((*it).dataPtr != NULL) {
      RTC_DCHECK(old_base_ptr != NULL && new_base_ptr != NULL);
      (*it).dataPtr = new_base_ptr + ((*it).dataPtr - old_base_ptr);
    }
}

int VCMSessionInfo::LowSequenceNumber() const {
  if (packets_.empty())
    return empty_seq_num_low_;
  return packets_.front().seqNum;
}

int VCMSessionInfo::HighSequenceNumber() const {
  if (packets_.empty())
    return empty_seq_num_high_;
  if (empty_seq_num_high_ == -1)
    return packets_.back().seqNum;
  return LatestSequenceNumber(packets_.back().seqNum, empty_seq_num_high_);
}

int VCMSessionInfo::PictureId() const {
  if (packets_.empty())
    return kNoPictureId;
  if (packets_.front().video_header.codec == kVideoCodecVP8) {
    return absl::get<RTPVideoHeaderVP8>(
               packets_.front().video_header.video_type_header)
        .pictureId;
  } else if (packets_.front().video_header.codec == kVideoCodecVP9) {
    return absl::get<RTPVideoHeaderVP9>(
               packets_.front().video_header.video_type_header)
        .picture_id;
  } else {
    return kNoPictureId;
  }
}

int VCMSessionInfo::TemporalId() const {
  if (packets_.empty())
    return kNoTemporalIdx;
  if (packets_.front().video_header.codec == kVideoCodecVP8) {
    return absl::get<RTPVideoHeaderVP8>(
               packets_.front().video_header.video_type_header)
        .temporalIdx;
  } else if (packets_.front().video_header.codec == kVideoCodecVP9) {
    return absl::get<RTPVideoHeaderVP9>(
               packets_.front().video_header.video_type_header)
        .temporal_idx;
  } else {
    return kNoTemporalIdx;
  }
}

bool VCMSessionInfo::LayerSync() const {
  if (packets_.empty())
    return false;
  if (packets_.front().video_header.codec == kVideoCodecVP8) {
    return absl::get<RTPVideoHeaderVP8>(
               packets_.front().video_header.video_type_header)
        .layerSync;
  } else if (packets_.front().video_header.codec == kVideoCodecVP9) {
    return absl::get<RTPVideoHeaderVP9>(
               packets_.front().video_header.video_type_header)
        .temporal_up_switch;
  } else {
    return false;
  }
}

int VCMSessionInfo::Tl0PicId() const {
  if (packets_.empty())
    return kNoTl0PicIdx;
  if (packets_.front().video_header.codec == kVideoCodecVP8) {
    return absl::get<RTPVideoHeaderVP8>(
               packets_.front().video_header.video_type_header)
        .tl0PicIdx;
  } else if (packets_.front().video_header.codec == kVideoCodecVP9) {
    return absl::get<RTPVideoHeaderVP9>(
               packets_.front().video_header.video_type_header)
        .tl0_pic_idx;
  } else {
    return kNoTl0PicIdx;
  }
}

std::vector<NaluInfo> VCMSessionInfo::GetNaluInfos() const {
  if (packets_.empty() ||
      packets_.front().video_header.codec != kVideoCodecH264)
    return std::vector<NaluInfo>();
  std::vector<NaluInfo> nalu_infos;
  for (const VCMPacket& packet : packets_) {
    const auto& h264 =
        absl::get<RTPVideoHeaderH264>(packet.video_header.video_type_header);
    for (size_t i = 0; i < h264.nalus_length; ++i) {
      nalu_infos.push_back(h264.nalus[i]);
    }
  }
  return nalu_infos;
}

void VCMSessionInfo::SetGofInfo(const GofInfoVP9& gof_info, size_t idx) {
  if (packets_.empty())
    return;

  auto* vp9_header = absl::get_if<RTPVideoHeaderVP9>(
      &packets_.front().video_header.video_type_header);
  if (!vp9_header || vp9_header->flexible_mode)
    return;

  vp9_header->temporal_idx = gof_info.temporal_idx[idx];
  vp9_header->temporal_up_switch = gof_info.temporal_up_switch[idx];
  vp9_header->num_ref_pics = gof_info.num_ref_pics[idx];
  for (uint8_t i = 0; i < gof_info.num_ref_pics[idx]; ++i) {
    vp9_header->pid_diff[i] = gof_info.pid_diff[idx][i];
  }
}

void VCMSessionInfo::Reset() {
  complete_ = false;
  frame_type_ = VideoFrameType::kVideoFrameDelta;
  packets_.clear();
  empty_seq_num_low_ = -1;
  empty_seq_num_high_ = -1;
  first_packet_seq_num_ = -1;
  last_packet_seq_num_ = -1;
}

size_t VCMSessionInfo::SessionLength() const {
  size_t length = 0;
  for (PacketIteratorConst it = packets_.begin(); it != packets_.end(); ++it)
    length += (*it).sizeBytes;
  return length;
}

int VCMSessionInfo::NumPackets() const {
  return packets_.size();
}

size_t VCMSessionInfo::InsertBuffer(uint8_t* frame_buffer,
                                    PacketIterator packet_it) {
  VCMPacket& packet = *packet_it;
  PacketIterator it;

  size_t offset = 0;
  for (it = packets_.begin(); it != packet_it; ++it)
    offset += (*it).sizeBytes;

  const uint8_t* packet_buffer = packet.dataPtr;
  packet.dataPtr = frame_buffer + offset;

  const size_t kH264NALHeaderLengthInBytes = 1;
  const size_t kLengthFieldLength = 2;
  const auto* h264 =
      absl::get_if<RTPVideoHeaderH264>(&packet.video_header.video_type_header);
  if (h264 && h264->packetization_type == kH264StapA) {
    size_t required_length = 0;
    const uint8_t* nalu_ptr = packet_buffer + kH264NALHeaderLengthInBytes;
    while (nalu_ptr < packet_buffer + packet.sizeBytes) {
      size_t length = BufferToUWord16(nalu_ptr);
      required_length +=
          length + (packet.insertStartCode ? kH264StartCodeLengthBytes : 0);
      nalu_ptr += kLengthFieldLength + length;
    }
    ShiftSubsequentPackets(packet_it, required_length);
    nalu_ptr = packet_buffer + kH264NALHeaderLengthInBytes;
    uint8_t* frame_buffer_ptr = frame_buffer + offset;
    while (nalu_ptr < packet_buffer + packet.sizeBytes) {
      size_t length = BufferToUWord16(nalu_ptr);
      nalu_ptr += kLengthFieldLength;
      frame_buffer_ptr += Insert(nalu_ptr, length, packet.insertStartCode,
                                 const_cast<uint8_t*>(frame_buffer_ptr));
      nalu_ptr += length;
    }
    packet.sizeBytes = required_length;
    return packet.sizeBytes;
  }
  ShiftSubsequentPackets(
      packet_it, packet.sizeBytes +
                     (packet.insertStartCode ? kH264StartCodeLengthBytes : 0));

  packet.sizeBytes =
      Insert(packet_buffer, packet.sizeBytes, packet.insertStartCode,
             const_cast<uint8_t*>(packet.dataPtr));
  return packet.sizeBytes;
}

size_t VCMSessionInfo::Insert(const uint8_t* buffer,
                              size_t length,
                              bool insert_start_code,
                              uint8_t* frame_buffer) {
  if (!buffer || !frame_buffer) {
    return 0;
  }
  if (insert_start_code) {
    const unsigned char startCode[] = {0, 0, 0, 1};
    memcpy(frame_buffer, startCode, kH264StartCodeLengthBytes);
  }
  memcpy(frame_buffer + (insert_start_code ? kH264StartCodeLengthBytes : 0),
         buffer, length);
  length += (insert_start_code ? kH264StartCodeLengthBytes : 0);

  return length;
}

void VCMSessionInfo::ShiftSubsequentPackets(PacketIterator it,
                                            int steps_to_shift) {
  ++it;
  if (it == packets_.end())
    return;
  uint8_t* first_packet_ptr = const_cast<uint8_t*>((*it).dataPtr);
  int shift_length = 0;

  for (; it != packets_.end(); ++it) {
    shift_length += (*it).sizeBytes;
    if ((*it).dataPtr != NULL)
      (*it).dataPtr += steps_to_shift;
  }
  memmove(first_packet_ptr + steps_to_shift, first_packet_ptr, shift_length);
}

void VCMSessionInfo::UpdateCompleteSession() {
  if (HaveFirstPacket() && HaveLastPacket()) {

    bool complete_session = true;
    PacketIterator it = packets_.begin();
    PacketIterator prev_it = it;
    ++it;
    for (; it != packets_.end(); ++it) {
      if (!InSequence(it, prev_it)) {
        complete_session = false;
        break;
      }
      prev_it = it;
    }
    complete_ = complete_session;
  }
}

bool VCMSessionInfo::complete() const {
  return complete_;
}

VCMSessionInfo::PacketIterator VCMSessionInfo::FindNaluEnd(
    PacketIterator packet_it) const {
  if ((*packet_it).completeNALU == kNaluEnd ||
      (*packet_it).completeNALU == kNaluComplete) {
    return packet_it;
  }

  for (; packet_it != packets_.end(); ++packet_it) {
    if (((*packet_it).completeNALU == kNaluComplete &&
         (*packet_it).sizeBytes > 0) ||

        (*packet_it).completeNALU == kNaluStart)
      return --packet_it;
    if ((*packet_it).completeNALU == kNaluEnd)
      return packet_it;
  }

  return --packet_it;
}

size_t VCMSessionInfo::DeletePacketData(PacketIterator start,
                                        PacketIterator end) {
  size_t bytes_to_delete = 0;
  PacketIterator packet_after_end = end;
  ++packet_after_end;

  for (PacketIterator it = start; it != packet_after_end; ++it) {
    bytes_to_delete += (*it).sizeBytes;
    (*it).sizeBytes = 0;
    (*it).dataPtr = NULL;
  }
  if (bytes_to_delete > 0)
    ShiftSubsequentPackets(end, -static_cast<int>(bytes_to_delete));
  return bytes_to_delete;
}

VCMSessionInfo::PacketIterator VCMSessionInfo::FindNextPartitionBeginning(
    PacketIterator it) const {
  while (it != packets_.end()) {
    if (absl::get<RTPVideoHeaderVP8>((*it).video_header.video_type_header)
            .beginningOfPartition) {
      return it;
    }
    ++it;
  }
  return it;
}

VCMSessionInfo::PacketIterator VCMSessionInfo::FindPartitionEnd(
    PacketIterator it) const {
  RTC_DCHECK_EQ((*it).codec(), kVideoCodecVP8);
  PacketIterator prev_it = it;
  const int partition_id =
      absl::get<RTPVideoHeaderVP8>((*it).video_header.video_type_header)
          .partitionId;
  while (it != packets_.end()) {
    bool beginning =
        absl::get<RTPVideoHeaderVP8>((*it).video_header.video_type_header)
            .beginningOfPartition;
    int current_partition_id =
        absl::get<RTPVideoHeaderVP8>((*it).video_header.video_type_header)
            .partitionId;
    bool packet_loss_found = (!beginning && !InSequence(it, prev_it));
    if (packet_loss_found ||
        (beginning && current_partition_id != partition_id)) {

      return prev_it;
    }
    prev_it = it;
    ++it;
  }
  return prev_it;
}

bool VCMSessionInfo::InSequence(const PacketIterator& packet_it,
                                const PacketIterator& prev_packet_it) {

  return (packet_it == prev_packet_it ||
          (static_cast<uint16_t>((*prev_packet_it).seqNum + 1) ==
           (*packet_it).seqNum));
}

size_t VCMSessionInfo::MakeDecodable() {
  size_t return_length = 0;
  if (packets_.empty()) {
    return 0;
  }
  PacketIterator it = packets_.begin();

  if ((*it).completeNALU == kNaluIncomplete || (*it).completeNALU == kNaluEnd) {
    PacketIterator nalu_end = FindNaluEnd(it);
    return_length += DeletePacketData(it, nalu_end);
    it = nalu_end;
  }
  PacketIterator prev_it = it;

  for (; it != packets_.end(); ++it) {
    bool start_of_nalu = ((*it).completeNALU == kNaluStart ||
                          (*it).completeNALU == kNaluComplete);
    if (!start_of_nalu && !InSequence(it, prev_it)) {

      PacketIterator nalu_end = FindNaluEnd(it);
      return_length += DeletePacketData(it, nalu_end);
      it = nalu_end;
    }
    prev_it = it;
  }
  return return_length;
}

bool VCMSessionInfo::HaveFirstPacket() const {
  return !packets_.empty() && (first_packet_seq_num_ != -1);
}

bool VCMSessionInfo::HaveLastPacket() const {
  return !packets_.empty() && (last_packet_seq_num_ != -1);
}

int VCMSessionInfo::InsertPacket(const VCMPacket& packet,
                                 uint8_t* frame_buffer,
                                 const FrameData& frame_data) {
  if (packet.video_header.frame_type == VideoFrameType::kEmptyFrame) {

    InformOfEmptyPacket(packet.seqNum);
    return 0;
  }

  if (packets_.size() == kMaxPacketsInSession) {
    RTC_LOG(LS_ERROR) << "Max number of packets per frame has been reached.";
    return -1;
  }

  ReversePacketIterator rit = packets_.rbegin();
  for (; rit != packets_.rend(); ++rit)
    if (LatestSequenceNumber(packet.seqNum, (*rit).seqNum) == packet.seqNum)
      break;

  if (rit != packets_.rend() && (*rit).seqNum == packet.seqNum &&
      (*rit).sizeBytes > 0)
    return -2;

  if (packet.codec() == kVideoCodecH264) {
    frame_type_ = packet.video_header.frame_type;
    if (packet.is_first_packet_in_frame() &&
        (first_packet_seq_num_ == -1 ||
         IsNewerSequenceNumber(first_packet_seq_num_, packet.seqNum))) {
      first_packet_seq_num_ = packet.seqNum;
    }
    if (packet.markerBit &&
        (last_packet_seq_num_ == -1 ||
         IsNewerSequenceNumber(packet.seqNum, last_packet_seq_num_))) {
      last_packet_seq_num_ = packet.seqNum;
    }
  } else {

    if (packet.is_first_packet_in_frame() && first_packet_seq_num_ == -1) {

      frame_type_ = packet.video_header.frame_type;

      first_packet_seq_num_ = static_cast<int>(packet.seqNum);
    } else if (first_packet_seq_num_ != -1 &&
               IsNewerSequenceNumber(first_packet_seq_num_, packet.seqNum)) {
      RTC_LOG(LS_WARNING)
          << "Received packet with a sequence number which is out "
             "of frame boundaries";
      return -3;
    } else if (frame_type_ == VideoFrameType::kEmptyFrame &&
               packet.video_header.frame_type != VideoFrameType::kEmptyFrame) {

      frame_type_ = packet.video_header.frame_type;
    }

    if (packet.markerBit && last_packet_seq_num_ == -1) {
      last_packet_seq_num_ = static_cast<int>(packet.seqNum);
    } else if (last_packet_seq_num_ != -1 &&
               IsNewerSequenceNumber(packet.seqNum, last_packet_seq_num_)) {
      RTC_LOG(LS_WARNING)
          << "Received packet with a sequence number which is out "
             "of frame boundaries";
      return -3;
    }
  }

  PacketIterator packet_list_it = packets_.insert(rit.base(), packet);

  size_t returnLength = InsertBuffer(frame_buffer, packet_list_it);
  UpdateCompleteSession();

  return static_cast<int>(returnLength);
}

void VCMSessionInfo::InformOfEmptyPacket(uint16_t seq_num) {

  if (empty_seq_num_high_ == -1)
    empty_seq_num_high_ = seq_num;
  else
    empty_seq_num_high_ = LatestSequenceNumber(seq_num, empty_seq_num_high_);
  if (empty_seq_num_low_ == -1 ||
      IsNewerSequenceNumber(empty_seq_num_low_, seq_num))
    empty_seq_num_low_ = seq_num;
}

}
