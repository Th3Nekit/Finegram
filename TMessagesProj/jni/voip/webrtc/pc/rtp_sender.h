/*
 *  Copyright 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// This file contains classes that implement RtpSenderInterface.
// An RtpSender associates a MediaStreamTrackInterface with an underlying
// transport (provided by AudioProviderInterface/VideoProviderInterface)

#ifndef PC_RTP_SENDER_H_
#define PC_RTP_SENDER_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "absl/types/optional.h"
#include "api/crypto/frame_encryptor_interface.h"
#include "api/dtls_transport_interface.h"
#include "api/dtmf_sender_interface.h"
#include "api/frame_transformer_interface.h"
#include "api/media_stream_interface.h"
#include "api/media_types.h"
#include "api/rtc_error.h"
#include "api/rtp_parameters.h"
#include "api/rtp_sender_interface.h"
#include "api/scoped_refptr.h"
#include "api/sequence_checker.h"
#include "media/base/audio_source.h"
#include "media/base/media_channel.h"
#include "pc/dtmf_sender.h"
#include "pc/legacy_stats_collector_interface.h"
#include "rtc_base/checks.h"
#include "rtc_base/synchronization/mutex.h"
#include "rtc_base/thread.h"
#include "rtc_base/thread_annotations.h"

namespace webrtc {

bool UnimplementedRtpParameterHasValue(const RtpParameters& parameters);

class RtpSenderInternal : public RtpSenderInterface {
 public:

  virtual void SetMediaChannel(
      cricket::MediaSendChannelInterface* media_channel) = 0;

  virtual void SetSsrc(uint32_t ssrc) = 0;

  virtual void set_stream_ids(const std::vector<std::string>& stream_ids) = 0;
  virtual void set_init_send_encodings(
      const std::vector<RtpEncodingParameters>& init_send_encodings) = 0;
  virtual void set_transport(
      rtc::scoped_refptr<DtlsTransportInterface> dtls_transport) = 0;

  virtual void Stop() = 0;

  virtual RtpParameters GetParametersInternal() const = 0;
  virtual void SetParametersInternal(const RtpParameters& parameters,
                                     SetParametersCallback,
                                     bool blocking) = 0;

  virtual RtpParameters GetParametersInternalWithAllLayers() const = 0;
  virtual RTCError SetParametersInternalWithAllLayers(
      const RtpParameters& parameters) = 0;

  virtual RTCError CheckCodecParameters(const RtpParameters& parameters) = 0;

  virtual int AttachmentId() const = 0;

  virtual RTCError DisableEncodingLayers(
      const std::vector<std::string>& rid) = 0;

  virtual void SetTransceiverAsStopped() = 0;

  virtual void SetCodecPreferences(
      std::vector<cricket::Codec> codec_preferences) = 0;
};

class RtpSenderBase : public RtpSenderInternal, public ObserverInterface {
 public:
  class SetStreamsObserver {
   public:
    virtual ~SetStreamsObserver() = default;
    virtual void OnSetStreams() = 0;
  };

  void SetMediaChannel(
      cricket::MediaSendChannelInterface* media_channel) override;

  bool SetTrack(MediaStreamTrackInterface* track) override;
  rtc::scoped_refptr<MediaStreamTrackInterface> track() const override {

    return track_;
  }

  RtpParameters GetParameters() const override;
  RTCError SetParameters(const RtpParameters& parameters) override;
  void SetParametersAsync(const RtpParameters& parameters,
                          SetParametersCallback callback) override;

  RtpParameters GetParametersInternal() const override;
  void SetParametersInternal(const RtpParameters& parameters,
                             SetParametersCallback callback = nullptr,
                             bool blocking = true) override;
  RTCError CheckSetParameters(const RtpParameters& parameters);
  RTCError CheckCodecParameters(const RtpParameters& parameters) override;
  RtpParameters GetParametersInternalWithAllLayers() const override;
  RTCError SetParametersInternalWithAllLayers(
      const RtpParameters& parameters) override;

  void SetSsrc(uint32_t ssrc) override;
  uint32_t ssrc() const override {

    return ssrc_;
  }

  std::vector<std::string> stream_ids() const override {
    RTC_DCHECK_RUN_ON(signaling_thread_);
    return stream_ids_;
  }

  void set_stream_ids(const std::vector<std::string>& stream_ids) override;
  void SetStreams(const std::vector<std::string>& stream_ids) override;

  std::string id() const override { return id_; }

  void set_init_send_encodings(
      const std::vector<RtpEncodingParameters>& init_send_encodings) override {
    init_parameters_.encodings = init_send_encodings;
  }
  std::vector<RtpEncodingParameters> init_send_encodings() const override {
    RTC_DCHECK_RUN_ON(signaling_thread_);
    return init_parameters_.encodings;
  }

  void set_transport(
      rtc::scoped_refptr<DtlsTransportInterface> dtls_transport) override {
    dtls_transport_ = dtls_transport;
  }
  rtc::scoped_refptr<DtlsTransportInterface> dtls_transport() const override {
    RTC_DCHECK_RUN_ON(signaling_thread_);
    return dtls_transport_;
  }

  void SetFrameEncryptor(
      rtc::scoped_refptr<FrameEncryptorInterface> frame_encryptor) override;

  rtc::scoped_refptr<FrameEncryptorInterface> GetFrameEncryptor()
      const override {
    return frame_encryptor_;
  }

  void Stop() override;

  int AttachmentId() const override { return attachment_id_; }

  RTCError DisableEncodingLayers(const std::vector<std::string>& rid) override;

  void SetEncoderToPacketizerFrameTransformer(
      rtc::scoped_refptr<FrameTransformerInterface> frame_transformer) override;

  void SetEncoderSelector(
      std::unique_ptr<VideoEncoderFactory::EncoderSelectorInterface>
          encoder_selector) override;

  void SetEncoderSelectorOnChannel();

  void SetTransceiverAsStopped() override {
    RTC_DCHECK_RUN_ON(signaling_thread_);
    is_transceiver_stopped_ = true;
  }

  void SetCodecPreferences(
      std::vector<cricket::Codec> codec_preferences) override {
    codec_preferences_ = codec_preferences;
  }

 protected:

  RtpSenderBase(rtc::Thread* worker_thread,
                const std::string& id,
                SetStreamsObserver* set_streams_observer);

  bool can_send_track() const { return track_ && ssrc_; }

  virtual std::string track_kind() const = 0;

  virtual void SetSend() = 0;

  virtual void ClearSend() = 0;

  virtual void AttachTrack() {}
  virtual void DetachTrack() {}
  virtual void AddTrackToStats() {}
  virtual void RemoveTrackFromStats() {}

  rtc::Thread* const signaling_thread_;
  rtc::Thread* const worker_thread_;
  uint32_t ssrc_ = 0;
  bool stopped_ RTC_GUARDED_BY(signaling_thread_) = false;
  bool is_transceiver_stopped_ RTC_GUARDED_BY(signaling_thread_) = false;
  int attachment_id_ = 0;
  const std::string id_;

  std::vector<std::string> stream_ids_;
  RtpParameters init_parameters_;
  std::vector<cricket::Codec> codec_preferences_;

  cricket::MediaSendChannelInterface* media_channel_ = nullptr;
  rtc::scoped_refptr<MediaStreamTrackInterface> track_;

  rtc::scoped_refptr<DtlsTransportInterface> dtls_transport_;
  rtc::scoped_refptr<FrameEncryptorInterface> frame_encryptor_;

  mutable absl::optional<std::string> last_transaction_id_;
  std::vector<std::string> disabled_rids_;

  SetStreamsObserver* set_streams_observer_ = nullptr;

  rtc::scoped_refptr<FrameTransformerInterface> frame_transformer_;
  std::unique_ptr<VideoEncoderFactory::EncoderSelectorInterface>
      encoder_selector_;

  virtual RTCError GenerateKeyFrame(const std::vector<std::string>& rids) = 0;
};

class LocalAudioSinkAdapter : public AudioTrackSinkInterface,
                              public cricket::AudioSource {
 public:
  LocalAudioSinkAdapter();
  virtual ~LocalAudioSinkAdapter();

 private:

  void OnData(const void* audio_data,
              int bits_per_sample,
              int sample_rate,
              size_t number_of_channels,
              size_t number_of_frames,
              absl::optional<int64_t> absolute_capture_timestamp_ms) override;

  void OnData(const void* audio_data,
              int bits_per_sample,
              int sample_rate,
              size_t number_of_channels,
              size_t number_of_frames) override {
    OnData(audio_data, bits_per_sample, sample_rate, number_of_channels,
           number_of_frames,
                                             absl::nullopt);
  }

  int NumPreferredChannels() const override { return num_preferred_channels_; }

  void SetSink(cricket::AudioSource::Sink* sink) override;

  cricket::AudioSource::Sink* sink_;

  Mutex lock_;
  int num_preferred_channels_ = -1;
};

class AudioRtpSender : public DtmfProviderInterface, public RtpSenderBase {
 public:

  static rtc::scoped_refptr<AudioRtpSender> Create(
      rtc::Thread* worker_thread,
      const std::string& id,
      LegacyStatsCollectorInterface* stats,
      SetStreamsObserver* set_streams_observer);
  virtual ~AudioRtpSender();

  bool CanInsertDtmf() override;
  bool InsertDtmf(int code, int duration) override;

  void OnChanged() override;

  cricket::MediaType media_type() const override {
    return cricket::MEDIA_TYPE_AUDIO;
  }
  std::string track_kind() const override {
    return MediaStreamTrackInterface::kAudioKind;
  }

  rtc::scoped_refptr<DtmfSenderInterface> GetDtmfSender() const override;
  RTCError GenerateKeyFrame(const std::vector<std::string>& rids) override;

 protected:
  AudioRtpSender(rtc::Thread* worker_thread,
                 const std::string& id,
                 LegacyStatsCollectorInterface* legacy_stats,
                 SetStreamsObserver* set_streams_observer);

  void SetSend() override;
  void ClearSend() override;

  void AttachTrack() override;
  void DetachTrack() override;
  void AddTrackToStats() override;
  void RemoveTrackFromStats() override;

 private:
  cricket::VoiceMediaSendChannelInterface* voice_media_channel() {
    return media_channel_->AsVoiceSendChannel();
  }
  rtc::scoped_refptr<AudioTrackInterface> audio_track() const {
    return rtc::scoped_refptr<AudioTrackInterface>(
        static_cast<AudioTrackInterface*>(track_.get()));
  }

  LegacyStatsCollectorInterface* legacy_stats_ = nullptr;
  rtc::scoped_refptr<DtmfSender> dtmf_sender_;
  rtc::scoped_refptr<DtmfSenderInterface> dtmf_sender_proxy_;
  bool cached_track_enabled_ = false;

  std::unique_ptr<LocalAudioSinkAdapter> sink_adapter_;
};

class VideoRtpSender : public RtpSenderBase {
 public:

  static rtc::scoped_refptr<VideoRtpSender> Create(
      rtc::Thread* worker_thread,
      const std::string& id,
      SetStreamsObserver* set_streams_observer);
  virtual ~VideoRtpSender();

  void OnChanged() override;

  cricket::MediaType media_type() const override {
    return cricket::MEDIA_TYPE_VIDEO;
  }
  std::string track_kind() const override {
    return MediaStreamTrackInterface::kVideoKind;
  }

  rtc::scoped_refptr<DtmfSenderInterface> GetDtmfSender() const override;
  RTCError GenerateKeyFrame(const std::vector<std::string>& rids) override;

 protected:
  VideoRtpSender(rtc::Thread* worker_thread,
                 const std::string& id,
                 SetStreamsObserver* set_streams_observer);

  void SetSend() override;
  void ClearSend() override;

  void AttachTrack() override;

 private:
  cricket::VideoMediaSendChannelInterface* video_media_channel() {
    return media_channel_->AsVideoSendChannel();
  }
  rtc::scoped_refptr<VideoTrackInterface> video_track() const {
    return rtc::scoped_refptr<VideoTrackInterface>(
        static_cast<VideoTrackInterface*>(track_.get()));
  }

  VideoTrackInterface::ContentHint cached_track_content_hint_ =
      VideoTrackInterface::ContentHint::kNone;
};

}

#endif
