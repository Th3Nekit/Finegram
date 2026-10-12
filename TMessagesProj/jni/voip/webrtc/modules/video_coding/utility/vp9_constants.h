/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_VIDEO_CODING_UTILITY_VP9_CONSTANTS_H_
#define MODULES_VIDEO_CODING_UTILITY_VP9_CONSTANTS_H_

#include <stddef.h>
#include <stdint.h>

#include <string>

namespace webrtc {

constexpr size_t kVp9NumRefFrames = 8;

constexpr size_t kVp9NumFrameContexts = 4;

constexpr size_t kVp9RefsPerFrame = 3;

constexpr size_t kVp9MvFrSize = 4;

constexpr size_t kVp9MvrefNeighbours = 8;

constexpr size_t kVp9BlockSizeGroups = 4;

constexpr size_t kVp9BlockSizes = 13;

constexpr size_t kVp9BlockInvalid = 14;

constexpr size_t kVp9PartitionContexts = 16;

constexpr size_t kVp9MiSize = 8;

constexpr size_t kVp9MinTileWidth_B64 = 4;

constexpr size_t kVp9MaxTileWidth_B64 = 64;

constexpr size_t kVp9MaxMvRefCandidates = 2;

constexpr size_t kVp9MaxRefFrames = 4;

constexpr size_t kVp9IsInterContexts = 4;

constexpr size_t kVp9CompModeContexts = 5;

constexpr size_t kVp9RefContexts = 5;

constexpr size_t kVp9MaxSegments = 8;

constexpr size_t kVp9SegLvlAlt_Q = 0;

constexpr size_t kVp9SegLvlAlt_L = 1;

constexpr size_t kVp9SegLvlRefFrame = 2;

constexpr size_t kVp9SegLvlSkip = 3;

constexpr size_t kVp9SegLvlMax = 4;

constexpr size_t kVp9BlockTypes = 2;

constexpr size_t kVp9RefTypes = 2;

constexpr size_t kVp9CoefBands = 6;

constexpr size_t kVp9PrevCoefContexts = 6;

constexpr size_t kVp9UnconstrainedNodes = 3;

constexpr size_t kVp9TxSizeContexts = 2;

constexpr size_t kVp9SwitchableFilters = 3;

constexpr size_t kVp9InterpFilterContexts = 4;

constexpr size_t kVp9SkipContexts = 3;

constexpr size_t kVp9PartitionTypes = 4;

constexpr size_t kVp9TxSizes = 4;

constexpr size_t kVp9TxModes = 5;

constexpr size_t kVp9DctDct = 0;

constexpr size_t kVp9AdstDct = 1;

constexpr size_t kVp9DctAdst = 2;

constexpr size_t kVp9AdstAdst = 3;

constexpr size_t kVp9MbModeCount = 14;

constexpr size_t kVp9IntraModes = 10;

constexpr size_t kVp9InterModes = 4;

constexpr size_t kVp9InterModeContexts = 7;

constexpr size_t kVp9MvJoints = 4;

constexpr size_t kVp9MvClasses = 11;

constexpr size_t kVp9Class0Size = 2;

constexpr size_t kVp9MvOffsetBits = 10;

constexpr size_t kVp9MaxProb = 255;

constexpr size_t kVp9MaxModeLfDeltas = 2;

constexpr size_t kVp9CompandedMvrefThresh = 8;

constexpr size_t kVp9MaxLoopFilter = 63;

constexpr size_t kVp9RefScaleShift = 14;

constexpr size_t kVp9SubpelBits = 4;

constexpr size_t kVp9SubpelShifts = 16;

constexpr size_t kVp9SubpelMask = 15;

constexpr size_t kVp9MvBorder = 128;

constexpr size_t kVp9InterpExtend = 4;

constexpr size_t kVp9Borderinpixels = 160;

constexpr size_t kVp9MaxUpdateFactor = 128;

constexpr size_t kVp9CountSat = 20;

constexpr size_t kVp9BothZero = 0;

constexpr size_t kVp9ZeroPlusPredicted = 1;

constexpr size_t kVp9BothPredicted = 2;

constexpr size_t kVp9NewPlusNonIntra = 3;

constexpr size_t kVp9BothNew = 4;

constexpr size_t kVp9IntraPlusNonIntra = 5;

constexpr size_t kVp9BothIntra = 6;

constexpr size_t kVp9InvalidCase = 9;

enum class Vp9TxMode : uint8_t {
  kOnly4X4 = 0,
  kAllow8X8 = 1,
  kAllow16x16 = 2,
  kAllow32x32 = 3,
  kTxModeSelect = 4
};

enum Vp9BlockSize : uint8_t {
  kBlock4X4 = 0,
  kBlock4X8 = 1,
  kBlock8X4 = 2,
  kBlock8X8 = 3,
  kBlock8X16 = 4,
  kBlock16X8 = 5,
  kBlock16X16 = 6,
  kBlock16X32 = 7,
  kBlock32X16 = 8,
  kBlock32X32 = 9,
  kBlock32X64 = 10,
  kBlock64X32 = 11,
  kBlock64X64 = 12
};

enum Vp9Partition : uint8_t {
  kPartitionNone = 0,
  kPartitionHorizontal = 1,
  kPartitionVertical = 2,
  kPartitionSplit = 3
};

enum class Vp9ReferenceMode : uint8_t {
  kSingleReference = 0,
  kCompoundReference = 1,
  kReferenceModeSelect = 2,
};

}

#endif
