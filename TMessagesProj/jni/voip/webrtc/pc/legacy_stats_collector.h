/*
 *  Copyright 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// This file contains a class used for gathering statistics from an ongoing
// libjingle PeerConnection.

#ifndef PC_LEGACY_STATS_COLLECTOR_H_
#define PC_LEGACY_STATS_COLLECTOR_H_

#include <stdint.h>

#include <algorithm>
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "absl/types/optional.h"
#include "api/candidate.h"
#include "api/field_trials_view.h"
#include "api/legacy_stats_types.h"
#include "api/media_stream_interface.h"
#include "api/peer_connection_interface.h"
#include "api/scoped_refptr.h"
#include "p2p/base/connection_info.h"
#include "p2p/base/port.h"
#include "pc/legacy_stats_collector_interface.h"
#include "pc/peer_connection_internal.h"
#include "pc/rtp_transceiver.h"
#include "pc/transport_stats.h"
#include "rtc_base/network_constants.h"
#include "rtc_base/ssl_certificate.h"
#include "rtc_base/thread_annotations.h"

namespace webrtc {

const char* IceCandidateTypeToStatsType(const cricket::Candidate& candidate);

const char* AdapterTypeToStatsType(rtc::AdapterType type);

typedef std::map<std::string, StatsReport*> TrackIdMap;

class LegacyStatsCollector : public LegacyStatsCollectorInterface {
 public:

  explicit LegacyStatsCollector(PeerConnectionInternal* pc);
  virtual ~LegacyStatsCollector();

  void AddStream(MediaStreamInterface* stream);
  void AddTrack(MediaStreamTrackInterface* track);

  void AddLocalAudioTrack(AudioTrackInterface* audio_track,
                          uint32_t ssrc) override;

  void RemoveLocalAudioTrack(AudioTrackInterface* audio_track,
                             uint32_t ssrc) override;

  void UpdateStats(PeerConnectionInterface::StatsOutputLevel level);

  void GetStats(MediaStreamTrackInterface* track,
                StatsReports* reports) override;

  StatsReport* PrepareReport(bool local,
                             uint32_t ssrc,
                             const std::string& track_id,
                             const StatsReport::Id& transport_id,
                             StatsReport::Direction direction);

  StatsReport* PrepareADMReport();

  bool IsValidTrack(const std::string& track_id);

  void InvalidateCache();

  bool UseStandardBytesStats() const { return use_standard_bytes_stats_; }

 private:
  friend class LegacyStatsCollectorTest;

  struct TransportStats {
    TransportStats() = default;
    TransportStats(std::string transport_name,
                   cricket::TransportStats transport_stats)
        : name(std::move(transport_name)), stats(std::move(transport_stats)) {}
    TransportStats(TransportStats&&) = default;
    TransportStats(const TransportStats&) = delete;

    std::string name;
    cricket::TransportStats stats;
    std::unique_ptr<rtc::SSLCertificateStats> local_cert_stats;
    std::unique_ptr<rtc::SSLCertificateStats> remote_cert_stats;
  };

  struct SessionStats {
    SessionStats() = default;
    SessionStats(SessionStats&&) = default;
    SessionStats(const SessionStats&) = delete;

    SessionStats& operator=(SessionStats&&) = default;
    SessionStats& operator=(SessionStats&) = delete;

    cricket::CandidateStatsList candidate_stats;
    std::vector<TransportStats> transport_stats;
    std::map<std::string, std::string> transport_names_by_mid;
  };

  virtual double GetTimeNow();

  bool CopySelectedReports(const std::string& selector, StatsReports* reports);

  StatsReport* AddCandidateReport(
      const cricket::CandidateStats& candidate_stats,
      bool local);

  StatsReport* AddCertificateReports(
      std::unique_ptr<rtc::SSLCertificateStats> cert_stats);

  StatsReport* AddConnectionInfoReport(const std::string& content_name,
                                       int component,
                                       int connection_id,
                                       const StatsReport::Id& channel_report_id,
                                       const cricket::ConnectionInfo& info);

  void ExtractDataInfo_n(StatsCollection* reports);

  std::map<std::string, std::string> ExtractSessionAndDataInfo();

  void ExtractBweInfo();
  void ExtractMediaInfo(
      const std::map<std::string, std::string>& transport_names_by_mid);
  void ExtractSenderInfo();
  StatsReport* GetReport(const StatsReport::StatsType& type,
                         const std::string& id,
                         StatsReport::Direction direction);

  void UpdateStatsFromExistingLocalAudioTracks(bool has_remote_tracks);
  void UpdateReportFromAudioTrack(AudioTrackInterface* track,
                                  StatsReport* report,
                                  bool has_remote_tracks);

  void UpdateTrackReports();

  SessionStats ExtractSessionInfo_n(
      const std::vector<rtc::scoped_refptr<
          RtpTransceiverProxyWithInternal<RtpTransceiver>>>& transceivers,
      absl::optional<std::string> sctp_transport_name,
      absl::optional<std::string> sctp_mid);
  void ExtractSessionInfo_s(SessionStats& session_stats);

  StatsCollection reports_;
  TrackIdMap track_ids_;

  PeerConnectionInternal* const pc_;
  int64_t cache_timestamp_ms_ RTC_GUARDED_BY(pc_->signaling_thread()) = 0;
  double stats_gathering_started_;
  const bool use_standard_bytes_stats_;

  typedef std::vector<std::pair<AudioTrackInterface*, uint32_t>>
      LocalAudioTrackVector;
  LocalAudioTrackVector local_audio_tracks_;
};

}

#endif
