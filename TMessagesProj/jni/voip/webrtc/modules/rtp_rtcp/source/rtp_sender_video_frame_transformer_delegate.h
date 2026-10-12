/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_RTP_RTCP_SOURCE_RTP_SENDER_VIDEO_FRAME_TRANSFORMER_DELEGATE_H_
#define MODULES_RTP_RTCP_SOURCE_RTP_SENDER_VIDEO_FRAME_TRANSFORMER_DELEGATE_H_

#include <memory>
#include <vector>

#include "api/frame_transformer_interface.h"
#include "api/scoped_refptr.h"
#include "api/sequence_checker.h"
#include "api/task_queue/task_queue_base.h"
#include "api/task_queue/task_queue_factory.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "api/video/video_layers_allocation.h"
#include "rtc_base/synchronization/mutex.h"

namespace webrtc {

class RTPVideoFrameSenderInterface {
 public:
  virtual bool SendVideo(int payload_type,
                         absl::optional<VideoCodecType> codec_type,
                         uint32_t rtp_timestamp,
                         Timestamp capture_time,
                         rtc::ArrayView<const uint8_t> payload,
                         size_t encoder_output_size,
                         RTPVideoHeader video_header,
                         TimeDelta expected_retransmission_time,
                         std::vector<uint32_t> csrcs) = 0;

  virtual void SetVideoStructureAfterTransformation(
      const FrameDependencyStructure* video_structure) = 0;
  virtual void SetVideoLayersAllocationAfterTransformation(
      VideoLayersAllocation allocation) = 0;

 protected:
  virtual ~RTPVideoFrameSenderInterface() = default;
};

class RTPSenderVideoFrameTransformerDelegate : public TransformedFrameCallback {
 public:
  RTPSenderVideoFrameTransformerDelegate(
      RTPVideoFrameSenderInterface* sender,
      rtc::scoped_refptr<FrameTransformerInterface> frame_transformer,
      uint32_t ssrc,
      TaskQueueFactory* send_transport_queue);

  void Init();

  bool TransformFrame(int payload_type,
                      absl::optional<VideoCodecType> codec_type,
                      uint32_t rtp_timestamp,
                      const EncodedImage& encoded_image,
                      RTPVideoHeader video_header,
                      TimeDelta expected_retransmission_time);

  void OnTransformedFrame(
      std::unique_ptr<TransformableFrameInterface> frame) override;

  void StartShortCircuiting() override;

  void SendVideo(std::unique_ptr<TransformableFrameInterface> frame) const
      RTC_RUN_ON(transformation_queue_);

  void SetVideoStructureUnderLock(
      const FrameDependencyStructure* video_structure);

  void SetVideoLayersAllocationUnderLock(VideoLayersAllocation allocation);

  void Reset();

 protected:
  ~RTPSenderVideoFrameTransformerDelegate() override = default;

 private:
  void EnsureEncoderQueueCreated();

  mutable Mutex sender_lock_;
  RTPVideoFrameSenderInterface* sender_ RTC_GUARDED_BY(sender_lock_);
  rtc::scoped_refptr<FrameTransformerInterface> frame_transformer_;
  const uint32_t ssrc_;

  std::unique_ptr<TaskQueueBase, TaskQueueDeleter> transformation_queue_;
  bool short_circuit_ RTC_GUARDED_BY(sender_lock_) = false;
};

std::unique_ptr<TransformableVideoFrameInterface> CloneSenderVideoFrame(
    TransformableVideoFrameInterface* original);

}

#endif
