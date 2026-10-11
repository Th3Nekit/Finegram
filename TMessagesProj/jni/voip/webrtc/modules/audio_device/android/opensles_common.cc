/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_device/android/opensles_common.h"

#include <SLES/OpenSLES.h>

#include "rtc_base/arraysize.h"
#include "rtc_base/checks.h"

namespace webrtc {

const char* GetSLErrorString(size_t code) {
  static const char* sl_error_strings[] = {
      "SL_RESULT_SUCCESS",
      "SL_RESULT_PRECONDITIONS_VIOLATED",
      "SL_RESULT_PARAMETER_INVALID",
      "SL_RESULT_MEMORY_FAILURE",
      "SL_RESULT_RESOURCE_ERROR",
      "SL_RESULT_RESOURCE_LOST",
      "SL_RESULT_IO_ERROR",
      "SL_RESULT_BUFFER_INSUFFICIENT",
      "SL_RESULT_CONTENT_CORRUPTED",
      "SL_RESULT_CONTENT_UNSUPPORTED",
      "SL_RESULT_CONTENT_NOT_FOUND",
      "SL_RESULT_PERMISSION_DENIED",
      "SL_RESULT_FEATURE_UNSUPPORTED",
      "SL_RESULT_INTERNAL_ERROR",
      "SL_RESULT_UNKNOWN_ERROR",
      "SL_RESULT_OPERATION_ABORTED",
      "SL_RESULT_CONTROL_LOST",
  };

  if (code >= arraysize(sl_error_strings)) {
    return "SL_RESULT_UNKNOWN_ERROR";
  }
  return sl_error_strings[code];
}

SLDataFormat_PCM CreatePCMConfiguration(size_t channels,
                                        int sample_rate,
                                        size_t bits_per_sample) {
  RTC_CHECK_EQ(bits_per_sample, SL_PCMSAMPLEFORMAT_FIXED_16);
  SLDataFormat_PCM format;
  format.formatType = SL_DATAFORMAT_PCM;
  format.numChannels = static_cast<SLuint32>(channels);

  switch (sample_rate) {
    case 8000:
      format.samplesPerSec = SL_SAMPLINGRATE_8;
      break;
    case 16000:
      format.samplesPerSec = SL_SAMPLINGRATE_16;
      break;
    case 22050:
      format.samplesPerSec = SL_SAMPLINGRATE_22_05;
      break;
    case 32000:
      format.samplesPerSec = SL_SAMPLINGRATE_32;
      break;
    case 44100:
      format.samplesPerSec = SL_SAMPLINGRATE_44_1;
      break;
    case 48000:
      format.samplesPerSec = SL_SAMPLINGRATE_48;
      break;
    case 64000:
      format.samplesPerSec = SL_SAMPLINGRATE_64;
      break;
    case 88200:
      format.samplesPerSec = SL_SAMPLINGRATE_88_2;
      break;
    case 96000:
      format.samplesPerSec = SL_SAMPLINGRATE_96;
      break;
    default:
      RTC_CHECK(false) << "Unsupported sample rate: " << sample_rate;
      break;
  }
  format.bitsPerSample = SL_PCMSAMPLEFORMAT_FIXED_16;
  format.containerSize = SL_PCMSAMPLEFORMAT_FIXED_16;
  format.endianness = SL_BYTEORDER_LITTLEENDIAN;
  if (format.numChannels == 1) {
    format.channelMask = SL_SPEAKER_FRONT_CENTER;
  } else if (format.numChannels == 2) {
    format.channelMask = SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT;
  } else {
    RTC_CHECK(false) << "Unsupported number of channels: "
                     << format.numChannels;
  }
  return format;
}

}
