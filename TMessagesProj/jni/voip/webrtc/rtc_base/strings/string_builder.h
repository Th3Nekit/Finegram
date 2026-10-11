/*
 *  Copyright 2018 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_STRINGS_STRING_BUILDER_H_
#define RTC_BASE_STRINGS_STRING_BUILDER_H_

#include <cstdio>
#include <string>
#include <utility>

#include "absl/strings/string_view.h"
#include "api/array_view.h"
#include "rtc_base/string_encode.h"

namespace rtc {

class SimpleStringBuilder {
 public:
  explicit SimpleStringBuilder(rtc::ArrayView<char> buffer);
  SimpleStringBuilder(const SimpleStringBuilder&) = delete;
  SimpleStringBuilder& operator=(const SimpleStringBuilder&) = delete;

  SimpleStringBuilder& operator<<(char ch);
  SimpleStringBuilder& operator<<(absl::string_view str);
  SimpleStringBuilder& operator<<(int i);
  SimpleStringBuilder& operator<<(unsigned i);
  SimpleStringBuilder& operator<<(long i);
  SimpleStringBuilder& operator<<(long long i);
  SimpleStringBuilder& operator<<(unsigned long i);
  SimpleStringBuilder& operator<<(unsigned long long i);
  SimpleStringBuilder& operator<<(float f);
  SimpleStringBuilder& operator<<(double f);
  SimpleStringBuilder& operator<<(long double f);

  const char* str() const { return buffer_.data(); }

  size_t size() const { return size_; }

#if defined(__GNUC__)
  __attribute__((__format__(__printf__, 2, 3)))
#endif
  SimpleStringBuilder&
  AppendFormat(const char* fmt, ...);

 private:
  bool IsConsistent() const {
    return size_ <= buffer_.size() - 1 && buffer_[size_] == '\0';
  }

  const rtc::ArrayView<char> buffer_;

  size_t size_ = 0;
};

class StringBuilder {
 public:
  StringBuilder() {}
  explicit StringBuilder(absl::string_view s) : str_(s) {}

  StringBuilder(const StringBuilder&) = delete;
  StringBuilder& operator=(const StringBuilder&) = delete;

  StringBuilder& operator<<(const absl::string_view str) {
    str_.append(str.data(), str.length());
    return *this;
  }

  StringBuilder& operator<<(char c) = delete;

  StringBuilder& operator<<(int i) {
    str_ += rtc::ToString(i);
    return *this;
  }

  StringBuilder& operator<<(unsigned i) {
    str_ += rtc::ToString(i);
    return *this;
  }

  StringBuilder& operator<<(long i) {
    str_ += rtc::ToString(i);
    return *this;
  }

  StringBuilder& operator<<(long long i) {
    str_ += rtc::ToString(i);
    return *this;
  }

  StringBuilder& operator<<(unsigned long i) {
    str_ += rtc::ToString(i);
    return *this;
  }

  StringBuilder& operator<<(unsigned long long i) {
    str_ += rtc::ToString(i);
    return *this;
  }

  StringBuilder& operator<<(float f) {
    str_ += rtc::ToString(f);
    return *this;
  }

  StringBuilder& operator<<(double f) {
    str_ += rtc::ToString(f);
    return *this;
  }

  StringBuilder& operator<<(long double f) {
    str_ += rtc::ToString(f);
    return *this;
  }

  const std::string& str() const { return str_; }

  void Clear() { str_.clear(); }

  size_t size() const { return str_.size(); }

  std::string Release() {
    std::string ret = std::move(str_);
    str_.clear();
    return ret;
  }

  StringBuilder& AppendFormat(const char* fmt, ...)
#if defined(__GNUC__)
      __attribute__((__format__(__printf__, 2, 3)))
#endif
      ;

 private:
  std::string str_;
};

}

#endif
