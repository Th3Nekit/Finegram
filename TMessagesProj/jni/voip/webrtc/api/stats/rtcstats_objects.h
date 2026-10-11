/*
 *  Copyright 2016 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_STATS_RTCSTATS_OBJECTS_H_
#define API_STATS_RTCSTATS_OBJECTS_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "absl/types/optional.h"
#include "api/stats/rtc_stats.h"
#include "rtc_base/system/rtc_export.h"

namespace webrtc {

class RTC_EXPORT RTCCertificateStats final : public RTCStats {
 public:
  WEBRTC_RTCSTATS_DECL();
  RTCCertificateStats(std::string id, Timestamp timestamp);
  ~RTCCertificateStats() override;

  absl::optional<std::string> fingerprint;
  absl::optional<std::string> fingerprint_algorithm;
  absl::optional<std::string> base64_certificate;
  absl::optional<std::string> issuer_certificate_id;
};

class RTC_EXPORT RTCCodecStats final : public RTCStats {
 public:
  WEBRTC_RTCSTATS_DECL();
  RTCCodecStats(std::string id, Timestamp timestamp);
  ~RTCCodecStats() override;

  absl::optional<std::string> transport_id;
  absl::optional<uint32_t> payload_type;
  absl::optional<std::string> mime_type;
  absl::optional<uint32_t> clock_rate;
  absl::optional<uint32_t> channels;
  absl::optional<std::string> sdp_fmtp_line;
};

class RTC_EXPORT RTCDataChannelStats final : public RTCStats {
 public:
  WEBRTC_RTCSTATS_DECL();
  RTCDataChannelStats(std::string id, Timestamp timestamp);
  ~RTCDataChannelStats() override;

  absl::optional<std::string> label;
  absl::optional<std::string> protocol;
  absl::optional<int32_t> data_channel_identifier;
  absl::optional<std::string> state;
  absl::optional<uint32_t> messages_sent;
  absl::optional<uint64_t> bytes_sent;
  absl::optional<uint32_t> messages_received;
  absl::optional<uint64_t> bytes_received;
};

class RTC_EXPORT RTCIceCandidatePairStats final : public RTCStats {
 public:
  WEBRTC_RTCSTATS_DECL();
  RTCIceCandidatePairStats(std::string id, Timestamp timestamp);
  ~RTCIceCandidatePairStats() override;

  absl::optional<std::string> transport_id;
  absl::optional<std::string> local_candidate_id;
  absl::optional<std::string> remote_candidate_id;
  absl::optional<std::string> state;

  absl::optional<uint64_t> priority;
  absl::optional<bool> nominated;

  absl::optional<bool> writable;
  absl::optional<uint64_t> packets_sent;
  absl::optional<uint64_t> packets_received;
  absl::optional<uint64_t> bytes_sent;
  absl::optional<uint64_t> bytes_received;
  absl::optional<double> total_round_trip_time;
  absl::optional<double> current_round_trip_time;
  absl::optional<double> available_outgoing_bitrate;
  absl::optional<double> available_incoming_bitrate;
  absl::optional<uint64_t> requests_received;
  absl::optional<uint64_t> requests_sent;
  absl::optional<uint64_t> responses_received;
  absl::optional<uint64_t> responses_sent;
  absl::optional<uint64_t> consent_requests_sent;
  absl::optional<uint64_t> packets_discarded_on_send;
  absl::optional<uint64_t> bytes_discarded_on_send;
  absl::optional<double> last_packet_received_timestamp;
  absl::optional<double> last_packet_sent_timestamp;
};

class RTC_EXPORT RTCIceCandidateStats : public RTCStats {
 public:
  WEBRTC_RTCSTATS_DECL();
  ~RTCIceCandidateStats() override;

  absl::optional<std::string> transport_id;

  absl::optional<bool> is_remote;
  absl::optional<std::string> network_type;
  absl::optional<std::string> ip;
  absl::optional<std::string> address;
  absl::optional<int32_t> port;
  absl::optional<std::string> protocol;
  absl::optional<std::string> relay_protocol;
  absl::optional<std::string> candidate_type;
  absl::optional<int32_t> priority;
  absl::optional<std::string> url;
  absl::optional<std::string> foundation;
  absl::optional<std::string> related_address;
  absl::optional<int32_t> related_port;
  absl::optional<std::string> username_fragment;
  absl::optional<std::string> tcp_type;

  absl::optional<bool> vpn;
  absl::optional<std::string> network_adapter_type;

 protected:
  RTCIceCandidateStats(std::string id, Timestamp timestamp, bool is_remote);
};

class RTC_EXPORT RTCLocalIceCandidateStats final : public RTCIceCandidateStats {
 public:
  static const char kType[];
  RTCLocalIceCandidateStats(std::string id, Timestamp timestamp);
  std::unique_ptr<RTCStats> copy() const override;
  const char* type() const override;
};

class RTC_EXPORT RTCRemoteIceCandidateStats final
    : public RTCIceCandidateStats {
 public:
  static const char kType[];
  RTCRemoteIceCandidateStats(std::string id, Timestamp timestamp);
  std::unique_ptr<RTCStats> copy() const override;
  const char* type() const override;
};

class RTC_EXPORT RTCPeerConnectionStats final : public RTCStats {
 public:
  WEBRTC_RTCSTATS_DECL();
  RTCPeerConnectionStats(std::string id, Timestamp timestamp);
  ~RTCPeerConnectionStats() override;

  absl::optional<uint32_t> data_channels_opened;
  absl::optional<uint32_t> data_channels_closed;
};

class RTC_EXPORT RTCRtpStreamStats : public RTCStats {
 public:
  WEBRTC_RTCSTATS_DECL();
  ~RTCRtpStreamStats() override;

  absl::optional<uint32_t> ssrc;
  absl::optional<std::string> kind;
  absl::optional<std::string> transport_id;
  absl::optional<std::string> codec_id;

 protected:
  RTCRtpStreamStats(std::string id, Timestamp timestamp);
};

class RTC_EXPORT RTCReceivedRtpStreamStats : public RTCRtpStreamStats {
 public:
  WEBRTC_RTCSTATS_DECL();
  ~RTCReceivedRtpStreamStats() override;

  absl::optional<double> jitter;
  absl::optional<int32_t> packets_lost;

 protected:
  RTCReceivedRtpStreamStats(std::string id, Timestamp timestamp);
};

class RTC_EXPORT RTCSentRtpStreamStats : public RTCRtpStreamStats {
 public:
  WEBRTC_RTCSTATS_DECL();
  ~RTCSentRtpStreamStats() override;

  absl::optional<uint64_t> packets_sent;
  absl::optional<uint64_t> bytes_sent;

 protected:
  RTCSentRtpStreamStats(std::string id, Timestamp timestamp);
};

class RTC_EXPORT RTCInboundRtpStreamStats final
    : public RTCReceivedRtpStreamStats {
 public:
  WEBRTC_RTCSTATS_DECL();
  RTCInboundRtpStreamStats(std::string id, Timestamp timestamp);
  ~RTCInboundRtpStreamStats() override;

  absl::optional<std::string> playout_id;
  absl::optional<std::string> track_identifier;
  absl::optional<std::string> mid;
  absl::optional<std::string> remote_id;
  absl::optional<uint32_t> packets_received;
  absl::optional<uint64_t> packets_discarded;
  absl::optional<uint64_t> fec_packets_received;
  absl::optional<uint64_t> fec_bytes_received;
  absl::optional<uint64_t> fec_packets_discarded;

  absl::optional<uint32_t> fec_ssrc;
  absl::optional<uint64_t> bytes_received;
  absl::optional<uint64_t> header_bytes_received;

  absl::optional<uint64_t> retransmitted_packets_received;
  absl::optional<uint64_t> retransmitted_bytes_received;
  absl::optional<uint32_t> rtx_ssrc;

  absl::optional<double> last_packet_received_timestamp;
  absl::optional<double> jitter_buffer_delay;
  absl::optional<double> jitter_buffer_target_delay;
  absl::optional<double> jitter_buffer_minimum_delay;
  absl::optional<uint64_t> jitter_buffer_emitted_count;
  absl::optional<uint64_t> total_samples_received;
  absl::optional<uint64_t> concealed_samples;
  absl::optional<uint64_t> silent_concealed_samples;
  absl::optional<uint64_t> concealment_events;
  absl::optional<uint64_t> inserted_samples_for_deceleration;
  absl::optional<uint64_t> removed_samples_for_acceleration;
  absl::optional<double> audio_level;
  absl::optional<double> total_audio_energy;
  absl::optional<double> total_samples_duration;

  absl::optional<uint32_t> frames_received;
  absl::optional<uint32_t> frame_width;
  absl::optional<uint32_t> frame_height;
  absl::optional<double> frames_per_second;
  absl::optional<uint32_t> frames_decoded;
  absl::optional<uint32_t> key_frames_decoded;
  absl::optional<uint32_t> frames_dropped;
  absl::optional<double> total_decode_time;
  absl::optional<double> total_processing_delay;
  absl::optional<double> total_assembly_time;
  absl::optional<uint32_t> frames_assembled_from_multiple_packets;

  absl::optional<double> total_inter_frame_delay;
  absl::optional<double> total_squared_inter_frame_delay;
  absl::optional<uint32_t> pause_count;
  absl::optional<double> total_pauses_duration;
  absl::optional<uint32_t> freeze_count;
  absl::optional<double> total_freezes_duration;

  absl::optional<std::string> content_type;

  absl::optional<double> estimated_playout_timestamp;

  absl::optional<std::string> decoder_implementation;

  absl::optional<uint32_t> fir_count;
  absl::optional<uint32_t> pli_count;
  absl::optional<uint32_t> nack_count;
  absl::optional<uint64_t> qp_sum;

  absl::optional<std::string> goog_timing_frame_info;

  absl::optional<bool> power_efficient_decoder;

  absl::optional<uint64_t> jitter_buffer_flushes;
  absl::optional<uint64_t> delayed_packet_outage_samples;
  absl::optional<double> relative_packet_arrival_delay;
  absl::optional<uint32_t> interruption_count;
  absl::optional<double> total_interruption_duration;
  absl::optional<double> min_playout_delay;
};

class RTC_EXPORT RTCOutboundRtpStreamStats final
    : public RTCSentRtpStreamStats {
 public:
  WEBRTC_RTCSTATS_DECL();
  RTCOutboundRtpStreamStats(std::string id, Timestamp timestamp);
  ~RTCOutboundRtpStreamStats() override;

  absl::optional<std::string> media_source_id;
  absl::optional<std::string> remote_id;
  absl::optional<std::string> mid;
  absl::optional<std::string> rid;
  absl::optional<uint64_t> retransmitted_packets_sent;
  absl::optional<uint64_t> header_bytes_sent;
  absl::optional<uint64_t> retransmitted_bytes_sent;
  absl::optional<double> target_bitrate;
  absl::optional<uint32_t> frames_encoded;
  absl::optional<uint32_t> key_frames_encoded;
  absl::optional<double> total_encode_time;
  absl::optional<uint64_t> total_encoded_bytes_target;
  absl::optional<uint32_t> frame_width;
  absl::optional<uint32_t> frame_height;
  absl::optional<double> frames_per_second;
  absl::optional<uint32_t> frames_sent;
  absl::optional<uint32_t> huge_frames_sent;
  absl::optional<double> total_packet_send_delay;
  absl::optional<std::string> quality_limitation_reason;
  absl::optional<std::map<std::string, double>> quality_limitation_durations;

  absl::optional<uint32_t> quality_limitation_resolution_changes;

  absl::optional<std::string> content_type;

  absl::optional<std::string> encoder_implementation;

  absl::optional<uint32_t> fir_count;
  absl::optional<uint32_t> pli_count;
  absl::optional<uint32_t> nack_count;
  absl::optional<uint64_t> qp_sum;
  absl::optional<bool> active;

  absl::optional<bool> power_efficient_encoder;
  absl::optional<std::string> scalability_mode;

  absl::optional<uint32_t> rtx_ssrc;
};

class RTC_EXPORT RTCRemoteInboundRtpStreamStats final
    : public RTCReceivedRtpStreamStats {
 public:
  WEBRTC_RTCSTATS_DECL();
  RTCRemoteInboundRtpStreamStats(std::string id, Timestamp timestamp);
  ~RTCRemoteInboundRtpStreamStats() override;

  absl::optional<std::string> local_id;
  absl::optional<double> round_trip_time;
  absl::optional<double> fraction_lost;
  absl::optional<double> total_round_trip_time;
  absl::optional<int32_t> round_trip_time_measurements;
};

class RTC_EXPORT RTCRemoteOutboundRtpStreamStats final
    : public RTCSentRtpStreamStats {
 public:
  WEBRTC_RTCSTATS_DECL();
  RTCRemoteOutboundRtpStreamStats(std::string id, Timestamp timestamp);
  ~RTCRemoteOutboundRtpStreamStats() override;

  absl::optional<std::string> local_id;
  absl::optional<double> remote_timestamp;
  absl::optional<uint64_t> reports_sent;
  absl::optional<double> round_trip_time;
  absl::optional<uint64_t> round_trip_time_measurements;
  absl::optional<double> total_round_trip_time;
};

class RTC_EXPORT RTCMediaSourceStats : public RTCStats {
 public:
  WEBRTC_RTCSTATS_DECL();
  ~RTCMediaSourceStats() override;

  absl::optional<std::string> track_identifier;
  absl::optional<std::string> kind;

 protected:
  RTCMediaSourceStats(std::string id, Timestamp timestamp);
};

class RTC_EXPORT RTCAudioSourceStats final : public RTCMediaSourceStats {
 public:
  WEBRTC_RTCSTATS_DECL();
  RTCAudioSourceStats(std::string id, Timestamp timestamp);
  ~RTCAudioSourceStats() override;

  absl::optional<double> audio_level;
  absl::optional<double> total_audio_energy;
  absl::optional<double> total_samples_duration;
  absl::optional<double> echo_return_loss;
  absl::optional<double> echo_return_loss_enhancement;
};

class RTC_EXPORT RTCVideoSourceStats final : public RTCMediaSourceStats {
 public:
  WEBRTC_RTCSTATS_DECL();
  RTCVideoSourceStats(std::string id, Timestamp timestamp);
  ~RTCVideoSourceStats() override;

  absl::optional<uint32_t> width;
  absl::optional<uint32_t> height;
  absl::optional<uint32_t> frames;
  absl::optional<double> frames_per_second;
};

class RTC_EXPORT RTCTransportStats final : public RTCStats {
 public:
  WEBRTC_RTCSTATS_DECL();
  RTCTransportStats(std::string id, Timestamp timestamp);
  ~RTCTransportStats() override;

  absl::optional<uint64_t> bytes_sent;
  absl::optional<uint64_t> packets_sent;
  absl::optional<uint64_t> bytes_received;
  absl::optional<uint64_t> packets_received;
  absl::optional<std::string> rtcp_transport_stats_id;
  absl::optional<std::string> dtls_state;
  absl::optional<std::string> selected_candidate_pair_id;
  absl::optional<std::string> local_certificate_id;
  absl::optional<std::string> remote_certificate_id;
  absl::optional<std::string> tls_version;
  absl::optional<std::string> dtls_cipher;
  absl::optional<std::string> dtls_role;
  absl::optional<std::string> srtp_cipher;
  absl::optional<uint32_t> selected_candidate_pair_changes;
  absl::optional<std::string> ice_role;
  absl::optional<std::string> ice_local_username_fragment;
  absl::optional<std::string> ice_state;
};

class RTC_EXPORT RTCAudioPlayoutStats final : public RTCStats {
 public:
  WEBRTC_RTCSTATS_DECL();
  RTCAudioPlayoutStats(const std::string& id, Timestamp timestamp);
  ~RTCAudioPlayoutStats() override;

  absl::optional<std::string> kind;
  absl::optional<double> synthesized_samples_duration;
  absl::optional<uint64_t> synthesized_samples_events;
  absl::optional<double> total_samples_duration;
  absl::optional<double> total_playout_delay;
  absl::optional<uint64_t> total_samples_count;
};

}

#endif
