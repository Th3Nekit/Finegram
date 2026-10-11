/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef MODULES_VIDEO_CODING_SVC_SCALABLE_VIDEO_CONTROLLER_H_
#define MODULES_VIDEO_CODING_SVC_SCALABLE_VIDEO_CONTROLLER_H_

#include <vector>

#include "absl/container/inlined_vector.h"
#include "api/transport/rtp/dependency_descriptor.h"
#include "api/video/video_bitrate_allocation.h"
#include "common_video/generic_frame_descriptor/generic_frame_info.h"

namespace webrtc {

class ScalableVideoController {
 public:
  struct StreamLayersConfig {
    int num_spatial_layers = 1;
    int num_temporal_layers = 1;

    bool uses_reference_scaling = true;

    int scaling_factor_num[DependencyDescriptor::kMaxSpatialIds] = {1, 1, 1, 1};
    int scaling_factor_den[DependencyDescriptor::kMaxSpatialIds] = {1, 1, 1, 1};
  };
  class LayerFrameConfig {
   public:

    LayerFrameConfig& Id(int value);
    LayerFrameConfig& Keyframe();
    LayerFrameConfig& S(int value);
    LayerFrameConfig& T(int value);
    LayerFrameConfig& Reference(int buffer_id);
    LayerFrameConfig& Update(int buffer_id);
    LayerFrameConfig& ReferenceAndUpdate(int buffer_id);

    int Id() const { return id_; }
    bool IsKeyframe() const { return is_keyframe_; }
    int SpatialId() const { return spatial_id_; }
    int TemporalId() const { return temporal_id_; }
    const absl::InlinedVector<CodecBufferUsage, kMaxEncoderBuffers>& Buffers()
        const {
      return buffers_;
    }

   private:

    int id_ = 0;

    bool is_keyframe_ = false;

    int spatial_id_ = 0;
    int temporal_id_ = 0;

    absl::InlinedVector<CodecBufferUsage, kMaxEncoderBuffers> buffers_;
  };

  virtual ~ScalableVideoController() = default;

  virtual StreamLayersConfig StreamConfig() const = 0;

  virtual FrameDependencyStructure DependencyStructure() const = 0;

  virtual void OnRatesUpdated(const VideoBitrateAllocation& bitrates) = 0;

  virtual std::vector<LayerFrameConfig> NextFrameConfig(bool restart) = 0;

  virtual GenericFrameInfo OnEncodeDone(const LayerFrameConfig& config) = 0;
};

inline ScalableVideoController::LayerFrameConfig&
ScalableVideoController::LayerFrameConfig::Id(int value) {
  id_ = value;
  return *this;
}
inline ScalableVideoController::LayerFrameConfig&
ScalableVideoController::LayerFrameConfig::Keyframe() {
  is_keyframe_ = true;
  return *this;
}
inline ScalableVideoController::LayerFrameConfig&
ScalableVideoController::LayerFrameConfig::S(int value) {
  spatial_id_ = value;
  return *this;
}
inline ScalableVideoController::LayerFrameConfig&
ScalableVideoController::LayerFrameConfig::T(int value) {
  temporal_id_ = value;
  return *this;
}
inline ScalableVideoController::LayerFrameConfig&
ScalableVideoController::LayerFrameConfig::Reference(int buffer_id) {
  buffers_.emplace_back(buffer_id,                true,             false);
  return *this;
}
inline ScalableVideoController::LayerFrameConfig&
ScalableVideoController::LayerFrameConfig::Update(int buffer_id) {
  buffers_.emplace_back(buffer_id,                false,             true);
  return *this;
}
inline ScalableVideoController::LayerFrameConfig&
ScalableVideoController::LayerFrameConfig::ReferenceAndUpdate(int buffer_id) {
  buffers_.emplace_back(buffer_id,                true,             true);
  return *this;
}

}

#endif
