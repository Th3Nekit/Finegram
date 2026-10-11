/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "audio/channel_send.h"

#include <algorithm>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "api/array_view.h"
#include "api/call/transport.h"
#include "api/crypto/frame_encryptor_interface.h"
#include "api/rtc_event_log/rtc_event_log.h"
#include "api/sequence_checker.h"
#include "audio/channel_send_frame_transformer_delegate.h"
#include "audio/utility/audio_frame_operations.h"
#include "call/rtp_transport_controller_send_interface.h"
#include "logging/rtc_event_log/events/rtc_event_audio_playout.h"
#include "modules/audio_coding/audio_network_adaptor/include/audio_network_adaptor_config.h"
#include "modules/audio_coding/include/audio_coding_module.h"
#include "modules/audio_processing/rms_level.h"
#include "modules/pacing/packet_router.h"
#include "modules/rtp_rtcp/source/rtp_rtcp_impl2.h"
#include "rtc_base/checks.h"
#include "rtc_base/event.h"
#include "rtc_base/logging.h"
#include "rtc_base/numerics/safe_conversions.h"
#include "rtc_base/race_checker.h"
#include "rtc_base/rate_limiter.h"
#include "rtc_base/strings/string_builder.h"
#include "rtc_base/synchronization/mutex.h"
#include "rtc_base/system/no_unique_address.h"
#include "rtc_base/time_utils.h"
#include "rtc_base/trace_event.h"
#include "system_wrappers/include/clock.h"
#include "system_wrappers/include/metrics.h"

namespace webrtc {
namespace voe {

namespace {

constexpr int64_t kMaxRetransmissionWindowMs = 1000;
constexpr int64_t kMinRetransmissionWindowMs = 30;

class RtpPacketSenderProxy;
class TransportSequenceNumberProxy;

class ChannelSend : public ChannelSendInterface,
                    public AudioPacketizationCallback,

                    public RtcpPacketTypeCounterObserver,
                    public ReportBlockDataObserver {
 public:
  ChannelSend(Clock* clock,
              TaskQueueFactory* task_queue_factory,
              Transport* rtp_transport,
              RtcpRttStats* rtcp_rtt_stats,
              RtcEventLog* rtc_event_log,
              FrameEncryptorInterface* frame_encryptor,
              const webrtc::CryptoOptions& crypto_options,
              bool extmap_allow_mixed,
              int rtcp_report_interval_ms,
              uint32_t ssrc,
              rtc::scoped_refptr<FrameTransformerInterface> frame_transformer,
              RtpTransportControllerSendInterface* transport_controller,
              const FieldTrialsView& field_trials);

  ~ChannelSend() override;

  void SetEncoder(int payload_type,
                  const SdpAudioFormat& encoder_format,
                  std::unique_ptr<AudioEncoder> encoder) override;
  void ModifyEncoder(rtc::FunctionView<void(std::unique_ptr<AudioEncoder>*)>
                         modifier) override;
  void CallEncoder(rtc::FunctionView<void(AudioEncoder*)> modifier) override;

  void StartSend() override;
  void StopSend() override;

  void OnBitrateAllocation(BitrateAllocationUpdate update) override;
  int GetTargetBitrate() const override;

  void ReceivedRTCPPacket(const uint8_t* data, size_t length) override;

  void SetInputMute(bool enable) override;

  ANAStats GetANAStatistics() const override;

  RtpRtcpInterface* GetRtpRtcp() const override;

  void RegisterCngPayloadType(int payload_type, int payload_frequency) override;

  bool SendTelephoneEventOutband(int event, int duration_ms) override;
  void SetSendTelephoneEventPayloadType(int payload_type,
                                        int payload_frequency) override;

  void SetSendAudioLevelIndicationStatus(bool enable, int id) override;

  void RegisterSenderCongestionControlObjects(
      RtpTransportControllerSendInterface* transport) override;
  void ResetSenderCongestionControlObjects() override;
  void SetRTCP_CNAME(absl::string_view c_name) override;
  std::vector<ReportBlockData> GetRemoteRTCPReportBlocks() const override;
  CallSendStatistics GetRTCPStatistics() const override;

  void ProcessAndEncodeAudio(std::unique_ptr<AudioFrame> audio_frame) override;

  int64_t GetRTT() const override;

  void SetFrameEncryptor(
      rtc::scoped_refptr<FrameEncryptorInterface> frame_encryptor) override;

  void SetEncoderToPacketizerFrameTransformer(
      rtc::scoped_refptr<webrtc::FrameTransformerInterface> frame_transformer)
      override;

  void RtcpPacketTypesCounterUpdated(
      uint32_t ssrc,
      const RtcpPacketTypeCounter& packet_counter) override;

  void OnReportBlockDataUpdated(ReportBlockData report_block) override;

 private:

  int32_t SendData(AudioFrameType frameType,
                   uint8_t payloadType,
                   uint32_t rtp_timestamp,
                   const uint8_t* payloadData,
                   size_t payloadSize,
                   int64_t absolute_capture_timestamp_ms) override;

  bool InputMute() const;

  int32_t SendRtpAudio(AudioFrameType frameType,
                       uint8_t payloadType,
                       uint32_t rtp_timestamp_without_offset,
                       rtc::ArrayView<const uint8_t> payload,
                       int64_t absolute_capture_timestamp_ms,
                       rtc::ArrayView<const uint32_t> csrcs)
      RTC_RUN_ON(encoder_queue_checker_);

  void OnReceivedRtt(int64_t rtt_ms);

  void InitFrameTransformerDelegate(
      rtc::scoped_refptr<webrtc::FrameTransformerInterface> frame_transformer);

  RTC_NO_UNIQUE_ADDRESS SequenceChecker worker_thread_checker_;

  rtc::RaceChecker audio_thread_race_checker_;

  mutable Mutex volume_settings_mutex_;

  const uint32_t ssrc_;
  bool sending_ RTC_GUARDED_BY(&worker_thread_checker_) = false;

  RtcEventLog* const event_log_;

  std::unique_ptr<ModuleRtpRtcpImpl2> rtp_rtcp_;
  std::unique_ptr<RTPSenderAudio> rtp_sender_audio_;

  std::unique_ptr<AudioCodingModule> audio_coding_;

  uint32_t timestamp_ RTC_GUARDED_BY(audio_thread_race_checker_) = 0;
  absl::optional<int64_t> last_capture_timestamp_ms_
      RTC_GUARDED_BY(audio_thread_race_checker_);

  RmsLevel rms_level_ RTC_GUARDED_BY(encoder_queue_checker_);
  bool input_mute_ RTC_GUARDED_BY(volume_settings_mutex_) = false;
  bool previous_frame_muted_ RTC_GUARDED_BY(encoder_queue_checker_) = false;

  PacketRouter* packet_router_ RTC_GUARDED_BY(&worker_thread_checker_) =
      nullptr;
  const std::unique_ptr<RtpPacketSenderProxy> rtp_packet_pacer_proxy_;
  const std::unique_ptr<RateLimiter> retransmission_rate_limiter_;

  RTC_NO_UNIQUE_ADDRESS SequenceChecker construction_thread_;

  std::atomic<bool> include_audio_level_indication_ = false;
  std::atomic<bool> encoder_queue_is_active_ = false;
  std::atomic<bool> first_frame_ = true;

  rtc::scoped_refptr<FrameEncryptorInterface> frame_encryptor_
      RTC_GUARDED_BY(encoder_queue_checker_);

  const webrtc::CryptoOptions crypto_options_;

  rtc::scoped_refptr<ChannelSendFrameTransformerDelegate>
      frame_transformer_delegate_ RTC_GUARDED_BY(encoder_queue_checker_);

  mutable Mutex rtcp_counter_mutex_;
  RtcpPacketTypeCounter rtcp_packet_type_counter_
      RTC_GUARDED_BY(rtcp_counter_mutex_);

  std::unique_ptr<TaskQueueBase, TaskQueueDeleter> encoder_queue_;
  RTC_NO_UNIQUE_ADDRESS SequenceChecker encoder_queue_checker_;

  SdpAudioFormat encoder_format_;
};

const int kTelephoneEventAttenuationdB = 10;

class RtpPacketSenderProxy : public RtpPacketSender {
 public:
  RtpPacketSenderProxy() : rtp_packet_pacer_(nullptr) {}

  void SetPacketPacer(RtpPacketSender* rtp_packet_pacer) {
    RTC_DCHECK(thread_checker_.IsCurrent());
    MutexLock lock(&mutex_);
    rtp_packet_pacer_ = rtp_packet_pacer;
  }

  void EnqueuePackets(
      std::vector<std::unique_ptr<RtpPacketToSend>> packets) override {
    MutexLock lock(&mutex_);
    rtp_packet_pacer_->EnqueuePackets(std::move(packets));
  }

  void RemovePacketsForSsrc(uint32_t ssrc) override {
    MutexLock lock(&mutex_);
    rtp_packet_pacer_->RemovePacketsForSsrc(ssrc);
  }

 private:
  RTC_NO_UNIQUE_ADDRESS SequenceChecker thread_checker_;
  Mutex mutex_;
  RtpPacketSender* rtp_packet_pacer_ RTC_GUARDED_BY(&mutex_);
};

int32_t ChannelSend::SendData(AudioFrameType frameType,
                              uint8_t payloadType,
                              uint32_t rtp_timestamp,
                              const uint8_t* payloadData,
                              size_t payloadSize,
                              int64_t absolute_capture_timestamp_ms) {
  RTC_DCHECK_RUN_ON(&encoder_queue_checker_);
  rtc::ArrayView<const uint8_t> payload(payloadData, payloadSize);
  if (frame_transformer_delegate_) {

    char buf[1024];
    rtc::SimpleStringBuilder mime_type(buf);
    mime_type << MediaTypeToString(cricket::MEDIA_TYPE_AUDIO) << "/"
              << encoder_format_.name;
    frame_transformer_delegate_->Transform(
        frameType, payloadType, rtp_timestamp + rtp_rtcp_->StartTimestamp(),
        payloadData, payloadSize, absolute_capture_timestamp_ms,
        rtp_rtcp_->SSRC(), mime_type.str());
    return 0;
  }
  return SendRtpAudio(frameType, payloadType, rtp_timestamp, payload,
                      absolute_capture_timestamp_ms,           {});
}

int32_t ChannelSend::SendRtpAudio(AudioFrameType frameType,
                                  uint8_t payloadType,
                                  uint32_t rtp_timestamp_without_offset,
                                  rtc::ArrayView<const uint8_t> payload,
                                  int64_t absolute_capture_timestamp_ms,
                                  rtc::ArrayView<const uint32_t> csrcs) {

  rtc::Buffer encrypted_audio_payload;

  if (!payload.empty()) {
    if (frame_encryptor_ != nullptr) {

      size_t max_ciphertext_size = frame_encryptor_->GetMaxCiphertextByteSize(
          cricket::MEDIA_TYPE_AUDIO, payload.size());
      encrypted_audio_payload.SetSize(max_ciphertext_size);

      size_t bytes_written = 0;
      int encrypt_status = frame_encryptor_->Encrypt(
          cricket::MEDIA_TYPE_AUDIO, rtp_rtcp_->SSRC(),
                              nullptr, payload, encrypted_audio_payload,
          &bytes_written);
      if (encrypt_status != 0) {
        RTC_DLOG(LS_ERROR)
            << "Channel::SendData() failed encrypt audio payload: "
            << encrypt_status;
        return -1;
      }

      encrypted_audio_payload.SetSize(bytes_written);

      payload = encrypted_audio_payload;
    } else if (crypto_options_.sframe.require_frame_encryption) {
      RTC_DLOG(LS_ERROR) << "Channel::SendData() failed sending audio payload: "
                            "A frame encryptor is required but one is not set.";
      return -1;
    }
  }

  if (!rtp_rtcp_->OnSendingRtpFrame(rtp_timestamp_without_offset,

                                    -1, payloadType,
                                                            false)) {
    return -1;
  }

  RTPSenderAudio::RtpAudioFrame frame = {
      .type = frameType,
      .payload = payload,
      .payload_id = payloadType,
      .rtp_timestamp =
          rtp_timestamp_without_offset + rtp_rtcp_->StartTimestamp(),
      .csrcs = csrcs};
  if (absolute_capture_timestamp_ms > 0) {
    frame.capture_time = Timestamp::Millis(absolute_capture_timestamp_ms);
  }
  if (include_audio_level_indication_.load()) {
    frame.audio_level_dbov = rms_level_.Average();
  }
  if (!rtp_sender_audio_->SendAudio(frame)) {
    RTC_DLOG(LS_ERROR)
        << "ChannelSend::SendData() failed to send data to RTP/RTCP module";
    return -1;
  }

  return 0;
}

ChannelSend::ChannelSend(
    Clock* clock,
    TaskQueueFactory* task_queue_factory,
    Transport* rtp_transport,
    RtcpRttStats* rtcp_rtt_stats,
    RtcEventLog* rtc_event_log,
    FrameEncryptorInterface* frame_encryptor,
    const webrtc::CryptoOptions& crypto_options,
    bool extmap_allow_mixed,
    int rtcp_report_interval_ms,
    uint32_t ssrc,
    rtc::scoped_refptr<FrameTransformerInterface> frame_transformer,
    RtpTransportControllerSendInterface* transport_controller,
    const FieldTrialsView& field_trials)
    : ssrc_(ssrc),
      event_log_(rtc_event_log),
      rtp_packet_pacer_proxy_(new RtpPacketSenderProxy()),
      retransmission_rate_limiter_(
          new RateLimiter(clock, kMaxRetransmissionWindowMs)),
      frame_encryptor_(frame_encryptor),
      crypto_options_(crypto_options),
      encoder_queue_(task_queue_factory->CreateTaskQueue(
          "AudioEncoder",
          TaskQueueFactory::Priority::NORMAL)),
      encoder_queue_checker_(encoder_queue_.get()),
      encoder_format_("x-unknown", 0, 0) {
  audio_coding_ = AudioCodingModule::Create();

  RtpRtcpInterface::Configuration configuration;
  configuration.report_block_data_observer = this;
  configuration.network_link_rtcp_observer =
      transport_controller->GetRtcpObserver();
  configuration.transport_feedback_callback =
      transport_controller->transport_feedback_observer();
  configuration.clock = (clock ? clock : Clock::GetRealTimeClock());
  configuration.audio = true;
  configuration.outgoing_transport = rtp_transport;

  configuration.paced_sender = rtp_packet_pacer_proxy_.get();

  configuration.event_log = event_log_;
  configuration.rtt_stats = rtcp_rtt_stats;
  if (field_trials.IsDisabled("WebRTC-DisableRtxRateLimiter")) {
    configuration.retransmission_rate_limiter =
        retransmission_rate_limiter_.get();
  }
  configuration.extmap_allow_mixed = extmap_allow_mixed;
  configuration.rtcp_report_interval_ms = rtcp_report_interval_ms;
  configuration.rtcp_packet_type_counter_observer = this;

  configuration.local_media_ssrc = ssrc;

  rtp_rtcp_ = ModuleRtpRtcpImpl2::Create(configuration);
  rtp_rtcp_->SetSendingMediaStatus(false);

  rtp_sender_audio_ = std::make_unique<RTPSenderAudio>(configuration.clock,
                                                       rtp_rtcp_->RtpSender());

  rtp_rtcp_->SetRTCPStatus(RtcpMode::kCompound);

  int error = audio_coding_->RegisterTransportCallback(this);
  RTC_DCHECK_EQ(0, error);
  if (frame_transformer)
    InitFrameTransformerDelegate(std::move(frame_transformer));
}

ChannelSend::~ChannelSend() {
  RTC_DCHECK(construction_thread_.IsCurrent());

  if (frame_transformer_delegate_)
    frame_transformer_delegate_->Reset();

  StopSend();
  int error = audio_coding_->RegisterTransportCallback(NULL);
  RTC_DCHECK_EQ(0, error);

  encoder_queue_ = nullptr;
}

void ChannelSend::StartSend() {
  RTC_DCHECK_RUN_ON(&worker_thread_checker_);
  RTC_DCHECK(!sending_);
  sending_ = true;

  RTC_DCHECK(packet_router_);
  packet_router_->AddSendRtpModule(rtp_rtcp_.get(),                    false);
  rtp_rtcp_->SetSendingMediaStatus(true);
  int ret = rtp_rtcp_->SetSendingStatus(true);
  RTC_DCHECK_EQ(0, ret);

  first_frame_.store(true);
  encoder_queue_is_active_.store(true);
}

void ChannelSend::StopSend() {
  RTC_DCHECK_RUN_ON(&worker_thread_checker_);
  if (!sending_) {
    return;
  }
  sending_ = false;
  encoder_queue_is_active_.store(false);

  rtc::Event flush;
  encoder_queue_->PostTask([this, &flush]() {
    RTC_DCHECK_RUN_ON(&encoder_queue_checker_);
    CallEncoder([](AudioEncoder* encoder) { encoder->Reset(); });
    flush.Set();
  });
  flush.Wait(rtc::Event::kForever);

  if (rtp_rtcp_->SetSendingStatus(false) == -1) {
    RTC_DLOG(LS_ERROR) << "StartSend() RTP/RTCP failed to stop sending";
  }
  rtp_rtcp_->SetSendingMediaStatus(false);

  RTC_DCHECK(packet_router_);
  packet_router_->RemoveSendRtpModule(rtp_rtcp_.get());
  rtp_packet_pacer_proxy_->RemovePacketsForSsrc(rtp_rtcp_->SSRC());
}

void ChannelSend::SetEncoder(int payload_type,
                             const SdpAudioFormat& encoder_format,
                             std::unique_ptr<AudioEncoder> encoder) {
  RTC_DCHECK_RUN_ON(&worker_thread_checker_);
  RTC_DCHECK_GE(payload_type, 0);
  RTC_DCHECK_LE(payload_type, 127);

  rtp_rtcp_->RegisterSendPayloadFrequency(payload_type,
                                          encoder->RtpTimestampRateHz());
  rtp_sender_audio_->RegisterAudioPayload("audio", payload_type,
                                          encoder->RtpTimestampRateHz(),
                                          encoder->NumChannels(), 0);

  encoder_format_ = encoder_format;
  audio_coding_->SetEncoder(std::move(encoder));
}

void ChannelSend::ModifyEncoder(
    rtc::FunctionView<void(std::unique_ptr<AudioEncoder>*)> modifier) {

  audio_coding_->ModifyEncoder(modifier);
}

void ChannelSend::CallEncoder(rtc::FunctionView<void(AudioEncoder*)> modifier) {
  ModifyEncoder([modifier](std::unique_ptr<AudioEncoder>* encoder_ptr) {
    if (*encoder_ptr) {
      modifier(encoder_ptr->get());
    } else {
      RTC_DLOG(LS_WARNING) << "Trying to call unset encoder.";
    }
  });
}

void ChannelSend::OnBitrateAllocation(BitrateAllocationUpdate update) {

  CallEncoder([&](AudioEncoder* encoder) {
    encoder->OnReceivedUplinkAllocation(update);
  });
  retransmission_rate_limiter_->SetMaxRate(update.target_bitrate.bps());
}

int ChannelSend::GetTargetBitrate() const {
  return audio_coding_->GetTargetBitrate();
}

void ChannelSend::OnReportBlockDataUpdated(ReportBlockData report_block) {
  float packet_loss_rate = report_block.fraction_lost();
  CallEncoder([&](AudioEncoder* encoder) {
    encoder->OnReceivedUplinkPacketLossFraction(packet_loss_rate);
  });
}

void ChannelSend::ReceivedRTCPPacket(const uint8_t* data, size_t length) {
  RTC_DCHECK_RUN_ON(&worker_thread_checker_);

  rtp_rtcp_->IncomingRtcpPacket(rtc::MakeArrayView(data, length));

  int64_t rtt = GetRTT();
  if (rtt == 0) {

    return;
  }

  int64_t nack_window_ms = rtt;
  if (nack_window_ms < kMinRetransmissionWindowMs) {
    nack_window_ms = kMinRetransmissionWindowMs;
  } else if (nack_window_ms > kMaxRetransmissionWindowMs) {
    nack_window_ms = kMaxRetransmissionWindowMs;
  }
  retransmission_rate_limiter_->SetWindowSize(nack_window_ms);

  OnReceivedRtt(rtt);
}

void ChannelSend::SetInputMute(bool enable) {
  RTC_DCHECK_RUN_ON(&worker_thread_checker_);
  MutexLock lock(&volume_settings_mutex_);
  input_mute_ = enable;
}

bool ChannelSend::InputMute() const {
  MutexLock lock(&volume_settings_mutex_);
  return input_mute_;
}

bool ChannelSend::SendTelephoneEventOutband(int event, int duration_ms) {
  RTC_DCHECK_RUN_ON(&worker_thread_checker_);
  RTC_DCHECK_LE(0, event);
  RTC_DCHECK_GE(255, event);
  RTC_DCHECK_LE(0, duration_ms);
  RTC_DCHECK_GE(65535, duration_ms);
  if (!sending_) {
    return false;
  }
  if (rtp_sender_audio_->SendTelephoneEvent(
          event, duration_ms, kTelephoneEventAttenuationdB) != 0) {
    RTC_DLOG(LS_ERROR) << "SendTelephoneEvent() failed to send event";
    return false;
  }
  return true;
}

void ChannelSend::RegisterCngPayloadType(int payload_type,
                                         int payload_frequency) {
  rtp_rtcp_->RegisterSendPayloadFrequency(payload_type, payload_frequency);
  rtp_sender_audio_->RegisterAudioPayload("CN", payload_type, payload_frequency,
                                          1, 0);
}

void ChannelSend::SetSendTelephoneEventPayloadType(int payload_type,
                                                   int payload_frequency) {
  RTC_DCHECK_RUN_ON(&worker_thread_checker_);
  RTC_DCHECK_LE(0, payload_type);
  RTC_DCHECK_GE(127, payload_type);
  rtp_rtcp_->RegisterSendPayloadFrequency(payload_type, payload_frequency);
  rtp_sender_audio_->RegisterAudioPayload("telephone-event", payload_type,
                                          payload_frequency, 0, 0);
}

void ChannelSend::SetSendAudioLevelIndicationStatus(bool enable, int id) {
  RTC_DCHECK_RUN_ON(&worker_thread_checker_);
  include_audio_level_indication_.store(enable);
  if (enable) {
    rtp_rtcp_->RegisterRtpHeaderExtension(AudioLevel::Uri(), id);
  } else {
    rtp_rtcp_->DeregisterSendRtpHeaderExtension(AudioLevel::Uri());
  }
}

void ChannelSend::RegisterSenderCongestionControlObjects(
    RtpTransportControllerSendInterface* transport) {
  RTC_DCHECK_RUN_ON(&worker_thread_checker_);
  RtpPacketSender* rtp_packet_pacer = transport->packet_sender();
  PacketRouter* packet_router = transport->packet_router();

  RTC_DCHECK(rtp_packet_pacer);
  RTC_DCHECK(packet_router);
  RTC_DCHECK(!packet_router_);
  rtp_packet_pacer_proxy_->SetPacketPacer(rtp_packet_pacer);
  rtp_rtcp_->SetStorePacketsStatus(true, 600);
  packet_router_ = packet_router;
}

void ChannelSend::ResetSenderCongestionControlObjects() {
  RTC_DCHECK_RUN_ON(&worker_thread_checker_);
  RTC_DCHECK(packet_router_);
  rtp_rtcp_->SetStorePacketsStatus(false, 600);
  packet_router_ = nullptr;
  rtp_packet_pacer_proxy_->SetPacketPacer(nullptr);
}

void ChannelSend::SetRTCP_CNAME(absl::string_view c_name) {
  RTC_DCHECK_RUN_ON(&worker_thread_checker_);

  const std::string c_name_limited(c_name.substr(0, 255));
  int ret = rtp_rtcp_->SetCNAME(c_name_limited.c_str()) != 0;
  RTC_DCHECK_EQ(0, ret) << "SetRTCP_CNAME() failed to set RTCP CNAME";
}

std::vector<ReportBlockData> ChannelSend::GetRemoteRTCPReportBlocks() const {
  RTC_DCHECK_RUN_ON(&worker_thread_checker_);

  return rtp_rtcp_->GetLatestReportBlockData();
}

CallSendStatistics ChannelSend::GetRTCPStatistics() const {
  RTC_DCHECK_RUN_ON(&worker_thread_checker_);
  CallSendStatistics stats = {0};
  stats.rttMs = GetRTT();

  StreamDataCounters rtp_stats;
  StreamDataCounters rtx_stats;
  rtp_rtcp_->GetSendStreamDataCounters(&rtp_stats, &rtx_stats);
  stats.payload_bytes_sent =
      rtp_stats.transmitted.payload_bytes + rtx_stats.transmitted.payload_bytes;
  stats.header_and_padding_bytes_sent =
      rtp_stats.transmitted.padding_bytes + rtp_stats.transmitted.header_bytes +
      rtx_stats.transmitted.padding_bytes + rtx_stats.transmitted.header_bytes;

  stats.retransmitted_bytes_sent = rtp_stats.retransmitted.payload_bytes;
  stats.packetsSent =
      rtp_stats.transmitted.packets + rtx_stats.transmitted.packets;
  stats.total_packet_send_delay = rtp_stats.transmitted.total_packet_delay;
  stats.retransmitted_packets_sent = rtp_stats.retransmitted.packets;
  stats.report_block_datas = rtp_rtcp_->GetLatestReportBlockData();

  {
    MutexLock lock(&rtcp_counter_mutex_);
    stats.nacks_received = rtcp_packet_type_counter_.nack_packets;
  }

  return stats;
}

void ChannelSend::RtcpPacketTypesCounterUpdated(
    uint32_t ssrc,
    const RtcpPacketTypeCounter& packet_counter) {
  if (ssrc != ssrc_) {
    return;
  }
  MutexLock lock(&rtcp_counter_mutex_);
  rtcp_packet_type_counter_ = packet_counter;
}

void ChannelSend::ProcessAndEncodeAudio(
    std::unique_ptr<AudioFrame> audio_frame) {
  TRACE_EVENT0("webrtc", "ChannelSend::ProcessAndEncodeAudio");

  RTC_DCHECK_RUNS_SERIALIZED(&audio_thread_race_checker_);
  RTC_DCHECK_GT(audio_frame->samples_per_channel_, 0);
  RTC_DCHECK_LE(audio_frame->num_channels_, 8);

  if (!encoder_queue_is_active_.load()) {
    return;
  }

  if (first_frame_.load()) {
    first_frame_.store(false);
    if (last_capture_timestamp_ms_ &&
        audio_frame->absolute_capture_timestamp_ms()) {
      int64_t diff_ms = *audio_frame->absolute_capture_timestamp_ms() -
                        *last_capture_timestamp_ms_;

      int64_t diff_frames = diff_ms * audio_frame->sample_rate_hz() / 1000 /
                                audio_frame->samples_per_channel() -
                            1;
      timestamp_ += std::max<int64_t>(
          diff_frames * audio_frame->samples_per_channel(), 0);
    }
  }

  audio_frame->timestamp_ = timestamp_;
  timestamp_ += audio_frame->samples_per_channel_;
  last_capture_timestamp_ms_ = audio_frame->absolute_capture_timestamp_ms();

  audio_frame->UpdateProfileTimeStamp();
  encoder_queue_->PostTask(
      [this, audio_frame = std::move(audio_frame)]() mutable {
        RTC_DCHECK_RUN_ON(&encoder_queue_checker_);
        if (!encoder_queue_is_active_.load()) {
          return;
        }

        RTC_HISTOGRAM_COUNTS_10000("WebRTC.Audio.EncodingTaskQueueLatencyMs",
                                   audio_frame->ElapsedProfileTimeMs());

        bool is_muted = InputMute();
        AudioFrameOperations::Mute(audio_frame.get(), previous_frame_muted_,
                                   is_muted);

        if (include_audio_level_indication_.load()) {
          size_t length =
              audio_frame->samples_per_channel_ * audio_frame->num_channels_;
          RTC_CHECK_LE(length, AudioFrame::kMaxDataSizeBytes);
          if (is_muted && previous_frame_muted_) {
            rms_level_.AnalyzeMuted(length);
          } else {
            rms_level_.Analyze(
                rtc::ArrayView<const int16_t>(audio_frame->data(), length));
          }
        }
        previous_frame_muted_ = is_muted;

        if (audio_coding_->Add10MsData(*audio_frame) < 0) {
          RTC_DLOG(LS_ERROR) << "ACM::Add10MsData() failed.";
          return;
        }
      });
}

ANAStats ChannelSend::GetANAStatistics() const {
  RTC_DCHECK_RUN_ON(&worker_thread_checker_);
  return audio_coding_->GetANAStats();
}

RtpRtcpInterface* ChannelSend::GetRtpRtcp() const {
  return rtp_rtcp_.get();
}

int64_t ChannelSend::GetRTT() const {
  std::vector<ReportBlockData> report_blocks =
      rtp_rtcp_->GetLatestReportBlockData();
  if (report_blocks.empty()) {
    return 0;
  }

  return report_blocks.front().last_rtt().ms();
}

void ChannelSend::SetFrameEncryptor(
    rtc::scoped_refptr<FrameEncryptorInterface> frame_encryptor) {
  RTC_DCHECK_RUN_ON(&worker_thread_checker_);
  encoder_queue_->PostTask([this, frame_encryptor]() mutable {
    RTC_DCHECK_RUN_ON(&encoder_queue_checker_);
    frame_encryptor_ = std::move(frame_encryptor);
  });
}

void ChannelSend::SetEncoderToPacketizerFrameTransformer(
    rtc::scoped_refptr<webrtc::FrameTransformerInterface> frame_transformer) {
  RTC_DCHECK_RUN_ON(&worker_thread_checker_);
  if (!frame_transformer)
    return;

  encoder_queue_->PostTask(
      [this, frame_transformer = std::move(frame_transformer)]() mutable {
        RTC_DCHECK_RUN_ON(&encoder_queue_checker_);
        InitFrameTransformerDelegate(std::move(frame_transformer));
      });
}

void ChannelSend::OnReceivedRtt(int64_t rtt_ms) {

  CallEncoder(
      [rtt_ms](AudioEncoder* encoder) { encoder->OnReceivedRtt(rtt_ms); });
}

void ChannelSend::InitFrameTransformerDelegate(
    rtc::scoped_refptr<webrtc::FrameTransformerInterface> frame_transformer) {
  RTC_DCHECK_RUN_ON(&encoder_queue_checker_);
  RTC_DCHECK(frame_transformer);
  RTC_DCHECK(!frame_transformer_delegate_);

  ChannelSendFrameTransformerDelegate::SendFrameCallback send_audio_callback =
      [this](AudioFrameType frameType, uint8_t payloadType,
             uint32_t rtp_timestamp_with_offset,
             rtc::ArrayView<const uint8_t> payload,
             int64_t absolute_capture_timestamp_ms,
             rtc::ArrayView<const uint32_t> csrcs) {
        RTC_DCHECK_RUN_ON(&encoder_queue_checker_);
        return SendRtpAudio(
            frameType, payloadType,
            rtp_timestamp_with_offset - rtp_rtcp_->StartTimestamp(), payload,
            absolute_capture_timestamp_ms, csrcs);
      };
  frame_transformer_delegate_ =
      rtc::make_ref_counted<ChannelSendFrameTransformerDelegate>(
          std::move(send_audio_callback), std::move(frame_transformer),
          encoder_queue_.get());
  frame_transformer_delegate_->Init();
}

}

std::unique_ptr<ChannelSendInterface> CreateChannelSend(
    Clock* clock,
    TaskQueueFactory* task_queue_factory,
    Transport* rtp_transport,
    RtcpRttStats* rtcp_rtt_stats,
    RtcEventLog* rtc_event_log,
    FrameEncryptorInterface* frame_encryptor,
    const webrtc::CryptoOptions& crypto_options,
    bool extmap_allow_mixed,
    int rtcp_report_interval_ms,
    uint32_t ssrc,
    rtc::scoped_refptr<FrameTransformerInterface> frame_transformer,
    RtpTransportControllerSendInterface* transport_controller,
    const FieldTrialsView& field_trials) {
  return std::make_unique<ChannelSend>(
      clock, task_queue_factory, rtp_transport, rtcp_rtt_stats, rtc_event_log,
      frame_encryptor, crypto_options, extmap_allow_mixed,
      rtcp_report_interval_ms, ssrc, std::move(frame_transformer),
      transport_controller, field_trials);
}

}
}
