/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/desktop_capture/desktop_capturer_differ_wrapper.h"

#include <stdint.h>
#include <string.h>

#include <utility>

#include "modules/desktop_capture/desktop_geometry.h"
#include "modules/desktop_capture/desktop_region.h"
#include "modules/desktop_capture/differ_block.h"
#include "rtc_base/checks.h"
#include "rtc_base/time_utils.h"

namespace webrtc {

namespace {

bool PartialBlockDifference(const uint8_t* old_buffer,
                            const uint8_t* new_buffer,
                            int width,
                            int height,
                            int stride) {
  RTC_DCHECK_LT(width, kBlockSize);
  const int width_bytes = width * DesktopFrame::kBytesPerPixel;
  for (int i = 0; i < height; i++) {
    if (memcmp(old_buffer, new_buffer, width_bytes) != 0) {
      return true;
    }
    old_buffer += stride;
    new_buffer += stride;
  }
  return false;
}

void CompareRow(const uint8_t* old_buffer,
                const uint8_t* new_buffer,
                const int left,
                const int right,
                const int top,
                const int bottom,
                const int stride,
                DesktopRegion* const output) {
  const int block_x_offset = kBlockSize * DesktopFrame::kBytesPerPixel;
  const int width = right - left;
  const int height = bottom - top;
  const int block_count = (width - 1) / kBlockSize;
  const int last_block_width = width - block_count * kBlockSize;
  RTC_DCHECK_GT(last_block_width, 0);
  RTC_DCHECK_LE(last_block_width, kBlockSize);

  int first_dirty_x_block = -1;

  for (int x = 0; x < block_count; x++) {
    if (BlockDifference(old_buffer, new_buffer, height, stride)) {
      if (first_dirty_x_block == -1) {

        first_dirty_x_block = x;
      }
    } else if (first_dirty_x_block != -1) {

      output->AddRect(
          DesktopRect::MakeLTRB(first_dirty_x_block * kBlockSize + left, top,
                                x * kBlockSize + left, bottom));
      first_dirty_x_block = -1;
    }
    old_buffer += block_x_offset;
    new_buffer += block_x_offset;
  }

  bool last_block_diff;
  if (last_block_width < kBlockSize) {

    last_block_diff = PartialBlockDifference(old_buffer, new_buffer,
                                             last_block_width, height, stride);
  } else {
    last_block_diff = BlockDifference(old_buffer, new_buffer, height, stride);
  }
  if (last_block_diff) {
    if (first_dirty_x_block == -1) {
      first_dirty_x_block = block_count;
    }
    output->AddRect(DesktopRect::MakeLTRB(
        first_dirty_x_block * kBlockSize + left, top, right, bottom));
  } else if (first_dirty_x_block != -1) {
    output->AddRect(
        DesktopRect::MakeLTRB(first_dirty_x_block * kBlockSize + left, top,
                              block_count * kBlockSize + left, bottom));
  }
}

void CompareFrames(const DesktopFrame& old_frame,
                   const DesktopFrame& new_frame,
                   DesktopRect rect,
                   DesktopRegion* const output) {
  RTC_DCHECK(old_frame.size().equals(new_frame.size()));
  RTC_DCHECK_EQ(old_frame.stride(), new_frame.stride());
  rect.IntersectWith(DesktopRect::MakeSize(old_frame.size()));

  const int y_block_count = (rect.height() - 1) / kBlockSize;
  const int last_y_block_height = rect.height() - y_block_count * kBlockSize;

  const int block_y_stride = old_frame.stride() * kBlockSize;
  const uint8_t* prev_block_row_start =
      old_frame.GetFrameDataAtPos(rect.top_left());
  const uint8_t* curr_block_row_start =
      new_frame.GetFrameDataAtPos(rect.top_left());

  int top = rect.top();

  for (int y = 0; y < y_block_count; y++) {
    CompareRow(prev_block_row_start, curr_block_row_start, rect.left(),
               rect.right(), top, top + kBlockSize, old_frame.stride(), output);
    top += kBlockSize;
    prev_block_row_start += block_y_stride;
    curr_block_row_start += block_y_stride;
  }
  CompareRow(prev_block_row_start, curr_block_row_start, rect.left(),
             rect.right(), top, top + last_y_block_height, old_frame.stride(),
             output);
}

}

DesktopCapturerDifferWrapper::DesktopCapturerDifferWrapper(
    std::unique_ptr<DesktopCapturer> base_capturer)
    : base_capturer_(std::move(base_capturer)) {
  RTC_DCHECK(base_capturer_);
}

DesktopCapturerDifferWrapper::~DesktopCapturerDifferWrapper() {}

void DesktopCapturerDifferWrapper::Start(DesktopCapturer::Callback* callback) {
  callback_ = callback;
  base_capturer_->Start(this);
}

void DesktopCapturerDifferWrapper::SetSharedMemoryFactory(
    std::unique_ptr<SharedMemoryFactory> shared_memory_factory) {
  base_capturer_->SetSharedMemoryFactory(std::move(shared_memory_factory));
}

void DesktopCapturerDifferWrapper::CaptureFrame() {
  base_capturer_->CaptureFrame();
}

void DesktopCapturerDifferWrapper::SetExcludedWindow(WindowId window) {
  base_capturer_->SetExcludedWindow(window);
}

bool DesktopCapturerDifferWrapper::GetSourceList(SourceList* sources) {
  return base_capturer_->GetSourceList(sources);
}

bool DesktopCapturerDifferWrapper::SelectSource(SourceId id) {
  return base_capturer_->SelectSource(id);
}

bool DesktopCapturerDifferWrapper::FocusOnSelectedSource() {
  return base_capturer_->FocusOnSelectedSource();
}

bool DesktopCapturerDifferWrapper::IsOccluded(const DesktopVector& pos) {
  return base_capturer_->IsOccluded(pos);
}

#if defined(WEBRTC_USE_GIO)
DesktopCaptureMetadata DesktopCapturerDifferWrapper::GetMetadata() {
  return base_capturer_->GetMetadata();
}
#endif

void DesktopCapturerDifferWrapper::OnCaptureResult(
    Result result,
    std::unique_ptr<DesktopFrame> input_frame) {
  int64_t start_time_nanos = rtc::TimeNanos();
  if (!input_frame) {
    callback_->OnCaptureResult(result, nullptr);
    return;
  }
  RTC_DCHECK(result == Result::SUCCESS);

  std::unique_ptr<SharedDesktopFrame> frame =
      SharedDesktopFrame::Wrap(std::move(input_frame));
  if (last_frame_ && (last_frame_->size().width() != frame->size().width() ||
                      last_frame_->size().height() != frame->size().height() ||
                      last_frame_->stride() != frame->stride())) {
    last_frame_.reset();
  }

  if (last_frame_) {
    DesktopRegion hints;
    hints.Swap(frame->mutable_updated_region());
    for (DesktopRegion::Iterator it(hints); !it.IsAtEnd(); it.Advance()) {
      CompareFrames(*last_frame_, *frame, it.rect(),
                    frame->mutable_updated_region());
    }
  } else {
    frame->mutable_updated_region()->SetRect(
        DesktopRect::MakeSize(frame->size()));
  }
  last_frame_ = frame->Share();

  frame->set_capture_time_ms(frame->capture_time_ms() +
                             (rtc::TimeNanos() - start_time_nanos) /
                                 rtc::kNumNanosecsPerMillisec);
  callback_->OnCaptureResult(result, std::move(frame));
}

}
