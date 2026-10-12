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

#ifndef RTC_BASE_CONTAINERS_FLAT_TREE_H_
#define RTC_BASE_CONTAINERS_FLAT_TREE_H_

#include <algorithm>
#include <iterator>
#include <type_traits>
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "rtc_base/checks.h"
#include "rtc_base/system/no_unique_address.h"

namespace webrtc {

struct sorted_unique_t {
  constexpr sorted_unique_t() = default;
};
extern sorted_unique_t sorted_unique;

namespace flat_containers_internal {

template <typename Range, typename Comp>
constexpr bool is_sorted_and_unique(const Range& range, Comp comp) {

  return absl::c_adjacent_find(range, std::not_fn(comp)) == std::end(range);
}

template <class Iterator>
using is_multipass =
    std::is_base_of<std::forward_iterator_tag,
                    typename std::iterator_traits<Iterator>::iterator_category>;

template <typename T, typename = void>
struct IsTransparentCompare : std::false_type {};
template <typename T>
struct IsTransparentCompare<T, std::void_t<typename T::is_transparent>>
    : std::true_type {};

template <typename U, typename T, size_t N, size_t... I>
constexpr std::array<U, N> ToArrayImpl(const T (&data)[N],
                                       std::index_sequence<I...>) {
  return {{data[I]...}};
}

template <typename U, typename T, size_t N>
constexpr std::array<U, N> ToArray(const T (&data)[N]) {
  return ToArrayImpl<U>(data, std::make_index_sequence<N>());
}

template <typename T>
constexpr void Assign(T& lhs, T&& rhs) {
  lhs = std::move(rhs);
}

template <typename T, typename U>
constexpr void Assign(std::pair<T, U>& lhs, std::pair<T, U>&& rhs) {
  Assign(lhs.first, std::move(rhs.first));
  Assign(lhs.second, std::move(rhs.second));
}

template <typename T>
constexpr void Swap(T& lhs, T& rhs) {
  T tmp = std::move(lhs);
  Assign(lhs, std::move(rhs));
  Assign(rhs, std::move(tmp));
}

template <typename BidirIt>
constexpr BidirIt Prev(BidirIt it) {
  return --it;
}

template <typename InputIt>
constexpr InputIt Next(InputIt it) {
  return ++it;
}

template <typename BidirIt, typename Compare>
constexpr void InsertionSort(BidirIt first, BidirIt last, const Compare& comp) {
  if (first == last)
    return;

  for (auto it = Next(first); it != last; ++it) {
    for (auto curr = it; curr != first && comp(*curr, *Prev(curr)); --curr)
      Swap(*curr, *Prev(curr));
  }
}

template <class Key, class GetKeyFromValue, class KeyCompare, class Container>
class flat_tree {
 public:

  using key_type = Key;
  using key_compare = KeyCompare;
  using value_type = typename Container::value_type;

  struct value_compare {
    constexpr bool operator()(const value_type& left,
                              const value_type& right) const {
      GetKeyFromValue extractor;
      return comp(extractor(left), extractor(right));
    }

    RTC_NO_UNIQUE_ADDRESS key_compare comp;
  };

  using pointer = typename Container::pointer;
  using const_pointer = typename Container::const_pointer;
  using reference = typename Container::reference;
  using const_reference = typename Container::const_reference;
  using size_type = typename Container::size_type;
  using difference_type = typename Container::difference_type;
  using iterator = typename Container::iterator;
  using const_iterator = typename Container::const_iterator;
  using reverse_iterator = typename Container::reverse_iterator;
  using const_reverse_iterator = typename Container::const_reverse_iterator;
  using container_type = Container;

  flat_tree() = default;
  flat_tree(const flat_tree&) = default;
  flat_tree(flat_tree&&) = default;

  explicit flat_tree(const key_compare& comp);

  template <class InputIterator>
  flat_tree(InputIterator first,
            InputIterator last,
            const key_compare& comp = key_compare());

  flat_tree(const container_type& items,
            const key_compare& comp = key_compare());

  explicit flat_tree(container_type&& items,
                     const key_compare& comp = key_compare());

  flat_tree(std::initializer_list<value_type> ilist,
            const key_compare& comp = key_compare());

  template <class InputIterator>
  flat_tree(sorted_unique_t,
            InputIterator first,
            InputIterator last,
            const key_compare& comp = key_compare());

  flat_tree(sorted_unique_t,
            const container_type& items,
            const key_compare& comp = key_compare());

  constexpr flat_tree(sorted_unique_t,
                      container_type&& items,
                      const key_compare& comp = key_compare());

  flat_tree(sorted_unique_t,
            std::initializer_list<value_type> ilist,
            const key_compare& comp = key_compare());

  ~flat_tree() = default;

  flat_tree& operator=(const flat_tree&) = default;
  flat_tree& operator=(flat_tree&&) = default;

  flat_tree& operator=(std::initializer_list<value_type> ilist);

  void reserve(size_type new_capacity);
  size_type capacity() const;
  void shrink_to_fit();

  void clear();

  constexpr size_type size() const;
  constexpr size_type max_size() const;
  constexpr bool empty() const;

  iterator begin();
  constexpr const_iterator begin() const;
  const_iterator cbegin() const;

  iterator end();
  constexpr const_iterator end() const;
  const_iterator cend() const;

  reverse_iterator rbegin();
  const_reverse_iterator rbegin() const;
  const_reverse_iterator crbegin() const;

  reverse_iterator rend();
  const_reverse_iterator rend() const;
  const_reverse_iterator crend() const;

  std::pair<iterator, bool> insert(const value_type& val);
  std::pair<iterator, bool> insert(value_type&& val);

  iterator insert(const_iterator position_hint, const value_type& x);
  iterator insert(const_iterator position_hint, value_type&& x);

  template <class InputIterator>
  void insert(InputIterator first, InputIterator last);

  template <class... Args>
  std::pair<iterator, bool> emplace(Args&&... args);

  template <class... Args>
  iterator emplace_hint(const_iterator position_hint, Args&&... args);

  container_type extract() &&;

  void replace(container_type&& body);

  iterator erase(iterator position);

  template <typename DummyT = void>
  iterator erase(const_iterator position);
  iterator erase(const_iterator first, const_iterator last);
  template <typename K>
  size_type erase(const K& key);

  constexpr key_compare key_comp() const;
  constexpr value_compare value_comp() const;

  template <typename K>
  size_type count(const K& key) const;

  template <typename K>
  iterator find(const K& key);

  template <typename K>
  const_iterator find(const K& key) const;

  template <typename K>
  bool contains(const K& key) const;

  template <typename K>
  std::pair<iterator, iterator> equal_range(const K& key);

  template <typename K>
  std::pair<const_iterator, const_iterator> equal_range(const K& key) const;

  template <typename K>
  iterator lower_bound(const K& key);

  template <typename K>
  const_iterator lower_bound(const K& key) const;

  template <typename K>
  iterator upper_bound(const K& key);

  template <typename K>
  const_iterator upper_bound(const K& key) const;

  void swap(flat_tree& other) noexcept;

  friend bool operator==(const flat_tree& lhs, const flat_tree& rhs) {
    return lhs.body_ == rhs.body_;
  }

  friend bool operator!=(const flat_tree& lhs, const flat_tree& rhs) {
    return !(lhs == rhs);
  }

  friend bool operator<(const flat_tree& lhs, const flat_tree& rhs) {
    return lhs.body_ < rhs.body_;
  }

  friend bool operator>(const flat_tree& lhs, const flat_tree& rhs) {
    return rhs < lhs;
  }

  friend bool operator>=(const flat_tree& lhs, const flat_tree& rhs) {
    return !(lhs < rhs);
  }

  friend bool operator<=(const flat_tree& lhs, const flat_tree& rhs) {
    return !(lhs > rhs);
  }

  friend void swap(flat_tree& lhs, flat_tree& rhs) noexcept { lhs.swap(rhs); }

 protected:

  template <class... Args>
  iterator unsafe_emplace(const_iterator position, Args&&... args);

  template <class K, class... Args>
  std::pair<iterator, bool> emplace_key_args(const K& key, Args&&... args);

  template <class K, class... Args>
  std::pair<iterator, bool> emplace_hint_key_args(const_iterator hint,
                                                  const K& key,
                                                  Args&&... args);

 private:

  struct KeyValueCompare {

    explicit KeyValueCompare(const key_compare& comp) : comp_(comp) {}

    template <typename T, typename U>
    bool operator()(const T& lhs, const U& rhs) const {
      return comp_(extract_if_value_type(lhs), extract_if_value_type(rhs));
    }

   private:
    const key_type& extract_if_value_type(const value_type& v) const {
      GetKeyFromValue extractor;
      return extractor(v);
    }

    template <typename K>
    const K& extract_if_value_type(const K& k) const {
      return k;
    }

    const key_compare& comp_;
  };

  iterator const_cast_it(const_iterator c_it) {
    auto distance = std::distance(cbegin(), c_it);
    return std::next(begin(), distance);
  }

  template <class V>
  std::pair<iterator, bool> insert_or_assign(V&& val) {
    auto position = lower_bound(GetKeyFromValue()(val));

    if (position == end() || value_comp()(val, *position))
      return {body_.emplace(position, std::forward<V>(val)), true};

    *position = std::forward<V>(val);
    return {position, false};
  }

  template <class V>
  std::pair<iterator, bool> append_or_assign(iterator first,
                                             iterator last,
                                             V&& val) {
    auto position = std::lower_bound(first, last, val, value_comp());

    if (position == last || value_comp()(val, *position)) {

      const difference_type distance = std::distance(begin(), position);
      body_.emplace_back(std::forward<V>(val));
      return {std::next(begin(), distance), true};
    }

    *position = std::forward<V>(val);
    return {position, false};
  }

  template <class V>
  std::pair<iterator, bool> append_unique(iterator first,
                                          iterator last,
                                          V&& val) {
    auto position = std::lower_bound(first, last, val, value_comp());

    if (position == last || value_comp()(val, *position)) {

      const difference_type distance = std::distance(begin(), position);
      body_.emplace_back(std::forward<V>(val));
      return {std::next(begin(), distance), true};
    }

    return {position, false};
  }

  void sort_and_unique(iterator first, iterator last) {

    std::stable_sort(first, last, value_comp());

    auto equal_comp = std::not_fn(value_comp());
    erase(std::unique(first, last, equal_comp), last);
  }

  void sort_and_unique() { sort_and_unique(begin(), end()); }

  RTC_NO_UNIQUE_ADDRESS key_compare comp_;

  container_type body_;

  template <typename K>
  using KeyTypeOrK = typename std::
      conditional<IsTransparentCompare<key_compare>::value, K, key_type>::type;
};

template <class Key, class GetKeyFromValue, class KeyCompare, class Container>
flat_tree<Key, GetKeyFromValue, KeyCompare, Container>::flat_tree(
    const KeyCompare& comp)
    : comp_(comp) {}

template <class Key, class GetKeyFromValue, class KeyCompare, class Container>
template <class InputIterator>
flat_tree<Key, GetKeyFromValue, KeyCompare, Container>::flat_tree(
    InputIterator first,
    InputIterator last,
    const KeyCompare& comp)
    : comp_(comp), body_(first, last) {
  sort_and_unique();
}

template <class Key, class GetKeyFromValue, class KeyCompare, class Container>
flat_tree<Key, GetKeyFromValue, KeyCompare, Container>::flat_tree(
    const container_type& items,
    const KeyCompare& comp)
    : comp_(comp), body_(items) {
  sort_and_unique();
}

template <class Key, class GetKeyFromValue, class KeyCompare, class Container>
flat_tree<Key, GetKeyFromValue, KeyCompare, Container>::flat_tree(
    container_type&& items,
    const KeyCompare& comp)
    : comp_(comp), body_(std::move(items)) {
  sort_and_unique();
}

template <class Key, class GetKeyFromValue, class KeyCompare, class Container>
flat_tree<Key, GetKeyFromValue, KeyCompare, Container>::flat_tree(
    std::initializer_list<value_type> ilist,
    const KeyCompare& comp)
    : flat_tree(std::begin(ilist), std::end(ilist), comp) {}

template <class Key, class GetKeyFromValue, class KeyCompare, class Container>
template <class InputIterator>
flat_tree<Key, GetKeyFromValue, KeyCompare, Container>::flat_tree(
    sorted_unique_t,
    InputIterator first,
    InputIterator last,
    const KeyCompare& comp)
    : comp_(comp), body_(first, last) {
  RTC_DCHECK(is_sorted_and_unique(*this, value_comp()));
}

template <class Key, class GetKeyFromValue, class KeyCompare, class Container>
flat_tree<Key, GetKeyFromValue, KeyCompare, Container>::flat_tree(
    sorted_unique_t,
    const container_type& items,
    const KeyCompare& comp)
    : comp_(comp), body_(items) {
  RTC_DCHECK(is_sorted_and_unique(*this, value_comp()));
}

template <class Key, class GetKeyFromValue, class KeyCompare, class Container>
constexpr flat_tree<Key, GetKeyFromValue, KeyCompare, Container>::flat_tree(
    sorted_unique_t,
    container_type&& items,
    const KeyCompare& comp)
    : comp_(comp), body_(std::move(items)) {
  RTC_DCHECK(is_sorted_and_unique(*this, value_comp()));
}

template <class Key, class GetKeyFromValue, class KeyCompare, class Container>
flat_tree<Key, GetKeyFromValue, KeyCompare, Container>::flat_tree(
    sorted_unique_t,
    std::initializer_list<value_type> ilist,
    const KeyCompare& comp)
    : flat_tree(sorted_unique, std::begin(ilist), std::end(ilist), comp) {}

template <class Key, class GetKeyFromValue, class KeyCompare, class Container>
auto flat_tree<Key, GetKeyFromValue, KeyCompare, Container>::operator=(
    std::initializer_list<value_type> ilist) -> flat_tree& {
  body_ = ilist;
  sort_and_unique();
  return *this;
}

template <class Key, class GetKeyFromValue, class KeyCompare, class Container>
void flat_tree<Key, GetKeyFromValue, KeyCompare, Container>::reserve(
    size_type new_capacity) {
  body_.reserve(new_capacity);
}

template <class Key, class GetKeyFromValue, class KeyCompare, class Container>
auto flat_tree<Key, GetKeyFromValue, KeyCompare, Container>::capacity() const
    -> size_type {
  return body_.capacity();
}

template <class Key, class GetKeyFromValue, class KeyCompare, class Container>
void flat_tree<Key, GetKeyFromValue, KeyCompare, Container>::shrink_to_fit() {
  body_.shrink_to_fit();
}

template <class Key, class GetKeyFromValue, class KeyCompare, class Container>
void flat_tree<Key, GetKeyFromValue, KeyCompare, Container>::clear() {
  body_.clear();
}

template <class Key, class GetKeyFromValue, class KeyCompare, class Container>
constexpr auto flat_tree<Key, GetKeyFromValue, KeyCompare, Container>::size()
    const -> size_type {
  return body_.size();
}

template <class Key, class GetKeyFromValue, class KeyCompare, class Container>
constexpr auto
flat_tree<Key, GetKeyFromValue, KeyCompare, Container>::max_size() const
    -> size_type {
  return body_.max_size();
}

template <class Key, class GetKeyFromValue, class KeyCompare, class Container>
constexpr bool flat_tree<Key, GetKeyFromValue, KeyCompare, Container>::empty()
    const {
  return body_.empty();
}

template <class Key, class GetKeyFromValue, class KeyCompare, class Container>
auto flat_tree<Key, GetKeyFromValue, KeyCompare, Container>::begin()
    -> iterator {
  return body_.begin();
}

template <class Key, class GetKeyFromValue, class KeyCompare, class Container>
constexpr auto flat_tree<Key, GetKeyFromValue, KeyCompare, Container>::begin()
    const -> const_iterator {
  return std::begin(body_);
}

template <class Key, class GetKeyFromValue, class KeyCompare, class Container>
auto flat_tree<Key, GetKeyFromValue, KeyCompare, Container>::cbegin() const
    -> const_iterator {
  return body_.cbegin();
}

template <class Key, class GetKeyFromValue, class KeyCompare, class Container>
auto flat_tree<Key, GetKeyFromValue, KeyCompare, Container>::end() -> iterator {
  return body_.end();
}

template <class Key, class GetKeyFromValue, class KeyCompare, class Container>
constexpr auto flat_tree<Key, GetKeyFromValue, KeyCompare, Container>::end()
    const -> const_iterator {
  return std::end(body_);
}

template <class Key, class GetKeyFromValue, class KeyCompare, class Container>
auto flat_tree<Key, GetKeyFromValue, KeyCompare, Container>::cend() const
    -> const_iterator {
  return body_.cend();
}

template <class Key, class GetKeyFromValue, class KeyCompare, class Container>
auto flat_tree<Key, GetKeyFromValue, KeyCompare, Container>::rbegin()
    -> reverse_iterator {
  return body_.rbegin();
}

template <class Key, class GetKeyFromValue, class KeyCompare, class Container>
auto flat_tree<Key, GetKeyFromValue, KeyCompare, Container>::rbegin() const
    -> const_reverse_iterator {
  return body_.rbegin();
}

template <class Key, class GetKeyFromValue, class KeyCompare, class Container>
auto flat_tree<Key, GetKeyFromValue, KeyCompare, Container>::crbegin() const
    -> const_reverse_iterator {
  return body_.crbegin();
}

template <class Key, class GetKeyFromValue, class KeyCompare, class Container>
auto flat_tree<Key, GetKeyFromValue, KeyCompare, Container>::rend()
    -> reverse_iterator {
  return body_.rend();
}

template <class Key, class GetKeyFromValue, class KeyCompare, class Container>
auto flat_tree<Key, GetKeyFromValue, KeyCompare, Container>::rend() const
    -> const_reverse_iterator {
  return body_.rend();
}

template <class Key, class GetKeyFromValue, class KeyCompare, class Container>
auto flat_tree<Key, GetKeyFromValue, KeyCompare, Container>::crend() const
    -> const_reverse_iterator {
  return body_.crend();
}

template <class Key, class GetKeyFromValue, class KeyCompare, class Container>
auto flat_tree<Key, GetKeyFromValue, KeyCompare, Container>::insert(
    const value_type& val) -> std::pair<iterator, bool> {
  return emplace_key_args(GetKeyFromValue()(val), val);
}

template <class Key, class GetKeyFromValue, class KeyCompare, class Container>
auto flat_tree<Key, GetKeyFromValue, KeyCompare, Container>::insert(
    value_type&& val) -> std::pair<iterator, bool> {
  return emplace_key_args(GetKeyFromValue()(val), std::move(val));
}

template <class Key, class GetKeyFromValue, class KeyCompare, class Container>
auto flat_tree<Key, GetKeyFromValue, KeyCompare, Container>::insert(
    const_iterator position_hint,
    const value_type& val) -> iterator {
  return emplace_hint_key_args(position_hint, GetKeyFromValue()(val), val)
      .first;
}

template <class Key, class GetKeyFromValue, class KeyCompare, class Container>
auto flat_tree<Key, GetKeyFromValue, KeyCompare, Container>::insert(
    const_iterator position_hint,
    value_type&& val) -> iterator {
  return emplace_hint_key_args(position_hint, GetKeyFromValue()(val),
                               std::move(val))
      .first;
}

template <class Key, class GetKeyFromValue, class KeyCompare, class Container>
template <class InputIterator>
void flat_tree<Key, GetKeyFromValue, KeyCompare, Container>::insert(
    InputIterator first,
    InputIterator last) {
  if (first == last)
    return;

  if (is_multipass<InputIterator>() && std::next(first) == last) {
    insert(end(), *first);
    return;
  }

  auto middle = [this, size = size()] { return std::next(begin(), size); };

  difference_type pos_first_new = size();

  for (; first != last; ++first) {
    std::pair<iterator, bool> result = append_unique(begin(), middle(), *first);
    if (result.second) {
      pos_first_new =
          std::min(pos_first_new, std::distance(begin(), result.first));
    }
  }

  sort_and_unique(middle(), end());
  std::inplace_merge(std::next(begin(), pos_first_new), middle(), end(),
                     value_comp());
}

template <class Key, class GetKeyFromValue, class KeyCompare, class Container>
template <class... Args>
auto flat_tree<Key, GetKeyFromValue, KeyCompare, Container>::emplace(
    Args&&... args) -> std::pair<iterator, bool> {
  return insert(value_type(std::forward<Args>(args)...));
}

template <class Key, class GetKeyFromValue, class KeyCompare, class Container>
template <class... Args>
auto flat_tree<Key, GetKeyFromValue, KeyCompare, Container>::emplace_hint(
    const_iterator position_hint,
    Args&&... args) -> iterator {
  return insert(position_hint, value_type(std::forward<Args>(args)...));
}

template <class Key, class GetKeyFromValue, class KeyCompare, class Container>
auto flat_tree<Key, GetKeyFromValue, KeyCompare, Container>::
    extract() && -> container_type {
  return std::exchange(body_, container_type());
}

template <class Key, class GetKeyFromValue, class KeyCompare, class Container>
void flat_tree<Key, GetKeyFromValue, KeyCompare, Container>::replace(
    container_type&& body) {

  RTC_DCHECK(is_sorted_and_unique(body, value_comp()));
  body_ = std::move(body);
}

template <class Key, class GetKeyFromValue, class KeyCompare, class Container>
auto flat_tree<Key, GetKeyFromValue, KeyCompare, Container>::erase(
    iterator position) -> iterator {
  RTC_CHECK(position != body_.end());
  return body_.erase(position);
}

template <class Key, class GetKeyFromValue, class KeyCompare, class Container>
template <typename DummyT>
auto flat_tree<Key, GetKeyFromValue, KeyCompare, Container>::erase(
    const_iterator position) -> iterator {
  RTC_CHECK(position != body_.end());
  return body_.erase(position);
}

template <class Key, class GetKeyFromValue, class KeyCompare, class Container>
template <typename K>
auto flat_tree<Key, GetKeyFromValue, KeyCompare, Container>::erase(const K& val)
    -> size_type {
  auto eq_range = equal_range(val);
  auto res = std::distance(eq_range.first, eq_range.second);
  erase(eq_range.first, eq_range.second);
  return res;
}

template <class Key, class GetKeyFromValue, class KeyCompare, class Container>
auto flat_tree<Key, GetKeyFromValue, KeyCompare, Container>::erase(
    const_iterator first,
    const_iterator last) -> iterator {
  return body_.erase(first, last);
}

template <class Key, class GetKeyFromValue, class KeyCompare, class Container>
constexpr auto
flat_tree<Key, GetKeyFromValue, KeyCompare, Container>::key_comp() const
    -> key_compare {
  return comp_;
}

template <class Key, class GetKeyFromValue, class KeyCompare, class Container>
constexpr auto
flat_tree<Key, GetKeyFromValue, KeyCompare, Container>::value_comp() const
    -> value_compare {
  return value_compare{comp_};
}

template <class Key, class GetKeyFromValue, class KeyCompare, class Container>
template <typename K>
auto flat_tree<Key, GetKeyFromValue, KeyCompare, Container>::count(
    const K& key) const -> size_type {
  auto eq_range = equal_range(key);
  return std::distance(eq_range.first, eq_range.second);
}

template <class Key, class GetKeyFromValue, class KeyCompare, class Container>
template <typename K>
auto flat_tree<Key, GetKeyFromValue, KeyCompare, Container>::find(const K& key)
    -> iterator {
  return const_cast_it(std::as_const(*this).find(key));
}

template <class Key, class GetKeyFromValue, class KeyCompare, class Container>
template <typename K>
auto flat_tree<Key, GetKeyFromValue, KeyCompare, Container>::find(
    const K& key) const -> const_iterator {
  auto eq_range = equal_range(key);
  return (eq_range.first == eq_range.second) ? end() : eq_range.first;
}

template <class Key, class GetKeyFromValue, class KeyCompare, class Container>
template <typename K>
bool flat_tree<Key, GetKeyFromValue, KeyCompare, Container>::contains(
    const K& key) const {
  auto lower = lower_bound(key);
  return lower != end() && !comp_(key, GetKeyFromValue()(*lower));
}

template <class Key, class GetKeyFromValue, class KeyCompare, class Container>
template <typename K>
auto flat_tree<Key, GetKeyFromValue, KeyCompare, Container>::equal_range(
    const K& key) -> std::pair<iterator, iterator> {
  auto res = std::as_const(*this).equal_range(key);
  return {const_cast_it(res.first), const_cast_it(res.second)};
}

template <class Key, class GetKeyFromValue, class KeyCompare, class Container>
template <typename K>
auto flat_tree<Key, GetKeyFromValue, KeyCompare, Container>::equal_range(
    const K& key) const -> std::pair<const_iterator, const_iterator> {
  auto lower = lower_bound(key);

  KeyValueCompare comp(comp_);
  if (lower == end() || comp(key, *lower))
    return {lower, lower};

  return {lower, std::next(lower)};
}

template <class Key, class GetKeyFromValue, class KeyCompare, class Container>
template <typename K>
auto flat_tree<Key, GetKeyFromValue, KeyCompare, Container>::lower_bound(
    const K& key) -> iterator {
  return const_cast_it(std::as_const(*this).lower_bound(key));
}

template <class Key, class GetKeyFromValue, class KeyCompare, class Container>
template <typename K>
auto flat_tree<Key, GetKeyFromValue, KeyCompare, Container>::lower_bound(
    const K& key) const -> const_iterator {
  static_assert(std::is_convertible<const KeyTypeOrK<K>&, const K&>::value,
                "Requested type cannot be bound to the container's key_type "
                "which is required for a non-transparent compare.");

  const KeyTypeOrK<K>& key_ref = key;

  KeyValueCompare comp(comp_);
  return absl::c_lower_bound(*this, key_ref, comp);
}

template <class Key, class GetKeyFromValue, class KeyCompare, class Container>
template <typename K>
auto flat_tree<Key, GetKeyFromValue, KeyCompare, Container>::upper_bound(
    const K& key) -> iterator {
  return const_cast_it(std::as_const(*this).upper_bound(key));
}

template <class Key, class GetKeyFromValue, class KeyCompare, class Container>
template <typename K>
auto flat_tree<Key, GetKeyFromValue, KeyCompare, Container>::upper_bound(
    const K& key) const -> const_iterator {
  static_assert(std::is_convertible<const KeyTypeOrK<K>&, const K&>::value,
                "Requested type cannot be bound to the container's key_type "
                "which is required for a non-transparent compare.");

  const KeyTypeOrK<K>& key_ref = key;

  KeyValueCompare comp(comp_);
  return absl::c_upper_bound(*this, key_ref, comp);
}

template <class Key, class GetKeyFromValue, class KeyCompare, class Container>
void flat_tree<Key, GetKeyFromValue, KeyCompare, Container>::swap(
    flat_tree& other) noexcept {
  std::swap(*this, other);
}

template <class Key, class GetKeyFromValue, class KeyCompare, class Container>
template <class... Args>
auto flat_tree<Key, GetKeyFromValue, KeyCompare, Container>::unsafe_emplace(
    const_iterator position,
    Args&&... args) -> iterator {
  return body_.emplace(position, std::forward<Args>(args)...);
}

template <class Key, class GetKeyFromValue, class KeyCompare, class Container>
template <class K, class... Args>
auto flat_tree<Key, GetKeyFromValue, KeyCompare, Container>::emplace_key_args(
    const K& key,
    Args&&... args) -> std::pair<iterator, bool> {
  auto lower = lower_bound(key);
  if (lower == end() || comp_(key, GetKeyFromValue()(*lower)))
    return {unsafe_emplace(lower, std::forward<Args>(args)...), true};
  return {lower, false};
}

template <class Key, class GetKeyFromValue, class KeyCompare, class Container>
template <class K, class... Args>
auto flat_tree<Key, GetKeyFromValue, KeyCompare, Container>::
    emplace_hint_key_args(const_iterator hint, const K& key, Args&&... args)
        -> std::pair<iterator, bool> {
  KeyValueCompare comp(comp_);
  if ((hint == begin() || comp(*std::prev(hint), key))) {
    if (hint == end() || comp(key, *hint)) {

      return {unsafe_emplace(hint, std::forward<Args>(args)...), true};
    }
    if (!comp(*hint, key)) {

      return {const_cast_it(hint), false};
    }
  }

  return emplace_key_args(key, std::forward<Args>(args)...);
}

template <class Key,
          class GetKeyFromValue,
          class KeyCompare,
          class Container,
          typename Predicate>
size_t EraseIf(
    webrtc::flat_containers_internal::
        flat_tree<Key, GetKeyFromValue, KeyCompare, Container>& container,
    Predicate pred) {
  auto it = std::remove_if(container.begin(), container.end(),
                           std::forward<Predicate>(pred));
  size_t removed = std::distance(it, container.end());
  container.erase(it, container.end());
  return removed;
}

}
}

#endif
