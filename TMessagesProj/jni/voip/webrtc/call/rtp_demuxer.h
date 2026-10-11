/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef CALL_RTP_DEMUXER_H_
#define CALL_RTP_DEMUXER_H_

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/string_view.h"
#include "rtc_base/containers/flat_map.h"
#include "rtc_base/containers/flat_set.h"

namespace webrtc {

class RtpPacketReceived;
class RtpPacketSinkInterface;

class RtpDemuxerCriteria {
 public:
  explicit RtpDemuxerCriteria(absl::string_view mid,
                              absl::string_view rsid = absl::string_view());
  RtpDemuxerCriteria();
  ~RtpDemuxerCriteria();

  bool operator==(const RtpDemuxerCriteria& other) const;
  bool operator!=(const RtpDemuxerCriteria& other) const;

  const std::string& mid() const { return mid_; }

  std::string ToString() const;

  const std::string& rsid() const { return rsid_; }

  const flat_set<uint32_t>& ssrcs() const { return ssrcs_; }

  flat_set<uint32_t>& ssrcs() { return ssrcs_; }

  const flat_set<uint8_t>& payload_types() const { return payload_types_; }

  flat_set<uint8_t>& payload_types() { return payload_types_; }

 private:

  const std::string mid_;
  const std::string rsid_;
  flat_set<uint32_t> ssrcs_;
  flat_set<uint8_t> payload_types_;
};

class RtpDemuxer {
 public:

  static constexpr int kMaxSsrcBindings = 1000;

  static std::string DescribePacket(const RtpPacketReceived& packet);

  explicit RtpDemuxer(bool use_mid = true);
  ~RtpDemuxer();

  RtpDemuxer(const RtpDemuxer&) = delete;
  void operator=(const RtpDemuxer&) = delete;

  bool AddSink(const RtpDemuxerCriteria& criteria,
               RtpPacketSinkInterface* sink);

  bool AddSink(uint32_t ssrc, RtpPacketSinkInterface* sink);

  void AddSink(absl::string_view rsid, RtpPacketSinkInterface* sink);

  bool RemoveSink(const RtpPacketSinkInterface* sink);

  flat_set<uint32_t> GetSsrcsForSink(const RtpPacketSinkInterface* sink) const;

  bool OnRtpPacket(const RtpPacketReceived& packet);

 private:

  bool CriteriaWouldConflict(const RtpDemuxerCriteria& criteria) const;

  RtpPacketSinkInterface* ResolveSink(const RtpPacketReceived& packet);

  RtpPacketSinkInterface* ResolveSinkByMid(absl::string_view mid,
                                           uint32_t ssrc);
  RtpPacketSinkInterface* ResolveSinkByMidRsid(absl::string_view mid,
                                               absl::string_view rsid,
                                               uint32_t ssrc);
  RtpPacketSinkInterface* ResolveSinkByRsid(absl::string_view rsid,
                                            uint32_t ssrc);
  RtpPacketSinkInterface* ResolveSinkByPayloadType(uint8_t payload_type,
                                                   uint32_t ssrc);

  void RefreshKnownMids();

  flat_map<std::string, RtpPacketSinkInterface*> sink_by_mid_;
  flat_map<uint32_t, RtpPacketSinkInterface*> sink_by_ssrc_;
  std::multimap<uint8_t, RtpPacketSinkInterface*> sinks_by_pt_;
  flat_map<std::pair<std::string, std::string>, RtpPacketSinkInterface*>
      sink_by_mid_and_rsid_;
  flat_map<std::string, RtpPacketSinkInterface*> sink_by_rsid_;

  flat_set<std::string> known_mids_;

  flat_map<uint32_t, std::string> mid_by_ssrc_;
  flat_map<uint32_t, std::string> rsid_by_ssrc_;

  void AddSsrcSinkBinding(uint32_t ssrc, RtpPacketSinkInterface* sink);

  const bool use_mid_;
};

}

#endif
