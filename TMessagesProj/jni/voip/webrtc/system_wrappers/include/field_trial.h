//
// Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
//

#ifndef SYSTEM_WRAPPERS_INCLUDE_FIELD_TRIAL_H_
#define SYSTEM_WRAPPERS_INCLUDE_FIELD_TRIAL_H_

#include <string>

#include "absl/strings/string_view.h"
#include "rtc_base/containers/flat_set.h"

namespace webrtc {
namespace field_trial {

std::string FindFullName(absl::string_view name);

inline bool IsEnabled(absl::string_view name) {
  return FindFullName(name).find("Enabled") == 0;
}

inline bool IsDisabled(absl::string_view name) {
  return FindFullName(name).find("Disabled") == 0;
}

void InitFieldTrialsFromString(const char* trials_string);

const char* GetFieldTrialString();

bool FieldTrialsStringIsValid(absl::string_view trials_string);

std::string MergeFieldTrialsStrings(absl::string_view first,
                                    absl::string_view second);

class FieldTrialsAllowedInScopeForTesting {
 public:
  explicit FieldTrialsAllowedInScopeForTesting(flat_set<std::string> keys);
  ~FieldTrialsAllowedInScopeForTesting();
};

}
}

#endif
