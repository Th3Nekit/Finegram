/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_DESKTOP_CAPTURE_SHARED_MEMORY_H_
#define MODULES_DESKTOP_CAPTURE_SHARED_MEMORY_H_

#include <stddef.h>

#if defined(WEBRTC_WIN)

typedef void* HANDLE;
#endif

#include <memory>

#include "rtc_base/system/rtc_export.h"

namespace webrtc {

class RTC_EXPORT SharedMemory {
 public:
#if defined(WEBRTC_WIN)
  typedef HANDLE Handle;
  static const Handle kInvalidHandle;
#else
  typedef int Handle;
  static const Handle kInvalidHandle;
#endif

  void* data() const { return data_; }
  size_t size() const { return size_; }

  Handle handle() const { return handle_; }

  int id() const { return id_; }

  virtual ~SharedMemory() {}

  SharedMemory(const SharedMemory&) = delete;
  SharedMemory& operator=(const SharedMemory&) = delete;

 protected:
  SharedMemory(void* data, size_t size, Handle handle, int id);

  void* const data_;
  const size_t size_;
  const Handle handle_;
  const int id_;
};

class SharedMemoryFactory {
 public:
  SharedMemoryFactory() {}
  virtual ~SharedMemoryFactory() {}

  SharedMemoryFactory(const SharedMemoryFactory&) = delete;
  SharedMemoryFactory& operator=(const SharedMemoryFactory&) = delete;

  virtual std::unique_ptr<SharedMemory> CreateSharedMemory(size_t size) = 0;
};

}

#endif
