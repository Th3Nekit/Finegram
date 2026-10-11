/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_VIDEO_CAPTURE_MAIN_SOURCE_WINDOWS_SINK_FILTER_DS_H_
#define MODULES_VIDEO_CAPTURE_MAIN_SOURCE_WINDOWS_SINK_FILTER_DS_H_

#include <dshow.h>

#include <atomic>
#include <memory>
#include <vector>

#include "api/sequence_checker.h"
#include "modules/video_capture/video_capture_impl.h"
#include "modules/video_capture/windows/help_functions_ds.h"
#include "rtc_base/thread_annotations.h"

namespace webrtc {
namespace videocapturemodule {

class CaptureSinkFilter;

class CaptureInputPin : public IMemInputPin, public IPin {
 public:
  CaptureInputPin(CaptureSinkFilter* filter);

  HRESULT SetRequestedCapability(const VideoCaptureCapability& capability);

  void OnFilterActivated();
  void OnFilterDeactivated();

 protected:
  virtual ~CaptureInputPin();

 private:
  CaptureSinkFilter* Filter() const;

  HRESULT AttemptConnection(IPin* receive_pin, const AM_MEDIA_TYPE* media_type);
  std::vector<AM_MEDIA_TYPE*> DetermineCandidateFormats(
      IPin* receive_pin,
      const AM_MEDIA_TYPE* media_type);
  void ClearAllocator(bool decommit);
  HRESULT CheckDirection(IPin* pin) const;

  STDMETHOD(QueryInterface)(REFIID riid, void** ppv) override;

  STDMETHOD(Connect)(IPin* receive_pin,
                     const AM_MEDIA_TYPE* media_type) override;
  STDMETHOD(ReceiveConnection)(IPin* connector,
                               const AM_MEDIA_TYPE* media_type) override;
  STDMETHOD(Disconnect)() override;
  STDMETHOD(ConnectedTo)(IPin** pin) override;
  STDMETHOD(ConnectionMediaType)(AM_MEDIA_TYPE* media_type) override;
  STDMETHOD(QueryPinInfo)(PIN_INFO* info) override;
  STDMETHOD(QueryDirection)(PIN_DIRECTION* pin_dir) override;
  STDMETHOD(QueryId)(LPWSTR* id) override;
  STDMETHOD(QueryAccept)(const AM_MEDIA_TYPE* media_type) override;
  STDMETHOD(EnumMediaTypes)(IEnumMediaTypes** types) override;
  STDMETHOD(QueryInternalConnections)(IPin** pins, ULONG* count) override;
  STDMETHOD(EndOfStream)() override;
  STDMETHOD(BeginFlush)() override;
  STDMETHOD(EndFlush)() override;
  STDMETHOD(NewSegment)(REFERENCE_TIME start, REFERENCE_TIME stop,
                        double rate) override;

  STDMETHOD(GetAllocator)(IMemAllocator** allocator) override;
  STDMETHOD(NotifyAllocator)(IMemAllocator* allocator, BOOL read_only) override;
  STDMETHOD(GetAllocatorRequirements)(ALLOCATOR_PROPERTIES* props) override;
  STDMETHOD(Receive)(IMediaSample* sample) override;
  STDMETHOD(ReceiveMultiple)(IMediaSample** samples, long count,
                             long* processed) override;
  STDMETHOD(ReceiveCanBlock)() override;

  SequenceChecker main_checker_;
  SequenceChecker capture_checker_;

  VideoCaptureCapability requested_capability_ RTC_GUARDED_BY(main_checker_);

  VideoCaptureCapability resulting_capability_;
  DWORD capture_thread_id_ = 0;
  rtc::scoped_refptr<IMemAllocator> allocator_ RTC_GUARDED_BY(main_checker_);
  rtc::scoped_refptr<IPin> receive_pin_ RTC_GUARDED_BY(main_checker_);
  std::atomic_bool flushing_{false};
  std::atomic_bool runtime_error_{false};

  PIN_INFO info_ = {};
  AM_MEDIA_TYPE media_type_ RTC_GUARDED_BY(main_checker_) = {};
};

class CaptureSinkFilter : public IBaseFilter {
 public:
  CaptureSinkFilter(VideoCaptureImpl* capture_observer);

  HRESULT SetRequestedCapability(const VideoCaptureCapability& capability);

  void ProcessCapturedFrame(unsigned char* buffer,
                            size_t length,
                            const VideoCaptureCapability& frame_info);

  void NotifyEvent(long code, LONG_PTR param1, LONG_PTR param2);
  bool IsStopped() const;

  STDMETHOD(QueryInterface)(REFIID riid, void** ppv) override;

  STDMETHOD(GetClassID)(CLSID* clsid) override;

  STDMETHOD(GetState)(DWORD msecs, FILTER_STATE* state) override;
  STDMETHOD(SetSyncSource)(IReferenceClock* clock) override;
  STDMETHOD(GetSyncSource)(IReferenceClock** clock) override;
  STDMETHOD(Pause)() override;
  STDMETHOD(Run)(REFERENCE_TIME start) override;
  STDMETHOD(Stop)() override;

  STDMETHOD(EnumPins)(IEnumPins** pins) override;
  STDMETHOD(FindPin)(LPCWSTR id, IPin** pin) override;
  STDMETHOD(QueryFilterInfo)(FILTER_INFO* info) override;
  STDMETHOD(JoinFilterGraph)(IFilterGraph* graph, LPCWSTR name) override;
  STDMETHOD(QueryVendorInfo)(LPWSTR* vendor_info) override;

 protected:
  virtual ~CaptureSinkFilter();

 private:
  SequenceChecker main_checker_;
  const rtc::scoped_refptr<ComRefCount<CaptureInputPin>> input_pin_;
  VideoCaptureImpl* const capture_observer_;
  FILTER_INFO info_ RTC_GUARDED_BY(main_checker_) = {};

  IMediaEventSink* sink_ = nullptr;
  FILTER_STATE state_ RTC_GUARDED_BY(main_checker_) = State_Stopped;
};
}
}
#endif
