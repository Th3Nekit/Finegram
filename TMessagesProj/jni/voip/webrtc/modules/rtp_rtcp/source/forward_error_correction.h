/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_RTP_RTCP_SOURCE_FORWARD_ERROR_CORRECTION_H_
#define MODULES_RTP_RTCP_SOURCE_FORWARD_ERROR_CORRECTION_H_

#include <stddef.h>
#include <stdint.h>

#include <list>
#include <memory>
#include <vector>

#include "absl/container/inlined_vector.h"
#include "api/scoped_refptr.h"
#include "api/units/timestamp.h"
#include "modules/include/module_fec_types.h"
#include "modules/rtp_rtcp/include/rtp_header_extension_map.h"
#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "modules/rtp_rtcp/source/forward_error_correction_internal.h"
#include "rtc_base/copy_on_write_buffer.h"

namespace webrtc {

class FecHeaderReader;
class FecHeaderWriter;

class ForwardErrorCorrection {
 public:

  class Packet {
   public:
    Packet();
    virtual ~Packet();

    virtual int32_t AddRef();

    virtual int32_t Release();

    rtc::CopyOnWriteBuffer data;

   private:
    int32_t ref_count_;
  };

  class SortablePacket {
   public:

    struct LessThan {
      template <typename S, typename T>
      bool operator()(const S& first, const T& second);
    };

    uint32_t ssrc;
    uint16_t seq_num;
  };

  class ReceivedPacket : public SortablePacket {
   public:
    ReceivedPacket();
    ~ReceivedPacket();

    bool is_fec;

    bool is_recovered;
    RtpHeaderExtensionMap extensions;
    rtc::scoped_refptr<Packet> pkt;
  };

  class RecoveredPacket : public SortablePacket {
   public:
    RecoveredPacket();
    ~RecoveredPacket();

    bool was_recovered;

    bool returned;

    rtc::scoped_refptr<Packet> pkt;
  };

  class ProtectedPacket : public SortablePacket {
   public:
    ProtectedPacket();
    ~ProtectedPacket();

    rtc::scoped_refptr<ForwardErrorCorrection::Packet> pkt;
  };

  using ProtectedPacketList = std::list<std::unique_ptr<ProtectedPacket>>;

  struct ProtectedStream {
    uint32_t ssrc = 0;
    uint16_t seq_num_base = 0;
    size_t packet_mask_offset = 0;
    size_t packet_mask_size = 0;
  };

  class ReceivedFecPacket : public SortablePacket {
   public:

    static constexpr size_t kInlinedSsrcsVectorSize = 4;

    ReceivedFecPacket();
    ~ReceivedFecPacket();

    ProtectedPacketList protected_packets;

    uint32_t ssrc;

    size_t fec_header_size;
    absl::InlinedVector<ProtectedStream, kInlinedSsrcsVectorSize>
        protected_streams;
    size_t protection_length;

    rtc::scoped_refptr<ForwardErrorCorrection::Packet> pkt;
  };

  using PacketList = std::list<std::unique_ptr<Packet>>;
  using RecoveredPacketList = std::list<std::unique_ptr<RecoveredPacket>>;
  using ReceivedFecPacketList = std::list<std::unique_ptr<ReceivedFecPacket>>;

  ~ForwardErrorCorrection();

  static std::unique_ptr<ForwardErrorCorrection> CreateUlpfec(uint32_t ssrc);
  static std::unique_ptr<ForwardErrorCorrection> CreateFlexfec(
      uint32_t ssrc,
      uint32_t protected_media_ssrc);

  int EncodeFec(const PacketList& media_packets,
                uint8_t protection_factor,
                int num_important_packets,
                bool use_unequal_protection,
                FecMaskType fec_mask_type,
                std::list<Packet*>* fec_packets);

  struct DecodeFecResult {

    size_t num_recovered_packets = 0;
  };

  DecodeFecResult DecodeFec(const ReceivedPacket& received_packet,
                            RecoveredPacketList* recovered_packets);

  static int NumFecPackets(int num_media_packets, int protection_factor);

  size_t MaxPacketOverhead() const;

  void ResetState(RecoveredPacketList* recovered_packets);

  static uint16_t ParseSequenceNumber(const uint8_t* packet);
  static uint32_t ParseSsrc(const uint8_t* packet);

 protected:
  ForwardErrorCorrection(std::unique_ptr<FecHeaderReader> fec_header_reader,
                         std::unique_ptr<FecHeaderWriter> fec_header_writer,
                         uint32_t ssrc,
                         uint32_t protected_media_ssrc);

 private:

  int InsertZerosInPacketMasks(const PacketList& media_packets,
                               size_t num_fec_packets);

  void GenerateFecPayloads(const PacketList& media_packets,
                           size_t num_fec_packets);

  void FinalizeFecHeaders(size_t num_fec_packets,
                          uint32_t media_ssrc,
                          uint16_t seq_num_base);

  void InsertPacket(const ReceivedPacket& received_packet,
                    RecoveredPacketList* recovered_packets);

  void InsertMediaPacket(RecoveredPacketList* recovered_packets,
                         const ReceivedPacket& received_packet);

  void UpdateCoveringFecPackets(const RecoveredPacket& packet);

  void InsertFecPacket(const RecoveredPacketList& recovered_packets,
                       const ReceivedPacket& received_packet);

  static void AssignRecoveredPackets(
      const RecoveredPacketList& recovered_packets,
      ReceivedFecPacket* fec_packet);

  size_t AttemptRecovery(RecoveredPacketList* recovered_packets);

  static bool StartPacketRecovery(const ReceivedFecPacket& fec_packet,
                                  RecoveredPacket* recovered_packet);

  static void XorHeaders(const Packet& src, Packet* dst);

  static void XorPayloads(const Packet& src,
                          size_t payload_length,
                          size_t dst_offset,
                          Packet* dst);

  static bool FinishPacketRecovery(const ReceivedFecPacket& fec_packet,
                                   RecoveredPacket* recovered_packet);

  static bool RecoverPacket(const ReceivedFecPacket& fec_packet,
                            RecoveredPacket* recovered_packet);

  static int NumCoveredPacketsMissing(const ReceivedFecPacket& fec_packet);

  void DiscardOldRecoveredPackets(RecoveredPacketList* recovered_packets);

  bool IsOldFecPacket(const ReceivedFecPacket& fec_packet,
                      const RecoveredPacketList* recovered_packets);

  const uint32_t ssrc_;
  const uint32_t protected_media_ssrc_;

  std::unique_ptr<FecHeaderReader> fec_header_reader_;
  std::unique_ptr<FecHeaderWriter> fec_header_writer_;

  std::vector<Packet> generated_fec_packets_;
  ReceivedFecPacketList received_fec_packets_;

  uint8_t packet_masks_[kUlpfecMaxMediaPackets * kUlpfecMaxPacketMaskSize];
  uint8_t tmp_packet_masks_[kUlpfecMaxMediaPackets * kUlpfecMaxPacketMaskSize];
  size_t packet_mask_size_;
};

class FecHeaderReader {
 public:
  virtual ~FecHeaderReader();

  size_t MaxMediaPackets() const;

  size_t MaxFecPackets() const;

  virtual bool ReadFecHeader(
      ForwardErrorCorrection::ReceivedFecPacket* fec_packet) const = 0;

 protected:
  FecHeaderReader(size_t max_media_packets, size_t max_fec_packets);

  const size_t max_media_packets_;
  const size_t max_fec_packets_;
};

class FecHeaderWriter {
 public:
  struct ProtectedStream {
    uint32_t ssrc = 0;
    uint16_t seq_num_base = 0;
    rtc::ArrayView<const uint8_t> packet_mask;
  };

  virtual ~FecHeaderWriter();

  size_t MaxMediaPackets() const;

  size_t MaxFecPackets() const;

  size_t MaxPacketOverhead() const;

  virtual size_t MinPacketMaskSize(const uint8_t* packet_mask,
                                   size_t packet_mask_size) const = 0;

  virtual size_t FecHeaderSize(size_t packet_mask_size) const = 0;

  virtual void FinalizeFecHeader(
      rtc::ArrayView<const ProtectedStream> protected_streams,
      ForwardErrorCorrection::Packet& fec_packet) const = 0;

 protected:
  FecHeaderWriter(size_t max_media_packets,
                  size_t max_fec_packets,
                  size_t max_packet_overhead);

  const size_t max_media_packets_;
  const size_t max_fec_packets_;
  const size_t max_packet_overhead_;
};

}

#endif
