/*
 *  Copyright 2015 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_ARRAY_VIEW_H_
#define API_ARRAY_VIEW_H_

#include <algorithm>
#include <array>
#include <iterator>
#include <type_traits>

#include "rtc_base/checks.h"
#include "rtc_base/type_traits.h"

namespace rtc {

namespace array_view_internal {

enum : std::ptrdiff_t { kArrayViewVarSize = -4711 };

template <typename T, std::ptrdiff_t Size>
class ArrayViewBase {
  static_assert(Size > 0, "ArrayView size must be variable or non-negative");

 public:
  ArrayViewBase(T* data, size_t size) : data_(data) {}

  static constexpr size_t size() { return Size; }
  static constexpr bool empty() { return false; }
  T* data() const { return data_; }

 protected:
  static constexpr bool fixed_size() { return true; }

 private:
  T* data_;
};

template <typename T>
class ArrayViewBase<T, 0> {
 public:
  explicit ArrayViewBase(T* data, size_t size) {}

  static constexpr size_t size() { return 0; }
  static constexpr bool empty() { return true; }
  T* data() const { return nullptr; }

 protected:
  static constexpr bool fixed_size() { return true; }
};

template <typename T>
class ArrayViewBase<T, array_view_internal::kArrayViewVarSize> {
 public:
  ArrayViewBase(T* data, size_t size)
      : data_(size == 0 ? nullptr : data), size_(size) {}

  size_t size() const { return size_; }
  bool empty() const { return size_ == 0; }
  T* data() const { return data_; }

 protected:
  static constexpr bool fixed_size() { return false; }

 private:
  T* data_;
  size_t size_;
};

}

template <typename T,
          std::ptrdiff_t Size = array_view_internal::kArrayViewVarSize>
class ArrayView final : public array_view_internal::ArrayViewBase<T, Size> {
 public:
  using value_type = T;
  using reference = value_type&;
  using const_reference = const value_type&;
  using pointer = value_type*;
  using const_pointer = const value_type*;
  using const_iterator = const T*;

  template <typename U>
  ArrayView(U* data, size_t size)
      : array_view_internal::ArrayViewBase<T, Size>::ArrayViewBase(data, size) {
    RTC_DCHECK_EQ(size == 0 ? nullptr : data, this->data());
    RTC_DCHECK_EQ(size, this->size());
    RTC_DCHECK_EQ(!this->data(),
                  this->size() == 0);
  }

  ArrayView() : ArrayView(nullptr, 0) {}
  ArrayView(std::nullptr_t)
      : ArrayView() {}
  ArrayView(std::nullptr_t, size_t size)
      : ArrayView(static_cast<T*>(nullptr), size) {
    static_assert(Size == 0 || Size == array_view_internal::kArrayViewVarSize,
                  "");
    RTC_DCHECK_EQ(0, size);
  }

  template <typename U, size_t N>
  ArrayView(U (&array)[N])
      : ArrayView(array, N) {
    static_assert(Size == N || Size == array_view_internal::kArrayViewVarSize,
                  "Array size must match ArrayView size");
  }

  template <typename U,
            size_t N,
            typename std::enable_if<
                Size == static_cast<std::ptrdiff_t>(N)>::type* = nullptr>
  ArrayView(std::array<U, N>& u)
      : ArrayView(u.data(), u.size()) {}

  template <typename U,
            size_t N,
            typename std::enable_if<
                Size == static_cast<std::ptrdiff_t>(N)>::type* = nullptr>
  ArrayView(const std::array<U, N>& u)
      : ArrayView(u.data(), u.size()) {}

  template <
      typename U,
      typename std::enable_if<Size != array_view_internal::kArrayViewVarSize &&
                              HasDataAndSize<U, T>::value>::type* = nullptr>
  ArrayView(U& u)
      : ArrayView(u.data(), u.size()) {
    static_assert(U::size() == Size, "Sizes must match exactly");
  }
  template <
      typename U,
      typename std::enable_if<Size != array_view_internal::kArrayViewVarSize &&
                              HasDataAndSize<U, T>::value>::type* = nullptr>
  ArrayView(const U& u)
      : ArrayView(u.data(), u.size()) {
    static_assert(U::size() == Size, "Sizes must match exactly");
  }

  template <
      typename U,
      typename std::enable_if<Size == array_view_internal::kArrayViewVarSize &&
                              HasDataAndSize<U, T>::value>::type* = nullptr>
  ArrayView(U& u)
      : ArrayView(u.data(), u.size()) {}
  template <
      typename U,
      typename std::enable_if<Size == array_view_internal::kArrayViewVarSize &&
                              HasDataAndSize<U, T>::value>::type* = nullptr>
  ArrayView(const U& u)
      : ArrayView(u.data(), u.size()) {}

  T& operator[](size_t idx) const {
    RTC_DCHECK_LT(idx, this->size());
    RTC_DCHECK(this->data());
    return this->data()[idx];
  }
  T* begin() const { return this->data(); }
  T* end() const { return this->data() + this->size(); }
  const T* cbegin() const { return this->data(); }
  const T* cend() const { return this->data() + this->size(); }
  std::reverse_iterator<T*> rbegin() const {
    return std::make_reverse_iterator(end());
  }
  std::reverse_iterator<T*> rend() const {
    return std::make_reverse_iterator(begin());
  }
  std::reverse_iterator<const T*> crbegin() const {
    return std::make_reverse_iterator(cend());
  }
  std::reverse_iterator<const T*> crend() const {
    return std::make_reverse_iterator(cbegin());
  }

  ArrayView<T> subview(size_t offset, size_t size) const {
    return offset < this->size()
               ? ArrayView<T>(this->data() + offset,
                              std::min(size, this->size() - offset))
               : ArrayView<T>();
  }
  ArrayView<T> subview(size_t offset) const {
    return subview(offset, this->size());
  }
};

template <typename T, std::ptrdiff_t Size1, std::ptrdiff_t Size2>
bool operator==(const ArrayView<T, Size1>& a, const ArrayView<T, Size2>& b) {
  return a.data() == b.data() && a.size() == b.size();
}
template <typename T, std::ptrdiff_t Size1, std::ptrdiff_t Size2>
bool operator!=(const ArrayView<T, Size1>& a, const ArrayView<T, Size2>& b) {
  return !(a == b);
}

static_assert(sizeof(ArrayView<int>) == 2 * sizeof(int*), "");
static_assert(sizeof(ArrayView<int, 17>) == sizeof(int*), "");
static_assert(std::is_empty<ArrayView<int, 0>>::value, "");

template <typename T>
inline ArrayView<T> MakeArrayView(T* data, size_t size) {
  return ArrayView<T>(data, size);
}

template <typename U, typename T, std::ptrdiff_t Size>
inline ArrayView<U, Size> reinterpret_array_view(ArrayView<T, Size> view) {
  static_assert(sizeof(U) == sizeof(T) && alignof(U) == alignof(T),
                "ArrayView reinterpret_cast is only supported for casting "
                "between views that represent the same chunk of memory.");
  static_assert(
      std::is_fundamental<T>::value && std::is_fundamental<U>::value,
      "ArrayView reinterpret_cast is only supported for casting between "
      "fundamental types.");
  return ArrayView<U, Size>(reinterpret_cast<U*>(view.data()), view.size());
}

}

#endif
