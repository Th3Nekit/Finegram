/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef PC_USED_IDS_H_
#define PC_USED_IDS_H_

#include <set>
#include <vector>

#include "api/rtp_parameters.h"
#include "media/base/codec.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"

namespace cricket {
template <typename IdStruct>
class UsedIds {
 public:
  UsedIds(int min_allowed_id, int max_allowed_id)
      : min_allowed_id_(min_allowed_id),
        max_allowed_id_(max_allowed_id),
        next_id_(max_allowed_id) {}
  virtual ~UsedIds() {}

  template <typename Id>
  void FindAndSetIdUsed(std::vector<Id>* ids) {
    for (const Id& id : *ids) {
      FindAndSetIdUsed(&id);
    }
  }

  void FindAndSetIdUsed(IdStruct* idstruct) {
    const int original_id = idstruct->id;
    int new_id = idstruct->id;

    if (original_id > max_allowed_id_ || original_id < min_allowed_id_) {

      return;
    }

    if (IsIdUsed(original_id)) {
      new_id = FindUnusedId();

      idstruct->id = new_id;
    }
    SetIdUsed(new_id);
  }

 protected:
  virtual bool IsIdUsed(int new_id) {
    return id_set_.find(new_id) != id_set_.end();
  }
  const int min_allowed_id_;
  const int max_allowed_id_;

 private:

  virtual int FindUnusedId() {
    while (IsIdUsed(next_id_) && next_id_ >= min_allowed_id_) {
      --next_id_;
    }
    RTC_DCHECK(next_id_ >= min_allowed_id_);
    return next_id_;
  }

  void SetIdUsed(int new_id) {
    RTC_DCHECK(new_id >= min_allowed_id_);
    RTC_DCHECK(new_id <= max_allowed_id_);
    RTC_DCHECK(!IsIdUsed(new_id));
    id_set_.insert(new_id);
  }
  int next_id_;
  std::set<int> id_set_;
};

class UsedPayloadTypes : public UsedIds<Codec> {
 public:
  UsedPayloadTypes()
      : UsedIds<Codec>(kFirstDynamicPayloadTypeLowerRange,
                       kLastDynamicPayloadTypeUpperRange) {}

  static bool IsIdValid(Codec codec, bool rtcp_mux) {
    if (rtcp_mux && (codec.id > kLastDynamicPayloadTypeLowerRange &&
                     codec.id < kFirstDynamicPayloadTypeUpperRange)) {
      return false;
    }
    return codec.id >= 0 && codec.id <= kLastDynamicPayloadTypeUpperRange;
  }

 protected:
  bool IsIdUsed(int new_id) override {

    if (new_id > kLastDynamicPayloadTypeLowerRange &&
        new_id < kFirstDynamicPayloadTypeUpperRange)
      return true;
    return UsedIds<Codec>::IsIdUsed(new_id);
  }

 private:
  static const int kFirstDynamicPayloadTypeLowerRange = 35;
  static const int kLastDynamicPayloadTypeLowerRange = 63;

  static const int kFirstDynamicPayloadTypeUpperRange = 96;
  static const int kLastDynamicPayloadTypeUpperRange = 127;
};

class UsedRtpHeaderExtensionIds : public UsedIds<webrtc::RtpExtension> {
 public:
  enum class IdDomain {

    kOneByteOnly,

    kTwoByteAllowed,
  };

  explicit UsedRtpHeaderExtensionIds(IdDomain id_domain)
      : UsedIds<webrtc::RtpExtension>(
            webrtc::RtpExtension::kMinId,
            id_domain == IdDomain::kTwoByteAllowed
                ? webrtc::RtpExtension::kMaxId
                : webrtc::RtpExtension::kOneByteHeaderExtensionMaxId),
        id_domain_(id_domain),
        next_extension_id_(webrtc::RtpExtension::kOneByteHeaderExtensionMaxId) {
  }

 private:

  int FindUnusedId() override {
    if (next_extension_id_ <=
        webrtc::RtpExtension::kOneByteHeaderExtensionMaxId) {

      while (IsIdUsed(next_extension_id_) &&
             next_extension_id_ >= min_allowed_id_) {
        --next_extension_id_;
      }
    }

    if (id_domain_ == IdDomain::kTwoByteAllowed) {
      if (next_extension_id_ < min_allowed_id_) {

        next_extension_id_ =
            webrtc::RtpExtension::kOneByteHeaderExtensionMaxId + 2;
      }

      if (next_extension_id_ >
          webrtc::RtpExtension::kOneByteHeaderExtensionMaxId) {
        while (IsIdUsed(next_extension_id_) &&
               next_extension_id_ <= max_allowed_id_) {
          ++next_extension_id_;
        }
      }
    }
    RTC_DCHECK(next_extension_id_ >= min_allowed_id_);
    RTC_DCHECK(next_extension_id_ <= max_allowed_id_);
    return next_extension_id_;
  }

  const IdDomain id_domain_;
  int next_extension_id_;
};

}

#endif
