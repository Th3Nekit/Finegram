/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_TEST_TEST_DEPENDENCY_FACTORY_H_
#define API_TEST_TEST_DEPENDENCY_FACTORY_H_

#include <memory>

#include "api/test/video_quality_test_fixture.h"

namespace webrtc {

class TestDependencyFactory {
 public:
  virtual ~TestDependencyFactory() = default;

  static const TestDependencyFactory& GetInstance();
  static void SetInstance(std::unique_ptr<TestDependencyFactory> instance);

  virtual std::unique_ptr<VideoQualityTestFixtureInterface::InjectionComponents>
  CreateComponents() const;

 private:
  static std::unique_ptr<TestDependencyFactory> instance_;
};

}

#endif
