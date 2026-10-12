/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_FIELD_TRIALS_H_
#define API_FIELD_TRIALS_H_

#include <memory>
#include <string>

#include "absl/strings/string_view.h"
#include "api/field_trials_registry.h"
#include "rtc_base/containers/flat_map.h"

namespace webrtc {

class FieldTrials : public FieldTrialsRegistry {
 public:
  explicit FieldTrials(const std::string& s);
  ~FieldTrials();

  static std::unique_ptr<FieldTrials> CreateNoGlobal(const std::string& s);

 private:
  explicit FieldTrials(const std::string& s, bool);

  std::string GetValue(absl::string_view key) const override;

  const bool uses_global_;
  const std::string field_trial_string_;
  const char* const previous_field_trial_string_;
  const flat_map<std::string, std::string> key_value_map_;
};

}

#endif
