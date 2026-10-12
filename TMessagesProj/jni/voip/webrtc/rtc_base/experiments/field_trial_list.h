/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef RTC_BASE_EXPERIMENTS_FIELD_TRIAL_LIST_H_
#define RTC_BASE_EXPERIMENTS_FIELD_TRIAL_LIST_H_

#include <initializer_list>
#include <memory>
#include <string>
#include <vector>

#include "absl/strings/string_view.h"
#include "rtc_base/experiments/field_trial_parser.h"
#include "rtc_base/string_encode.h"

namespace webrtc {

class FieldTrialListBase : public FieldTrialParameterInterface {
 protected:
  friend class FieldTrialListWrapper;
  explicit FieldTrialListBase(absl::string_view key);

  bool Failed() const;
  bool Used() const;

  virtual int Size() = 0;

  bool failed_;
  bool parse_got_called_;
};

template <typename T>
class FieldTrialList : public FieldTrialListBase {
 public:
  explicit FieldTrialList(absl::string_view key) : FieldTrialList(key, {}) {}
  FieldTrialList(absl::string_view key, std::initializer_list<T> default_values)
      : FieldTrialListBase(key), values_(default_values) {}

  std::vector<T> Get() const { return values_; }
  operator std::vector<T>() const { return Get(); }
  typename std::vector<T>::const_reference operator[](size_t index) const {
    return values_[index];
  }
  const std::vector<T>* operator->() const { return &values_; }

 protected:
  bool Parse(absl::optional<std::string> str_value) override {
    parse_got_called_ = true;

    if (!str_value) {
      values_.clear();
      return true;
    }

    std::vector<T> new_values_;

    for (const absl::string_view token : rtc::split(str_value.value(), '|')) {
      absl::optional<T> value = ParseTypedParameter<T>(token);
      if (value) {
        new_values_.push_back(*value);
      } else {
        failed_ = true;
        return false;
      }
    }

    values_.swap(new_values_);
    return true;
  }

  int Size() override { return values_.size(); }

 private:
  std::vector<T> values_;
};

class FieldTrialListWrapper {
 public:
  virtual ~FieldTrialListWrapper() = default;

  virtual void WriteElement(void* struct_to_write, int index) = 0;

  virtual FieldTrialListBase* GetList() = 0;

  int Length();

  bool Failed();

  bool Used();

 protected:
  FieldTrialListWrapper() = default;
};

namespace field_trial_list_impl {

template <typename T>
struct LambdaTypeTraits : public LambdaTypeTraits<decltype(&T::operator())> {};

template <typename ClassType, typename RetType, typename SourceType>
struct LambdaTypeTraits<RetType* (ClassType::*)(SourceType*) const> {
  using ret = RetType;
  using src = SourceType;
};

template <typename T>
struct TypedFieldTrialListWrapper : FieldTrialListWrapper {
 public:
  TypedFieldTrialListWrapper(absl::string_view key,
                             std::function<void(void*, T)> sink)
      : list_(key), sink_(sink) {}

  void WriteElement(void* struct_to_write, int index) override {
    sink_(struct_to_write, list_[index]);
  }

  FieldTrialListBase* GetList() override { return &list_; }

 private:
  FieldTrialList<T> list_;
  std::function<void(void*, T)> sink_;
};

}

template <typename F,
          typename Traits = typename field_trial_list_impl::LambdaTypeTraits<F>>
FieldTrialListWrapper* FieldTrialStructMember(absl::string_view key,
                                              F accessor) {
  return new field_trial_list_impl::TypedFieldTrialListWrapper<
      typename Traits::ret>(key, [accessor](void* s, typename Traits::ret t) {
    *accessor(static_cast<typename Traits::src*>(s)) = t;
  });
}

class FieldTrialStructListBase : public FieldTrialParameterInterface {
 protected:
  FieldTrialStructListBase(
      std::initializer_list<FieldTrialListWrapper*> sub_lists)
      : FieldTrialParameterInterface(""), sub_lists_() {

    for (FieldTrialListWrapper* const* it = sub_lists.begin();
         it != sub_lists.end(); it++) {
      sub_parameters_.push_back((*it)->GetList());
      sub_lists_.push_back(std::unique_ptr<FieldTrialListWrapper>(*it));
    }
  }

  int ValidateAndGetLength();

  bool Parse(absl::optional<std::string> str_value) override;

  std::vector<std::unique_ptr<FieldTrialListWrapper>> sub_lists_;
};

template <typename S>
class FieldTrialStructList : public FieldTrialStructListBase {
 public:
  FieldTrialStructList(std::initializer_list<FieldTrialListWrapper*> l,
                       std::initializer_list<S> default_list)
      : FieldTrialStructListBase(l), values_(default_list) {}

  std::vector<S> Get() const { return values_; }
  operator std::vector<S>() const { return Get(); }
  const S& operator[](size_t index) const { return values_[index]; }
  const std::vector<S>* operator->() const { return &values_; }

 protected:
  void ParseDone() override {
    int length = ValidateAndGetLength();

    if (length == -1)
      return;

    std::vector<S> new_values(length, S());

    for (std::unique_ptr<FieldTrialListWrapper>& li : sub_lists_) {
      if (li->Used()) {
        for (int i = 0; i < length; i++) {
          li->WriteElement(&new_values[i], i);
        }
      }
    }

    values_.swap(new_values);
  }

 private:
  std::vector<S> values_;
};

}

#endif
