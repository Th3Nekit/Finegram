/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MEDIA_ENGINE_WEBRTC_VIDEO_ENGINE_H_
#define MEDIA_ENGINE_WEBRTC_VIDEO_ENGINE_H_

#include <stddef.h>

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "absl/functional/any_invocable.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "api/array_view.h"
#include "api/call/transport.h"
#include "api/crypto/crypto_options.h"
#include "api/crypto/frame_decryptor_interface.h"
#include "api/crypto/frame_encryptor_interface.h"
#include "api/field_trials_view.h"
#include "api/frame_transformer_interface.h"
#include "api/rtc_error.h"
#include "api/rtp_headers.h"
#include "api/rtp_parameters.h"
#include "api/rtp_sender_interface.h"
#include "api/scoped_refptr.h"
#include "api/sequence_checker.h"
#include "api/task_queue/pending_task_safety_flag.h"
#include "api/task_queue/task_queue_base.h"
#include "api/transport/bitrate_settings.h"
#include "api/transport/field_trial_based_config.h"
#include "api/transport/rtp/rtp_source.h"
#include "api/video/recordable_encoded_frame.h"
#include "api/video/video_bitrate_allocator_factory.h"
#include "api/video/video_frame.h"
#include "api/video/video_sink_interface.h"
#include "api/video/video_source_interface.h"
#include "api/video/video_stream_encoder_settings.h"
#include "api/video_codecs/sdp_video_format.h"
#include "api/video_codecs/video_encoder_factory.h"
#include "call/call.h"
#include "call/flexfec_receive_stream.h"
#include "call/rtp_config.h"
#include "call/video_receive_stream.h"
#include "call/video_send_stream.h"
#include "media/base/codec.h"
#include "media/base/media_channel.h"
#include "media/base/media_channel_impl.h"
#include "media/base/media_config.h"
#include "media/base/media_engine.h"
#include "media/base/stream_params.h"
#include "modules/rtp_rtcp/include/rtp_header_extension_map.h"
#include "modules/rtp_rtcp/source/rtp_packet_received.h"
#include "rtc_base/network/sent_packet.h"
#include "rtc_base/network_route.h"
#include "rtc_base/synchronization/mutex.h"
#include "rtc_base/system/no_unique_address.h"
#include "rtc_base/thread_annotations.h"
#include "video/config/video_encoder_config.h"

namespace webrtc {
class VideoDecoderFactory;
class VideoEncoderFactory;
}

namespace cricket {

std::map<uint32_t, webrtc::VideoSendStream::StreamStats>
MergeInfoAboutOutboundRtpSubstreamsForTesting(
    const std::map<uint32_t, webrtc::VideoSendStream::StreamStats>& substreams);

class WebRtcVideoEngine : public VideoEngineInterface {
 public:

  WebRtcVideoEngine(
      std::unique_ptr<webrtc::VideoEncoderFactory> video_encoder_factory,
      std::unique_ptr<webrtc::VideoDecoderFactory> video_decoder_factory,
      const webrtc::FieldTrialsView& trials);

  ~WebRtcVideoEngine() override;

  std::unique_ptr<VideoMediaSendChannelInterface> CreateSendChannel(
      webrtc::Call* call,
      const MediaConfig& config,
      const VideoOptions& options,
      const webrtc::CryptoOptions& crypto_options,
      webrtc::VideoBitrateAllocatorFactory* video_bitrate_allocator_factory)
      override;
  std::unique_ptr<VideoMediaReceiveChannelInterface> CreateReceiveChannel(
      webrtc::Call* call,
      const MediaConfig& config,
      const VideoOptions& options,
      const webrtc::CryptoOptions& crypto_options) override;

  std::vector<VideoCodec> send_codecs() const override {
    return send_codecs(true);
  }
  std::vector<VideoCodec> recv_codecs() const override {
    return recv_codecs(true);
  }
  std::vector<VideoCodec> send_codecs(bool include_rtx) const override;
  std::vector<VideoCodec> recv_codecs(bool include_rtx) const override;
  std::vector<webrtc::RtpHeaderExtensionCapability> GetRtpHeaderExtensions()
      const override;

 private:
  const std::unique_ptr<webrtc::VideoDecoderFactory> decoder_factory_;
  const std::unique_ptr<webrtc::VideoEncoderFactory> encoder_factory_;
  const std::unique_ptr<webrtc::VideoBitrateAllocatorFactory>
      bitrate_allocator_factory_;
  const webrtc::FieldTrialsView& trials_;
};

struct VideoCodecSettings {
  explicit VideoCodecSettings(const VideoCodec& codec);

  bool operator==(const VideoCodecSettings& other) const;
  bool operator!=(const VideoCodecSettings& other) const;

  static bool EqualsDisregardingFlexfec(const VideoCodecSettings& a,
                                        const VideoCodecSettings& b);

  VideoCodec codec;
  webrtc::UlpfecConfig ulpfec;
  int flexfec_payload_type;
  int rtx_payload_type;
  absl::optional<int> rtx_time;
};

class WebRtcVideoSendChannel : public MediaChannelUtil,
                               public VideoMediaSendChannelInterface,
                               public webrtc::EncoderSwitchRequestCallback {
 public:
  WebRtcVideoSendChannel(
      webrtc::Call* call,
      const MediaConfig& config,
      const VideoOptions& options,
      const webrtc::CryptoOptions& crypto_options,
      webrtc::VideoEncoderFactory* encoder_factory,
      webrtc::VideoDecoderFactory* decoder_factory,
      webrtc::VideoBitrateAllocatorFactory* bitrate_allocator_factory);
  ~WebRtcVideoSendChannel() override;

  MediaType media_type() const override { return MEDIA_TYPE_VIDEO; }

  VideoMediaSendChannelInterface* AsVideoSendChannel() override { return this; }
  VoiceMediaSendChannelInterface* AsVoiceSendChannel() override {
    RTC_CHECK_NOTREACHED();
    return nullptr;
  }

  bool HasNetworkInterface() const override {
    return MediaChannelUtil::HasNetworkInterface();
  }
  void SetExtmapAllowMixed(bool extmap_allow_mixed) override {
    MediaChannelUtil::SetExtmapAllowMixed(extmap_allow_mixed);
  }
  bool ExtmapAllowMixed() const override {
    return MediaChannelUtil::ExtmapAllowMixed();
  }

  void SetInterface(MediaChannelNetworkInterface* iface) override;

  bool SetSenderParameters(const VideoSenderParameters& params) override;
  webrtc::RTCError SetRtpSendParameters(
      uint32_t ssrc,
      const webrtc::RtpParameters& parameters,
      webrtc::SetParametersCallback callback) override;
  webrtc::RtpParameters GetRtpSendParameters(uint32_t ssrc) const override;
  absl::optional<Codec> GetSendCodec() const override;
  bool SetSend(bool send) override;
  bool SetVideoSend(
      uint32_t ssrc,
      const VideoOptions* options,
      rtc::VideoSourceInterface<webrtc::VideoFrame>* source) override;
  bool AddSendStream(const StreamParams& sp) override;
  bool RemoveSendStream(uint32_t ssrc) override;
  void FillBitrateInfo(BandwidthEstimationInfo* bwe_info) override;
  bool GetStats(VideoMediaSendInfo* info) override;

  void OnPacketSent(const rtc::SentPacket& sent_packet) override;
  void OnReadyToSend(bool ready) override;
  void OnNetworkRouteChanged(absl::string_view transport_name,
                             const rtc::NetworkRoute& network_route) override;

  void SetFrameEncryptor(uint32_t ssrc,
                         rtc::scoped_refptr<webrtc::FrameEncryptorInterface>
                             frame_encryptor) override;

  void SetEncoderSelector(uint32_t ssrc,
                          webrtc::VideoEncoderFactory::EncoderSelectorInterface*
                              encoder_selector) override;

  void SetSendCodecChangedCallback(
      absl::AnyInvocable<void()> callback) override {
    send_codec_changed_callback_ = std::move(callback);
  }

  void SetSsrcListChangedCallback(
      absl::AnyInvocable<void(const std::set<uint32_t>&)> callback) override {
    ssrc_list_changed_callback_ = std::move(callback);
  }

  bool sending() const {
    RTC_DCHECK_RUN_ON(&thread_checker_);
    return sending_;
  }

  enum AdaptReason {
    ADAPTREASON_NONE = 0,
    ADAPTREASON_CPU = 1,
    ADAPTREASON_BANDWIDTH = 2,
  };

  void RequestEncoderFallback() override;
  void RequestEncoderSwitch(const webrtc::SdpVideoFormat& format,
                            bool allow_default_fallback) override;

  void GenerateSendKeyFrame(uint32_t ssrc,
                            const std::vector<std::string>& rids) override;

  void SetEncoderToPacketizerFrameTransformer(
      uint32_t ssrc,
      rtc::scoped_refptr<webrtc::FrameTransformerInterface> frame_transformer)
      override;

  webrtc::RtcpMode SendCodecRtcpMode() const override {
    RTC_DCHECK_RUN_ON(&thread_checker_);
    return send_params_.rtcp.reduced_size ? webrtc::RtcpMode::kReducedSize
                                          : webrtc::RtcpMode::kCompound;
  }

  bool SendCodecHasLntf() const override {
    RTC_DCHECK_RUN_ON(&thread_checker_);
    if (!send_codec()) {
      return false;
    }
    return HasLntf(send_codec()->codec);
  }
  bool SendCodecHasNack() const override {
    RTC_DCHECK_RUN_ON(&thread_checker_);
    if (!send_codec()) {
      return false;
    }
    return HasNack(send_codec()->codec);
  }
  absl::optional<int> SendCodecRtxTime() const override {
    RTC_DCHECK_RUN_ON(&thread_checker_);
    if (!send_codec()) {
      return absl::nullopt;
    }
    return send_codec()->rtx_time;
  }

 private:
  struct ChangedSenderParameters {

    absl::optional<VideoCodecSettings> send_codec;
    absl::optional<std::vector<VideoCodecSettings>> negotiated_codecs;
    absl::optional<std::vector<webrtc::RtpExtension>> rtp_header_extensions;
    absl::optional<std::string> mid;
    absl::optional<bool> extmap_allow_mixed;
    absl::optional<int> max_bandwidth_bps;
    absl::optional<bool> conference_mode;
    absl::optional<webrtc::RtcpMode> rtcp_mode;
  };

  bool GetChangedSenderParameters(const VideoSenderParameters& params,
                                  ChangedSenderParameters* changed_params) const
      RTC_EXCLUSIVE_LOCKS_REQUIRED(thread_checker_);
  bool ApplyChangedParams(const ChangedSenderParameters& changed_params);
  bool ValidateSendSsrcAvailability(const StreamParams& sp) const
      RTC_EXCLUSIVE_LOCKS_REQUIRED(thread_checker_);

  static void ExtractCodecInformation(
      rtc::ArrayView<const VideoCodecSettings> recv_codecs,
      std::map<int, int>& rtx_associated_payload_types,
      std::set<int>& raw_payload_types,
      std::vector<webrtc::VideoReceiveStreamInterface::Decoder>& decoders);

  class WebRtcVideoSendStream {
   public:
    WebRtcVideoSendStream(
        webrtc::Call* call,
        const StreamParams& sp,
        webrtc::VideoSendStream::Config config,
        const VideoOptions& options,
        bool enable_cpu_overuse_detection,
        int max_bitrate_bps,
        const absl::optional<VideoCodecSettings>& codec_settings,
        const absl::optional<std::vector<webrtc::RtpExtension>>& rtp_extensions,
        const VideoSenderParameters& send_params);
    ~WebRtcVideoSendStream();

    void SetSenderParameters(const ChangedSenderParameters& send_params);
    webrtc::RTCError SetRtpParameters(const webrtc::RtpParameters& parameters,
                                      webrtc::SetParametersCallback callback);
    webrtc::RtpParameters GetRtpParameters() const;

    void SetFrameEncryptor(
        rtc::scoped_refptr<webrtc::FrameEncryptorInterface> frame_encryptor);

    bool SetVideoSend(const VideoOptions* options,
                      rtc::VideoSourceInterface<webrtc::VideoFrame>* source);

    void SetEncoderSelector(
        webrtc::VideoEncoderFactory::EncoderSelectorInterface*
            encoder_selector);

    void SetSend(bool send);

    const std::vector<uint32_t>& GetSsrcs() const;

    std::vector<VideoSenderInfo> GetPerLayerVideoSenderInfos(bool log_stats);

    VideoSenderInfo GetAggregatedVideoSenderInfo(
        const std::vector<VideoSenderInfo>& infos) const;
    void FillBitrateInfo(BandwidthEstimationInfo* bwe_info);

    void SetEncoderToPacketizerFrameTransformer(
        rtc::scoped_refptr<webrtc::FrameTransformerInterface>
            frame_transformer);
    void GenerateKeyFrame(const std::vector<std::string>& rids);

   private:

    struct VideoSendStreamParameters {
      VideoSendStreamParameters(
          webrtc::VideoSendStream::Config config,
          const VideoOptions& options,
          int max_bitrate_bps,
          const absl::optional<VideoCodecSettings>& codec_settings);
      webrtc::VideoSendStream::Config config;
      VideoOptions options;
      int max_bitrate_bps;
      bool conference_mode;
      absl::optional<VideoCodecSettings> codec_settings;

      webrtc::VideoEncoderConfig encoder_config;
    };

    rtc::scoped_refptr<webrtc::VideoEncoderConfig::EncoderSpecificSettings>
    ConfigureVideoEncoderSettings(const VideoCodec& codec);
    void SetCodec(const VideoCodecSettings& codec);
    void RecreateWebRtcStream();
    webrtc::VideoEncoderConfig CreateVideoEncoderConfig(
        const VideoCodec& codec) const;
    void ReconfigureEncoder(webrtc::SetParametersCallback callback);

    void UpdateSendState();

    webrtc::DegradationPreference GetDegradationPreference() const
        RTC_EXCLUSIVE_LOCKS_REQUIRED(&thread_checker_);

    RTC_NO_UNIQUE_ADDRESS webrtc::SequenceChecker thread_checker_;
    webrtc::TaskQueueBase* const worker_thread_;
    const std::vector<uint32_t> ssrcs_ RTC_GUARDED_BY(&thread_checker_);
    const std::vector<SsrcGroup> ssrc_groups_ RTC_GUARDED_BY(&thread_checker_);
    webrtc::Call* const call_;
    const bool enable_cpu_overuse_detection_;
    rtc::VideoSourceInterface<webrtc::VideoFrame>* source_
        RTC_GUARDED_BY(&thread_checker_);

    webrtc::VideoSendStream* stream_ RTC_GUARDED_BY(&thread_checker_);

    VideoSendStreamParameters parameters_ RTC_GUARDED_BY(&thread_checker_);

    webrtc::RtpParameters rtp_parameters_ RTC_GUARDED_BY(&thread_checker_);

    bool sending_ RTC_GUARDED_BY(&thread_checker_);

    const bool disable_automatic_resize_;
  };

  void Construct(webrtc::Call* call, WebRtcVideoEngine* engine);

  std::vector<VideoCodecSettings> SelectSendVideoCodecs(
      const std::vector<VideoCodecSettings>& remote_mapped_codecs) const
      RTC_EXCLUSIVE_LOCKS_REQUIRED(thread_checker_);

  void FillSenderStats(VideoMediaSendInfo* info, bool log_stats)
      RTC_EXCLUSIVE_LOCKS_REQUIRED(thread_checker_);
  void FillBandwidthEstimationStats(const webrtc::Call::Stats& stats,
                                    VideoMediaInfo* info)
      RTC_EXCLUSIVE_LOCKS_REQUIRED(thread_checker_);
  void FillSendCodecStats(VideoMediaSendInfo* video_media_info)
      RTC_EXCLUSIVE_LOCKS_REQUIRED(thread_checker_);

  absl::optional<VideoCodecSettings>& send_codec() { return send_codec_; }
  const absl::optional<VideoCodecSettings>& send_codec() const {
    return send_codec_;
  }
  webrtc::TaskQueueBase* const worker_thread_;
  webrtc::ScopedTaskSafety task_safety_;
  RTC_NO_UNIQUE_ADDRESS webrtc::SequenceChecker network_thread_checker_{
      webrtc::SequenceChecker::kDetached};
  RTC_NO_UNIQUE_ADDRESS webrtc::SequenceChecker thread_checker_;

  uint32_t rtcp_receiver_report_ssrc_ RTC_GUARDED_BY(thread_checker_);
  bool sending_ RTC_GUARDED_BY(thread_checker_);
  bool receiving_ RTC_GUARDED_BY(&thread_checker_);
  webrtc::Call* const call_;

  rtc::VideoSinkInterface<webrtc::VideoFrame>* default_sink_
      RTC_GUARDED_BY(thread_checker_);

  int default_recv_base_minimum_delay_ms_ RTC_GUARDED_BY(thread_checker_) = 0;

  const MediaConfig::Video video_config_ RTC_GUARDED_BY(thread_checker_);

  std::map<uint32_t, WebRtcVideoSendStream*> send_streams_
      RTC_GUARDED_BY(thread_checker_);

  uint32_t demuxer_criteria_id_ RTC_GUARDED_BY(thread_checker_) = 0;
  uint32_t demuxer_criteria_completed_id_ RTC_GUARDED_BY(thread_checker_) = 0;
  absl::optional<int64_t> last_unsignalled_ssrc_creation_time_ms_
      RTC_GUARDED_BY(thread_checker_);
  std::set<uint32_t> send_ssrcs_ RTC_GUARDED_BY(thread_checker_);
  std::set<uint32_t> receive_ssrcs_ RTC_GUARDED_BY(thread_checker_);

  absl::optional<VideoCodecSettings> send_codec_
      RTC_GUARDED_BY(thread_checker_);
  std::vector<VideoCodecSettings> negotiated_codecs_
      RTC_GUARDED_BY(thread_checker_);

  std::vector<webrtc::RtpExtension> send_rtp_extensions_
      RTC_GUARDED_BY(thread_checker_);

  webrtc::VideoEncoderFactory* const encoder_factory_
      RTC_GUARDED_BY(thread_checker_);
  webrtc::VideoDecoderFactory* const decoder_factory_
      RTC_GUARDED_BY(thread_checker_);
  webrtc::VideoBitrateAllocatorFactory* const bitrate_allocator_factory_
      RTC_GUARDED_BY(thread_checker_);
  std::vector<VideoCodecSettings> recv_codecs_ RTC_GUARDED_BY(thread_checker_);
  webrtc::RtpHeaderExtensionMap recv_rtp_extension_map_
      RTC_GUARDED_BY(thread_checker_);
  std::vector<webrtc::RtpExtension> recv_rtp_extensions_
      RTC_GUARDED_BY(thread_checker_);

  int recv_flexfec_payload_type_ RTC_GUARDED_BY(thread_checker_);
  webrtc::BitrateConstraints bitrate_config_ RTC_GUARDED_BY(thread_checker_);

  VideoSenderParameters send_params_ RTC_GUARDED_BY(thread_checker_);
  VideoOptions default_send_options_ RTC_GUARDED_BY(thread_checker_);
  VideoReceiverParameters recv_params_ RTC_GUARDED_BY(thread_checker_);
  int64_t last_send_stats_log_ms_ RTC_GUARDED_BY(thread_checker_);
  int64_t last_receive_stats_log_ms_ RTC_GUARDED_BY(thread_checker_);
  const bool discard_unknown_ssrc_packets_ RTC_GUARDED_BY(thread_checker_);

  StreamParams unsignaled_stream_params_ RTC_GUARDED_BY(thread_checker_);

  const webrtc::CryptoOptions crypto_options_ RTC_GUARDED_BY(thread_checker_);

  rtc::scoped_refptr<webrtc::FrameTransformerInterface>
      unsignaled_frame_transformer_ RTC_GUARDED_BY(thread_checker_);

  webrtc::VideoReceiveStreamInterface::Config::Rtp rtp_config_;

  absl::AnyInvocable<void()> send_codec_changed_callback_;

  absl::AnyInvocable<void(const std::set<uint32_t>&)>
      ssrc_list_changed_callback_;
};

class WebRtcVideoReceiveChannel : public MediaChannelUtil,
                                  public VideoMediaReceiveChannelInterface {
 public:
  WebRtcVideoReceiveChannel(webrtc::Call* call,
                            const MediaConfig& config,
                            const VideoOptions& options,
                            const webrtc::CryptoOptions& crypto_options,
                            webrtc::VideoDecoderFactory* decoder_factory);
  ~WebRtcVideoReceiveChannel() override;

 public:
  MediaType media_type() const override { return MEDIA_TYPE_VIDEO; }
  VideoMediaReceiveChannelInterface* AsVideoReceiveChannel() override {
    return this;
  }
  VoiceMediaReceiveChannelInterface* AsVoiceReceiveChannel() override {
    RTC_CHECK_NOTREACHED();
    return nullptr;
  }

  void SetInterface(MediaChannelNetworkInterface* iface) override;

  bool SetReceiverParameters(const VideoReceiverParameters& params) override;
  webrtc::RtpParameters GetRtpReceiverParameters(uint32_t ssrc) const override;
  webrtc::RtpParameters GetDefaultRtpReceiveParameters() const override;
  void SetReceive(bool receive) override;
  bool AddRecvStream(const StreamParams& sp) override;
  bool AddDefaultRecvStreamForTesting(const StreamParams& sp) override {

    return AddRecvStream(sp, true);
  }
  bool RemoveRecvStream(uint32_t ssrc) override;
  void ResetUnsignaledRecvStream() override;
  absl::optional<uint32_t> GetUnsignaledSsrc() const override;
  void OnDemuxerCriteriaUpdatePending() override;
  void OnDemuxerCriteriaUpdateComplete() override;
  bool SetSink(uint32_t ssrc,
               rtc::VideoSinkInterface<webrtc::VideoFrame>* sink) override;
  void SetDefaultSink(
      rtc::VideoSinkInterface<webrtc::VideoFrame>* sink) override;
  bool GetStats(VideoMediaReceiveInfo* info) override;
  void OnPacketReceived(const webrtc::RtpPacketReceived& packet) override;
  bool SetBaseMinimumPlayoutDelayMs(uint32_t ssrc, int delay_ms) override;

  absl::optional<int> GetBaseMinimumPlayoutDelayMs(
      uint32_t ssrc) const override;

  void ChooseReceiverReportSsrc(const std::set<uint32_t>& choices) override;

  void SetFrameDecryptor(uint32_t ssrc,
                         rtc::scoped_refptr<webrtc::FrameDecryptorInterface>
                             frame_decryptor) override;
  void SetRecordableEncodedFrameCallback(
      uint32_t ssrc,
      std::function<void(const webrtc::RecordableEncodedFrame&)> callback)
      override;
  void ClearRecordableEncodedFrameCallback(uint32_t ssrc) override;
  void RequestRecvKeyFrame(uint32_t ssrc) override;
  void SetDepacketizerToDecoderFrameTransformer(
      uint32_t ssrc,
      rtc::scoped_refptr<webrtc::FrameTransformerInterface> frame_transformer)
      override;
  std::vector<webrtc::RtpSource> GetSources(uint32_t ssrc) const override;

  void SetReceiverFeedbackParameters(bool lntf_enabled,
                                     bool nack_enabled,
                                     webrtc::RtcpMode rtcp_mode,
                                     absl::optional<int> rtx_time) override;

 private:
  class WebRtcVideoReceiveStream;
  struct ChangedReceiverParameters {

    absl::optional<std::vector<VideoCodecSettings>> codec_settings;
    absl::optional<std::vector<webrtc::RtpExtension>> rtp_header_extensions;

    absl::optional<int> flexfec_payload_type;
  };

  WebRtcVideoReceiveStream* FindReceiveStream(uint32_t ssrc)
      RTC_EXCLUSIVE_LOCKS_REQUIRED(thread_checker_);

  void ProcessReceivedPacket(webrtc::RtpPacketReceived packet)
      RTC_RUN_ON(thread_checker_);

  bool MaybeCreateDefaultReceiveStream(
      const webrtc::RtpPacketReceived& parsed_packet)
      RTC_EXCLUSIVE_LOCKS_REQUIRED(thread_checker_);
  void ReCreateDefaultReceiveStream(uint32_t ssrc,
                                    absl::optional<uint32_t> rtx_ssrc);

  bool AddRecvStream(const StreamParams& sp, bool default_stream);

  void ConfigureReceiverRtp(
      webrtc::VideoReceiveStreamInterface::Config* config,
      webrtc::FlexfecReceiveStream::Config* flexfec_config,
      const StreamParams& sp) const
      RTC_EXCLUSIVE_LOCKS_REQUIRED(thread_checker_);
  bool ValidateReceiveSsrcAvailability(const StreamParams& sp) const
      RTC_EXCLUSIVE_LOCKS_REQUIRED(thread_checker_);
  void DeleteReceiveStream(WebRtcVideoReceiveStream* stream)
      RTC_EXCLUSIVE_LOCKS_REQUIRED(thread_checker_);

  void SetReceiverReportSsrc(uint32_t ssrc) RTC_RUN_ON(&thread_checker_);

  class WebRtcVideoReceiveStream
      : public rtc::VideoSinkInterface<webrtc::VideoFrame> {
   public:
    WebRtcVideoReceiveStream(
        webrtc::Call* call,
        const StreamParams& sp,
        webrtc::VideoReceiveStreamInterface::Config config,
        bool default_stream,
        const std::vector<VideoCodecSettings>& recv_codecs,
        const webrtc::FlexfecReceiveStream::Config& flexfec_config);
    ~WebRtcVideoReceiveStream();

    webrtc::VideoReceiveStreamInterface& stream();

    webrtc::FlexfecReceiveStream* flexfec_stream();

    const std::vector<uint32_t>& GetSsrcs() const;

    std::vector<webrtc::RtpSource> GetSources();

    webrtc::RtpParameters GetRtpParameters() const;

    void SetFeedbackParameters(bool lntf_enabled,
                               bool nack_enabled,
                               webrtc::RtcpMode rtcp_mode,
                               absl::optional<int> rtx_time);
    void SetReceiverParameters(const ChangedReceiverParameters& recv_params);

    void OnFrame(const webrtc::VideoFrame& frame) override;
    bool IsDefaultStream() const;

    void SetFrameDecryptor(
        rtc::scoped_refptr<webrtc::FrameDecryptorInterface> frame_decryptor);

    bool SetBaseMinimumPlayoutDelayMs(int delay_ms);

    int GetBaseMinimumPlayoutDelayMs() const;

    void SetSink(rtc::VideoSinkInterface<webrtc::VideoFrame>* sink);

    VideoReceiverInfo GetVideoReceiverInfo(bool log_stats);

    void SetRecordableEncodedFrameCallback(
        std::function<void(const webrtc::RecordableEncodedFrame&)> callback);
    void ClearRecordableEncodedFrameCallback();
    void GenerateKeyFrame();

    void SetDepacketizerToDecoderFrameTransformer(
        rtc::scoped_refptr<webrtc::FrameTransformerInterface>
            frame_transformer);

    void SetLocalSsrc(uint32_t local_ssrc);
    void UpdateRtxSsrc(uint32_t ssrc);
    void StartReceiveStream();
    void StopReceiveStream();

   private:

    void SetFlexFecPayload(int payload_type);

    void RecreateReceiveStream();
    void CreateReceiveStream();

    bool ReconfigureCodecs(const std::vector<VideoCodecSettings>& recv_codecs);

    webrtc::Call* const call_;
    const StreamParams stream_params_;

    webrtc::VideoReceiveStreamInterface* stream_;
    const bool default_stream_;
    webrtc::VideoReceiveStreamInterface::Config config_;
    webrtc::FlexfecReceiveStream::Config flexfec_config_;
    webrtc::FlexfecReceiveStream* flexfec_stream_;

    webrtc::Mutex sink_lock_;
    rtc::VideoSinkInterface<webrtc::VideoFrame>* sink_
        RTC_GUARDED_BY(sink_lock_);
    int64_t first_frame_timestamp_ RTC_GUARDED_BY(sink_lock_);

    int64_t estimated_remote_start_ntp_time_ms_ RTC_GUARDED_BY(sink_lock_);

    RTC_NO_UNIQUE_ADDRESS webrtc::SequenceChecker thread_checker_;
    bool receiving_ RTC_GUARDED_BY(&thread_checker_);
  };
  bool GetChangedReceiverParameters(const VideoReceiverParameters& params,
                                    ChangedReceiverParameters* changed_params)
      const RTC_EXCLUSIVE_LOCKS_REQUIRED(thread_checker_);

  std::map<uint32_t, WebRtcVideoReceiveStream*> receive_streams_
      RTC_GUARDED_BY(thread_checker_);
  void FillReceiverStats(VideoMediaReceiveInfo* info, bool log_stats)
      RTC_EXCLUSIVE_LOCKS_REQUIRED(thread_checker_);
  void FillReceiveCodecStats(VideoMediaReceiveInfo* video_media_info)
      RTC_EXCLUSIVE_LOCKS_REQUIRED(thread_checker_);

  StreamParams unsignaled_stream_params() {
    RTC_DCHECK_RUN_ON(&thread_checker_);
    return unsignaled_stream_params_;
  }

  webrtc::TaskQueueBase* const worker_thread_;
  webrtc::ScopedTaskSafety task_safety_;
  RTC_NO_UNIQUE_ADDRESS webrtc::SequenceChecker network_thread_checker_{
      webrtc::SequenceChecker::kDetached};
  RTC_NO_UNIQUE_ADDRESS webrtc::SequenceChecker thread_checker_;

  uint32_t rtcp_receiver_report_ssrc_ RTC_GUARDED_BY(thread_checker_);
  bool receiving_ RTC_GUARDED_BY(&thread_checker_);
  webrtc::Call* const call_;

  rtc::VideoSinkInterface<webrtc::VideoFrame>* default_sink_
      RTC_GUARDED_BY(thread_checker_);

  int default_recv_base_minimum_delay_ms_ RTC_GUARDED_BY(thread_checker_) = 0;

  const MediaConfig::Video video_config_ RTC_GUARDED_BY(thread_checker_);

  uint32_t demuxer_criteria_id_ RTC_GUARDED_BY(thread_checker_) = 0;
  uint32_t demuxer_criteria_completed_id_ RTC_GUARDED_BY(thread_checker_) = 0;
  absl::optional<int64_t> last_unsignalled_ssrc_creation_time_ms_
      RTC_GUARDED_BY(thread_checker_);
  std::set<uint32_t> send_ssrcs_ RTC_GUARDED_BY(thread_checker_);
  std::set<uint32_t> receive_ssrcs_ RTC_GUARDED_BY(thread_checker_);

  absl::optional<VideoCodecSettings> send_codec_
      RTC_GUARDED_BY(thread_checker_);
  std::vector<VideoCodecSettings> negotiated_codecs_
      RTC_GUARDED_BY(thread_checker_);

  std::vector<webrtc::RtpExtension> send_rtp_extensions_
      RTC_GUARDED_BY(thread_checker_);

  webrtc::VideoDecoderFactory* const decoder_factory_
      RTC_GUARDED_BY(thread_checker_);
  std::vector<VideoCodecSettings> recv_codecs_ RTC_GUARDED_BY(thread_checker_);
  webrtc::RtpHeaderExtensionMap recv_rtp_extension_map_
      RTC_GUARDED_BY(thread_checker_);
  std::vector<webrtc::RtpExtension> recv_rtp_extensions_
      RTC_GUARDED_BY(thread_checker_);

  int recv_flexfec_payload_type_ RTC_GUARDED_BY(thread_checker_);
  webrtc::BitrateConstraints bitrate_config_ RTC_GUARDED_BY(thread_checker_);

  VideoSenderParameters send_params_ RTC_GUARDED_BY(thread_checker_);
  VideoOptions default_send_options_ RTC_GUARDED_BY(thread_checker_);
  VideoReceiverParameters recv_params_ RTC_GUARDED_BY(thread_checker_);
  int64_t last_receive_stats_log_ms_ RTC_GUARDED_BY(thread_checker_);
  const bool discard_unknown_ssrc_packets_ RTC_GUARDED_BY(thread_checker_);

  StreamParams unsignaled_stream_params_ RTC_GUARDED_BY(thread_checker_);

  const webrtc::CryptoOptions crypto_options_ RTC_GUARDED_BY(thread_checker_);

  rtc::scoped_refptr<webrtc::FrameTransformerInterface>
      unsignaled_frame_transformer_ RTC_GUARDED_BY(thread_checker_);

  webrtc::VideoReceiveStreamInterface::Config::Rtp rtp_config_;

  absl::AnyInvocable<void()> send_codec_changed_callback_;

  absl::AnyInvocable<void(const std::set<uint32_t>&)>
      ssrc_list_changed_callback_;

  const int receive_buffer_size_;
};

class WebRtcVideoChannel : public WebRtcVideoSendChannel {
 public:

  using WebRtcVideoSendChannel::AdaptReason;
};

}

#endif
