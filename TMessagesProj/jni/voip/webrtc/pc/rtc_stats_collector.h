/*
 *  Copyright 2016 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef PC_RTC_STATS_COLLECTOR_H_
#define PC_RTC_STATS_COLLECTOR_H_

#include <stdint.h>

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "absl/types/optional.h"
#include "api/data_channel_interface.h"
#include "api/media_types.h"
#include "api/scoped_refptr.h"
#include "api/stats/rtc_stats_collector_callback.h"
#include "api/stats/rtc_stats_report.h"
#include "api/stats/rtcstats_objects.h"
#include "call/call.h"
#include "media/base/media_channel.h"
#include "modules/audio_device/include/audio_device.h"
#include "pc/data_channel_utils.h"
#include "pc/peer_connection_internal.h"
#include "pc/rtp_receiver.h"
#include "pc/rtp_sender.h"
#include "pc/rtp_transceiver.h"
#include "pc/sctp_data_channel.h"
#include "pc/track_media_info_map.h"
#include "pc/transport_stats.h"
#include "rtc_base/checks.h"
#include "rtc_base/containers/flat_set.h"
#include "rtc_base/event.h"
#include "rtc_base/ref_count.h"
#include "rtc_base/ssl_certificate.h"
#include "rtc_base/ssl_identity.h"
#include "rtc_base/synchronization/mutex.h"
#include "rtc_base/thread.h"
#include "rtc_base/time_utils.h"

namespace webrtc {

class RtpSenderInternal;
class RtpReceiverInternal;

class RTCStatsCollector : public rtc::RefCountInterface {
 public:
  static rtc::scoped_refptr<RTCStatsCollector> Create(
      PeerConnectionInternal* pc,
      int64_t cache_lifetime_us = 50 * rtc::kNumMicrosecsPerMillisec);

  void GetStatsReport(rtc::scoped_refptr<RTCStatsCollectorCallback> callback);

  void GetStatsReport(rtc::scoped_refptr<RtpSenderInternal> selector,
                      rtc::scoped_refptr<RTCStatsCollectorCallback> callback);

  void GetStatsReport(rtc::scoped_refptr<RtpReceiverInternal> selector,
                      rtc::scoped_refptr<RTCStatsCollectorCallback> callback);

  void ClearCachedStatsReport();

  void WaitForPendingRequest();

  void OnSctpDataChannelStateChanged(int channel_id,
                                     DataChannelInterface::DataState state);

 protected:
  RTCStatsCollector(PeerConnectionInternal* pc, int64_t cache_lifetime_us);
  ~RTCStatsCollector();

  struct CertificateStatsPair {
    std::unique_ptr<rtc::SSLCertificateStats> local;
    std::unique_ptr<rtc::SSLCertificateStats> remote;

    CertificateStatsPair Copy() const;
  };

  virtual void ProducePartialResultsOnSignalingThreadImpl(
      Timestamp timestamp,
      RTCStatsReport* partial_report);
  virtual void ProducePartialResultsOnNetworkThreadImpl(
      Timestamp timestamp,
      const std::map<std::string, cricket::TransportStats>&
          transport_stats_by_name,
      const std::map<std::string, CertificateStatsPair>& transport_cert_stats,
      RTCStatsReport* partial_report);

 private:
  class RequestInfo {
   public:
    enum class FilterMode { kAll, kSenderSelector, kReceiverSelector };

    explicit RequestInfo(
        rtc::scoped_refptr<RTCStatsCollectorCallback> callback);

    RequestInfo(rtc::scoped_refptr<RtpSenderInternal> selector,
                rtc::scoped_refptr<RTCStatsCollectorCallback> callback);

    RequestInfo(rtc::scoped_refptr<RtpReceiverInternal> selector,
                rtc::scoped_refptr<RTCStatsCollectorCallback> callback);

    FilterMode filter_mode() const { return filter_mode_; }
    rtc::scoped_refptr<RTCStatsCollectorCallback> callback() const {
      return callback_;
    }
    rtc::scoped_refptr<RtpSenderInternal> sender_selector() const {
      RTC_DCHECK(filter_mode_ == FilterMode::kSenderSelector);
      return sender_selector_;
    }
    rtc::scoped_refptr<RtpReceiverInternal> receiver_selector() const {
      RTC_DCHECK(filter_mode_ == FilterMode::kReceiverSelector);
      return receiver_selector_;
    }

   private:
    RequestInfo(FilterMode filter_mode,
                rtc::scoped_refptr<RTCStatsCollectorCallback> callback,
                rtc::scoped_refptr<RtpSenderInternal> sender_selector,
                rtc::scoped_refptr<RtpReceiverInternal> receiver_selector);

    FilterMode filter_mode_;
    rtc::scoped_refptr<RTCStatsCollectorCallback> callback_;
    rtc::scoped_refptr<RtpSenderInternal> sender_selector_;
    rtc::scoped_refptr<RtpReceiverInternal> receiver_selector_;
  };

  void GetStatsReportInternal(RequestInfo request);

  struct RtpTransceiverStatsInfo {
    rtc::scoped_refptr<RtpTransceiver> transceiver;
    cricket::MediaType media_type;
    absl::optional<std::string> mid;
    absl::optional<std::string> transport_name;
    TrackMediaInfoMap track_media_info_map;
    absl::optional<RtpTransceiverDirection> current_direction;
  };

  void DeliverCachedReport(
      rtc::scoped_refptr<const RTCStatsReport> cached_report,
      std::vector<RequestInfo> requests);

  void ProduceCertificateStats_n(
      Timestamp timestamp,
      const std::map<std::string, CertificateStatsPair>& transport_cert_stats,
      RTCStatsReport* report) const;

  void ProduceDataChannelStats_n(Timestamp timestamp,
                                 RTCStatsReport* report) const;

  void ProduceIceCandidateAndPairStats_n(
      Timestamp timestamp,
      const std::map<std::string, cricket::TransportStats>&
          transport_stats_by_name,
      const Call::Stats& call_stats,
      RTCStatsReport* report) const;

  void ProduceMediaSourceStats_s(Timestamp timestamp,
                                 RTCStatsReport* report) const;

  void ProducePeerConnectionStats_s(Timestamp timestamp,
                                    RTCStatsReport* report) const;

  void ProduceAudioPlayoutStats_s(Timestamp timestamp,
                                  RTCStatsReport* report) const;

  void ProduceRTPStreamStats_n(
      Timestamp timestamp,
      const std::vector<RtpTransceiverStatsInfo>& transceiver_stats_infos,
      RTCStatsReport* report) const;
  void ProduceAudioRTPStreamStats_n(Timestamp timestamp,
                                    const RtpTransceiverStatsInfo& stats,
                                    RTCStatsReport* report) const;
  void ProduceVideoRTPStreamStats_n(Timestamp timestamp,
                                    const RtpTransceiverStatsInfo& stats,
                                    RTCStatsReport* report) const;

  void ProduceTransportStats_n(
      Timestamp timestamp,
      const std::map<std::string, cricket::TransportStats>&
          transport_stats_by_name,
      const std::map<std::string, CertificateStatsPair>& transport_cert_stats,
      RTCStatsReport* report) const;

  std::map<std::string, CertificateStatsPair>
  PrepareTransportCertificateStats_n(
      const std::map<std::string, cricket::TransportStats>&
          transport_stats_by_name);

  void PrepareTransceiverStatsInfosAndCallStats_s_w_n();

  void ProducePartialResultsOnSignalingThread(Timestamp timestamp);
  void ProducePartialResultsOnNetworkThread(
      Timestamp timestamp,
      absl::optional<std::string> sctp_transport_name);

  void MergeNetworkReport_s();

  rtc::scoped_refptr<RTCStatsReport> CreateReportFilteredBySelector(
      bool filter_by_sender_selector,
      rtc::scoped_refptr<const RTCStatsReport> report,
      rtc::scoped_refptr<RtpSenderInternal> sender_selector,
      rtc::scoped_refptr<RtpReceiverInternal> receiver_selector);

  PeerConnectionInternal* const pc_;
  rtc::Thread* const signaling_thread_;
  rtc::Thread* const worker_thread_;
  rtc::Thread* const network_thread_;

  int num_pending_partial_reports_;
  int64_t partial_report_timestamp_us_;

  rtc::scoped_refptr<RTCStatsReport> partial_report_;
  std::vector<RequestInfo> requests_;

  rtc::scoped_refptr<RTCStatsReport> network_report_;

  rtc::Event network_report_event_;

  std::vector<RtpTransceiverStatsInfo> transceiver_stats_infos_;

  Mutex cached_certificates_mutex_;
  std::map<std::string, CertificateStatsPair> cached_certificates_by_transport_
      RTC_GUARDED_BY(cached_certificates_mutex_);

  Call::Stats call_stats_;

  absl::optional<AudioDeviceModule::Stats> audio_device_stats_;

  int64_t cache_timestamp_us_;
  int64_t cache_lifetime_us_;
  rtc::scoped_refptr<const RTCStatsReport> cached_report_;

  struct InternalRecord {
    InternalRecord() : data_channels_opened(0), data_channels_closed(0) {}

    uint32_t data_channels_opened;
    uint32_t data_channels_closed;

    flat_set<int> opened_data_channels;
  };
  InternalRecord internal_record_;
};

}

#endif
