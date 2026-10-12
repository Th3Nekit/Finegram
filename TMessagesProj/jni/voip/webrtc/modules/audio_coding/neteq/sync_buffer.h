/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_CODING_NETEQ_SYNC_BUFFER_H_
#define MODULES_AUDIO_CODING_NETEQ_SYNC_BUFFER_H_

#include <stddef.h>
#include <stdint.h>

#include <vector>

#include "api/audio/audio_frame.h"
#include "modules/audio_coding/neteq/audio_multi_vector.h"
#include "modules/audio_coding/neteq/audio_vector.h"
#include "rtc_base/buffer.h"

namespace webrtc {

class SyncBuffer : public AudioMultiVector {
 public:
  SyncBuffer(size_t channels, size_t length)
      : AudioMultiVector(channels, length),
        next_index_(length),
        end_timestamp_(0),
        dtmf_index_(0) {}

  SyncBuffer(const SyncBuffer&) = delete;
  SyncBuffer& operator=(const SyncBuffer&) = delete;

  size_t FutureLength() const;

  void PushBack(const AudioMultiVector& append_this) override;

  void PushBackInterleaved(const rtc::BufferT<int16_t>& append_this);

  void PushFrontZeros(size_t length);

  virtual void InsertZerosAtIndex(size_t length, size_t position);

  virtual void ReplaceAtIndex(const AudioMultiVector& insert_this,
                              size_t length,
                              size_t position);

  virtual void ReplaceAtIndex(const AudioMultiVector& insert_this,
                              size_t position);

  void GetNextAudioInterleaved(size_t requested_len, AudioFrame* output);

  void IncreaseEndTimestamp(uint32_t increment);

  void Flush();

  const AudioVector& Channel(size_t n) const { return *channels_[n]; }
  AudioVector& Channel(size_t n) { return *channels_[n]; }

  size_t next_index() const { return next_index_; }
  void set_next_index(size_t value);
  uint32_t end_timestamp() const { return end_timestamp_; }
  void set_end_timestamp(uint32_t value) { end_timestamp_ = value; }
  size_t dtmf_index() const { return dtmf_index_; }
  void set_dtmf_index(size_t value);

 private:
  size_t next_index_;
  uint32_t end_timestamp_;
  size_t dtmf_index_;
};

}
#endif
