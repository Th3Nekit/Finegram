/*
 *  Copyright 2017 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef API_REF_COUNTED_BASE_H_
#define API_REF_COUNTED_BASE_H_

#include <type_traits>

#include "rtc_base/ref_counter.h"

namespace rtc {

class RefCountedBase {
 public:
  RefCountedBase() = default;

  RefCountedBase(const RefCountedBase&) = delete;
  RefCountedBase& operator=(const RefCountedBase&) = delete;

  void AddRef() const { ref_count_.IncRef(); }
  RefCountReleaseStatus Release() const {
    const auto status = ref_count_.DecRef();
    if (status == RefCountReleaseStatus::kDroppedLastRef) {
      delete this;
    }
    return status;
  }

 protected:

  bool HasOneRef() const { return ref_count_.HasOneRef(); }

  virtual ~RefCountedBase() = default;

 private:
  mutable webrtc::webrtc_impl::RefCounter ref_count_{0};
};

template <typename T>
class RefCountedNonVirtual {
 public:
  RefCountedNonVirtual() = default;

  RefCountedNonVirtual(const RefCountedNonVirtual&) = delete;
  RefCountedNonVirtual& operator=(const RefCountedNonVirtual&) = delete;

  void AddRef() const { ref_count_.IncRef(); }
  RefCountReleaseStatus Release() const {

    static_assert(!std::is_polymorphic<T>::value,
                  "T has virtual methods. RefCountedBase is a better fit.");
    const auto status = ref_count_.DecRef();
    if (status == RefCountReleaseStatus::kDroppedLastRef) {
      delete static_cast<const T*>(this);
    }
    return status;
  }

 protected:

  bool HasOneRef() const { return ref_count_.HasOneRef(); }

  ~RefCountedNonVirtual() = default;

 private:
  mutable webrtc::webrtc_impl::RefCounter ref_count_{0};
};

}

#endif
