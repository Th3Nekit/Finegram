/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_coding/include/audio_coding_module.h"

#include <algorithm>
#include <cstdint>

#include "absl/strings/match.h"
#include "absl/strings/string_view.h"
#include "api/array_view.h"
#include "modules/audio_coding/acm2/acm_remixing.h"
#include "modules/audio_coding/acm2/acm_resampler.h"
#include "modules/include/module_common_types.h"
#include "modules/include/module_common_types_public.h"
#include "rtc_base/buffer.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/numerics/safe_conversions.h"
#include "rtc_base/synchronization/mutex.h"
#include "rtc_base/thread_annotations.h"
#include "system_wrappers/include/metrics.h"

namespace webrtc {

namespace {

constexpr size_t kInitialInputDataBufferSize = 6 * 480;

constexpr int32_t kMaxInputSampleRateHz = 192000;

class AudioCodingModuleImpl final : public AudioCodingModule {
 public:
  explicit AudioCodingModuleImpl();
  ~AudioCodingModuleImpl() override;

  void ModifyEncoder(rtc::FunctionView<void(std::unique_ptr<AudioEncoder>*)>
                         modifier) override;

  int RegisterTransportCallback(AudioPacketizationCallback* transport) override;

  int Add10MsData(const AudioFrame& audio_frame) override;

  int SetPacketLossRate(int loss_rate) override;

  ANAStats GetANAStats() const override;

  int GetTargetBitrate() const override;

 private:
  struct InputData {
    InputData() : buffer(kInitialInputDataBufferSize) {}
    uint32_t input_timestamp;
    const int16_t* audio;
    size_t length_per_channel;
    size_t audio_channel;

    std::vector<int16_t> buffer;
  };

  InputData input_data_ RTC_GUARDED_BY(acm_mutex_);

  class ChangeLogger {
   public:
    explicit ChangeLogger(absl::string_view histogram_name)
        : histogram_name_(histogram_name) {}

    void MaybeLog(int value);

   private:
    int last_value_ = 0;
    int first_time_ = true;
    const std::string histogram_name_;
  };

  int Add10MsDataInternal(const AudioFrame& audio_frame, InputData* input_data)
      RTC_EXCLUSIVE_LOCKS_REQUIRED(acm_mutex_);

  int Encode(const InputData& input_data,
             absl::optional<int64_t> absolute_capture_timestamp_ms)
      RTC_EXCLUSIVE_LOCKS_REQUIRED(acm_mutex_);

  bool HaveValidEncoder(absl::string_view caller_name) const
      RTC_EXCLUSIVE_LOCKS_REQUIRED(acm_mutex_);

  int PreprocessToAddData(const AudioFrame& in_frame,
                          const AudioFrame** ptr_out)
      RTC_EXCLUSIVE_LOCKS_REQUIRED(acm_mutex_);

  int UpdateUponReceivingCodec(int index);

  mutable Mutex acm_mutex_;
  rtc::Buffer encode_buffer_ RTC_GUARDED_BY(acm_mutex_);
  uint32_t expected_codec_ts_ RTC_GUARDED_BY(acm_mutex_);
  uint32_t expected_in_ts_ RTC_GUARDED_BY(acm_mutex_);
  acm2::ACMResampler resampler_ RTC_GUARDED_BY(acm_mutex_);
  ChangeLogger bitrate_logger_ RTC_GUARDED_BY(acm_mutex_);

  std::unique_ptr<AudioEncoder> encoder_stack_ RTC_GUARDED_BY(acm_mutex_);

  uint8_t previous_pltype_ RTC_GUARDED_BY(acm_mutex_);

  AudioFrame preprocess_frame_ RTC_GUARDED_BY(acm_mutex_);
  bool first_10ms_data_ RTC_GUARDED_BY(acm_mutex_);

  bool first_frame_ RTC_GUARDED_BY(acm_mutex_);
  uint32_t last_timestamp_ RTC_GUARDED_BY(acm_mutex_);
  uint32_t last_rtp_timestamp_ RTC_GUARDED_BY(acm_mutex_);

  Mutex callback_mutex_;
  AudioPacketizationCallback* packetization_callback_
      RTC_GUARDED_BY(callback_mutex_);

  int codec_histogram_bins_log_[static_cast<size_t>(
      AudioEncoder::CodecType::kMaxLoggedAudioCodecTypes)];
  int number_of_consecutive_empty_packets_;
};

void UpdateCodecTypeHistogram(size_t codec_type) {
  RTC_HISTOGRAM_ENUMERATION(
      "WebRTC.Audio.Encoder.CodecType", static_cast<int>(codec_type),
      static_cast<int>(
          webrtc::AudioEncoder::CodecType::kMaxLoggedAudioCodecTypes));
}

void AudioCodingModuleImpl::ChangeLogger::MaybeLog(int value) {
  if (value != last_value_ || first_time_) {
    first_time_ = false;
    last_value_ = value;
    RTC_HISTOGRAM_COUNTS_SPARSE_100(histogram_name_, value);
  }
}

AudioCodingModuleImpl::AudioCodingModuleImpl()
    : expected_codec_ts_(0xD87F3F9F),
      expected_in_ts_(0xD87F3F9F),
      bitrate_logger_("WebRTC.Audio.TargetBitrateInKbps"),
      encoder_stack_(nullptr),
      previous_pltype_(255),
      first_10ms_data_(false),
      first_frame_(true),
      packetization_callback_(NULL),
      codec_histogram_bins_log_(),
      number_of_consecutive_empty_packets_(0) {
  RTC_LOG(LS_INFO) << "Created";
}

AudioCodingModuleImpl::~AudioCodingModuleImpl() = default;

int32_t AudioCodingModuleImpl::Encode(
    const InputData& input_data,
    absl::optional<int64_t> absolute_capture_timestamp_ms) {

  AudioEncoder::EncodedInfo encoded_info;
  uint8_t previous_pltype;

  if (!HaveValidEncoder("Process"))
    return -1;

  if (!first_frame_) {
    RTC_DCHECK(IsNewerTimestamp(input_data.input_timestamp, last_timestamp_))
        << "Time should not move backwards";
  }

  uint32_t rtp_timestamp =
      first_frame_
          ? input_data.input_timestamp
          : last_rtp_timestamp_ +
                rtc::dchecked_cast<uint32_t>(rtc::CheckedDivExact(
                    int64_t{input_data.input_timestamp - last_timestamp_} *
                        encoder_stack_->RtpTimestampRateHz(),
                    int64_t{encoder_stack_->SampleRateHz()}));

  last_timestamp_ = input_data.input_timestamp;
  last_rtp_timestamp_ = rtp_timestamp;
  first_frame_ = false;

  encode_buffer_.Clear();
  encoded_info = encoder_stack_->Encode(
      rtp_timestamp,
      rtc::ArrayView<const int16_t>(
          input_data.audio,
          input_data.audio_channel * input_data.length_per_channel),
      &encode_buffer_);

  bitrate_logger_.MaybeLog(encoder_stack_->GetTargetBitrate() / 1000);
  if (encode_buffer_.size() == 0 && !encoded_info.send_even_if_empty) {

    return 0;
  }
  previous_pltype = previous_pltype_;

  if (encoded_info.encoded_bytes == 0) {
    ++number_of_consecutive_empty_packets_;
  } else {
    size_t codec_type = static_cast<size_t>(encoded_info.encoder_type);
    codec_histogram_bins_log_[codec_type] +=
        number_of_consecutive_empty_packets_ + 1;
    number_of_consecutive_empty_packets_ = 0;
    if (codec_histogram_bins_log_[codec_type] >= 500) {
      codec_histogram_bins_log_[codec_type] -= 500;
      UpdateCodecTypeHistogram(codec_type);
    }
  }

  AudioFrameType frame_type;
  if (encode_buffer_.size() == 0 && encoded_info.send_even_if_empty) {
    frame_type = AudioFrameType::kEmptyFrame;
    encoded_info.payload_type = previous_pltype;
  } else {
    RTC_DCHECK_GT(encode_buffer_.size(), 0);
    frame_type = encoded_info.speech ? AudioFrameType::kAudioFrameSpeech
                                     : AudioFrameType::kAudioFrameCN;
  }

  {
    MutexLock lock(&callback_mutex_);
    if (packetization_callback_) {
      packetization_callback_->SendData(
          frame_type, encoded_info.payload_type, encoded_info.encoded_timestamp,
          encode_buffer_.data(), encode_buffer_.size(),
          absolute_capture_timestamp_ms.value_or(-1));
    }
  }
  previous_pltype_ = encoded_info.payload_type;
  return static_cast<int32_t>(encode_buffer_.size());
}

void AudioCodingModuleImpl::ModifyEncoder(
    rtc::FunctionView<void(std::unique_ptr<AudioEncoder>*)> modifier) {
  MutexLock lock(&acm_mutex_);
  modifier(&encoder_stack_);
}

int AudioCodingModuleImpl::RegisterTransportCallback(
    AudioPacketizationCallback* transport) {
  MutexLock lock(&callback_mutex_);
  packetization_callback_ = transport;
  return 0;
}

int AudioCodingModuleImpl::Add10MsData(const AudioFrame& audio_frame) {
  MutexLock lock(&acm_mutex_);
  int r = Add10MsDataInternal(audio_frame, &input_data_);

  return r < 0
             ? r
             : Encode(input_data_, audio_frame.absolute_capture_timestamp_ms());
}

int AudioCodingModuleImpl::Add10MsDataInternal(const AudioFrame& audio_frame,
                                               InputData* input_data) {
  if (audio_frame.samples_per_channel_ == 0) {
    RTC_DCHECK_NOTREACHED();
    RTC_LOG(LS_ERROR) << "Cannot Add 10 ms audio, payload length is zero";
    return -1;
  }

  if (audio_frame.sample_rate_hz_ > kMaxInputSampleRateHz) {
    RTC_DCHECK_NOTREACHED();
    RTC_LOG(LS_ERROR) << "Cannot Add 10 ms audio, input frequency not valid";
    return -1;
  }

  if (static_cast<size_t>(audio_frame.sample_rate_hz_ / 100) !=
      audio_frame.samples_per_channel_) {
    RTC_LOG(LS_ERROR)
        << "Cannot Add 10 ms audio, input frequency and length doesn't match";
    return -1;
  }

  if (audio_frame.num_channels_ != 1 && audio_frame.num_channels_ != 2 &&
      audio_frame.num_channels_ != 4 && audio_frame.num_channels_ != 6 &&
      audio_frame.num_channels_ != 8) {
    RTC_LOG(LS_ERROR) << "Cannot Add 10 ms audio, invalid number of channels.";
    return -1;
  }

  if (!HaveValidEncoder("Add10MsData")) {
    return -1;
  }

  const AudioFrame* ptr_frame;

  if (PreprocessToAddData(audio_frame, &ptr_frame) < 0) {
    return -1;
  }

  const size_t current_num_channels = encoder_stack_->NumChannels();
  const bool same_num_channels =
      ptr_frame->num_channels_ == current_num_channels;

  input_data->input_timestamp = ptr_frame->timestamp_;
  input_data->length_per_channel = ptr_frame->samples_per_channel_;
  input_data->audio_channel = current_num_channels;

  if (!same_num_channels) {

    ReMixFrame(*ptr_frame, current_num_channels, &input_data->buffer);

    input_data->audio = input_data->buffer.data();
    RTC_DCHECK_GE(input_data->buffer.size(),
                  input_data->length_per_channel * input_data->audio_channel);
  } else {

    input_data->audio = ptr_frame->data();
  }

  return 0;
}

int AudioCodingModuleImpl::PreprocessToAddData(const AudioFrame& in_frame,
                                               const AudioFrame** ptr_out) {
  const bool resample =
      in_frame.sample_rate_hz_ != encoder_stack_->SampleRateHz();

  const bool down_mix =
      in_frame.num_channels_ == 2 && encoder_stack_->NumChannels() == 1;

  if (!first_10ms_data_) {
    expected_in_ts_ = in_frame.timestamp_;
    expected_codec_ts_ = in_frame.timestamp_;
    first_10ms_data_ = true;
  } else if (in_frame.timestamp_ != expected_in_ts_) {
    RTC_LOG(LS_WARNING) << "Unexpected input timestamp: " << in_frame.timestamp_
                        << ", expected: " << expected_in_ts_;
    expected_codec_ts_ +=
        (in_frame.timestamp_ - expected_in_ts_) *
        static_cast<uint32_t>(
            static_cast<double>(encoder_stack_->SampleRateHz()) /
            static_cast<double>(in_frame.sample_rate_hz_));
    expected_in_ts_ = in_frame.timestamp_;
  }

  if (!down_mix && !resample) {

    if (expected_in_ts_ == expected_codec_ts_) {

      *ptr_out = &in_frame;
    } else {

      preprocess_frame_.CopyFrom(in_frame);
      preprocess_frame_.timestamp_ = expected_codec_ts_;
      *ptr_out = &preprocess_frame_;
    }

    expected_in_ts_ += static_cast<uint32_t>(in_frame.samples_per_channel_);
    expected_codec_ts_ += static_cast<uint32_t>(in_frame.samples_per_channel_);
    return 0;
  }

  *ptr_out = &preprocess_frame_;
  preprocess_frame_.num_channels_ = in_frame.num_channels_;
  preprocess_frame_.samples_per_channel_ = in_frame.samples_per_channel_;
  std::array<int16_t, AudioFrame::kMaxDataSizeSamples> audio;
  const int16_t* src_ptr_audio;
  if (down_mix) {

    int16_t* dest_ptr_audio =
        resample ? audio.data() : preprocess_frame_.mutable_data();
    RTC_DCHECK_GE(audio.size(), preprocess_frame_.samples_per_channel_);
    RTC_DCHECK_GE(audio.size(), in_frame.samples_per_channel_);
    DownMixFrame(in_frame,
                 rtc::ArrayView<int16_t>(
                     dest_ptr_audio, preprocess_frame_.samples_per_channel_));
    preprocess_frame_.num_channels_ = 1;

    src_ptr_audio = audio.data();
  } else {

    src_ptr_audio = in_frame.data();
  }

  preprocess_frame_.timestamp_ = expected_codec_ts_;
  preprocess_frame_.sample_rate_hz_ = in_frame.sample_rate_hz_;

  if (resample) {

    int16_t* dest_ptr_audio = preprocess_frame_.mutable_data();

    int samples_per_channel = resampler_.Resample10Msec(
        src_ptr_audio, in_frame.sample_rate_hz_, encoder_stack_->SampleRateHz(),
        preprocess_frame_.num_channels_, AudioFrame::kMaxDataSizeSamples,
        dest_ptr_audio);

    if (samples_per_channel < 0) {
      RTC_LOG(LS_ERROR) << "Cannot add 10 ms audio, resampling failed";
      return -1;
    }
    preprocess_frame_.samples_per_channel_ =
        static_cast<size_t>(samples_per_channel);
    preprocess_frame_.sample_rate_hz_ = encoder_stack_->SampleRateHz();
  }

  expected_codec_ts_ +=
      static_cast<uint32_t>(preprocess_frame_.samples_per_channel_);
  expected_in_ts_ += static_cast<uint32_t>(in_frame.samples_per_channel_);

  return 0;
}

int AudioCodingModuleImpl::SetPacketLossRate(int loss_rate) {
  MutexLock lock(&acm_mutex_);
  if (HaveValidEncoder("SetPacketLossRate")) {
    encoder_stack_->OnReceivedUplinkPacketLossFraction(loss_rate / 100.0);
  }
  return 0;
}

bool AudioCodingModuleImpl::HaveValidEncoder(
    absl::string_view caller_name) const {
  if (!encoder_stack_) {
    RTC_LOG(LS_ERROR) << caller_name << " failed: No send codec is registered.";
    return false;
  }
  return true;
}

ANAStats AudioCodingModuleImpl::GetANAStats() const {
  MutexLock lock(&acm_mutex_);
  if (encoder_stack_)
    return encoder_stack_->GetANAStats();

  return ANAStats();
}

int AudioCodingModuleImpl::GetTargetBitrate() const {
  MutexLock lock(&acm_mutex_);
  if (!encoder_stack_) {
    return -1;
  }
  return encoder_stack_->GetTargetBitrate();
}

}

std::unique_ptr<AudioCodingModule> AudioCodingModule::Create() {
  return std::make_unique<AudioCodingModuleImpl>();
}

}
