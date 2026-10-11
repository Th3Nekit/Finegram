/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_VIDEO_CODING_CODECS_H264_H264_COLOR_SPACE_H_
#define MODULES_VIDEO_CODING_CODECS_H264_H264_COLOR_SPACE_H_

#ifdef WEBRTC_USE_H264

#if defined(WEBRTC_WIN) && !defined(__clang__)
#error "See: bugs.webrtc.org/9213#c13."
#endif

#include "api/video/color_space.h"

extern "C" {
#include <libavcodec/avcodec.h>
}

namespace webrtc {

ColorSpace ExtractH264ColorSpace(AVCodecContext* codec);

}

#endif

#endif
