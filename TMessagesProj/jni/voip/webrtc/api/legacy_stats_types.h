/*
 *  Copyright 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// This file contains structures used for retrieving statistics from an ongoing
// libjingle session.

#ifndef API_LEGACY_STATS_TYPES_H_
#define API_LEGACY_STATS_TYPES_H_

#include <algorithm>
#include <list>
#include <map>
#include <string>
#include <vector>

#include "api/ref_count.h"
#include "api/scoped_refptr.h"
#include "api/sequence_checker.h"
#include "rtc_base/system/rtc_export.h"

namespace webrtc {

class RTC_EXPORT StatsReport {
 public:

  enum Direction {
    kSend = 0,
    kReceive,
  };

  enum StatsType {

    kStatsReportTypeSession,

    kStatsReportTypeTransport,

    kStatsReportTypeComponent,

    kStatsReportTypeCandidatePair,

    kStatsReportTypeBwe,

    kStatsReportTypeSsrc,

    kStatsReportTypeRemoteSsrc,

    kStatsReportTypeTrack,

    kStatsReportTypeIceLocalCandidate,
    kStatsReportTypeIceRemoteCandidate,

    kStatsReportTypeCertificate,

    kStatsReportTypeDataChannel,
  };

  enum StatsValueName {
    kStatsValueNameActiveConnection,
    kStatsValueNameAecDivergentFilterFraction,
    kStatsValueNameAudioInputLevel,
    kStatsValueNameAudioOutputLevel,
    kStatsValueNameBytesReceived,
    kStatsValueNameBytesSent,
    kStatsValueNameCodecImplementationName,
    kStatsValueNameConcealedSamples,
    kStatsValueNameConcealmentEvents,
    kStatsValueNameDataChannelId,
    kStatsValueNameFramesDecoded,
    kStatsValueNameFramesEncoded,
    kStatsValueNameJitterBufferDelay,
    kStatsValueNameMediaType,
    kStatsValueNamePacketsLost,
    kStatsValueNamePacketsReceived,
    kStatsValueNamePacketsSent,
    kStatsValueNameProtocol,
    kStatsValueNameQpSum,
    kStatsValueNameReceiving,
    kStatsValueNameSelectedCandidatePairId,
    kStatsValueNameSsrc,
    kStatsValueNameState,
    kStatsValueNameTotalAudioEnergy,
    kStatsValueNameTotalSamplesDuration,
    kStatsValueNameTotalSamplesReceived,
    kStatsValueNameTransportId,
    kStatsValueNameSentPingRequestsTotal,
    kStatsValueNameSentPingRequestsBeforeFirstResponse,
    kStatsValueNameSentPingResponses,
    kStatsValueNameRecvPingRequests,
    kStatsValueNameRecvPingResponses,
    kStatsValueNameSentStunKeepaliveRequests,
    kStatsValueNameRecvStunKeepaliveResponses,
    kStatsValueNameStunKeepaliveRttTotal,
    kStatsValueNameStunKeepaliveRttSquaredTotal,

    kStatsValueNameAccelerateRate,
    kStatsValueNameActualEncBitrate,
    kStatsValueNameAdaptationChanges,
    kStatsValueNameAvailableReceiveBandwidth,
    kStatsValueNameAvailableSendBandwidth,
    kStatsValueNameAvgEncodeMs,
    kStatsValueNameBandwidthLimitedResolution,
    kStatsValueNameBucketDelay,
    kStatsValueNameCaptureStartNtpTimeMs,
    kStatsValueNameCandidateIPAddress,
    kStatsValueNameCandidateNetworkType,
    kStatsValueNameCandidatePortNumber,
    kStatsValueNameCandidatePriority,
    kStatsValueNameCandidateTransportType,
    kStatsValueNameCandidateType,
    kStatsValueNameChannelId,
    kStatsValueNameCodecName,
    kStatsValueNameComponent,
    kStatsValueNameContentName,
    kStatsValueNameContentType,
    kStatsValueNameCpuLimitedResolution,
    kStatsValueNameCurrentDelayMs,
    kStatsValueNameDecodeMs,
    kStatsValueNameDecodingCNG,
    kStatsValueNameDecodingCTN,
    kStatsValueNameDecodingCTSG,
    kStatsValueNameDecodingMutedOutput,
    kStatsValueNameDecodingNormal,
    kStatsValueNameDecodingPLC,
    kStatsValueNameDecodingCodecPLC,
    kStatsValueNameDecodingPLCCNG,
    kStatsValueNameDer,
    kStatsValueNameDtlsCipher,
    kStatsValueNameEchoDelayMedian,
    kStatsValueNameEchoDelayStdDev,
    kStatsValueNameEchoReturnLoss,
    kStatsValueNameEchoReturnLossEnhancement,
    kStatsValueNameEncodeUsagePercent,
    kStatsValueNameExpandRate,
    kStatsValueNameFingerprint,
    kStatsValueNameFingerprintAlgorithm,
    kStatsValueNameFirsReceived,
    kStatsValueNameFirsSent,
    kStatsValueNameFirstFrameReceivedToDecodedMs,
    kStatsValueNameFrameHeightInput,
    kStatsValueNameFrameHeightReceived,
    kStatsValueNameFrameHeightSent,
    kStatsValueNameFrameRateDecoded,
    kStatsValueNameFrameRateInput,
    kStatsValueNameFrameRateOutput,
    kStatsValueNameFrameRateReceived,
    kStatsValueNameFrameRateSent,
    kStatsValueNameFrameWidthInput,
    kStatsValueNameFrameWidthReceived,
    kStatsValueNameFrameWidthSent,
    kStatsValueNameHasEnteredLowResolution,
    kStatsValueNameHugeFramesSent,
    kStatsValueNameInitiator,
    kStatsValueNameInterframeDelayMaxMs,
    kStatsValueNameIssuerId,
    kStatsValueNameJitterBufferMs,
    kStatsValueNameJitterReceived,
    kStatsValueNameLabel,
    kStatsValueNameLocalAddress,
    kStatsValueNameLocalCandidateId,
    kStatsValueNameLocalCandidateType,
    kStatsValueNameLocalCertificateId,
    kStatsValueNameMaxDecodeMs,
    kStatsValueNameMinPlayoutDelayMs,
    kStatsValueNameNacksReceived,
    kStatsValueNameNacksSent,
    kStatsValueNamePlisReceived,
    kStatsValueNamePlisSent,
    kStatsValueNamePreemptiveExpandRate,
    kStatsValueNamePreferredJitterBufferMs,
    kStatsValueNameRemoteAddress,
    kStatsValueNameRemoteCandidateId,
    kStatsValueNameRemoteCandidateType,
    kStatsValueNameRemoteCertificateId,
    kStatsValueNameRenderDelayMs,
    kStatsValueNameResidualEchoLikelihood,
    kStatsValueNameResidualEchoLikelihoodRecentMax,
    kStatsValueNameAnaBitrateActionCounter,
    kStatsValueNameAnaChannelActionCounter,
    kStatsValueNameAnaDtxActionCounter,
    kStatsValueNameAnaFecActionCounter,
    kStatsValueNameAnaFrameLengthIncreaseCounter,
    kStatsValueNameAnaFrameLengthDecreaseCounter,
    kStatsValueNameAnaUplinkPacketLossFraction,
    kStatsValueNameRetransmitBitrate,
    kStatsValueNameRtt,
    kStatsValueNameSecondaryDecodedRate,
    kStatsValueNameSecondaryDiscardedRate,
    kStatsValueNameSendPacketsDiscarded,
    kStatsValueNameSpeechExpandRate,
    kStatsValueNameSrtpCipher,
    kStatsValueNameTargetDelayMs,
    kStatsValueNameTargetEncBitrate,
    kStatsValueNameTimingFrameInfo,
    kStatsValueNameTrackId,
    kStatsValueNameTransmitBitrate,
    kStatsValueNameTransportType,
    kStatsValueNameWritable,
    kStatsValueNameAudioDeviceUnderrunCounter,
    kStatsValueNameLocalCandidateRelayProtocol,
  };

  class RTC_EXPORT IdBase : public webrtc::RefCountInterface {
   public:
    ~IdBase() override;
    StatsType type() const;

    bool Equals(const rtc::scoped_refptr<IdBase>& other) const {
      return Equals(*other.get());
    }

    virtual std::string ToString() const = 0;

   protected:

    virtual bool Equals(const IdBase& other) const;

    explicit IdBase(StatsType type);
    const StatsType type_;

    static const char kSeparator = '_';
  };

  typedef rtc::scoped_refptr<IdBase> Id;

  struct RTC_EXPORT Value {
    enum Type {
      kInt,
      kInt64,
      kFloat,
      kString,
      kStaticString,
      kBool,
      kId,
    };

    Value(StatsValueName name, int64_t value, Type int_type);
    Value(StatsValueName name, float f);
    Value(StatsValueName name, const std::string& value);
    Value(StatsValueName name, const char* value);
    Value(StatsValueName name, bool b);
    Value(StatsValueName name, const Id& value);

    ~Value();

    Value(const Value&) = delete;
    Value& operator=(const Value&) = delete;

    int AddRef() const {
      RTC_DCHECK_RUN_ON(&thread_checker_);
      return ++ref_count_;
    }
    int Release() const {
      RTC_DCHECK_RUN_ON(&thread_checker_);
      int count = --ref_count_;
      if (!count)
        delete this;
      return count;
    }

    bool Equals(const Value& other) const;

    bool operator==(const std::string& value) const;
    bool operator==(const char* value) const;
    bool operator==(int64_t value) const;
    bool operator==(bool value) const;
    bool operator==(float value) const;
    bool operator==(const Id& value) const;

    int int_val() const;
    int64_t int64_val() const;
    float float_val() const;
    const char* static_string_val() const;
    const std::string& string_val() const;
    bool bool_val() const;
    const Id& id_val() const;

    const char* display_name() const;

    std::string ToString() const;

    Type type() const { return type_; }

    const StatsValueName name;

   protected:
#if RTC_DCHECK_IS_ON
    friend class StatsReport;
    void DetachSequenceChecker() { thread_checker_.Detach(); }
    void AttachSequenceChecker() { RTC_DCHECK_RUN_ON(&thread_checker_); }
#endif

   private:
    webrtc::SequenceChecker thread_checker_{webrtc::SequenceChecker::kDetached};
    mutable int ref_count_ RTC_GUARDED_BY(thread_checker_) = 0;

    const Type type_;

    union InternalType {
      int int_;
      int64_t int64_;
      float float_;
      bool bool_;
      std::string* string_;
      const char* static_string_;
      Id* id_;
    } value_;
  };

  typedef rtc::scoped_refptr<Value> ValuePtr;
  typedef std::map<StatsValueName, ValuePtr> Values;

  explicit StatsReport(const Id& id);
  ~StatsReport();

  StatsReport(const StatsReport&) = delete;
  StatsReport& operator=(const StatsReport&) = delete;

  static Id NewBandwidthEstimationId();
  static Id NewTypedId(StatsType type, const std::string& id);
  static Id NewTypedIntId(StatsType type, int id);
  static Id NewIdWithDirection(StatsType type,
                               const std::string& id,
                               Direction direction);
  static Id NewCandidateId(bool local, const std::string& id);
  static Id NewComponentId(const std::string& content_name, int component);
  static Id NewCandidatePairId(const std::string& content_name,
                               int component,
                               int index);

  const Id& id() const { return id_; }
  StatsType type() const { return id_->type(); }
  double timestamp() const { return timestamp_; }
  void set_timestamp(double t) { timestamp_ = t; }
  bool empty() const { return values_.empty(); }
  const Values& values() const { return values_; }

  const char* TypeToString() const;

  void AddString(StatsValueName name, const std::string& value);
  void AddString(StatsValueName name, const char* value);
  void AddInt64(StatsValueName name, int64_t value);
  void AddInt(StatsValueName name, int value);
  void AddFloat(StatsValueName name, float value);
  void AddBoolean(StatsValueName name, bool value);
  void AddId(StatsValueName name, const Id& value);

  const Value* FindValue(StatsValueName name) const;

#if RTC_DCHECK_IS_ON
  void DetachSequenceCheckers() {
    for (auto& v : values_) {
      v.second->DetachSequenceChecker();
    }
  }
  void AttachSequenceCheckers() {
    for (auto& v : values_) {
      v.second->AttachSequenceChecker();
    }
  }
#endif

 private:

  const Id id_;
  double timestamp_;
  Values values_;
};

typedef std::vector<const StatsReport*> StatsReports;

class StatsCollection {
 public:
  StatsCollection();
  ~StatsCollection();

  typedef std::list<StatsReport*> Container;
  typedef Container::iterator iterator;
  typedef Container::const_iterator const_iterator;

  const_iterator begin() const;
  const_iterator end() const;
  size_t size() const;

  StatsReport* InsertNew(const StatsReport::Id& id);
  StatsReport* FindOrAddNew(const StatsReport::Id& id);
  StatsReport* ReplaceOrAddNew(const StatsReport::Id& id);

  Container DetachCollection();
  void MergeCollection(Container collection);

  StatsReport* Find(const StatsReport::Id& id);

 private:
  Container list_;
  webrtc::SequenceChecker thread_checker_{SequenceChecker::kDetached};
};

}

#endif
