/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_DESKTOP_CAPTURE_DESKTOP_REGION_H_
#define MODULES_DESKTOP_CAPTURE_DESKTOP_REGION_H_

#include <stdint.h>

#include <map>
#include <vector>

#include "modules/desktop_capture/desktop_geometry.h"
#include "rtc_base/system/rtc_export.h"

namespace webrtc {

class RTC_EXPORT DesktopRegion {
 private:

  struct RowSpan {
    RowSpan(int32_t left, int32_t right);

    bool operator==(const RowSpan& that) const {
      return left == that.left && right == that.right;
    }

    int32_t left;
    int32_t right;
  };

  typedef std::vector<RowSpan> RowSpanSet;

  struct Row {
    Row(const Row&);
    Row(Row&&);
    Row(int32_t top, int32_t bottom);
    ~Row();

    int32_t top;
    int32_t bottom;

    RowSpanSet spans;
  };

  typedef std::map<int, Row*> Rows;

 public:

  class RTC_EXPORT Iterator {
   public:
    explicit Iterator(const DesktopRegion& target);
    ~Iterator();

    bool IsAtEnd() const;
    void Advance();

    const DesktopRect& rect() const { return rect_; }

   private:
    const DesktopRegion& region_;

    void UpdateCurrentRect();

    Rows::const_iterator row_;
    Rows::const_iterator previous_row_;
    RowSpanSet::const_iterator row_span_;
    DesktopRect rect_;
  };

  DesktopRegion();
  explicit DesktopRegion(const DesktopRect& rect);
  DesktopRegion(const DesktopRect* rects, int count);
  DesktopRegion(const DesktopRegion& other);
  ~DesktopRegion();

  DesktopRegion& operator=(const DesktopRegion& other);

  bool is_empty() const { return rows_.empty(); }

  bool Equals(const DesktopRegion& region) const;

  void Clear();

  void SetRect(const DesktopRect& rect);

  void AddRect(const DesktopRect& rect);
  void AddRects(const DesktopRect* rects, int count);
  void AddRegion(const DesktopRegion& region);

  void Intersect(const DesktopRegion& region1, const DesktopRegion& region2);

  void IntersectWith(const DesktopRegion& region);

  void IntersectWith(const DesktopRect& rect);

  void Subtract(const DesktopRegion& region);

  void Subtract(const DesktopRect& rect);

  void Translate(int32_t dx, int32_t dy);

  void Swap(DesktopRegion* region);

 private:

  static bool CompareSpanLeft(const RowSpan& r, int32_t value);
  static bool CompareSpanRight(const RowSpan& r, int32_t value);

  static void AddSpanToRow(Row* row, int32_t left, int32_t right);

  static bool IsSpanInRow(const Row& row, const RowSpan& rect);

  static void IntersectRows(const RowSpanSet& set1,
                            const RowSpanSet& set2,
                            RowSpanSet* output);

  static void SubtractRows(const RowSpanSet& set_a,
                           const RowSpanSet& set_b,
                           RowSpanSet* output);

  void MergeWithPrecedingRow(Rows::iterator row);

  Rows rows_;
};

}

#endif
