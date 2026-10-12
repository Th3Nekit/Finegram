/*
 *  Copyright (c) 2023 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef LOGGING_RTC_EVENT_LOG_ENCODER_OPTIONAL_BLOB_ENCODING_H_
#define LOGGING_RTC_EVENT_LOG_ENCODER_OPTIONAL_BLOB_ENCODING_H_

#include <stddef.h>

#include <string>
#include <vector>

#include "absl/strings/string_view.h"
#include "absl/types/optional.h"

namespace webrtc {

std::string EncodeOptionalBlobs(
    const std::vector<absl::optional<std::string>>& blobs);

std::vector<absl::optional<std::string>> DecodeOptionalBlobs(
    absl::string_view encoded_blobs,
    size_t num_of_blobs);

}

#endif
