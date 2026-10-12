/*
 *  Copyright 2021 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "pc/jsep_transport_collection.h"

#include <algorithm>
#include <map>
#include <set>
#include <type_traits>
#include <utility>

#include "p2p/base/p2p_constants.h"
#include "rtc_base/logging.h"

namespace webrtc {

void BundleManager::Update(const cricket::SessionDescription* description,
                           SdpType type) {
  RTC_DCHECK_RUN_ON(&sequence_checker_);

  RTC_DCHECK(type != SdpType::kRollback);
  bool bundle_groups_changed = false;

  if (bundle_policy_ == PeerConnectionInterface::kBundlePolicyMaxBundle ||
      type == SdpType::kAnswer) {

    bundle_groups_changed = true;
    bundle_groups_.clear();
    for (const cricket::ContentGroup* new_bundle_group :
         description->GetGroupsByName(cricket::GROUP_TYPE_BUNDLE)) {
      bundle_groups_.push_back(
          std::make_unique<cricket::ContentGroup>(*new_bundle_group));
      RTC_DLOG(LS_VERBOSE) << "Establishing bundle group "
                           << new_bundle_group->ToString();
    }
  } else if (type == SdpType::kOffer) {

    for (const cricket::ContentGroup* new_bundle_group :
         description->GetGroupsByName(cricket::GROUP_TYPE_BUNDLE)) {

      for (const std::string& mid : new_bundle_group->content_names()) {
        auto it = established_bundle_groups_by_mid_.find(mid);
        if (it != established_bundle_groups_by_mid_.end()) {
          *it->second = *new_bundle_group;
          bundle_groups_changed = true;
          RTC_DLOG(LS_VERBOSE)
              << "Establishing bundle group " << new_bundle_group->ToString();
          break;
        }
      }
    }
  }
  if (bundle_groups_changed) {
    RefreshEstablishedBundleGroupsByMid();
  }
}

const cricket::ContentGroup* BundleManager::LookupGroupByMid(
    const std::string& mid) const {
  auto it = established_bundle_groups_by_mid_.find(mid);
  return it != established_bundle_groups_by_mid_.end() ? it->second : nullptr;
}
bool BundleManager::IsFirstMidInGroup(const std::string& mid) const {
  auto group = LookupGroupByMid(mid);
  if (!group) {
    return true;
  }
  return mid == *(group->FirstContentName());
}

cricket::ContentGroup* BundleManager::LookupGroupByMid(const std::string& mid) {
  auto it = established_bundle_groups_by_mid_.find(mid);
  return it != established_bundle_groups_by_mid_.end() ? it->second : nullptr;
}

void BundleManager::DeleteMid(const cricket::ContentGroup* bundle_group,
                              const std::string& mid) {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  RTC_LOG(LS_VERBOSE) << "Deleting mid " << mid << " from bundle group "
                      << bundle_group->ToString();

  auto bundle_group_it = std::find_if(
      bundle_groups_.begin(), bundle_groups_.end(),
      [bundle_group](std::unique_ptr<cricket::ContentGroup>& group) {
        return bundle_group == group.get();
      });
  RTC_DCHECK(bundle_group_it != bundle_groups_.end());
  (*bundle_group_it)->RemoveContentName(mid);
  established_bundle_groups_by_mid_.erase(
      established_bundle_groups_by_mid_.find(mid));
}

void BundleManager::DeleteGroup(const cricket::ContentGroup* bundle_group) {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  RTC_DLOG(LS_VERBOSE) << "Deleting bundle group " << bundle_group->ToString();

  auto bundle_group_it = std::find_if(
      bundle_groups_.begin(), bundle_groups_.end(),
      [bundle_group](std::unique_ptr<cricket::ContentGroup>& group) {
        return bundle_group == group.get();
      });
  RTC_DCHECK(bundle_group_it != bundle_groups_.end());
  auto mid_list = (*bundle_group_it)->content_names();
  for (const auto& content_name : mid_list) {
    DeleteMid(bundle_group, content_name);
  }
  bundle_groups_.erase(bundle_group_it);
}

void BundleManager::Rollback() {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  bundle_groups_.clear();
  for (const auto& bundle_group : stable_bundle_groups_) {
    bundle_groups_.push_back(
        std::make_unique<cricket::ContentGroup>(*bundle_group));
  }
  RefreshEstablishedBundleGroupsByMid();
}

void BundleManager::Commit() {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  stable_bundle_groups_.clear();
  for (const auto& bundle_group : bundle_groups_) {
    stable_bundle_groups_.push_back(
        std::make_unique<cricket::ContentGroup>(*bundle_group));
  }
}

void BundleManager::RefreshEstablishedBundleGroupsByMid() {
  established_bundle_groups_by_mid_.clear();
  for (const auto& bundle_group : bundle_groups_) {
    for (const std::string& content_name : bundle_group->content_names()) {
      established_bundle_groups_by_mid_[content_name] = bundle_group.get();
    }
  }
}

void JsepTransportCollection::RegisterTransport(
    const std::string& mid,
    std::unique_ptr<cricket::JsepTransport> transport) {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  SetTransportForMid(mid, transport.get());
  jsep_transports_by_name_[mid] = std::move(transport);
  RTC_DCHECK(IsConsistent());
}

std::vector<cricket::JsepTransport*> JsepTransportCollection::Transports() {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  std::vector<cricket::JsepTransport*> result;
  for (auto& kv : jsep_transports_by_name_) {
    result.push_back(kv.second.get());
  }
  return result;
}

std::vector<cricket::JsepTransport*>
JsepTransportCollection::ActiveTransports() {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  std::set<cricket::JsepTransport*> transports;
  for (const auto& kv : mid_to_transport_) {
    transports.insert(kv.second);
  }
  return std::vector<cricket::JsepTransport*>(transports.begin(),
                                              transports.end());
}

void JsepTransportCollection::DestroyAllTransports() {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  for (const auto& jsep_transport : jsep_transports_by_name_) {
    map_change_callback_(jsep_transport.first, nullptr);
  }
  jsep_transports_by_name_.clear();
  RTC_DCHECK(IsConsistent());
}

const cricket::JsepTransport* JsepTransportCollection::GetTransportByName(
    const std::string& transport_name) const {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  auto it = jsep_transports_by_name_.find(transport_name);
  return (it == jsep_transports_by_name_.end()) ? nullptr : it->second.get();
}

cricket::JsepTransport* JsepTransportCollection::GetTransportByName(
    const std::string& transport_name) {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  auto it = jsep_transports_by_name_.find(transport_name);
  return (it == jsep_transports_by_name_.end()) ? nullptr : it->second.get();
}

cricket::JsepTransport* JsepTransportCollection::GetTransportForMid(
    const std::string& mid) {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  auto it = mid_to_transport_.find(mid);
  return it == mid_to_transport_.end() ? nullptr : it->second;
}

const cricket::JsepTransport* JsepTransportCollection::GetTransportForMid(
    const std::string& mid) const {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  auto it = mid_to_transport_.find(mid);
  return it == mid_to_transport_.end() ? nullptr : it->second;
}

cricket::JsepTransport* JsepTransportCollection::GetTransportForMid(
    absl::string_view mid) {
  RTC_DCHECK_RUN_ON(&sequence_checker_);

  auto it = mid_to_transport_.find(std::string(mid));
  return it == mid_to_transport_.end() ? nullptr : it->second;
}

const cricket::JsepTransport* JsepTransportCollection::GetTransportForMid(
    absl::string_view mid) const {
  RTC_DCHECK_RUN_ON(&sequence_checker_);

  auto it = mid_to_transport_.find(std::string(mid));
  return it == mid_to_transport_.end() ? nullptr : it->second;
}

bool JsepTransportCollection::SetTransportForMid(
    const std::string& mid,
    cricket::JsepTransport* jsep_transport) {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  RTC_DCHECK(jsep_transport);

  auto it = mid_to_transport_.find(mid);
  if (it != mid_to_transport_.end() && it->second == jsep_transport)
    return true;

  bool result = map_change_callback_(mid, jsep_transport);

  if (it == mid_to_transport_.end()) {
    mid_to_transport_.insert(std::make_pair(mid, jsep_transport));
  } else {
    auto old_transport = it->second;
    it->second = jsep_transport;
    MaybeDestroyJsepTransport(old_transport);
  }
  RTC_DCHECK(IsConsistent());
  return result;
}

void JsepTransportCollection::RemoveTransportForMid(const std::string& mid) {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  RTC_DCHECK(IsConsistent());
  bool ret = map_change_callback_(mid, nullptr);

  RTC_DCHECK(ret);

  auto old_transport = GetTransportForMid(mid);
  if (old_transport) {
    mid_to_transport_.erase(mid);
    MaybeDestroyJsepTransport(old_transport);
  }
  RTC_DCHECK(IsConsistent());
}

bool JsepTransportCollection::RollbackTransports() {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  bool ret = true;

  for (const auto& kv : mid_to_transport_) {
    if (stable_mid_to_transport_.count(kv.first) == 0) {
      ret = ret && map_change_callback_(kv.first, nullptr);
    }
  }

  for (const auto& kv : stable_mid_to_transport_) {
    auto it = mid_to_transport_.find(kv.first);
    if (it == mid_to_transport_.end() || it->second != kv.second) {
      ret = ret && map_change_callback_(kv.first, kv.second);
    }
  }
  mid_to_transport_ = stable_mid_to_transport_;

  state_change_callback_();
  DestroyUnusedTransports();
  RTC_DCHECK(IsConsistent());
  return ret;
}

void JsepTransportCollection::CommitTransports() {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  stable_mid_to_transport_ = mid_to_transport_;
  DestroyUnusedTransports();
  RTC_DCHECK(IsConsistent());
}

bool JsepTransportCollection::TransportInUse(
    cricket::JsepTransport* jsep_transport) const {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  for (const auto& kv : mid_to_transport_) {
    if (kv.second == jsep_transport) {
      return true;
    }
  }
  return false;
}

bool JsepTransportCollection::TransportNeededForRollback(
    cricket::JsepTransport* jsep_transport) const {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  for (const auto& kv : stable_mid_to_transport_) {
    if (kv.second == jsep_transport) {
      return true;
    }
  }
  return false;
}

void JsepTransportCollection::MaybeDestroyJsepTransport(
    cricket::JsepTransport* transport) {
  RTC_DCHECK_RUN_ON(&sequence_checker_);

  if (TransportInUse(transport)) {
    return;
  }

  if (TransportNeededForRollback(transport)) {
    state_change_callback_();
    return;
  }
  for (const auto& it : jsep_transports_by_name_) {
    if (it.second.get() == transport) {
      jsep_transports_by_name_.erase(it.first);
      state_change_callback_();
      break;
    }
  }
  RTC_DCHECK(IsConsistent());
}

void JsepTransportCollection::DestroyUnusedTransports() {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  bool need_state_change_callback = false;
  auto it = jsep_transports_by_name_.begin();
  while (it != jsep_transports_by_name_.end()) {
    if (TransportInUse(it->second.get()) ||
        TransportNeededForRollback(it->second.get())) {
      ++it;
    } else {
      it = jsep_transports_by_name_.erase(it);
      need_state_change_callback = true;
    }
  }
  if (need_state_change_callback) {
    state_change_callback_();
  }
}

bool JsepTransportCollection::IsConsistent() {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  for (const auto& it : jsep_transports_by_name_) {
    if (!TransportInUse(it.second.get()) &&
        !TransportNeededForRollback(it.second.get())) {
      RTC_LOG(LS_ERROR) << "Transport registered with mid " << it.first
                        << " is not in use, transport " << it.second.get();
      return false;
    }
  }
  return true;
}

}
