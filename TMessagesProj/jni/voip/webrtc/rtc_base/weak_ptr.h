/*
 *  Copyright 2016 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_WEAK_PTR_H_
#define RTC_BASE_WEAK_PTR_H_

#include <memory>
#include <utility>

#include "api/scoped_refptr.h"
#include "api/sequence_checker.h"
#include "rtc_base/ref_count.h"
#include "rtc_base/ref_counted_object.h"
#include "rtc_base/system/no_unique_address.h"

namespace rtc {

namespace internal {

class WeakReference {
 public:

  class Flag : public RefCountInterface {
   public:
    Flag();

    void Invalidate();
    bool IsValid() const;

   private:
    friend class RefCountedObject<Flag>;

    ~Flag() override;

    RTC_NO_UNIQUE_ADDRESS ::webrtc::SequenceChecker checker_{
        webrtc::SequenceChecker::kDetached};
    bool is_valid_;
  };

  WeakReference();
  explicit WeakReference(const Flag* flag);
  ~WeakReference();

  WeakReference(WeakReference&& other);
  WeakReference(const WeakReference& other);
  WeakReference& operator=(WeakReference&& other) = default;
  WeakReference& operator=(const WeakReference& other) = default;

  bool is_valid() const;

 private:
  scoped_refptr<const Flag> flag_;
};

class WeakReferenceOwner {
 public:
  WeakReferenceOwner();
  ~WeakReferenceOwner();

  WeakReference GetRef() const;

  bool HasRefs() const { return flag_.get() && !flag_->HasOneRef(); }

  void Invalidate();

 private:
  mutable scoped_refptr<RefCountedObject<WeakReference::Flag>> flag_;
};

class WeakPtrBase {
 public:
  WeakPtrBase();
  ~WeakPtrBase();

  WeakPtrBase(const WeakPtrBase& other) = default;
  WeakPtrBase(WeakPtrBase&& other) = default;
  WeakPtrBase& operator=(const WeakPtrBase& other) = default;
  WeakPtrBase& operator=(WeakPtrBase&& other) = default;

 protected:
  explicit WeakPtrBase(const WeakReference& ref);

  WeakReference ref_;
};

}

template <typename T>
class WeakPtrFactory;

template <typename T>
class WeakPtr : public internal::WeakPtrBase {
 public:
  WeakPtr() : ptr_(nullptr) {}

  template <typename U>
  WeakPtr(const WeakPtr<U>& other)
      : internal::WeakPtrBase(other), ptr_(other.ptr_) {}
  template <typename U>
  WeakPtr(WeakPtr<U>&& other)
      : internal::WeakPtrBase(std::move(other)), ptr_(other.ptr_) {}

  T* get() const { return ref_.is_valid() ? ptr_ : nullptr; }

  T& operator*() const {
    RTC_DCHECK(get() != nullptr);
    return *get();
  }
  T* operator->() const {
    RTC_DCHECK(get() != nullptr);
    return get();
  }

  void reset() {
    ref_ = internal::WeakReference();
    ptr_ = nullptr;
  }

  explicit operator bool() const { return get() != nullptr; }

 private:
  template <typename U>
  friend class WeakPtr;
  friend class WeakPtrFactory<T>;

  WeakPtr(const internal::WeakReference& ref, T* ptr)
      : internal::WeakPtrBase(ref), ptr_(ptr) {}

  T* ptr_;
};

template <class T>
bool operator!=(const WeakPtr<T>& weak_ptr, std::nullptr_t) {
  return !(weak_ptr == nullptr);
}
template <class T>
bool operator!=(std::nullptr_t, const WeakPtr<T>& weak_ptr) {
  return weak_ptr != nullptr;
}
template <class T>
bool operator==(const WeakPtr<T>& weak_ptr, std::nullptr_t) {
  return weak_ptr.get() == nullptr;
}
template <class T>
bool operator==(std::nullptr_t, const WeakPtr<T>& weak_ptr) {
  return weak_ptr == nullptr;
}

template <class T>
class WeakPtrFactory {
 public:
  explicit WeakPtrFactory(T* ptr) : ptr_(ptr) {}

  WeakPtrFactory() = delete;
  WeakPtrFactory(const WeakPtrFactory&) = delete;
  WeakPtrFactory& operator=(const WeakPtrFactory&) = delete;

  ~WeakPtrFactory() { ptr_ = nullptr; }

  WeakPtr<T> GetWeakPtr() {
    RTC_DCHECK(ptr_);
    return WeakPtr<T>(weak_reference_owner_.GetRef(), ptr_);
  }

  void InvalidateWeakPtrs() {
    RTC_DCHECK(ptr_);
    weak_reference_owner_.Invalidate();
  }

  bool HasWeakPtrs() const {
    RTC_DCHECK(ptr_);
    return weak_reference_owner_.HasRefs();
  }

 private:
  internal::WeakReferenceOwner weak_reference_owner_;
  T* ptr_;
};

}

#endif
