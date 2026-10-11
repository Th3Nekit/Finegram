/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_FEC_CONTROLLER_H_
#define API_FEC_CONTROLLER_H_

#include <memory>
#include <vector>

#include "api/environment/environment.h"
#include "api/video/video_frame_type.h"
#include "modules/include/module_fec_types.h"

namespace webrtc {

class VCMProtectionCallback {
 public:
  virtual int ProtectionRequest(const FecProtectionParams* delta_params,
                                const FecProtectionParams* key_params,
                                uint32_t* sent_video_rate_bps,
                                uint32_t* sent_nack_rate_bps,
                                uint32_t* sent_fec_rate_bps) = 0;

  virtual void SetRetransmissionMode(int retransmission_mode) = 0;
 protected:
  virtual ~VCMProtectionCallback() {}
};

class FecController {
 public:
  virtual ~FecController() {}

  virtual void SetProtectionCallback(
      VCMProtectionCallback* protection_callback) = 0;
  virtual void SetProtectionMethod(bool enable_fec, bool enable_nack) = 0;

  virtual void SetEncodingData(size_t width,
                               size_t height,
                               size_t num_temporal_layers,
                               size_t max_payload_size) = 0;

  virtual uint32_t UpdateFecRates(uint32_t estimated_bitrate_bps,
                                  int actual_framerate,
                                  uint8_t fraction_lost,
                                  std::vector<bool> loss_mask_vector,
                                  int64_t round_trip_time_ms) = 0;

  virtual void UpdateWithEncodedData(
      size_t encoded_image_length,
      VideoFrameType encoded_image_frametype) = 0;

  virtual bool UseLossVectorMask() = 0;
};

class FecControllerFactoryInterface {
 public:
  virtual ~FecControllerFactoryInterface() = default;

  virtual std::unique_ptr<FecController> CreateFecController(
      const Environment& env) = 0;
};

}
#endif
