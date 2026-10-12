/*
 *  Copyright 2020 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef P2P_BASE_STUN_DICTIONARY_H_
#define P2P_BASE_STUN_DICTIONARY_H_

#include <deque>
#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "api/rtc_error.h"
#include "api/transport/stun.h"

namespace cricket {

class StunDictionaryView {
 public:

  static constexpr uint16_t kVersionKey = 0xFFFF;

  static constexpr uint16_t kDeltaMagic = 0x7788;

  static constexpr uint16_t kDeltaVersion = 0x1;

  const StunAddressAttribute* GetAddress(int key) const;
  const StunUInt32Attribute* GetUInt32(int key) const;
  const StunUInt64Attribute* GetUInt64(int key) const;
  const StunByteStringAttribute* GetByteString(int key) const;
  const StunUInt16ListAttribute* GetUInt16List(int key) const;

  bool empty() const { return attrs_.empty(); }
  size_t size() const { return attrs_.size(); }
  int bytes_stored() const { return bytes_stored_; }
  void set_max_bytes_stored(int max_bytes_stored) {
    max_bytes_stored_ = max_bytes_stored;
  }

  webrtc::RTCErrorOr<
      std::pair<std::unique_ptr<StunUInt64Attribute>, std::vector<uint16_t>>>
  ApplyDelta(const StunByteStringAttribute& delta);

 private:
  friend class StunDictionaryWriter;

  const StunAttribute* GetOrNull(
      int key,
      absl::optional<StunAttributeValueType> = absl::nullopt) const;
  size_t GetLength(int key) const;
  static webrtc::RTCErrorOr<
      std::pair<uint64_t, std::deque<std::unique_ptr<StunAttribute>>>>
  ParseDelta(const StunByteStringAttribute& delta);

  std::map<uint16_t, std::unique_ptr<StunAttribute>> attrs_;
  std::map<uint16_t, uint64_t> version_per_key_;

  int max_bytes_stored_ = 16384;
  int bytes_stored_ = 0;
};

class StunDictionaryWriter {
 public:
  StunDictionaryWriter() {
    dictionary_ = std::make_unique<StunDictionaryView>();
  }
  explicit StunDictionaryWriter(
      std::unique_ptr<StunDictionaryView> dictionary) {
    dictionary_ = std::move(dictionary);
  }

  template <typename T>
  class Modification {
   public:
    ~Modification() { commit(); }

    T* operator->() { return attr_.get(); }

    void abort() { attr_ = nullptr; }
    void commit() {
      if (attr_) {
        writer_->Set(std::move(attr_));
      }
    }

   private:
    friend class StunDictionaryWriter;
    Modification(StunDictionaryWriter* writer, std::unique_ptr<T> attr)
        : writer_(writer), attr_(std::move(attr)) {}
    StunDictionaryWriter* writer_;
    std::unique_ptr<T> attr_;

    Modification(const Modification<T>&) =
        delete;
    Modification& operator=(Modification<T>&) =
        delete;
  };

  Modification<StunAddressAttribute> SetAddress(int key) {
    return Modification<StunAddressAttribute>(
        this, StunAttribute::CreateAddress(key));
  }
  Modification<StunUInt32Attribute> SetUInt32(int key) {
    return Modification<StunUInt32Attribute>(this,
                                             StunAttribute::CreateUInt32(key));
  }
  Modification<StunUInt64Attribute> SetUInt64(int key) {
    return Modification<StunUInt64Attribute>(this,
                                             StunAttribute::CreateUInt64(key));
  }
  Modification<StunByteStringAttribute> SetByteString(int key) {
    return Modification<StunByteStringAttribute>(
        this, StunAttribute::CreateByteString(key));
  }
  Modification<StunUInt16ListAttribute> SetUInt16List(int key) {
    return Modification<StunUInt16ListAttribute>(
        this, StunAttribute::CreateUInt16ListAttribute(key));
  }

  void Delete(int key);

  bool Pending(int key) const;

  int Pending() const;

  std::unique_ptr<StunByteStringAttribute> CreateDelta();

  void ApplyDeltaAck(const StunUInt64Attribute&);

  const StunDictionaryView* dictionary() { return dictionary_.get(); }
  const StunDictionaryView* operator->() { return dictionary_.get(); }

  void Disable();
  bool disabled() const { return disabled_; }

 private:
  void Set(std::unique_ptr<StunAttribute> attr);

  bool disabled_ = false;

  int64_t version_ = 1;

  std::unique_ptr<StunDictionaryView> dictionary_;

  std::vector<std::pair<uint64_t, const StunAttribute*>> pending_;

  std::map<uint16_t, std::unique_ptr<StunAttribute>> tombstones_;
};

}

#endif
