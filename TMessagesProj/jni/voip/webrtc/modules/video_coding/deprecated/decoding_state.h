/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_VIDEO_CODING_DEPRECATED_DECODING_STATE_H_
#define MODULES_VIDEO_CODING_DEPRECATED_DECODING_STATE_H_

#include <cstdint>
#include <map>
#include <set>
#include <vector>

namespace webrtc {

struct NaluInfo;
class VCMFrameBuffer;
class VCMPacket;

class VCMDecodingState {
 public:

  static const uint16_t kNumRefBits = 7;
  static const uint16_t kFrameDecodedLength = 1 << kNumRefBits;

  VCMDecodingState();
  ~VCMDecodingState();

  bool IsOldFrame(const VCMFrameBuffer* frame) const;

  bool IsOldPacket(const VCMPacket* packet) const;

  bool ContinuousFrame(const VCMFrameBuffer* frame) const;
  void SetState(const VCMFrameBuffer* frame);
  void CopyFrom(const VCMDecodingState& state);
  bool UpdateEmptyFrame(const VCMFrameBuffer* frame);

  void UpdateOldPacket(const VCMPacket* packet);
  void SetSeqNum(uint16_t new_seq_num);
  void Reset();
  uint32_t time_stamp() const;
  uint16_t sequence_num() const;

  bool in_initial_state() const;

  bool full_sync() const;

 private:
  void UpdateSyncState(const VCMFrameBuffer* frame);

  bool ContinuousPictureId(int picture_id) const;
  bool ContinuousSeqNum(uint16_t seq_num) const;
  bool ContinuousLayer(int temporal_id, int tl0_pic_id) const;
  bool ContinuousFrameRefs(const VCMFrameBuffer* frame) const;
  bool UsingPictureId(const VCMFrameBuffer* frame) const;
  bool UsingFlexibleMode(const VCMFrameBuffer* frame) const;
  bool AheadOfFramesDecodedClearedTo(uint16_t index) const;
  bool HaveSpsAndPps(const std::vector<NaluInfo>& nalus) const;

  uint16_t sequence_num_;
  uint32_t time_stamp_;
  int picture_id_;
  int temporal_id_;
  int tl0_pic_id_;
  bool full_sync_;
  bool in_initial_state_;

  bool frame_decoded_[kFrameDecodedLength];
  uint16_t frame_decoded_cleared_to_;
  std::set<int> received_sps_;
  std::map<int, int> received_pps_;
};

}

#endif
