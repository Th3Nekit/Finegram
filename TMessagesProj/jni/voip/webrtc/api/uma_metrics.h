/*
 *  Copyright 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// This file contains enums related to Chrome UMA histograms. See
// https://chromium.googlesource.com/chromium/src.git/+/HEAD/tools/metrics/histograms/README.md#requirements
// for requirements when adding or changing metrics.

#ifndef API_UMA_METRICS_H_
#define API_UMA_METRICS_H_

namespace webrtc {

enum PeerConnectionAddressFamilyCounter {
  kPeerConnection_IPv4 = 0,
  kPeerConnection_IPv6 = 1,
  kBestConnections_IPv4 = 2,
  kBestConnections_IPv6 = 3,
  kPeerConnectionAddressFamilyCounter_Max
};

enum PeerConnectionMetricsName {
  kNetworkInterfaces_IPv4 = 0,
  kNetworkInterfaces_IPv6 = 1,
  kTimeToConnect = 2,
  kLocalCandidates_IPv4 = 3,
  kLocalCandidates_IPv6 = 4,
  kPeerConnectionMetricsName_Max
};

enum IceCandidatePairType {

  kIceCandidatePairHostHost = 0,
  kIceCandidatePairHostSrflx = 1,
  kIceCandidatePairHostRelay = 2,
  kIceCandidatePairHostPrflx = 3,
  kIceCandidatePairSrflxHost = 4,
  kIceCandidatePairSrflxSrflx = 5,
  kIceCandidatePairSrflxRelay = 6,
  kIceCandidatePairSrflxPrflx = 7,
  kIceCandidatePairRelayHost = 8,
  kIceCandidatePairRelaySrflx = 9,
  kIceCandidatePairRelayRelay = 10,
  kIceCandidatePairRelayPrflx = 11,
  kIceCandidatePairPrflxHost = 12,
  kIceCandidatePairPrflxSrflx = 13,
  kIceCandidatePairPrflxRelay = 14,

  kIceCandidatePairHostPrivateHostPrivate = 15,
  kIceCandidatePairHostPrivateHostPublic = 16,
  kIceCandidatePairHostPublicHostPrivate = 17,
  kIceCandidatePairHostPublicHostPublic = 18,
  kIceCandidatePairHostNameHostName = 19,
  kIceCandidatePairHostNameHostPrivate = 20,
  kIceCandidatePairHostNameHostPublic = 21,
  kIceCandidatePairHostPrivateHostName = 22,
  kIceCandidatePairHostPublicHostName = 23,
  kIceCandidatePairMax
};

enum KeyExchangeProtocolMedia {
  kEnumCounterKeyProtocolMediaTypeDtlsAudio = 0,
  kEnumCounterKeyProtocolMediaTypeDtlsVideo = 1,
  kEnumCounterKeyProtocolMediaTypeDtlsData = 2,
  kEnumCounterKeyProtocolMediaTypeSdesAudio = 3,
  kEnumCounterKeyProtocolMediaTypeSdesVideo = 4,
  kEnumCounterKeyProtocolMediaTypeSdesData = 5,
  kEnumCounterKeyProtocolMediaTypeMax
};

enum SdpSemanticRequested {
  kSdpSemanticRequestDefault = 0,
  kSdpSemanticRequestPlanB = 1,
  kSdpSemanticRequestUnifiedPlan = 2,
  kSdpSemanticRequestMax
};

enum AddIceCandidateResult {
  kAddIceCandidateSuccess = 0,
  kAddIceCandidateFailClosed = 1,
  kAddIceCandidateFailNoRemoteDescription = 2,
  kAddIceCandidateFailNullCandidate = 3,
  kAddIceCandidateFailNotValid = 4,
  kAddIceCandidateFailNotReady = 5,
  kAddIceCandidateFailInAddition = 6,
  kAddIceCandidateFailNotUsable = 7,
  kAddIceCandidateMax
};

enum BundleUsage {

  kBundleUsageEmpty = 0,

  kBundleUsageNoBundleDatachannelOnly = 1,

  kBundleUsageNoBundleSimple = 2,

  kBundleUsageNoBundleComplex = 3,

  kBundleUsageBundleDatachannelOnly = 4,

  kBundleUsageBundleSimple = 5,

  kBundleUsageBundleComplex = 6,

  kBundleUsageNoBundlePlanB = 7,
  kBundleUsageBundlePlanB = 8,
  kBundleUsageMax
};

enum BundlePolicyUsage {
  kBundlePolicyUsageBalanced = 0,
  kBundlePolicyUsageMaxBundle = 1,
  kBundlePolicyUsageMaxCompat = 2,
  kBundlePolicyUsageMax
};

enum ProvisionalAnswerUsage {
  kProvisionalAnswerNotUsed = 0,
  kProvisionalAnswerLocal = 1,
  kProvisionalAnswerRemote = 2,
  kProvisionalAnswerMax
};

enum RtcpMuxPolicyUsage {
  kRtcpMuxPolicyUsageRequire = 0,
  kRtcpMuxPolicyUsageNegotiate = 1,
  kRtcpMuxPolicyUsageMax
};

}

#endif
