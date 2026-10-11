/*
 *  Copyright (c) 2010 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef AUDIO_DEVICE_LATEBINDINGSYMBOLTABLE_LINUX_H_
#define AUDIO_DEVICE_LATEBINDINGSYMBOLTABLE_LINUX_H_

#include <stddef.h>
#include <string.h>

#include "absl/strings/string_view.h"
#include "rtc_base/checks.h"

namespace webrtc {
namespace adm_linux {

#ifdef WEBRTC_LINUX
typedef void* DllHandle;

const DllHandle kInvalidDllHandle = NULL;
#else
#error Not implemented
#endif

DllHandle InternalLoadDll(absl::string_view);

void InternalUnloadDll(DllHandle handle);

bool InternalLoadSymbols(DllHandle handle,
                         int num_symbols,
                         const char* const symbol_names[],
                         void* symbols[]);

template <int SYMBOL_TABLE_SIZE,
          const char kDllName[],
          const char* const kSymbolNames[]>
class LateBindingSymbolTable {
 public:
  LateBindingSymbolTable()
      : handle_(kInvalidDllHandle), undefined_symbols_(false) {
    memset(symbols_, 0, sizeof(symbols_));
  }

  ~LateBindingSymbolTable() { Unload(); }

  LateBindingSymbolTable(const LateBindingSymbolTable&) = delete;
  LateBindingSymbolTable& operator=(LateBindingSymbolTable&) = delete;

  static int NumSymbols() { return SYMBOL_TABLE_SIZE; }

  static const char* GetSymbolName(int index) {
    RTC_DCHECK_LT(index, NumSymbols());
    return kSymbolNames[index];
  }

  bool IsLoaded() const { return handle_ != kInvalidDllHandle; }

  bool Load() {
    if (IsLoaded()) {
      return true;
    }
    if (undefined_symbols_) {

      return false;
    }
    handle_ = InternalLoadDll(kDllName);
    if (!IsLoaded()) {
      return false;
    }
    if (!InternalLoadSymbols(handle_, NumSymbols(), kSymbolNames, symbols_)) {
      undefined_symbols_ = true;
      Unload();
      return false;
    }
    return true;
  }

  void Unload() {
    if (!IsLoaded()) {
      return;
    }
    InternalUnloadDll(handle_);
    handle_ = kInvalidDllHandle;
    memset(symbols_, 0, sizeof(symbols_));
  }

  void* GetSymbol(int index) const {
    RTC_DCHECK(IsLoaded());
    RTC_DCHECK_LT(index, NumSymbols());
    return symbols_[index];
  }

 private:
  DllHandle handle_;
  bool undefined_symbols_;
  void* symbols_[SYMBOL_TABLE_SIZE];
};

#define LATE_BINDING_SYMBOL_TABLE_DECLARE_BEGIN(ClassName) enum {

#define LATE_BINDING_SYMBOL_TABLE_DECLARE_ENTRY(ClassName, sym) \
  ClassName##_SYMBOL_TABLE_INDEX_##sym,

#define LATE_BINDING_SYMBOL_TABLE_DECLARE_END(ClassName)       \
  ClassName##_SYMBOL_TABLE_SIZE                                \
  }                                                            \
  ;                                                            \
                                                               \
  extern const char ClassName##_kDllName[];                    \
  extern const char* const                                     \
      ClassName##_kSymbolNames[ClassName##_SYMBOL_TABLE_SIZE]; \
                                                               \
  typedef ::webrtc::adm_linux::LateBindingSymbolTable<         \
      ClassName##_SYMBOL_TABLE_SIZE, ClassName##_kDllName,     \
      ClassName##_kSymbolNames>                                \
      ClassName;

#define LATE_BINDING_SYMBOL_TABLE_DEFINE_BEGIN(ClassName, dllName) \
  const char ClassName##_kDllName[] = dllName;                     \
  const char* const ClassName##_kSymbolNames[ClassName##_SYMBOL_TABLE_SIZE] = {

#define LATE_BINDING_SYMBOL_TABLE_DEFINE_ENTRY(ClassName, sym) #sym,

#define LATE_BINDING_SYMBOL_TABLE_DEFINE_END(ClassName) \
  }                                                     \
  ;

#define LATESYM_INDEXOF(ClassName, sym) (ClassName##_SYMBOL_TABLE_INDEX_##sym)

#define LATESYM_GET(ClassName, inst, sym) \
  (*reinterpret_cast<__typeof__(&sym)>(   \
      (inst)->GetSymbol(LATESYM_INDEXOF(ClassName, sym))))

}
}

#endif
