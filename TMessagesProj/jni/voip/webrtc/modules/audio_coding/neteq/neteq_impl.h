/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_CODING_NETEQ_NETEQ_IMPL_H_
#define MODULES_AUDIO_CODING_NETEQ_NETEQ_IMPL_H_

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/types/optional.h"
#include "api/audio/audio_frame.h"
#include "api/neteq/neteq.h"
#include "api/neteq/neteq_controller.h"
#include "api/neteq/neteq_controller_factory.h"
#include "api/neteq/tick_timer.h"
#include "api/rtp_packet_info.h"
#include "modules/audio_coding/neteq/audio_multi_vector.h"
#include "modules/audio_coding/neteq/expand_uma_logger.h"
#include "modules/audio_coding/neteq/packet.h"
#include "modules/audio_coding/neteq/packet_buffer.h"
#include "modules/audio_coding/neteq/random_vector.h"
#include "modules/audio_coding/neteq/statistics_calculator.h"
#include "rtc_base/synchronization/mutex.h"
#include "rtc_base/thread_annotations.h"

namespace webrtc {

class Accelerate;
class BackgroundNoise;
class Clock;
class ComfortNoise;
class DecoderDatabase;
class DtmfBuffer;
class DtmfToneGenerator;
class Expand;
class Merge;
class NackTracker;
class Normal;
class RedPayloadSplitter;
class PreemptiveExpand;
class RandomVector;
class SyncBuffer;
class TimestampScaler;
struct AccelerateFactory;
struct DtmfEvent;
struct ExpandFactory;
struct PreemptiveExpandFactory;

class NetEqImpl : public webrtc::NetEq {
 public:
  enum class OutputType {
    kNormalSpeech,
    kPLC,
    kCNG,
    kPLCCNG,
    kVadPassive,
    kCodecPLC
  };

  enum ErrorCodes {
    kNoError = 0,
    kOtherError,
    kUnknownRtpPayloadType,
    kDecoderNotFound,
    kInvalidPointer,
    kAccelerateError,
    kPreemptiveExpandError,
    kComfortNoiseErrorCode,
    kDecoderErrorCode,
    kOtherDecoderError,
    kInvalidOperation,
    kDtmfParsingError,
    kDtmfInsertError,
    kSampleUnderrun,
    kDecodedTooMuch,
    kRedundancySplitError,
    kPacketBufferCorruption
  };

  struct Dependencies {

    Dependencies(const NetEq::Config& config,
                 Clock* clock,
                 const rtc::scoped_refptr<AudioDecoderFactory>& decoder_factory,
                 const NetEqControllerFactory& controller_factory);
    ~Dependencies();

    Clock* const clock;
    std::unique_ptr<TickTimer> tick_timer;
    std::unique_ptr<StatisticsCalculator> stats;
    std::unique_ptr<DecoderDatabase> decoder_database;
    std::unique_ptr<DtmfBuffer> dtmf_buffer;
    std::unique_ptr<DtmfToneGenerator> dtmf_tone_generator;
    std::unique_ptr<PacketBuffer> packet_buffer;
    std::unique_ptr<NetEqController> neteq_controller;
    std::unique_ptr<RedPayloadSplitter> red_payload_splitter;
    std::unique_ptr<TimestampScaler> timestamp_scaler;
    std::unique_ptr<AccelerateFactory> accelerate_factory;
    std::unique_ptr<ExpandFactory> expand_factory;
    std::unique_ptr<PreemptiveExpandFactory> preemptive_expand_factory;
  };

  NetEqImpl(const NetEq::Config& config,
            Dependencies&& deps,
            bool create_components = true);

  ~NetEqImpl() override;

  NetEqImpl(const NetEqImpl&) = delete;
  NetEqImpl& operator=(const NetEqImpl&) = delete;

  int InsertPacket(const RTPHeader& rtp_header,
                   rtc::ArrayView<const uint8_t> payload) override;

  void InsertEmptyPacket(const RTPHeader& rtp_header) override;

  int GetAudio(
      AudioFrame* audio_frame,
      bool* muted,
      int* current_sample_rate_hz = nullptr,
      absl::optional<Operation> action_override = absl::nullopt) override;

  void SetCodecs(const std::map<int, SdpAudioFormat>& codecs) override;

  bool RegisterPayloadType(int rtp_payload_type,
                           const SdpAudioFormat& audio_format) override;

  int RemovePayloadType(uint8_t rtp_payload_type) override;

  void RemoveAllPayloadTypes() override;

  bool SetMinimumDelay(int delay_ms) override;

  bool SetMaximumDelay(int delay_ms) override;

  bool SetBaseMinimumDelayMs(int delay_ms) override;

  int GetBaseMinimumDelayMs() const override;

  int TargetDelayMs() const override;

  int FilteredCurrentDelayMs() const override;

  int NetworkStatistics(NetEqNetworkStatistics* stats) override;

  NetEqNetworkStatistics CurrentNetworkStatistics() const override;

  NetEqLifetimeStatistics GetLifetimeStatistics() const override;

  NetEqOperationsAndState GetOperationsAndState() const override;

  absl::optional<uint32_t> GetPlayoutTimestamp() const override;

  int last_output_sample_rate_hz() const override;

  absl::optional<DecoderFormat> GetDecoderFormat(
      int payload_type) const override;

  void FlushBuffers() override;

  void EnableNack(size_t max_nack_list_size) override;

  void DisableNack() override;

  std::vector<uint16_t> GetNackList(int64_t round_trip_time_ms) const override;

  int SyncBufferSizeMs() const override;

  const SyncBuffer* sync_buffer_for_test() const;
  Operation last_operation_for_test() const;

 protected:
  static const int kOutputSizeMs = 10;
  static const size_t kMaxFrameSize = 5760;

  static const size_t kSyncBufferSize = kMaxFrameSize + 60 * 48;

  int InsertPacketInternal(const RTPHeader& rtp_header,
                           rtc::ArrayView<const uint8_t> payload)
      RTC_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  bool MaybeChangePayloadType(uint8_t payload_type)
      RTC_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  int GetAudioInternal(AudioFrame* audio_frame,
                       bool* muted,
                       absl::optional<Operation> action_override)
      RTC_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  int GetDecision(Operation* operation,
                  PacketList* packet_list,
                  DtmfEvent* dtmf_event,
                  bool* play_dtmf,
                  absl::optional<Operation> action_override)
      RTC_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  int Decode(PacketList* packet_list,
             Operation* operation,
             int* decoded_length,
             AudioDecoder::SpeechType* speech_type)
      RTC_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  int DecodeCng(AudioDecoder* decoder,
                int* decoded_length,
                AudioDecoder::SpeechType* speech_type)
      RTC_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  int DecodeLoop(PacketList* packet_list,
                 const Operation& operation,
                 AudioDecoder* decoder,
                 int* decoded_length,
                 AudioDecoder::SpeechType* speech_type)
      RTC_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  void DoNormal(const int16_t* decoded_buffer,
                size_t decoded_length,
                AudioDecoder::SpeechType speech_type,
                bool play_dtmf) RTC_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  void DoMerge(int16_t* decoded_buffer,
               size_t decoded_length,
               AudioDecoder::SpeechType speech_type,
               bool play_dtmf) RTC_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  bool DoCodecPlc() RTC_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  int DoExpand(bool play_dtmf) RTC_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  int DoAccelerate(int16_t* decoded_buffer,
                   size_t decoded_length,
                   AudioDecoder::SpeechType speech_type,
                   bool play_dtmf,
                   bool fast_accelerate) RTC_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  int DoPreemptiveExpand(int16_t* decoded_buffer,
                         size_t decoded_length,
                         AudioDecoder::SpeechType speech_type,
                         bool play_dtmf) RTC_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  int DoRfc3389Cng(PacketList* packet_list, bool play_dtmf)
      RTC_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  void DoCodecInternalCng(const int16_t* decoded_buffer, size_t decoded_length)
      RTC_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  int DoDtmf(const DtmfEvent& dtmf_event, bool* play_dtmf)
      RTC_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  int DtmfOverdub(const DtmfEvent& dtmf_event,
                  size_t num_channels,
                  int16_t* output) const RTC_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  int ExtractPackets(size_t required_samples, PacketList* packet_list)
      RTC_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  void SetSampleRateAndChannels(int fs_hz, size_t channels)
      RTC_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  OutputType LastOutputType() RTC_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  virtual void UpdatePlcComponents(int fs_hz, size_t channels)
      RTC_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  NetEqNetworkStatistics CurrentNetworkStatisticsInternal() const
      RTC_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  NetEqController::PacketArrivedInfo ToPacketArrivedInfo(
      const Packet& packet) const RTC_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  Clock* const clock_;

  mutable Mutex mutex_;
  const std::unique_ptr<TickTimer> tick_timer_ RTC_GUARDED_BY(mutex_);
  const std::unique_ptr<DecoderDatabase> decoder_database_
      RTC_GUARDED_BY(mutex_);
  const std::unique_ptr<DtmfBuffer> dtmf_buffer_ RTC_GUARDED_BY(mutex_);
  const std::unique_ptr<DtmfToneGenerator> dtmf_tone_generator_
      RTC_GUARDED_BY(mutex_);
  const std::unique_ptr<PacketBuffer> packet_buffer_ RTC_GUARDED_BY(mutex_);
  const std::unique_ptr<RedPayloadSplitter> red_payload_splitter_
      RTC_GUARDED_BY(mutex_);
  const std::unique_ptr<TimestampScaler> timestamp_scaler_
      RTC_GUARDED_BY(mutex_);
  const std::unique_ptr<ExpandFactory> expand_factory_ RTC_GUARDED_BY(mutex_);
  const std::unique_ptr<AccelerateFactory> accelerate_factory_
      RTC_GUARDED_BY(mutex_);
  const std::unique_ptr<PreemptiveExpandFactory> preemptive_expand_factory_
      RTC_GUARDED_BY(mutex_);
  const std::unique_ptr<StatisticsCalculator> stats_ RTC_GUARDED_BY(mutex_);
  const bool enable_fec_delay_adaptation_ RTC_GUARDED_BY(mutex_);

  std::unique_ptr<BackgroundNoise> background_noise_ RTC_GUARDED_BY(mutex_);
  std::unique_ptr<NetEqController> controller_ RTC_GUARDED_BY(mutex_);
  std::unique_ptr<AudioMultiVector> algorithm_buffer_ RTC_GUARDED_BY(mutex_);
  std::unique_ptr<SyncBuffer> sync_buffer_ RTC_GUARDED_BY(mutex_);
  std::unique_ptr<Expand> expand_ RTC_GUARDED_BY(mutex_);
  std::unique_ptr<Normal> normal_ RTC_GUARDED_BY(mutex_);
  std::unique_ptr<Merge> merge_ RTC_GUARDED_BY(mutex_);
  std::unique_ptr<Accelerate> accelerate_ RTC_GUARDED_BY(mutex_);
  std::unique_ptr<PreemptiveExpand> preemptive_expand_ RTC_GUARDED_BY(mutex_);
  RandomVector random_vector_ RTC_GUARDED_BY(mutex_);
  std::unique_ptr<ComfortNoise> comfort_noise_ RTC_GUARDED_BY(mutex_);
  int fs_hz_ RTC_GUARDED_BY(mutex_);
  int fs_mult_ RTC_GUARDED_BY(mutex_);
  int last_output_sample_rate_hz_ RTC_GUARDED_BY(mutex_);
  size_t output_size_samples_ RTC_GUARDED_BY(mutex_);
  size_t decoder_frame_length_ RTC_GUARDED_BY(mutex_);
  Mode last_mode_ RTC_GUARDED_BY(mutex_);
  Operation last_operation_ RTC_GUARDED_BY(mutex_);
  absl::optional<AudioDecoder::SpeechType> last_decoded_type_
      RTC_GUARDED_BY(mutex_);
  size_t decoded_buffer_length_ RTC_GUARDED_BY(mutex_);
  std::unique_ptr<int16_t[]> decoded_buffer_ RTC_GUARDED_BY(mutex_);
  uint32_t playout_timestamp_ RTC_GUARDED_BY(mutex_);
  bool new_codec_ RTC_GUARDED_BY(mutex_);
  uint32_t timestamp_ RTC_GUARDED_BY(mutex_);
  bool reset_decoder_ RTC_GUARDED_BY(mutex_);
  absl::optional<uint8_t> current_rtp_payload_type_ RTC_GUARDED_BY(mutex_);
  absl::optional<uint8_t> current_cng_rtp_payload_type_ RTC_GUARDED_BY(mutex_);
  bool first_packet_ RTC_GUARDED_BY(mutex_);
  bool enable_fast_accelerate_ RTC_GUARDED_BY(mutex_);
  std::unique_ptr<NackTracker> nack_ RTC_GUARDED_BY(mutex_);
  bool nack_enabled_ RTC_GUARDED_BY(mutex_);
  const bool enable_muted_state_ RTC_GUARDED_BY(mutex_);
  std::unique_ptr<TickTimer::Stopwatch> generated_noise_stopwatch_
      RTC_GUARDED_BY(mutex_);
  std::vector<RtpPacketInfo> last_decoded_packet_infos_ RTC_GUARDED_BY(mutex_);
  ExpandUmaLogger expand_uma_logger_ RTC_GUARDED_BY(mutex_);
  ExpandUmaLogger speech_expand_uma_logger_ RTC_GUARDED_BY(mutex_);
  bool no_time_stretching_ RTC_GUARDED_BY(mutex_);
  rtc::BufferT<int16_t> concealment_audio_ RTC_GUARDED_BY(mutex_);
};

}
#endif
