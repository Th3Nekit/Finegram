/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/desktop_capture/fallback_desktop_capturer_wrapper.h"

#include <stddef.h>

#include <utility>

#include "api/sequence_checker.h"
#include "rtc_base/checks.h"
#include "system_wrappers/include/metrics.h"

namespace webrtc {

namespace {

class SharedMemoryFactoryProxy : public SharedMemoryFactory {
 public:

  static std::unique_ptr<SharedMemoryFactory> Create(
      SharedMemoryFactory* factory);
  ~SharedMemoryFactoryProxy() override;

  std::unique_ptr<SharedMemory> CreateSharedMemory(size_t size) override;

 private:
  explicit SharedMemoryFactoryProxy(SharedMemoryFactory* factory);

  SharedMemoryFactory* factory_ = nullptr;
  SequenceChecker thread_checker_;
};

}

SharedMemoryFactoryProxy::SharedMemoryFactoryProxy(
    SharedMemoryFactory* factory) {
  RTC_DCHECK(factory);
  factory_ = factory;
}

std::unique_ptr<SharedMemoryFactory> SharedMemoryFactoryProxy::Create(
    SharedMemoryFactory* factory) {
  return std::unique_ptr<SharedMemoryFactory>(
      new SharedMemoryFactoryProxy(factory));
}

SharedMemoryFactoryProxy::~SharedMemoryFactoryProxy() = default;

std::unique_ptr<SharedMemory> SharedMemoryFactoryProxy::CreateSharedMemory(
    size_t size) {
  RTC_DCHECK(thread_checker_.IsCurrent());
  return factory_->CreateSharedMemory(size);
}

FallbackDesktopCapturerWrapper::FallbackDesktopCapturerWrapper(
    std::unique_ptr<DesktopCapturer> main_capturer,
    std::unique_ptr<DesktopCapturer> secondary_capturer)
    : main_capturer_(std::move(main_capturer)),
      secondary_capturer_(std::move(secondary_capturer)) {
  RTC_DCHECK(main_capturer_);
  RTC_DCHECK(secondary_capturer_);
}

FallbackDesktopCapturerWrapper::~FallbackDesktopCapturerWrapper() = default;

void FallbackDesktopCapturerWrapper::Start(
    DesktopCapturer::Callback* callback) {
  callback_ = callback;

  main_capturer_->Start(this);

  secondary_capturer_->Start(callback);
}

void FallbackDesktopCapturerWrapper::SetSharedMemoryFactory(
    std::unique_ptr<SharedMemoryFactory> shared_memory_factory) {
  shared_memory_factory_ = std::move(shared_memory_factory);
  if (shared_memory_factory_) {
    main_capturer_->SetSharedMemoryFactory(
        SharedMemoryFactoryProxy::Create(shared_memory_factory_.get()));
    secondary_capturer_->SetSharedMemoryFactory(
        SharedMemoryFactoryProxy::Create(shared_memory_factory_.get()));
  } else {
    main_capturer_->SetSharedMemoryFactory(
        std::unique_ptr<SharedMemoryFactory>());
    secondary_capturer_->SetSharedMemoryFactory(
        std::unique_ptr<SharedMemoryFactory>());
  }
}

void FallbackDesktopCapturerWrapper::CaptureFrame() {
  RTC_DCHECK(callback_);
  if (main_capturer_permanent_error_) {
    secondary_capturer_->CaptureFrame();
  } else {
    main_capturer_->CaptureFrame();
  }
}

void FallbackDesktopCapturerWrapper::SetExcludedWindow(WindowId window) {
  main_capturer_->SetExcludedWindow(window);
  secondary_capturer_->SetExcludedWindow(window);
}

bool FallbackDesktopCapturerWrapper::GetSourceList(SourceList* sources) {
  if (main_capturer_permanent_error_) {
    return secondary_capturer_->GetSourceList(sources);
  }
  return main_capturer_->GetSourceList(sources);
}

bool FallbackDesktopCapturerWrapper::SelectSource(SourceId id) {
  if (main_capturer_permanent_error_) {
    return secondary_capturer_->SelectSource(id);
  }
  const bool main_capturer_result = main_capturer_->SelectSource(id);
  RTC_HISTOGRAM_BOOLEAN(
      "WebRTC.DesktopCapture.PrimaryCapturerSelectSourceError",
      main_capturer_result);
  if (!main_capturer_result) {
    main_capturer_permanent_error_ = true;
  }

  return secondary_capturer_->SelectSource(id);
}

bool FallbackDesktopCapturerWrapper::FocusOnSelectedSource() {
  if (main_capturer_permanent_error_) {
    return secondary_capturer_->FocusOnSelectedSource();
  }
  return main_capturer_->FocusOnSelectedSource() ||
         secondary_capturer_->FocusOnSelectedSource();
}

bool FallbackDesktopCapturerWrapper::IsOccluded(const DesktopVector& pos) {

  if (main_capturer_permanent_error_) {
    return secondary_capturer_->IsOccluded(pos);
  }
  return main_capturer_->IsOccluded(pos) ||
         secondary_capturer_->IsOccluded(pos);
}

void FallbackDesktopCapturerWrapper::OnCaptureResult(
    Result result,
    std::unique_ptr<DesktopFrame> frame) {
  RTC_DCHECK(callback_);
  RTC_HISTOGRAM_BOOLEAN("WebRTC.DesktopCapture.PrimaryCapturerError",
                        result != Result::SUCCESS);
  RTC_HISTOGRAM_BOOLEAN("WebRTC.DesktopCapture.PrimaryCapturerPermanentError",
                        result == Result::ERROR_PERMANENT);
  if (result == Result::SUCCESS) {
    callback_->OnCaptureResult(result, std::move(frame));
    return;
  }

  if (result == Result::ERROR_PERMANENT) {
    main_capturer_permanent_error_ = true;
  }
  secondary_capturer_->CaptureFrame();
}

}
