/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_TRANSPORT_BITRATE_SETTINGS_H_
#define API_TRANSPORT_BITRATE_SETTINGS_H_

#include <algorithm>

#include "absl/types/optional.h"
#include "rtc_base/system/rtc_export.h"

namespace webrtc {

struct RTC_EXPORT BitrateSettings {
  BitrateSettings();
  ~BitrateSettings();
  BitrateSettings(const BitrateSettings&);

  absl::optional<int> min_bitrate_bps;
  absl::optional<int> start_bitrate_bps;
  absl::optional<int> max_bitrate_bps;
};

struct BitrateConstraints {
  int min_bitrate_bps = 0;
  int start_bitrate_bps = kDefaultStartBitrateBps;
  int max_bitrate_bps = -1;

 private:
  static constexpr int kDefaultStartBitrateBps = 300000;
};

}

#endif
