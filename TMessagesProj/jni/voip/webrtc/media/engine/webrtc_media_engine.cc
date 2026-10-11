/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "media/engine/webrtc_media_engine.h"

#include <algorithm>
#include <map>
#include <string>
#include <utility>

#include "absl/algorithm/container.h"
#include "absl/strings/match.h"
#include "media/base/media_constants.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"

namespace cricket {
namespace {

void DiscardRedundantExtensions(
    std::vector<webrtc::RtpExtension>* extensions,
    rtc::ArrayView<const char* const> extensions_decreasing_prio) {
  RTC_DCHECK(extensions);
  bool found = false;
  for (const char* uri : extensions_decreasing_prio) {
    auto it = absl::c_find_if(
        *extensions,
        [uri](const webrtc::RtpExtension& rhs) { return rhs.uri == uri; });
    if (it != extensions->end()) {
      if (found) {
        extensions->erase(it);
      }
      found = true;
    }
  }
}
}

bool ValidateRtpExtensions(
    rtc::ArrayView<const webrtc::RtpExtension> extensions,
    rtc::ArrayView<const webrtc::RtpExtension> old_extensions) {
  bool id_used[1 + webrtc::RtpExtension::kMaxId] = {false};
  for (const auto& extension : extensions) {
    if (extension.id < webrtc::RtpExtension::kMinId ||
        extension.id > webrtc::RtpExtension::kMaxId) {
      RTC_LOG(LS_ERROR) << "Bad RTP extension ID: " << extension.ToString();
      return false;
    }
    if (id_used[extension.id]) {
      RTC_LOG(LS_ERROR) << "Duplicate RTP extension ID: "
                        << extension.ToString();
      return false;
    }
    id_used[extension.id] = true;
  }

  if (!old_extensions.empty()) {
    absl::string_view urimap[1 + webrtc::RtpExtension::kMaxId];
    std::map<absl::string_view, int> idmap;
    for (const auto& old_extension : old_extensions) {
      urimap[old_extension.id] = old_extension.uri;
      idmap[old_extension.uri] = old_extension.id;
    }
    for (const auto& extension : extensions) {
      if (!urimap[extension.id].empty() &&
          urimap[extension.id] != extension.uri) {
        RTC_LOG(LS_ERROR) << "Extension negotiation failure: " << extension.id
                          << " was mapped to " << urimap[extension.id]
                          << " but is proposed changed to " << extension.uri;
        return false;
      }
      const auto& it = idmap.find(extension.uri);
      if (it != idmap.end() && it->second != extension.id) {
        RTC_LOG(LS_ERROR) << "Extension negotation failure: " << extension.uri
                          << " was identified by " << it->second
                          << " but is proposed changed to " << extension.id;
        return false;
      }
    }
  }
  return true;
}

std::vector<webrtc::RtpExtension> FilterRtpExtensions(
    const std::vector<webrtc::RtpExtension>& extensions,
    bool (*supported)(absl::string_view),
    bool filter_redundant_extensions,
    const webrtc::FieldTrialsView& trials) {

  RTC_DCHECK(ValidateRtpExtensions(extensions, {}));
  RTC_DCHECK(supported);
  std::vector<webrtc::RtpExtension> result;

  for (const auto& extension : extensions) {
    if (supported(extension.uri)) {
      result.push_back(extension);
    } else {
      RTC_LOG(LS_WARNING) << "Unsupported RTP extension: "
                          << extension.ToString();
    }
  }

  absl::c_sort(result, [](const webrtc::RtpExtension& rhs,
                          const webrtc::RtpExtension& lhs) {
    return rhs.encrypt == lhs.encrypt ? rhs.uri < lhs.uri
                                      : rhs.encrypt > lhs.encrypt;
  });

  if (filter_redundant_extensions) {
    auto it = std::unique(
        result.begin(), result.end(),
        [](const webrtc::RtpExtension& rhs, const webrtc::RtpExtension& lhs) {
          return rhs.uri == lhs.uri && rhs.encrypt == lhs.encrypt;
        });
    result.erase(it, result.end());

    if (absl::StartsWith(trials.Lookup("WebRTC-FilterAbsSendTimeExtension"),
                         "Enabled")) {
      static const char* const kBweExtensionPriorities[] = {
          webrtc::RtpExtension::kTransportSequenceNumberUri,
          webrtc::RtpExtension::kAbsSendTimeUri,
          webrtc::RtpExtension::kTimestampOffsetUri};
      DiscardRedundantExtensions(&result, kBweExtensionPriorities);
    } else {
      static const char* const kBweExtensionPriorities[] = {
          webrtc::RtpExtension::kAbsSendTimeUri,
          webrtc::RtpExtension::kTimestampOffsetUri};
      DiscardRedundantExtensions(&result, kBweExtensionPriorities);
    }
  }
  return result;
}

webrtc::BitrateConstraints GetBitrateConfigForCodec(const Codec& codec) {
  webrtc::BitrateConstraints config;
  int bitrate_kbps = 0;
  if (codec.GetParam(kCodecParamMinBitrate, &bitrate_kbps) &&
      bitrate_kbps > 0) {
    config.min_bitrate_bps = bitrate_kbps * 1000;
  } else {
    config.min_bitrate_bps = 0;
  }
  if (codec.GetParam(kCodecParamStartBitrate, &bitrate_kbps) &&
      bitrate_kbps > 0) {
    config.start_bitrate_bps = bitrate_kbps * 1000;
  } else {

    config.start_bitrate_bps = -1;
  }
  if (codec.GetParam(kCodecParamMaxBitrate, &bitrate_kbps) &&
      bitrate_kbps > 0) {
    config.max_bitrate_bps = bitrate_kbps * 1000;
  } else {
    config.max_bitrate_bps = -1;
  }
  return config;
}
}
