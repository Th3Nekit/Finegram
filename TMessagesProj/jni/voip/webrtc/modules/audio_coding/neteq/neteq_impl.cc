/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_coding/neteq/neteq_impl.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <list>
#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "api/audio_codecs/audio_decoder.h"
#include "api/neteq/neteq_controller.h"
#include "api/neteq/tick_timer.h"
#include "common_audio/signal_processing/include/signal_processing_library.h"
#include "modules/audio_coding/codecs/cng/webrtc_cng.h"
#include "modules/audio_coding/neteq/accelerate.h"
#include "modules/audio_coding/neteq/background_noise.h"
#include "modules/audio_coding/neteq/comfort_noise.h"
#include "modules/audio_coding/neteq/decision_logic.h"
#include "modules/audio_coding/neteq/decoder_database.h"
#include "modules/audio_coding/neteq/dtmf_buffer.h"
#include "modules/audio_coding/neteq/dtmf_tone_generator.h"
#include "modules/audio_coding/neteq/expand.h"
#include "modules/audio_coding/neteq/merge.h"
#include "modules/audio_coding/neteq/nack_tracker.h"
#include "modules/audio_coding/neteq/normal.h"
#include "modules/audio_coding/neteq/packet.h"
#include "modules/audio_coding/neteq/packet_buffer.h"
#include "modules/audio_coding/neteq/preemptive_expand.h"
#include "modules/audio_coding/neteq/red_payload_splitter.h"
#include "modules/audio_coding/neteq/statistics_calculator.h"
#include "modules/audio_coding/neteq/sync_buffer.h"
#include "modules/audio_coding/neteq/time_stretch.h"
#include "modules/audio_coding/neteq/timestamp_scaler.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/numerics/safe_conversions.h"
#include "rtc_base/sanitizer.h"
#include "rtc_base/strings/audio_format_to_string.h"
#include "rtc_base/trace_event.h"
#include "system_wrappers/include/clock.h"
#include "system_wrappers/include/field_trial.h"

namespace webrtc {
namespace {

std::unique_ptr<NetEqController> CreateNetEqController(
    const NetEqControllerFactory& controller_factory,
    int base_min_delay,
    int max_packets_in_buffer,
    bool allow_time_stretching,
    TickTimer* tick_timer,
    webrtc::Clock* clock) {
  NetEqController::Config config;
  config.base_min_delay_ms = base_min_delay;
  config.max_packets_in_buffer = max_packets_in_buffer;
  config.allow_time_stretching = allow_time_stretching;
  config.tick_timer = tick_timer;
  config.clock = clock;
  return controller_factory.CreateNetEqController(config);
}

AudioFrame::SpeechType ToSpeechType(NetEqImpl::OutputType type) {
  switch (type) {
    case NetEqImpl::OutputType::kNormalSpeech: {
      return AudioFrame::kNormalSpeech;
    }
    case NetEqImpl::OutputType::kCNG: {
      return AudioFrame::kCNG;
    }
    case NetEqImpl::OutputType::kPLC: {
      return AudioFrame::kPLC;
    }
    case NetEqImpl::OutputType::kPLCCNG: {
      return AudioFrame::kPLCCNG;
    }
    case NetEqImpl::OutputType::kCodecPLC: {
      return AudioFrame::kCodecPLC;
    }
    default:
      RTC_DCHECK_NOTREACHED();
      return AudioFrame::kUndefined;
  }
}

bool EqualSampleRates(uint8_t pt1,
                      uint8_t pt2,
                      const DecoderDatabase& decoder_database) {
  auto* di1 = decoder_database.GetDecoderInfo(pt1);
  auto* di2 = decoder_database.GetDecoderInfo(pt2);
  return di1 && di2 && di1->SampleRateHz() == di2->SampleRateHz();
}

}

NetEqImpl::Dependencies::Dependencies(
    const NetEq::Config& config,
    Clock* clock,
    const rtc::scoped_refptr<AudioDecoderFactory>& decoder_factory,
    const NetEqControllerFactory& controller_factory)
    : clock(clock),
      tick_timer(new TickTimer),
      stats(new StatisticsCalculator),
      decoder_database(
          new DecoderDatabase(decoder_factory, config.codec_pair_id)),
      dtmf_buffer(new DtmfBuffer(config.sample_rate_hz)),
      dtmf_tone_generator(new DtmfToneGenerator),
      packet_buffer(new PacketBuffer(config.max_packets_in_buffer,
                                     tick_timer.get(),
                                     stats.get())),
      neteq_controller(
          CreateNetEqController(controller_factory,
                                config.min_delay_ms,
                                config.max_packets_in_buffer,
                                !config.for_test_no_time_stretching,
                                tick_timer.get(),
                                clock)),
      red_payload_splitter(new RedPayloadSplitter),
      timestamp_scaler(new TimestampScaler(*decoder_database)),
      accelerate_factory(new AccelerateFactory),
      expand_factory(new ExpandFactory),
      preemptive_expand_factory(new PreemptiveExpandFactory) {}

NetEqImpl::Dependencies::~Dependencies() = default;

NetEqImpl::NetEqImpl(const NetEq::Config& config,
                     Dependencies&& deps,
                     bool create_components)
    : clock_(deps.clock),
      tick_timer_(std::move(deps.tick_timer)),
      decoder_database_(std::move(deps.decoder_database)),
      dtmf_buffer_(std::move(deps.dtmf_buffer)),
      dtmf_tone_generator_(std::move(deps.dtmf_tone_generator)),
      packet_buffer_(std::move(deps.packet_buffer)),
      red_payload_splitter_(std::move(deps.red_payload_splitter)),
      timestamp_scaler_(std::move(deps.timestamp_scaler)),
      expand_factory_(std::move(deps.expand_factory)),
      accelerate_factory_(std::move(deps.accelerate_factory)),
      preemptive_expand_factory_(std::move(deps.preemptive_expand_factory)),
      stats_(std::move(deps.stats)),
      enable_fec_delay_adaptation_(
          !field_trial::IsDisabled("WebRTC-Audio-NetEqFecDelayAdaptation")),
      controller_(std::move(deps.neteq_controller)),
      last_mode_(Mode::kNormal),
      decoded_buffer_length_(kMaxFrameSize),
      decoded_buffer_(new int16_t[decoded_buffer_length_]),
      playout_timestamp_(0),
      new_codec_(false),
      timestamp_(0),
      reset_decoder_(false),
      first_packet_(true),
      enable_fast_accelerate_(config.enable_fast_accelerate),
      nack_enabled_(false),
      enable_muted_state_(config.enable_muted_state),
      expand_uma_logger_("WebRTC.Audio.ExpandRatePercent",
                         10,
                         tick_timer_.get()),
      speech_expand_uma_logger_("WebRTC.Audio.SpeechExpandRatePercent",
                                10,
                                tick_timer_.get()),
      no_time_stretching_(config.for_test_no_time_stretching) {
  RTC_LOG(LS_INFO) << "NetEq config: " << config.ToString();
  int fs = config.sample_rate_hz;
  if (fs != 8000 && fs != 16000 && fs != 32000 && fs != 48000) {
    RTC_LOG(LS_ERROR) << "Sample rate " << fs
                      << " Hz not supported. "
                         "Changing to 8000 Hz.";
    fs = 8000;
  }
  controller_->SetMaximumDelay(config.max_delay_ms);
  fs_hz_ = fs;
  fs_mult_ = fs / 8000;
  last_output_sample_rate_hz_ = fs;
  output_size_samples_ = static_cast<size_t>(kOutputSizeMs * 8 * fs_mult_);
  controller_->SetSampleRate(fs_hz_, output_size_samples_);
  decoder_frame_length_ = 2 * output_size_samples_;
  if (create_components) {
    SetSampleRateAndChannels(fs, 1);
  }
}

NetEqImpl::~NetEqImpl() = default;

int NetEqImpl::InsertPacket(const RTPHeader& rtp_header,
                            rtc::ArrayView<const uint8_t> payload) {
  rtc::MsanCheckInitialized(payload);
  TRACE_EVENT0("webrtc", "NetEqImpl::InsertPacket");
  MutexLock lock(&mutex_);
  if (InsertPacketInternal(rtp_header, payload) != 0) {
    return kFail;
  }
  return kOK;
}

void NetEqImpl::InsertEmptyPacket(const RTPHeader& rtp_header) {
  MutexLock lock(&mutex_);
  if (nack_enabled_) {
    nack_->UpdateLastReceivedPacket(rtp_header.sequenceNumber,
                                    rtp_header.timestamp);
  }
  controller_->RegisterEmptyPacket();
}

int NetEqImpl::GetAudio(AudioFrame* audio_frame,
                        bool* muted,
                        int* current_sample_rate_hz,
                        absl::optional<Operation> action_override) {
  TRACE_EVENT0("webrtc", "NetEqImpl::GetAudio");
  MutexLock lock(&mutex_);
  if (GetAudioInternal(audio_frame, muted, action_override) != 0) {
    return kFail;
  }
  RTC_DCHECK_EQ(
      audio_frame->sample_rate_hz_,
      rtc::dchecked_cast<int>(audio_frame->samples_per_channel_ * 100));
  RTC_DCHECK_EQ(*muted, audio_frame->muted());
  audio_frame->speech_type_ = ToSpeechType(LastOutputType());
  last_output_sample_rate_hz_ = audio_frame->sample_rate_hz_;
  RTC_DCHECK(last_output_sample_rate_hz_ == 8000 ||
             last_output_sample_rate_hz_ == 16000 ||
             last_output_sample_rate_hz_ == 32000 ||
             last_output_sample_rate_hz_ == 48000)
      << "Unexpected sample rate " << last_output_sample_rate_hz_;

  if (current_sample_rate_hz) {
    *current_sample_rate_hz = last_output_sample_rate_hz_;
  }

  return kOK;
}

void NetEqImpl::SetCodecs(const std::map<int, SdpAudioFormat>& codecs) {
  MutexLock lock(&mutex_);
  const std::vector<int> changed_payload_types =
      decoder_database_->SetCodecs(codecs);
  for (const int pt : changed_payload_types) {
    packet_buffer_->DiscardPacketsWithPayloadType(pt);
  }
}

bool NetEqImpl::RegisterPayloadType(int rtp_payload_type,
                                    const SdpAudioFormat& audio_format) {
  RTC_LOG(LS_VERBOSE) << "NetEqImpl::RegisterPayloadType: payload type "
                      << rtp_payload_type << ", codec "
                      << rtc::ToString(audio_format);
  MutexLock lock(&mutex_);
  return decoder_database_->RegisterPayload(rtp_payload_type, audio_format) ==
         DecoderDatabase::kOK;
}

int NetEqImpl::RemovePayloadType(uint8_t rtp_payload_type) {
  MutexLock lock(&mutex_);
  int ret = decoder_database_->Remove(rtp_payload_type);
  if (ret == DecoderDatabase::kOK || ret == DecoderDatabase::kDecoderNotFound) {
    packet_buffer_->DiscardPacketsWithPayloadType(rtp_payload_type);
    return kOK;
  }
  return kFail;
}

void NetEqImpl::RemoveAllPayloadTypes() {
  MutexLock lock(&mutex_);
  decoder_database_->RemoveAll();
}

bool NetEqImpl::SetMinimumDelay(int delay_ms) {
  MutexLock lock(&mutex_);
  if (delay_ms >= 0 && delay_ms <= 10000) {
    RTC_DCHECK(controller_.get());
    return controller_->SetMinimumDelay(delay_ms);
  }
  return false;
}

bool NetEqImpl::SetMaximumDelay(int delay_ms) {
  MutexLock lock(&mutex_);
  if (delay_ms >= 0 && delay_ms <= 10000) {
    RTC_DCHECK(controller_.get());
    return controller_->SetMaximumDelay(delay_ms);
  }
  return false;
}

bool NetEqImpl::SetBaseMinimumDelayMs(int delay_ms) {
  MutexLock lock(&mutex_);
  if (delay_ms >= 0 && delay_ms <= 10000) {
    return controller_->SetBaseMinimumDelay(delay_ms);
  }
  return false;
}

int NetEqImpl::GetBaseMinimumDelayMs() const {
  MutexLock lock(&mutex_);
  return controller_->GetBaseMinimumDelay();
}

int NetEqImpl::TargetDelayMs() const {
  MutexLock lock(&mutex_);
  RTC_DCHECK(controller_.get());
  return controller_->TargetLevelMs();
}

int NetEqImpl::FilteredCurrentDelayMs() const {
  MutexLock lock(&mutex_);

  const int delay_samples =
      controller_->GetFilteredBufferLevel() + sync_buffer_->FutureLength();

  return delay_samples / rtc::CheckedDivExact(fs_hz_, 1000);
}

int NetEqImpl::NetworkStatistics(NetEqNetworkStatistics* stats) {
  MutexLock lock(&mutex_);
  RTC_DCHECK(decoder_database_.get());
  *stats = CurrentNetworkStatisticsInternal();
  stats_->GetNetworkStatistics(decoder_frame_length_, stats);
  return 0;
}

NetEqNetworkStatistics NetEqImpl::CurrentNetworkStatistics() const {
  MutexLock lock(&mutex_);
  return CurrentNetworkStatisticsInternal();
}

NetEqNetworkStatistics NetEqImpl::CurrentNetworkStatisticsInternal() const {
  RTC_DCHECK(decoder_database_.get());
  NetEqNetworkStatistics stats;
  const size_t total_samples_in_buffers =
      packet_buffer_->NumSamplesInBuffer(decoder_frame_length_) +
      sync_buffer_->FutureLength();

  RTC_DCHECK(controller_.get());
  stats.preferred_buffer_size_ms = controller_->TargetLevelMs();
  stats.jitter_peaks_found = controller_->PeakFound();
  RTC_DCHECK_GT(fs_hz_, 0);
  stats.current_buffer_size_ms =
      static_cast<uint16_t>(total_samples_in_buffers * 1000 / fs_hz_);
  return stats;
}

NetEqLifetimeStatistics NetEqImpl::GetLifetimeStatistics() const {
  MutexLock lock(&mutex_);
  return stats_->GetLifetimeStatistics();
}

NetEqOperationsAndState NetEqImpl::GetOperationsAndState() const {
  MutexLock lock(&mutex_);
  auto result = stats_->GetOperationsAndState();
  result.current_buffer_size_ms =
      (packet_buffer_->NumSamplesInBuffer(decoder_frame_length_) +
       sync_buffer_->FutureLength()) *
      1000 / fs_hz_;
  result.current_frame_size_ms = decoder_frame_length_ * 1000 / fs_hz_;
  result.next_packet_available = packet_buffer_->PeekNextPacket() &&
                                 packet_buffer_->PeekNextPacket()->timestamp ==
                                     sync_buffer_->end_timestamp();
  return result;
}

absl::optional<uint32_t> NetEqImpl::GetPlayoutTimestamp() const {
  MutexLock lock(&mutex_);
  if (first_packet_ || last_mode_ == Mode::kRfc3389Cng ||
      last_mode_ == Mode::kCodecInternalCng) {

    return absl::nullopt;
  }
  return timestamp_scaler_->ToExternal(playout_timestamp_);
}

int NetEqImpl::last_output_sample_rate_hz() const {
  MutexLock lock(&mutex_);
  return last_output_sample_rate_hz_;
}

absl::optional<NetEq::DecoderFormat> NetEqImpl::GetDecoderFormat(
    int payload_type) const {
  MutexLock lock(&mutex_);
  const DecoderDatabase::DecoderInfo* const di =
      decoder_database_->GetDecoderInfo(payload_type);
  if (di) {
    const AudioDecoder* const decoder = di->GetDecoder();

    return DecoderFormat{
                           di->IsRed() ? 8000 : di->SampleRateHz(),

        decoder ? rtc::dchecked_cast<int>(decoder->Channels()) : 1,
                       di->GetFormat()};
  } else {

    return absl::nullopt;
  }
}

void NetEqImpl::FlushBuffers() {
  MutexLock lock(&mutex_);
  RTC_LOG(LS_VERBOSE) << "FlushBuffers";
  packet_buffer_->Flush();
  RTC_DCHECK(sync_buffer_.get());
  RTC_DCHECK(expand_.get());
  sync_buffer_->Flush();
  sync_buffer_->set_next_index(sync_buffer_->next_index() -
                               expand_->overlap_length());

  first_packet_ = true;
}

void NetEqImpl::EnableNack(size_t max_nack_list_size) {
  MutexLock lock(&mutex_);
  if (!nack_enabled_) {
    nack_ = std::make_unique<NackTracker>();
    nack_enabled_ = true;
    nack_->UpdateSampleRate(fs_hz_);
  }
  nack_->SetMaxNackListSize(max_nack_list_size);
}

void NetEqImpl::DisableNack() {
  MutexLock lock(&mutex_);
  nack_.reset();
  nack_enabled_ = false;
}

std::vector<uint16_t> NetEqImpl::GetNackList(int64_t round_trip_time_ms) const {
  MutexLock lock(&mutex_);
  if (!nack_enabled_) {
    return std::vector<uint16_t>();
  }
  RTC_DCHECK(nack_.get());
  return nack_->GetNackList(round_trip_time_ms);
}

int NetEqImpl::SyncBufferSizeMs() const {
  MutexLock lock(&mutex_);
  return rtc::dchecked_cast<int>(sync_buffer_->FutureLength() /
                                 rtc::CheckedDivExact(fs_hz_, 1000));
}

const SyncBuffer* NetEqImpl::sync_buffer_for_test() const {
  MutexLock lock(&mutex_);
  return sync_buffer_.get();
}

NetEq::Operation NetEqImpl::last_operation_for_test() const {
  MutexLock lock(&mutex_);
  return last_operation_;
}

int NetEqImpl::InsertPacketInternal(const RTPHeader& rtp_header,
                                    rtc::ArrayView<const uint8_t> payload) {
  if (payload.empty()) {
    RTC_LOG_F(LS_ERROR) << "payload is empty";
    return kInvalidPointer;
  }

  Timestamp receive_time = clock_->CurrentTime();
  stats_->ReceivedPacket();

  PacketList packet_list;

  packet_list.push_back([&rtp_header, &payload] {

    Packet packet;
    packet.payload_type = rtp_header.payloadType;
    packet.sequence_number = rtp_header.sequenceNumber;
    packet.timestamp = rtp_header.timestamp;
    packet.payload.SetData(payload.data(), payload.size());

    RTC_DCHECK(!packet.waiting_time);
    return packet;
  }());

  bool update_sample_rate_and_channels = first_packet_;

  if (update_sample_rate_and_channels) {

    timestamp_scaler_->Reset();
  }

  if (!decoder_database_->IsRed(rtp_header.payloadType)) {

    timestamp_scaler_->ToInternal(&packet_list);
  }

  uint32_t main_timestamp = packet_list.front().timestamp;
  uint8_t main_payload_type = packet_list.front().payload_type;
  uint16_t main_sequence_number = packet_list.front().sequence_number;

  if (update_sample_rate_and_channels) {

    packet_buffer_->Flush();
    dtmf_buffer_->Flush();

    sync_buffer_->IncreaseEndTimestamp(main_timestamp - timestamp_);

    timestamp_ = main_timestamp;
  }

  if (nack_enabled_) {
    RTC_DCHECK(nack_);
    if (update_sample_rate_and_channels) {
      nack_->Reset();
    }
    nack_->UpdateLastReceivedPacket(main_sequence_number, main_timestamp);
  }

  if (decoder_database_->IsRed(rtp_header.payloadType)) {
    if (!red_payload_splitter_->SplitRed(&packet_list)) {
      return kRedundancySplitError;
    }

    red_payload_splitter_->CheckRedPayloads(&packet_list, *decoder_database_);
    if (packet_list.empty()) {
      return kRedundancySplitError;
    }
  }

  if (decoder_database_->CheckPayloadTypes(packet_list) ==
      DecoderDatabase::kDecoderNotFound) {
    return kUnknownRtpPayloadType;
  }

  RTC_DCHECK(!packet_list.empty());

  if (decoder_database_->IsRed(rtp_header.payloadType)) {
    timestamp_scaler_->ToInternal(&packet_list);
    main_timestamp = packet_list.front().timestamp;
    main_payload_type = packet_list.front().payload_type;
    main_sequence_number = packet_list.front().sequence_number;
  }

  PacketList::iterator it = packet_list.begin();
  while (it != packet_list.end()) {
    const Packet& current_packet = (*it);
    RTC_DCHECK(!current_packet.payload.empty());
    if (decoder_database_->IsDtmf(current_packet.payload_type)) {
      DtmfEvent event;
      int ret = DtmfBuffer::ParseEvent(current_packet.timestamp,
                                       current_packet.payload.data(),
                                       current_packet.payload.size(), &event);
      if (ret != DtmfBuffer::kOK) {
        return kDtmfParsingError;
      }
      if (dtmf_buffer_->InsertEvent(event) != DtmfBuffer::kOK) {
        return kDtmfInsertError;
      }
      it = packet_list.erase(it);
    } else {
      ++it;
    }
  }

  PacketList parsed_packet_list;
  bool is_dtx = false;
  while (!packet_list.empty()) {
    Packet& packet = packet_list.front();
    const DecoderDatabase::DecoderInfo* info =
        decoder_database_->GetDecoderInfo(packet.payload_type);
    if (!info) {
      RTC_LOG(LS_WARNING) << "SplitAudio unknown payload type";
      return kUnknownRtpPayloadType;
    }

    if (info->IsComfortNoise()) {

      parsed_packet_list.splice(parsed_packet_list.end(), packet_list,
                                packet_list.begin());
    } else {
      const uint16_t sequence_number = packet.sequence_number;
      const uint8_t payload_type = packet.payload_type;
      const Packet::Priority original_priority = packet.priority;
      auto packet_from_result = [&](AudioDecoder::ParseResult& result) {
        Packet new_packet;
        new_packet.sequence_number = sequence_number;
        new_packet.payload_type = payload_type;
        new_packet.timestamp = result.timestamp;
        new_packet.priority.codec_level = result.priority;
        new_packet.priority.red_level = original_priority.red_level;

        if (new_packet.timestamp == rtp_header.timestamp) {
          new_packet.packet_info = RtpPacketInfo(rtp_header, receive_time);
        }
        new_packet.frame = std::move(result.frame);
        return new_packet;
      };

      std::vector<AudioDecoder::ParseResult> results =
          info->GetDecoder()->ParsePayload(std::move(packet.payload),
                                           packet.timestamp);
      if (results.empty()) {
        packet_list.pop_front();
      } else {
        bool first = true;
        for (auto& result : results) {
          RTC_DCHECK(result.frame);
          RTC_DCHECK_GE(result.priority, 0);
          is_dtx = is_dtx || result.frame->IsDtxPacket();
          if (first) {

            packet_list.front() = packet_from_result(result);
            parsed_packet_list.splice(parsed_packet_list.end(), packet_list,
                                      packet_list.begin());
            first = false;
          } else {
            parsed_packet_list.push_back(packet_from_result(result));
          }
        }
      }
    }
  }

  const size_t number_of_primary_packets = std::count_if(
      parsed_packet_list.begin(), parsed_packet_list.end(),
      [](const Packet& in) { return in.priority.codec_level == 0; });
  if (number_of_primary_packets < parsed_packet_list.size()) {
    stats_->SecondaryPacketsReceived(parsed_packet_list.size() -
                                     number_of_primary_packets);
  }

  bool buffer_flush_occured = false;
  for (Packet& packet : parsed_packet_list) {
    if (MaybeChangePayloadType(packet.payload_type)) {
      packet_buffer_->Flush();
      buffer_flush_occured = true;
    }
    NetEqController::PacketArrivedInfo info = ToPacketArrivedInfo(packet);
    int return_val = packet_buffer_->InsertPacket(std::move(packet));
    if (return_val == PacketBuffer::kFlushed) {
      buffer_flush_occured = true;
    } else if (return_val != PacketBuffer::kOK) {

      return kOtherError;
    }
    if (enable_fec_delay_adaptation_) {
      info.buffer_flush = buffer_flush_occured;
      const bool should_update_stats = !new_codec_ && !buffer_flush_occured;
      auto relative_delay =
          controller_->PacketArrived(fs_hz_, should_update_stats, info);
      if (relative_delay) {
        stats_->RelativePacketArrivalDelay(relative_delay.value());
      }
    }
  }

  if (buffer_flush_occured) {

    new_codec_ = true;
    update_sample_rate_and_channels = true;
  }

  if (first_packet_) {
    first_packet_ = false;

    new_codec_ = true;
  }

  if (current_rtp_payload_type_) {
    RTC_DCHECK(decoder_database_->GetDecoderInfo(*current_rtp_payload_type_))
        << "Payload type " << static_cast<int>(*current_rtp_payload_type_)
        << " is unknown where it shouldn't be";
  }

  if (update_sample_rate_and_channels && !packet_buffer_->Empty()) {

    const Packet* next_packet = packet_buffer_->PeekNextPacket();
    RTC_DCHECK(next_packet);
    const int payload_type = next_packet->payload_type;
    size_t channels = 1;
    if (!decoder_database_->IsComfortNoise(payload_type)) {
      AudioDecoder* decoder = decoder_database_->GetDecoder(payload_type);
      RTC_DCHECK(decoder);
      channels = decoder->Channels();
    }
    const DecoderDatabase::DecoderInfo* decoder_info =
        decoder_database_->GetDecoderInfo(payload_type);
    RTC_DCHECK(decoder_info);
    if (decoder_info->SampleRateHz() != fs_hz_ ||
        channels != algorithm_buffer_->Channels()) {
      SetSampleRateAndChannels(decoder_info->SampleRateHz(), channels);
    }
    if (nack_enabled_) {
      RTC_DCHECK(nack_);

      nack_->UpdateSampleRate(fs_hz_);
    }
  }

  if (!enable_fec_delay_adaptation_) {
    const DecoderDatabase::DecoderInfo* dec_info =
        decoder_database_->GetDecoderInfo(main_payload_type);
    RTC_DCHECK(dec_info);

    NetEqController::PacketArrivedInfo info;
    info.is_cng_or_dtmf = dec_info->IsComfortNoise() || dec_info->IsDtmf();
    info.packet_length_samples =
        number_of_primary_packets * decoder_frame_length_;
    info.main_timestamp = main_timestamp;
    info.main_sequence_number = main_sequence_number;
    info.is_dtx = is_dtx;
    info.buffer_flush = buffer_flush_occured;

    const bool should_update_stats = !new_codec_;
    auto relative_delay =
        controller_->PacketArrived(fs_hz_, should_update_stats, info);
    if (relative_delay) {
      stats_->RelativePacketArrivalDelay(relative_delay.value());
    }
  }
  return 0;
}

bool NetEqImpl::MaybeChangePayloadType(uint8_t payload_type) {
  bool changed = false;
  if (decoder_database_->IsComfortNoise(payload_type)) {
    if (current_cng_rtp_payload_type_ &&
        *current_cng_rtp_payload_type_ != payload_type) {

      current_rtp_payload_type_ = absl::nullopt;
      changed = true;
    }
    current_cng_rtp_payload_type_ = payload_type;
  } else if (!decoder_database_->IsDtmf(payload_type)) {

    if ((current_rtp_payload_type_ &&
         *current_rtp_payload_type_ != payload_type) ||
        (current_cng_rtp_payload_type_ &&
         !EqualSampleRates(payload_type, *current_cng_rtp_payload_type_,
                           *decoder_database_))) {
      current_cng_rtp_payload_type_ = absl::nullopt;
      changed = true;
    }
    current_rtp_payload_type_ = payload_type;
  }
  return changed;
}

int NetEqImpl::GetAudioInternal(AudioFrame* audio_frame,
                                bool* muted,
                                absl::optional<Operation> action_override) {
  PacketList packet_list;
  DtmfEvent dtmf_event;
  Operation operation;
  bool play_dtmf;
  *muted = false;
  last_decoded_packet_infos_.clear();
  tick_timer_->Increment();
  stats_->IncreaseCounter(output_size_samples_, fs_hz_);
  const auto lifetime_stats = stats_->GetLifetimeStatistics();
  expand_uma_logger_.UpdateSampleCounter(lifetime_stats.concealed_samples,
                                         fs_hz_);
  speech_expand_uma_logger_.UpdateSampleCounter(
      lifetime_stats.concealed_samples -
          lifetime_stats.silent_concealed_samples,
      fs_hz_);

  if (enable_muted_state_ && expand_->Muted() && packet_buffer_->Empty()) {
    RTC_DCHECK_EQ(last_mode_, Mode::kExpand);
    audio_frame->Reset();
    RTC_DCHECK(audio_frame->muted());
    playout_timestamp_ += static_cast<uint32_t>(output_size_samples_);
    audio_frame->sample_rate_hz_ = fs_hz_;

    if (output_size_samples_ * sync_buffer_->Channels() >
        AudioFrame::kMaxDataSizeSamples) {
      return kSampleUnderrun;
    }
    audio_frame->samples_per_channel_ = output_size_samples_;
    audio_frame->timestamp_ =
        first_packet_
            ? 0
            : timestamp_scaler_->ToExternal(playout_timestamp_) -
                  static_cast<uint32_t>(audio_frame->samples_per_channel_);
    audio_frame->num_channels_ = sync_buffer_->Channels();
    stats_->ExpandedNoiseSamples(output_size_samples_, false);
    controller_->NotifyMutedState();
    *muted = true;
    return 0;
  }
  int return_value = GetDecision(&operation, &packet_list, &dtmf_event,
                                 &play_dtmf, action_override);
  if (return_value != 0) {
    last_mode_ = Mode::kError;
    return return_value;
  }

  AudioDecoder::SpeechType speech_type;
  int length = 0;
  const size_t start_num_packets = packet_list.size();
  int decode_return_value =
      Decode(&packet_list, &operation, &length, &speech_type);
  if (length > 0) {
    last_decoded_type_ = speech_type;
  }

  bool sid_frame_available =
      (operation == Operation::kRfc3389Cng && !packet_list.empty());

  const bool codec_internal_sid_frame =
      (speech_type == AudioDecoder::kComfortNoise &&
       start_num_packets > packet_list.size());

  if (sid_frame_available || codec_internal_sid_frame) {

    generated_noise_stopwatch_ = tick_timer_->GetNewStopwatch();
  }

  algorithm_buffer_->Clear();
  switch (operation) {
    case Operation::kNormal: {
      DoNormal(decoded_buffer_.get(), length, speech_type, play_dtmf);
      if (length > 0) {
        stats_->DecodedOutputPlayed();
      }
      break;
    }
    case Operation::kMerge: {
      DoMerge(decoded_buffer_.get(), length, speech_type, play_dtmf);
      break;
    }
    case Operation::kExpand: {
      RTC_DCHECK_EQ(return_value, 0);
      if (!current_rtp_payload_type_ || !DoCodecPlc()) {
        return_value = DoExpand(play_dtmf);
      }
      RTC_DCHECK_GE(sync_buffer_->FutureLength() - expand_->overlap_length(),
                    output_size_samples_);
      break;
    }
    case Operation::kAccelerate:
    case Operation::kFastAccelerate: {
      const bool fast_accelerate =
          enable_fast_accelerate_ && (operation == Operation::kFastAccelerate);
      return_value = DoAccelerate(decoded_buffer_.get(), length, speech_type,
                                  play_dtmf, fast_accelerate);
      break;
    }
    case Operation::kPreemptiveExpand: {
      return_value = DoPreemptiveExpand(decoded_buffer_.get(), length,
                                        speech_type, play_dtmf);
      break;
    }
    case Operation::kRfc3389Cng:
    case Operation::kRfc3389CngNoPacket: {
      return_value = DoRfc3389Cng(&packet_list, play_dtmf);
      break;
    }
    case Operation::kCodecInternalCng: {

      DoCodecInternalCng(decoded_buffer_.get(), length);
      break;
    }
    case Operation::kDtmf: {

      return_value = DoDtmf(dtmf_event, &play_dtmf);
      break;
    }
    case Operation::kUndefined: {
      RTC_LOG(LS_ERROR) << "Invalid operation kUndefined.";
      RTC_DCHECK_NOTREACHED();
      last_mode_ = Mode::kError;
      return kInvalidOperation;
    }
  }
  last_operation_ = operation;
  if (return_value < 0) {
    return return_value;
  }

  if (last_mode_ != Mode::kRfc3389Cng) {
    comfort_noise_->Reset();
  }

  RtpPacketInfos packet_infos(last_decoded_packet_infos_);

  sync_buffer_->PushBack(*algorithm_buffer_);

  size_t num_output_samples_per_channel = output_size_samples_;
  size_t num_output_samples = output_size_samples_ * sync_buffer_->Channels();
  if (num_output_samples > AudioFrame::kMaxDataSizeSamples) {
    RTC_LOG(LS_WARNING) << "Output array is too short. "
                        << AudioFrame::kMaxDataSizeSamples << " < "
                        << output_size_samples_ << " * "
                        << sync_buffer_->Channels();
    num_output_samples = AudioFrame::kMaxDataSizeSamples;
    num_output_samples_per_channel =
        AudioFrame::kMaxDataSizeSamples / sync_buffer_->Channels();
  }
  sync_buffer_->GetNextAudioInterleaved(num_output_samples_per_channel,
                                        audio_frame);
  audio_frame->sample_rate_hz_ = fs_hz_;

  audio_frame->packet_infos_ = std::move(packet_infos);
  if (sync_buffer_->FutureLength() < expand_->overlap_length()) {

    const size_t missing_lookahead_samples =
        expand_->overlap_length() - sync_buffer_->FutureLength();
    RTC_DCHECK_GE(sync_buffer_->next_index(), missing_lookahead_samples);
    sync_buffer_->set_next_index(sync_buffer_->next_index() -
                                 missing_lookahead_samples);
  }
  if (audio_frame->samples_per_channel_ != output_size_samples_) {
    RTC_LOG(LS_ERROR) << "audio_frame->samples_per_channel_ ("
                      << audio_frame->samples_per_channel_
                      << ") != output_size_samples_ (" << output_size_samples_
                      << ")";

    audio_frame->Mute();
    return kSampleUnderrun;
  }

  RTC_DCHECK_GE(sync_buffer_->FutureLength(), expand_->overlap_length());

  if (play_dtmf) {
    return_value = DtmfOverdub(dtmf_event, sync_buffer_->Channels(),
                               audio_frame->mutable_data());
  }

  if ((last_mode_ == Mode::kNormal) || (last_mode_ == Mode::kAccelerateFail) ||
      (last_mode_ == Mode::kPreemptiveExpandFail) ||
      (last_mode_ == Mode::kRfc3389Cng) ||
      (last_mode_ == Mode::kCodecInternalCng)) {
    background_noise_->Update(*sync_buffer_);
  }

  if (operation == Operation::kDtmf) {

    sync_buffer_->set_dtmf_index(sync_buffer_->Size());
  }

  if (last_mode_ != Mode::kExpand && last_mode_ != Mode::kCodecPlc) {

    uint32_t temp_timestamp =
        sync_buffer_->end_timestamp() -
        static_cast<uint32_t>(sync_buffer_->FutureLength());
    if (static_cast<int32_t>(temp_timestamp - playout_timestamp_) > 0) {
      playout_timestamp_ = temp_timestamp;
    }
  } else {

    playout_timestamp_ += static_cast<uint32_t>(output_size_samples_);
  }

  audio_frame->timestamp_ =
      first_packet_
          ? 0
          : timestamp_scaler_->ToExternal(playout_timestamp_) -
                static_cast<uint32_t>(audio_frame->samples_per_channel_);

  if (!(last_mode_ == Mode::kRfc3389Cng ||
        last_mode_ == Mode::kCodecInternalCng || last_mode_ == Mode::kExpand ||
        last_mode_ == Mode::kCodecPlc)) {
    generated_noise_stopwatch_.reset();
  }

  if (decode_return_value)
    return decode_return_value;
  return return_value;
}

int NetEqImpl::GetDecision(Operation* operation,
                           PacketList* packet_list,
                           DtmfEvent* dtmf_event,
                           bool* play_dtmf,
                           absl::optional<Operation> action_override) {

  *play_dtmf = false;
  *operation = Operation::kUndefined;

  RTC_DCHECK(sync_buffer_.get());
  uint32_t end_timestamp = sync_buffer_->end_timestamp();
  if (!new_codec_) {
    const uint32_t five_seconds_samples = 5 * fs_hz_;
    packet_buffer_->DiscardOldPackets(end_timestamp, five_seconds_samples);
  }
  const Packet* packet = packet_buffer_->PeekNextPacket();

  RTC_DCHECK(!generated_noise_stopwatch_ ||
             generated_noise_stopwatch_->ElapsedTicks() >= 1);
  uint64_t generated_noise_samples =
      generated_noise_stopwatch_ ? (generated_noise_stopwatch_->ElapsedTicks() -
                                    1) * output_size_samples_ +
                                       controller_->noise_fast_forward()
                                 : 0;

  if (last_mode_ == Mode::kRfc3389Cng) {

    while (packet && decoder_database_->IsComfortNoise(packet->payload_type) &&
           (end_timestamp >= packet->timestamp ||
            end_timestamp + generated_noise_samples > packet->timestamp)) {

      if (packet_buffer_->DiscardNextPacket() != PacketBuffer::kOK) {
        RTC_DCHECK_NOTREACHED();
      }

      if (!new_codec_) {
        packet_buffer_->DiscardOldPackets(end_timestamp, 5 * fs_hz_);
      }
      packet = packet_buffer_->PeekNextPacket();
    }
  }

  RTC_DCHECK(expand_.get());
  const int samples_left = static_cast<int>(sync_buffer_->FutureLength() -
                                            expand_->overlap_length());
  if (last_mode_ == Mode::kAccelerateSuccess ||
      last_mode_ == Mode::kAccelerateLowEnergy ||
      last_mode_ == Mode::kPreemptiveExpandSuccess ||
      last_mode_ == Mode::kPreemptiveExpandLowEnergy) {

    controller_->AddSampleMemory(
        -(samples_left + rtc::dchecked_cast<int>(output_size_samples_)));
  }

  if (dtmf_buffer_->GetEvent(
          static_cast<uint32_t>(end_timestamp + generated_noise_samples),
          dtmf_event)) {
    *play_dtmf = true;
  }

  RTC_DCHECK(sync_buffer_.get());
  RTC_DCHECK(expand_.get());
  generated_noise_samples =
      generated_noise_stopwatch_
          ? generated_noise_stopwatch_->ElapsedTicks() * output_size_samples_ +
                controller_->noise_fast_forward()
          : 0;
  NetEqController::NetEqStatus status;
  status.packet_buffer_info.dtx_or_cng =
      packet_buffer_->ContainsDtxOrCngPacket(decoder_database_.get());
  status.packet_buffer_info.num_samples =
      packet_buffer_->NumSamplesInBuffer(decoder_frame_length_);
  status.packet_buffer_info.span_samples = packet_buffer_->GetSpanSamples(
      decoder_frame_length_, last_output_sample_rate_hz_, false);
  status.packet_buffer_info.span_samples_wait_time =
      packet_buffer_->GetSpanSamples(decoder_frame_length_,
                                     last_output_sample_rate_hz_, true);
  status.packet_buffer_info.num_packets = packet_buffer_->NumPacketsInBuffer();
  status.target_timestamp = sync_buffer_->end_timestamp();
  status.expand_mutefactor = expand_->MuteFactor(0);
  status.last_packet_samples = decoder_frame_length_;
  status.last_mode = last_mode_;
  status.play_dtmf = *play_dtmf;
  status.generated_noise_samples = generated_noise_samples;
  status.sync_buffer_samples = sync_buffer_->FutureLength();
  if (packet) {
    status.next_packet = {
        packet->timestamp, packet->frame && packet->frame->IsDtxPacket(),
        decoder_database_->IsComfortNoise(packet->payload_type)};
  }
  *operation = controller_->GetDecision(status, &reset_decoder_);

  if (packet && packet->frame && packet->frame->IsDtxPacket() &&
      (*operation == Operation::kMerge ||
       *operation == Operation::kAccelerate ||
       *operation == Operation::kFastAccelerate ||
       *operation == Operation::kPreemptiveExpand)) {
    *operation = Operation::kNormal;
  }

  if (action_override) {

    *operation = *action_override;
  }

  if (samples_left >= rtc::dchecked_cast<int>(output_size_samples_) &&
      *operation != Operation::kMerge && *operation != Operation::kAccelerate &&
      *operation != Operation::kFastAccelerate &&
      *operation != Operation::kPreemptiveExpand) {
    *operation = Operation::kNormal;
    return 0;
  }

  controller_->ExpandDecision(*operation);
  if ((last_mode_ == Mode::kCodecPlc) && (*operation != Operation::kExpand)) {

    stats_->EndExpandEvent(fs_hz_);
  }

  if (new_codec_ || *operation == Operation::kUndefined) {

    RTC_DCHECK(new_codec_);
    if (*play_dtmf && !packet) {
      timestamp_ = dtmf_event->timestamp;
    } else {
      if (!packet) {
        RTC_LOG(LS_ERROR) << "Packet missing where it shouldn't.";
        return -1;
      }
      timestamp_ = packet->timestamp;
      if (*operation == Operation::kRfc3389CngNoPacket &&
          decoder_database_->IsComfortNoise(packet->payload_type)) {

        *operation = Operation::kRfc3389Cng;
      } else if (*operation != Operation::kRfc3389Cng) {
        *operation = Operation::kNormal;
      }
    }

    sync_buffer_->IncreaseEndTimestamp(timestamp_ - end_timestamp);
    end_timestamp = timestamp_;
    new_codec_ = false;
    controller_->SoftReset();
    stats_->ResetMcu();
  }

  size_t required_samples = output_size_samples_;
  const size_t samples_10_ms = static_cast<size_t>(80 * fs_mult_);
  const size_t samples_20_ms = 2 * samples_10_ms;
  const size_t samples_30_ms = 3 * samples_10_ms;

  switch (*operation) {
    case Operation::kExpand: {
      timestamp_ = end_timestamp;
      return 0;
    }
    case Operation::kRfc3389CngNoPacket:
    case Operation::kCodecInternalCng: {
      return 0;
    }
    case Operation::kDtmf: {

      timestamp_ = end_timestamp;
      const uint64_t generated_noise_samples =
          generated_noise_stopwatch_
              ? generated_noise_stopwatch_->ElapsedTicks() *
                        output_size_samples_ +
                    controller_->noise_fast_forward()
              : 0;
      if (generated_noise_samples > 0 && last_mode_ != Mode::kDtmf) {

        uint32_t timestamp_jump =
            static_cast<uint32_t>(generated_noise_samples);
        sync_buffer_->IncreaseEndTimestamp(timestamp_jump);
        timestamp_ += timestamp_jump;
      }
      return 0;
    }
    case Operation::kAccelerate:
    case Operation::kFastAccelerate: {

      if (samples_left >= static_cast<int>(samples_30_ms)) {

        controller_->set_sample_memory(samples_left);
        controller_->set_prev_time_scale(true);
        return 0;
      } else if (samples_left >= static_cast<int>(samples_10_ms) &&
                 decoder_frame_length_ >= samples_30_ms) {

        *operation = Operation::kNormal;
        return 0;
      } else if (samples_left < static_cast<int>(samples_20_ms) &&
                 decoder_frame_length_ < samples_30_ms) {

        required_samples = 2 * output_size_samples_;
        *operation = Operation::kNormal;
      }

      break;
    }
    case Operation::kPreemptiveExpand: {

      if ((samples_left >= static_cast<int>(samples_30_ms)) ||
          (samples_left >= static_cast<int>(samples_10_ms) &&
           decoder_frame_length_ >= samples_30_ms)) {

        controller_->set_sample_memory(samples_left);
        controller_->set_prev_time_scale(true);
        return 0;
      }
      if (samples_left < static_cast<int>(samples_20_ms) &&
          decoder_frame_length_ < samples_30_ms) {

        required_samples = 2 * output_size_samples_;
      }

      break;
    }
    case Operation::kMerge: {
      required_samples =
          std::max(merge_->RequiredFutureSamples(), required_samples);
      break;
    }
    default: {

    }
  }

  int extracted_samples = 0;
  if (packet) {
    sync_buffer_->IncreaseEndTimestamp(packet->timestamp - end_timestamp);
    extracted_samples = ExtractPackets(required_samples, packet_list);
    if (extracted_samples < 0) {
      return kPacketBufferCorruption;
    }
  }

  if (*operation == Operation::kAccelerate ||
      *operation == Operation::kFastAccelerate ||
      *operation == Operation::kPreemptiveExpand) {
    controller_->set_sample_memory(samples_left + extracted_samples);
    controller_->set_prev_time_scale(true);
  }

  if (*operation == Operation::kAccelerate ||
      *operation == Operation::kFastAccelerate) {

    if (extracted_samples + samples_left < static_cast<int>(samples_30_ms)) {

      *operation = Operation::kNormal;
    }
  }

  timestamp_ = sync_buffer_->end_timestamp();
  return 0;
}

int NetEqImpl::Decode(PacketList* packet_list,
                      Operation* operation,
                      int* decoded_length,
                      AudioDecoder::SpeechType* speech_type) {
  *speech_type = AudioDecoder::kSpeech;

  AudioDecoder* decoder = decoder_database_->GetActiveDecoder();

  if (!packet_list->empty()) {
    const Packet& packet = packet_list->front();
    uint8_t payload_type = packet.payload_type;
    if (!decoder_database_->IsComfortNoise(payload_type)) {
      decoder = decoder_database_->GetDecoder(payload_type);
      RTC_DCHECK(decoder);
      if (!decoder) {
        RTC_LOG(LS_WARNING)
            << "Unknown payload type " << static_cast<int>(payload_type);
        packet_list->clear();
        return kDecoderNotFound;
      }
      bool decoder_changed;
      decoder_database_->SetActiveDecoder(payload_type, &decoder_changed);
      if (decoder_changed) {

        const DecoderDatabase::DecoderInfo* decoder_info =
            decoder_database_->GetDecoderInfo(payload_type);
        RTC_DCHECK(decoder_info);
        if (!decoder_info) {
          RTC_LOG(LS_WARNING)
              << "Unknown payload type " << static_cast<int>(payload_type);
          packet_list->clear();
          return kDecoderNotFound;
        }

        if (decoder_info->SampleRateHz() != fs_hz_ ||
            decoder->Channels() != algorithm_buffer_->Channels()) {

          SetSampleRateAndChannels(decoder_info->SampleRateHz(),
                                   decoder->Channels());
        }
        sync_buffer_->set_end_timestamp(timestamp_);
        playout_timestamp_ = timestamp_;
      }
    }
  }

  if (reset_decoder_) {

    if (decoder)
      decoder->Reset();

    ComfortNoiseDecoder* cng_decoder = decoder_database_->GetActiveCngDecoder();
    if (cng_decoder)
      cng_decoder->Reset();

    reset_decoder_ = false;
  }

  *decoded_length = 0;

  if ((*operation == Operation::kMerge) && decoder && decoder->HasDecodePlc()) {
    decoder->DecodePlc(1, &decoded_buffer_[*decoded_length]);
  }

  int return_value;
  if (*operation == Operation::kCodecInternalCng) {
    RTC_DCHECK(packet_list->empty());
    return_value = DecodeCng(decoder, decoded_length, speech_type);
  } else {
    return_value = DecodeLoop(packet_list, *operation, decoder, decoded_length,
                              speech_type);
  }

  if (*decoded_length < 0) {

    *decoded_length = 0;
    sync_buffer_->IncreaseEndTimestamp(
        static_cast<uint32_t>(decoder_frame_length_));
    int error_code = 0;
    if (decoder)
      error_code = decoder->ErrorCode();
    if (error_code != 0) {

      return_value = kDecoderErrorCode;
      RTC_LOG(LS_WARNING) << "Decoder returned error code: " << error_code;
    } else {

      return_value = kOtherDecoderError;
      RTC_LOG(LS_WARNING) << "Decoder error (no error code)";
    }
    *operation = Operation::kExpand;
  }
  if (*speech_type != AudioDecoder::kComfortNoise) {

    RTC_DCHECK(*decoded_length == 0 ||
               (decoder && decoder->Channels() == sync_buffer_->Channels()));
    sync_buffer_->IncreaseEndTimestamp(
        *decoded_length / static_cast<int>(sync_buffer_->Channels()));
  }
  return return_value;
}

int NetEqImpl::DecodeCng(AudioDecoder* decoder,
                         int* decoded_length,
                         AudioDecoder::SpeechType* speech_type) {
  if (!decoder) {

    *decoded_length = -1;
    return 0;
  }

  while (*decoded_length < rtc::dchecked_cast<int>(output_size_samples_)) {
    const int length = decoder->Decode(
        nullptr, 0, fs_hz_,
        (decoded_buffer_length_ - *decoded_length) * sizeof(int16_t),
        &decoded_buffer_[*decoded_length], speech_type);
    if (length > 0) {
      *decoded_length += length;
    } else {

      RTC_LOG(LS_WARNING) << "Failed to decode CNG";
      *decoded_length = -1;
      break;
    }
    if (*decoded_length > static_cast<int>(decoded_buffer_length_)) {

      RTC_LOG(LS_WARNING) << "Decoded too much CNG.";
      return kDecodedTooMuch;
    }
  }
  stats_->GeneratedNoiseSamples(*decoded_length);
  return 0;
}

int NetEqImpl::DecodeLoop(PacketList* packet_list,
                          const Operation& operation,
                          AudioDecoder* decoder,
                          int* decoded_length,
                          AudioDecoder::SpeechType* speech_type) {
  RTC_DCHECK(last_decoded_packet_infos_.empty());

  while (!packet_list->empty() && !decoder_database_->IsComfortNoise(
                                      packet_list->front().payload_type)) {
    RTC_DCHECK(decoder);

    RTC_DCHECK_EQ(sync_buffer_->Channels(), decoder->Channels());
    RTC_DCHECK_GE(decoded_buffer_length_, kMaxFrameSize * decoder->Channels());
    RTC_DCHECK(operation == Operation::kNormal ||
               operation == Operation::kAccelerate ||
               operation == Operation::kFastAccelerate ||
               operation == Operation::kMerge ||
               operation == Operation::kPreemptiveExpand);

    auto opt_result = packet_list->front().frame->Decode(
        rtc::ArrayView<int16_t>(&decoded_buffer_[*decoded_length],
                                decoded_buffer_length_ - *decoded_length));
    if (packet_list->front().packet_info) {
      last_decoded_packet_infos_.push_back(*packet_list->front().packet_info);
    }
    packet_list->pop_front();
    if (opt_result) {
      const auto& result = *opt_result;
      *speech_type = result.speech_type;
      if (result.num_decoded_samples > 0) {
        *decoded_length += rtc::dchecked_cast<int>(result.num_decoded_samples);

        decoder_frame_length_ =
            result.num_decoded_samples / decoder->Channels();
      }
    } else {

      RTC_LOG(LS_WARNING) << "Decode error";
      *decoded_length = -1;
      last_decoded_packet_infos_.clear();
      packet_list->clear();
      break;
    }
    if (*decoded_length > rtc::dchecked_cast<int>(decoded_buffer_length_)) {

      RTC_LOG(LS_WARNING) << "Decoded too much.";
      packet_list->clear();
      return kDecodedTooMuch;
    }
  }

  RTC_DCHECK(
      packet_list->empty() || *decoded_length < 0 ||
      (packet_list->size() == 1 &&
       decoder_database_->IsComfortNoise(packet_list->front().payload_type)));
  return 0;
}

void NetEqImpl::DoNormal(const int16_t* decoded_buffer,
                         size_t decoded_length,
                         AudioDecoder::SpeechType speech_type,
                         bool play_dtmf) {
  RTC_DCHECK(normal_.get());
  normal_->Process(decoded_buffer, decoded_length, last_mode_,
                   algorithm_buffer_.get());
  if (decoded_length != 0) {
    last_mode_ = Mode::kNormal;
  }

  if ((speech_type == AudioDecoder::kComfortNoise) ||
      ((last_mode_ == Mode::kCodecInternalCng) && (decoded_length == 0))) {

    last_mode_ = Mode::kCodecInternalCng;
  }

  if (!play_dtmf) {
    dtmf_tone_generator_->Reset();
  }
}

void NetEqImpl::DoMerge(int16_t* decoded_buffer,
                        size_t decoded_length,
                        AudioDecoder::SpeechType speech_type,
                        bool play_dtmf) {
  RTC_DCHECK(merge_.get());
  size_t new_length =
      merge_->Process(decoded_buffer, decoded_length, algorithm_buffer_.get());

  int expand_length_correction =
      rtc::dchecked_cast<int>(new_length) -
      rtc::dchecked_cast<int>(decoded_length / algorithm_buffer_->Channels());

  if (expand_->Muted() || last_decoded_type_ == AudioDecoder::kComfortNoise) {

    stats_->ExpandedNoiseSamplesCorrection(expand_length_correction);
  } else {

    stats_->ExpandedVoiceSamplesCorrection(expand_length_correction);
  }

  last_mode_ = Mode::kMerge;

  if (speech_type == AudioDecoder::kComfortNoise) {
    last_mode_ = Mode::kCodecInternalCng;
  }
  expand_->Reset();
  if (!play_dtmf) {
    dtmf_tone_generator_->Reset();
  }
}

bool NetEqImpl::DoCodecPlc() {
  AudioDecoder* decoder = decoder_database_->GetActiveDecoder();
  if (!decoder) {
    return false;
  }
  const size_t channels = algorithm_buffer_->Channels();
  const size_t requested_samples_per_channel =
      output_size_samples_ -
      (sync_buffer_->FutureLength() - expand_->overlap_length());
  concealment_audio_.Clear();
  decoder->GeneratePlc(requested_samples_per_channel, &concealment_audio_);
  if (concealment_audio_.empty()) {

    return false;
  }
  RTC_CHECK_GE(concealment_audio_.size(),
               requested_samples_per_channel * channels);
  sync_buffer_->PushBackInterleaved(concealment_audio_);
  RTC_DCHECK_NE(algorithm_buffer_->Channels(), 0);
  const size_t concealed_samples_per_channel =
      concealment_audio_.size() / channels;

  const bool is_new_concealment_event = (last_mode_ != Mode::kCodecPlc);
  if (std::all_of(concealment_audio_.cbegin(), concealment_audio_.cend(),
                  [](int16_t i) { return i == 0; })) {

    stats_->ExpandedNoiseSamples(concealed_samples_per_channel,
                                 is_new_concealment_event);
  } else {

    stats_->ExpandedVoiceSamples(concealed_samples_per_channel,
                                 is_new_concealment_event);
  }
  last_mode_ = Mode::kCodecPlc;
  if (!generated_noise_stopwatch_) {

    generated_noise_stopwatch_ = tick_timer_->GetNewStopwatch();
  }
  return true;
}

int NetEqImpl::DoExpand(bool play_dtmf) {
  while ((sync_buffer_->FutureLength() - expand_->overlap_length()) <
         output_size_samples_) {
    algorithm_buffer_->Clear();
    int return_value = expand_->Process(algorithm_buffer_.get());
    size_t length = algorithm_buffer_->Size();
    bool is_new_concealment_event = (last_mode_ != Mode::kExpand);

    if (expand_->Muted() || last_decoded_type_ == AudioDecoder::kComfortNoise) {

      stats_->ExpandedNoiseSamples(length, is_new_concealment_event);
    } else {

      stats_->ExpandedVoiceSamples(length, is_new_concealment_event);
    }

    last_mode_ = Mode::kExpand;

    if (return_value < 0) {
      return return_value;
    }

    sync_buffer_->PushBack(*algorithm_buffer_);
    algorithm_buffer_->Clear();
  }
  if (!play_dtmf) {
    dtmf_tone_generator_->Reset();
  }

  if (!generated_noise_stopwatch_) {

    generated_noise_stopwatch_ = tick_timer_->GetNewStopwatch();
  }

  return 0;
}

int NetEqImpl::DoAccelerate(int16_t* decoded_buffer,
                            size_t decoded_length,
                            AudioDecoder::SpeechType speech_type,
                            bool play_dtmf,
                            bool fast_accelerate) {
  const size_t required_samples =
      static_cast<size_t>(240 * fs_mult_);
  size_t borrowed_samples_per_channel = 0;
  size_t num_channels = algorithm_buffer_->Channels();
  size_t decoded_length_per_channel = decoded_length / num_channels;
  if (decoded_length_per_channel < required_samples) {

    borrowed_samples_per_channel =
        static_cast<int>(required_samples - decoded_length_per_channel);
    memmove(&decoded_buffer[borrowed_samples_per_channel * num_channels],
            decoded_buffer, sizeof(int16_t) * decoded_length);
    sync_buffer_->ReadInterleavedFromEnd(borrowed_samples_per_channel,
                                         decoded_buffer);
    decoded_length = required_samples * num_channels;
  }

  size_t samples_removed = 0;
  Accelerate::ReturnCodes return_code =
      accelerate_->Process(decoded_buffer, decoded_length, fast_accelerate,
                           algorithm_buffer_.get(), &samples_removed);
  stats_->AcceleratedSamples(samples_removed);
  switch (return_code) {
    case Accelerate::kSuccess:
      last_mode_ = Mode::kAccelerateSuccess;
      break;
    case Accelerate::kSuccessLowEnergy:
      last_mode_ = Mode::kAccelerateLowEnergy;
      break;
    case Accelerate::kNoStretch:
      last_mode_ = Mode::kAccelerateFail;
      break;
    case Accelerate::kError:

      last_mode_ = Mode::kAccelerateFail;
      return kAccelerateError;
  }

  if (borrowed_samples_per_channel > 0) {

    size_t length = algorithm_buffer_->Size();
    if (length < borrowed_samples_per_channel) {

      sync_buffer_->ReplaceAtIndex(
          *algorithm_buffer_,
          sync_buffer_->Size() - borrowed_samples_per_channel);
      sync_buffer_->PushFrontZeros(borrowed_samples_per_channel - length);
      algorithm_buffer_->PopFront(length);
      RTC_DCHECK(algorithm_buffer_->Empty());
    } else {
      sync_buffer_->ReplaceAtIndex(
          *algorithm_buffer_, borrowed_samples_per_channel,
          sync_buffer_->Size() - borrowed_samples_per_channel);
      algorithm_buffer_->PopFront(borrowed_samples_per_channel);
    }
  }

  if (speech_type == AudioDecoder::kComfortNoise) {
    last_mode_ = Mode::kCodecInternalCng;
  }
  if (!play_dtmf) {
    dtmf_tone_generator_->Reset();
  }
  expand_->Reset();
  return 0;
}

int NetEqImpl::DoPreemptiveExpand(int16_t* decoded_buffer,
                                  size_t decoded_length,
                                  AudioDecoder::SpeechType speech_type,
                                  bool play_dtmf) {
  const size_t required_samples =
      static_cast<size_t>(240 * fs_mult_);
  size_t num_channels = algorithm_buffer_->Channels();
  size_t borrowed_samples_per_channel = 0;
  size_t old_borrowed_samples_per_channel = 0;
  size_t decoded_length_per_channel = decoded_length / num_channels;
  if (decoded_length_per_channel < required_samples) {

    borrowed_samples_per_channel =
        required_samples - decoded_length_per_channel;

    old_borrowed_samples_per_channel =
        (borrowed_samples_per_channel > sync_buffer_->FutureLength())
            ? (borrowed_samples_per_channel - sync_buffer_->FutureLength())
            : 0;
    memmove(&decoded_buffer[borrowed_samples_per_channel * num_channels],
            decoded_buffer, sizeof(int16_t) * decoded_length);
    sync_buffer_->ReadInterleavedFromEnd(borrowed_samples_per_channel,
                                         decoded_buffer);
    decoded_length = required_samples * num_channels;
  }

  size_t samples_added = 0;
  PreemptiveExpand::ReturnCodes return_code = preemptive_expand_->Process(
      decoded_buffer, decoded_length, old_borrowed_samples_per_channel,
      algorithm_buffer_.get(), &samples_added);
  stats_->PreemptiveExpandedSamples(samples_added);
  switch (return_code) {
    case PreemptiveExpand::kSuccess:
      last_mode_ = Mode::kPreemptiveExpandSuccess;
      break;
    case PreemptiveExpand::kSuccessLowEnergy:
      last_mode_ = Mode::kPreemptiveExpandLowEnergy;
      break;
    case PreemptiveExpand::kNoStretch:
      last_mode_ = Mode::kPreemptiveExpandFail;
      break;
    case PreemptiveExpand::kError:

      last_mode_ = Mode::kPreemptiveExpandFail;
      return kPreemptiveExpandError;
  }

  if (borrowed_samples_per_channel > 0) {

    sync_buffer_->ReplaceAtIndex(
        *algorithm_buffer_, borrowed_samples_per_channel,
        sync_buffer_->Size() - borrowed_samples_per_channel);
    algorithm_buffer_->PopFront(borrowed_samples_per_channel);
  }

  if (speech_type == AudioDecoder::kComfortNoise) {
    last_mode_ = Mode::kCodecInternalCng;
  }
  if (!play_dtmf) {
    dtmf_tone_generator_->Reset();
  }
  expand_->Reset();
  return 0;
}

int NetEqImpl::DoRfc3389Cng(PacketList* packet_list, bool play_dtmf) {
  if (!packet_list->empty()) {

    RTC_DCHECK_EQ(packet_list->size(), 1);
    const Packet& packet = packet_list->front();
    if (!decoder_database_->IsComfortNoise(packet.payload_type)) {
      RTC_LOG(LS_ERROR) << "Trying to decode non-CNG payload as CNG.";
      return kOtherError;
    }
    if (comfort_noise_->UpdateParameters(packet) ==
        ComfortNoise::kInternalError) {
      algorithm_buffer_->Zeros(output_size_samples_);
      return -comfort_noise_->internal_error_code();
    }
  }
  int cn_return =
      comfort_noise_->Generate(output_size_samples_, algorithm_buffer_.get());
  expand_->Reset();
  last_mode_ = Mode::kRfc3389Cng;
  if (!play_dtmf) {
    dtmf_tone_generator_->Reset();
  }
  if (cn_return == ComfortNoise::kInternalError) {
    RTC_LOG(LS_WARNING) << "Comfort noise generator returned error code: "
                        << comfort_noise_->internal_error_code();
    return kComfortNoiseErrorCode;
  } else if (cn_return == ComfortNoise::kUnknownPayloadType) {
    return kUnknownRtpPayloadType;
  }
  return 0;
}

void NetEqImpl::DoCodecInternalCng(const int16_t* decoded_buffer,
                                   size_t decoded_length) {
  RTC_DCHECK(normal_.get());
  normal_->Process(decoded_buffer, decoded_length, last_mode_,
                   algorithm_buffer_.get());
  last_mode_ = Mode::kCodecInternalCng;
  expand_->Reset();
}

int NetEqImpl::DoDtmf(const DtmfEvent& dtmf_event, bool* play_dtmf) {

  int dtmf_return_value = 0;
  if (!dtmf_tone_generator_->initialized()) {

    dtmf_return_value = dtmf_tone_generator_->Init(fs_hz_, dtmf_event.event_no,
                                                   dtmf_event.volume);
  }

  if (dtmf_return_value == 0) {

    dtmf_return_value = dtmf_tone_generator_->Generate(output_size_samples_,
                                                       algorithm_buffer_.get());
  }

  if (dtmf_return_value < 0) {
    algorithm_buffer_->Zeros(output_size_samples_);
    return dtmf_return_value;
  }

  sync_buffer_->IncreaseEndTimestamp(
      static_cast<uint32_t>(output_size_samples_));
  expand_->Reset();
  last_mode_ = Mode::kDtmf;

  *play_dtmf = false;
  return 0;
}

int NetEqImpl::DtmfOverdub(const DtmfEvent& dtmf_event,
                           size_t num_channels,
                           int16_t* output) const {
  size_t out_index = 0;
  size_t overdub_length = output_size_samples_;

  if (sync_buffer_->dtmf_index() > sync_buffer_->next_index()) {

    out_index =
        std::min(sync_buffer_->dtmf_index() - sync_buffer_->next_index(),
                 output_size_samples_);
    overdub_length = output_size_samples_ - out_index;
  }

  AudioMultiVector dtmf_output(num_channels);
  int dtmf_return_value = 0;
  if (!dtmf_tone_generator_->initialized()) {
    dtmf_return_value = dtmf_tone_generator_->Init(fs_hz_, dtmf_event.event_no,
                                                   dtmf_event.volume);
  }
  if (dtmf_return_value == 0) {
    dtmf_return_value =
        dtmf_tone_generator_->Generate(overdub_length, &dtmf_output);
    RTC_DCHECK_EQ(overdub_length, dtmf_output.Size());
  }
  dtmf_output.ReadInterleaved(overdub_length, &output[out_index]);
  return dtmf_return_value < 0 ? dtmf_return_value : 0;
}

int NetEqImpl::ExtractPackets(size_t required_samples,
                              PacketList* packet_list) {
  bool first_packet = true;
  bool next_packet_available = false;

  const Packet* next_packet = packet_buffer_->PeekNextPacket();
  RTC_DCHECK(next_packet);
  if (!next_packet) {
    RTC_LOG(LS_ERROR) << "Packet buffer unexpectedly empty.";
    return -1;
  }
  uint32_t first_timestamp = next_packet->timestamp;
  size_t extracted_samples = 0;

  do {
    timestamp_ = next_packet->timestamp;
    absl::optional<Packet> packet = packet_buffer_->GetNextPacket();

    next_packet = nullptr;
    if (!packet) {
      RTC_LOG(LS_ERROR) << "Should always be able to extract a packet here";
      RTC_DCHECK_NOTREACHED();

      return -1;
    }
    const uint64_t waiting_time_ms = packet->waiting_time->ElapsedMs();
    stats_->StoreWaitingTime(waiting_time_ms);
    RTC_DCHECK(!packet->empty());

    if (first_packet) {
      first_packet = false;
      if (nack_enabled_) {
        RTC_DCHECK(nack_);

        nack_->UpdateLastDecodedPacket(packet->sequence_number,
                                       packet->timestamp);
      }
    }

    const bool has_cng_packet =
        decoder_database_->IsComfortNoise(packet->payload_type);

    size_t packet_duration = 0;
    if (packet->frame) {
      packet_duration = packet->frame->Duration();

      if (packet->priority.codec_level > 0) {
        stats_->SecondaryDecodedSamples(
            rtc::dchecked_cast<int>(packet_duration));
      }
    } else if (!has_cng_packet) {
      RTC_LOG(LS_WARNING) << "Unknown payload type "
                          << static_cast<int>(packet->payload_type);
      RTC_DCHECK_NOTREACHED();
    }

    if (packet_duration == 0) {

      packet_duration = decoder_frame_length_;
    }
    extracted_samples = packet->timestamp - first_timestamp + packet_duration;

    RTC_DCHECK(controller_);
    stats_->JitterBufferDelay(packet_duration, waiting_time_ms,
                              controller_->TargetLevelMs(),
                              controller_->UnlimitedTargetLevelMs());

    next_packet = packet_buffer_->PeekNextPacket();
    next_packet_available =
        next_packet && next_packet->payload_type == packet->payload_type &&
        next_packet->timestamp == packet->timestamp + packet_duration &&
        !has_cng_packet;

    packet_list->push_back(std::move(*packet));
    packet = absl::nullopt;
  } while (extracted_samples < required_samples && next_packet_available);

  if (extracted_samples > 0) {

    packet_buffer_->DiscardAllOldPackets(timestamp_);
  }

  return rtc::dchecked_cast<int>(extracted_samples);
}

void NetEqImpl::UpdatePlcComponents(int fs_hz, size_t channels) {

  expand_.reset(expand_factory_->Create(background_noise_.get(),
                                        sync_buffer_.get(), &random_vector_,
                                        stats_.get(), fs_hz, channels));
  merge_.reset(new Merge(fs_hz, channels, expand_.get(), sync_buffer_.get()));
}

void NetEqImpl::SetSampleRateAndChannels(int fs_hz, size_t channels) {
  RTC_LOG(LS_VERBOSE) << "SetSampleRateAndChannels " << fs_hz << " "
                      << channels;

  RTC_DCHECK(fs_hz == 8000 || fs_hz == 16000 || fs_hz == 32000 ||
             fs_hz == 48000);
  RTC_DCHECK_GT(channels, 0);

  stats_->EndExpandEvent(fs_hz_);
  fs_hz_ = fs_hz;
  fs_mult_ = fs_hz / 8000;
  output_size_samples_ = static_cast<size_t>(kOutputSizeMs * 8 * fs_mult_);
  decoder_frame_length_ = 3 * output_size_samples_;

  last_mode_ = Mode::kNormal;

  ComfortNoiseDecoder* cng_decoder = decoder_database_->GetActiveCngDecoder();
  if (cng_decoder)
    cng_decoder->Reset();

  algorithm_buffer_.reset(new AudioMultiVector(channels));

  sync_buffer_.reset(new SyncBuffer(channels, kSyncBufferSize * fs_mult_));

  background_noise_.reset(new BackgroundNoise(channels));

  random_vector_.Reset();

  UpdatePlcComponents(fs_hz, channels);

  sync_buffer_->set_next_index(sync_buffer_->next_index() -
                               expand_->overlap_length());

  normal_.reset(new Normal(fs_hz, decoder_database_.get(), *background_noise_,
                           expand_.get(), stats_.get()));
  accelerate_.reset(
      accelerate_factory_->Create(fs_hz, channels, *background_noise_));
  preemptive_expand_.reset(preemptive_expand_factory_->Create(
      fs_hz, channels, *background_noise_, expand_->overlap_length()));

  comfort_noise_.reset(
      new ComfortNoise(fs_hz, decoder_database_.get(), sync_buffer_.get()));

  if (decoded_buffer_length_ < kMaxFrameSize * channels) {

    decoded_buffer_length_ = kMaxFrameSize * channels;
    decoded_buffer_.reset(new int16_t[decoded_buffer_length_]);
  }
  RTC_CHECK(controller_) << "Unexpectedly found no NetEqController";
  controller_->SetSampleRate(fs_hz_, output_size_samples_);
}

NetEqImpl::OutputType NetEqImpl::LastOutputType() {
  RTC_DCHECK(expand_.get());
  if (last_mode_ == Mode::kCodecInternalCng ||
      last_mode_ == Mode::kRfc3389Cng) {
    return OutputType::kCNG;
  } else if (last_mode_ == Mode::kExpand && expand_->MuteFactor(0) == 0) {

    return OutputType::kPLCCNG;
  } else if (last_mode_ == Mode::kExpand) {
    return OutputType::kPLC;
  } else if (last_mode_ == Mode::kCodecPlc) {
    return OutputType::kCodecPLC;
  } else {
    return OutputType::kNormalSpeech;
  }
}

NetEqController::PacketArrivedInfo NetEqImpl::ToPacketArrivedInfo(
    const Packet& packet) const {
  const DecoderDatabase::DecoderInfo* dec_info =
      decoder_database_->GetDecoderInfo(packet.payload_type);

  NetEqController::PacketArrivedInfo info;
  info.is_cng_or_dtmf =
      dec_info && (dec_info->IsComfortNoise() || dec_info->IsDtmf());
  info.packet_length_samples =
      packet.frame ? packet.frame->Duration() : decoder_frame_length_;
  info.main_timestamp = packet.timestamp;
  info.main_sequence_number = packet.sequence_number;
  info.is_dtx = packet.frame && packet.frame->IsDtxPacket();
  return info;
}

}
