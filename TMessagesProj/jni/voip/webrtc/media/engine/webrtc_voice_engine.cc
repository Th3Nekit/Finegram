/*
 *  Copyright (c) 2004 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "media/engine/webrtc_voice_engine.h"

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <functional>
#include <initializer_list>
#include <iterator>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "absl/algorithm/algorithm.h"
#include "absl/algorithm/container.h"
#include "absl/functional/bind_front.h"
#include "absl/strings/match.h"
#include "absl/types/optional.h"
#include "api/audio/audio_frame.h"
#include "api/audio/audio_frame_processor.h"
#include "api/audio_codecs/audio_codec_pair_id.h"
#include "api/audio_codecs/audio_encoder.h"
#include "api/call/audio_sink.h"
#include "api/field_trials_view.h"
#include "api/make_ref_counted.h"
#include "api/media_types.h"
#include "api/priority.h"
#include "api/rtp_headers.h"
#include "api/rtp_parameters.h"
#include "api/rtp_transceiver_direction.h"
#include "api/task_queue/pending_task_safety_flag.h"
#include "api/transport/bitrate_settings.h"
#include "api/units/data_rate.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "call/audio_receive_stream.h"
#include "call/packet_receiver.h"
#include "call/rtp_config.h"
#include "call/rtp_transport_controller_send_interface.h"
#include "media/base/audio_source.h"
#include "media/base/codec.h"
#include "media/base/media_constants.h"
#include "media/base/stream_params.h"
#include "media/engine/adm_helpers.h"
#include "media/engine/payload_type_mapper.h"
#include "media/engine/webrtc_media_engine.h"
#include "modules/async_audio_processing/async_audio_processing.h"
#include "modules/audio_mixer/audio_mixer_impl.h"
#include "modules/audio_processing/include/audio_processing.h"
#include "modules/audio_processing/include/audio_processing_statistics.h"
#include "modules/rtp_rtcp/include/report_block_data.h"
#include "modules/rtp_rtcp/include/rtp_header_extension_map.h"
#include "modules/rtp_rtcp/source/rtp_packet_received.h"
#include "modules/rtp_rtcp/source/rtp_util.h"
#include "rtc_base/checks.h"
#include "rtc_base/dscp.h"
#include "rtc_base/experiments/struct_parameters_parser.h"
#include "rtc_base/logging.h"
#include "rtc_base/race_checker.h"
#include "rtc_base/string_encode.h"
#include "rtc_base/strings/audio_format_to_string.h"
#include "rtc_base/strings/string_builder.h"
#include "rtc_base/strings/string_format.h"
#include "rtc_base/thread_annotations.h"
#include "rtc_base/time_utils.h"
#include "rtc_base/trace_event.h"
#include "system_wrappers/include/metrics.h"

#if WEBRTC_ENABLE_PROTOBUF
#ifdef WEBRTC_ANDROID_PLATFORM_BUILD
#include "external/webrtc/webrtc/modules/audio_coding/audio_network_adaptor/config.pb.h"
#else
#include "modules/audio_coding/audio_network_adaptor/config.pb.h"
#endif

#endif

namespace cricket {
namespace {

using ::webrtc::ParseRtpSsrc;

constexpr size_t kMaxUnsignaledRecvStreams = 4;

constexpr int kNackRtpHistoryMs = 5000;

const int kMinTelephoneEventCode = 0;
const int kMaxTelephoneEventCode = 255;

const int kMinPayloadType = 0;
const int kMaxPayloadType = 127;

class ProxySink : public webrtc::AudioSinkInterface {
 public:
  explicit ProxySink(AudioSinkInterface* sink) : sink_(sink) {
    RTC_DCHECK(sink);
  }

  void OnData(const Data& audio) override { sink_->OnData(audio); }

 private:
  webrtc::AudioSinkInterface* sink_;
};

bool ValidateStreamParams(const StreamParams& sp) {
  if (sp.ssrcs.empty()) {
    RTC_DLOG(LS_ERROR) << "No SSRCs in stream parameters: " << sp.ToString();
    return false;
  }
  if (sp.ssrcs.size() > 1) {
    RTC_DLOG(LS_ERROR) << "Multiple SSRCs in stream parameters: "
                       << sp.ToString();
    return false;
  }
  return true;
}

std::string ToString(const AudioCodec& codec) {
  rtc::StringBuilder ss;
  ss << codec.name << "/" << codec.clockrate << "/" << codec.channels;
  if (!codec.params.empty()) {
    ss << " {";
    for (const auto& param : codec.params) {
      ss << " " << param.first << "=" << param.second;
    }
    ss << " }";
  }
  ss << " (" << codec.id << ")";
  return ss.Release();
}

bool IsCodec(const AudioCodec& codec, const char* ref_name) {
  return absl::EqualsIgnoreCase(codec.name, ref_name);
}

absl::optional<AudioCodec> FindCodec(const std::vector<AudioCodec>& codecs,
                                     const AudioCodec& codec) {
  for (const AudioCodec& c : codecs) {
    if (c.Matches(codec)) {
      return c;
    }
  }
  return absl::nullopt;
}

bool VerifyUniquePayloadTypes(const std::vector<AudioCodec>& codecs) {
  if (codecs.empty()) {
    return true;
  }
  std::vector<int> payload_types;
  absl::c_transform(codecs, std::back_inserter(payload_types),
                    [](const AudioCodec& codec) { return codec.id; });
  absl::c_sort(payload_types);
  return absl::c_adjacent_find(payload_types) == payload_types.end();
}

absl::optional<std::string> GetAudioNetworkAdaptorConfig(
    const AudioOptions& options) {
  if (options.audio_network_adaptor && *options.audio_network_adaptor &&
      options.audio_network_adaptor_config) {

    return options.audio_network_adaptor_config;
  }
  return absl::nullopt;
}

int MinPositive(int a, int b) {
  if (a <= 0) {
    return b;
  }
  if (b <= 0) {
    return a;
  }
  return std::min(a, b);
}

absl::optional<int> ComputeSendBitrate(int max_send_bitrate_bps,
                                       absl::optional<int> rtp_max_bitrate_bps,
                                       const webrtc::AudioCodecSpec& spec) {

  const int bps = rtp_max_bitrate_bps
                      ? MinPositive(max_send_bitrate_bps, *rtp_max_bitrate_bps)
                      : max_send_bitrate_bps;
  if (bps <= 0) {
    return spec.info.default_bitrate_bps;
  }

  if (bps < spec.info.min_bitrate_bps) {

    RTC_LOG(LS_ERROR) << "Failed to set codec " << spec.format.name
                      << " to bitrate " << bps
                      << " bps"
                         ", requires at least "
                      << spec.info.min_bitrate_bps << " bps.";
    return absl::nullopt;
  }

  if (spec.info.HasFixedBitrate()) {
    return spec.info.default_bitrate_bps;
  } else {

    return std::min(bps, spec.info.max_bitrate_bps);
  }
}

bool IsEnabled(const webrtc::FieldTrialsView& config, absl::string_view trial) {
  return absl::StartsWith(config.Lookup(trial), "Enabled");
}

struct AdaptivePtimeConfig {
  bool enabled = false;
  webrtc::DataRate min_payload_bitrate = webrtc::DataRate::KilobitsPerSec(16);

  webrtc::DataRate min_encoder_bitrate = webrtc::DataRate::KilobitsPerSec(16);
  bool use_slow_adaptation = true;

  absl::optional<std::string> audio_network_adaptor_config;

  std::unique_ptr<webrtc::StructParametersParser> Parser() {
    return webrtc::StructParametersParser::Create(
        "enabled", &enabled,
        "min_payload_bitrate", &min_payload_bitrate,
        "min_encoder_bitrate", &min_encoder_bitrate,
        "use_slow_adaptation", &use_slow_adaptation);
  }

  explicit AdaptivePtimeConfig(const webrtc::FieldTrialsView& trials) {
    Parser()->Parse(trials.Lookup("WebRTC-Audio-AdaptivePtime"));
#if WEBRTC_ENABLE_PROTOBUF
    webrtc::audio_network_adaptor::config::ControllerManager config;
    auto* frame_length_controller =
        config.add_controllers()->mutable_frame_length_controller_v2();
    frame_length_controller->set_min_payload_bitrate_bps(
        min_payload_bitrate.bps());
    frame_length_controller->set_use_slow_adaptation(use_slow_adaptation);
    config.add_controllers()->mutable_bitrate_controller();
    audio_network_adaptor_config = config.SerializeAsString();
#endif
  }
};

webrtc::AudioReceiveStreamInterface::Config BuildReceiveStreamConfig(
    uint32_t remote_ssrc,
    uint32_t local_ssrc,
    bool use_nack,
    bool enable_non_sender_rtt,
    const std::vector<std::string>& stream_ids,
    const std::vector<webrtc::RtpExtension>& extensions,
    webrtc::Transport* rtcp_send_transport,
    const rtc::scoped_refptr<webrtc::AudioDecoderFactory>& decoder_factory,
    const std::map<int, webrtc::SdpAudioFormat>& decoder_map,
    absl::optional<webrtc::AudioCodecPairId> codec_pair_id,
    size_t jitter_buffer_max_packets,
    bool jitter_buffer_fast_accelerate,
    int jitter_buffer_min_delay_ms,
    rtc::scoped_refptr<webrtc::FrameDecryptorInterface> frame_decryptor,
    const webrtc::CryptoOptions& crypto_options,
    rtc::scoped_refptr<webrtc::FrameTransformerInterface> frame_transformer) {
  webrtc::AudioReceiveStreamInterface::Config config;
  config.rtp.remote_ssrc = remote_ssrc;
  config.rtp.local_ssrc = local_ssrc;
  config.rtp.nack.rtp_history_ms = use_nack ? kNackRtpHistoryMs : 0;
  if (!stream_ids.empty()) {
    config.sync_group = stream_ids[0];
  }
  config.rtcp_send_transport = rtcp_send_transport;
  config.enable_non_sender_rtt = enable_non_sender_rtt;
  config.decoder_factory = decoder_factory;
  config.decoder_map = decoder_map;
  config.codec_pair_id = codec_pair_id;
  config.jitter_buffer_max_packets = jitter_buffer_max_packets;
  config.jitter_buffer_fast_accelerate = jitter_buffer_fast_accelerate;
  config.jitter_buffer_min_delay_ms = jitter_buffer_min_delay_ms;
  config.frame_decryptor = std::move(frame_decryptor);
  config.crypto_options = crypto_options;
  config.frame_transformer = std::move(frame_transformer);
  return config;
}

bool CheckRedParameters(
    const AudioCodec& red_codec,
    const webrtc::AudioSendStream::Config::SendCodecSpec& send_codec_spec) {
  if (red_codec.clockrate != send_codec_spec.format.clockrate_hz ||
      red_codec.channels != send_codec_spec.format.num_channels) {
    return false;
  }

  auto red_parameters = red_codec.params.find("");
  if (red_parameters == red_codec.params.end()) {
    RTC_LOG(LS_WARNING) << "audio/RED missing fmtp parameters.";
    return false;
  }
  std::vector<absl::string_view> redundant_payloads =
      rtc::split(red_parameters->second, '/');

  if (redundant_payloads.size() < 2 || redundant_payloads.size() > 32) {
    return false;
  }
  for (auto pt : redundant_payloads) {
    if (pt != rtc::ToString(send_codec_spec.payload_type)) {
      return false;
    }
  }
  return true;
}

}

WebRtcVoiceEngine::WebRtcVoiceEngine(
    webrtc::TaskQueueFactory* task_queue_factory,
    webrtc::AudioDeviceModule* adm,
    const rtc::scoped_refptr<webrtc::AudioEncoderFactory>& encoder_factory,
    const rtc::scoped_refptr<webrtc::AudioDecoderFactory>& decoder_factory,
    rtc::scoped_refptr<webrtc::AudioMixer> audio_mixer,
    rtc::scoped_refptr<webrtc::AudioProcessing> audio_processing,
    std::unique_ptr<webrtc::AudioFrameProcessor> audio_frame_processor,
    const webrtc::FieldTrialsView& trials)
    : task_queue_factory_(task_queue_factory),
      adm_(adm),
      encoder_factory_(encoder_factory),
      decoder_factory_(decoder_factory),
      audio_mixer_(audio_mixer),
      apm_(audio_processing),
      audio_frame_processor_(std::move(audio_frame_processor)),
      minimized_remsampling_on_mobile_trial_enabled_(
          IsEnabled(trials, "WebRTC-Audio-MinimizeResamplingOnMobile")) {
  RTC_LOG(LS_INFO) << "WebRtcVoiceEngine::WebRtcVoiceEngine";
  RTC_DCHECK(decoder_factory);
  RTC_DCHECK(encoder_factory);

}

WebRtcVoiceEngine::~WebRtcVoiceEngine() {
  RTC_DCHECK_RUN_ON(&worker_thread_checker_);
  RTC_LOG(LS_INFO) << "WebRtcVoiceEngine::~WebRtcVoiceEngine";
  if (initialized_) {
    StopAecDump();

    adm()->StopPlayout();
    adm()->StopRecording();
    adm()->RegisterAudioCallback(nullptr);
    adm()->Terminate();
  }
}

void WebRtcVoiceEngine::Init() {
  RTC_DCHECK_RUN_ON(&worker_thread_checker_);
  RTC_LOG(LS_INFO) << "WebRtcVoiceEngine::Init";

  RTC_DCHECK(!low_priority_worker_queue_);
  low_priority_worker_queue_ = task_queue_factory_->CreateTaskQueue(
      "rtc-low-prio", webrtc::TaskQueueFactory::Priority::LOW);

  RTC_LOG(LS_VERBOSE) << "Supported send codecs in order of preference:";
  send_codecs_ = CollectCodecs(encoder_factory_->GetSupportedEncoders());
  for (const AudioCodec& codec : send_codecs_) {
    RTC_LOG(LS_VERBOSE) << ToString(codec);
  }

  RTC_LOG(LS_VERBOSE) << "Supported recv codecs in order of preference:";
  recv_codecs_ = CollectCodecs(decoder_factory_->GetSupportedDecoders());
  for (const AudioCodec& codec : recv_codecs_) {
    RTC_LOG(LS_VERBOSE) << ToString(codec);
  }

#if defined(WEBRTC_INCLUDE_INTERNAL_AUDIO_DEVICE)

  if (!adm_) {
    adm_ = webrtc::AudioDeviceModule::Create(
        webrtc::AudioDeviceModule::kPlatformDefaultAudio, task_queue_factory_);
  }
#endif
  RTC_CHECK(adm());
  webrtc::adm_helpers::Init(adm());

  {
    webrtc::AudioState::Config config;
    if (audio_mixer_) {
      config.audio_mixer = audio_mixer_;
    } else {
      config.audio_mixer = webrtc::AudioMixerImpl::Create();
    }
    config.audio_processing = apm_;
    config.audio_device_module = adm_;
    if (audio_frame_processor_) {
      config.async_audio_processing_factory =
          rtc::make_ref_counted<webrtc::AsyncAudioProcessing::Factory>(
              std::move(audio_frame_processor_), *task_queue_factory_);
    }
    audio_state_ = webrtc::AudioState::Create(config);
  }

  adm()->RegisterAudioCallback(audio_state()->audio_transport());

  {
    AudioOptions options;
    options.echo_cancellation = true;
    options.auto_gain_control = true;
#if defined(WEBRTC_IOS)

    options.noise_suppression = false;
#else
    options.noise_suppression = true;
#endif
    options.highpass_filter = true;
    options.stereo_swapping = false;
    options.audio_jitter_buffer_max_packets = 200;
    options.audio_jitter_buffer_fast_accelerate = false;
    options.audio_jitter_buffer_min_delay_ms = 0;
    ApplyOptions(options);
  }
  initialized_ = true;
}

rtc::scoped_refptr<webrtc::AudioState> WebRtcVoiceEngine::GetAudioState()
    const {
  RTC_DCHECK_RUN_ON(&worker_thread_checker_);
  return audio_state_;
}

std::unique_ptr<VoiceMediaSendChannelInterface>
WebRtcVoiceEngine::CreateSendChannel(
    webrtc::Call* call,
    const MediaConfig& config,
    const AudioOptions& options,
    const webrtc::CryptoOptions& crypto_options,
    webrtc::AudioCodecPairId codec_pair_id) {
  return std::make_unique<WebRtcVoiceSendChannel>(
      this, config, options, crypto_options, call, codec_pair_id);
}

std::unique_ptr<VoiceMediaReceiveChannelInterface>
WebRtcVoiceEngine::CreateReceiveChannel(
    webrtc::Call* call,
    const MediaConfig& config,
    const AudioOptions& options,
    const webrtc::CryptoOptions& crypto_options,
    webrtc::AudioCodecPairId codec_pair_id) {
  return std::make_unique<WebRtcVoiceReceiveChannel>(
      this, config, options, crypto_options, call, codec_pair_id);
}

void WebRtcVoiceEngine::ApplyOptions(const AudioOptions& options_in) {
  RTC_DCHECK_RUN_ON(&worker_thread_checker_);
  RTC_LOG(LS_INFO) << "WebRtcVoiceEngine::ApplyOptions: "
                   << options_in.ToString();
  AudioOptions options = options_in;

  bool use_mobile_software_aec = false;

#if defined(WEBRTC_IOS)
  if (options.ios_force_software_aec_HACK &&
      *options.ios_force_software_aec_HACK) {

    options.echo_cancellation = true;
    RTC_LOG(LS_WARNING)
        << "Force software AEC on iOS. May conflict with platform AEC.";
  } else {

    options.echo_cancellation = false;
    RTC_LOG(LS_INFO) << "Always disable AEC on iOS. Use built-in instead.";
  }
#elif defined(WEBRTC_ANDROID)
  use_mobile_software_aec = true;
#endif

#if defined(WEBRTC_IOS)

  options.auto_gain_control = false;
  RTC_LOG(LS_INFO) << "Always disable AGC on iOS. Use built-in instead.";
#endif

#if defined(WEBRTC_IOS) || defined(WEBRTC_ANDROID)

  if (minimized_remsampling_on_mobile_trial_enabled_) {
    options.auto_gain_control = false;
    RTC_LOG(LS_INFO) << "Disable AGC according to field trial.";
    if (!(options.noise_suppression.value_or(false) ||
          options.echo_cancellation.value_or(false))) {

      RTC_LOG(LS_INFO)
          << "Disable high-pass filter in response to field trial.";
      options.highpass_filter = false;
    }
  }
#endif

  if (options.echo_cancellation) {

    const bool built_in_aec = adm()->BuiltInAECIsAvailable();
    if (built_in_aec) {

      const bool enable_built_in_aec = *options.echo_cancellation;
      if (adm()->EnableBuiltInAEC(enable_built_in_aec) == 0 &&
          enable_built_in_aec) {

        options.echo_cancellation = false;
        RTC_LOG(LS_INFO)
            << "Disabling EC since built-in EC will be used instead";
      }
    }
  }

  if (options.auto_gain_control) {
    bool built_in_agc_avaliable = adm()->BuiltInAGCIsAvailable();
    if (built_in_agc_avaliable) {
      if (adm()->EnableBuiltInAGC(*options.auto_gain_control) == 0 &&
          *options.auto_gain_control) {

        options.auto_gain_control = false;
        RTC_LOG(LS_INFO)
            << "Disabling AGC since built-in AGC will be used instead";
      }
    }
  }

  if (options.noise_suppression) {
    if (adm()->BuiltInNSIsAvailable()) {
      bool builtin_ns = *options.noise_suppression;
      if (adm()->EnableBuiltInNS(builtin_ns) == 0 && builtin_ns) {

        options.noise_suppression = false;
        RTC_LOG(LS_INFO)
            << "Disabling NS since built-in NS will be used instead";
      }
    }
  }

  if (options.stereo_swapping) {
    audio_state()->SetStereoChannelSwapping(*options.stereo_swapping);
  }

  if (options.audio_jitter_buffer_max_packets) {
    audio_jitter_buffer_max_packets_ =
        std::max(20, *options.audio_jitter_buffer_max_packets);
  }
  if (options.audio_jitter_buffer_fast_accelerate) {
    audio_jitter_buffer_fast_accelerate_ =
        *options.audio_jitter_buffer_fast_accelerate;
  }
  if (options.audio_jitter_buffer_min_delay_ms) {
    audio_jitter_buffer_min_delay_ms_ =
        *options.audio_jitter_buffer_min_delay_ms;
  }

  webrtc::AudioProcessing* ap = apm();
  if (!ap) {
    return;
  }

  webrtc::AudioProcessing::Config apm_config = ap->GetConfig();

  if (options.echo_cancellation) {
    apm_config.echo_canceller.enabled = *options.echo_cancellation;
    apm_config.echo_canceller.mobile_mode = use_mobile_software_aec;
  }

  if (options.auto_gain_control) {
    const bool enabled = *options.auto_gain_control;
    apm_config.gain_controller1.enabled = enabled;
#if defined(WEBRTC_IOS) || defined(WEBRTC_ANDROID)
    apm_config.gain_controller1.mode =
        apm_config.gain_controller1.kFixedDigital;
#else
    apm_config.gain_controller1.mode =
        apm_config.gain_controller1.kAdaptiveAnalog;
#endif
  }

  if (options.highpass_filter) {
    apm_config.high_pass_filter.enabled = *options.highpass_filter;
  }

  if (options.noise_suppression) {
    const bool enabled = *options.noise_suppression;
    apm_config.noise_suppression.enabled = enabled;
    apm_config.noise_suppression.level =
        webrtc::AudioProcessing::Config::NoiseSuppression::Level::kHigh;
  }

  ap->ApplyConfig(apm_config);
}

const std::vector<AudioCodec>& WebRtcVoiceEngine::send_codecs() const {
  RTC_DCHECK(signal_thread_checker_.IsCurrent());
  return send_codecs_;
}

const std::vector<AudioCodec>& WebRtcVoiceEngine::recv_codecs() const {
  RTC_DCHECK(signal_thread_checker_.IsCurrent());
  return recv_codecs_;
}

std::vector<webrtc::RtpHeaderExtensionCapability>
WebRtcVoiceEngine::GetRtpHeaderExtensions() const {
  RTC_DCHECK(signal_thread_checker_.IsCurrent());
  std::vector<webrtc::RtpHeaderExtensionCapability> result;

  int id = 1;
  for (const auto& uri : {webrtc::RtpExtension::kAudioLevelUri,
                          webrtc::RtpExtension::kAbsSendTimeUri,
                          webrtc::RtpExtension::kTransportSequenceNumberUri,
                          webrtc::RtpExtension::kMidUri}) {
    result.emplace_back(uri, id++, webrtc::RtpTransceiverDirection::kSendRecv);
  }
  for (const auto& uri : {webrtc::RtpExtension::kAbsoluteCaptureTimeUri}) {
    result.emplace_back(uri, id, webrtc::RtpTransceiverDirection::kStopped);
  }
  return result;
}

bool WebRtcVoiceEngine::StartAecDump(webrtc::FileWrapper file,
                                     int64_t max_size_bytes) {
  RTC_DCHECK_RUN_ON(&worker_thread_checker_);

  webrtc::AudioProcessing* ap = apm();
  if (!ap) {
    RTC_LOG(LS_WARNING)
        << "Attempting to start aecdump when no audio processing module is "
           "present, hence no aecdump is started.";
    return false;
  }

  return ap->CreateAndAttachAecDump(file.Release(), max_size_bytes,
                                    low_priority_worker_queue_.get());
}

void WebRtcVoiceEngine::StopAecDump() {
  RTC_DCHECK_RUN_ON(&worker_thread_checker_);
  webrtc::AudioProcessing* ap = apm();
  if (ap) {
    ap->DetachAecDump();
  } else {
    RTC_LOG(LS_WARNING) << "Attempting to stop aecdump when no audio "
                           "processing module is present";
  }
}

absl::optional<webrtc::AudioDeviceModule::Stats>
WebRtcVoiceEngine::GetAudioDeviceStats() {
  return adm()->GetStats();
}

webrtc::AudioDeviceModule* WebRtcVoiceEngine::adm() {
  RTC_DCHECK_RUN_ON(&worker_thread_checker_);
  RTC_DCHECK(adm_);
  return adm_.get();
}

webrtc::AudioProcessing* WebRtcVoiceEngine::apm() const {
  RTC_DCHECK_RUN_ON(&worker_thread_checker_);
  return apm_.get();
}

webrtc::AudioState* WebRtcVoiceEngine::audio_state() {
  RTC_DCHECK_RUN_ON(&worker_thread_checker_);
  RTC_DCHECK(audio_state_);
  return audio_state_.get();
}

std::vector<AudioCodec> WebRtcVoiceEngine::CollectCodecs(
    const std::vector<webrtc::AudioCodecSpec>& specs) const {
  PayloadTypeMapper mapper;
  std::vector<AudioCodec> out;

  std::map<int, bool, std::greater<int>> generate_cn = {
      {8000, false}, {16000, false}, {32000, false}};

  std::map<int, bool, std::greater<int>> generate_dtmf = {
      {8000, false}, {16000, false}, {32000, false}, {48000, false}};

  auto map_format = [&mapper](const webrtc::SdpAudioFormat& format,
                              std::vector<AudioCodec>* out) {
    absl::optional<AudioCodec> opt_codec = mapper.ToAudioCodec(format);
    if (opt_codec) {
      if (out) {
        out->push_back(*opt_codec);
      }
    } else {
      RTC_LOG(LS_ERROR) << "Unable to assign payload type to format: "
                        << rtc::ToString(format);
    }

    return opt_codec;
  };

  for (const auto& spec : specs) {

    absl::optional<AudioCodec> opt_codec = map_format(spec.format, nullptr);
    if (opt_codec) {
      AudioCodec& codec = *opt_codec;
      if (spec.info.supports_network_adaption) {
        codec.AddFeedbackParam(
            FeedbackParam(kRtcpFbParamTransportCc, kParamValueEmpty));
      }

      if (spec.info.allow_comfort_noise) {

        auto cn = generate_cn.find(spec.format.clockrate_hz);
        if (cn != generate_cn.end()) {
          cn->second = true;
        }
      }

      auto dtmf = generate_dtmf.find(spec.format.clockrate_hz);
      if (dtmf != generate_dtmf.end()) {
        dtmf->second = true;
      }

      out.push_back(codec);

      if (codec.name == kOpusCodecName) {
        std::string red_fmtp =
            rtc::ToString(codec.id) + "/" + rtc::ToString(codec.id);
        map_format({kRedCodecName, 48000, 2, {{"", red_fmtp}}}, &out);
      }
    }
  }

  for (const auto& cn : generate_cn) {
    if (cn.second) {
      map_format({kCnCodecName, cn.first, 1}, &out);
    }
  }

  for (const auto& dtmf : generate_dtmf) {
    if (dtmf.second) {
      map_format({kDtmfCodecName, dtmf.first, 1}, &out);
    }
  }

  return out;
}

class WebRtcVoiceSendChannel::WebRtcAudioSendStream : public AudioSource::Sink {
 public:
  WebRtcAudioSendStream(
      uint32_t ssrc,
      const std::string& mid,
      const std::string& c_name,
      const std::string track_id,
      const absl::optional<webrtc::AudioSendStream::Config::SendCodecSpec>&
          send_codec_spec,
      bool extmap_allow_mixed,
      const std::vector<webrtc::RtpExtension>& extensions,
      int max_send_bitrate_bps,
      int rtcp_report_interval_ms,
      const absl::optional<std::string>& audio_network_adaptor_config,
      webrtc::Call* call,
      webrtc::Transport* send_transport,
      const rtc::scoped_refptr<webrtc::AudioEncoderFactory>& encoder_factory,
      const absl::optional<webrtc::AudioCodecPairId> codec_pair_id,
      rtc::scoped_refptr<webrtc::FrameEncryptorInterface> frame_encryptor,
      const webrtc::CryptoOptions& crypto_options)
      : adaptive_ptime_config_(call->trials()),
        call_(call),
        config_(send_transport),
        max_send_bitrate_bps_(max_send_bitrate_bps),
        rtp_parameters_(CreateRtpParametersWithOneEncoding()) {
    RTC_DCHECK(call);
    RTC_DCHECK(encoder_factory);
    config_.rtp.ssrc = ssrc;
    config_.rtp.mid = mid;
    config_.rtp.c_name = c_name;
    config_.rtp.extmap_allow_mixed = extmap_allow_mixed;
    config_.rtp.extensions = extensions;
    config_.has_dscp =
        rtp_parameters_.encodings[0].network_priority != webrtc::Priority::kLow;
    config_.encoder_factory = encoder_factory;
    config_.codec_pair_id = codec_pair_id;
    config_.track_id = track_id;
    config_.frame_encryptor = frame_encryptor;
    config_.crypto_options = crypto_options;
    config_.rtcp_report_interval_ms = rtcp_report_interval_ms;
    rtp_parameters_.encodings[0].ssrc = ssrc;
    rtp_parameters_.rtcp.cname = c_name;
    rtp_parameters_.header_extensions = extensions;

    audio_network_adaptor_config_from_options_ = audio_network_adaptor_config;
    UpdateAudioNetworkAdaptorConfig();

    if (send_codec_spec) {
      UpdateSendCodecSpec(*send_codec_spec);
    }

    stream_ = call_->CreateAudioSendStream(config_);
  }

  WebRtcAudioSendStream() = delete;
  WebRtcAudioSendStream(const WebRtcAudioSendStream&) = delete;
  WebRtcAudioSendStream& operator=(const WebRtcAudioSendStream&) = delete;

  ~WebRtcAudioSendStream() override {
    RTC_DCHECK_RUN_ON(&worker_thread_checker_);
    ClearSource();
    call_->DestroyAudioSendStream(stream_);
  }

  void SetSendCodecSpec(
      const webrtc::AudioSendStream::Config::SendCodecSpec& send_codec_spec) {
    UpdateSendCodecSpec(send_codec_spec);
    ReconfigureAudioSendStream(nullptr);
  }

  void SetRtpExtensions(const std::vector<webrtc::RtpExtension>& extensions) {
    RTC_DCHECK_RUN_ON(&worker_thread_checker_);
    config_.rtp.extensions = extensions;
    rtp_parameters_.header_extensions = extensions;
    ReconfigureAudioSendStream(nullptr);
  }

  void SetExtmapAllowMixed(bool extmap_allow_mixed) {
    config_.rtp.extmap_allow_mixed = extmap_allow_mixed;
    ReconfigureAudioSendStream(nullptr);
  }

  void SetMid(const std::string& mid) {
    RTC_DCHECK_RUN_ON(&worker_thread_checker_);
    if (config_.rtp.mid == mid) {
      return;
    }
    config_.rtp.mid = mid;
    ReconfigureAudioSendStream(nullptr);
  }

  void SetFrameEncryptor(
      rtc::scoped_refptr<webrtc::FrameEncryptorInterface> frame_encryptor) {
    RTC_DCHECK_RUN_ON(&worker_thread_checker_);
    config_.frame_encryptor = frame_encryptor;
    ReconfigureAudioSendStream(nullptr);
  }

  void SetAudioNetworkAdaptorConfig(
      const absl::optional<std::string>& audio_network_adaptor_config) {
    RTC_DCHECK_RUN_ON(&worker_thread_checker_);
    if (audio_network_adaptor_config_from_options_ ==
        audio_network_adaptor_config) {
      return;
    }
    audio_network_adaptor_config_from_options_ = audio_network_adaptor_config;
    UpdateAudioNetworkAdaptorConfig();
    UpdateAllowedBitrateRange();
    ReconfigureAudioSendStream(nullptr);
  }

  bool SetMaxSendBitrate(int bps) {
    RTC_DCHECK_RUN_ON(&worker_thread_checker_);
    RTC_DCHECK(config_.send_codec_spec);
    RTC_DCHECK(audio_codec_spec_);
    auto send_rate = ComputeSendBitrate(
        bps, rtp_parameters_.encodings[0].max_bitrate_bps, *audio_codec_spec_);

    if (!send_rate) {
      return false;
    }

    max_send_bitrate_bps_ = bps;

    if (send_rate != config_.send_codec_spec->target_bitrate_bps) {
      config_.send_codec_spec->target_bitrate_bps = send_rate;
      ReconfigureAudioSendStream(nullptr);
    }
    return true;
  }

  bool SendTelephoneEvent(int payload_type,
                          int payload_freq,
                          int event,
                          int duration_ms) {
    RTC_DCHECK_RUN_ON(&worker_thread_checker_);
    RTC_DCHECK(stream_);
    return stream_->SendTelephoneEvent(payload_type, payload_freq, event,
                                       duration_ms);
  }

  void SetSend(bool send) {
    RTC_DCHECK_RUN_ON(&worker_thread_checker_);
    send_ = send;
    UpdateSendState();
  }

  void SetMuted(bool muted) {
    RTC_DCHECK_RUN_ON(&worker_thread_checker_);
    RTC_DCHECK(stream_);
    stream_->SetMuted(muted);
    muted_ = muted;
  }

  bool muted() const {
    RTC_DCHECK_RUN_ON(&worker_thread_checker_);
    return muted_;
  }

  webrtc::AudioSendStream::Stats GetStats(bool has_remote_tracks) const {
    RTC_DCHECK_RUN_ON(&worker_thread_checker_);
    RTC_DCHECK(stream_);
    return stream_->GetStats(has_remote_tracks);
  }

  void SetSource(AudioSource* source) {
    RTC_DCHECK_RUN_ON(&worker_thread_checker_);
    RTC_DCHECK(source);
    if (source_) {
      RTC_DCHECK(source_ == source);
      return;
    }
    source->SetSink(this);
    source_ = source;
    UpdateSendState();
  }

  void ClearSource() {
    RTC_DCHECK_RUN_ON(&worker_thread_checker_);
    if (source_) {
      source_->SetSink(nullptr);
      source_ = nullptr;
    }
    UpdateSendState();
  }

  void OnData(const void* audio_data,
              int bits_per_sample,
              int sample_rate,
              size_t number_of_channels,
              size_t number_of_frames,
              absl::optional<int64_t> absolute_capture_timestamp_ms) override {
    TRACE_EVENT_BEGIN2("webrtc", "WebRtcAudioSendStream::OnData", "sample_rate",
                       sample_rate, "number_of_frames", number_of_frames);
    RTC_DCHECK_EQ(16, bits_per_sample);
    RTC_CHECK_RUNS_SERIALIZED(&audio_capture_race_checker_);
    RTC_DCHECK(stream_);
    std::unique_ptr<webrtc::AudioFrame> audio_frame(new webrtc::AudioFrame());
    audio_frame->UpdateFrame(
        audio_frame->timestamp_, static_cast<const int16_t*>(audio_data),
        number_of_frames, sample_rate, audio_frame->speech_type_,
        audio_frame->vad_activity_, number_of_channels);

    if (absolute_capture_timestamp_ms) {
      audio_frame->set_absolute_capture_timestamp_ms(
          *absolute_capture_timestamp_ms);
    }
    stream_->SendAudioData(std::move(audio_frame));
    TRACE_EVENT_END1("webrtc", "WebRtcAudioSendStream::OnData",
                     "number_of_channels", number_of_channels);
  }

  void OnClose() override {
    RTC_DCHECK_RUN_ON(&worker_thread_checker_);

    source_ = nullptr;
    UpdateSendState();
  }

  const webrtc::RtpParameters& rtp_parameters() const {
    return rtp_parameters_;
  }

  webrtc::RTCError SetRtpParameters(const webrtc::RtpParameters& parameters,
                                    webrtc::SetParametersCallback callback) {
    webrtc::RTCError error = CheckRtpParametersInvalidModificationAndValues(
        rtp_parameters_, parameters);
    if (!error.ok()) {
      return webrtc::InvokeSetParametersCallback(callback, error);
    }

    absl::optional<int> send_rate;
    if (audio_codec_spec_) {
      send_rate = ComputeSendBitrate(max_send_bitrate_bps_,
                                     parameters.encodings[0].max_bitrate_bps,
                                     *audio_codec_spec_);
      if (!send_rate) {
        return webrtc::InvokeSetParametersCallback(
            callback, webrtc::RTCError(webrtc::RTCErrorType::INTERNAL_ERROR));
      }
    }

    const absl::optional<int> old_rtp_max_bitrate =
        rtp_parameters_.encodings[0].max_bitrate_bps;
    double old_priority = rtp_parameters_.encodings[0].bitrate_priority;
    webrtc::Priority old_dscp = rtp_parameters_.encodings[0].network_priority;
    bool old_adaptive_ptime = rtp_parameters_.encodings[0].adaptive_ptime;
    rtp_parameters_ = parameters;
    config_.bitrate_priority = rtp_parameters_.encodings[0].bitrate_priority;
    config_.has_dscp = (rtp_parameters_.encodings[0].network_priority !=
                        webrtc::Priority::kLow);

    bool reconfigure_send_stream =
        (rtp_parameters_.encodings[0].max_bitrate_bps != old_rtp_max_bitrate) ||
        (rtp_parameters_.encodings[0].bitrate_priority != old_priority) ||
        (rtp_parameters_.encodings[0].network_priority != old_dscp) ||
        (rtp_parameters_.encodings[0].adaptive_ptime != old_adaptive_ptime);
    if (rtp_parameters_.encodings[0].max_bitrate_bps != old_rtp_max_bitrate) {

      if (send_rate) {
        config_.send_codec_spec->target_bitrate_bps = send_rate;
      }
    }
    if (reconfigure_send_stream) {

      UpdateAudioNetworkAdaptorConfig();
      UpdateAllowedBitrateRange();
      ReconfigureAudioSendStream(std::move(callback));
    } else {
      webrtc::InvokeSetParametersCallback(callback, webrtc::RTCError::OK());
    }

    rtp_parameters_.rtcp.cname = config_.rtp.c_name;
    rtp_parameters_.rtcp.reduced_size = false;

    UpdateSendState();
    return webrtc::RTCError::OK();
  }

  void SetEncoderToPacketizerFrameTransformer(
      rtc::scoped_refptr<webrtc::FrameTransformerInterface> frame_transformer) {
    RTC_DCHECK_RUN_ON(&worker_thread_checker_);
    config_.frame_transformer = std::move(frame_transformer);
    ReconfigureAudioSendStream(nullptr);
  }

 private:
  void UpdateSendState() {
    RTC_DCHECK_RUN_ON(&worker_thread_checker_);
    RTC_DCHECK(stream_);
    RTC_DCHECK_EQ(1UL, rtp_parameters_.encodings.size());

    if (send_ && rtp_parameters_.encodings[0].active) {
      stream_->Start();
    } else {
      stream_->Stop();
    }
  }

  void UpdateAllowedBitrateRange() {
    RTC_DCHECK_RUN_ON(&worker_thread_checker_);

    const int kDefaultBitrateBps = 32000;
    config_.min_bitrate_bps = kDefaultBitrateBps;
    config_.max_bitrate_bps = kDefaultBitrateBps;

    if (config_.send_codec_spec &&
        config_.send_codec_spec->target_bitrate_bps) {
      config_.min_bitrate_bps = *config_.send_codec_spec->target_bitrate_bps;
      config_.max_bitrate_bps = *config_.send_codec_spec->target_bitrate_bps;
    }

    if (rtp_parameters_.encodings[0].adaptive_ptime) {
      config_.min_bitrate_bps = std::min(
          config_.min_bitrate_bps,
          static_cast<int>(adaptive_ptime_config_.min_encoder_bitrate.bps()));
    }
  }

  void UpdateSendCodecSpec(
      const webrtc::AudioSendStream::Config::SendCodecSpec& send_codec_spec) {
    RTC_DCHECK_RUN_ON(&worker_thread_checker_);
    config_.send_codec_spec = send_codec_spec;
    auto info =
        config_.encoder_factory->QueryAudioEncoder(send_codec_spec.format);
    RTC_DCHECK(info);

    if (send_codec_spec.target_bitrate_bps) {
      info->default_bitrate_bps = std::max(
          info->min_bitrate_bps,
          std::min(info->max_bitrate_bps, *send_codec_spec.target_bitrate_bps));
    }

    audio_codec_spec_.emplace(
        webrtc::AudioCodecSpec{send_codec_spec.format, *info});

    config_.send_codec_spec->target_bitrate_bps = ComputeSendBitrate(
        max_send_bitrate_bps_, rtp_parameters_.encodings[0].max_bitrate_bps,
        *audio_codec_spec_);

    UpdateAllowedBitrateRange();

    const auto& it = send_codec_spec.format.parameters.find("stereo");
    if (it != send_codec_spec.format.parameters.end() && it->second == "1") {
      num_encoded_channels_ = 2;
    } else {
      num_encoded_channels_ = 1;
    }
  }

  void UpdateAudioNetworkAdaptorConfig() {
    if (adaptive_ptime_config_.enabled ||
        rtp_parameters_.encodings[0].adaptive_ptime) {
      config_.audio_network_adaptor_config =
          adaptive_ptime_config_.audio_network_adaptor_config;
      return;
    }
    config_.audio_network_adaptor_config =
        audio_network_adaptor_config_from_options_;
  }

  void ReconfigureAudioSendStream(webrtc::SetParametersCallback callback) {
    RTC_DCHECK_RUN_ON(&worker_thread_checker_);
    RTC_DCHECK(stream_);
    stream_->Reconfigure(config_, std::move(callback));
  }

  int NumPreferredChannels() const override { return num_encoded_channels_; }

  const AdaptivePtimeConfig adaptive_ptime_config_;
  webrtc::SequenceChecker worker_thread_checker_;
  rtc::RaceChecker audio_capture_race_checker_;
  webrtc::Call* call_ = nullptr;
  webrtc::AudioSendStream::Config config_;

  webrtc::AudioSendStream* stream_ = nullptr;

  AudioSource* source_ = nullptr;
  bool send_ = false;
  bool muted_ = false;
  int max_send_bitrate_bps_;
  webrtc::RtpParameters rtp_parameters_;
  absl::optional<webrtc::AudioCodecSpec> audio_codec_spec_;

  absl::optional<std::string> audio_network_adaptor_config_from_options_;
  std::atomic<int> num_encoded_channels_{-1};
};

WebRtcVoiceSendChannel::WebRtcVoiceSendChannel(
    WebRtcVoiceEngine* engine,
    const MediaConfig& config,
    const AudioOptions& options,
    const webrtc::CryptoOptions& crypto_options,
    webrtc::Call* call,
    webrtc::AudioCodecPairId codec_pair_id)
    : MediaChannelUtil(call->network_thread(), config.enable_dscp),
      worker_thread_(call->worker_thread()),
      engine_(engine),
      call_(call),
      audio_config_(config.audio),
      codec_pair_id_(codec_pair_id),
      crypto_options_(crypto_options) {
  RTC_LOG(LS_VERBOSE) << "WebRtcVoiceSendChannel::WebRtcVoiceSendChannel";
  RTC_DCHECK(call);
  SetOptions(options);
}

WebRtcVoiceSendChannel::~WebRtcVoiceSendChannel() {
  RTC_DCHECK_RUN_ON(worker_thread_);
  RTC_DLOG(LS_VERBOSE) << "WebRtcVoiceSendChannel::~WebRtcVoiceSendChannel";

  while (!send_streams_.empty()) {
    RemoveSendStream(send_streams_.begin()->first);
  }
}

bool WebRtcVoiceSendChannel::SetOptions(const AudioOptions& options) {
  RTC_DCHECK_RUN_ON(worker_thread_);
  RTC_LOG(LS_INFO) << "Setting voice channel options: " << options.ToString();

  options_.SetAll(options);
  engine()->ApplyOptions(options_);

  absl::optional<std::string> audio_network_adaptor_config =
      GetAudioNetworkAdaptorConfig(options_);
  for (auto& it : send_streams_) {
    it.second->SetAudioNetworkAdaptorConfig(audio_network_adaptor_config);
  }

  RTC_LOG(LS_INFO) << "Set voice send channel options. Current options: "
                   << options_.ToString();
  return true;
}

bool WebRtcVoiceSendChannel::SetSenderParameters(
    const AudioSenderParameter& params) {
  TRACE_EVENT0("webrtc", "WebRtcVoiceMediaChannel::SetSenderParameters");
  RTC_DCHECK_RUN_ON(worker_thread_);
  RTC_LOG(LS_INFO) << "WebRtcVoiceMediaChannel::SetSenderParameters: "
                   << params.ToString();

  absl::optional<Codec> force_codec;
  if (send_streams_.size() == 1) {

    auto rtp_parameters = send_streams_.begin()->second->rtp_parameters();
    if (rtp_parameters.encodings[0].codec) {
      auto matched_codec =
          absl::c_find_if(params.codecs, [&](auto negotiated_codec) {
            return negotiated_codec.MatchesRtpCodec(
                *rtp_parameters.encodings[0].codec);
          });
      if (matched_codec != params.codecs.end()) {
        force_codec = *matched_codec;
      } else {

        for (auto& encoding : rtp_parameters.encodings) {
          encoding.codec.reset();
        }
        send_streams_.begin()->second->SetRtpParameters(rtp_parameters,
                                                        nullptr);
      }
    }
  }

  if (!SetSendCodecs(params.codecs, force_codec)) {
    return false;
  }

  if (!ValidateRtpExtensions(params.extensions, send_rtp_extensions_)) {
    return false;
  }

  if (ExtmapAllowMixed() != params.extmap_allow_mixed) {
    SetExtmapAllowMixed(params.extmap_allow_mixed);
    for (auto& it : send_streams_) {
      it.second->SetExtmapAllowMixed(params.extmap_allow_mixed);
    }
  }

  std::vector<webrtc::RtpExtension> filtered_extensions = FilterRtpExtensions(
      params.extensions, webrtc::RtpExtension::IsSupportedForAudio, true,
      call_->trials());
  if (send_rtp_extensions_ != filtered_extensions) {
    send_rtp_extensions_.swap(filtered_extensions);
    for (auto& it : send_streams_) {
      it.second->SetRtpExtensions(send_rtp_extensions_);
    }
  }
  if (!params.mid.empty()) {
    mid_ = params.mid;
    for (auto& it : send_streams_) {
      it.second->SetMid(params.mid);
    }
  }

  if (send_codec_spec_ && !SetMaxSendBitrate(params.max_bandwidth_bps)) {
    return false;
  }
  return SetOptions(params.options);
}

absl::optional<Codec> WebRtcVoiceSendChannel::GetSendCodec() const {
  if (send_codec_spec_) {
    return CreateAudioCodec(send_codec_spec_->format);
  }
  return absl::nullopt;
}

bool WebRtcVoiceSendChannel::SetSendCodecs(
    const std::vector<Codec>& codecs,
    absl::optional<Codec> preferred_codec) {
  RTC_DCHECK_RUN_ON(worker_thread_);
  dtmf_payload_type_ = absl::nullopt;
  dtmf_payload_freq_ = -1;

  for (const Codec& codec : codecs) {

    if (codec.id < kMinPayloadType || codec.id > kMaxPayloadType) {
      RTC_LOG(LS_WARNING) << "Codec payload type out of range: "
                          << ToString(codec);
      return false;
    }
  }

  std::vector<Codec> dtmf_codecs;
  for (const Codec& codec : codecs) {
    if (IsCodec(codec, kDtmfCodecName)) {
      dtmf_codecs.push_back(codec);
      if (!dtmf_payload_type_ || codec.clockrate < dtmf_payload_freq_) {
        dtmf_payload_type_ = codec.id;
        dtmf_payload_freq_ = codec.clockrate;
      }
    }
  }

  absl::optional<webrtc::AudioSendStream::Config::SendCodecSpec>
      send_codec_spec;
  webrtc::BitrateConstraints bitrate_config;
  absl::optional<webrtc::AudioCodecInfo> voice_codec_info;
  size_t send_codec_position = 0;
  for (const Codec& voice_codec : codecs) {
    if (!(IsCodec(voice_codec, kCnCodecName) ||
          IsCodec(voice_codec, kDtmfCodecName) ||
          IsCodec(voice_codec, kRedCodecName)) &&
        (!preferred_codec || preferred_codec->Matches(voice_codec))) {
      webrtc::SdpAudioFormat format(voice_codec.name, voice_codec.clockrate,
                                    voice_codec.channels, voice_codec.params);

      voice_codec_info = engine()->encoder_factory_->QueryAudioEncoder(format);
      if (!voice_codec_info) {
        RTC_LOG(LS_WARNING) << "Unknown codec " << ToString(voice_codec);
        continue;
      }

      send_codec_spec = webrtc::AudioSendStream::Config::SendCodecSpec(
          voice_codec.id, format);
      if (voice_codec.bitrate > 0) {
        send_codec_spec->target_bitrate_bps = voice_codec.bitrate;
      }
      send_codec_spec->transport_cc_enabled = HasTransportCc(voice_codec);
      send_codec_spec->nack_enabled = HasNack(voice_codec);
      send_codec_spec->enable_non_sender_rtt = HasRrtr(voice_codec);
      bitrate_config = GetBitrateConfigForCodec(voice_codec);
      break;
    }
    send_codec_position++;
  }

  if (!send_codec_spec) {

    return true;
  }

  RTC_DCHECK(voice_codec_info);
  if (voice_codec_info->allow_comfort_noise) {

    for (const Codec& cn_codec : codecs) {
      if (IsCodec(cn_codec, kCnCodecName) &&
          cn_codec.clockrate == send_codec_spec->format.clockrate_hz &&
          cn_codec.channels == voice_codec_info->num_channels) {
        if (cn_codec.channels != 1) {
          RTC_LOG(LS_WARNING)
              << "CN #channels " << cn_codec.channels << " not supported.";
        } else if (cn_codec.clockrate != 8000 && cn_codec.clockrate != 16000 &&
                   cn_codec.clockrate != 32000) {
          RTC_LOG(LS_WARNING)
              << "CN frequency " << cn_codec.clockrate << " not supported.";
        } else {
          send_codec_spec->cng_payload_type = cn_codec.id;
        }
        break;
      }
    }

    for (const Codec& dtmf_codec : dtmf_codecs) {
      if (dtmf_codec.clockrate == send_codec_spec->format.clockrate_hz) {
        dtmf_payload_type_ = dtmf_codec.id;
        dtmf_payload_freq_ = dtmf_codec.clockrate;
        break;
      }
    }
  }

  for (size_t i = 0; i < send_codec_position; ++i) {
    const Codec& red_codec = codecs[i];
    if (IsCodec(red_codec, kRedCodecName) &&
        CheckRedParameters(red_codec, *send_codec_spec)) {
      send_codec_spec->red_payload_type = red_codec.id;
      break;
    }
  }

  if (send_codec_spec_ != send_codec_spec) {
    send_codec_spec_ = std::move(send_codec_spec);

    for (const auto& kv : send_streams_) {
      kv.second->SetSendCodecSpec(*send_codec_spec_);
    }
  } else {

    bitrate_config.start_bitrate_bps = -1;
  }
  call_->GetTransportControllerSend()->SetSdpBitrateParameters(bitrate_config);

  send_codecs_ = codecs;

  if (send_codec_changed_callback_) {
    send_codec_changed_callback_();
  }

  return true;
}

void WebRtcVoiceSendChannel::SetSend(bool send) {
  TRACE_EVENT0("webrtc", "WebRtcVoiceMediaChannel::SetSend");
  if (send_ == send) {
    return;
  }

  if (send) {
    engine()->ApplyOptions(options_);

    if (options_.init_recording_on_send.value_or(true) &&

        !engine()->adm()->RecordingIsInitialized() &&
        !engine()->adm()->Recording()) {
      if (engine()->adm()->InitRecording() != 0) {
        RTC_LOG(LS_WARNING) << "Failed to initialize recording";
      }
    }
  }

  for (auto& kv : send_streams_) {
    kv.second->SetSend(send);
  }

  send_ = send;
}

bool WebRtcVoiceSendChannel::SetAudioSend(uint32_t ssrc,
                                          bool enable,
                                          const AudioOptions* options,
                                          AudioSource* source) {
  RTC_DCHECK_RUN_ON(worker_thread_);

  if (!SetLocalSource(ssrc, source)) {
    return false;
  }
  if (!MuteStream(ssrc, !enable)) {
    return false;
  }
  if (enable && options) {
    return SetOptions(*options);
  }
  return true;
}

bool WebRtcVoiceSendChannel::AddSendStream(const StreamParams& sp) {
  TRACE_EVENT0("webrtc", "WebRtcVoiceMediaChannel::AddSendStream");
  RTC_DCHECK_RUN_ON(worker_thread_);
  RTC_LOG(LS_INFO) << "AddSendStream: " << sp.ToString();

  uint32_t ssrc = sp.first_ssrc();
  RTC_DCHECK(0 != ssrc);

  if (send_streams_.find(ssrc) != send_streams_.end()) {
    RTC_LOG(LS_ERROR) << "Stream already exists with ssrc " << ssrc;
    return false;
  }

  absl::optional<std::string> audio_network_adaptor_config =
      GetAudioNetworkAdaptorConfig(options_);
  WebRtcAudioSendStream* stream = new WebRtcAudioSendStream(
      ssrc, mid_, sp.cname, sp.id, send_codec_spec_, ExtmapAllowMixed(),
      send_rtp_extensions_, max_send_bitrate_bps_,
      audio_config_.rtcp_report_interval_ms, audio_network_adaptor_config,
      call_, transport(), engine()->encoder_factory_, codec_pair_id_, nullptr,
      crypto_options_);
  send_streams_.insert(std::make_pair(ssrc, stream));
  if (ssrc_list_changed_callback_) {
    std::set<uint32_t> ssrcs_in_use;
    for (auto it : send_streams_) {
      ssrcs_in_use.insert(it.first);
    }
    ssrc_list_changed_callback_(ssrcs_in_use);
  }

  send_streams_[ssrc]->SetSend(send_);
  return true;
}

bool WebRtcVoiceSendChannel::RemoveSendStream(uint32_t ssrc) {
  TRACE_EVENT0("webrtc", "WebRtcVoiceMediaChannel::RemoveSendStream");
  RTC_DCHECK_RUN_ON(worker_thread_);
  RTC_LOG(LS_INFO) << "RemoveSendStream: " << ssrc;

  auto it = send_streams_.find(ssrc);
  if (it == send_streams_.end()) {
    RTC_LOG(LS_WARNING) << "Try to remove stream with ssrc " << ssrc
                        << " which doesn't exist.";
    return false;
  }

  it->second->SetSend(false);

  delete it->second;
  send_streams_.erase(it);
  if (send_streams_.empty()) {
    SetSend(false);
  }
  return true;
}

void WebRtcVoiceSendChannel::SetSsrcListChangedCallback(
    absl::AnyInvocable<void(const std::set<uint32_t>&)> callback) {
  ssrc_list_changed_callback_ = std::move(callback);
}

bool WebRtcVoiceSendChannel::SetLocalSource(uint32_t ssrc,
                                            AudioSource* source) {
  auto it = send_streams_.find(ssrc);
  if (it == send_streams_.end()) {
    if (source) {

      RTC_LOG(LS_ERROR) << "SetLocalSource failed with ssrc " << ssrc;
      return false;
    }

    return true;
  }

  if (source) {
    it->second->SetSource(source);
  } else {
    it->second->ClearSource();
  }

  return true;
}

bool WebRtcVoiceSendChannel::CanInsertDtmf() {
  return dtmf_payload_type_.has_value() && send_;
}

void WebRtcVoiceSendChannel::SetFrameEncryptor(
    uint32_t ssrc,
    rtc::scoped_refptr<webrtc::FrameEncryptorInterface> frame_encryptor) {
  RTC_DCHECK_RUN_ON(worker_thread_);
  auto matching_stream = send_streams_.find(ssrc);
  if (matching_stream != send_streams_.end()) {
    matching_stream->second->SetFrameEncryptor(frame_encryptor);
  }
}

bool WebRtcVoiceSendChannel::InsertDtmf(uint32_t ssrc,
                                        int event,
                                        int duration) {
  RTC_DCHECK_RUN_ON(worker_thread_);
  RTC_LOG(LS_INFO) << "WebRtcVoiceMediaChannel::InsertDtmf";
  if (!CanInsertDtmf()) {
    return false;
  }

  auto it = ssrc != 0 ? send_streams_.find(ssrc) : send_streams_.begin();
  if (it == send_streams_.end()) {
    RTC_LOG(LS_WARNING) << "The specified ssrc " << ssrc << " is not in use.";
    return false;
  }
  if (event < kMinTelephoneEventCode || event > kMaxTelephoneEventCode) {
    RTC_LOG(LS_WARNING) << "DTMF event code " << event << " out of range.";
    return false;
  }
  RTC_DCHECK_NE(-1, dtmf_payload_freq_);
  return it->second->SendTelephoneEvent(*dtmf_payload_type_, dtmf_payload_freq_,
                                        event, duration);
}

void WebRtcVoiceSendChannel::OnPacketSent(const rtc::SentPacket& sent_packet) {
  RTC_DCHECK_RUN_ON(&network_thread_checker_);

  call_->OnSentPacket(sent_packet);
}

void WebRtcVoiceSendChannel::OnNetworkRouteChanged(
    absl::string_view transport_name,
    const rtc::NetworkRoute& network_route) {
  RTC_DCHECK_RUN_ON(&network_thread_checker_);

  call_->OnAudioTransportOverheadChanged(network_route.packet_overhead);

  worker_thread_->PostTask(SafeTask(
      task_safety_.flag(),
      [this, name = std::string(transport_name), route = network_route] {
        RTC_DCHECK_RUN_ON(worker_thread_);
        call_->GetTransportControllerSend()->OnNetworkRouteChanged(name, route);
      }));
}

bool WebRtcVoiceSendChannel::MuteStream(uint32_t ssrc, bool muted) {
  RTC_DCHECK_RUN_ON(worker_thread_);
  const auto it = send_streams_.find(ssrc);
  if (it == send_streams_.end()) {
    RTC_LOG(LS_WARNING) << "The specified ssrc " << ssrc << " is not in use.";
    return false;
  }
  it->second->SetMuted(muted);

  bool all_muted = muted;
  for (const auto& kv : send_streams_) {
    all_muted = all_muted && kv.second->muted();
  }
  webrtc::AudioProcessing* ap = engine()->apm();
  if (ap) {
    ap->set_output_will_be_muted(all_muted);
  }

  return true;
}

bool WebRtcVoiceSendChannel::SetMaxSendBitrate(int bps) {
  RTC_LOG(LS_INFO) << "WebRtcVoiceMediaChannel::SetMaxSendBitrate.";
  max_send_bitrate_bps_ = bps;
  bool success = true;
  for (const auto& kv : send_streams_) {
    if (!kv.second->SetMaxSendBitrate(max_send_bitrate_bps_)) {
      success = false;
    }
  }
  return success;
}

void WebRtcVoiceSendChannel::OnReadyToSend(bool ready) {
  RTC_DCHECK_RUN_ON(&network_thread_checker_);
  RTC_LOG(LS_VERBOSE) << "OnReadyToSend: " << (ready ? "Ready." : "Not ready.");
  call_->SignalChannelNetworkState(
      webrtc::MediaType::AUDIO,
      ready ? webrtc::kNetworkUp : webrtc::kNetworkDown);
}

bool WebRtcVoiceSendChannel::GetStats(VoiceMediaSendInfo* info) {
  TRACE_EVENT0("webrtc", "WebRtcVoiceMediaChannel::GetSendStats");
  RTC_DCHECK_RUN_ON(worker_thread_);
  RTC_DCHECK(info);

  RTC_DCHECK(info->senders.size() == 0U || send_streams_.size() == 0);
  for (const auto& stream : send_streams_) {
    webrtc::AudioSendStream::Stats stats = stream.second->GetStats(false);
    VoiceSenderInfo sinfo;
    sinfo.add_ssrc(stats.local_ssrc);
    sinfo.payload_bytes_sent = stats.payload_bytes_sent;
    sinfo.header_and_padding_bytes_sent = stats.header_and_padding_bytes_sent;
    sinfo.retransmitted_bytes_sent = stats.retransmitted_bytes_sent;
    sinfo.packets_sent = stats.packets_sent;
    sinfo.total_packet_send_delay = stats.total_packet_send_delay;
    sinfo.retransmitted_packets_sent = stats.retransmitted_packets_sent;
    sinfo.packets_lost = stats.packets_lost;
    sinfo.fraction_lost = stats.fraction_lost;
    sinfo.nacks_received = stats.nacks_received;
    sinfo.target_bitrate = stats.target_bitrate_bps;
    sinfo.codec_name = stats.codec_name;
    sinfo.codec_payload_type = stats.codec_payload_type;
    sinfo.jitter_ms = stats.jitter_ms;
    sinfo.rtt_ms = stats.rtt_ms;
    sinfo.audio_level = stats.audio_level;
    sinfo.total_input_energy = stats.total_input_energy;
    sinfo.total_input_duration = stats.total_input_duration;
    sinfo.ana_statistics = stats.ana_statistics;
    sinfo.apm_statistics = stats.apm_statistics;
    sinfo.report_block_datas = std::move(stats.report_block_datas);

    auto encodings = stream.second->rtp_parameters().encodings;
    if (!encodings.empty()) {
      sinfo.active = encodings[0].active;
    }

    info->senders.push_back(sinfo);
  }

  FillSendCodecStats(info);

  return true;
}

void WebRtcVoiceSendChannel::FillSendCodecStats(
    VoiceMediaSendInfo* voice_media_info) {
  for (const auto& sender : voice_media_info->senders) {
    auto codec = absl::c_find_if(send_codecs_, [&sender](const AudioCodec& c) {
      return sender.codec_payload_type && *sender.codec_payload_type == c.id;
    });
    if (codec != send_codecs_.end()) {
      voice_media_info->send_codecs.insert(
          std::make_pair(codec->id, codec->ToCodecParameters()));
    }
  }
}

void WebRtcVoiceSendChannel::SetEncoderToPacketizerFrameTransformer(
    uint32_t ssrc,
    rtc::scoped_refptr<webrtc::FrameTransformerInterface> frame_transformer) {
  RTC_DCHECK_RUN_ON(worker_thread_);
  auto matching_stream = send_streams_.find(ssrc);
  if (matching_stream == send_streams_.end()) {
    RTC_LOG(LS_INFO) << "Attempting to set frame transformer for SSRC:" << ssrc
                     << " which doesn't exist.";
    return;
  }
  matching_stream->second->SetEncoderToPacketizerFrameTransformer(
      std::move(frame_transformer));
}

webrtc::RtpParameters WebRtcVoiceSendChannel::GetRtpSendParameters(
    uint32_t ssrc) const {
  RTC_DCHECK_RUN_ON(worker_thread_);
  auto it = send_streams_.find(ssrc);
  if (it == send_streams_.end()) {
    RTC_LOG(LS_WARNING) << "Attempting to get RTP send parameters for stream "
                           "with ssrc "
                        << ssrc << " which doesn't exist.";
    return webrtc::RtpParameters();
  }

  webrtc::RtpParameters rtp_params = it->second->rtp_parameters();

  for (const AudioCodec& codec : send_codecs_) {
    rtp_params.codecs.push_back(codec.ToCodecParameters());
  }
  return rtp_params;
}

webrtc::RTCError WebRtcVoiceSendChannel::SetRtpSendParameters(
    uint32_t ssrc,
    const webrtc::RtpParameters& parameters,
    webrtc::SetParametersCallback callback) {
  RTC_DCHECK_RUN_ON(worker_thread_);
  auto it = send_streams_.find(ssrc);
  if (it == send_streams_.end()) {
    RTC_LOG(LS_WARNING) << "Attempting to set RTP send parameters for stream "
                           "with ssrc "
                        << ssrc << " which doesn't exist.";
    return webrtc::InvokeSetParametersCallback(
        callback, webrtc::RTCError(webrtc::RTCErrorType::INTERNAL_ERROR));
  }

  webrtc::RtpParameters current_parameters = GetRtpSendParameters(ssrc);
  if (current_parameters.codecs != parameters.codecs) {
    RTC_DLOG(LS_ERROR) << "Using SetParameters to change the set of codecs "
                          "is not currently supported.";
    return webrtc::InvokeSetParametersCallback(
        callback, webrtc::RTCError(webrtc::RTCErrorType::INTERNAL_ERROR));
  }

  if (!parameters.encodings.empty()) {

    rtc::DiffServCodePoint new_dscp = rtc::DSCP_DEFAULT;
    switch (parameters.encodings[0].network_priority) {
      case webrtc::Priority::kVeryLow:
        new_dscp = rtc::DSCP_CS1;
        break;
      case webrtc::Priority::kLow:
        new_dscp = rtc::DSCP_DEFAULT;
        break;
      case webrtc::Priority::kMedium:
        new_dscp = rtc::DSCP_EF;
        break;
      case webrtc::Priority::kHigh:
        new_dscp = rtc::DSCP_EF;
        break;
    }
    SetPreferredDscp(new_dscp);

    absl::optional<cricket::Codec> send_codec = GetSendCodec();

    if (parameters.encodings[0].codec && send_codec &&
        !send_codec->MatchesRtpCodec(*parameters.encodings[0].codec)) {
      RTC_LOG(LS_VERBOSE) << "Trying to change codec to "
                        << parameters.encodings[0].codec->name;
      auto matched_codec =
          absl::c_find_if(send_codecs_, [&](auto negotiated_codec) {
            return negotiated_codec.MatchesRtpCodec(
                *parameters.encodings[0].codec);
          });

      if (matched_codec == send_codecs_.end()) {
        return webrtc::InvokeSetParametersCallback(
            callback, webrtc::RTCError(
                          webrtc::RTCErrorType::INVALID_MODIFICATION,
                          "Attempted to use an unsupported codec for layer 0"));
      }

      SetSendCodecs(send_codecs_, *matched_codec);
    }
  }

  webrtc::RtpParameters reduced_params = parameters;
  reduced_params.codecs.clear();
  return it->second->SetRtpParameters(reduced_params, std::move(callback));
}

class WebRtcVoiceReceiveChannel::WebRtcAudioReceiveStream {
 public:
  WebRtcAudioReceiveStream(webrtc::AudioReceiveStreamInterface::Config config,
                           webrtc::Call* call)
      : call_(call), stream_(call_->CreateAudioReceiveStream(config)) {
    RTC_DCHECK(call);
    RTC_DCHECK(stream_);
  }

  WebRtcAudioReceiveStream() = delete;
  WebRtcAudioReceiveStream(const WebRtcAudioReceiveStream&) = delete;
  WebRtcAudioReceiveStream& operator=(const WebRtcAudioReceiveStream&) = delete;

  ~WebRtcAudioReceiveStream() {
    RTC_DCHECK_RUN_ON(&worker_thread_checker_);
    call_->DestroyAudioReceiveStream(stream_);
  }

  webrtc::AudioReceiveStreamInterface& stream() {
    RTC_DCHECK(stream_);
    return *stream_;
  }

  void SetFrameDecryptor(
      rtc::scoped_refptr<webrtc::FrameDecryptorInterface> frame_decryptor) {
    RTC_DCHECK_RUN_ON(&worker_thread_checker_);
    stream_->SetFrameDecryptor(std::move(frame_decryptor));
  }

  void SetUseNack(bool use_nack) {
    RTC_DCHECK_RUN_ON(&worker_thread_checker_);
    stream_->SetNackHistory(use_nack ? kNackRtpHistoryMs : 0);
  }

  void SetNonSenderRttMeasurement(bool enabled) {
    RTC_DCHECK_RUN_ON(&worker_thread_checker_);
    stream_->SetNonSenderRttMeasurement(enabled);
  }

  void SetDecoderMap(const std::map<int, webrtc::SdpAudioFormat>& decoder_map) {
    RTC_DCHECK_RUN_ON(&worker_thread_checker_);
    stream_->SetDecoderMap(decoder_map);
  }

  webrtc::AudioReceiveStreamInterface::Stats GetStats(
      bool get_and_clear_legacy_stats) const {
    RTC_DCHECK_RUN_ON(&worker_thread_checker_);
    return stream_->GetStats(get_and_clear_legacy_stats);
  }

  void SetRawAudioSink(std::unique_ptr<webrtc::AudioSinkInterface> sink) {
    RTC_DCHECK_RUN_ON(&worker_thread_checker_);

    stream_->SetSink(sink.get());
    raw_audio_sink_ = std::move(sink);
  }

  void SetOutputVolume(double volume) {
    RTC_DCHECK_RUN_ON(&worker_thread_checker_);
    stream_->SetGain(volume);
  }

  void SetPlayout(bool playout) {
    RTC_DCHECK_RUN_ON(&worker_thread_checker_);
    if (playout) {
      stream_->Start();
    } else {
      stream_->Stop();
    }
  }

  bool SetBaseMinimumPlayoutDelayMs(int delay_ms) {
    RTC_DCHECK_RUN_ON(&worker_thread_checker_);
    if (stream_->SetBaseMinimumPlayoutDelayMs(delay_ms))
      return true;

    RTC_LOG(LS_ERROR) << "Failed to SetBaseMinimumPlayoutDelayMs"
                         " on AudioReceiveStreamInterface on SSRC="
                      << stream_->remote_ssrc()
                      << " with delay_ms=" << delay_ms;
    return false;
  }

  int GetBaseMinimumPlayoutDelayMs() const {
    RTC_DCHECK_RUN_ON(&worker_thread_checker_);
    return stream_->GetBaseMinimumPlayoutDelayMs();
  }

  std::vector<webrtc::RtpSource> GetSources() {
    RTC_DCHECK_RUN_ON(&worker_thread_checker_);
    return stream_->GetSources();
  }

  void SetDepacketizerToDecoderFrameTransformer(
      rtc::scoped_refptr<webrtc::FrameTransformerInterface> frame_transformer) {
    RTC_DCHECK_RUN_ON(&worker_thread_checker_);
    stream_->SetDepacketizerToDecoderFrameTransformer(frame_transformer);
  }

 private:
  webrtc::SequenceChecker worker_thread_checker_;
  webrtc::Call* call_ = nullptr;
  webrtc::AudioReceiveStreamInterface* const stream_ = nullptr;
  std::unique_ptr<webrtc::AudioSinkInterface> raw_audio_sink_
      RTC_GUARDED_BY(worker_thread_checker_);
};

WebRtcVoiceReceiveChannel::WebRtcVoiceReceiveChannel(
    WebRtcVoiceEngine* engine,
    const MediaConfig& config,
    const AudioOptions& options,
    const webrtc::CryptoOptions& crypto_options,
    webrtc::Call* call,
    webrtc::AudioCodecPairId codec_pair_id)
    : MediaChannelUtil(call->network_thread(), config.enable_dscp),
      worker_thread_(call->worker_thread()),
      engine_(engine),
      call_(call),
      audio_config_(config.audio),
      codec_pair_id_(codec_pair_id),
      crypto_options_(crypto_options) {
  RTC_LOG(LS_VERBOSE) << "WebRtcVoiceReceiveChannel::WebRtcVoiceReceiveChannel";
  RTC_DCHECK(call);
  SetOptions(options);
}

WebRtcVoiceReceiveChannel::~WebRtcVoiceReceiveChannel() {
  RTC_DCHECK_RUN_ON(worker_thread_);
  RTC_DLOG(LS_VERBOSE)
      << "WebRtcVoiceReceiveChannel::~WebRtcVoiceReceiveChannel";

  while (!recv_streams_.empty()) {
    RemoveRecvStream(recv_streams_.begin()->first);
  }
}

bool WebRtcVoiceReceiveChannel::SetReceiverParameters(
    const AudioReceiverParameters& params) {
  TRACE_EVENT0("webrtc", "WebRtcVoiceMediaChannel::SetReceiverParameters");
  RTC_DCHECK_RUN_ON(worker_thread_);
  RTC_LOG(LS_INFO) << "WebRtcVoiceMediaChannel::SetReceiverParameters: "
                   << params.ToString();

  if (!SetRecvCodecs(params.codecs)) {
    return false;
  }

  if (!ValidateRtpExtensions(params.extensions, recv_rtp_extensions_)) {
    return false;
  }
  std::vector<webrtc::RtpExtension> filtered_extensions = FilterRtpExtensions(
      params.extensions, webrtc::RtpExtension::IsSupportedForAudio, false,
      call_->trials());
  if (recv_rtp_extensions_ != filtered_extensions) {
    recv_rtp_extensions_.swap(filtered_extensions);
    recv_rtp_extension_map_ =
        webrtc::RtpHeaderExtensionMap(recv_rtp_extensions_);
  }
  return true;
}

webrtc::RtpParameters WebRtcVoiceReceiveChannel::GetRtpReceiverParameters(
    uint32_t ssrc) const {
  RTC_DCHECK_RUN_ON(worker_thread_);
  webrtc::RtpParameters rtp_params;
  auto it = recv_streams_.find(ssrc);
  if (it == recv_streams_.end()) {
    RTC_LOG(LS_WARNING)
        << "Attempting to get RTP receive parameters for stream "
           "with ssrc "
        << ssrc << " which doesn't exist.";
    return webrtc::RtpParameters();
  }
  rtp_params.encodings.emplace_back();
  rtp_params.encodings.back().ssrc = it->second->stream().remote_ssrc();
  rtp_params.header_extensions = recv_rtp_extensions_;

  for (const AudioCodec& codec : recv_codecs_) {
    rtp_params.codecs.push_back(codec.ToCodecParameters());
  }
  return rtp_params;
}

webrtc::RtpParameters
WebRtcVoiceReceiveChannel::GetDefaultRtpReceiveParameters() const {
  RTC_DCHECK_RUN_ON(worker_thread_);
  webrtc::RtpParameters rtp_params;
  if (!default_sink_) {

    return rtp_params;
  }
  rtp_params.encodings.emplace_back();

  for (const AudioCodec& codec : recv_codecs_) {
    rtp_params.codecs.push_back(codec.ToCodecParameters());
  }
  return rtp_params;
}

bool WebRtcVoiceReceiveChannel::SetOptions(const AudioOptions& options) {
  RTC_DCHECK_RUN_ON(worker_thread_);
  RTC_LOG(LS_INFO) << "Setting voice channel options: " << options.ToString();

  options_.SetAll(options);
  engine()->ApplyOptions(options_);

  RTC_LOG(LS_INFO) << "Set voice receive channel options. Current options: "
                   << options_.ToString();
  return true;
}

bool WebRtcVoiceReceiveChannel::SetRecvCodecs(
    const std::vector<AudioCodec>& codecs) {
  RTC_DCHECK_RUN_ON(worker_thread_);

  RTC_LOG(LS_INFO) << "Setting receive voice codecs.";

  if (!VerifyUniquePayloadTypes(codecs)) {
    RTC_LOG(LS_ERROR) << "Codec payload types overlap.";
    return false;
  }

  std::map<int, webrtc::SdpAudioFormat> decoder_map;
  for (const AudioCodec& codec : codecs) {

    absl::optional<AudioCodec> old_codec = FindCodec(recv_codecs_, codec);
    if (old_codec && old_codec->id != codec.id) {
      RTC_LOG(LS_WARNING) << codec.name << " mapped to a second payload type ("
                          << codec.id << ", was already mapped to "
                          << old_codec->id << ")";
    }
    auto format = AudioCodecToSdpAudioFormat(codec);
    if (!IsCodec(codec, kCnCodecName) && !IsCodec(codec, kDtmfCodecName) &&
        !IsCodec(codec, kRedCodecName) &&
        !engine()->decoder_factory_->IsSupportedDecoder(format)) {
      RTC_LOG(LS_ERROR) << "Unsupported codec: " << rtc::ToString(format);
      return false;
    }

    auto existing = decoder_map_.find(codec.id);
    if (existing != decoder_map_.end() && !existing->second.Matches(format)) {
      RTC_LOG(LS_ERROR) << "Attempting to use payload type " << codec.id
                        << " for " << codec.name
                        << ", but it is already used for "
                        << existing->second.name;
      return false;
    }
    decoder_map.insert({codec.id, std::move(format)});
  }

  if (decoder_map == decoder_map_) {

    return true;
  }

  bool playout_enabled = playout_;

  SetPlayout(false);
  RTC_DCHECK(!playout_);

  decoder_map_ = std::move(decoder_map);
  for (auto& kv : recv_streams_) {
    kv.second->SetDecoderMap(decoder_map_);
  }

  recv_codecs_ = codecs;

  SetPlayout(playout_enabled);
  RTC_DCHECK_EQ(playout_, playout_enabled);

  return true;
}

void WebRtcVoiceReceiveChannel::SetReceiveNackEnabled(bool enabled) {

  if (recv_nack_enabled_ != enabled) {
    RTC_LOG(LS_INFO) << "Changing NACK status on receive streams.";
    recv_nack_enabled_ = enabled;
    for (auto& kv : recv_streams_) {
      kv.second->SetUseNack(recv_nack_enabled_);
    }
  }
}

void WebRtcVoiceReceiveChannel::SetReceiveNonSenderRttEnabled(bool enabled) {

  if (enable_non_sender_rtt_ != enabled) {
    RTC_LOG(LS_INFO) << "Changing receive-side RTT status on receive streams.";
    enable_non_sender_rtt_ = enabled;
    for (auto& kv : recv_streams_) {
      kv.second->SetNonSenderRttMeasurement(enable_non_sender_rtt_);
    }
  }
}

void WebRtcVoiceReceiveChannel::SetPlayout(bool playout) {
  TRACE_EVENT0("webrtc", "WebRtcVoiceMediaChannel::SetPlayout");
  RTC_DCHECK_RUN_ON(worker_thread_);
  if (playout_ == playout) {
    return;
  }

  for (const auto& kv : recv_streams_) {
    kv.second->SetPlayout(playout);
  }
  playout_ = playout;
}

bool WebRtcVoiceReceiveChannel::AddRecvStream(const StreamParams& sp) {
  TRACE_EVENT0("webrtc", "WebRtcVoiceMediaChannel::AddRecvStream");
  RTC_DCHECK_RUN_ON(worker_thread_);
  RTC_LOG(LS_INFO) << "AddRecvStream: " << sp.ToString();

  if (!sp.has_ssrcs()) {

    unsignaled_stream_params_ = sp;
    return true;
  }

  if (!ValidateStreamParams(sp)) {
    return false;
  }

  const uint32_t ssrc = sp.first_ssrc();

  if (MaybeDeregisterUnsignaledRecvStream(ssrc)) {
    auto stream_ids = sp.stream_ids();
    std::string sync_group = stream_ids.empty() ? std::string() : stream_ids[0];
    call_->OnUpdateSyncGroup(recv_streams_[ssrc]->stream(),
                             std::move(sync_group));
    return true;
  }

  if (recv_streams_.find(ssrc) != recv_streams_.end()) {
    RTC_LOG(LS_ERROR) << "Stream already exists with ssrc " << ssrc;
    return false;
  }

  auto config = BuildReceiveStreamConfig(
      ssrc, receiver_reports_ssrc_, recv_nack_enabled_, enable_non_sender_rtt_,
      sp.stream_ids(), recv_rtp_extensions_, transport(),
      engine()->decoder_factory_, decoder_map_, codec_pair_id_,
      engine()->audio_jitter_buffer_max_packets_,
      engine()->audio_jitter_buffer_fast_accelerate_,
      engine()->audio_jitter_buffer_min_delay_ms_, unsignaled_frame_decryptor_,
      crypto_options_, unsignaled_frame_transformer_);

  recv_streams_.insert(std::make_pair(
      ssrc, new WebRtcAudioReceiveStream(std::move(config), call_)));
  recv_streams_[ssrc]->SetPlayout(playout_);

  return true;
}

bool WebRtcVoiceReceiveChannel::RemoveRecvStream(uint32_t ssrc) {
  TRACE_EVENT0("webrtc", "WebRtcVoiceMediaChannel::RemoveRecvStream");
  RTC_DCHECK_RUN_ON(worker_thread_);
  RTC_LOG(LS_INFO) << "RemoveRecvStream: " << ssrc;

  const auto it = recv_streams_.find(ssrc);
  if (it == recv_streams_.end()) {
    RTC_LOG(LS_WARNING) << "Try to remove stream with ssrc " << ssrc
                        << " which doesn't exist.";
    return false;
  }

  MaybeDeregisterUnsignaledRecvStream(ssrc);

  it->second->SetRawAudioSink(nullptr);
  delete it->second;
  recv_streams_.erase(it);
  return true;
}

void WebRtcVoiceReceiveChannel::ResetUnsignaledRecvStream() {
  RTC_DCHECK_RUN_ON(worker_thread_);
  RTC_LOG(LS_INFO) << "ResetUnsignaledRecvStream.";
  unsignaled_stream_params_ = StreamParams();

  std::vector<uint32_t> to_remove = unsignaled_recv_ssrcs_;
  for (uint32_t ssrc : to_remove) {
    RemoveRecvStream(ssrc);
  }
}

absl::optional<uint32_t> WebRtcVoiceReceiveChannel::GetUnsignaledSsrc() const {
  if (unsignaled_recv_ssrcs_.empty()) {
    return absl::nullopt;
  }

  return unsignaled_recv_ssrcs_.back();
}

void WebRtcVoiceReceiveChannel::ChooseReceiverReportSsrc(
    const std::set<uint32_t>& choices) {

  if (choices.empty()) {
    return;
  }
  if (choices.find(receiver_reports_ssrc_) != choices.end()) {
    return;
  }
  uint32_t ssrc = *(choices.begin());
  receiver_reports_ssrc_ = ssrc;
  for (auto& kv : recv_streams_) {
    call_->OnLocalSsrcUpdated(kv.second->stream(), ssrc);
  }
}

void WebRtcVoiceReceiveChannel::OnDemuxerCriteriaUpdatePending() {}
void WebRtcVoiceReceiveChannel::OnDemuxerCriteriaUpdateComplete() {}

bool WebRtcVoiceReceiveChannel::SetOutputVolume(uint32_t ssrc, double volume) {
  RTC_DCHECK_RUN_ON(worker_thread_);
  RTC_LOG(LS_INFO) << rtc::StringFormat("WRVMC::%s({ssrc=%u}, {volume=%.2f})",
                                        __func__, ssrc, volume);
  const auto it = recv_streams_.find(ssrc);
  if (it == recv_streams_.end()) {
    RTC_LOG(LS_WARNING) << rtc::StringFormat(
        "WRVMC::%s => (WARNING: no receive stream for SSRC %u)", __func__,
        ssrc);
    return false;
  }
  it->second->SetOutputVolume(volume);
  RTC_LOG(LS_INFO) << rtc::StringFormat(
      "WRVMC::%s => (stream with SSRC %u now uses volume %.2f)", __func__, ssrc,
      volume);
  return true;
}

bool WebRtcVoiceReceiveChannel::SetDefaultOutputVolume(double volume) {
  RTC_DCHECK_RUN_ON(worker_thread_);
  default_recv_volume_ = volume;
  for (uint32_t ssrc : unsignaled_recv_ssrcs_) {
    const auto it = recv_streams_.find(ssrc);
    if (it == recv_streams_.end()) {
      RTC_LOG(LS_WARNING) << "SetDefaultOutputVolume: no recv stream " << ssrc;
      return false;
    }
    it->second->SetOutputVolume(volume);
    RTC_LOG(LS_INFO) << "SetDefaultOutputVolume() to " << volume
                     << " for recv stream with ssrc " << ssrc;
  }
  return true;
}

bool WebRtcVoiceReceiveChannel::SetBaseMinimumPlayoutDelayMs(uint32_t ssrc,
                                                             int delay_ms) {
  RTC_DCHECK_RUN_ON(worker_thread_);
  std::vector<uint32_t> ssrcs(1, ssrc);

  if (ssrc == 0) {
    default_recv_base_minimum_delay_ms_ = delay_ms;
    ssrcs = unsignaled_recv_ssrcs_;
  }
  for (uint32_t ssrc : ssrcs) {
    const auto it = recv_streams_.find(ssrc);
    if (it == recv_streams_.end()) {
      RTC_LOG(LS_WARNING) << "SetBaseMinimumPlayoutDelayMs: no recv stream "
                          << ssrc;
      return false;
    }
    it->second->SetBaseMinimumPlayoutDelayMs(delay_ms);
    RTC_LOG(LS_INFO) << "SetBaseMinimumPlayoutDelayMs() to " << delay_ms
                     << " for recv stream with ssrc " << ssrc;
  }
  return true;
}

absl::optional<int> WebRtcVoiceReceiveChannel::GetBaseMinimumPlayoutDelayMs(
    uint32_t ssrc) const {

  if (ssrc == 0) {
    return default_recv_base_minimum_delay_ms_;
  }

  const auto it = recv_streams_.find(ssrc);

  if (it != recv_streams_.end()) {
    return it->second->GetBaseMinimumPlayoutDelayMs();
  }
  return absl::nullopt;
}

void WebRtcVoiceReceiveChannel::SetFrameDecryptor(
    uint32_t ssrc,
    rtc::scoped_refptr<webrtc::FrameDecryptorInterface> frame_decryptor) {
  RTC_DCHECK_RUN_ON(worker_thread_);
  auto matching_stream = recv_streams_.find(ssrc);
  if (matching_stream != recv_streams_.end()) {
    matching_stream->second->SetFrameDecryptor(frame_decryptor);
  }

  if (ssrc == 0) {
    unsignaled_frame_decryptor_ = frame_decryptor;
  }
}

void WebRtcVoiceReceiveChannel::OnPacketReceived(
    const webrtc::RtpPacketReceived& packet) {
  RTC_DCHECK_RUN_ON(&network_thread_checker_);

  worker_thread_->PostTask(
      SafeTask(task_safety_.flag(), [this, packet = packet]() mutable {
        RTC_DCHECK_RUN_ON(worker_thread_);

        packet.IdentifyExtensions(recv_rtp_extension_map_);
        if (!packet.arrival_time().IsFinite()) {
          packet.set_arrival_time(webrtc::Timestamp::Micros(rtc::TimeMicros()));
        }

        call_->Receiver()->DeliverRtpPacket(
            webrtc::MediaType::AUDIO, std::move(packet),
            absl::bind_front(
                &WebRtcVoiceReceiveChannel::MaybeCreateDefaultReceiveStream,
                this));
      }));
}

bool WebRtcVoiceReceiveChannel::MaybeCreateDefaultReceiveStream(
    const webrtc::RtpPacketReceived& packet) {

  uint32_t ssrc = packet.Ssrc();
  RTC_DCHECK(!absl::c_linear_search(unsignaled_recv_ssrcs_, ssrc));

  StreamParams sp = unsignaled_stream_params_;
  sp.ssrcs.push_back(ssrc);
  RTC_LOG(LS_INFO) << "Creating unsignaled receive stream for SSRC=" << ssrc;
  if (!AddRecvStream(sp)) {
    RTC_LOG(LS_WARNING) << "Could not create unsignaled receive stream.";
    return false;
  }
  unsignaled_recv_ssrcs_.push_back(ssrc);
  RTC_HISTOGRAM_COUNTS_LINEAR("WebRTC.Audio.NumOfUnsignaledStreams",
                              unsignaled_recv_ssrcs_.size(), 1, 100, 101);

  if (unsignaled_recv_ssrcs_.size() > kMaxUnsignaledRecvStreams) {
    uint32_t remove_ssrc = unsignaled_recv_ssrcs_.front();
    RTC_DLOG(LS_INFO) << "Removing unsignaled receive stream with SSRC="
                      << remove_ssrc;
    RemoveRecvStream(remove_ssrc);
  }
  RTC_DCHECK_GE(kMaxUnsignaledRecvStreams, unsignaled_recv_ssrcs_.size());

  SetOutputVolume(ssrc, default_recv_volume_);
  SetBaseMinimumPlayoutDelayMs(ssrc, default_recv_base_minimum_delay_ms_);

  if (default_sink_) {
    for (uint32_t drop_ssrc : unsignaled_recv_ssrcs_) {
      auto it = recv_streams_.find(drop_ssrc);
      it->second->SetRawAudioSink(nullptr);
    }
    std::unique_ptr<webrtc::AudioSinkInterface> proxy_sink(
        new ProxySink(default_sink_.get()));
    SetRawAudioSink(ssrc, std::move(proxy_sink));
  }
  return true;
}

bool WebRtcVoiceReceiveChannel::GetStats(VoiceMediaReceiveInfo* info,
                                         bool get_and_clear_legacy_stats) {
  TRACE_EVENT0("webrtc", "WebRtcVoiceMediaChannel::GetReceiveStats");
  RTC_DCHECK_RUN_ON(worker_thread_);
  RTC_DCHECK(info);

  RTC_DCHECK_EQ(info->receivers.size(), 0U);
  for (const auto& stream : recv_streams_) {
    uint32_t ssrc = stream.first;

    if (!unsignaled_recv_ssrcs_.empty()) {
      auto end_it = --unsignaled_recv_ssrcs_.end();
      if (absl::linear_search(unsignaled_recv_ssrcs_.begin(), end_it, ssrc)) {
        continue;
      }
    }
    webrtc::AudioReceiveStreamInterface::Stats stats =
        stream.second->GetStats(get_and_clear_legacy_stats);
    VoiceReceiverInfo rinfo;
    rinfo.add_ssrc(stats.remote_ssrc);
    rinfo.payload_bytes_received = stats.payload_bytes_received;
    rinfo.header_and_padding_bytes_received =
        stats.header_and_padding_bytes_received;
    rinfo.packets_received = stats.packets_received;
    rinfo.fec_packets_received = stats.fec_packets_received;
    rinfo.fec_packets_discarded = stats.fec_packets_discarded;
    rinfo.packets_lost = stats.packets_lost;
    rinfo.packets_discarded = stats.packets_discarded;
    rinfo.codec_name = stats.codec_name;
    rinfo.codec_payload_type = stats.codec_payload_type;
    rinfo.jitter_ms = stats.jitter_ms;
    rinfo.jitter_buffer_ms = stats.jitter_buffer_ms;
    rinfo.jitter_buffer_preferred_ms = stats.jitter_buffer_preferred_ms;
    rinfo.delay_estimate_ms = stats.delay_estimate_ms;
    rinfo.audio_level = stats.audio_level;
    rinfo.total_output_energy = stats.total_output_energy;
    rinfo.total_samples_received = stats.total_samples_received;
    rinfo.total_output_duration = stats.total_output_duration;
    rinfo.concealed_samples = stats.concealed_samples;
    rinfo.silent_concealed_samples = stats.silent_concealed_samples;
    rinfo.concealment_events = stats.concealment_events;
    rinfo.jitter_buffer_delay_seconds = stats.jitter_buffer_delay_seconds;
    rinfo.jitter_buffer_emitted_count = stats.jitter_buffer_emitted_count;
    rinfo.jitter_buffer_target_delay_seconds =
        stats.jitter_buffer_target_delay_seconds;
    rinfo.jitter_buffer_minimum_delay_seconds =
        stats.jitter_buffer_minimum_delay_seconds;
    rinfo.inserted_samples_for_deceleration =
        stats.inserted_samples_for_deceleration;
    rinfo.removed_samples_for_acceleration =
        stats.removed_samples_for_acceleration;
    rinfo.expand_rate = stats.expand_rate;
    rinfo.speech_expand_rate = stats.speech_expand_rate;
    rinfo.secondary_decoded_rate = stats.secondary_decoded_rate;
    rinfo.secondary_discarded_rate = stats.secondary_discarded_rate;
    rinfo.accelerate_rate = stats.accelerate_rate;
    rinfo.preemptive_expand_rate = stats.preemptive_expand_rate;
    rinfo.delayed_packet_outage_samples = stats.delayed_packet_outage_samples;
    rinfo.decoding_calls_to_silence_generator =
        stats.decoding_calls_to_silence_generator;
    rinfo.decoding_calls_to_neteq = stats.decoding_calls_to_neteq;
    rinfo.decoding_normal = stats.decoding_normal;
    rinfo.decoding_plc = stats.decoding_plc;
    rinfo.decoding_codec_plc = stats.decoding_codec_plc;
    rinfo.decoding_cng = stats.decoding_cng;
    rinfo.decoding_plc_cng = stats.decoding_plc_cng;
    rinfo.decoding_muted_output = stats.decoding_muted_output;
    rinfo.capture_start_ntp_time_ms = stats.capture_start_ntp_time_ms;
    rinfo.last_packet_received = stats.last_packet_received;
    rinfo.estimated_playout_ntp_timestamp_ms =
        stats.estimated_playout_ntp_timestamp_ms;
    rinfo.jitter_buffer_flushes = stats.jitter_buffer_flushes;
    rinfo.relative_packet_arrival_delay_seconds =
        stats.relative_packet_arrival_delay_seconds;
    rinfo.interruption_count = stats.interruption_count;
    rinfo.total_interruption_duration_ms = stats.total_interruption_duration_ms;
    rinfo.last_sender_report_timestamp_ms =
        stats.last_sender_report_timestamp_ms;
    rinfo.last_sender_report_remote_timestamp_ms =
        stats.last_sender_report_remote_timestamp_ms;
    rinfo.sender_reports_packets_sent = stats.sender_reports_packets_sent;
    rinfo.sender_reports_bytes_sent = stats.sender_reports_bytes_sent;
    rinfo.sender_reports_reports_count = stats.sender_reports_reports_count;
    rinfo.round_trip_time = stats.round_trip_time;
    rinfo.round_trip_time_measurements = stats.round_trip_time_measurements;
    rinfo.total_round_trip_time = stats.total_round_trip_time;

    if (recv_nack_enabled_) {
      rinfo.nacks_sent = stats.nacks_sent;
    }

    info->receivers.push_back(rinfo);
  }

  FillReceiveCodecStats(info);

  info->device_underrun_count = engine_->adm()->GetPlayoutUnderrunCount();

  return true;
}

void WebRtcVoiceReceiveChannel::FillReceiveCodecStats(
    VoiceMediaReceiveInfo* voice_media_info) {
  for (const auto& receiver : voice_media_info->receivers) {
    auto codec =
        absl::c_find_if(recv_codecs_, [&receiver](const AudioCodec& c) {
          return receiver.codec_payload_type &&
                 *receiver.codec_payload_type == c.id;
        });
    if (codec != recv_codecs_.end()) {
      voice_media_info->receive_codecs.insert(
          std::make_pair(codec->id, codec->ToCodecParameters()));
    }
  }
}

void WebRtcVoiceReceiveChannel::SetRawAudioSink(
    uint32_t ssrc,
    std::unique_ptr<webrtc::AudioSinkInterface> sink) {
  RTC_DCHECK_RUN_ON(worker_thread_);
  RTC_LOG(LS_VERBOSE) << "WebRtcVoiceMediaChannel::SetRawAudioSink: ssrc:"
                      << ssrc << " " << (sink ? "(ptr)" : "NULL");
  const auto it = recv_streams_.find(ssrc);
  if (it == recv_streams_.end()) {
    RTC_LOG(LS_WARNING) << "SetRawAudioSink: no recv stream " << ssrc;
    return;
  }
  it->second->SetRawAudioSink(std::move(sink));
}

void WebRtcVoiceReceiveChannel::SetDefaultRawAudioSink(
    std::unique_ptr<webrtc::AudioSinkInterface> sink) {
  RTC_DCHECK_RUN_ON(worker_thread_);
  RTC_LOG(LS_VERBOSE) << "WebRtcVoiceMediaChannel::SetDefaultRawAudioSink:";
  if (!unsignaled_recv_ssrcs_.empty()) {
    std::unique_ptr<webrtc::AudioSinkInterface> proxy_sink(
        sink ? new ProxySink(sink.get()) : nullptr);
    SetRawAudioSink(unsignaled_recv_ssrcs_.back(), std::move(proxy_sink));
  }
  default_sink_ = std::move(sink);
}

std::vector<webrtc::RtpSource> WebRtcVoiceReceiveChannel::GetSources(
    uint32_t ssrc) const {
  auto it = recv_streams_.find(ssrc);
  if (it == recv_streams_.end()) {
    RTC_LOG(LS_ERROR) << "Attempting to get contributing sources for SSRC:"
                      << ssrc << " which doesn't exist.";
    return std::vector<webrtc::RtpSource>();
  }
  return it->second->GetSources();
}

void WebRtcVoiceReceiveChannel::SetDepacketizerToDecoderFrameTransformer(
    uint32_t ssrc,
    rtc::scoped_refptr<webrtc::FrameTransformerInterface> frame_transformer) {
  RTC_DCHECK_RUN_ON(worker_thread_);
  if (ssrc == 0) {

    unsignaled_frame_transformer_ = std::move(frame_transformer);
    return;
  }

  auto matching_stream = recv_streams_.find(ssrc);
  if (matching_stream == recv_streams_.end()) {
    RTC_LOG(LS_INFO) << "Attempting to set frame transformer for SSRC:" << ssrc
                     << " which doesn't exist.";
    return;
  }
  matching_stream->second->SetDepacketizerToDecoderFrameTransformer(
      std::move(frame_transformer));
}

bool WebRtcVoiceReceiveChannel::MaybeDeregisterUnsignaledRecvStream(
    uint32_t ssrc) {
  RTC_DCHECK_RUN_ON(worker_thread_);
  auto it = absl::c_find(unsignaled_recv_ssrcs_, ssrc);
  if (it != unsignaled_recv_ssrcs_.end()) {
    unsignaled_recv_ssrcs_.erase(it);
    return true;
  }
  return false;
}
}
