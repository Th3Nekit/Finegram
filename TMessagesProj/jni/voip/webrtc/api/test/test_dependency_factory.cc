/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/test/test_dependency_factory.h"

#include <memory>
#include <utility>

#include "rtc_base/checks.h"
#include "rtc_base/platform_thread_types.h"

namespace webrtc {

namespace {

bool IsValidTestDependencyFactoryThread() {
  const rtc::PlatformThreadRef main_thread = rtc::CurrentThreadRef();
  return rtc::IsThreadRefEqual(main_thread, rtc::CurrentThreadRef());
}
}

std::unique_ptr<TestDependencyFactory> TestDependencyFactory::instance_ =
    nullptr;

const TestDependencyFactory& TestDependencyFactory::GetInstance() {
  RTC_DCHECK(IsValidTestDependencyFactoryThread());
  if (instance_ == nullptr) {
    instance_ = std::make_unique<TestDependencyFactory>();
  }
  return *instance_;
}

void TestDependencyFactory::SetInstance(
    std::unique_ptr<TestDependencyFactory> instance) {
  RTC_DCHECK(IsValidTestDependencyFactoryThread());
  RTC_CHECK(instance_ == nullptr);
  instance_ = std::move(instance);
}

std::unique_ptr<VideoQualityTestFixtureInterface::InjectionComponents>
TestDependencyFactory::CreateComponents() const {
  RTC_DCHECK(IsValidTestDependencyFactoryThread());
  return nullptr;
}

}
