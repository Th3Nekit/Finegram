/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef AUDIO_CHANNEL_RECEIVE_FRAME_TRANSFORMER_DELEGATE_H_
#define AUDIO_CHANNEL_RECEIVE_FRAME_TRANSFORMER_DELEGATE_H_

#include <memory>
#include <string>

#include "api/frame_transformer_interface.h"
#include "api/sequence_checker.h"
#include "api/task_queue/task_queue_base.h"
#include "rtc_base/system/no_unique_address.h"
#include "rtc_base/thread.h"

namespace webrtc {

class ChannelReceiveFrameTransformerDelegate : public TransformedFrameCallback {
 public:
  using ReceiveFrameCallback =
      std::function<void(rtc::ArrayView<const uint8_t> packet,
                         const RTPHeader& header)>;
  ChannelReceiveFrameTransformerDelegate(
      ReceiveFrameCallback receive_frame_callback,
      rtc::scoped_refptr<FrameTransformerInterface> frame_transformer,
      TaskQueueBase* channel_receive_thread);

  void Init();

  void Reset();

  void Transform(rtc::ArrayView<const uint8_t> packet,
                 const RTPHeader& header,
                 uint32_t ssrc,
                 const std::string& codec_mime_type);

  void OnTransformedFrame(
      std::unique_ptr<TransformableFrameInterface> frame) override;

  void StartShortCircuiting() override;

  void ReceiveFrame(std::unique_ptr<TransformableFrameInterface> frame) const;

  rtc::scoped_refptr<FrameTransformerInterface> FrameTransformer();

 protected:
  ~ChannelReceiveFrameTransformerDelegate() override = default;

 private:
  RTC_NO_UNIQUE_ADDRESS SequenceChecker sequence_checker_;
  ReceiveFrameCallback receive_frame_callback_
      RTC_GUARDED_BY(sequence_checker_);
  rtc::scoped_refptr<FrameTransformerInterface> frame_transformer_
      RTC_GUARDED_BY(sequence_checker_);
  TaskQueueBase* const channel_receive_thread_;
  bool short_circuit_ RTC_GUARDED_BY(sequence_checker_) = false;
};

}
#endif
