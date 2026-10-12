/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef LOGGING_RTC_EVENT_LOG_EVENTS_RTC_EVENT_FIELD_ENCODING_H_
#define LOGGING_RTC_EVENT_LOG_EVENTS_RTC_EVENT_FIELD_ENCODING_H_

#include <string>
#include <vector>

#include "absl/types/optional.h"
#include "api/array_view.h"
#include "api/rtc_event_log/rtc_event.h"
#include "logging/rtc_event_log/encoder/rtc_event_log_encoder_common.h"
#include "logging/rtc_event_log/events/fixed_length_encoding_parameters_v3.h"
#include "logging/rtc_event_log/events/rtc_event_field_extraction.h"
#include "rtc_base/logging.h"

namespace webrtc {

enum class FieldType : uint8_t {
  kFixed8 = 0,
  kFixed32 = 1,
  kFixed64 = 2,
  kVarInt = 3,
  kString = 4,
};

struct EventParameters {

  const char* const name;

  const RtcEvent::Type id;
};

struct FieldParameters {

  const char* const name;

  const uint64_t field_id;

  const FieldType field_type;

  const uint64_t value_width;

  static constexpr uint64_t kTimestampField = 0;
};

class EventEncoder {
 public:
  EventEncoder(EventParameters params, rtc::ArrayView<const RtcEvent*> batch);

  void EncodeField(const FieldParameters& params,
                   const std::vector<uint64_t>& values,
                   const std::vector<bool>* positions = nullptr);

  void EncodeField(const FieldParameters& params,
                   const ValuesWithPositions& values);

  void EncodeField(const FieldParameters& params,
                   const std::vector<absl::string_view>& values);

  std::string AsString();

 private:
  size_t batch_size_;
  uint32_t event_tag_;
  std::vector<std::string> encoded_fields_;
};

std::string EncodeSingleValue(uint64_t value, FieldType field_type);
std::string EncodeDeltasV3(FixedLengthEncodingParametersV3 params,
                           uint64_t base,
                           rtc::ArrayView<const uint64_t> values);

template <typename T,
          typename E,
          std::enable_if_t<std::is_integral<T>::value, bool> = true>
std::vector<uint64_t> ExtractRtcEventMember(
    rtc::ArrayView<const RtcEvent*> batch,
    const T E::*member) {
  std::vector<uint64_t> values;
  values.reserve(batch.size());
  for (const RtcEvent* event : batch) {
    RTC_CHECK_EQ(event->GetType(), E::kType);
    T value = static_cast<const E*>(event)->*member;
    values.push_back(EncodeAsUnsigned(value));
  }
  return values;
}

template <typename T,
          typename E,
          std::enable_if_t<std::is_integral<T>::value, bool> = true>
ValuesWithPositions ExtractRtcEventMember(rtc::ArrayView<const RtcEvent*> batch,
                                          const absl::optional<T> E::*member) {
  ValuesWithPositions result;
  result.position_mask.reserve(batch.size());
  result.values.reserve(batch.size());
  for (const RtcEvent* event : batch) {
    RTC_CHECK_EQ(event->GetType(), E::kType);
    absl::optional<T> field = static_cast<const E*>(event)->*member;
    result.position_mask.push_back(field.has_value());
    if (field.has_value()) {
      result.values.push_back(EncodeAsUnsigned(field.value()));
    }
  }
  return result;
}

template <typename T,
          typename E,
          std::enable_if_t<std::is_enum<T>::value, bool> = true>
std::vector<uint64_t> ExtractRtcEventMember(
    rtc::ArrayView<const RtcEvent*> batch,
    const T E::*member) {
  std::vector<uint64_t> values;
  values.reserve(batch.size());
  for (const RtcEvent* event : batch) {
    RTC_CHECK_EQ(event->GetType(), E::kType);
    T value = static_cast<const E*>(event)->*member;
    values.push_back(RtcEventLogEnum<T>::Encode(value));
  }
  return values;
}

template <typename E>
std::vector<absl::string_view> ExtractRtcEventMember(
    rtc::ArrayView<const RtcEvent*> batch,
    const std::string E::*member) {
  std::vector<absl::string_view> values;
  values.reserve(batch.size());
  for (const RtcEvent* event : batch) {
    RTC_CHECK_EQ(event->GetType(), E::kType);
    absl::string_view str = static_cast<const E*>(event)->*member;
    values.push_back(str);
  }
  return values;
}

}
#endif
