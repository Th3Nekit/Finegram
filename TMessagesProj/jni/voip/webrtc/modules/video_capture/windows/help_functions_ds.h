/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_VIDEO_CAPTURE_MAIN_SOURCE_WINDOWS_HELP_FUNCTIONS_DS_H_
#define MODULES_VIDEO_CAPTURE_MAIN_SOURCE_WINDOWS_HELP_FUNCTIONS_DS_H_

#include <dshow.h>

#include <type_traits>
#include <utility>

#include "api/scoped_refptr.h"
#include "rtc_base/ref_counter.h"

DEFINE_GUID(MEDIASUBTYPE_I420,
            0x30323449,
            0x0000,
            0x0010,
            0x80,
            0x00,
            0x00,
            0xAA,
            0x00,
            0x38,
            0x9B,
            0x71);
DEFINE_GUID(MEDIASUBTYPE_HDYC,
            0x43594448,
            0x0000,
            0x0010,
            0x80,
            0x00,
            0x00,
            0xAA,
            0x00,
            0x38,
            0x9B,
            0x71);

#define RELEASE_AND_CLEAR(p) \
  if (p) {                   \
    (p)->Release();          \
    (p) = NULL;              \
  }

namespace webrtc {
namespace videocapturemodule {
LONGLONG GetMaxOfFrameArray(LONGLONG* maxFps, long size);

IPin* GetInputPin(IBaseFilter* filter);
IPin* GetOutputPin(IBaseFilter* filter, REFGUID Category);
BOOL PinMatchesCategory(IPin* pPin, REFGUID Category);
void ResetMediaType(AM_MEDIA_TYPE* media_type);
void FreeMediaType(AM_MEDIA_TYPE* media_type);
HRESULT CopyMediaType(AM_MEDIA_TYPE* target, const AM_MEDIA_TYPE* source);

template <class T>
HRESULT GetComInterface(IUnknown* object, rtc::scoped_refptr<T>* ptr) {

  RTC_DCHECK(!ptr->get());
  void* new_ptr = nullptr;
  HRESULT hr = object->QueryInterface(__uuidof(T), &new_ptr);
  if (SUCCEEDED(hr))
    ptr->swap(reinterpret_cast<T**>(&new_ptr));
  return hr;
}

template <class T>
class ComRefCount : public T {
 public:
  ComRefCount() {}

  template <class P0>
  explicit ComRefCount(P0&& p0) : T(std::forward<P0>(p0)) {}

  STDMETHOD_(ULONG, AddRef)() override {
    ref_count_.IncRef();
    return 1;
  }

  STDMETHOD_(ULONG, Release)() override {
    const auto status = ref_count_.DecRef();
    if (status == rtc::RefCountReleaseStatus::kDroppedLastRef) {
      delete this;
      return 0;
    }
    return 1;
  }

 protected:
  ~ComRefCount() {}

 private:
  webrtc::webrtc_impl::RefCounter ref_count_{0};
};

}
}
#endif
