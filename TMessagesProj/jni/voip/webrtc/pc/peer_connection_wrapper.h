/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef PC_PEER_CONNECTION_WRAPPER_H_
#define PC_PEER_CONNECTION_WRAPPER_H_

#include <memory>
#include <string>
#include <vector>

#include "absl/types/optional.h"
#include "api/data_channel_interface.h"
#include "api/function_view.h"
#include "api/jsep.h"
#include "api/media_stream_interface.h"
#include "api/media_types.h"
#include "api/peer_connection_interface.h"
#include "api/rtc_error.h"
#include "api/rtp_sender_interface.h"
#include "api/rtp_transceiver_interface.h"
#include "api/scoped_refptr.h"
#include "api/stats/rtc_stats_report.h"
#include "pc/test/mock_peer_connection_observers.h"

namespace webrtc {

class PeerConnectionWrapper {
 public:

  PeerConnectionWrapper(
      rtc::scoped_refptr<PeerConnectionFactoryInterface> pc_factory,
      rtc::scoped_refptr<PeerConnectionInterface> pc,
      std::unique_ptr<MockPeerConnectionObserver> observer);
  virtual ~PeerConnectionWrapper();

  PeerConnectionFactoryInterface* pc_factory();
  PeerConnectionInterface* pc();
  MockPeerConnectionObserver* observer();

  std::unique_ptr<SessionDescriptionInterface> CreateOffer(
      const PeerConnectionInterface::RTCOfferAnswerOptions& options,
      std::string* error_out = nullptr);

  std::unique_ptr<SessionDescriptionInterface> CreateOffer();

  std::unique_ptr<SessionDescriptionInterface> CreateOfferAndSetAsLocal(
      const PeerConnectionInterface::RTCOfferAnswerOptions& options);

  std::unique_ptr<SessionDescriptionInterface> CreateOfferAndSetAsLocal();

  std::unique_ptr<SessionDescriptionInterface> CreateAnswer(
      const PeerConnectionInterface::RTCOfferAnswerOptions& options,
      std::string* error_out = nullptr);

  std::unique_ptr<SessionDescriptionInterface> CreateAnswer();

  std::unique_ptr<SessionDescriptionInterface> CreateAnswerAndSetAsLocal(
      const PeerConnectionInterface::RTCOfferAnswerOptions& options);

  std::unique_ptr<SessionDescriptionInterface> CreateAnswerAndSetAsLocal();
  std::unique_ptr<SessionDescriptionInterface> CreateRollback();

  bool SetLocalDescription(std::unique_ptr<SessionDescriptionInterface> desc,
                           std::string* error_out = nullptr);

  bool SetRemoteDescription(std::unique_ptr<SessionDescriptionInterface> desc,
                            std::string* error_out = nullptr);
  bool SetRemoteDescription(std::unique_ptr<SessionDescriptionInterface> desc,
                            RTCError* error_out);

  bool ExchangeOfferAnswerWith(PeerConnectionWrapper* answerer);
  bool ExchangeOfferAnswerWith(
      PeerConnectionWrapper* answerer,
      const PeerConnectionInterface::RTCOfferAnswerOptions& offer_options,
      const PeerConnectionInterface::RTCOfferAnswerOptions& answer_options);

  rtc::scoped_refptr<RtpTransceiverInterface> AddTransceiver(
      cricket::MediaType media_type);
  rtc::scoped_refptr<RtpTransceiverInterface> AddTransceiver(
      cricket::MediaType media_type,
      const RtpTransceiverInit& init);
  rtc::scoped_refptr<RtpTransceiverInterface> AddTransceiver(
      rtc::scoped_refptr<MediaStreamTrackInterface> track);
  rtc::scoped_refptr<RtpTransceiverInterface> AddTransceiver(
      rtc::scoped_refptr<MediaStreamTrackInterface> track,
      const RtpTransceiverInit& init);

  rtc::scoped_refptr<AudioTrackInterface> CreateAudioTrack(
      const std::string& label);

  rtc::scoped_refptr<VideoTrackInterface> CreateVideoTrack(
      const std::string& label);

  rtc::scoped_refptr<RtpSenderInterface> AddTrack(
      rtc::scoped_refptr<MediaStreamTrackInterface> track,
      const std::vector<std::string>& stream_ids = {});

  rtc::scoped_refptr<RtpSenderInterface> AddTrack(
      rtc::scoped_refptr<MediaStreamTrackInterface> track,
      const std::vector<std::string>& stream_ids,
      const std::vector<RtpEncodingParameters>& init_send_encodings);

  rtc::scoped_refptr<RtpSenderInterface> AddAudioTrack(
      const std::string& track_label,
      const std::vector<std::string>& stream_ids = {});

  rtc::scoped_refptr<RtpSenderInterface> AddVideoTrack(
      const std::string& track_label,
      const std::vector<std::string>& stream_ids = {});

  rtc::scoped_refptr<DataChannelInterface> CreateDataChannel(
      const std::string& label,
      const absl::optional<DataChannelInit>& config = absl::nullopt);

  PeerConnectionInterface::SignalingState signaling_state();

  bool IsIceGatheringDone();

  bool IsIceConnected();

  rtc::scoped_refptr<const RTCStatsReport> GetStats();

 private:
  std::unique_ptr<SessionDescriptionInterface> CreateSdp(
      rtc::FunctionView<void(CreateSessionDescriptionObserver*)> fn,
      std::string* error_out);
  bool SetSdp(rtc::FunctionView<void(SetSessionDescriptionObserver*)> fn,
              std::string* error_out);

  rtc::scoped_refptr<PeerConnectionFactoryInterface> pc_factory_;
  std::unique_ptr<MockPeerConnectionObserver> observer_;
  rtc::scoped_refptr<PeerConnectionInterface> pc_;
};

}

#endif
