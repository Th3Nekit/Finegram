/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/rtp_rtcp/source/ulpfec_receiver.h"

#include <memory>
#include <utility>

#include "api/scoped_refptr.h"
#include "modules/rtp_rtcp/source/rtp_packet_received.h"
#include "rtc_base/logging.h"
#include "rtc_base/time_utils.h"
#include "system_wrappers/include/metrics.h"

namespace webrtc {

UlpfecReceiver::UlpfecReceiver(uint32_t ssrc,
                               int ulpfec_payload_type,
                               RecoveredPacketReceiver* callback,
                               Clock* clock)
    : ssrc_(ssrc),
      ulpfec_payload_type_(ulpfec_payload_type),
      clock_(clock),
      recovered_packet_callback_(callback),
      fec_(ForwardErrorCorrection::CreateUlpfec(ssrc_)) {

  RTC_DCHECK_GE(ulpfec_payload_type_, -1);
}

UlpfecReceiver::~UlpfecReceiver() {
  RTC_DCHECK_RUN_ON(&sequence_checker_);

  if (packet_counter_.first_packet_time != Timestamp::MinusInfinity()) {
    const Timestamp now = clock_->CurrentTime();
    TimeDelta elapsed = (now - packet_counter_.first_packet_time);
    if (elapsed.seconds() >= metrics::kMinRunTimeInSeconds) {
      if (packet_counter_.num_packets > 0) {
        RTC_HISTOGRAM_PERCENTAGE(
            "WebRTC.Video.ReceivedFecPacketsInPercent",
            static_cast<int>(packet_counter_.num_fec_packets * 100 /
                             packet_counter_.num_packets));
      }
      if (packet_counter_.num_fec_packets > 0) {
        RTC_HISTOGRAM_PERCENTAGE(
            "WebRTC.Video.RecoveredMediaPacketsInPercentOfFec",
            static_cast<int>(packet_counter_.num_recovered_packets * 100 /
                             packet_counter_.num_fec_packets));
      }
      if (ulpfec_payload_type_ != -1) {
        RTC_HISTOGRAM_COUNTS_10000(
            "WebRTC.Video.FecBitrateReceivedInKbps",
            static_cast<int>(packet_counter_.num_bytes * 8 / elapsed.seconds() /
                             1000));
      }
    }
  }

  received_packets_.clear();
  fec_->ResetState(&recovered_packets_);
}

FecPacketCounter UlpfecReceiver::GetPacketCounter() const {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  return packet_counter_;
}

bool UlpfecReceiver::AddReceivedRedPacket(const RtpPacketReceived& rtp_packet) {
  RTC_DCHECK_RUN_ON(&sequence_checker_);

  if (rtp_packet.Ssrc() != ssrc_) {
    RTC_LOG(LS_WARNING)
        << "Received RED packet with different SSRC than expected; dropping.";
    return false;
  }
  if (rtp_packet.size() > IP_PACKET_SIZE) {
    RTC_LOG(LS_WARNING) << "Received RED packet with length exceeds maximum IP "
                           "packet size; dropping.";
    return false;
  }

  static constexpr uint8_t kRedHeaderLength = 1;

  if (rtp_packet.payload_size() == 0) {
    RTC_LOG(LS_WARNING) << "Corrupt/truncated FEC packet.";
    return false;
  }

  auto received_packet =
      std::make_unique<ForwardErrorCorrection::ReceivedPacket>();
  received_packet->pkt = new ForwardErrorCorrection::Packet();

  uint8_t payload_type = rtp_packet.payload()[0] & 0x7f;
  received_packet->is_fec = payload_type == ulpfec_payload_type_;
  received_packet->is_recovered = rtp_packet.recovered();
  received_packet->ssrc = rtp_packet.Ssrc();
  received_packet->seq_num = rtp_packet.SequenceNumber();
  received_packet->extensions = rtp_packet.extension_manager();

  if (rtp_packet.payload()[0] & 0x80) {

    RTC_LOG(LS_WARNING) << "More than 1 block in RED packet is not supported.";
    return false;
  }

  ++packet_counter_.num_packets;
  packet_counter_.num_bytes += rtp_packet.size();
  if (packet_counter_.first_packet_time == Timestamp::MinusInfinity()) {
    packet_counter_.first_packet_time = clock_->CurrentTime();
  }

  if (received_packet->is_fec) {
    ++packet_counter_.num_fec_packets;

    received_packet->pkt->data =
        rtp_packet.Buffer().Slice(rtp_packet.headers_size() + kRedHeaderLength,
                                  rtp_packet.payload_size() - kRedHeaderLength);
  } else {
    received_packet->pkt->data.EnsureCapacity(rtp_packet.size() -
                                              kRedHeaderLength);

    received_packet->pkt->data.SetData(rtp_packet.data(),
                                       rtp_packet.headers_size());

    uint8_t& payload_type_byte = received_packet->pkt->data.MutableData()[1];
    payload_type_byte &= 0x80;
    payload_type_byte += payload_type;

    received_packet->pkt->data.AppendData(
        rtp_packet.data() + rtp_packet.headers_size() + kRedHeaderLength,
        rtp_packet.size() - rtp_packet.headers_size() - kRedHeaderLength);
  }

  if (received_packet->pkt->data.size() > 0) {
    received_packets_.push_back(std::move(received_packet));
  }
  return true;
}

void UlpfecReceiver::ProcessReceivedFec() {
  RTC_DCHECK_RUN_ON(&sequence_checker_);

  std::vector<std::unique_ptr<ForwardErrorCorrection::ReceivedPacket>>
      received_packets;
  received_packets.swap(received_packets_);
  RtpHeaderExtensionMap* last_recovered_extension_map = nullptr;
  size_t num_recovered_packets = 0;

  for (const auto& received_packet : received_packets) {

    if (!received_packet->is_fec) {
      ForwardErrorCorrection::Packet* packet = received_packet->pkt.get();

      RtpPacketReceived rtp_packet(&received_packet->extensions);
      if (!rtp_packet.Parse(std::move(packet->data))) {
        RTC_LOG(LS_WARNING) << "Corrupted media packet";
        continue;
      }
      recovered_packet_callback_->OnRecoveredPacket(rtp_packet);

      rtp_packet.ZeroMutableExtensions();
      packet->data = rtp_packet.Buffer();
    }
    if (!received_packet->is_recovered) {

      ForwardErrorCorrection::DecodeFecResult decode_result =
          fec_->DecodeFec(*received_packet, &recovered_packets_);
      last_recovered_extension_map = &received_packet->extensions;
      num_recovered_packets += decode_result.num_recovered_packets;
    }
  }

  if (num_recovered_packets == 0) {
    return;
  }

  for (const auto& recovered_packet : recovered_packets_) {
    if (recovered_packet->returned) {

      continue;
    }
    ForwardErrorCorrection::Packet* packet = recovered_packet->pkt.get();
    ++packet_counter_.num_recovered_packets;

    recovered_packet->returned = true;
    RtpPacketReceived parsed_packet(last_recovered_extension_map);
    if (!parsed_packet.Parse(packet->data)) {
      continue;
    }
    parsed_packet.set_recovered(true);
    recovered_packet_callback_->OnRecoveredPacket(parsed_packet);
  }
}

}
