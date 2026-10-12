/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_DESKTOP_CAPTURE_DESKTOP_FRAME_H_
#define MODULES_DESKTOP_CAPTURE_DESKTOP_FRAME_H_

#include <stdint.h>

#include <memory>
#include <vector>

#include "modules/desktop_capture/desktop_geometry.h"
#include "modules/desktop_capture/desktop_region.h"
#include "modules/desktop_capture/shared_memory.h"
#include "rtc_base/system/rtc_export.h"

namespace webrtc {

const float kStandardDPI = 96.0f;

class RTC_EXPORT DesktopFrame {
 public:

  static const int kBytesPerPixel = 4;

  virtual ~DesktopFrame();

  DesktopFrame(const DesktopFrame&) = delete;
  DesktopFrame& operator=(const DesktopFrame&) = delete;

  DesktopRect rect() const;

  float scale_factor() const;

  const DesktopSize& size() const { return size_; }

  const DesktopVector& top_left() const { return top_left_; }
  void set_top_left(const DesktopVector& top_left) { top_left_ = top_left; }

  int stride() const { return stride_; }

  uint8_t* data() const { return data_; }

  SharedMemory* shared_memory() const { return shared_memory_; }

  const DesktopRegion& updated_region() const { return updated_region_; }
  DesktopRegion* mutable_updated_region() { return &updated_region_; }

  const DesktopVector& dpi() const { return dpi_; }
  void set_dpi(const DesktopVector& dpi) { dpi_ = dpi; }

  bool may_contain_cursor() const { return may_contain_cursor_; }
  void set_may_contain_cursor(bool may_contain_cursor) {
    may_contain_cursor_ = may_contain_cursor;
  }

  int64_t capture_time_ms() const { return capture_time_ms_; }
  void set_capture_time_ms(int64_t time_ms) { capture_time_ms_ = time_ms; }

  void CopyPixelsFrom(const uint8_t* src_buffer,
                      int src_stride,
                      const DesktopRect& dest_rect);
  void CopyPixelsFrom(const DesktopFrame& src_frame,
                      const DesktopVector& src_pos,
                      const DesktopRect& dest_rect);

  bool CopyIntersectingPixelsFrom(const DesktopFrame& src_frame,
                                  double horizontal_scale,
                                  double vertical_scale);

  uint8_t* GetFrameDataAtPos(const DesktopVector& pos) const;

  uint32_t capturer_id() const { return capturer_id_; }
  void set_capturer_id(uint32_t capturer_id) { capturer_id_ = capturer_id; }

  void CopyFrameInfoFrom(const DesktopFrame& other);

  void MoveFrameInfoFrom(DesktopFrame* other);

  const std::vector<uint8_t>& icc_profile() const { return icc_profile_; }
  void set_icc_profile(const std::vector<uint8_t>& icc_profile) {
    icc_profile_ = icc_profile;
  }

  void SetFrameDataToBlack();

  bool FrameDataIsBlack() const;

 protected:
  DesktopFrame(DesktopSize size,
               int stride,
               uint8_t* data,
               SharedMemory* shared_memory);

  uint8_t* const data_;
  SharedMemory* const shared_memory_;

 private:
  const DesktopSize size_;
  const int stride_;

  DesktopRegion updated_region_;
  DesktopVector top_left_;
  DesktopVector dpi_;
  bool may_contain_cursor_ = false;
  int64_t capture_time_ms_;
  uint32_t capturer_id_;
  std::vector<uint8_t> icc_profile_;
};

class RTC_EXPORT BasicDesktopFrame : public DesktopFrame {
 public:

  explicit BasicDesktopFrame(DesktopSize size);

  ~BasicDesktopFrame() override;

  BasicDesktopFrame(const BasicDesktopFrame&) = delete;
  BasicDesktopFrame& operator=(const BasicDesktopFrame&) = delete;

  static DesktopFrame* CopyOf(const DesktopFrame& frame);
};

class RTC_EXPORT SharedMemoryDesktopFrame : public DesktopFrame {
 public:

  static std::unique_ptr<DesktopFrame> Create(
      DesktopSize size,
      SharedMemoryFactory* shared_memory_factory);

  SharedMemoryDesktopFrame(DesktopSize size,
                           int stride,
                           SharedMemory* shared_memory);

  SharedMemoryDesktopFrame(DesktopSize size,
                           int stride,
                           std::unique_ptr<SharedMemory> shared_memory);

  ~SharedMemoryDesktopFrame() override;

  SharedMemoryDesktopFrame(const SharedMemoryDesktopFrame&) = delete;
  SharedMemoryDesktopFrame& operator=(const SharedMemoryDesktopFrame&) = delete;

 private:

  SharedMemoryDesktopFrame(DesktopRect rect,
                           int stride,
                           SharedMemory* shared_memory);
};

}

#endif
