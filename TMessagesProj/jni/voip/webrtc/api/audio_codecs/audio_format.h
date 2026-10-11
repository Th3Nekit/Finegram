/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_AUDIO_CODECS_AUDIO_FORMAT_H_
#define API_AUDIO_CODECS_AUDIO_FORMAT_H_

#include <stddef.h>

#include <map>
#include <string>

#include "absl/strings/string_view.h"
#include "api/rtp_parameters.h"
#include "rtc_base/checks.h"
#include "rtc_base/system/rtc_export.h"

namespace webrtc {

struct RTC_EXPORT SdpAudioFormat {
  using Parameters [[deprecated(("Use webrtc::CodecParameterMap"))]] =
      std::map<std::string, std::string>;

  SdpAudioFormat(const SdpAudioFormat&);
  SdpAudioFormat(SdpAudioFormat&&);
  SdpAudioFormat(absl::string_view name, int clockrate_hz, size_t num_channels);
  SdpAudioFormat(absl::string_view name,
                 int clockrate_hz,
                 size_t num_channels,
                 const CodecParameterMap& param);
  SdpAudioFormat(absl::string_view name,
                 int clockrate_hz,
                 size_t num_channels,
                 CodecParameterMap&& param);
  ~SdpAudioFormat();

  bool Matches(const SdpAudioFormat& o) const;

  SdpAudioFormat& operator=(const SdpAudioFormat&);
  SdpAudioFormat& operator=(SdpAudioFormat&&);

  friend bool operator==(const SdpAudioFormat& a, const SdpAudioFormat& b);
  friend bool operator!=(const SdpAudioFormat& a, const SdpAudioFormat& b) {
    return !(a == b);
  }

  std::string name;
  int clockrate_hz;
  size_t num_channels;
  CodecParameterMap parameters;
};

struct AudioCodecInfo {
  AudioCodecInfo(int sample_rate_hz, size_t num_channels, int bitrate_bps);
  AudioCodecInfo(int sample_rate_hz,
                 size_t num_channels,
                 int default_bitrate_bps,
                 int min_bitrate_bps,
                 int max_bitrate_bps);
  AudioCodecInfo(const AudioCodecInfo& b) = default;
  ~AudioCodecInfo() = default;

  bool operator==(const AudioCodecInfo& b) const {
    return sample_rate_hz == b.sample_rate_hz &&
           num_channels == b.num_channels &&
           default_bitrate_bps == b.default_bitrate_bps &&
           min_bitrate_bps == b.min_bitrate_bps &&
           max_bitrate_bps == b.max_bitrate_bps &&
           allow_comfort_noise == b.allow_comfort_noise &&
           supports_network_adaption == b.supports_network_adaption;
  }

  bool operator!=(const AudioCodecInfo& b) const { return !(*this == b); }

  bool HasFixedBitrate() const {
    RTC_DCHECK_GE(min_bitrate_bps, 0);
    RTC_DCHECK_LE(min_bitrate_bps, default_bitrate_bps);
    RTC_DCHECK_GE(max_bitrate_bps, default_bitrate_bps);
    return min_bitrate_bps == max_bitrate_bps;
  }

  int sample_rate_hz;
  size_t num_channels;
  int default_bitrate_bps;
  int min_bitrate_bps;
  int max_bitrate_bps;

  bool allow_comfort_noise = true;

  bool supports_network_adaption = false;

};

struct AudioCodecSpec {
  bool operator==(const AudioCodecSpec& b) const {
    return format == b.format && info == b.info;
  }

  bool operator!=(const AudioCodecSpec& b) const { return !(*this == b); }

  SdpAudioFormat format;
  AudioCodecInfo info;
};

}

#endif
