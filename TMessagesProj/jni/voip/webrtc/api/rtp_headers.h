/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_RTP_HEADERS_H_
#define API_RTP_HEADERS_H_

#include <stddef.h>
#include <stdint.h>

#include <string>

#include "absl/types/optional.h"
#include "api/array_view.h"
#include "api/units/timestamp.h"
#include "api/video/color_space.h"
#include "api/video/video_content_type.h"
#include "api/video/video_rotation.h"
#include "api/video/video_timing.h"

namespace webrtc {

struct FeedbackRequest {

  bool include_timestamps;

  int sequence_count;
};

struct AbsoluteCaptureTime {

  uint64_t absolute_capture_timestamp;

  absl::optional<int64_t> estimated_capture_clock_offset;
};

inline bool operator==(const AbsoluteCaptureTime& lhs,
                       const AbsoluteCaptureTime& rhs) {
  return (lhs.absolute_capture_timestamp == rhs.absolute_capture_timestamp) &&
         (lhs.estimated_capture_clock_offset ==
          rhs.estimated_capture_clock_offset);
}

inline bool operator!=(const AbsoluteCaptureTime& lhs,
                       const AbsoluteCaptureTime& rhs) {
  return !(lhs == rhs);
}

struct RTPHeaderExtension {
  RTPHeaderExtension();
  RTPHeaderExtension(const RTPHeaderExtension& other);
  RTPHeaderExtension& operator=(const RTPHeaderExtension& other);

  static constexpr int kAbsSendTimeFraction = 18;

  Timestamp GetAbsoluteSendTimestamp() const {
    RTC_DCHECK(hasAbsoluteSendTime);
    RTC_DCHECK(absoluteSendTime < (1ul << 24));
    return Timestamp::Micros((absoluteSendTime * 1000000ll) /
                             (1 << kAbsSendTimeFraction));
  }

  bool hasTransmissionTimeOffset;
  int32_t transmissionTimeOffset;
  bool hasAbsoluteSendTime;
  uint32_t absoluteSendTime;
  absl::optional<AbsoluteCaptureTime> absolute_capture_time;
  bool hasTransportSequenceNumber;
  uint16_t transportSequenceNumber;
  absl::optional<FeedbackRequest> feedback_request;

  bool hasAudioLevel;
  bool voiceActivity;
  uint8_t audioLevel;

  bool hasVideoRotation;
  VideoRotation videoRotation;

  bool hasVideoContentType;
  VideoContentType videoContentType;

  bool has_video_timing;
  VideoSendTiming video_timing;

  VideoPlayoutDelay playout_delay;

  std::string stream_id;
  std::string repaired_stream_id;

  std::string mid;

  absl::optional<ColorSpace> color_space;
};

enum { kRtpCsrcSize = 15 };

struct RTC_EXPORT RTPHeader {
  RTPHeader();
  RTPHeader(const RTPHeader& other);
  RTPHeader& operator=(const RTPHeader& other);

  bool markerBit;
  uint8_t payloadType;
  uint16_t sequenceNumber;
  uint32_t timestamp;
  uint32_t ssrc;
  uint8_t numCSRCs;
  uint32_t arrOfCSRCs[kRtpCsrcSize];
  size_t paddingLength;
  size_t headerLength;
  RTPHeaderExtension extension;
};

enum class RtcpMode { kOff, kCompound, kReducedSize };

enum NetworkState {
  kNetworkUp,
  kNetworkDown,
};

}

#endif
