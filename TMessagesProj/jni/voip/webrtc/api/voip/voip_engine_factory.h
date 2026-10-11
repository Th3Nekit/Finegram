/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_VOIP_VOIP_ENGINE_FACTORY_H_
#define API_VOIP_VOIP_ENGINE_FACTORY_H_

#include <memory>

#include "api/audio_codecs/audio_decoder_factory.h"
#include "api/audio_codecs/audio_encoder_factory.h"
#include "api/scoped_refptr.h"
#include "api/task_queue/task_queue_factory.h"
#include "api/voip/voip_engine.h"
#include "modules/audio_device/include/audio_device.h"
#include "modules/audio_processing/include/audio_processing.h"

namespace webrtc {

struct VoipEngineConfig {

  rtc::scoped_refptr<AudioEncoderFactory> encoder_factory;

  rtc::scoped_refptr<AudioDecoderFactory> decoder_factory;

  std::unique_ptr<TaskQueueFactory> task_queue_factory;

  rtc::scoped_refptr<AudioDeviceModule> audio_device_module;

  rtc::scoped_refptr<AudioProcessing> audio_processing;
};

std::unique_ptr<VoipEngine> CreateVoipEngine(VoipEngineConfig config);

}

#endif
