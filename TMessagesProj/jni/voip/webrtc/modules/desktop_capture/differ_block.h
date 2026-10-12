/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_DESKTOP_CAPTURE_DIFFER_BLOCK_H_
#define MODULES_DESKTOP_CAPTURE_DIFFER_BLOCK_H_

#include <stdint.h>

namespace webrtc {

const int kBlockSize = 32;

const int kBytesPerPixel = 4;

bool VectorDifference(const uint8_t* image1, const uint8_t* image2);

bool BlockDifference(const uint8_t* image1,
                     const uint8_t* image2,
                     int height,
                     int stride);

bool BlockDifference(const uint8_t* image1, const uint8_t* image2, int stride);

}

#endif
