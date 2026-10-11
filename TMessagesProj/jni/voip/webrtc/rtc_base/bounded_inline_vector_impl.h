/*
 *  Copyright 2020 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_BOUNDED_INLINE_VECTOR_IMPL_H_
#define RTC_BASE_BOUNDED_INLINE_VECTOR_IMPL_H_

#include <stdint.h>

#include <cstring>
#include <memory>
#include <type_traits>
#include <utility>

namespace webrtc {
namespace bounded_inline_vector_impl {

template <bool...>
struct BoolPack;

template <bool... Bs>
using AllTrue = std::is_same<BoolPack<Bs..., true>, BoolPack<true, Bs...>>;

template <typename To, typename... Froms>
using AllConvertible = AllTrue<std::is_convertible<Froms, To>::value...>;

template <typename T>
void InitializeElements(T* data) {}
template <typename T, typename U, typename... Us>
void InitializeElements(T* data, U&& element, Us&&... elements) {

  ::new (data) T(std::forward<U>(element));
  InitializeElements(data + 1, std::forward<Us>(elements)...);
}

template <typename T>
void DefaultInitializeElements(T* data, int size) {
  for (int i = 0; i < size; ++i) {

    ::new (&data[i]) T;
  }
}

template <typename T>
void CopyElements(const T* src_data, int src_size, T* dst_data, int* dst_size) {
  if               (std::is_trivially_copy_constructible<T>::value) {
    std::memcpy(dst_data, src_data, src_size * sizeof(T));
  } else {
    std::uninitialized_copy_n(src_data, src_size, dst_data);
  }
  *dst_size = src_size;
}

template <typename T>
void MoveElements(T* src_data, int src_size, T* dst_data, int* dst_size) {
  if               (std::is_trivially_move_constructible<T>::value) {
    std::memcpy(dst_data, src_data, src_size * sizeof(T));
  } else {

    for (int i = 0; i < src_size; ++i) {

      ::new (&dst_data[i]) T(std::move(src_data[i]));
    }
  }
  *dst_size = src_size;
}

template <typename T>
void DestroyElements(T* data, int size) {
  if               (!std::is_trivially_destructible<T>::value) {
    for (int i = 0; i < size; ++i) {
      data[i].~T();
    }
  }
}

static constexpr int kSmallSize = 64;

template <typename T,
          int fixed_capacity,
          bool is_trivial = std::is_trivial<T>::value,
          bool is_small = (sizeof(T) * fixed_capacity <= kSmallSize)>
struct Storage {
  static_assert(!std::is_trivial<T>::value, "");

  template <
      typename... Ts,
      typename std::enable_if_t<AllConvertible<T, Ts...>::value>* = nullptr>
  explicit Storage(Ts&&... elements) : size(sizeof...(Ts)) {
    InitializeElements(data, std::forward<Ts>(elements)...);
  }

  Storage(const Storage& other) {
    CopyElements(other.data, other.size, data, &size);
  }

  Storage(Storage&& other) {
    MoveElements(other.data, other.size, data, &size);
  }

  Storage& operator=(const Storage& other) {
    if (this != &other) {
      DestroyElements(data, size);
      CopyElements(other.data, other.size, data, &size);
    }
    return *this;
  }

  Storage& operator=(Storage&& other) {
    DestroyElements(data, size);
    size = 0;
    MoveElements(other.data, other.size, data, &size);
    return *this;
  }

  ~Storage() { DestroyElements(data, size); }

  int size;
  union {

    T data[fixed_capacity];
  };
};

template <typename T, int fixed_capacity>
struct Storage<T, fixed_capacity,                true,              true> {
  static_assert(std::is_trivial<T>::value, "");
  static_assert(sizeof(T) * fixed_capacity <= kSmallSize, "");

  template <
      typename... Ts,
      typename std::enable_if_t<AllConvertible<T, Ts...>::value>* = nullptr>
  explicit Storage(Ts&&... elements) : size(sizeof...(Ts)) {
    InitializeElements(data, std::forward<Ts>(elements)...);
  }

  Storage(const Storage&) = default;
  Storage& operator=(const Storage&) = default;
  ~Storage() = default;

  int size;
  T data[fixed_capacity];
};

template <typename T, int fixed_capacity>
struct Storage<T, fixed_capacity,                true,              false> {
  static_assert(std::is_trivial<T>::value, "");
  static_assert(sizeof(T) * fixed_capacity > kSmallSize, "");

  template <
      typename... Ts,
      typename std::enable_if_t<AllConvertible<T, Ts...>::value>* = nullptr>
  explicit Storage(Ts&&... elements) : size(sizeof...(Ts)) {
    InitializeElements(data, std::forward<Ts>(elements)...);
  }

  Storage(const Storage& other) : size(other.size) {
    std::memcpy(data, other.data, other.size * sizeof(T));
  }

  Storage& operator=(const Storage& other) {
    if (this != &other) {
      size = other.size;
      std::memcpy(data, other.data, other.size * sizeof(T));
    }
    return *this;
  }

  ~Storage() = default;

  int size;
  union {
    T data[fixed_capacity];
  };
};

}
}

#endif
