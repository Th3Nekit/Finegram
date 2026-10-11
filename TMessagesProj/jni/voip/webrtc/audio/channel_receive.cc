/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "audio/channel_receive.h"

#include <algorithm>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "api/crypto/frame_decryptor_interface.h"
#include "api/frame_transformer_interface.h"
#include "api/rtc_event_log/rtc_event_log.h"
#include "api/sequence_checker.h"
#include "api/task_queue/pending_task_safety_flag.h"
#include "api/task_queue/task_queue_base.h"
#include "api/units/time_delta.h"
#include "audio/audio_level.h"
#include "audio/channel_receive_frame_transformer_delegate.h"
#include "audio/channel_send.h"
#include "audio/utility/audio_frame_operations.h"
#include "logging/rtc_event_log/events/rtc_event_audio_playout.h"
#include "logging/rtc_event_log/events/rtc_event_neteq_set_minimum_delay.h"
#include "modules/audio_coding/acm2/acm_receiver.h"
#include "modules/audio_coding/audio_network_adaptor/include/audio_network_adaptor_config.h"
#include "modules/audio_device/include/audio_device.h"
#include "modules/pacing/packet_router.h"
#include "modules/rtp_rtcp/include/receive_statistics.h"
#include "modules/rtp_rtcp/include/remote_ntp_time_estimator.h"
#include "modules/rtp_rtcp/source/absolute_capture_time_interpolator.h"
#include "modules/rtp_rtcp/source/capture_clock_offset_updater.h"
#include "modules/rtp_rtcp/source/rtp_header_extensions.h"
#include "modules/rtp_rtcp/source/rtp_packet_received.h"
#include "modules/rtp_rtcp/source/rtp_rtcp_config.h"
#include "modules/rtp_rtcp/source/rtp_rtcp_impl2.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/numerics/safe_minmax.h"
#include "rtc_base/numerics/sequence_number_unwrapper.h"
#include "rtc_base/race_checker.h"
#include "rtc_base/strings/string_builder.h"
#include "rtc_base/synchronization/mutex.h"
#include "rtc_base/system/no_unique_address.h"
#include "rtc_base/time_utils.h"
#include "rtc_base/trace_event.h"
#include "system_wrappers/include/metrics.h"
#include "system_wrappers/include/ntp_time.h"

namespace webrtc {
namespace voe {

namespace {

constexpr double kAudioSampleDurationSeconds = 0.01;

constexpr int kVoiceEngineMinMinPlayoutDelayMs = 0;
constexpr int kVoiceEngineMaxMinPlayoutDelayMs = 10000;

acm2::AcmReceiver::Config AcmConfig(
    NetEqFactory* neteq_factory,
    rtc::scoped_refptr<AudioDecoderFactory> decoder_factory,
    absl::optional<AudioCodecPairId> codec_pair_id,
    size_t jitter_buffer_max_packets,
    bool jitter_buffer_fast_playout,
    int jitter_buffer_min_delay_ms) {
  acm2::AcmReceiver::Config acm_config;
  acm_config.neteq_factory = neteq_factory;
  acm_config.decoder_factory = decoder_factory;
  acm_config.neteq_config.codec_pair_id = codec_pair_id;
  acm_config.neteq_config.max_packets_in_buffer = jitter_buffer_max_packets;
  acm_config.neteq_config.enable_fast_accelerate = jitter_buffer_fast_playout;
  acm_config.neteq_config.enable_muted_state = true;
  acm_config.neteq_config.min_delay_ms = jitter_buffer_min_delay_ms;

  return acm_config;
}

class ChannelReceive : public ChannelReceiveInterface,
                       public RtcpPacketTypeCounterObserver {
 public:

  ChannelReceive(
      Clock* clock,
      NetEqFactory* neteq_factory,
      AudioDeviceModule* audio_device_module,
      Transport* rtcp_send_transport,
      RtcEventLog* rtc_event_log,
      uint32_t local_ssrc,
      uint32_t remote_ssrc,
      size_t jitter_buffer_max_packets,
      bool jitter_buffer_fast_playout,
      int jitter_buffer_min_delay_ms,
      bool enable_non_sender_rtt,
      rtc::scoped_refptr<AudioDecoderFactory> decoder_factory,
      absl::optional<AudioCodecPairId> codec_pair_id,
      rtc::scoped_refptr<FrameDecryptorInterface> frame_decryptor,
      const webrtc::CryptoOptions& crypto_options,
      rtc::scoped_refptr<FrameTransformerInterface> frame_transformer);
  ~ChannelReceive() override;

  void SetSink(AudioSinkInterface* sink) override;

  void SetReceiveCodecs(const std::map<int, SdpAudioFormat>& codecs) override;

  void StartPlayout() override;
  void StopPlayout() override;

  absl::optional<std::pair<int, SdpAudioFormat>> GetReceiveCodec()
      const override;

  void ReceivedRTCPPacket(const uint8_t* data, size_t length) override;

  void OnRtpPacket(const RtpPacketReceived& packet) override;

  void SetChannelOutputVolumeScaling(float scaling) override;
  int GetSpeechOutputLevelFullRange() const override;

  double GetTotalOutputEnergy() const override;
  double GetTotalOutputDuration() const override;

  NetworkStatistics GetNetworkStatistics(
      bool get_and_clear_legacy_stats) const override;
  AudioDecodingCallStats GetDecodingCallStatistics() const override;

  uint32_t GetDelayEstimate() const override;
  bool SetMinimumPlayoutDelay(int delayMs) override;
  bool GetPlayoutRtpTimestamp(uint32_t* rtp_timestamp,
                              int64_t* time_ms) const override;
  void SetEstimatedPlayoutNtpTimestampMs(int64_t ntp_timestamp_ms,
                                         int64_t time_ms) override;
  absl::optional<int64_t> GetCurrentEstimatedPlayoutNtpTimestampMs(
      int64_t now_ms) const override;

  bool SetBaseMinimumPlayoutDelayMs(int delay_ms) override;
  int GetBaseMinimumPlayoutDelayMs() const override;

  absl::optional<Syncable::Info> GetSyncInfo() const override;

  void RegisterReceiverCongestionControlObjects(
      PacketRouter* packet_router) override;
  void ResetReceiverCongestionControlObjects() override;

  CallReceiveStatistics GetRTCPStatistics() const override;
  void SetNACKStatus(bool enable, int maxNumberOfPackets) override;
  void SetNonSenderRttMeasurement(bool enabled) override;

  AudioMixer::Source::AudioFrameInfo GetAudioFrameWithInfo(
      int sample_rate_hz,
      AudioFrame* audio_frame) override;

  int PreferredSampleRate() const override;

  void SetSourceTracker(SourceTracker* source_tracker) override;

  void SetAssociatedSendChannel(const ChannelSendInterface* channel) override;

  void SetDepacketizerToDecoderFrameTransformer(
      rtc::scoped_refptr<webrtc::FrameTransformerInterface> frame_transformer)
      override;

  void SetFrameDecryptor(rtc::scoped_refptr<webrtc::FrameDecryptorInterface>
                             frame_decryptor) override;

  void OnLocalSsrcChange(uint32_t local_ssrc) override;
  uint32_t GetLocalSsrc() const override;

  void RtcpPacketTypesCounterUpdated(
      uint32_t ssrc,
      const RtcpPacketTypeCounter& packet_counter) override;

 private:
  void ReceivePacket(const uint8_t* packet,
                     size_t packet_length,
                     const RTPHeader& header)
      RTC_RUN_ON(worker_thread_checker_);
  int ResendPackets(const uint16_t* sequence_numbers, int length);
  void UpdatePlayoutTimestamp(bool rtcp, int64_t now_ms)
      RTC_RUN_ON(worker_thread_checker_);

  int GetRtpTimestampRateHz() const;

  void OnReceivedPayloadData(rtc::ArrayView<const uint8_t> payload,
                             const RTPHeader& rtpHeader)
      RTC_RUN_ON(worker_thread_checker_);

  void InitFrameTransformerDelegate(
      rtc::scoped_refptr<webrtc::FrameTransformerInterface> frame_transformer)
      RTC_RUN_ON(worker_thread_checker_);

  RTC_NO_UNIQUE_ADDRESS SequenceChecker worker_thread_checker_;
  RTC_NO_UNIQUE_ADDRESS SequenceChecker network_thread_checker_;

  TaskQueueBase* const worker_thread_;
  ScopedTaskSafety worker_safety_;

  rtc::RaceChecker audio_thread_race_checker_;
  Mutex callback_mutex_;
  Mutex volume_settings_mutex_;

  bool playing_ RTC_GUARDED_BY(worker_thread_checker_) = false;

  RtcEventLog* const event_log_;

  std::map<uint8_t, int> payload_type_frequencies_;

  std::unique_ptr<ReceiveStatistics> rtp_receive_statistics_;
  std::unique_ptr<ModuleRtpRtcpImpl2> rtp_rtcp_;
  const uint32_t remote_ssrc_;
  SourceTracker* source_tracker_ = nullptr;

  absl::optional<uint32_t> last_received_rtp_timestamp_
      RTC_GUARDED_BY(&worker_thread_checker_);
  absl::optional<int64_t> last_received_rtp_system_time_ms_
      RTC_GUARDED_BY(&worker_thread_checker_);

  acm2::AcmReceiver acm_receiver_;
  AudioSinkInterface* audio_sink_ = nullptr;
  AudioLevel _outputAudioLevel;

  Clock* const clock_;
  RemoteNtpTimeEstimator ntp_estimator_ RTC_GUARDED_BY(ts_stats_lock_);

  absl::optional<uint32_t> jitter_buffer_playout_timestamp_;

  uint32_t playout_timestamp_rtp_ RTC_GUARDED_BY(worker_thread_checker_);
  absl::optional<int64_t> playout_timestamp_rtp_time_ms_
      RTC_GUARDED_BY(worker_thread_checker_);
  uint32_t playout_delay_ms_ RTC_GUARDED_BY(worker_thread_checker_);
  absl::optional<int64_t> playout_timestamp_ntp_
      RTC_GUARDED_BY(worker_thread_checker_);
  absl::optional<int64_t> playout_timestamp_ntp_time_ms_
      RTC_GUARDED_BY(worker_thread_checker_);

  mutable Mutex ts_stats_lock_;

  webrtc::RtpTimestampUnwrapper rtp_ts_wraparound_handler_;

  int64_t capture_start_rtp_time_stamp_;

  int64_t capture_start_ntp_time_ms_ RTC_GUARDED_BY(ts_stats_lock_);

  AudioDeviceModule* _audioDeviceModulePtr;
  float _outputGain RTC_GUARDED_BY(volume_settings_mutex_);

  const ChannelSendInterface* associated_send_channel_
      RTC_GUARDED_BY(network_thread_checker_);

  PacketRouter* packet_router_ = nullptr;

  SequenceChecker construction_thread_;

  rtc::scoped_refptr<FrameDecryptorInterface> frame_decryptor_
      RTC_GUARDED_BY(worker_thread_checker_);
  webrtc::CryptoOptions crypto_options_;

  webrtc::AbsoluteCaptureTimeInterpolator absolute_capture_time_interpolator_
      RTC_GUARDED_BY(worker_thread_checker_);

  webrtc::CaptureClockOffsetUpdater capture_clock_offset_updater_
      RTC_GUARDED_BY(ts_stats_lock_);

  rtc::scoped_refptr<ChannelReceiveFrameTransformerDelegate>
      frame_transformer_delegate_;

  int audio_frame_interval_count_ RTC_GUARDED_BY(audio_thread_race_checker_) =
      0;

  constexpr static int kHistogramReportingInterval = 100;

  mutable Mutex rtcp_counter_mutex_;
  RtcpPacketTypeCounter rtcp_packet_type_counter_
      RTC_GUARDED_BY(rtcp_counter_mutex_);

  std::map<int, SdpAudioFormat> payload_type_map_;
};

void ChannelReceive::OnReceivedPayloadData(
    rtc::ArrayView<const uint8_t> payload,
    const RTPHeader& rtpHeader) {
  if (!playing_) {

    if (source_tracker_) {
      RtpPacketInfos::vector_type packet_vector = {
          RtpPacketInfo(rtpHeader, clock_->CurrentTime())};
      source_tracker_->OnFrameDelivered(RtpPacketInfos(packet_vector));
    }

    return;
  }

  if (acm_receiver_.InsertPacket(rtpHeader, payload) != 0) {
    RTC_DLOG(LS_ERROR) << "ChannelReceive::OnReceivedPayloadData() unable to "
                          "push data to the ACM";
    return;
  }

  TimeDelta round_trip_time = rtp_rtcp_->LastRtt().value_or(TimeDelta::Zero());

  std::vector<uint16_t> nack_list =
      acm_receiver_.GetNackList(round_trip_time.ms());
  if (!nack_list.empty()) {

    ResendPackets(&(nack_list[0]), static_cast<int>(nack_list.size()));
  }
}

void ChannelReceive::InitFrameTransformerDelegate(
    rtc::scoped_refptr<webrtc::FrameTransformerInterface> frame_transformer) {
  RTC_DCHECK(frame_transformer);
  RTC_DCHECK(!frame_transformer_delegate_);
  RTC_DCHECK(worker_thread_->IsCurrent());

  ChannelReceiveFrameTransformerDelegate::ReceiveFrameCallback
      receive_audio_callback = [this](rtc::ArrayView<const uint8_t> packet,
                                      const RTPHeader& header) {
        RTC_DCHECK_RUN_ON(&worker_thread_checker_);
        OnReceivedPayloadData(packet, header);
      };
  frame_transformer_delegate_ =
      rtc::make_ref_counted<ChannelReceiveFrameTransformerDelegate>(
          std::move(receive_audio_callback), std::move(frame_transformer),
          worker_thread_);
  frame_transformer_delegate_->Init();
}

AudioMixer::Source::AudioFrameInfo ChannelReceive::GetAudioFrameWithInfo(
    int sample_rate_hz,
    AudioFrame* audio_frame) {
  TRACE_EVENT_BEGIN1("webrtc", "ChannelReceive::GetAudioFrameWithInfo",
                     "sample_rate_hz", sample_rate_hz);
  RTC_DCHECK_RUNS_SERIALIZED(&audio_thread_race_checker_);
  audio_frame->sample_rate_hz_ = sample_rate_hz;

  event_log_->Log(std::make_unique<RtcEventAudioPlayout>(remote_ssrc_));

  bool muted;
  if (acm_receiver_.GetAudio(audio_frame->sample_rate_hz_, audio_frame,
                             &muted) == -1) {
    RTC_DLOG(LS_ERROR)
        << "ChannelReceive::GetAudioFrame() PlayoutData10Ms() failed!";

    TRACE_EVENT_END1("webrtc", "ChannelReceive::GetAudioFrameWithInfo", "error",
                     1);
    return AudioMixer::Source::AudioFrameInfo::kError;
  }

  if (muted) {

    AudioFrameOperations::Mute(audio_frame);
  }

  {

    MutexLock lock(&callback_mutex_);
    if (audio_sink_) {
      AudioSinkInterface::Data data(
          audio_frame->data(), audio_frame->samples_per_channel_,
          audio_frame->sample_rate_hz_, audio_frame->num_channels_,
          audio_frame->timestamp_);
      audio_sink_->OnData(data);
    }
  }

  float output_gain = 1.0f;
  {
    MutexLock lock(&volume_settings_mutex_);
    output_gain = _outputGain;
  }

  if (output_gain < 0.99f || output_gain > 1.01f) {

    AudioFrameOperations::ScaleWithSat(output_gain, audio_frame);
  }

  _outputAudioLevel.ComputeLevel(*audio_frame, kAudioSampleDurationSeconds);

  if (capture_start_rtp_time_stamp_ < 0 && audio_frame->timestamp_ != 0) {

    capture_start_rtp_time_stamp_ = audio_frame->timestamp_;
  }

  if (capture_start_rtp_time_stamp_ >= 0) {

    int64_t unwrap_timestamp =
        rtp_ts_wraparound_handler_.Unwrap(audio_frame->timestamp_);
    audio_frame->elapsed_time_ms_ =
        (unwrap_timestamp - capture_start_rtp_time_stamp_) /
        (GetRtpTimestampRateHz() / 1000);

    {
      MutexLock lock(&ts_stats_lock_);

      audio_frame->ntp_time_ms_ =
          ntp_estimator_.Estimate(audio_frame->timestamp_);

      if (audio_frame->ntp_time_ms_ > 0) {

        capture_start_ntp_time_ms_ =
            audio_frame->ntp_time_ms_ - audio_frame->elapsed_time_ms_;
      }
    }
  }

  RtpPacketInfos::vector_type packet_infos;
  for (auto& packet_info : audio_frame->packet_infos_) {
    RtpPacketInfo new_packet_info(packet_info);
    if (packet_info.absolute_capture_time().has_value()) {
      MutexLock lock(&ts_stats_lock_);
      new_packet_info.set_local_capture_clock_offset(
          capture_clock_offset_updater_.ConvertsToTimeDela(
              capture_clock_offset_updater_.AdjustEstimatedCaptureClockOffset(
                  packet_info.absolute_capture_time()
                      ->estimated_capture_clock_offset)));
    }
    packet_infos.push_back(std::move(new_packet_info));
  }
  audio_frame->packet_infos_ = RtpPacketInfos(packet_infos);

  ++audio_frame_interval_count_;
  if (audio_frame_interval_count_ >= kHistogramReportingInterval) {
    audio_frame_interval_count_ = 0;
    worker_thread_->PostTask(SafeTask(worker_safety_.flag(), [this]() {
      RTC_DCHECK_RUN_ON(&worker_thread_checker_);
      RTC_HISTOGRAM_COUNTS_1000("WebRTC.Audio.TargetJitterBufferDelayMs",
                                acm_receiver_.TargetDelayMs());
      const int jitter_buffer_delay = acm_receiver_.FilteredCurrentDelayMs();
      RTC_HISTOGRAM_COUNTS_1000("WebRTC.Audio.ReceiverDelayEstimateMs",
                                jitter_buffer_delay + playout_delay_ms_);
      RTC_HISTOGRAM_COUNTS_1000("WebRTC.Audio.ReceiverJitterBufferDelayMs",
                                jitter_buffer_delay);
      RTC_HISTOGRAM_COUNTS_1000("WebRTC.Audio.ReceiverDeviceDelayMs",
                                playout_delay_ms_);
    }));
  }

  TRACE_EVENT_END2("webrtc", "ChannelReceive::GetAudioFrameWithInfo", "gain",
                   output_gain, "muted", muted);
  return muted ? AudioMixer::Source::AudioFrameInfo::kMuted
               : AudioMixer::Source::AudioFrameInfo::kNormal;
}

int ChannelReceive::PreferredSampleRate() const {
  RTC_DCHECK_RUNS_SERIALIZED(&audio_thread_race_checker_);

  return std::max(acm_receiver_.last_packet_sample_rate_hz().value_or(0),
                  acm_receiver_.last_output_sample_rate_hz());
}

void ChannelReceive::SetSourceTracker(SourceTracker* source_tracker) {
  source_tracker_ = source_tracker;
}

ChannelReceive::ChannelReceive(
    Clock* clock,
    NetEqFactory* neteq_factory,
    AudioDeviceModule* audio_device_module,
    Transport* rtcp_send_transport,
    RtcEventLog* rtc_event_log,
    uint32_t local_ssrc,
    uint32_t remote_ssrc,
    size_t jitter_buffer_max_packets,
    bool jitter_buffer_fast_playout,
    int jitter_buffer_min_delay_ms,
    bool enable_non_sender_rtt,
    rtc::scoped_refptr<AudioDecoderFactory> decoder_factory,
    absl::optional<AudioCodecPairId> codec_pair_id,
    rtc::scoped_refptr<FrameDecryptorInterface> frame_decryptor,
    const webrtc::CryptoOptions& crypto_options,
    rtc::scoped_refptr<FrameTransformerInterface> frame_transformer)
    : worker_thread_(TaskQueueBase::Current()),
      event_log_(rtc_event_log),
      rtp_receive_statistics_(ReceiveStatistics::Create(clock)),
      remote_ssrc_(remote_ssrc),
      acm_receiver_(AcmConfig(neteq_factory,
                              decoder_factory,
                              codec_pair_id,
                              jitter_buffer_max_packets,
                              jitter_buffer_fast_playout,
                              jitter_buffer_min_delay_ms)),
      _outputAudioLevel(),
      clock_(clock),
      ntp_estimator_(clock),
      playout_timestamp_rtp_(0),
      playout_delay_ms_(0),
      capture_start_rtp_time_stamp_(-1),
      capture_start_ntp_time_ms_(-1),
      _audioDeviceModulePtr(audio_device_module),
      _outputGain(1.0f),
      associated_send_channel_(nullptr),
      frame_decryptor_(frame_decryptor),
      crypto_options_(crypto_options),
      absolute_capture_time_interpolator_(clock) {
  RTC_DCHECK(audio_device_module);

  network_thread_checker_.Detach();

  rtp_receive_statistics_->EnableRetransmitDetection(remote_ssrc_, true);
  RtpRtcpInterface::Configuration configuration;
  configuration.clock = clock;
  configuration.audio = true;
  configuration.receiver_only = true;
  configuration.outgoing_transport = rtcp_send_transport;
  configuration.receive_statistics = rtp_receive_statistics_.get();
  configuration.event_log = event_log_;
  configuration.local_media_ssrc = local_ssrc;
  configuration.rtcp_packet_type_counter_observer = this;
  configuration.non_sender_rtt_measurement = enable_non_sender_rtt;

  if (frame_transformer)
    InitFrameTransformerDelegate(std::move(frame_transformer));

  rtp_rtcp_ = ModuleRtpRtcpImpl2::Create(configuration);
  rtp_rtcp_->SetRemoteSSRC(remote_ssrc_);

  rtp_rtcp_->SetRTCPStatus(RtcpMode::kCompound);
}

ChannelReceive::~ChannelReceive() {
  RTC_DCHECK_RUN_ON(&construction_thread_);

  if (frame_transformer_delegate_)
    frame_transformer_delegate_->Reset();

  StopPlayout();
}

void ChannelReceive::SetSink(AudioSinkInterface* sink) {
  RTC_DCHECK_RUN_ON(&worker_thread_checker_);
  MutexLock lock(&callback_mutex_);
  audio_sink_ = sink;
}

void ChannelReceive::StartPlayout() {
  RTC_DCHECK_RUN_ON(&worker_thread_checker_);
  playing_ = true;
}

void ChannelReceive::StopPlayout() {
  RTC_DCHECK_RUN_ON(&worker_thread_checker_);
  playing_ = false;
  _outputAudioLevel.ResetLevelFullRange();
  acm_receiver_.FlushBuffers();
}

absl::optional<std::pair<int, SdpAudioFormat>> ChannelReceive::GetReceiveCodec()
    const {
  RTC_DCHECK_RUN_ON(&worker_thread_checker_);
  return acm_receiver_.LastDecoder();
}

void ChannelReceive::SetReceiveCodecs(
    const std::map<int, SdpAudioFormat>& codecs) {
  RTC_DCHECK_RUN_ON(&worker_thread_checker_);
  for (const auto& kv : codecs) {
    RTC_DCHECK_GE(kv.second.clockrate_hz, 1000);
    payload_type_frequencies_[kv.first] = kv.second.clockrate_hz;
  }
  payload_type_map_ = codecs;
  acm_receiver_.SetCodecs(codecs);
}

void ChannelReceive::OnRtpPacket(const RtpPacketReceived& packet) {
  RTC_DCHECK_RUN_ON(&worker_thread_checker_);

  int64_t now_ms = rtc::TimeMillis();

  last_received_rtp_timestamp_ = packet.Timestamp();
  last_received_rtp_system_time_ms_ = now_ms;

  UpdatePlayoutTimestamp(false, now_ms);

  const auto& it = payload_type_frequencies_.find(packet.PayloadType());
  if (it == payload_type_frequencies_.end())
    return;

  RtpPacketReceived packet_copy(packet);
  packet_copy.set_payload_type_frequency(it->second);

  rtp_receive_statistics_->OnRtpPacket(packet_copy);

  RTPHeader header;
  packet_copy.GetHeader(&header);

  header.extension.absolute_capture_time =
      absolute_capture_time_interpolator_.OnReceivePacket(
          AbsoluteCaptureTimeInterpolator::GetSource(header.ssrc,
                                                     header.arrOfCSRCs),
          header.timestamp,
          rtc::saturated_cast<uint32_t>(packet_copy.payload_type_frequency()),
          header.extension.absolute_capture_time);

  ReceivePacket(packet_copy.data(), packet_copy.size(), header);
}

void ChannelReceive::ReceivePacket(const uint8_t* packet,
                                   size_t packet_length,
                                   const RTPHeader& header) {
  const uint8_t* payload = packet + header.headerLength;
  RTC_DCHECK_GE(packet_length, header.headerLength);
  size_t payload_length = packet_length - header.headerLength;

  size_t payload_data_length = payload_length - header.paddingLength;

  rtc::Buffer decrypted_audio_payload;
  if (frame_decryptor_ != nullptr) {
    const size_t max_plaintext_size = frame_decryptor_->GetMaxPlaintextByteSize(
        cricket::MEDIA_TYPE_AUDIO, payload_length);
    decrypted_audio_payload.SetSize(max_plaintext_size);

    const std::vector<uint32_t> csrcs(header.arrOfCSRCs,
                                      header.arrOfCSRCs + header.numCSRCs);
    const FrameDecryptorInterface::Result decrypt_result =
        frame_decryptor_->Decrypt(
            cricket::MEDIA_TYPE_AUDIO, csrcs,
                                nullptr,
            rtc::ArrayView<const uint8_t>(payload, payload_data_length),
            decrypted_audio_payload);

    if (decrypt_result.IsOk()) {
      decrypted_audio_payload.SetSize(decrypt_result.bytes_written);
    } else {

      decrypted_audio_payload.SetSize(0);
    }

    payload = decrypted_audio_payload.data();
    payload_data_length = decrypted_audio_payload.size();
  } else if (crypto_options_.sframe.require_frame_encryption) {
    RTC_DLOG(LS_ERROR)
        << "FrameDecryptor required but not set, dropping packet";
    payload_data_length = 0;
  }

  rtc::ArrayView<const uint8_t> payload_data(payload, payload_data_length);
  if (frame_transformer_delegate_) {

    char buf[1024];
    rtc::SimpleStringBuilder mime_type(buf);
    auto it = payload_type_map_.find(header.payloadType);
    mime_type << MediaTypeToString(cricket::MEDIA_TYPE_AUDIO) << "/"
              << (it != payload_type_map_.end() ? it->second.name
                                                : "x-unknown");
    frame_transformer_delegate_->Transform(payload_data, header, remote_ssrc_,
                                           mime_type.str());
  } else {
    OnReceivedPayloadData(payload_data, header);
  }
}

void ChannelReceive::ReceivedRTCPPacket(const uint8_t* data, size_t length) {
  RTC_DCHECK_RUN_ON(&worker_thread_checker_);

  UpdatePlayoutTimestamp(true, rtc::TimeMillis());

  rtp_rtcp_->IncomingRtcpPacket(rtc::MakeArrayView(data, length));

  absl::optional<TimeDelta> rtt = rtp_rtcp_->LastRtt();
  if (!rtt.has_value()) {

    return;
  }

  absl::optional<RtpRtcpInterface::SenderReportStats> last_sr =
      rtp_rtcp_->GetSenderReportStats();
  if (!last_sr.has_value()) {

    return;
  }

  {
    MutexLock lock(&ts_stats_lock_);
    ntp_estimator_.UpdateRtcpTimestamp(*rtt, last_sr->last_remote_timestamp,
                                       last_sr->last_remote_rtp_timestamp);
    absl::optional<int64_t> remote_to_local_clock_offset =
        ntp_estimator_.EstimateRemoteToLocalClockOffset();
    if (remote_to_local_clock_offset.has_value()) {
      capture_clock_offset_updater_.SetRemoteToLocalClockOffset(
          *remote_to_local_clock_offset);
    }
  }
}

int ChannelReceive::GetSpeechOutputLevelFullRange() const {
  RTC_DCHECK_RUN_ON(&worker_thread_checker_);
  return _outputAudioLevel.LevelFullRange();
}

double ChannelReceive::GetTotalOutputEnergy() const {
  RTC_DCHECK_RUN_ON(&worker_thread_checker_);
  return _outputAudioLevel.TotalEnergy();
}

double ChannelReceive::GetTotalOutputDuration() const {
  RTC_DCHECK_RUN_ON(&worker_thread_checker_);
  return _outputAudioLevel.TotalDuration();
}

void ChannelReceive::SetChannelOutputVolumeScaling(float scaling) {
  RTC_DCHECK_RUN_ON(&worker_thread_checker_);
  MutexLock lock(&volume_settings_mutex_);
  _outputGain = scaling;
}

void ChannelReceive::RegisterReceiverCongestionControlObjects(
    PacketRouter* packet_router) {
  RTC_DCHECK_RUN_ON(&worker_thread_checker_);
  RTC_DCHECK(packet_router);
  RTC_DCHECK(!packet_router_);
  constexpr bool remb_candidate = false;
  packet_router->AddReceiveRtpModule(rtp_rtcp_.get(), remb_candidate);
  packet_router_ = packet_router;
}

void ChannelReceive::ResetReceiverCongestionControlObjects() {
  RTC_DCHECK_RUN_ON(&worker_thread_checker_);
  RTC_DCHECK(packet_router_);
  packet_router_->RemoveReceiveRtpModule(rtp_rtcp_.get());
  packet_router_ = nullptr;
}

CallReceiveStatistics ChannelReceive::GetRTCPStatistics() const {
  RTC_DCHECK_RUN_ON(&worker_thread_checker_);
  CallReceiveStatistics stats;

  RtpReceiveStats rtp_stats;
  StreamStatistician* statistician =
      rtp_receive_statistics_->GetStatistician(remote_ssrc_);
  if (statistician) {
    rtp_stats = statistician->GetStats();
  }

  stats.cumulativeLost = rtp_stats.packets_lost;
  stats.jitterSamples = rtp_stats.jitter;

  if (statistician) {
    stats.payload_bytes_received = rtp_stats.packet_counter.payload_bytes;

    stats.header_and_padding_bytes_received =
        rtp_stats.packet_counter.header_bytes +
        rtp_stats.packet_counter.padding_bytes;
    stats.packetsReceived = rtp_stats.packet_counter.packets;
    stats.last_packet_received = rtp_stats.last_packet_received;
  } else {
    stats.payload_bytes_received = 0;
    stats.header_and_padding_bytes_received = 0;
    stats.packetsReceived = 0;
    stats.last_packet_received = absl::nullopt;
  }

  {
    MutexLock lock(&rtcp_counter_mutex_);
    stats.nacks_sent = rtcp_packet_type_counter_.nack_packets;
  }

  {
    MutexLock lock(&ts_stats_lock_);
    stats.capture_start_ntp_time_ms_ = capture_start_ntp_time_ms_;
  }

  absl::optional<RtpRtcpInterface::SenderReportStats> rtcp_sr_stats =
      rtp_rtcp_->GetSenderReportStats();
  if (rtcp_sr_stats.has_value()) {
    stats.last_sender_report_timestamp_ms =
        rtcp_sr_stats->last_arrival_timestamp.ToMs() -
        rtc::kNtpJan1970Millisecs;
    stats.last_sender_report_remote_timestamp_ms =
        rtcp_sr_stats->last_remote_timestamp.ToMs() - rtc::kNtpJan1970Millisecs;
    stats.sender_reports_packets_sent = rtcp_sr_stats->packets_sent;
    stats.sender_reports_bytes_sent = rtcp_sr_stats->bytes_sent;
    stats.sender_reports_reports_count = rtcp_sr_stats->reports_count;
  }

  absl::optional<RtpRtcpInterface::NonSenderRttStats> non_sender_rtt_stats =
      rtp_rtcp_->GetNonSenderRttStats();
  if (non_sender_rtt_stats.has_value()) {
    stats.round_trip_time = non_sender_rtt_stats->round_trip_time;
    stats.round_trip_time_measurements =
        non_sender_rtt_stats->round_trip_time_measurements;
    stats.total_round_trip_time = non_sender_rtt_stats->total_round_trip_time;
  }

  return stats;
}

void ChannelReceive::SetNACKStatus(bool enable, int max_packets) {
  RTC_DCHECK_RUN_ON(&worker_thread_checker_);

  if (enable) {
    rtp_receive_statistics_->SetMaxReorderingThreshold(max_packets);
    acm_receiver_.EnableNack(max_packets);
  } else {
    rtp_receive_statistics_->SetMaxReorderingThreshold(
        kDefaultMaxReorderingThreshold);
    acm_receiver_.DisableNack();
  }
}

void ChannelReceive::SetNonSenderRttMeasurement(bool enabled) {
  RTC_DCHECK_RUN_ON(&worker_thread_checker_);
  rtp_rtcp_->SetNonSenderRttMeasurement(enabled);
}

int ChannelReceive::ResendPackets(const uint16_t* sequence_numbers,
                                  int length) {
  return rtp_rtcp_->SendNACK(sequence_numbers, length);
}

void ChannelReceive::RtcpPacketTypesCounterUpdated(
    uint32_t ssrc,
    const RtcpPacketTypeCounter& packet_counter) {
  if (ssrc != remote_ssrc_) {
    return;
  }
  MutexLock lock(&rtcp_counter_mutex_);
  rtcp_packet_type_counter_ = packet_counter;
}

void ChannelReceive::SetAssociatedSendChannel(
    const ChannelSendInterface* channel) {
  RTC_DCHECK_RUN_ON(&network_thread_checker_);
  associated_send_channel_ = channel;
}

void ChannelReceive::SetDepacketizerToDecoderFrameTransformer(
    rtc::scoped_refptr<webrtc::FrameTransformerInterface> frame_transformer) {
  RTC_DCHECK_RUN_ON(&worker_thread_checker_);
  if (!frame_transformer) {
    RTC_DCHECK_NOTREACHED() << "Not setting the transformer?";
    return;
  }
  if (frame_transformer_delegate_) {

    RTC_CHECK_EQ(frame_transformer_delegate_->FrameTransformer(),
                 frame_transformer);
    return;
  }

  InitFrameTransformerDelegate(std::move(frame_transformer));
}

void ChannelReceive::SetFrameDecryptor(
    rtc::scoped_refptr<webrtc::FrameDecryptorInterface> frame_decryptor) {

  RTC_DCHECK_RUN_ON(&worker_thread_checker_);
  frame_decryptor_ = std::move(frame_decryptor);
}

void ChannelReceive::OnLocalSsrcChange(uint32_t local_ssrc) {

  RTC_DCHECK_RUN_ON(&worker_thread_checker_);
  rtp_rtcp_->SetLocalSsrc(local_ssrc);
}

uint32_t ChannelReceive::GetLocalSsrc() const {

  RTC_DCHECK_RUN_ON(&worker_thread_checker_);
  return rtp_rtcp_->local_media_ssrc();
}

NetworkStatistics ChannelReceive::GetNetworkStatistics(
    bool get_and_clear_legacy_stats) const {
  RTC_DCHECK_RUN_ON(&worker_thread_checker_);
  NetworkStatistics stats;
  acm_receiver_.GetNetworkStatistics(&stats, get_and_clear_legacy_stats);
  return stats;
}

AudioDecodingCallStats ChannelReceive::GetDecodingCallStatistics() const {
  RTC_DCHECK_RUN_ON(&worker_thread_checker_);
  AudioDecodingCallStats stats;
  acm_receiver_.GetDecodingCallStatistics(&stats);
  return stats;
}

uint32_t ChannelReceive::GetDelayEstimate() const {
  RTC_DCHECK_RUN_ON(&worker_thread_checker_);

  return acm_receiver_.FilteredCurrentDelayMs() + playout_delay_ms_;
}

bool ChannelReceive::SetMinimumPlayoutDelay(int delay_ms) {

  RTC_DCHECK_RUN_ON(&worker_thread_checker_);

  delay_ms = rtc::SafeClamp(delay_ms, kVoiceEngineMinMinPlayoutDelayMs,
                            kVoiceEngineMaxMinPlayoutDelayMs);
  if (acm_receiver_.SetMinimumDelay(delay_ms) != 0) {
    RTC_DLOG(LS_ERROR)
        << "SetMinimumPlayoutDelay() failed to set min playout delay";
    return false;
  }
  return true;
}

bool ChannelReceive::GetPlayoutRtpTimestamp(uint32_t* rtp_timestamp,
                                            int64_t* time_ms) const {
  RTC_DCHECK_RUN_ON(&worker_thread_checker_);
  if (!playout_timestamp_rtp_time_ms_)
    return false;
  *rtp_timestamp = playout_timestamp_rtp_;
  *time_ms = playout_timestamp_rtp_time_ms_.value();
  return true;
}

void ChannelReceive::SetEstimatedPlayoutNtpTimestampMs(int64_t ntp_timestamp_ms,
                                                       int64_t time_ms) {
  RTC_DCHECK_RUN_ON(&worker_thread_checker_);
  playout_timestamp_ntp_ = ntp_timestamp_ms;
  playout_timestamp_ntp_time_ms_ = time_ms;
}

absl::optional<int64_t>
ChannelReceive::GetCurrentEstimatedPlayoutNtpTimestampMs(int64_t now_ms) const {
  RTC_DCHECK_RUN_ON(&worker_thread_checker_);
  if (!playout_timestamp_ntp_ || !playout_timestamp_ntp_time_ms_)
    return absl::nullopt;

  int64_t elapsed_ms = now_ms - *playout_timestamp_ntp_time_ms_;
  return *playout_timestamp_ntp_ + elapsed_ms;
}

bool ChannelReceive::SetBaseMinimumPlayoutDelayMs(int delay_ms) {
  event_log_->Log(
      std::make_unique<RtcEventNetEqSetMinimumDelay>(remote_ssrc_, delay_ms));
  return acm_receiver_.SetBaseMinimumDelayMs(delay_ms);
}

int ChannelReceive::GetBaseMinimumPlayoutDelayMs() const {
  return acm_receiver_.GetBaseMinimumDelayMs();
}

absl::optional<Syncable::Info> ChannelReceive::GetSyncInfo() const {

  RTC_DCHECK_RUN_ON(&worker_thread_checker_);
  Syncable::Info info;
  absl::optional<RtpRtcpInterface::SenderReportStats> last_sr =
      rtp_rtcp_->GetSenderReportStats();
  if (!last_sr.has_value()) {
    return absl::nullopt;
  }
  info.capture_time_ntp_secs = last_sr->last_remote_timestamp.seconds();
  info.capture_time_ntp_frac = last_sr->last_remote_timestamp.fractions();
  info.capture_time_source_clock = last_sr->last_remote_rtp_timestamp;

  if (!last_received_rtp_timestamp_ || !last_received_rtp_system_time_ms_) {
    return absl::nullopt;
  }
  info.latest_received_capture_timestamp = *last_received_rtp_timestamp_;
  info.latest_receive_time_ms = *last_received_rtp_system_time_ms_;

  int jitter_buffer_delay = acm_receiver_.FilteredCurrentDelayMs();
  info.current_delay_ms = jitter_buffer_delay + playout_delay_ms_;

  return info;
}

void ChannelReceive::UpdatePlayoutTimestamp(bool rtcp, int64_t now_ms) {
  RTC_DCHECK_RUN_ON(&worker_thread_checker_);

  jitter_buffer_playout_timestamp_ = acm_receiver_.GetPlayoutTimestamp();

  if (!jitter_buffer_playout_timestamp_) {

    return;
  }

  uint16_t delay_ms = 0;
  if (_audioDeviceModulePtr->PlayoutDelay(&delay_ms) == -1) {
    RTC_DLOG(LS_WARNING)
        << "ChannelReceive::UpdatePlayoutTimestamp() failed to read"
           " playout delay from the ADM";
    return;
  }

  RTC_DCHECK(jitter_buffer_playout_timestamp_);
  uint32_t playout_timestamp = *jitter_buffer_playout_timestamp_;

  playout_timestamp -= (delay_ms * (GetRtpTimestampRateHz() / 1000));

  if (!rtcp && playout_timestamp != playout_timestamp_rtp_) {
    playout_timestamp_rtp_ = playout_timestamp;
    playout_timestamp_rtp_time_ms_ = now_ms;
  }
  playout_delay_ms_ = delay_ms;
}

int ChannelReceive::GetRtpTimestampRateHz() const {
  const auto decoder = acm_receiver_.LastDecoder();

  return (decoder && decoder->second.clockrate_hz != 0)
             ? decoder->second.clockrate_hz
             : acm_receiver_.last_output_sample_rate_hz();
}

}

std::unique_ptr<ChannelReceiveInterface> CreateChannelReceive(
    Clock* clock,
    NetEqFactory* neteq_factory,
    AudioDeviceModule* audio_device_module,
    Transport* rtcp_send_transport,
    RtcEventLog* rtc_event_log,
    uint32_t local_ssrc,
    uint32_t remote_ssrc,
    size_t jitter_buffer_max_packets,
    bool jitter_buffer_fast_playout,
    int jitter_buffer_min_delay_ms,
    bool enable_non_sender_rtt,
    rtc::scoped_refptr<AudioDecoderFactory> decoder_factory,
    absl::optional<AudioCodecPairId> codec_pair_id,
    rtc::scoped_refptr<FrameDecryptorInterface> frame_decryptor,
    const webrtc::CryptoOptions& crypto_options,
    rtc::scoped_refptr<FrameTransformerInterface> frame_transformer) {
  return std::make_unique<ChannelReceive>(
      clock, neteq_factory, audio_device_module, rtcp_send_transport,
      rtc_event_log, local_ssrc, remote_ssrc, jitter_buffer_max_packets,
      jitter_buffer_fast_playout, jitter_buffer_min_delay_ms,
      enable_non_sender_rtt, decoder_factory, codec_pair_id,
      std::move(frame_decryptor), crypto_options, std::move(frame_transformer));
}

}
}
