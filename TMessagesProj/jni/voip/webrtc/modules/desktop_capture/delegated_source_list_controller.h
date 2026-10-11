/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_DESKTOP_CAPTURE_DELEGATED_SOURCE_LIST_CONTROLLER_H_
#define MODULES_DESKTOP_CAPTURE_DELEGATED_SOURCE_LIST_CONTROLLER_H_

#include "rtc_base/system/rtc_export.h"

namespace webrtc {

class RTC_EXPORT DelegatedSourceListController {
 public:

  class Observer {
   public:

    virtual void OnSelection() = 0;

    virtual void OnCancelled() = 0;

    virtual void OnError() = 0;

   protected:
    virtual ~Observer() {}
  };

  virtual void Observe(Observer* observer) = 0;

  virtual void EnsureVisible() = 0;

  virtual void EnsureHidden() = 0;

 protected:
  virtual ~DelegatedSourceListController() {}
};

}

#endif
