/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_WIN_WINDOWS_VERSION_H_
#define RTC_BASE_WIN_WINDOWS_VERSION_H_

#include <stddef.h>

#include <string>

typedef void* HANDLE;

namespace rtc {
namespace rtc_win {

enum Version {
  VERSION_PRE_XP = 0,
  VERSION_XP = 1,
  VERSION_SERVER_2003 = 2,
  VERSION_VISTA = 3,
  VERSION_WIN7 = 4,
  VERSION_WIN8 = 5,
  VERSION_WIN8_1 = 6,
  VERSION_WIN10 = 7,
  VERSION_WIN10_TH2 = 8,
  VERSION_WIN10_RS1 = 9,
  VERSION_WIN10_RS2 = 10,
  VERSION_WIN10_RS3 = 11,
  VERSION_WIN10_RS4 = 12,
  VERSION_WIN10_RS5 = 13,
  VERSION_WIN10_19H1 = 14,
  VERSION_WIN10_19H2 = 15,
  VERSION_WIN10_20H1 = 16,
  VERSION_WIN10_20H2 = 17,
  VERSION_WIN10_21H1 = 18,
  VERSION_WIN10_21H2 = 19,
  VERSION_SERVER_2022 = 20,
  VERSION_WIN11 = 21,
  VERSION_WIN_LAST,
};

enum VersionType {
  SUITE_HOME = 0,
  SUITE_PROFESSIONAL,
  SUITE_SERVER,
  SUITE_ENTERPRISE,
  SUITE_EDUCATION,
  SUITE_LAST,
};

class OSInfo {
 public:
  struct VersionNumber {
    int major;
    int minor;
    int build;
    int patch;
  };

  struct ServicePack {
    int major;
    int minor;
  };

  enum WindowsArchitecture {
    X86_ARCHITECTURE,
    X64_ARCHITECTURE,
    IA64_ARCHITECTURE,
    OTHER_ARCHITECTURE,
  };

  enum WOW64Status {
    WOW64_DISABLED,
    WOW64_ENABLED,
    WOW64_UNKNOWN,
  };

  OSInfo(const OSInfo&) = delete;
  OSInfo& operator=(const OSInfo&) = delete;

  static OSInfo* GetInstance();

  Version version() const { return version_; }
  VersionNumber version_number() const { return version_number_; }
  VersionType version_type() const { return version_type_; }
  ServicePack service_pack() const { return service_pack_; }
  std::string service_pack_str() const { return service_pack_str_; }
  WindowsArchitecture architecture() const { return architecture_; }
  int processors() const { return processors_; }
  size_t allocation_granularity() const { return allocation_granularity_; }
  WOW64Status wow64_status() const { return wow64_status_; }
  std::string processor_model_name();

  static WOW64Status GetWOW64StatusForProcess(HANDLE process_handle);

 private:
  OSInfo();
  ~OSInfo();

  Version version_;
  VersionNumber version_number_;
  VersionType version_type_;
  ServicePack service_pack_;

  std::string service_pack_str_;
  WindowsArchitecture architecture_;
  int processors_;
  size_t allocation_granularity_;
  WOW64Status wow64_status_;
  std::string processor_model_name_;
};

Version GetVersion();

}
}

#endif
