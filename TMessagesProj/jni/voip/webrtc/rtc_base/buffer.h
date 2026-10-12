/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_BUFFER_H_
#define RTC_BASE_BUFFER_H_

#include <stdint.h>

#include <algorithm>
#include <cstring>
#include <memory>
#include <type_traits>
#include <utility>

#include "absl/strings/string_view.h"
#include "api/array_view.h"
#include "rtc_base/checks.h"
#include "rtc_base/type_traits.h"
#include "rtc_base/zero_memory.h"

namespace rtc {

namespace internal {

template <typename T, typename U>
struct BufferCompat {
  static constexpr bool value =
      !std::is_volatile<U>::value &&
      ((std::is_integral<T>::value && sizeof(T) == 1)
           ? (std::is_integral<U>::value && sizeof(U) == 1)
           : (std::is_same<T, typename std::remove_const<U>::type>::value));
};

}

template <typename T, bool ZeroOnFree = false>
class BufferT {

  static_assert(std::is_trivial<T>::value, "T must be a trivial type.");

  static_assert(!std::is_const<T>::value, "T may not be const");

 public:
  using value_type = T;
  using const_iterator = const T*;

  BufferT() : size_(0), capacity_(0), data_(nullptr) {
    RTC_DCHECK(IsConsistent());
  }

  BufferT(const BufferT&) = delete;
  BufferT& operator=(const BufferT&) = delete;

  BufferT(BufferT&& buf)
      : size_(buf.size()),
        capacity_(buf.capacity()),
        data_(std::move(buf.data_)) {
    RTC_DCHECK(IsConsistent());
    buf.OnMovedFrom();
  }

  explicit BufferT(size_t size) : BufferT(size, size) {}

  BufferT(size_t size, size_t capacity)
      : size_(size),
        capacity_(std::max(size, capacity)),
        data_(capacity_ > 0 ? new T[capacity_] : nullptr) {
    RTC_DCHECK(IsConsistent());
  }

  template <typename U,
            typename std::enable_if<
                internal::BufferCompat<T, U>::value>::type* = nullptr>
  BufferT(const U* data, size_t size) : BufferT(data, size, size) {}

  template <typename U,
            typename std::enable_if<
                internal::BufferCompat<T, U>::value>::type* = nullptr>
  BufferT(U* data, size_t size, size_t capacity) : BufferT(size, capacity) {
    static_assert(sizeof(T) == sizeof(U), "");
    if (size > 0) {
      RTC_DCHECK(data);
      std::memcpy(data_.get(), data, size * sizeof(U));
    }
  }

  template <typename U,
            size_t N,
            typename std::enable_if<
                internal::BufferCompat<T, U>::value>::type* = nullptr>
  BufferT(U (&array)[N]) : BufferT(array, N) {}

  ~BufferT() { MaybeZeroCompleteBuffer(); }

  template <typename U = T>
  operator typename std::enable_if<internal::BufferCompat<U, char>::value,
                                   absl::string_view>::type() const {
    return absl::string_view(data<char>(), size());
  }

  template <typename U = T,
            typename std::enable_if<
                internal::BufferCompat<T, U>::value>::type* = nullptr>
  const U* data() const {
    RTC_DCHECK(IsConsistent());
    return reinterpret_cast<U*>(data_.get());
  }

  template <typename U = T,
            typename std::enable_if<
                internal::BufferCompat<T, U>::value>::type* = nullptr>
  U* data() {
    RTC_DCHECK(IsConsistent());
    return reinterpret_cast<U*>(data_.get());
  }

  bool empty() const {
    RTC_DCHECK(IsConsistent());
    return size_ == 0;
  }

  size_t size() const {
    RTC_DCHECK(IsConsistent());
    return size_;
  }

  size_t capacity() const {
    RTC_DCHECK(IsConsistent());
    return capacity_;
  }

  BufferT& operator=(BufferT&& buf) {
    RTC_DCHECK(buf.IsConsistent());
    MaybeZeroCompleteBuffer();
    size_ = buf.size_;
    capacity_ = buf.capacity_;
    using std::swap;
    swap(data_, buf.data_);
    buf.data_.reset();
    buf.OnMovedFrom();
    return *this;
  }

  bool operator==(const BufferT& buf) const {
    RTC_DCHECK(IsConsistent());
    if (size_ != buf.size_) {
      return false;
    }
    if (std::is_integral<T>::value) {

      return std::memcmp(data_.get(), buf.data_.get(), size_ * sizeof(T)) == 0;
    }
    for (size_t i = 0; i < size_; ++i) {
      if (data_[i] != buf.data_[i]) {
        return false;
      }
    }
    return true;
  }

  bool operator!=(const BufferT& buf) const { return !(*this == buf); }

  T& operator[](size_t index) {
    RTC_DCHECK_LT(index, size_);
    return data()[index];
  }

  T operator[](size_t index) const {
    RTC_DCHECK_LT(index, size_);
    return data()[index];
  }

  T* begin() { return data(); }
  T* end() { return data() + size(); }
  const T* begin() const { return data(); }
  const T* end() const { return data() + size(); }
  const T* cbegin() const { return data(); }
  const T* cend() const { return data() + size(); }

  template <typename U,
            typename std::enable_if<
                internal::BufferCompat<T, U>::value>::type* = nullptr>
  void SetData(const U* data, size_t size) {
    RTC_DCHECK(IsConsistent());
    const size_t old_size = size_;
    size_ = 0;
    AppendData(data, size);
    if (ZeroOnFree && size_ < old_size) {
      ZeroTrailingData(old_size - size_);
    }
  }

  template <typename U,
            size_t N,
            typename std::enable_if<
                internal::BufferCompat<T, U>::value>::type* = nullptr>
  void SetData(const U (&array)[N]) {
    SetData(array, N);
  }

  template <typename W,
            typename std::enable_if<
                HasDataAndSize<const W, const T>::value>::type* = nullptr>
  void SetData(const W& w) {
    SetData(w.data(), w.size());
  }

  template <typename U = T,
            typename F,
            typename std::enable_if<
                internal::BufferCompat<T, U>::value>::type* = nullptr>
  size_t SetData(size_t max_elements, F&& setter) {
    RTC_DCHECK(IsConsistent());
    const size_t old_size = size_;
    size_ = 0;
    const size_t written = AppendData<U>(max_elements, std::forward<F>(setter));
    if (ZeroOnFree && size_ < old_size) {
      ZeroTrailingData(old_size - size_);
    }
    return written;
  }

  template <typename U,
            typename std::enable_if<
                internal::BufferCompat<T, U>::value>::type* = nullptr>
  void AppendData(const U* data, size_t size) {
    if (size == 0) {
      return;
    }
    RTC_DCHECK(data);
    RTC_DCHECK(IsConsistent());
    const size_t new_size = size_ + size;
    EnsureCapacityWithHeadroom(new_size, true);
    static_assert(sizeof(T) == sizeof(U), "");
    std::memcpy(data_.get() + size_, data, size * sizeof(U));
    size_ = new_size;
    RTC_DCHECK(IsConsistent());
  }

  template <typename U,
            size_t N,
            typename std::enable_if<
                internal::BufferCompat<T, U>::value>::type* = nullptr>
  void AppendData(const U (&array)[N]) {
    AppendData(array, N);
  }

  template <typename W,
            typename std::enable_if<
                HasDataAndSize<const W, const T>::value>::type* = nullptr>
  void AppendData(const W& w) {
    AppendData(w.data(), w.size());
  }

  template <typename U,
            typename std::enable_if<
                internal::BufferCompat<T, U>::value>::type* = nullptr>
  void AppendData(const U& item) {
    AppendData(&item, 1);
  }

  template <typename U = T,
            typename F,
            typename std::enable_if<
                internal::BufferCompat<T, U>::value>::type* = nullptr>
  size_t AppendData(size_t max_elements, F&& setter) {
    RTC_DCHECK(IsConsistent());
    const size_t old_size = size_;
    SetSize(old_size + max_elements);
    U* base_ptr = data<U>() + old_size;
    size_t written_elements = setter(rtc::ArrayView<U>(base_ptr, max_elements));

    RTC_CHECK_LE(written_elements, max_elements);
    size_ = old_size + written_elements;
    RTC_DCHECK(IsConsistent());
    return written_elements;
  }

  void SetSize(size_t size) {
    const size_t old_size = size_;
    EnsureCapacityWithHeadroom(size, true);
    size_ = size;
    if (ZeroOnFree && size_ < old_size) {
      ZeroTrailingData(old_size - size_);
    }
  }

  void EnsureCapacity(size_t capacity) {

    EnsureCapacityWithHeadroom(capacity, false);
  }

  void Clear() {
    MaybeZeroCompleteBuffer();
    size_ = 0;
    RTC_DCHECK(IsConsistent());
  }

  friend void swap(BufferT& a, BufferT& b) {
    using std::swap;
    swap(a.size_, b.size_);
    swap(a.capacity_, b.capacity_);
    swap(a.data_, b.data_);
  }

 private:
  void EnsureCapacityWithHeadroom(size_t capacity, bool extra_headroom) {
    RTC_DCHECK(IsConsistent());
    if (capacity <= capacity_)
      return;

    const size_t new_capacity =
        extra_headroom ? std::max(capacity, capacity_ + capacity_ / 2)
                       : capacity;

    std::unique_ptr<T[]> new_data(new T[new_capacity]);
    if (data_ != nullptr) {
      std::memcpy(new_data.get(), data_.get(), size_ * sizeof(T));
    }
    MaybeZeroCompleteBuffer();
    data_ = std::move(new_data);
    capacity_ = new_capacity;
    RTC_DCHECK(IsConsistent());
  }

  void MaybeZeroCompleteBuffer() {
    if (ZeroOnFree && capacity_ > 0) {

      ExplicitZeroMemory(data_.get(), capacity_ * sizeof(T));
    }
  }

  void ZeroTrailingData(size_t count) {
    RTC_DCHECK(IsConsistent());
    RTC_DCHECK_LE(count, capacity_ - size_);
    ExplicitZeroMemory(data_.get() + size_, count * sizeof(T));
  }

  bool IsConsistent() const {
    return (data_ || capacity_ == 0) && capacity_ >= size_;
  }

  void OnMovedFrom() {
    RTC_DCHECK(!data_);
#if RTC_DCHECK_IS_ON

    size_ = 1;
    capacity_ = 0;
#else

    size_ = 0;
    capacity_ = 0;
#endif
  }

  size_t size_;
  size_t capacity_;
  std::unique_ptr<T[]> data_;
};

using Buffer = BufferT<uint8_t>;

template <typename T>
using ZeroOnFreeBuffer = BufferT<T, true>;

}

#endif
