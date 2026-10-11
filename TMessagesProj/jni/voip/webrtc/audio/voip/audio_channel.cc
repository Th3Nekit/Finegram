/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "audio/voip/audio_channel.h"

#include <utility>
#include <vector>

#include "api/audio_codecs/audio_format.h"
#include "api/task_queue/task_queue_factory.h"
#include "modules/rtp_rtcp/include/receive_statistics.h"
#include "modules/rtp_rtcp/source/rtp_rtcp_impl2.h"
#include "rtc_base/logging.h"

namespace webrtc {

namespace {

constexpr int kRtcpReportIntervalMs = 5000;

}

AudioChannel::AudioChannel(
    Transport* transport,
    uint32_t local_ssrc,
    TaskQueueFactory* task_queue_factory,
    AudioMixer* audio_mixer,
    rtc::scoped_refptr<AudioDecoderFactory> decoder_factory)
    : audio_mixer_(audio_mixer) {
  RTC_DCHECK(task_queue_factory);
  RTC_DCHECK(audio_mixer);

  Clock* clock = Clock::GetRealTimeClock();
  receive_statistics_ = ReceiveStatistics::Create(clock);

  RtpRtcpInterface::Configuration rtp_config;
  rtp_config.clock = clock;
  rtp_config.audio = true;
  rtp_config.receive_statistics = receive_statistics_.get();
  rtp_config.rtcp_report_interval_ms = kRtcpReportIntervalMs;
  rtp_config.outgoing_transport = transport;
  rtp_config.local_media_ssrc = local_ssrc;

  rtp_rtcp_ = ModuleRtpRtcpImpl2::Create(rtp_config);

  rtp_rtcp_->SetSendingMediaStatus(false);
  rtp_rtcp_->SetRTCPStatus(RtcpMode::kCompound);

  ingress_ = std::make_unique<AudioIngress>(rtp_rtcp_.get(), clock,
                                            receive_statistics_.get(),
                                            std::move(decoder_factory));
  egress_ =
      std::make_unique<AudioEgress>(rtp_rtcp_.get(), clock, task_queue_factory);

  audio_mixer_->AddSource(ingress_.get());
}

AudioChannel::~AudioChannel() {
  if (egress_->IsSending()) {
    StopSend();
  }
  if (ingress_->IsPlaying()) {
    StopPlay();
  }

  audio_mixer_->RemoveSource(ingress_.get());

  egress_.reset();
  ingress_.reset();
}

bool AudioChannel::StartSend() {

  if (!egress_->StartSend()) {
    return false;
  }

  if (!rtp_rtcp_->Sending()) {
    rtp_rtcp_->SetSendingStatus(true);
  }
  return true;
}

void AudioChannel::StopSend() {
  egress_->StopSend();

  if (!ingress_->IsPlaying() && rtp_rtcp_->Sending()) {
    rtp_rtcp_->SetSendingStatus(false);
  }
}

bool AudioChannel::StartPlay() {

  if (!ingress_->StartPlay()) {
    return false;
  }

  if (!rtp_rtcp_->Sending()) {
    rtp_rtcp_->SetSendingStatus(true);
  }
  return true;
}

void AudioChannel::StopPlay() {
  ingress_->StopPlay();

  if (!rtp_rtcp_->SendingMedia() && rtp_rtcp_->Sending()) {
    rtp_rtcp_->SetSendingStatus(false);
  }
}

IngressStatistics AudioChannel::GetIngressStatistics() {
  IngressStatistics ingress_stats;
  NetworkStatistics stats = ingress_->GetNetworkStatistics();
  ingress_stats.neteq_stats.total_samples_received = stats.totalSamplesReceived;
  ingress_stats.neteq_stats.concealed_samples = stats.concealedSamples;
  ingress_stats.neteq_stats.concealment_events = stats.concealmentEvents;
  ingress_stats.neteq_stats.jitter_buffer_delay_ms = stats.jitterBufferDelayMs;
  ingress_stats.neteq_stats.jitter_buffer_emitted_count =
      stats.jitterBufferEmittedCount;
  ingress_stats.neteq_stats.jitter_buffer_target_delay_ms =
      stats.jitterBufferTargetDelayMs;
  ingress_stats.neteq_stats.inserted_samples_for_deceleration =
      stats.insertedSamplesForDeceleration;
  ingress_stats.neteq_stats.removed_samples_for_acceleration =
      stats.removedSamplesForAcceleration;
  ingress_stats.neteq_stats.silent_concealed_samples =
      stats.silentConcealedSamples;
  ingress_stats.neteq_stats.fec_packets_received = stats.fecPacketsReceived;
  ingress_stats.neteq_stats.fec_packets_discarded = stats.fecPacketsDiscarded;
  ingress_stats.neteq_stats.delayed_packet_outage_samples =
      stats.delayedPacketOutageSamples;
  ingress_stats.neteq_stats.relative_packet_arrival_delay_ms =
      stats.relativePacketArrivalDelayMs;
  ingress_stats.neteq_stats.interruption_count = stats.interruptionCount;
  ingress_stats.neteq_stats.total_interruption_duration_ms =
      stats.totalInterruptionDurationMs;
  ingress_stats.total_duration = ingress_->GetOutputTotalDuration();
  return ingress_stats;
}

ChannelStatistics AudioChannel::GetChannelStatistics() {
  ChannelStatistics channel_stat = ingress_->GetChannelStatistics();

  StreamDataCounters rtp_stats, rtx_stats;
  rtp_rtcp_->GetSendStreamDataCounters(&rtp_stats, &rtx_stats);
  channel_stat.bytes_sent =
      rtp_stats.transmitted.payload_bytes + rtx_stats.transmitted.payload_bytes;
  channel_stat.packets_sent =
      rtp_stats.transmitted.packets + rtx_stats.transmitted.packets;

  return channel_stat;
}

}
