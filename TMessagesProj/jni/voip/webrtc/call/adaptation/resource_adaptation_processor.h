/*
 *  Copyright 2020 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef CALL_ADAPTATION_RESOURCE_ADAPTATION_PROCESSOR_H_
#define CALL_ADAPTATION_RESOURCE_ADAPTATION_PROCESSOR_H_

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "api/adaptation/resource.h"
#include "api/rtp_parameters.h"
#include "api/scoped_refptr.h"
#include "api/task_queue/task_queue_base.h"
#include "api/video/video_adaptation_counters.h"
#include "api/video/video_frame.h"
#include "call/adaptation/resource_adaptation_processor_interface.h"
#include "call/adaptation/video_source_restrictions.h"
#include "call/adaptation/video_stream_adapter.h"
#include "call/adaptation/video_stream_input_state.h"
#include "call/adaptation/video_stream_input_state_provider.h"
#include "video/video_stream_encoder_observer.h"

namespace webrtc {

class ResourceAdaptationProcessor : public ResourceAdaptationProcessorInterface,
                                    public VideoSourceRestrictionsListener,
                                    public ResourceListener {
 public:
  explicit ResourceAdaptationProcessor(
      VideoStreamAdapter* video_stream_adapter);
  ~ResourceAdaptationProcessor() override;

  void AddResourceLimitationsListener(
      ResourceLimitationsListener* limitations_listener) override;
  void RemoveResourceLimitationsListener(
      ResourceLimitationsListener* limitations_listener) override;
  void AddResource(rtc::scoped_refptr<Resource> resource) override;
  std::vector<rtc::scoped_refptr<Resource>> GetResources() const override;
  void RemoveResource(rtc::scoped_refptr<Resource> resource) override;

  void OnResourceUsageStateMeasured(rtc::scoped_refptr<Resource> resource,
                                    ResourceUsageState usage_state) override;

  void OnVideoSourceRestrictionsUpdated(
      VideoSourceRestrictions restrictions,
      const VideoAdaptationCounters& adaptation_counters,
      rtc::scoped_refptr<Resource> reason,
      const VideoSourceRestrictions& unfiltered_restrictions) override;

 private:

  class ResourceListenerDelegate : public rtc::RefCountInterface,
                                   public ResourceListener {
   public:
    explicit ResourceListenerDelegate(ResourceAdaptationProcessor* processor);

    void OnProcessorDestroyed();

    void OnResourceUsageStateMeasured(rtc::scoped_refptr<Resource> resource,
                                      ResourceUsageState usage_state) override;

   private:
    TaskQueueBase* task_queue_;
    ResourceAdaptationProcessor* processor_ RTC_GUARDED_BY(task_queue_);
  };

  enum class MitigationResult {
    kNotMostLimitedResource,
    kSharedMostLimitedResource,
    kRejectedByAdapter,
    kAdaptationApplied,
  };

  struct MitigationResultAndLogMessage {
    MitigationResultAndLogMessage();
    MitigationResultAndLogMessage(MitigationResult result,
                                  absl::string_view message);
    MitigationResult result;
    std::string message;
  };

  MitigationResultAndLogMessage OnResourceUnderuse(
      rtc::scoped_refptr<Resource> reason_resource);
  MitigationResultAndLogMessage OnResourceOveruse(
      rtc::scoped_refptr<Resource> reason_resource);

  void UpdateResourceLimitations(rtc::scoped_refptr<Resource> reason_resource,
                                 const VideoSourceRestrictions& restrictions,
                                 const VideoAdaptationCounters& counters)
      RTC_RUN_ON(task_queue_);

  std::pair<std::vector<rtc::scoped_refptr<Resource>>,
            VideoStreamAdapter::RestrictionsWithCounters>
  FindMostLimitedResources() const RTC_RUN_ON(task_queue_);

  void RemoveLimitationsImposedByResource(
      rtc::scoped_refptr<Resource> resource);

  TaskQueueBase* task_queue_;
  rtc::scoped_refptr<ResourceListenerDelegate> resource_listener_delegate_;

  mutable Mutex resources_lock_;
  std::vector<rtc::scoped_refptr<Resource>> resources_
      RTC_GUARDED_BY(resources_lock_);
  std::vector<ResourceLimitationsListener*> resource_limitations_listeners_
      RTC_GUARDED_BY(task_queue_);

  std::map<rtc::scoped_refptr<Resource>,
           VideoStreamAdapter::RestrictionsWithCounters>
      adaptation_limits_by_resources_ RTC_GUARDED_BY(task_queue_);

  VideoStreamAdapter* const stream_adapter_ RTC_GUARDED_BY(task_queue_);
  VideoSourceRestrictions last_reported_source_restrictions_
      RTC_GUARDED_BY(task_queue_);

  std::map<Resource*, MitigationResult> previous_mitigation_results_
      RTC_GUARDED_BY(task_queue_);
};

}

#endif
