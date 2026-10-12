/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/desktop_capture/win/cursor.h"

#include <memory>

#include "modules/desktop_capture/desktop_frame.h"
#include "modules/desktop_capture/desktop_geometry.h"
#include "modules/desktop_capture/mouse_cursor.h"
#include "modules/desktop_capture/win/cursor_unittest_resources.h"
#include "modules/desktop_capture/win/scoped_gdi_object.h"
#include "test/gmock.h"

namespace webrtc {

namespace {

bool ConvertToMouseShapeAndCompare(unsigned left, unsigned right) {
  HMODULE instance = GetModuleHandle(NULL);

  win::ScopedCursor cursor(reinterpret_cast<HCURSOR>(
      LoadImage(instance, MAKEINTRESOURCE(left), IMAGE_CURSOR, 0, 0, 0)));
  EXPECT_TRUE(cursor != NULL);

  HDC dc = GetDC(NULL);
  std::unique_ptr<MouseCursor> mouse_shape(
      CreateMouseCursorFromHCursor(dc, cursor));
  ReleaseDC(NULL, dc);

  EXPECT_TRUE(mouse_shape.get());

  cursor.Set(reinterpret_cast<HCURSOR>(
      LoadImage(instance, MAKEINTRESOURCE(right), IMAGE_CURSOR, 0, 0, 0)));

  ICONINFO iinfo;
  EXPECT_TRUE(GetIconInfo(cursor, &iinfo));
  EXPECT_TRUE(iinfo.hbmColor);

  win::ScopedBitmap scoped_mask(iinfo.hbmMask);
  win::ScopedBitmap scoped_color(iinfo.hbmColor);

  BITMAP bitmap_info;
  EXPECT_TRUE(GetObject(scoped_color, sizeof(bitmap_info), &bitmap_info));

  int width = bitmap_info.bmWidth;
  int height = bitmap_info.bmHeight;
  EXPECT_TRUE(DesktopSize(width, height).equals(mouse_shape->image()->size()));

  int size = width * height;
  std::unique_ptr<uint32_t[]> data(new uint32_t[size]);
  EXPECT_TRUE(GetBitmapBits(scoped_color, size * sizeof(uint32_t), data.get()));

  return memcmp(data.get(), mouse_shape->image()->data(),
                size * sizeof(uint32_t)) == 0;
}

}

TEST(MouseCursorTest, MatchCursors) {
  EXPECT_TRUE(
      ConvertToMouseShapeAndCompare(IDD_CURSOR1_24BPP, IDD_CURSOR1_32BPP));

  EXPECT_TRUE(
      ConvertToMouseShapeAndCompare(IDD_CURSOR1_8BPP, IDD_CURSOR1_32BPP));

  EXPECT_TRUE(
      ConvertToMouseShapeAndCompare(IDD_CURSOR2_1BPP, IDD_CURSOR2_32BPP));

  EXPECT_TRUE(
      ConvertToMouseShapeAndCompare(IDD_CURSOR3_4BPP, IDD_CURSOR3_32BPP));
}

}
