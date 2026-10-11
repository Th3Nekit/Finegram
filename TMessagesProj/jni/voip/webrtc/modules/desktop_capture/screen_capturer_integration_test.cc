/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <string.h>

#include <algorithm>
#include <initializer_list>
#include <iostream>
#include <memory>
#include <utility>

#include "modules/desktop_capture/desktop_capture_options.h"
#include "modules/desktop_capture/desktop_capturer.h"
#include "modules/desktop_capture/desktop_frame.h"
#include "modules/desktop_capture/desktop_region.h"
#include "modules/desktop_capture/mock_desktop_capturer_callback.h"
#include "modules/desktop_capture/rgba_color.h"
#include "modules/desktop_capture/screen_drawer.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/third_party/base64/base64.h"
#include "test/gmock.h"
#include "test/gtest.h"

#if defined(WEBRTC_WIN)
#include "modules/desktop_capture/win/screen_capturer_win_directx.h"
#include "rtc_base/win/windows_version.h"
#endif

using ::testing::_;

namespace webrtc {

namespace {

ACTION_P2(SaveCaptureResult, result, dest) {
  *result = arg0;
  *dest = std::move(*arg1);
}

bool ArePixelsColoredBy(const DesktopFrame& frame,
                        DesktopRect rect,
                        RgbaColor color,
                        bool may_partially_draw) {
  if (!may_partially_draw) {

    DesktopRegion updated_region(frame.updated_region());
    updated_region.IntersectWith(rect);
    if (!updated_region.Equals(DesktopRegion(rect))) {
      return false;
    }
  }

  uint8_t* row = frame.GetFrameDataAtPos(rect.top_left());
  for (int i = 0; i < rect.height(); i++) {
    uint8_t* column = row;
    for (int j = 0; j < rect.width(); j++) {
      if (color != RgbaColor(column)) {
        return false;
      }
      column += DesktopFrame::kBytesPerPixel;
    }
    row += frame.stride();
  }
  return true;
}

}

class ScreenCapturerIntegrationTest : public ::testing::Test {
 public:
  void SetUp() override {
    capturer_ = DesktopCapturer::CreateScreenCapturer(
        DesktopCaptureOptions::CreateDefault());
  }

 protected:
  void TestCaptureUpdatedRegion(
      std::initializer_list<DesktopCapturer*> capturers) {
    RTC_DCHECK(capturers.size() > 0);

#if defined(WEBRTC_WIN)

    const int kTestArea = 416;
#else
    const int kTestArea = 512;
#endif
    const int kRectSize = 32;
    std::unique_ptr<ScreenDrawer> drawer = ScreenDrawer::Create();
    if (!drawer || drawer->DrawableRegion().is_empty()) {
      RTC_LOG(LS_WARNING)
          << "No ScreenDrawer implementation for current platform.";
      return;
    }
    if (drawer->DrawableRegion().width() < kTestArea ||
        drawer->DrawableRegion().height() < kTestArea) {
      RTC_LOG(LS_WARNING)
          << "ScreenDrawer::DrawableRegion() is too small for the "
             "CaptureUpdatedRegion tests.";
      return;
    }

    for (DesktopCapturer* capturer : capturers) {
      capturer->Start(&callback_);
    }

    for (int c = 0; c < 3; c++) {

      for (int i = 0; i < kTestArea - kRectSize; i += 16) {
        DesktopRect rect = DesktopRect::MakeXYWH(i, i, kRectSize, kRectSize);
        rect.Translate(drawer->DrawableRegion().top_left());
        RgbaColor color((c == 0 ? (i & 0xff) : 0x7f),
                        (c == 1 ? (i & 0xff) : 0x7f),
                        (c == 2 ? (i & 0xff) : 0x7f));

        ASSERT_NO_FATAL_FAILURE(
            TestCaptureOneFrame(capturers, drawer.get(), rect, color));
      }

      for (int i = 0; i < kTestArea - kRectSize; i += 16) {
        DesktopRect rect = DesktopRect::MakeXYWH(kRectSize, kRectSize, i, i);
        rect.Translate(drawer->DrawableRegion().top_left());
        RgbaColor color((c == 0 ? (i & 0xff) : 0x7f),
                        (c == 1 ? (i & 0xff) : 0x7f),
                        (c == 2 ? (i & 0xff) : 0x7f));

        ASSERT_NO_FATAL_FAILURE(
            TestCaptureOneFrame(capturers, drawer.get(), rect, color));
      }
    }
  }

  void TestCaptureUpdatedRegion() {
    TestCaptureUpdatedRegion({capturer_.get()});
  }

#if defined(WEBRTC_WIN)

  void MaybeCreateDirectxCapturer() {
    DesktopCaptureOptions options(DesktopCaptureOptions::CreateDefault());
    options.set_allow_directx_capturer(true);
    capturer_ = DesktopCapturer::CreateScreenCapturer(options);
  }

  bool CreateDirectxCapturer() {
    if (!ScreenCapturerWinDirectx::IsSupported()) {
      RTC_LOG(LS_WARNING) << "Directx capturer is not supported";
      return false;
    }

    MaybeCreateDirectxCapturer();
    return true;
  }
#endif

  std::unique_ptr<DesktopCapturer> capturer_;
  MockDesktopCapturerCallback callback_;

 private:

  void TestCaptureOneFrame(std::vector<DesktopCapturer*> capturers,
                           ScreenDrawer* drawer,
                           DesktopRect rect,
                           RgbaColor color) {
    const int wait_capture_round = 600;
    drawer->Clear();
    size_t succeeded_capturers = 0;
    for (int i = 0; i < wait_capture_round; i++) {
      drawer->DrawRectangle(rect, color);
      drawer->WaitForPendingDraws();
      for (size_t j = 0; j < capturers.size(); j++) {
        if (capturers[j] == nullptr) {

          continue;
        }
        std::unique_ptr<DesktopFrame> frame = CaptureFrame(capturers[j]);
        if (!frame) {

          return;
        }

        if (ArePixelsColoredBy(*frame, rect, color,
                               drawer->MayDrawIncompleteShapes())) {
          capturers[j] = nullptr;
          succeeded_capturers++;
        }

        else if (i == wait_capture_round - 1) {
          std::string result;
          rtc::Base64::EncodeFromArray(
              frame->data(), frame->size().height() * frame->stride(), &result);
          std::cout << frame->size().width() << " x " << frame->size().height()
                    << std::endl;

          static const size_t kLineLength = 32768;
          const char* result_end = result.c_str() + result.length();
          for (const char* it = result.c_str(); it < result_end;
               it += kLineLength) {
            const size_t max_length = result_end - it;
            std::cout << std::string(it, std::min(kLineLength, max_length))
                      << std::endl;
          }
          std::cout << "Failed to capture rectangle " << rect.left() << " x "
                    << rect.top() << " - " << rect.right() << " x "
                    << rect.bottom() << " with color ("
                    << static_cast<int>(color.red) << ", "
                    << static_cast<int>(color.green) << ", "
                    << static_cast<int>(color.blue) << ", "
                    << static_cast<int>(color.alpha) << ")" << std::endl;
          ASSERT_TRUE(false) << "ScreenCapturerIntegrationTest may be flaky. "
                                "Please kindly FYI the broken link to "
                                "zijiehe@chromium.org for investigation. If "
                                "the failure continually happens, but I have "
                                "not responded as quick as expected, disable "
                                "*all* tests in "
                                "screen_capturer_integration_test.cc to "
                                "unblock other developers.";
        }
      }

      if (succeeded_capturers == capturers.size()) {
        break;
      }
    }

    ASSERT_EQ(succeeded_capturers, capturers.size());
  }

  std::unique_ptr<DesktopFrame> CaptureFrame(DesktopCapturer* capturer) {
    for (int i = 0; i < 10; i++) {
      std::unique_ptr<DesktopFrame> frame;
      DesktopCapturer::Result result;
      EXPECT_CALL(callback_, OnCaptureResultPtr(_, _))
          .WillOnce(SaveCaptureResult(&result, &frame));
      capturer->CaptureFrame();
      ::testing::Mock::VerifyAndClearExpectations(&callback_);
      if (result == DesktopCapturer::Result::SUCCESS) {
        EXPECT_TRUE(frame);
        return frame;
      } else {
        EXPECT_FALSE(frame);
      }
    }

    EXPECT_TRUE(false);
    return nullptr;
  }
};

#if defined(WEBRTC_WIN)

#define MAYBE_CaptureUpdatedRegion DISABLED_CaptureUpdatedRegion
#else
#define MAYBE_CaptureUpdatedRegion CaptureUpdatedRegion
#endif
TEST_F(ScreenCapturerIntegrationTest, MAYBE_CaptureUpdatedRegion) {
  TestCaptureUpdatedRegion();
}

#if defined(WEBRTC_WIN)

#define MAYBE_TwoCapturers DISABLED_TwoCapturers
#else
#define MAYBE_TwoCapturers TwoCapturers
#endif
TEST_F(ScreenCapturerIntegrationTest, MAYBE_TwoCapturers) {
  std::unique_ptr<DesktopCapturer> capturer2 = std::move(capturer_);
  SetUp();
  TestCaptureUpdatedRegion({capturer_.get(), capturer2.get()});
}

#if defined(WEBRTC_WIN)

TEST_F(ScreenCapturerIntegrationTest,
       DISABLED_CaptureUpdatedRegionWithDirectxCapturer) {
  if (!CreateDirectxCapturer()) {
    return;
  }

  TestCaptureUpdatedRegion();
}

TEST_F(ScreenCapturerIntegrationTest, DISABLED_TwoDirectxCapturers) {
  if (!CreateDirectxCapturer()) {
    return;
  }

  std::unique_ptr<DesktopCapturer> capturer2 = std::move(capturer_);
  RTC_CHECK(CreateDirectxCapturer());
  TestCaptureUpdatedRegion({capturer_.get(), capturer2.get()});
}

TEST_F(ScreenCapturerIntegrationTest,
       DISABLED_MaybeCaptureUpdatedRegionWithDirectxCapturer) {
  if (rtc::rtc_win::GetVersion() < rtc::rtc_win::Version::VERSION_WIN8) {

    return;
  }

  MaybeCreateDirectxCapturer();
  TestCaptureUpdatedRegion();
}

#endif

}
