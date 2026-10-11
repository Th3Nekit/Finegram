/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef SDK_ANDROID_NATIVE_API_STACKTRACE_STACKTRACE_H_
#define SDK_ANDROID_NATIVE_API_STACKTRACE_STACKTRACE_H_

#include <string>
#include <vector>

namespace webrtc {

struct StackTraceElement {

  const char* shared_object_path;

  uint32_t relative_address;

  const char* symbol_name;
};

std::vector<StackTraceElement> GetStackTrace(int tid);

std::vector<StackTraceElement> GetStackTrace();

std::string StackTraceToString(
    const std::vector<StackTraceElement>& stack_trace);

}

#endif
