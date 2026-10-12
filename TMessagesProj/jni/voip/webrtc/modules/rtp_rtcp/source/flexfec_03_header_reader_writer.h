/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_RTP_RTCP_SOURCE_FLEXFEC_03_HEADER_READER_WRITER_H_
#define MODULES_RTP_RTCP_SOURCE_FLEXFEC_03_HEADER_READER_WRITER_H_

#include <stddef.h>
#include <stdint.h>

#include "modules/rtp_rtcp/source/forward_error_correction.h"

namespace webrtc {

class Flexfec03HeaderReader : public FecHeaderReader {
 public:
  Flexfec03HeaderReader();
  ~Flexfec03HeaderReader() override;

  bool ReadFecHeader(
      ForwardErrorCorrection::ReceivedFecPacket* fec_packet) const override;
};

class Flexfec03HeaderWriter : public FecHeaderWriter {
 public:
  Flexfec03HeaderWriter();
  ~Flexfec03HeaderWriter() override;

  size_t MinPacketMaskSize(const uint8_t* packet_mask,
                           size_t packet_mask_size) const override;

  size_t FecHeaderSize(size_t packet_mask_row_size) const override;

  void FinalizeFecHeader(
      rtc::ArrayView<const ProtectedStream> protected_streams,
      ForwardErrorCorrection::Packet& fec_packet) const override;
};

}

#endif
