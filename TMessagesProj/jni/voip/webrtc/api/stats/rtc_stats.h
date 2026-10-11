/*
 *  Copyright 2016 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_STATS_RTC_STATS_H_
#define API_STATS_RTC_STATS_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/types/optional.h"
#include "api/stats/attribute.h"
#include "api/units/timestamp.h"
#include "rtc_base/checks.h"
#include "rtc_base/system/rtc_export.h"
#include "rtc_base/system/rtc_export_template.h"

namespace webrtc {

class RTC_EXPORT RTCStats {
 public:
  RTCStats(const std::string& id, Timestamp timestamp)
      : id_(id), timestamp_(timestamp) {}
  RTCStats(const RTCStats& other);
  virtual ~RTCStats();

  virtual std::unique_ptr<RTCStats> copy() const = 0;

  const std::string& id() const { return id_; }

  Timestamp timestamp() const { return timestamp_; }

  virtual const char* type() const = 0;

  std::vector<Attribute> Attributes() const;
  template <typename T>
  Attribute GetAttribute(const absl::optional<T>& stat) const {
    for (const auto& attribute : Attributes()) {
      if (!attribute.holds_alternative<T>()) {
        continue;
      }
      if (absl::get<const absl::optional<T>*>(attribute.as_variant()) ==
          &stat) {
        return attribute;
      }
    }
    RTC_CHECK_NOTREACHED();
  }

  bool operator==(const RTCStats& other) const;
  bool operator!=(const RTCStats& other) const;

  std::string ToJson() const;

  template <typename T>
  const T& cast_to() const {
    RTC_DCHECK_EQ(type(), T::kType);
    return static_cast<const T&>(*this);
  }

 protected:
  virtual std::vector<Attribute> AttributesImpl(
      size_t additional_capacity) const;

  std::string const id_;
  Timestamp timestamp_;
};

#define WEBRTC_RTCSTATS_DECL()                                              \
 protected:                                                                 \
  std::vector<webrtc::Attribute> AttributesImpl(size_t additional_capacity) \
      const override;                                                       \
                                                                            \
 public:                                                                    \
  static const char kType[];                                                \
                                                                            \
  std::unique_ptr<webrtc::RTCStats> copy() const override;                  \
  const char* type() const override

#define WEBRTC_RTCSTATS_IMPL(this_class, parent_class, type_str, ...)         \
  const char this_class::kType[] = type_str;                                  \
                                                                              \
  std::unique_ptr<webrtc::RTCStats> this_class::copy() const {                \
    return std::make_unique<this_class>(*this);                               \
  }                                                                           \
                                                                              \
  const char* this_class::type() const {                                      \
    return this_class::kType;                                                 \
  }                                                                           \
                                                                              \
  std::vector<webrtc::Attribute> this_class::AttributesImpl(                  \
      size_t additional_capacity) const {                                     \
    webrtc::AttributeInit attribute_inits[] = {__VA_ARGS__};                  \
    size_t attribute_inits_size =                                             \
        sizeof(attribute_inits) / sizeof(attribute_inits[0]);                 \
    std::vector<webrtc::Attribute> attributes = parent_class::AttributesImpl( \
        attribute_inits_size + additional_capacity);                          \
    for (size_t i = 0; i < attribute_inits_size; ++i) {                       \
      attributes.push_back(absl::visit(                                       \
          [&](const auto* field) {                                            \
            return Attribute(attribute_inits[i].name, field);                 \
          },                                                                  \
          attribute_inits[i].variant));                                       \
    }                                                                         \
    return attributes;                                                        \
  }

}

#endif
