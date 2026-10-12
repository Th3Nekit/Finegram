/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef LOGGING_RTC_EVENT_LOG_ENCODER_VAR_INT_H_
#define LOGGING_RTC_EVENT_LOG_ENCODER_VAR_INT_H_

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <utility>

#include "absl/strings/string_view.h"
#include "rtc_base/bitstream_reader.h"

namespace webrtc {

extern const size_t kMaxVarIntLengthBytes;

std::string EncodeVarInt(uint64_t input);

std::pair<bool, absl::string_view> DecodeVarInt(absl::string_view input,
                                                uint64_t* output);

uint64_t DecodeVarInt(BitstreamReader& input);

}

#endif
