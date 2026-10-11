/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_DESKTOP_CAPTURE_LINUX_X11_X_WINDOW_PROPERTY_H_
#define MODULES_DESKTOP_CAPTURE_LINUX_X11_X_WINDOW_PROPERTY_H_

#include <X11/X.h>
#include <X11/Xlib.h>

namespace webrtc {

class XWindowPropertyBase {
 public:
  XWindowPropertyBase(Display* display,
                      Window window,
                      Atom property,
                      int expected_size);
  virtual ~XWindowPropertyBase();

  XWindowPropertyBase(const XWindowPropertyBase&) = delete;
  XWindowPropertyBase& operator=(const XWindowPropertyBase&) = delete;

  bool is_valid() const { return is_valid_; }

  size_t size() const { return size_; }

 protected:
  unsigned char* data_ = nullptr;

 private:
  bool is_valid_ = false;
  unsigned long size_ = 0;
};

template <class PropertyType>
class XWindowProperty : public XWindowPropertyBase {
 public:
  XWindowProperty(Display* display, const Window window, const Atom property)
      : XWindowPropertyBase(display, window, property, sizeof(PropertyType)) {}
  ~XWindowProperty() override = default;

  XWindowProperty(const XWindowProperty&) = delete;
  XWindowProperty& operator=(const XWindowProperty&) = delete;

  const PropertyType* data() const {
    return reinterpret_cast<PropertyType*>(data_);
  }
  PropertyType* data() { return reinterpret_cast<PropertyType*>(data_); }
};

}

#endif
