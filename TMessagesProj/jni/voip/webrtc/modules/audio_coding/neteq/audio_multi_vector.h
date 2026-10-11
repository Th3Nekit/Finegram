/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_CODING_NETEQ_AUDIO_MULTI_VECTOR_H_
#define MODULES_AUDIO_CODING_NETEQ_AUDIO_MULTI_VECTOR_H_

#include <stdint.h>
#include <string.h>

#include <vector>

#include "api/array_view.h"
#include "modules/audio_coding/neteq/audio_vector.h"

namespace webrtc {

class AudioMultiVector {
 public:

  explicit AudioMultiVector(size_t N);

  AudioMultiVector(size_t N, size_t initial_size);

  virtual ~AudioMultiVector();

  AudioMultiVector(const AudioMultiVector&) = delete;
  AudioMultiVector& operator=(const AudioMultiVector&) = delete;

  virtual void Clear();

  virtual void Zeros(size_t length);

  virtual void CopyTo(AudioMultiVector* copy_to) const;

  void PushBackInterleaved(rtc::ArrayView<const int16_t> append_this);

  virtual void PushBack(const AudioMultiVector& append_this);

  virtual void PushBackFromIndex(const AudioMultiVector& append_this,
                                 size_t index);

  virtual void PopFront(size_t length);

  virtual void PopBack(size_t length);

  virtual size_t ReadInterleaved(size_t length, int16_t* destination) const;

  virtual size_t ReadInterleavedFromIndex(size_t start_index,
                                          size_t length,
                                          int16_t* destination) const;

  virtual size_t ReadInterleavedFromEnd(size_t length,
                                        int16_t* destination) const;

  virtual void OverwriteAt(const AudioMultiVector& insert_this,
                           size_t length,
                           size_t position);

  virtual void CrossFade(const AudioMultiVector& append_this,
                         size_t fade_length);

  virtual size_t Channels() const;

  virtual size_t Size() const;

  virtual void AssertSize(size_t required_size);

  virtual bool Empty() const;

  virtual void CopyChannel(size_t from_channel, size_t to_channel);

  const AudioVector& operator[](size_t index) const;
  AudioVector& operator[](size_t index);

 protected:
  std::vector<AudioVector*> channels_;
  size_t num_channels_;
};

}
#endif
