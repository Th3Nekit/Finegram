/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/rtp_rtcp/source/ulpfec_generator.h"

#include <string.h>

#include <cstdint>
#include <memory>
#include <utility>

#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "modules/rtp_rtcp/source/byte_io.h"
#include "modules/rtp_rtcp/source/forward_error_correction.h"
#include "modules/rtp_rtcp/source/forward_error_correction_internal.h"
#include "rtc_base/checks.h"
#include "rtc_base/synchronization/mutex.h"

namespace webrtc {

namespace {

constexpr size_t kRedForFecHeaderLength = 1;

constexpr int kMaxExcessOverhead = 50;

constexpr size_t kMinMediaPackets = 4;

constexpr uint8_t kHighProtectionThreshold = 80;

constexpr float kMinMediaPacketsAdaptationThreshold = 2.0f;

constexpr uint32_t kUnknownSsrc = 0;

}

UlpfecGenerator::Params::Params() = default;
UlpfecGenerator::Params::Params(FecProtectionParams delta_params,
                                FecProtectionParams keyframe_params)
    : delta_params(delta_params), keyframe_params(keyframe_params) {}

UlpfecGenerator::UlpfecGenerator(int red_payload_type,
                                 int ulpfec_payload_type,
                                 Clock* clock)
    : red_payload_type_(red_payload_type),
      ulpfec_payload_type_(ulpfec_payload_type),
      clock_(clock),
      fec_(ForwardErrorCorrection::CreateUlpfec(kUnknownSsrc)),
      num_protected_frames_(0),
      min_num_media_packets_(1),
      media_contains_keyframe_(false),
      fec_bitrate_(                    TimeDelta::Seconds(1)) {}

UlpfecGenerator::UlpfecGenerator(std::unique_ptr<ForwardErrorCorrection> fec,
                                 Clock* clock)
    : red_payload_type_(0),
      ulpfec_payload_type_(0),
      clock_(clock),
      fec_(std::move(fec)),
      num_protected_frames_(0),
      min_num_media_packets_(1),
      media_contains_keyframe_(false),
      fec_bitrate_(                    TimeDelta::Seconds(1)) {}

UlpfecGenerator::~UlpfecGenerator() = default;

void UlpfecGenerator::SetProtectionParameters(
    const FecProtectionParams& delta_params,
    const FecProtectionParams& key_params) {
  RTC_DCHECK_GE(delta_params.fec_rate, 0);
  RTC_DCHECK_LE(delta_params.fec_rate, 255);
  RTC_DCHECK_GE(key_params.fec_rate, 0);
  RTC_DCHECK_LE(key_params.fec_rate, 255);

  MutexLock lock(&mutex_);
  pending_params_.emplace(delta_params, key_params);
}

void UlpfecGenerator::AddPacketAndGenerateFec(const RtpPacketToSend& packet) {
  RTC_DCHECK_RUNS_SERIALIZED(&race_checker_);
  RTC_DCHECK(generated_fec_packets_.empty());

  {
    MutexLock lock(&mutex_);
    if (pending_params_) {
      current_params_ = *pending_params_;
      pending_params_.reset();

      if (CurrentParams().fec_rate > kHighProtectionThreshold) {
        min_num_media_packets_ = kMinMediaPackets;
      } else {
        min_num_media_packets_ = 1;
      }
    }
  }

  if (packet.is_key_frame()) {
    media_contains_keyframe_ = true;
  }
  const bool complete_frame = packet.Marker();
  if (media_packets_.size() < kUlpfecMaxMediaPackets) {

    auto fec_packet = std::make_unique<ForwardErrorCorrection::Packet>();
    fec_packet->data = packet.Buffer();
    media_packets_.push_back(std::move(fec_packet));

    RTC_DCHECK_GE(packet.headers_size(), kRtpHeaderSize);
    last_media_packet_ = packet;
  }

  if (complete_frame) {
    ++num_protected_frames_;
  }

  auto params = CurrentParams();

  if (complete_frame &&
      (num_protected_frames_ >= params.max_fec_frames ||
       (ExcessOverheadBelowMax() && MinimumMediaPacketsReached()))) {

    constexpr int kNumImportantPackets = 0;
    constexpr bool kUseUnequalProtection = false;
    fec_->EncodeFec(media_packets_, params.fec_rate, kNumImportantPackets,
                    kUseUnequalProtection, params.fec_mask_type,
                    &generated_fec_packets_);
    if (generated_fec_packets_.empty()) {
      ResetState();
    }
  }
}

bool UlpfecGenerator::ExcessOverheadBelowMax() const {
  RTC_DCHECK_RUNS_SERIALIZED(&race_checker_);

  return ((Overhead() - CurrentParams().fec_rate) < kMaxExcessOverhead);
}

bool UlpfecGenerator::MinimumMediaPacketsReached() const {
  RTC_DCHECK_RUNS_SERIALIZED(&race_checker_);
  float average_num_packets_per_frame =
      static_cast<float>(media_packets_.size()) / num_protected_frames_;
  int num_media_packets = static_cast<int>(media_packets_.size());
  if (average_num_packets_per_frame < kMinMediaPacketsAdaptationThreshold) {
    return num_media_packets >= min_num_media_packets_;
  } else {

    return num_media_packets >= min_num_media_packets_ + 1;
  }
}

const FecProtectionParams& UlpfecGenerator::CurrentParams() const {
  RTC_DCHECK_RUNS_SERIALIZED(&race_checker_);
  return media_contains_keyframe_ ? current_params_.keyframe_params
                                  : current_params_.delta_params;
}

size_t UlpfecGenerator::MaxPacketOverhead() const {
  RTC_DCHECK_RUNS_SERIALIZED(&race_checker_);
  return fec_->MaxPacketOverhead();
}

std::vector<std::unique_ptr<RtpPacketToSend>> UlpfecGenerator::GetFecPackets() {
  RTC_DCHECK_RUNS_SERIALIZED(&race_checker_);
  if (generated_fec_packets_.empty()) {
    return std::vector<std::unique_ptr<RtpPacketToSend>>();
  }

  RTC_CHECK(last_media_packet_.has_value());
  last_media_packet_->SetPayloadSize(0);

  std::vector<std::unique_ptr<RtpPacketToSend>> fec_packets;
  fec_packets.reserve(generated_fec_packets_.size());

  size_t total_fec_size_bytes = 0;
  for (const auto* fec_packet : generated_fec_packets_) {
    std::unique_ptr<RtpPacketToSend> red_packet =
        std::make_unique<RtpPacketToSend>(*last_media_packet_);
    red_packet->SetPayloadType(red_payload_type_);
    red_packet->SetMarker(false);
    uint8_t* payload_buffer = red_packet->SetPayloadSize(
        kRedForFecHeaderLength + fec_packet->data.size());

    payload_buffer[0] = ulpfec_payload_type_;
    memcpy(&payload_buffer[1], fec_packet->data.data(),
           fec_packet->data.size());
    total_fec_size_bytes += red_packet->size();
    red_packet->set_packet_type(RtpPacketMediaType::kForwardErrorCorrection);
    red_packet->set_allow_retransmission(false);
    red_packet->set_is_red(true);
    red_packet->set_fec_protect_packet(false);
    fec_packets.push_back(std::move(red_packet));
  }

  ResetState();

  MutexLock lock(&mutex_);
  fec_bitrate_.Update(total_fec_size_bytes, clock_->CurrentTime());

  return fec_packets;
}

DataRate UlpfecGenerator::CurrentFecRate() const {
  MutexLock lock(&mutex_);
  return fec_bitrate_.Rate(clock_->CurrentTime()).value_or(DataRate::Zero());
}

int UlpfecGenerator::Overhead() const {
  RTC_DCHECK_RUNS_SERIALIZED(&race_checker_);
  RTC_DCHECK(!media_packets_.empty());
  int num_fec_packets =
      fec_->NumFecPackets(media_packets_.size(), CurrentParams().fec_rate);

  return (num_fec_packets << 8) / media_packets_.size();
}

void UlpfecGenerator::ResetState() {
  RTC_DCHECK_RUNS_SERIALIZED(&race_checker_);
  media_packets_.clear();
  last_media_packet_.reset();
  generated_fec_packets_.clear();
  num_protected_frames_ = 0;
  media_contains_keyframe_ = false;
}

}
