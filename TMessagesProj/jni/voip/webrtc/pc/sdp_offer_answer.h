/*
 *  Copyright 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef PC_SDP_OFFER_ANSWER_H_
#define PC_SDP_OFFER_ANSWER_H_

#include <stddef.h>
#include <stdint.h>

#include <functional>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "absl/types/optional.h"
#include "api/audio_options.h"
#include "api/candidate.h"
#include "api/jsep.h"
#include "api/jsep_ice_candidate.h"
#include "api/media_stream_interface.h"
#include "api/media_types.h"
#include "api/peer_connection_interface.h"
#include "api/rtc_error.h"
#include "api/rtp_transceiver_direction.h"
#include "api/rtp_transceiver_interface.h"
#include "api/scoped_refptr.h"
#include "api/sequence_checker.h"
#include "api/set_local_description_observer_interface.h"
#include "api/set_remote_description_observer_interface.h"
#include "api/uma_metrics.h"
#include "api/video/video_bitrate_allocator_factory.h"
#include "media/base/media_channel.h"
#include "media/base/stream_params.h"
#include "p2p/base/port_allocator.h"
#include "pc/connection_context.h"
#include "pc/data_channel_controller.h"
#include "pc/jsep_transport_controller.h"
#include "pc/media_session.h"
#include "pc/media_stream_observer.h"
#include "pc/peer_connection_internal.h"
#include "pc/rtp_receiver.h"
#include "pc/rtp_transceiver.h"
#include "pc/rtp_transmission_manager.h"
#include "pc/sdp_state_provider.h"
#include "pc/session_description.h"
#include "pc/stream_collection.h"
#include "pc/transceiver_list.h"
#include "pc/webrtc_session_description_factory.h"
#include "rtc_base/checks.h"
#include "rtc_base/operations_chain.h"
#include "rtc_base/ssl_stream_adapter.h"
#include "rtc_base/thread.h"
#include "rtc_base/thread_annotations.h"
#include "rtc_base/unique_id_generator.h"
#include "rtc_base/weak_ptr.h"

namespace webrtc {

class SdpOfferAnswerHandler : public SdpStateProvider {
 public:
  ~SdpOfferAnswerHandler();

  static std::unique_ptr<SdpOfferAnswerHandler> Create(
      PeerConnectionSdpMethods* pc,
      const PeerConnectionInterface::RTCConfiguration& configuration,
      PeerConnectionDependencies& dependencies,
      ConnectionContext* context);

  void ResetSessionDescFactory() {
    RTC_DCHECK_RUN_ON(signaling_thread());
    webrtc_session_desc_factory_.reset();
  }
  const WebRtcSessionDescriptionFactory* webrtc_session_desc_factory() const {
    RTC_DCHECK_RUN_ON(signaling_thread());
    return webrtc_session_desc_factory_.get();
  }

  void Close();

  void PrepareForShutdown();

  PeerConnectionInterface::SignalingState signaling_state() const override;

  const SessionDescriptionInterface* local_description() const override;
  const SessionDescriptionInterface* remote_description() const override;
  const SessionDescriptionInterface* current_local_description() const override;
  const SessionDescriptionInterface* current_remote_description()
      const override;
  const SessionDescriptionInterface* pending_local_description() const override;
  const SessionDescriptionInterface* pending_remote_description()
      const override;

  bool NeedsIceRestart(const std::string& content_name) const override;
  bool IceRestartPending(const std::string& content_name) const override;
  absl::optional<rtc::SSLRole> GetDtlsRole(
      const std::string& mid) const override;

  void RestartIce();

  void CreateOffer(
      CreateSessionDescriptionObserver* observer,
      const PeerConnectionInterface::RTCOfferAnswerOptions& options);
  void CreateAnswer(
      CreateSessionDescriptionObserver* observer,
      const PeerConnectionInterface::RTCOfferAnswerOptions& options);

  void SetLocalDescription(
      std::unique_ptr<SessionDescriptionInterface> desc,
      rtc::scoped_refptr<SetLocalDescriptionObserverInterface> observer);
  void SetLocalDescription(
      rtc::scoped_refptr<SetLocalDescriptionObserverInterface> observer);
  void SetLocalDescription(SetSessionDescriptionObserver* observer,
                           SessionDescriptionInterface* desc);
  void SetLocalDescription(SetSessionDescriptionObserver* observer);

  void SetRemoteDescription(
      std::unique_ptr<SessionDescriptionInterface> desc,
      rtc::scoped_refptr<SetRemoteDescriptionObserverInterface> observer);
  void SetRemoteDescription(SetSessionDescriptionObserver* observer,
                            SessionDescriptionInterface* desc);

  PeerConnectionInterface::RTCConfiguration GetConfiguration();
  RTCError SetConfiguration(
      const PeerConnectionInterface::RTCConfiguration& configuration);
  bool AddIceCandidate(const IceCandidateInterface* candidate);
  void AddIceCandidate(std::unique_ptr<IceCandidateInterface> candidate,
                       std::function<void(RTCError)> callback);
  bool RemoveIceCandidates(const std::vector<cricket::Candidate>& candidates);

  void AddLocalIceCandidate(const JsepIceCandidate* candidate);
  void RemoveLocalIceCandidates(
      const std::vector<cricket::Candidate>& candidates);
  bool ShouldFireNegotiationNeededEvent(uint32_t event_id);

  bool AddStream(MediaStreamInterface* local_stream);
  void RemoveStream(MediaStreamInterface* local_stream);

  absl::optional<bool> is_caller() const;
  bool HasNewIceCredentials();
  void UpdateNegotiationNeeded();
  void AllocateSctpSids();

  absl::optional<rtc::SSLRole> GuessSslRole() const;

  void DestroyMediaChannels();

  rtc::scoped_refptr<StreamCollectionInterface> local_streams();
  rtc::scoped_refptr<StreamCollectionInterface> remote_streams();

  bool initial_offerer() {
    RTC_DCHECK_RUN_ON(signaling_thread());
    if (initial_offerer_) {
      return *initial_offerer_;
    }
    return false;
  }

 private:
  class RemoteDescriptionOperation;
  class ImplicitCreateSessionDescriptionObserver;

  friend class ImplicitCreateSessionDescriptionObserver;
  class SetSessionDescriptionObserverAdapter;

  friend class SetSessionDescriptionObserverAdapter;

  enum class SessionError {
    kNone,
    kContent,
    kTransport,
  };

  class LocalIceCredentialsToReplace;

  explicit SdpOfferAnswerHandler(PeerConnectionSdpMethods* pc,
                                 ConnectionContext* context);

  void Initialize(
      const PeerConnectionInterface::RTCConfiguration& configuration,
      PeerConnectionDependencies& dependencies,
      ConnectionContext* context);

  rtc::Thread* signaling_thread() const;
  rtc::Thread* network_thread() const;

  SessionDescriptionInterface* mutable_local_description()
      RTC_RUN_ON(signaling_thread()) {
    return pending_local_description_ ? pending_local_description_.get()
                                      : current_local_description_.get();
  }
  SessionDescriptionInterface* mutable_remote_description()
      RTC_RUN_ON(signaling_thread()) {
    return pending_remote_description_ ? pending_remote_description_.get()
                                       : current_remote_description_.get();
  }

  RTCError ApplyLocalDescription(
      std::unique_ptr<SessionDescriptionInterface> desc,
      const std::map<std::string, const cricket::ContentGroup*>&
          bundle_groups_by_mid);
  void ApplyRemoteDescription(
      std::unique_ptr<RemoteDescriptionOperation> operation);

  RTCError ReplaceRemoteDescription(
      std::unique_ptr<SessionDescriptionInterface> desc,
      SdpType sdp_type,
      std::unique_ptr<SessionDescriptionInterface>* replaced_description)
      RTC_RUN_ON(signaling_thread());

  void ApplyRemoteDescriptionUpdateTransceiverState(SdpType sdp_type);

  void PlanBUpdateSendersAndReceivers(
      const cricket::ContentInfo* audio_content,
      const cricket::AudioContentDescription* audio_desc,
      const cricket::ContentInfo* video_content,
      const cricket::VideoContentDescription* video_desc);

  void DoCreateOffer(
      const PeerConnectionInterface::RTCOfferAnswerOptions& options,
      rtc::scoped_refptr<CreateSessionDescriptionObserver> observer);
  void DoCreateAnswer(
      const PeerConnectionInterface::RTCOfferAnswerOptions& options,
      rtc::scoped_refptr<CreateSessionDescriptionObserver> observer);
  void DoSetLocalDescription(
      std::unique_ptr<SessionDescriptionInterface> desc,
      rtc::scoped_refptr<SetLocalDescriptionObserverInterface> observer);
  void DoSetRemoteDescription(
      std::unique_ptr<RemoteDescriptionOperation> operation);

  void SetRemoteDescriptionPostProcess(bool was_answer)
      RTC_RUN_ON(signaling_thread());

  void ChangeSignalingState(
      PeerConnectionInterface::SignalingState signaling_state);

  RTCError UpdateSessionState(
      SdpType type,
      cricket::ContentSource source,
      const cricket::SessionDescription* description,
      const std::map<std::string, const cricket::ContentGroup*>&
          bundle_groups_by_mid);

  bool IsUnifiedPlan() const RTC_RUN_ON(signaling_thread());

  void OnAudioTrackAdded(AudioTrackInterface* track,
                         MediaStreamInterface* stream)
      RTC_RUN_ON(signaling_thread());
  void OnAudioTrackRemoved(AudioTrackInterface* track,
                           MediaStreamInterface* stream)
      RTC_RUN_ON(signaling_thread());
  void OnVideoTrackAdded(VideoTrackInterface* track,
                         MediaStreamInterface* stream)
      RTC_RUN_ON(signaling_thread());
  void OnVideoTrackRemoved(VideoTrackInterface* track,
                           MediaStreamInterface* stream)
      RTC_RUN_ON(signaling_thread());

  RTCError Rollback(SdpType desc_type);
  void OnOperationsChainEmpty();

  void SetAssociatedRemoteStreams(
      rtc::scoped_refptr<RtpReceiverInternal> receiver,
      const std::vector<std::string>& stream_ids,
      std::vector<rtc::scoped_refptr<MediaStreamInterface>>* added_streams,
      std::vector<rtc::scoped_refptr<MediaStreamInterface>>* removed_streams);

  bool CheckIfNegotiationIsNeeded();
  void GenerateNegotiationNeededEvent();

  RTCError ValidateSessionDescription(
      const SessionDescriptionInterface* sdesc,
      cricket::ContentSource source,
      const std::map<std::string, const cricket::ContentGroup*>&
          bundle_groups_by_mid) RTC_RUN_ON(signaling_thread());

  RTCError UpdateTransceiversAndDataChannels(
      cricket::ContentSource source,
      const SessionDescriptionInterface& new_session,
      const SessionDescriptionInterface* old_local_description,
      const SessionDescriptionInterface* old_remote_description,
      const std::map<std::string, const cricket::ContentGroup*>&
          bundle_groups_by_mid);

  RTCErrorOr<
      rtc::scoped_refptr<RtpTransceiverProxyWithInternal<RtpTransceiver>>>
  AssociateTransceiver(cricket::ContentSource source,
                       SdpType type,
                       size_t mline_index,
                       const cricket::ContentInfo& content,
                       const cricket::ContentInfo* old_local_content,
                       const cricket::ContentInfo* old_remote_content)
      RTC_RUN_ON(signaling_thread());

  const cricket::ContentInfo* FindMediaSectionForTransceiver(
      const RtpTransceiver* transceiver,
      const SessionDescriptionInterface* sdesc) const;

  RTCError UpdateTransceiverChannel(
      rtc::scoped_refptr<RtpTransceiverProxyWithInternal<RtpTransceiver>>
          transceiver,
      const cricket::ContentInfo& content,
      const cricket::ContentGroup* bundle_group) RTC_RUN_ON(signaling_thread());

  RTCError UpdateDataChannelTransport(cricket::ContentSource source,
                                      const cricket::ContentInfo& content,
                                      const cricket::ContentGroup* bundle_group)
      RTC_RUN_ON(signaling_thread());

  bool ExpectSetLocalDescription(SdpType type);

  bool ExpectSetRemoteDescription(SdpType type);

  void FillInMissingRemoteMids(cricket::SessionDescription* remote_description);

  rtc::scoped_refptr<RtpTransceiverProxyWithInternal<RtpTransceiver>>
  FindAvailableTransceiverToReceive(cricket::MediaType media_type) const;

  void GetOptionsForOffer(const PeerConnectionInterface::RTCOfferAnswerOptions&
                              offer_answer_options,
                          cricket::MediaSessionOptions* session_options);
  void GetOptionsForPlanBOffer(
      const PeerConnectionInterface::RTCOfferAnswerOptions&
          offer_answer_options,
      cricket::MediaSessionOptions* session_options)
      RTC_RUN_ON(signaling_thread());
  void GetOptionsForUnifiedPlanOffer(
      const PeerConnectionInterface::RTCOfferAnswerOptions&
          offer_answer_options,
      cricket::MediaSessionOptions* session_options)
      RTC_RUN_ON(signaling_thread());

  void GetOptionsForAnswer(const PeerConnectionInterface::RTCOfferAnswerOptions&
                               offer_answer_options,
                           cricket::MediaSessionOptions* session_options);
  void GetOptionsForPlanBAnswer(
      const PeerConnectionInterface::RTCOfferAnswerOptions&
          offer_answer_options,
      cricket::MediaSessionOptions* session_options)
      RTC_RUN_ON(signaling_thread());
  void GetOptionsForUnifiedPlanAnswer(
      const PeerConnectionInterface::RTCOfferAnswerOptions&
          offer_answer_options,
      cricket::MediaSessionOptions* session_options)
      RTC_RUN_ON(signaling_thread());

  const char* SessionErrorToString(SessionError error) const;
  std::string GetSessionErrorMsg();

  SessionError session_error() const {
    RTC_DCHECK_RUN_ON(signaling_thread());
    return session_error_;
  }
  const std::string& session_error_desc() const { return session_error_desc_; }

  RTCError HandleLegacyOfferOptions(
      const PeerConnectionInterface::RTCOfferAnswerOptions& options);
  void RemoveRecvDirectionFromReceivingTransceiversOfType(
      cricket::MediaType media_type) RTC_RUN_ON(signaling_thread());
  void AddUpToOneReceivingTransceiverOfType(cricket::MediaType media_type);

  std::vector<
      rtc::scoped_refptr<RtpTransceiverProxyWithInternal<RtpTransceiver>>>
  GetReceivingTransceiversOfType(cricket::MediaType media_type)
      RTC_RUN_ON(signaling_thread());

  void ProcessRemovalOfRemoteTrack(
      const rtc::scoped_refptr<RtpTransceiverProxyWithInternal<RtpTransceiver>>
          transceiver,
      std::vector<rtc::scoped_refptr<RtpTransceiverInterface>>* remove_list,
      std::vector<rtc::scoped_refptr<MediaStreamInterface>>* removed_streams);

  void RemoveRemoteStreamsIfEmpty(
      const std::vector<rtc::scoped_refptr<MediaStreamInterface>>&
          remote_streams,
      std::vector<rtc::scoped_refptr<MediaStreamInterface>>* removed_streams);

  void RemoveSenders(cricket::MediaType media_type);

  void UpdateLocalSenders(const std::vector<cricket::StreamParams>& streams,
                          cricket::MediaType media_type);

  void UpdateRemoteSendersList(
      const std::vector<cricket::StreamParams>& streams,
      bool default_track_needed,
      cricket::MediaType media_type,
      StreamCollection* new_streams);

  void EnableSending();

  RTCError PushdownMediaDescription(
      SdpType type,
      cricket::ContentSource source,
      const std::map<std::string, const cricket::ContentGroup*>&
          bundle_groups_by_mid);

  RTCError PushdownTransportDescription(cricket::ContentSource source,
                                        SdpType type);

  void RemoveStoppedTransceivers();

  void RemoveUnusedChannels(const cricket::SessionDescription* desc);

  void UpdateEndedRemoteMediaStreams();

  bool UseCandidatesInRemoteDescription();

  bool UseCandidate(const IceCandidateInterface* candidate);

  bool ReadyToUseRemoteCandidate(const IceCandidateInterface* candidate,
                                 const SessionDescriptionInterface* remote_desc,
                                 bool* valid);

  RTCErrorOr<const cricket::ContentInfo*> FindContentInfo(
      const SessionDescriptionInterface* description,
      const IceCandidateInterface* candidate) RTC_RUN_ON(signaling_thread());

  RTCError CreateChannels(const cricket::SessionDescription& desc);

  void GenerateMediaDescriptionOptions(
      const SessionDescriptionInterface* session_desc,
      RtpTransceiverDirection audio_direction,
      RtpTransceiverDirection video_direction,
      absl::optional<size_t>* audio_index,
      absl::optional<size_t>* video_index,
      absl::optional<size_t>* data_index,
      cricket::MediaSessionOptions* session_options);

  cricket::MediaDescriptionOptions GetMediaDescriptionOptionsForActiveData(
      const std::string& mid) const;

  cricket::MediaDescriptionOptions GetMediaDescriptionOptionsForRejectedData(
      const std::string& mid) const;

  bool UpdatePayloadTypeDemuxingState(
      cricket::ContentSource source,
      const std::map<std::string, const cricket::ContentGroup*>&
          bundle_groups_by_mid);

  void SetSessionError(SessionError error, const std::string& error_desc);

  AddIceCandidateResult AddIceCandidateInternal(
      const IceCandidateInterface* candidate);

  cricket::MediaEngineInterface* media_engine() const;
  TransceiverList* transceivers();
  const TransceiverList* transceivers() const;
  DataChannelController* data_channel_controller();
  const DataChannelController* data_channel_controller() const;
  cricket::PortAllocator* port_allocator();
  const cricket::PortAllocator* port_allocator() const;
  RtpTransmissionManager* rtp_manager();
  const RtpTransmissionManager* rtp_manager() const;
  JsepTransportController* transport_controller_s()
      RTC_RUN_ON(signaling_thread());
  const JsepTransportController* transport_controller_s() const
      RTC_RUN_ON(signaling_thread());
  JsepTransportController* transport_controller_n()
      RTC_RUN_ON(network_thread());
  const JsepTransportController* transport_controller_n() const
      RTC_RUN_ON(network_thread());

  const cricket::AudioOptions& audio_options() { return audio_options_; }
  const cricket::VideoOptions& video_options() { return video_options_; }
  bool ConfiguredForMedia() const;

  PeerConnectionSdpMethods* const pc_;
  ConnectionContext* const context_;

  std::unique_ptr<WebRtcSessionDescriptionFactory> webrtc_session_desc_factory_
      RTC_GUARDED_BY(signaling_thread());

  std::unique_ptr<SessionDescriptionInterface> current_local_description_
      RTC_GUARDED_BY(signaling_thread());
  std::unique_ptr<SessionDescriptionInterface> pending_local_description_
      RTC_GUARDED_BY(signaling_thread());
  std::unique_ptr<SessionDescriptionInterface> current_remote_description_
      RTC_GUARDED_BY(signaling_thread());
  std::unique_ptr<SessionDescriptionInterface> pending_remote_description_
      RTC_GUARDED_BY(signaling_thread());

  PeerConnectionInterface::SignalingState signaling_state_
      RTC_GUARDED_BY(signaling_thread()) = PeerConnectionInterface::kStable;

  absl::optional<bool> is_caller_ RTC_GUARDED_BY(signaling_thread());

  const rtc::scoped_refptr<StreamCollection> local_streams_
      RTC_GUARDED_BY(signaling_thread());

  const rtc::scoped_refptr<StreamCollection> remote_streams_
      RTC_GUARDED_BY(signaling_thread());

  std::vector<std::unique_ptr<MediaStreamObserver>> stream_observers_
      RTC_GUARDED_BY(signaling_thread());

  rtc::scoped_refptr<rtc::OperationsChain> operations_chain_
      RTC_GUARDED_BY(signaling_thread());

  const std::string rtcp_cname_;

  rtc::UniqueStringGenerator mid_generator_ RTC_GUARDED_BY(signaling_thread());

  std::set<std::string> pending_ice_restarts_
      RTC_GUARDED_BY(signaling_thread());

  std::unique_ptr<LocalIceCredentialsToReplace>
      local_ice_credentials_to_replace_ RTC_GUARDED_BY(signaling_thread());

  bool remote_peer_supports_msid_ RTC_GUARDED_BY(signaling_thread()) = false;
  bool is_negotiation_needed_ RTC_GUARDED_BY(signaling_thread()) = false;
  uint32_t negotiation_needed_event_id_ RTC_GUARDED_BY(signaling_thread()) = 0;
  bool update_negotiation_needed_on_empty_chain_
      RTC_GUARDED_BY(signaling_thread()) = false;

  bool pt_demuxing_has_been_used_audio_ RTC_GUARDED_BY(signaling_thread()) =
      false;
  bool pt_demuxing_has_been_used_video_ RTC_GUARDED_BY(signaling_thread()) =
      false;

  rtc::scoped_refptr<MediaStreamInterface> missing_msid_default_stream_
      RTC_GUARDED_BY(signaling_thread());

  SessionError session_error_ RTC_GUARDED_BY(signaling_thread()) =
      SessionError::kNone;
  std::string session_error_desc_ RTC_GUARDED_BY(signaling_thread());

  cricket::AudioOptions audio_options_ RTC_GUARDED_BY(signaling_thread());
  cricket::VideoOptions video_options_ RTC_GUARDED_BY(signaling_thread());

  std::unique_ptr<VideoBitrateAllocatorFactory> video_bitrate_allocator_factory_
      RTC_GUARDED_BY(signaling_thread());

  absl::optional<bool> initial_offerer_ RTC_GUARDED_BY(signaling_thread());

  rtc::WeakPtrFactory<SdpOfferAnswerHandler> weak_ptr_factory_
      RTC_GUARDED_BY(signaling_thread());
};

}

#endif
