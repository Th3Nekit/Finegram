/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_DESKTOP_CAPTURE_LINUX_X11_SHARED_X_DISPLAY_H_
#define MODULES_DESKTOP_CAPTURE_LINUX_X11_SHARED_X_DISPLAY_H_

#include <map>
#include <vector>

#include "absl/strings/string_view.h"
#include "api/ref_counted_base.h"
#include "api/scoped_refptr.h"
#include "rtc_base/synchronization/mutex.h"
#include "rtc_base/system/rtc_export.h"
#include "rtc_base/thread_annotations.h"

typedef struct _XDisplay Display;
typedef union _XEvent XEvent;

namespace webrtc {

class RTC_EXPORT SharedXDisplay
    : public rtc::RefCountedNonVirtual<SharedXDisplay> {
 public:
  class XEventHandler {
   public:
    virtual ~XEventHandler() {}

    virtual bool HandleXEvent(const XEvent& event) = 0;
  };

  static rtc::scoped_refptr<SharedXDisplay> Create(
      absl::string_view display_name);

  static rtc::scoped_refptr<SharedXDisplay> CreateDefault();

  Display* display() { return display_; }

  void AddEventHandler(int type, XEventHandler* handler);

  void RemoveEventHandler(int type, XEventHandler* handler);

  void ProcessPendingXEvents();

  void IgnoreXServerGrabs();

  ~SharedXDisplay();

  SharedXDisplay(const SharedXDisplay&) = delete;
  SharedXDisplay& operator=(const SharedXDisplay&) = delete;

 protected:

  explicit SharedXDisplay(Display* display);

 private:
  typedef std::map<int, std::vector<XEventHandler*> > EventHandlersMap;

  Display* display_;

  Mutex mutex_;

  EventHandlersMap event_handlers_ RTC_GUARDED_BY(mutex_);
};

}

#endif
