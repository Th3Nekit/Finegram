/*
 *  Copyright 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// This file contains declarations of interfaces that wrap SDP-related
// constructs; session descriptions and ICE candidates. The inner "cricket::"
// objects shouldn't be accessed directly; the intention is that an application
// using the PeerConnection API only creates these objects from strings, and
// them passes them into the PeerConnection.
//
// Though in the future, we're planning to provide an SDP parsing API, with a
// structure more friendly than cricket::SessionDescription.

#ifndef API_JSEP_H_
#define API_JSEP_H_

#include <stddef.h>

#include <memory>
#include <string>
#include <vector>

#include "absl/types/optional.h"
#include "api/ref_count.h"
#include "api/rtc_error.h"
#include "rtc_base/system/rtc_export.h"

namespace cricket {
class Candidate;
class SessionDescription;
}

namespace webrtc {

struct SdpParseError {
 public:

  std::string line;

  std::string description;
};

class RTC_EXPORT IceCandidateInterface {
 public:
  virtual ~IceCandidateInterface() {}

  virtual std::string sdp_mid() const = 0;

  virtual int sdp_mline_index() const = 0;

  virtual const cricket::Candidate& candidate() const = 0;

  virtual std::string server_url() const;

  virtual bool ToString(std::string* out) const = 0;
};

RTC_EXPORT IceCandidateInterface* CreateIceCandidate(const std::string& sdp_mid,
                                                     int sdp_mline_index,
                                                     const std::string& sdp,
                                                     SdpParseError* error);

RTC_EXPORT std::unique_ptr<IceCandidateInterface> CreateIceCandidate(
    const std::string& sdp_mid,
    int sdp_mline_index,
    const cricket::Candidate& candidate);

class IceCandidateCollection {
 public:
  virtual ~IceCandidateCollection() {}
  virtual size_t count() const = 0;

  virtual bool HasCandidate(const IceCandidateInterface* candidate) const = 0;
  virtual const IceCandidateInterface* at(size_t index) const = 0;
};

enum class SdpType {
  kOffer,
  kPrAnswer,

  kAnswer,

  kRollback

};

RTC_EXPORT const char* SdpTypeToString(SdpType type);

absl::optional<SdpType> SdpTypeFromString(const std::string& type_str);

class RTC_EXPORT SessionDescriptionInterface {
 public:

  static const char kOffer[];
  static const char kPrAnswer[];
  static const char kAnswer[];
  static const char kRollback[];

  virtual ~SessionDescriptionInterface() {}

  virtual std::unique_ptr<SessionDescriptionInterface> Clone() const {
    return nullptr;
  }

  virtual cricket::SessionDescription* description() = 0;
  virtual const cricket::SessionDescription* description() const = 0;

  virtual std::string session_id() const = 0;
  virtual std::string session_version() const = 0;

  virtual SdpType GetType() const;

  virtual std::string type() const = 0;

  virtual bool AddCandidate(const IceCandidateInterface* candidate) = 0;

  virtual size_t RemoveCandidates(
      const std::vector<cricket::Candidate>& candidates);

  virtual size_t number_of_mediasections() const = 0;

  virtual const IceCandidateCollection* candidates(
      size_t mediasection_index) const = 0;

  virtual bool ToString(std::string* out) const = 0;
};

RTC_EXPORT SessionDescriptionInterface* CreateSessionDescription(
    const std::string& type,
    const std::string& sdp,
    SdpParseError* error);

RTC_EXPORT std::unique_ptr<SessionDescriptionInterface>
CreateSessionDescription(SdpType type, const std::string& sdp);
RTC_EXPORT std::unique_ptr<SessionDescriptionInterface>
CreateSessionDescription(SdpType type,
                         const std::string& sdp,
                         SdpParseError* error_out);

std::unique_ptr<SessionDescriptionInterface> CreateSessionDescription(
    SdpType type,
    const std::string& session_id,
    const std::string& session_version,
    std::unique_ptr<cricket::SessionDescription> description);

class RTC_EXPORT CreateSessionDescriptionObserver
    : public webrtc::RefCountInterface {
 public:

  virtual void OnSuccess(SessionDescriptionInterface* desc) = 0;

  virtual void OnFailure(RTCError error) = 0;

 protected:
  ~CreateSessionDescriptionObserver() override = default;
};

class RTC_EXPORT SetSessionDescriptionObserver
    : public webrtc::RefCountInterface {
 public:
  virtual void OnSuccess() = 0;

  virtual void OnFailure(RTCError error) = 0;

 protected:
  ~SetSessionDescriptionObserver() override = default;
};

}

#endif
