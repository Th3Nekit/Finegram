/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MEDIA_BASE_MEDIA_CONSTANTS_H_
#define MEDIA_BASE_MEDIA_CONSTANTS_H_

#include <stddef.h>

#include "rtc_base/system/rtc_export.h"

namespace cricket {

extern const int kVideoCodecClockrate;

extern const int kVideoMtu;
extern const int kVideoRtpSendBufferSize;
extern const int kVideoRtpRecvBufferSize;

extern const float kHighSystemCpuThreshold;
extern const float kLowSystemCpuThreshold;
extern const float kProcessCpuThreshold;

extern const char kRedCodecName[];
extern const char kUlpfecCodecName[];
extern const char kFlexfecCodecName[];
extern const char kMultiplexCodecName[];

extern const char kFlexfecFmtpRepairWindow[];

extern const char kRtxCodecName[];
extern const char kCodecParamRtxTime[];
extern const char kCodecParamAssociatedPayloadType[];

extern const char kCodecParamAssociatedCodecName[];
extern const char kCodecParamNotInNameValueFormat[];

extern const char kOpusCodecName[];
extern const char kL16CodecName[];
extern const char kG722CodecName[];
extern const char kIlbcCodecName[];
extern const char kPcmuCodecName[];
extern const char kPcmaCodecName[];
extern const char kCnCodecName[];
extern const char kDtmfCodecName[];

extern const char kCodecParamPTime[];
extern const char kCodecParamMaxPTime[];

extern const char kCodecParamMinPTime[];
extern const char kCodecParamSPropStereo[];
extern const char kCodecParamStereo[];
extern const char kCodecParamUseInbandFec[];
extern const char kCodecParamUseDtx[];
extern const char kCodecParamMaxAverageBitrate[];
extern const char kCodecParamMaxPlaybackRate[];

extern const char kParamValueTrue[];

extern const char kParamValueEmpty[];

extern const int kOpusDefaultMaxPTime;
extern const int kOpusDefaultPTime;
extern const int kOpusDefaultMinPTime;
extern const int kOpusDefaultSPropStereo;
extern const int kOpusDefaultStereo;
extern const int kOpusDefaultUseInbandFec;
extern const int kOpusDefaultUseDtx;
extern const int kOpusDefaultMaxPlaybackRate;

extern const int kPreferredMaxPTime;
extern const int kPreferredMinPTime;
extern const int kPreferredSPropStereo;
extern const int kPreferredStereo;
extern const int kPreferredUseInbandFec;

extern const char kPacketizationParamRaw[];

extern const char kRtcpFbParamLntf[];

extern const char kRtcpFbParamNack[];
extern const char kRtcpFbNackParamPli[];

extern const char kRtcpFbParamRemb[];

extern const char kRtcpFbParamTransportCc[];

extern const char kRtcpFbParamCcm[];
extern const char kRtcpFbCcmParamFir[];

extern const char kRtcpFbParamRrtr[];

extern const char kCodecParamMaxBitrate[];
extern const char kCodecParamMinBitrate[];
extern const char kCodecParamStartBitrate[];
extern const char kCodecParamMaxQuantization[];

extern const char kComfortNoiseCodecName[];

RTC_EXPORT extern const char kVp8CodecName[];
RTC_EXPORT extern const char kVp9CodecName[];
RTC_EXPORT extern const char kAv1CodecName[];
RTC_EXPORT extern const char kH264CodecName[];
RTC_EXPORT extern const char kH265CodecName[];

RTC_EXPORT extern const char kH264FmtpProfileLevelId[];
RTC_EXPORT extern const char kH264FmtpLevelAsymmetryAllowed[];
RTC_EXPORT extern const char kH264FmtpPacketizationMode[];
extern const char kH264FmtpSpropParameterSets[];
extern const char kH264FmtpSpsPpsIdrInKeyframe[];
extern const char kH264ProfileLevelConstrainedBaseline[];
extern const char kH264ProfileLevelConstrainedHigh[];

RTC_EXPORT extern const char kH265FmtpProfileSpace[];
RTC_EXPORT extern const char kH265FmtpTierFlag[];
RTC_EXPORT extern const char kH265FmtpProfileId[];
RTC_EXPORT extern const char kH265FmtpLevelId[];
RTC_EXPORT extern const char kH265FmtpProfileCompatibilityIndicator[];
RTC_EXPORT extern const char kH265FmtpInteropConstraints[];
RTC_EXPORT extern const char kH265FmtpTxMode[];

extern const char kVP9ProfileId[];

extern const char kAv1FmtpProfile[];
extern const char kAv1FmtpLevelIdx[];
extern const char kAv1FmtpTier[];

extern const int kDefaultVideoMaxFramerate;
extern const int kDefaultVideoMaxQpVpx;
extern const int kDefaultVideoMaxQpH26x;

extern const size_t kConferenceMaxNumSpatialLayers;
extern const size_t kConferenceMaxNumTemporalLayers;
extern const size_t kConferenceDefaultNumTemporalLayers;

extern const char kApplicationSpecificBandwidth[];
extern const char kTransportSpecificBandwidth[];
}

#endif
