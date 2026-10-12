/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/rtp_rtcp/include/flexfec_sender.h"

#include <string.h>

#include <list>
#include <utility>

#include "absl/strings/string_view.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "modules/rtp_rtcp/source/forward_error_correction.h"
#include "modules/rtp_rtcp/source/rtp_header_extensions.h"
#include "modules/rtp_rtcp/source/rtp_packet_to_send.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"

namespace webrtc {

namespace {

constexpr uint16_t kMaxInitRtpSeqNumber = 0x7fff;

constexpr size_t kFlexfecMaxHeaderSize = 32;

const int kMsToRtpTimestamp = kVideoPayloadTypeFrequency / 1000;

constexpr TimeDelta kPacketLogInterval = TimeDelta::Seconds(10);

RtpHeaderExtensionMap RegisterSupportedExtensions(
    const std::vector<RtpExtension>& rtp_header_extensions) {
  RtpHeaderExtensionMap map;
  for (const auto& extension : rtp_header_extensions) {
    if (extension.uri == TransportSequenceNumber::Uri()) {
      map.Register<TransportSequenceNumber>(extension.id);
    } else if (extension.uri == AbsoluteSendTime::Uri()) {
      map.Register<AbsoluteSendTime>(extension.id);
    } else if (extension.uri == TransmissionOffset::Uri()) {
      map.Register<TransmissionOffset>(extension.id);
    } else if (extension.uri == RtpMid::Uri()) {
      map.Register<RtpMid>(extension.id);
    } else {
      RTC_LOG(LS_INFO)
          << "FlexfecSender only supports RTP header extensions for "
             "BWE and MID, so the extension "
          << extension.ToString() << " will not be used.";
    }
  }
  return map;
}

}

FlexfecSender::FlexfecSender(
    int payload_type,
    uint32_t ssrc,
    uint32_t protected_media_ssrc,
    absl::string_view mid,
    const std::vector<RtpExtension>& rtp_header_extensions,
    rtc::ArrayView<const RtpExtensionSize> extension_sizes,
    const RtpState* rtp_state,
    Clock* clock)
    : clock_(clock),
      random_(clock_->TimeInMicroseconds()),
      payload_type_(payload_type),

      timestamp_offset_(rtp_state ? rtp_state->start_timestamp
                                  : random_.Rand<uint32_t>()),
      ssrc_(ssrc),
      protected_media_ssrc_(protected_media_ssrc),
      mid_(mid),
      seq_num_(rtp_state ? rtp_state->sequence_number
                         : random_.Rand(1, kMaxInitRtpSeqNumber)),
      ulpfec_generator_(
          ForwardErrorCorrection::CreateFlexfec(ssrc, protected_media_ssrc),
          clock_),
      rtp_header_extension_map_(
          RegisterSupportedExtensions(rtp_header_extensions)),
      header_extensions_size_(
          RtpHeaderExtensionSize(extension_sizes, rtp_header_extension_map_)),
      fec_bitrate_(                    TimeDelta::Seconds(1)) {

  RTC_DCHECK_GE(payload_type, 0);
  RTC_DCHECK_LE(payload_type, 127);
}

FlexfecSender::~FlexfecSender() = default;

void FlexfecSender::SetProtectionParameters(
    const FecProtectionParams& delta_params,
    const FecProtectionParams& key_params) {
  ulpfec_generator_.SetProtectionParameters(delta_params, key_params);
}

void FlexfecSender::AddPacketAndGenerateFec(const RtpPacketToSend& packet) {

  RTC_DCHECK_EQ(packet.Ssrc(), protected_media_ssrc_);
  ulpfec_generator_.AddPacketAndGenerateFec(packet);
}

std::vector<std::unique_ptr<RtpPacketToSend>> FlexfecSender::GetFecPackets() {
  RTC_CHECK_RUNS_SERIALIZED(&ulpfec_generator_.race_checker_);
  std::vector<std::unique_ptr<RtpPacketToSend>> fec_packets_to_send;
  fec_packets_to_send.reserve(ulpfec_generator_.generated_fec_packets_.size());
  size_t total_fec_data_bytes = 0;
  for (const auto* fec_packet : ulpfec_generator_.generated_fec_packets_) {
    std::unique_ptr<RtpPacketToSend> fec_packet_to_send(
        new RtpPacketToSend(&rtp_header_extension_map_));
    fec_packet_to_send->set_packet_type(
        RtpPacketMediaType::kForwardErrorCorrection);
    fec_packet_to_send->set_allow_retransmission(false);

    fec_packet_to_send->SetMarker(false);
    fec_packet_to_send->SetPayloadType(payload_type_);
    fec_packet_to_send->SetSequenceNumber(seq_num_++);
    fec_packet_to_send->SetTimestamp(
        timestamp_offset_ +
        static_cast<uint32_t>(kMsToRtpTimestamp *
                              clock_->TimeInMilliseconds()));

    fec_packet_to_send->set_capture_time(clock_->CurrentTime());
    fec_packet_to_send->SetSsrc(ssrc_);

    fec_packet_to_send->ReserveExtension<AbsoluteSendTime>();
    fec_packet_to_send->ReserveExtension<TransmissionOffset>();
    fec_packet_to_send->ReserveExtension<TransportSequenceNumber>();

    if (!mid_.empty()) {

      fec_packet_to_send->SetExtension<RtpMid>(mid_);
    }

    uint8_t* payload =
        fec_packet_to_send->AllocatePayload(fec_packet->data.size());
    memcpy(payload, fec_packet->data.cdata(), fec_packet->data.size());

    total_fec_data_bytes += fec_packet_to_send->size();
    fec_packets_to_send.push_back(std::move(fec_packet_to_send));
  }

  if (!fec_packets_to_send.empty()) {
    ulpfec_generator_.ResetState();
  }

  Timestamp now = clock_->CurrentTime();
  if (!fec_packets_to_send.empty() &&
      now - last_generated_packet_ > kPacketLogInterval) {
    RTC_LOG(LS_VERBOSE) << "Generated " << fec_packets_to_send.size()
                        << " FlexFEC packets with payload type: "
                        << payload_type_ << " and SSRC: " << ssrc_ << ".";
    last_generated_packet_ = now;
  }

  MutexLock lock(&mutex_);
  fec_bitrate_.Update(total_fec_data_bytes, now);

  return fec_packets_to_send;
}

size_t FlexfecSender::MaxPacketOverhead() const {
  return header_extensions_size_ + kFlexfecMaxHeaderSize;
}

DataRate FlexfecSender::CurrentFecRate() const {
  MutexLock lock(&mutex_);
  return fec_bitrate_.Rate(clock_->CurrentTime()).value_or(DataRate::Zero());
}

absl::optional<RtpState> FlexfecSender::GetRtpState() {
  RtpState rtp_state;
  rtp_state.sequence_number = seq_num_;
  rtp_state.start_timestamp = timestamp_offset_;
  return rtp_state;
}

}
