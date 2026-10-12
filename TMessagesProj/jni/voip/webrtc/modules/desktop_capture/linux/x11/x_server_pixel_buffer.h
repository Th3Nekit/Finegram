/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// Don't include this file in any .h files because it pulls in some X headers.

#ifndef MODULES_DESKTOP_CAPTURE_LINUX_X11_X_SERVER_PIXEL_BUFFER_H_
#define MODULES_DESKTOP_CAPTURE_LINUX_X11_X_SERVER_PIXEL_BUFFER_H_

#include <X11/Xutil.h>
#include <X11/extensions/XShm.h>

#include <memory>
#include <vector>

#include "modules/desktop_capture/desktop_geometry.h"

namespace webrtc {

class DesktopFrame;
class XAtomCache;

class XServerPixelBuffer {
 public:
  XServerPixelBuffer();
  ~XServerPixelBuffer();

  XServerPixelBuffer(const XServerPixelBuffer&) = delete;
  XServerPixelBuffer& operator=(const XServerPixelBuffer&) = delete;

  void Release();

  bool Init(XAtomCache* cache, Window window);

  bool is_initialized() { return window_ != 0; }

  DesktopSize window_size() { return window_rect_.size(); }

  const DesktopRect& window_rect() { return window_rect_; }

  bool IsWindowValid() const;

  void Synchronize();

  bool CaptureRect(const DesktopRect& rect, DesktopFrame* frame);

 private:
  void ReleaseSharedMemorySegment();

  void InitShm(const XWindowAttributes& attributes);
  bool InitPixmaps(int depth);

  Display* display_ = nullptr;
  Window window_ = 0;
  DesktopRect window_rect_;
  XImage* x_image_ = nullptr;
  XShmSegmentInfo* shm_segment_info_ = nullptr;
  XImage* x_shm_image_ = nullptr;
  Pixmap shm_pixmap_ = 0;
  GC shm_gc_ = nullptr;
  bool xshm_get_image_succeeded_ = false;
  std::vector<uint8_t> icc_profile_;
};

}

#endif
