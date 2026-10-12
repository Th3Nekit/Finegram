/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_DESKTOP_CAPTURE_DESKTOP_FRAME_GENERATOR_H_
#define MODULES_DESKTOP_CAPTURE_DESKTOP_FRAME_GENERATOR_H_

#include <memory>

#include "modules/desktop_capture/desktop_frame.h"
#include "modules/desktop_capture/desktop_geometry.h"
#include "modules/desktop_capture/desktop_region.h"
#include "modules/desktop_capture/shared_memory.h"

namespace webrtc {

class DesktopFrameGenerator {
 public:
  DesktopFrameGenerator();
  virtual ~DesktopFrameGenerator();

  virtual std::unique_ptr<DesktopFrame> GetNextFrame(
      SharedMemoryFactory* factory) = 0;
};

class DesktopFramePainter {
 public:
  DesktopFramePainter();
  virtual ~DesktopFramePainter();

  virtual bool Paint(DesktopFrame* frame, DesktopRegion* updated_region) = 0;
};

class PainterDesktopFrameGenerator final : public DesktopFrameGenerator {
 public:
  PainterDesktopFrameGenerator();
  ~PainterDesktopFrameGenerator() override;

  std::unique_ptr<DesktopFrame> GetNextFrame(
      SharedMemoryFactory* factory) override;

  DesktopSize* size();

  void set_return_frame(bool return_frame);

  void set_provide_updated_region_hints(bool provide_updated_region_hints);

  void set_enlarge_updated_region(bool enlarge_updated_region);

  void set_enlarge_range(int enlarge_range);

  void set_add_random_updated_region(bool add_random_updated_region);

  void set_desktop_frame_painter(DesktopFramePainter* painter);

 private:
  DesktopSize size_;
  bool return_frame_;
  bool provide_updated_region_hints_;
  bool enlarge_updated_region_;
  int enlarge_range_;
  bool add_random_updated_region_;
  DesktopFramePainter* painter_;
};

class BlackWhiteDesktopFramePainter final : public DesktopFramePainter {
 public:
  BlackWhiteDesktopFramePainter();
  ~BlackWhiteDesktopFramePainter() override;

  DesktopRegion* updated_region();

  bool Paint(DesktopFrame* frame, DesktopRegion* updated_region) override;

 private:
  DesktopRegion updated_region_;
};

}

#endif
