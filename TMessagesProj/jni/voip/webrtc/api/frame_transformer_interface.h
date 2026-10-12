/*
 *  Copyright 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_FRAME_TRANSFORMER_INTERFACE_H_
#define API_FRAME_TRANSFORMER_INTERFACE_H_

#include <memory>
#include <string>

#include "api/ref_count.h"
#include "api/scoped_refptr.h"
#include "api/video/encoded_frame.h"
#include "api/video/video_frame_metadata.h"

namespace webrtc {

class TransformableFrameInterface {
 public:
  virtual ~TransformableFrameInterface() = default;

  virtual rtc::ArrayView<const uint8_t> GetData() const = 0;

  virtual void SetData(rtc::ArrayView<const uint8_t> data) = 0;

  virtual uint8_t GetPayloadType() const = 0;
  virtual uint32_t GetSsrc() const = 0;
  virtual uint32_t GetTimestamp() const = 0;
  virtual void SetRTPTimestamp(uint32_t timestamp) = 0;

  virtual absl::optional<Timestamp> GetCaptureTimeIdentifier() const {
    return absl::nullopt;
  }

  enum class Direction {
    kUnknown,
    kReceiver,
    kSender,
  };

  virtual Direction GetDirection() const { return Direction::kUnknown; }
  virtual std::string GetMimeType() const = 0;
};

class TransformableVideoFrameInterface : public TransformableFrameInterface {
 public:
  virtual ~TransformableVideoFrameInterface() = default;
  virtual bool IsKeyFrame() const = 0;

  virtual VideoFrameMetadata Metadata() const = 0;

  virtual void SetMetadata(const VideoFrameMetadata&) = 0;
};

class TransformableAudioFrameInterface : public TransformableFrameInterface {
 public:
  virtual ~TransformableAudioFrameInterface() = default;

  virtual rtc::ArrayView<const uint32_t> GetContributingSources() const = 0;

  virtual const absl::optional<uint16_t> SequenceNumber() const = 0;

  virtual absl::optional<uint64_t> AbsoluteCaptureTimestamp() const = 0;

  enum class FrameType { kEmptyFrame, kAudioFrameSpeech, kAudioFrameCN };

  virtual FrameType Type() const { return FrameType::kEmptyFrame; }
};

class TransformedFrameCallback : public rtc::RefCountInterface {
 public:
  virtual void OnTransformedFrame(
      std::unique_ptr<TransformableFrameInterface> frame) = 0;

  virtual void StartShortCircuiting() {}

 protected:
  ~TransformedFrameCallback() override = default;
};

class FrameTransformerInterface : public rtc::RefCountInterface {
 public:

  virtual void Transform(
      std::unique_ptr<TransformableFrameInterface> transformable_frame) = 0;

  virtual void RegisterTransformedFrameCallback(
      rtc::scoped_refptr<TransformedFrameCallback>) {}
  virtual void RegisterTransformedFrameSinkCallback(
      rtc::scoped_refptr<TransformedFrameCallback>,
      uint32_t ssrc) {}
  virtual void UnregisterTransformedFrameCallback() {}
  virtual void UnregisterTransformedFrameSinkCallback(uint32_t ssrc) {}

 protected:
  ~FrameTransformerInterface() override = default;
};

}

#endif
