/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_INCLUDE_MODULE_COMMON_TYPES_H_
#define MODULES_INCLUDE_MODULE_COMMON_TYPES_H_

#include <stdint.h>

#include <vector>

namespace webrtc {

class CallStatsObserver {
 public:
  virtual void OnRttUpdate(int64_t avg_rtt_ms, int64_t max_rtt_ms) = 0;

  virtual ~CallStatsObserver() {}
};

class NackSender {
 public:

  virtual void SendNack(const std::vector<uint16_t>& sequence_numbers,
                        bool buffering_allowed) = 0;

 protected:
  virtual ~NackSender() {}
};

class KeyFrameRequestSender {
 public:
  virtual void RequestKeyFrame() = 0;

 protected:
  virtual ~KeyFrameRequestSender() {}
};

class LossNotificationSender {
 public:
  virtual ~LossNotificationSender() {}

  virtual void SendLossNotification(uint16_t last_decoded_seq_num,
                                    uint16_t last_received_seq_num,
                                    bool decodability_flag,
                                    bool buffering_allowed) = 0;
};

}

#endif
