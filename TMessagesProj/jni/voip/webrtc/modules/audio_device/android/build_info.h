/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_DEVICE_ANDROID_BUILD_INFO_H_
#define MODULES_AUDIO_DEVICE_ANDROID_BUILD_INFO_H_

#include <jni.h>

#include <memory>
#include <string>

#include "modules/utility/include/jvm_android.h"

namespace webrtc {

enum SdkCode {
  SDK_CODE_JELLY_BEAN = 16,
  SDK_CODE_JELLY_BEAN_MR1 = 17,
  SDK_CODE_JELLY_BEAN_MR2 = 18,
  SDK_CODE_KITKAT = 19,
  SDK_CODE_WATCH = 20,
  SDK_CODE_LOLLIPOP = 21,
  SDK_CODE_LOLLIPOP_MR1 = 22,
  SDK_CODE_MARSHMALLOW = 23,
  SDK_CODE_N = 24,
};

class BuildInfo {
 public:
  BuildInfo();
  ~BuildInfo() {}

  std::string GetDeviceModel();

  std::string GetBrand();

  std::string GetDeviceManufacturer();

  std::string GetAndroidBuildId();

  std::string GetBuildType();

  std::string GetBuildRelease();

  SdkCode GetSdkVersion();

 private:

  std::string GetStringFromJava(const char* name);

  JvmThreadConnector attach_thread_if_needed_;

  std::unique_ptr<JNIEnvironment> j_environment_;

  JavaClass j_build_info_;
};

}

#endif
