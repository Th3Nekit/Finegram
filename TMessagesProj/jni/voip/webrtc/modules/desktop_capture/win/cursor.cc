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

#include <algorithm>
#include <memory>

#include "modules/desktop_capture/desktop_frame.h"
#include "modules/desktop_capture/desktop_geometry.h"
#include "modules/desktop_capture/mouse_cursor.h"
#include "modules/desktop_capture/win/scoped_gdi_object.h"
#include "rtc_base/logging.h"
#include "rtc_base/system/arch.h"

namespace webrtc {

namespace {

#if defined(WEBRTC_ARCH_LITTLE_ENDIAN)

#define RGBA(r, g, b, a)                                   \
  ((((a) << 24) & 0xff000000) | (((b) << 16) & 0xff0000) | \
   (((g) << 8) & 0xff00) | ((r)&0xff))

#else

#define RGBA(r, g, b, a)                                   \
  ((((r) << 24) & 0xff000000) | (((g) << 16) & 0xff0000) | \
   (((b) << 8) & 0xff00) | ((a)&0xff))

#endif

const int kBytesPerPixel = DesktopFrame::kBytesPerPixel;

const uint32_t kPixelRgbaBlack = RGBA(0, 0, 0, 0xff);
const uint32_t kPixelRgbaWhite = RGBA(0xff, 0xff, 0xff, 0xff);
const uint32_t kPixelRgbaTransparent = RGBA(0, 0, 0, 0);

const uint32_t kPixelRgbWhite = RGB(0xff, 0xff, 0xff);

void AddCursorOutline(int width, int height, uint32_t* data) {
  for (int y = 0; y < height; y++) {
    for (int x = 0; x < width; x++) {

      if (*data == kPixelRgbaTransparent) {

        if ((y > 0 && data[-width] == kPixelRgbaBlack) ||
            (y < height - 1 && data[width] == kPixelRgbaBlack) ||
            (x > 0 && data[-1] == kPixelRgbaBlack) ||
            (x < width - 1 && data[1] == kPixelRgbaBlack)) {
          *data = kPixelRgbaWhite;
        }
      }
      data++;
    }
  }
}

void AlphaMul(uint32_t* data, int width, int height) {
  static_assert(sizeof(uint32_t) == kBytesPerPixel,
                "size of uint32 should be the number of bytes per pixel");

  for (uint32_t* data_end = data + width * height; data != data_end; ++data) {
    RGBQUAD* from = reinterpret_cast<RGBQUAD*>(data);
    RGBQUAD* to = reinterpret_cast<RGBQUAD*>(data);
    to->rgbBlue =
        (static_cast<uint16_t>(from->rgbBlue) * from->rgbReserved) / 0xff;
    to->rgbGreen =
        (static_cast<uint16_t>(from->rgbGreen) * from->rgbReserved) / 0xff;
    to->rgbRed =
        (static_cast<uint16_t>(from->rgbRed) * from->rgbReserved) / 0xff;
  }
}

bool HasAlphaChannel(const uint32_t* data, int stride, int width, int height) {
  const RGBQUAD* plane = reinterpret_cast<const RGBQUAD*>(data);
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      if (plane->rgbReserved != 0)
        return true;
      plane += 1;
    }
    plane += stride - width;
  }

  return false;
}

}

MouseCursor* CreateMouseCursorFromHCursor(HDC dc, HCURSOR cursor) {
  ICONINFO iinfo;
  if (!GetIconInfo(cursor, &iinfo)) {
    RTC_LOG_F(LS_ERROR) << "Unable to get cursor icon info. Error = "
                        << GetLastError();
    return NULL;
  }

  int hotspot_x = iinfo.xHotspot;
  int hotspot_y = iinfo.yHotspot;

  win::ScopedBitmap scoped_mask(iinfo.hbmMask);
  win::ScopedBitmap scoped_color(iinfo.hbmColor);
  bool is_color = iinfo.hbmColor != NULL;

  BITMAP bitmap_info;
  if (!GetObject(scoped_mask, sizeof(bitmap_info), &bitmap_info)) {
    RTC_LOG_F(LS_ERROR) << "Unable to get bitmap info. Error = "
                        << GetLastError();
    return NULL;
  }

  int width = bitmap_info.bmWidth;
  int height = bitmap_info.bmHeight;
  std::unique_ptr<uint32_t[]> mask_data(new uint32_t[width * height]);

  BITMAPV5HEADER bmi = {0};
  bmi.bV5Size = sizeof(bmi);
  bmi.bV5Width = width;
  bmi.bV5Height = -height;
  bmi.bV5Planes = 1;
  bmi.bV5BitCount = kBytesPerPixel * 8;
  bmi.bV5Compression = BI_RGB;
  bmi.bV5AlphaMask = 0xff000000;
  bmi.bV5CSType = LCS_WINDOWS_COLOR_SPACE;
  bmi.bV5Intent = LCS_GM_BUSINESS;
  if (!GetDIBits(dc, scoped_mask, 0, height, mask_data.get(),
                 reinterpret_cast<BITMAPINFO*>(&bmi), DIB_RGB_COLORS)) {
    RTC_LOG_F(LS_ERROR) << "Unable to get bitmap bits. Error = "
                        << GetLastError();
    return NULL;
  }

  uint32_t* mask_plane = mask_data.get();
  std::unique_ptr<DesktopFrame> image(
      new BasicDesktopFrame(DesktopSize(width, height)));
  bool has_alpha = false;

  if (is_color) {
    image.reset(new BasicDesktopFrame(DesktopSize(width, height)));

    if (!GetDIBits(dc, scoped_color, 0, height, image->data(),
                   reinterpret_cast<BITMAPINFO*>(&bmi), DIB_RGB_COLORS)) {
      RTC_LOG_F(LS_ERROR) << "Unable to get bitmap bits. Error = "
                          << GetLastError();
      return NULL;
    }

    has_alpha = HasAlphaChannel(reinterpret_cast<uint32_t*>(image->data()),
                                width, width, height);
  } else {

    height /= 2;

    image.reset(new BasicDesktopFrame(DesktopSize(width, height)));

    memcpy(image->data(), mask_plane + (width * height),
           image->stride() * height);
  }

  if (!has_alpha) {
    bool add_outline = false;
    uint32_t* dst = reinterpret_cast<uint32_t*>(image->data());
    uint32_t* mask = mask_plane;
    for (int y = 0; y < height; y++) {
      for (int x = 0; x < width; x++) {

        if (*mask == kPixelRgbWhite) {
          if (*dst != 0) {
            add_outline = true;
            *dst = kPixelRgbaBlack;
          } else {
            *dst = kPixelRgbaTransparent;
          }
        } else {
          *dst = kPixelRgbaBlack ^ *dst;
        }

        ++dst;
        ++mask;
      }
    }
    if (add_outline) {
      AddCursorOutline(width, height,
                       reinterpret_cast<uint32_t*>(image->data()));
    }
  }

  AlphaMul(reinterpret_cast<uint32_t*>(image->data()), width, height);

  return new MouseCursor(image.release(), DesktopVector(hotspot_x, hotspot_y));
}

}
