/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef LOGGING_RTC_EVENT_LOG_RTC_EVENT_PROCESSOR_H_
#define LOGGING_RTC_EVENT_LOG_RTC_EVENT_PROCESSOR_H_

#include <stdint.h>

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

#include "api/function_view.h"
#include "logging/rtc_event_log/rtc_event_processor_order.h"
#include "rtc_base/checks.h"

namespace webrtc {

namespace event_processor_impl {

class ProcessableEventListInterface {
 public:
  virtual ~ProcessableEventListInterface() = default;
  virtual void ProcessNext() = 0;
  virtual bool IsEmpty() const = 0;
  virtual int64_t GetNextTime() const = 0;
  virtual int GetTypeOrder() const = 0;
  virtual absl::optional<uint16_t> GetTransportSeqNum() const = 0;
  virtual int GetInsertionOrder() const = 0;
};

template <typename Iterator, typename T>
class ProcessableEventList : public ProcessableEventListInterface {
 public:
  ProcessableEventList(Iterator begin,
                       Iterator end,
                       std::function<void(const T&)> f,
                       int type_order,
                       std::function<absl::optional<uint16_t>(const T&)>
                           transport_seq_num_accessor,
                       int insertion_order)
      : begin_(begin),
        end_(end),
        f_(f),
        type_order_(type_order),
        transport_seq_num_accessor_(transport_seq_num_accessor),
        insertion_order_(insertion_order) {}

  void ProcessNext() override {
    RTC_DCHECK(!IsEmpty());
    f_(*begin_);
    ++begin_;
  }

  bool IsEmpty() const override { return begin_ == end_; }

  int64_t GetNextTime() const override {
    RTC_DCHECK(!IsEmpty());
    return begin_->log_time_us();
  }

  int GetTypeOrder() const override { return type_order_; }

  absl::optional<uint16_t> GetTransportSeqNum() const override {
    RTC_DCHECK(!IsEmpty());
    return transport_seq_num_accessor_(*begin_);
  }

  int GetInsertionOrder() const override { return insertion_order_; }

 private:
  Iterator begin_;
  Iterator end_;
  std::function<void(const T&)> f_;
  int type_order_;
  std::function<absl::optional<uint16_t>(const T&)> transport_seq_num_accessor_;
  int insertion_order_;
};

}

class RtcEventProcessor {
 public:
  RtcEventProcessor();
  ~RtcEventProcessor();

  template <typename Iterable>
  void AddEvents(
      const Iterable& iterable,
      std::function<void(const typename Iterable::value_type&)> handler) {
    using ValueType =
        typename std::remove_const<typename Iterable::value_type>::type;
    AddEvents(iterable, handler, TieBreaker<ValueType>::type_order,
              TieBreaker<ValueType>::transport_seq_num_accessor,
              num_insertions_);
  }

  template <typename Iterable>
  void AddEvents(
      const Iterable& iterable,
      std::function<void(const typename Iterable::value_type&)> handler,
      PacketDirection direction) {
    using ValueType =
        typename std::remove_const<typename Iterable::value_type>::type;
    AddEvents(iterable, handler, TieBreaker<ValueType>::type_order(direction),
              TieBreaker<ValueType>::transport_seq_num_accessor,
              num_insertions_);
  }

  template <typename Iterable>
  void AddEvents(
      const Iterable& iterable,
      std::function<void(const typename Iterable::value_type&)> handler,
      int type_order,
      std::function<absl::optional<uint16_t>(
          const typename Iterable::value_type&)> transport_seq_num_accessor,
      int insertion_order) {
    if (iterable.begin() == iterable.end())
      return;
    num_insertions_++;
    event_lists_.push_back(
        std::make_unique<event_processor_impl::ProcessableEventList<
            typename Iterable::const_iterator, typename Iterable::value_type>>(
            iterable.begin(), iterable.end(), handler, type_order,
            transport_seq_num_accessor, insertion_order));
  }

  void ProcessEventsInOrder();

 private:
  using ListPtrType =
      std::unique_ptr<event_processor_impl::ProcessableEventListInterface>;

  static bool Cmp(const ListPtrType& a, const ListPtrType& b);

  std::vector<ListPtrType> event_lists_;
  int num_insertions_ = 0;
};

}

#endif
