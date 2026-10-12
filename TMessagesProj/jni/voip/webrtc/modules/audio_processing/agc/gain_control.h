/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_AGC_GAIN_CONTROL_H_
#define MODULES_AUDIO_PROCESSING_AGC_GAIN_CONTROL_H_

namespace webrtc {

class GainControl {
 public:

  virtual int set_stream_analog_level(int level) = 0;

  virtual int stream_analog_level() const = 0;

  enum Mode {

    kAdaptiveAnalog,

    kAdaptiveDigital,

    kFixedDigital
  };

  virtual int set_mode(Mode mode) = 0;
  virtual Mode mode() const = 0;

  virtual int set_target_level_dbfs(int level) = 0;
  virtual int target_level_dbfs() const = 0;

  virtual int set_compression_gain_db(int gain) = 0;
  virtual int compression_gain_db() const = 0;

  virtual int enable_limiter(bool enable) = 0;
  virtual bool is_limiter_enabled() const = 0;

  virtual int set_analog_level_limits(int minimum, int maximum) = 0;
  virtual int analog_level_minimum() const = 0;
  virtual int analog_level_maximum() const = 0;

  virtual bool stream_is_saturated() const = 0;

 protected:
  virtual ~GainControl() {}
};
}

#endif
