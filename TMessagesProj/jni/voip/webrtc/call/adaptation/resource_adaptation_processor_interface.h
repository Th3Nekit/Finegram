/*
 *  Copyright 2020 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef CALL_ADAPTATION_RESOURCE_ADAPTATION_PROCESSOR_INTERFACE_H_
#define CALL_ADAPTATION_RESOURCE_ADAPTATION_PROCESSOR_INTERFACE_H_

#include <map>
#include <vector>

#include "absl/types/optional.h"
#include "api/adaptation/resource.h"
#include "api/rtp_parameters.h"
#include "api/scoped_refptr.h"
#include "api/task_queue/task_queue_base.h"
#include "api/video/video_adaptation_counters.h"
#include "api/video/video_frame.h"
#include "call/adaptation/adaptation_constraint.h"
#include "call/adaptation/encoder_settings.h"
#include "call/adaptation/video_source_restrictions.h"

namespace webrtc {

class ResourceLimitationsListener {
 public:
  virtual ~ResourceLimitationsListener();

  virtual void OnResourceLimitationChanged(
      rtc::scoped_refptr<Resource> resource,
      const std::map<rtc::scoped_refptr<Resource>, VideoAdaptationCounters>&
          resource_limitations) = 0;
};

class ResourceAdaptationProcessorInterface {
 public:
  virtual ~ResourceAdaptationProcessorInterface();

  virtual void AddResourceLimitationsListener(
      ResourceLimitationsListener* limitations_listener) = 0;
  virtual void RemoveResourceLimitationsListener(
      ResourceLimitationsListener* limitations_listener) = 0;

  virtual void AddResource(rtc::scoped_refptr<Resource> resource) = 0;
  virtual std::vector<rtc::scoped_refptr<Resource>> GetResources() const = 0;
  virtual void RemoveResource(rtc::scoped_refptr<Resource> resource) = 0;
};

}

#endif
