/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// This implementation is borrowed from Chromium.

#ifndef RTC_BASE_CONTAINERS_FLAT_MAP_H_
#define RTC_BASE_CONTAINERS_FLAT_MAP_H_

#include <functional>
#include <tuple>
#include <utility>
#include <vector>

#include "rtc_base/checks.h"
#include "rtc_base/containers/flat_tree.h"

namespace webrtc {

namespace flat_containers_internal {

struct GetFirst {
  template <class Key, class Mapped>
  constexpr const Key& operator()(const std::pair<Key, Mapped>& p) const {
    return p.first;
  }
};

}

template <class Key,
          class Mapped,
          class Compare = std::less<>,
          class Container = std::vector<std::pair<Key, Mapped>>>
class flat_map : public ::webrtc::flat_containers_internal::flat_tree<
                     Key,
                     flat_containers_internal::GetFirst,
                     Compare,
                     Container> {
 private:
  using tree = typename ::webrtc::flat_containers_internal::
      flat_tree<Key, flat_containers_internal::GetFirst, Compare, Container>;

 public:
  using key_type = typename tree::key_type;
  using mapped_type = Mapped;
  using value_type = typename tree::value_type;
  using reference = typename Container::reference;
  using const_reference = typename Container::const_reference;
  using size_type = typename Container::size_type;
  using difference_type = typename Container::difference_type;
  using iterator = typename tree::iterator;
  using const_iterator = typename tree::const_iterator;
  using reverse_iterator = typename tree::reverse_iterator;
  using const_reverse_iterator = typename tree::const_reverse_iterator;
  using container_type = typename tree::container_type;

  using tree::tree;
  using tree::operator=;

  template <class K>
  mapped_type& at(const K& key);
  template <class K>
  const mapped_type& at(const K& key) const;

  mapped_type& operator[](const key_type& key);
  mapped_type& operator[](key_type&& key);

  template <class K, class M>
  std::pair<iterator, bool> insert_or_assign(K&& key, M&& obj);
  template <class K, class M>
  iterator insert_or_assign(const_iterator hint, K&& key, M&& obj);

  template <class K, class... Args>
  std::enable_if_t<std::is_constructible<key_type, K&&>::value,
                   std::pair<iterator, bool>>
  try_emplace(K&& key, Args&&... args);

  template <class K, class... Args>
  std::enable_if_t<std::is_constructible<key_type, K&&>::value, iterator>
  try_emplace(const_iterator hint, K&& key, Args&&... args);

  void swap(flat_map& other) noexcept;

  friend void swap(flat_map& lhs, flat_map& rhs) noexcept { lhs.swap(rhs); }
};

template <class Key, class Mapped, class Compare, class Container>
template <class K>
auto flat_map<Key, Mapped, Compare, Container>::at(const K& key)
    -> mapped_type& {
  iterator found = tree::find(key);
  RTC_CHECK(found != tree::end());
  return found->second;
}

template <class Key, class Mapped, class Compare, class Container>
template <class K>
auto flat_map<Key, Mapped, Compare, Container>::at(const K& key) const
    -> const mapped_type& {
  const_iterator found = tree::find(key);
  RTC_CHECK(found != tree::cend());
  return found->second;
}

template <class Key, class Mapped, class Compare, class Container>
auto flat_map<Key, Mapped, Compare, Container>::operator[](const key_type& key)
    -> mapped_type& {
  iterator found = tree::lower_bound(key);
  if (found == tree::end() || tree::key_comp()(key, found->first))
    found = tree::unsafe_emplace(found, key, mapped_type());
  return found->second;
}

template <class Key, class Mapped, class Compare, class Container>
auto flat_map<Key, Mapped, Compare, Container>::operator[](key_type&& key)
    -> mapped_type& {
  iterator found = tree::lower_bound(key);
  if (found == tree::end() || tree::key_comp()(key, found->first))
    found = tree::unsafe_emplace(found, std::move(key), mapped_type());
  return found->second;
}

template <class Key, class Mapped, class Compare, class Container>
template <class K, class M>
auto flat_map<Key, Mapped, Compare, Container>::insert_or_assign(K&& key,
                                                                 M&& obj)
    -> std::pair<iterator, bool> {
  auto result =
      tree::emplace_key_args(key, std::forward<K>(key), std::forward<M>(obj));
  if (!result.second)
    result.first->second = std::forward<M>(obj);
  return result;
}

template <class Key, class Mapped, class Compare, class Container>
template <class K, class M>
auto flat_map<Key, Mapped, Compare, Container>::insert_or_assign(
    const_iterator hint,
    K&& key,
    M&& obj) -> iterator {
  auto result = tree::emplace_hint_key_args(hint, key, std::forward<K>(key),
                                            std::forward<M>(obj));
  if (!result.second)
    result.first->second = std::forward<M>(obj);
  return result.first;
}

template <class Key, class Mapped, class Compare, class Container>
template <class K, class... Args>
auto flat_map<Key, Mapped, Compare, Container>::try_emplace(K&& key,
                                                            Args&&... args)
    -> std::enable_if_t<std::is_constructible<key_type, K&&>::value,
                        std::pair<iterator, bool>> {
  return tree::emplace_key_args(
      key, std::piecewise_construct,
      std::forward_as_tuple(std::forward<K>(key)),
      std::forward_as_tuple(std::forward<Args>(args)...));
}

template <class Key, class Mapped, class Compare, class Container>
template <class K, class... Args>
auto flat_map<Key, Mapped, Compare, Container>::try_emplace(const_iterator hint,
                                                            K&& key,
                                                            Args&&... args)
    -> std::enable_if_t<std::is_constructible<key_type, K&&>::value, iterator> {
  return tree::emplace_hint_key_args(
             hint, key, std::piecewise_construct,
             std::forward_as_tuple(std::forward<K>(key)),
             std::forward_as_tuple(std::forward<Args>(args)...))
      .first;
}

template <class Key, class Mapped, class Compare, class Container>
void flat_map<Key, Mapped, Compare, Container>::swap(flat_map& other) noexcept {
  tree::swap(other);
}

using ::webrtc::flat_containers_internal::EraseIf;

}

#endif
