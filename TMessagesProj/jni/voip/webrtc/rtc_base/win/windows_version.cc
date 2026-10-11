/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/win/windows_version.h"

#include <windows.h>

#include <memory>

#include "rtc_base/checks.h"
#include "rtc_base/string_utils.h"

#if !defined(__clang__) && _MSC_FULL_VER < 191125507
#error VS 2017 Update 3.2 or higher is required
#endif

#if !defined(WINUWP)

namespace {

typedef BOOL(WINAPI* GetProductInfoPtr)(DWORD, DWORD, DWORD, DWORD, PDWORD);

const REGSAM kWow64AccessMask = KEY_WOW64_32KEY | KEY_WOW64_64KEY;

class RegKey {
 public:
  RegKey() : key_(nullptr), wow64access_(0) {}

  RegKey(HKEY rootkey, const wchar_t* subkey, REGSAM access)
      : key_(nullptr), wow64access_(0) {
    if (rootkey) {
      if (access & (KEY_SET_VALUE | KEY_CREATE_SUB_KEY | KEY_CREATE_LINK))
        Create(rootkey, subkey, access);
      else
        Open(rootkey, subkey, access);
    } else {
      RTC_DCHECK(!subkey);
      wow64access_ = access & kWow64AccessMask;
    }
  }

  ~RegKey() { Close(); }

  LONG Create(HKEY rootkey, const wchar_t* subkey, REGSAM access) {
    DWORD disposition_value;
    return CreateWithDisposition(rootkey, subkey, &disposition_value, access);
  }

  LONG CreateWithDisposition(HKEY rootkey,
                             const wchar_t* subkey,
                             DWORD* disposition,
                             REGSAM access) {
    RTC_DCHECK(rootkey && subkey && access && disposition);
    HKEY subhkey = NULL;
    LONG result =
        ::RegCreateKeyExW(rootkey, subkey, 0, NULL, REG_OPTION_NON_VOLATILE,
                          access, NULL, &subhkey, disposition);
    if (result == ERROR_SUCCESS) {
      Close();
      key_ = subhkey;
      wow64access_ = access & kWow64AccessMask;
    }

    return result;
  }

  LONG Open(HKEY rootkey, const wchar_t* subkey, REGSAM access) {
    RTC_DCHECK(rootkey && subkey && access);
    HKEY subhkey = NULL;

    LONG result = ::RegOpenKeyExW(rootkey, subkey, 0, access, &subhkey);
    if (result == ERROR_SUCCESS) {
      Close();
      key_ = subhkey;
      wow64access_ = access & kWow64AccessMask;
    }

    return result;
  }

  void Close() {
    if (key_) {
      ::RegCloseKey(key_);
      key_ = nullptr;
    }
  }

  LONG ReadValueDW(const wchar_t* name, DWORD* out_value) const {
    RTC_DCHECK(out_value);
    DWORD type = REG_DWORD;
    DWORD size = sizeof(DWORD);
    DWORD local_value = 0;
    LONG result = ReadValue(name, &local_value, &size, &type);
    if (result == ERROR_SUCCESS) {
      if ((type == REG_DWORD || type == REG_BINARY) && size == sizeof(DWORD))
        *out_value = local_value;
      else
        result = ERROR_CANTREAD;
    }

    return result;
  }

  LONG ReadValue(const wchar_t* name, std::wstring* out_value) const {
    RTC_DCHECK(out_value);
    const size_t kMaxStringLength = 1024;

    wchar_t raw_value[kMaxStringLength];
    DWORD type = REG_SZ, size = sizeof(raw_value);
    LONG result = ReadValue(name, raw_value, &size, &type);
    if (result == ERROR_SUCCESS) {
      if (type == REG_SZ) {
        *out_value = raw_value;
      } else if (type == REG_EXPAND_SZ) {
        wchar_t expanded[kMaxStringLength];
        size =
            ::ExpandEnvironmentStringsW(raw_value, expanded, kMaxStringLength);

        if (size == 0 || size > kMaxStringLength) {
          result = ERROR_MORE_DATA;
        } else {
          *out_value = expanded;
        }
      } else {

        result = ERROR_CANTREAD;
      }
    }

    return result;
  }

  LONG ReadValue(const wchar_t* name,
                 void* data,
                 DWORD* dsize,
                 DWORD* dtype) const {
    LONG result = RegQueryValueExW(key_, name, 0, dtype,
                                   reinterpret_cast<LPBYTE>(data), dsize);
    return result;
  }

 private:
  HKEY key_;
  REGSAM wow64access_;
};

}

#endif

namespace rtc {
namespace rtc_win {
namespace {

Version MajorMinorBuildToVersion(int major, int minor, int build) {
  if ((major == 5) && (minor > 0)) {

    return (minor == 1) ? VERSION_XP : VERSION_SERVER_2003;
  } else if (major == 6) {
    switch (minor) {
      case 0:

        return VERSION_VISTA;
      case 1:

        return VERSION_WIN7;
      case 2:

        return VERSION_WIN8;
      default:
        RTC_DCHECK_EQ(minor, 3);
        return VERSION_WIN8_1;
    }
  } else if (major == 10) {
    if (build < 10586) {
      return VERSION_WIN10;
    } else if (build < 14393) {
      return VERSION_WIN10_TH2;
    } else if (build < 15063) {
      return VERSION_WIN10_RS1;
    } else if (build < 16299) {
      return VERSION_WIN10_RS2;
    } else if (build < 17134) {
      return VERSION_WIN10_RS3;
    } else if (build < 17763) {
      return VERSION_WIN10_RS4;
    } else if (build < 18362) {
      return VERSION_WIN10_RS5;
    } else if (build < 18363) {
      return VERSION_WIN10_19H1;
    } else if (build < 19041) {
      return VERSION_WIN10_19H2;
    } else if (build < 19042) {
      return VERSION_WIN10_20H1;
    } else if (build < 19043) {
      return VERSION_WIN10_20H2;
    } else if (build < 19044) {
      return VERSION_WIN10_21H1;
    } else if (build < 20348) {
      return VERSION_WIN10_21H2;
    } else if (build < 22000) {
      return VERSION_SERVER_2022;
    } else {
      return VERSION_WIN11;
    }
  } else if (major == 11) {
    return VERSION_WIN11;
  } else if (major > 6) {
    RTC_DCHECK_NOTREACHED();
    return VERSION_WIN_LAST;
  }

  return VERSION_PRE_XP;
}

int GetUBR() {
#if defined(WINUWP)

  return 0;
#else

  static constexpr wchar_t kRegKeyWindowsNTCurrentVersion[] =
      L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion";

  RegKey key;
  if (key.Open(HKEY_LOCAL_MACHINE, kRegKeyWindowsNTCurrentVersion,
               KEY_QUERY_VALUE) != ERROR_SUCCESS) {
    return 0;
  }

  DWORD ubr = 0;
  key.ReadValueDW(L"UBR", &ubr);

  return static_cast<int>(ubr);
#endif
}

}

OSInfo* OSInfo::GetInstance() {

  static OSInfo* info;
  if (!info) {
    OSInfo* new_info = new OSInfo();
    if (InterlockedCompareExchangePointer(reinterpret_cast<PVOID*>(&info),
                                          new_info, NULL)) {
      delete new_info;
    }
  }
  return info;
}

OSInfo::OSInfo()
    : version_(VERSION_PRE_XP),
      architecture_(OTHER_ARCHITECTURE),
      wow64_status_(GetWOW64StatusForProcess(GetCurrentProcess())) {
  OSVERSIONINFOEXW version_info = {sizeof version_info};

#pragma warning(push)
#pragma warning(disable : 4996)
  ::GetVersionExW(reinterpret_cast<OSVERSIONINFOW*>(&version_info));
#pragma warning(pop)
  version_number_.major = version_info.dwMajorVersion;
  version_number_.minor = version_info.dwMinorVersion;
  version_number_.build = version_info.dwBuildNumber;
  version_number_.patch = GetUBR();
  version_ = MajorMinorBuildToVersion(
      version_number_.major, version_number_.minor, version_number_.build);
  service_pack_.major = version_info.wServicePackMajor;
  service_pack_.minor = version_info.wServicePackMinor;
  service_pack_str_ = rtc::ToUtf8(version_info.szCSDVersion);

  SYSTEM_INFO system_info = {};
  ::GetNativeSystemInfo(&system_info);
  switch (system_info.wProcessorArchitecture) {
    case PROCESSOR_ARCHITECTURE_INTEL:
      architecture_ = X86_ARCHITECTURE;
      break;
    case PROCESSOR_ARCHITECTURE_AMD64:
      architecture_ = X64_ARCHITECTURE;
      break;
    case PROCESSOR_ARCHITECTURE_IA64:
      architecture_ = IA64_ARCHITECTURE;
      break;
  }
  processors_ = system_info.dwNumberOfProcessors;
  allocation_granularity_ = system_info.dwAllocationGranularity;

#if !defined(WINUWP)
  GetProductInfoPtr get_product_info;
  DWORD os_type;

  if (version_info.dwMajorVersion == 6 || version_info.dwMajorVersion == 10) {

    get_product_info = reinterpret_cast<GetProductInfoPtr>(::GetProcAddress(
        ::GetModuleHandleW(L"kernel32.dll"), "GetProductInfo"));

    get_product_info(version_info.dwMajorVersion, version_info.dwMinorVersion,
                     0, 0, &os_type);
    switch (os_type) {
      case PRODUCT_CLUSTER_SERVER:
      case PRODUCT_DATACENTER_SERVER:
      case PRODUCT_DATACENTER_SERVER_CORE:
      case PRODUCT_ENTERPRISE_SERVER:
      case PRODUCT_ENTERPRISE_SERVER_CORE:
      case PRODUCT_ENTERPRISE_SERVER_IA64:
      case PRODUCT_SMALLBUSINESS_SERVER:
      case PRODUCT_SMALLBUSINESS_SERVER_PREMIUM:
      case PRODUCT_STANDARD_SERVER:
      case PRODUCT_STANDARD_SERVER_CORE:
      case PRODUCT_WEB_SERVER:
        version_type_ = SUITE_SERVER;
        break;
      case PRODUCT_PROFESSIONAL:
      case PRODUCT_ULTIMATE:
        version_type_ = SUITE_PROFESSIONAL;
        break;
      case PRODUCT_ENTERPRISE:
      case PRODUCT_ENTERPRISE_E:
      case PRODUCT_ENTERPRISE_EVALUATION:
      case PRODUCT_ENTERPRISE_N:
      case PRODUCT_ENTERPRISE_N_EVALUATION:
      case PRODUCT_ENTERPRISE_S:
      case PRODUCT_ENTERPRISE_S_EVALUATION:
      case PRODUCT_ENTERPRISE_S_N:
      case PRODUCT_ENTERPRISE_S_N_EVALUATION:
      case PRODUCT_BUSINESS:
      case PRODUCT_BUSINESS_N:
        version_type_ = SUITE_ENTERPRISE;
        break;
      case PRODUCT_EDUCATION:
      case PRODUCT_EDUCATION_N:
        version_type_ = SUITE_EDUCATION;
        break;
      case PRODUCT_HOME_BASIC:
      case PRODUCT_HOME_PREMIUM:
      case PRODUCT_STARTER:
      default:
        version_type_ = SUITE_HOME;
        break;
    }
  } else if (version_info.dwMajorVersion == 5 &&
             version_info.dwMinorVersion == 2) {
    if (version_info.wProductType == VER_NT_WORKSTATION &&
        system_info.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_AMD64) {
      version_type_ = SUITE_PROFESSIONAL;
    } else if (version_info.wSuiteMask & VER_SUITE_WH_SERVER) {
      version_type_ = SUITE_HOME;
    } else {
      version_type_ = SUITE_SERVER;
    }
  } else if (version_info.dwMajorVersion == 5 &&
             version_info.dwMinorVersion == 1) {
    if (version_info.wSuiteMask & VER_SUITE_PERSONAL)
      version_type_ = SUITE_HOME;
    else
      version_type_ = SUITE_PROFESSIONAL;
  } else {

    version_type_ = SUITE_HOME;
  }
#else

  version_type_ = SUITE_HOME;
#endif
}

OSInfo::~OSInfo() {}

std::string OSInfo::processor_model_name() {
#if defined(WINUWP)

  return "Unknown Processor (UWP)";
#else
  if (processor_model_name_.empty()) {
    const wchar_t kProcessorNameString[] =
        L"HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0";
    RegKey key(HKEY_LOCAL_MACHINE, kProcessorNameString, KEY_READ);
    std::wstring value;
    key.ReadValue(L"ProcessorNameString", &value);
    processor_model_name_ = rtc::ToUtf8(value);
  }
  return processor_model_name_;
#endif
}

OSInfo::WOW64Status OSInfo::GetWOW64StatusForProcess(HANDLE process_handle) {
  BOOL is_wow64;
#if defined(WINUWP)
  if (!IsWow64Process(process_handle, &is_wow64))
    return WOW64_UNKNOWN;
#else
  typedef BOOL(WINAPI * IsWow64ProcessFunc)(HANDLE, PBOOL);
  IsWow64ProcessFunc is_wow64_process = reinterpret_cast<IsWow64ProcessFunc>(
      GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "IsWow64Process"));
  if (!is_wow64_process)
    return WOW64_DISABLED;
  if (!(*is_wow64_process)(process_handle, &is_wow64))
    return WOW64_UNKNOWN;
#endif
  return is_wow64 ? WOW64_ENABLED : WOW64_DISABLED;
}

Version GetVersion() {
  return OSInfo::GetInstance()->version();
}

}
}
