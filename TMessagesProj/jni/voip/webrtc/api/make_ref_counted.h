/*
 *  Copyright 2022 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef API_MAKE_REF_COUNTED_H_
#define API_MAKE_REF_COUNTED_H_

#include <type_traits>
#include <utility>

#include "rtc_base/ref_counted_object.h"

namespace rtc {

namespace webrtc_make_ref_counted_internal {

template <typename T>
class HasAddRefAndRelease {
 private:
  template <typename C,
            decltype(std::declval<C>().AddRef())* = nullptr,
            decltype(std::declval<C>().Release())* = nullptr>
  static int Test(int);
  template <typename>
  static char Test(...);

 public:
  static constexpr bool value = std::is_same_v<decltype(Test<T>(0)), int>;
};
}

template <
    typename T,
    typename... Args,
    typename std::enable_if<std::is_convertible_v<T*, RefCountInterface*> &&
                                std::is_abstract_v<T>,
                            T>::type* = nullptr>
scoped_refptr<T> make_ref_counted(Args&&... args) {
  return scoped_refptr<T>(new RefCountedObject<T>(std::forward<Args>(args)...));
}

template <
    typename T,
    typename... Args,
    typename std::enable_if<
        !std::is_convertible_v<T*, RefCountInterface*> &&
            webrtc_make_ref_counted_internal::HasAddRefAndRelease<T>::value,
        T>::type* = nullptr>
scoped_refptr<T> make_ref_counted(Args&&... args) {
  return scoped_refptr<T>(new T(std::forward<Args>(args)...));
}

template <
    typename T,
    typename... Args,
    typename std::enable_if<
        !std::is_convertible_v<T*, RefCountInterface*> &&
            !webrtc_make_ref_counted_internal::HasAddRefAndRelease<T>::value,

        T>::type* = nullptr>
scoped_refptr<FinalRefCountedObject<T>> make_ref_counted(Args&&... args) {
  return scoped_refptr<FinalRefCountedObject<T>>(
      new FinalRefCountedObject<T>(std::forward<Args>(args)...));
}

}

#endif
