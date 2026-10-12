/*
 *  Copyright 2019 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_ADAPTATION_RESOURCE_H_
#define API_ADAPTATION_RESOURCE_H_

#include <string>

#include "api/scoped_refptr.h"
#include "rtc_base/ref_count.h"
#include "rtc_base/system/rtc_export.h"

namespace webrtc {

class Resource;

enum class ResourceUsageState {

  kOveruse,

  kUnderuse,
};

RTC_EXPORT const char* ResourceUsageStateToString(
    ResourceUsageState usage_state);

class RTC_EXPORT ResourceListener {
 public:
  virtual ~ResourceListener();

  virtual void OnResourceUsageStateMeasured(
      rtc::scoped_refptr<Resource> resource,
      ResourceUsageState usage_state) = 0;
};

class RTC_EXPORT Resource : public rtc::RefCountInterface {
 public:
  Resource();

  ~Resource() override;

  virtual std::string Name() const = 0;

  virtual void SetResourceListener(ResourceListener* listener) = 0;
};

}

#endif
