/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_STREAM_H_
#define RTC_BASE_STREAM_H_

#include <memory>

#include "api/array_view.h"
#include "rtc_base/buffer.h"
#include "rtc_base/system/rtc_export.h"
#include "rtc_base/third_party/sigslot/sigslot.h"
#include "rtc_base/thread.h"

namespace rtc {

enum StreamState { SS_CLOSED, SS_OPENING, SS_OPEN };

enum StreamResult { SR_ERROR, SR_SUCCESS, SR_BLOCK, SR_EOS };

enum StreamEvent { SE_OPEN = 1, SE_READ = 2, SE_WRITE = 4, SE_CLOSE = 8 };

class RTC_EXPORT StreamInterface {
 public:
  virtual ~StreamInterface() {}

  StreamInterface(const StreamInterface&) = delete;
  StreamInterface& operator=(const StreamInterface&) = delete;

  virtual StreamState GetState() const = 0;

  virtual StreamResult Read(rtc::ArrayView<uint8_t> buffer,
                            size_t& read,
                            int& error) = 0;
  virtual StreamResult Write(rtc::ArrayView<const uint8_t> data,
                             size_t& written,
                             int& error) = 0;

  virtual void Close() = 0;

  sigslot::signal3<StreamInterface*, int, int> SignalEvent;

  virtual bool Flush();

  [[deprecated("Use version with ArrayView")]] StreamResult
  WriteAll(const void* data, size_t data_len, size_t* written, int* error);

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"

  StreamResult WriteAll(ArrayView<const uint8_t> data,
                        size_t& written,
                        int& error) {
    return WriteAll(data.data(), data.size(), &written, &error);
  }
#pragma clang diagnostic pop

 protected:
  StreamInterface();
};

}

#endif
