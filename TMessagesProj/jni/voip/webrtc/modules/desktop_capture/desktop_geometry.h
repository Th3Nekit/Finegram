/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_DESKTOP_CAPTURE_DESKTOP_GEOMETRY_H_
#define MODULES_DESKTOP_CAPTURE_DESKTOP_GEOMETRY_H_

#include <stdint.h>

#include "rtc_base/system/rtc_export.h"

namespace webrtc {

class DesktopVector {
 public:
  DesktopVector() : x_(0), y_(0) {}
  DesktopVector(int32_t x, int32_t y) : x_(x), y_(y) {}

  int32_t x() const { return x_; }
  int32_t y() const { return y_; }
  bool is_zero() const { return x_ == 0 && y_ == 0; }

  bool equals(const DesktopVector& other) const {
    return x_ == other.x_ && y_ == other.y_;
  }

  void set(int32_t x, int32_t y) {
    x_ = x;
    y_ = y;
  }

  DesktopVector add(const DesktopVector& other) const {
    return DesktopVector(x() + other.x(), y() + other.y());
  }
  DesktopVector subtract(const DesktopVector& other) const {
    return DesktopVector(x() - other.x(), y() - other.y());
  }

  DesktopVector operator-() const { return DesktopVector(-x_, -y_); }

 private:
  int32_t x_;
  int32_t y_;
};

class DesktopSize {
 public:
  DesktopSize() : width_(0), height_(0) {}
  DesktopSize(int32_t width, int32_t height) : width_(width), height_(height) {}

  int32_t width() const { return width_; }
  int32_t height() const { return height_; }

  bool is_empty() const { return width_ <= 0 || height_ <= 0; }

  bool equals(const DesktopSize& other) const {
    return width_ == other.width_ && height_ == other.height_;
  }

  void set(int32_t width, int32_t height) {
    width_ = width;
    height_ = height;
  }

 private:
  int32_t width_;
  int32_t height_;
};

class RTC_EXPORT DesktopRect {
 public:
  static DesktopRect MakeSize(const DesktopSize& size) {
    return DesktopRect(0, 0, size.width(), size.height());
  }
  static DesktopRect MakeWH(int32_t width, int32_t height) {
    return DesktopRect(0, 0, width, height);
  }
  static DesktopRect MakeXYWH(int32_t x,
                              int32_t y,
                              int32_t width,
                              int32_t height) {
    return DesktopRect(x, y, x + width, y + height);
  }
  static DesktopRect MakeLTRB(int32_t left,
                              int32_t top,
                              int32_t right,
                              int32_t bottom) {
    return DesktopRect(left, top, right, bottom);
  }
  static DesktopRect MakeOriginSize(const DesktopVector& origin,
                                    const DesktopSize& size) {
    return MakeXYWH(origin.x(), origin.y(), size.width(), size.height());
  }

  DesktopRect() : left_(0), top_(0), right_(0), bottom_(0) {}

  int32_t left() const { return left_; }
  int32_t top() const { return top_; }
  int32_t right() const { return right_; }
  int32_t bottom() const { return bottom_; }
  int32_t width() const { return right_ - left_; }
  int32_t height() const { return bottom_ - top_; }

  void set_width(int32_t width) { right_ = left_ + width; }
  void set_height(int32_t height) { bottom_ = top_ + height; }

  DesktopVector top_left() const { return DesktopVector(left_, top_); }
  DesktopSize size() const { return DesktopSize(width(), height()); }

  bool is_empty() const { return left_ >= right_ || top_ >= bottom_; }

  bool equals(const DesktopRect& other) const {
    return left_ == other.left_ && top_ == other.top_ &&
           right_ == other.right_ && bottom_ == other.bottom_;
  }

  bool Contains(const DesktopVector& point) const;

  bool ContainsRect(const DesktopRect& rect) const;

  void IntersectWith(const DesktopRect& rect);

  void UnionWith(const DesktopRect& rect);

  void Translate(int32_t dx, int32_t dy);
  void Translate(DesktopVector d) { Translate(d.x(), d.y()); }

  void Extend(int32_t left_offset,
              int32_t top_offset,
              int32_t right_offset,
              int32_t bottom_offset);

  void Scale(double horizontal, double vertical);

 private:
  DesktopRect(int32_t left, int32_t top, int32_t right, int32_t bottom)
      : left_(left), top_(top), right_(right), bottom_(bottom) {}

  int32_t left_;
  int32_t top_;
  int32_t right_;
  int32_t bottom_;
};

}

#endif
