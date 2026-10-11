/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_INCLUDE_MODULE_FEC_TYPES_H_
#define MODULES_INCLUDE_MODULE_FEC_TYPES_H_

namespace webrtc {

enum FecMaskType {
  kFecMaskRandom,
  kFecMaskBursty,
};

struct FecProtectionParams {
  int fec_rate = 0;
  int max_fec_frames = 0;
  FecMaskType fec_mask_type = FecMaskType::kFecMaskRandom;
};

}

#endif
