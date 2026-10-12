/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_CODING_NETEQ_AUDIO_VECTOR_H_
#define MODULES_AUDIO_CODING_NETEQ_AUDIO_VECTOR_H_

#include <string.h>

#include <cstdint>
#include <memory>

#include "rtc_base/checks.h"

namespace webrtc {

class AudioVector {
 public:

  AudioVector();

  explicit AudioVector(size_t initial_size);

  virtual ~AudioVector();

  AudioVector(const AudioVector&) = delete;
  AudioVector& operator=(const AudioVector&) = delete;

  virtual void Clear();

  virtual void CopyTo(AudioVector* copy_to) const;

  virtual void CopyTo(size_t length, size_t position, int16_t* copy_to) const;

  virtual void PushFront(const AudioVector& prepend_this);

  virtual void PushFront(const int16_t* prepend_this, size_t length);

  virtual void PushBack(const AudioVector& append_this);

  virtual void PushBack(const AudioVector& append_this,
                        size_t length,
                        size_t position);

  virtual void PushBack(const int16_t* append_this, size_t length);

  virtual void PopFront(size_t length);

  virtual void PopBack(size_t length);

  virtual void Extend(size_t extra_length);

  virtual void InsertAt(const int16_t* insert_this,
                        size_t length,
                        size_t position);

  virtual void InsertZerosAt(size_t length, size_t position);

  virtual void OverwriteAt(const AudioVector& insert_this,
                           size_t length,
                           size_t position);

  virtual void OverwriteAt(const int16_t* insert_this,
                           size_t length,
                           size_t position);

  virtual void CrossFade(const AudioVector& append_this, size_t fade_length);

  virtual size_t Size() const;

  virtual bool Empty() const;

  inline const int16_t& operator[](size_t index) const {
    return array_[WrapIndex(index, begin_index_, capacity_)];
  }

  inline int16_t& operator[](size_t index) {
    return array_[WrapIndex(index, begin_index_, capacity_)];
  }

 private:
  static const size_t kDefaultInitialSize = 10;

  static inline size_t WrapIndex(size_t index,
                                 size_t begin_index,
                                 size_t capacity) {
    RTC_DCHECK_LT(index, capacity);
    RTC_DCHECK_LT(begin_index, capacity);
    size_t ix = begin_index + index;
    RTC_DCHECK_GE(ix, index);
    if (ix >= capacity) {
      ix -= capacity;
    }
    RTC_DCHECK_LT(ix, capacity);
    return ix;
  }

  void Reserve(size_t n);

  void InsertByPushBack(const int16_t* insert_this,
                        size_t length,
                        size_t position);

  void InsertByPushFront(const int16_t* insert_this,
                         size_t length,
                         size_t position);

  void InsertZerosByPushBack(size_t length, size_t position);

  void InsertZerosByPushFront(size_t length, size_t position);

  std::unique_ptr<int16_t[]> array_;

  size_t capacity_;

  size_t begin_index_;

  size_t end_index_;
};

}
#endif
