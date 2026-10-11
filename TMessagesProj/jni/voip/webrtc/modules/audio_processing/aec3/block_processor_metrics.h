/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_AEC3_BLOCK_PROCESSOR_METRICS_H_
#define MODULES_AUDIO_PROCESSING_AEC3_BLOCK_PROCESSOR_METRICS_H_

namespace webrtc {

class BlockProcessorMetrics {
 public:
  BlockProcessorMetrics() = default;

  BlockProcessorMetrics(const BlockProcessorMetrics&) = delete;
  BlockProcessorMetrics& operator=(const BlockProcessorMetrics&) = delete;

  void UpdateCapture(bool underrun);

  void UpdateRender(bool overrun);

  bool MetricsReported() { return metrics_reported_; }

 private:

  void ResetMetrics();

  int capture_block_counter_ = 0;
  bool metrics_reported_ = false;
  int render_buffer_underruns_ = 0;
  int render_buffer_overruns_ = 0;
  int buffer_render_calls_ = 0;
};

}

#endif
