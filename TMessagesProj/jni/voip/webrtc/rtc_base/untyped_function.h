/*
 *  Copyright 2020 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_UNTYPED_FUNCTION_H_
#define RTC_BASE_UNTYPED_FUNCTION_H_

#include <cstddef>
#include <cstring>
#include <memory>
#include <type_traits>
#include <utility>

#include "rtc_base/system/assume.h"

namespace webrtc {
namespace webrtc_function_impl {

using FunVoid = void();

enum : size_t { kInlineStorageWords = 4 };

union VoidUnion {
  void* void_ptr;
  FunVoid* fun_ptr;
  typename std::aligned_storage<kInlineStorageWords * sizeof(uintptr_t)>::type
      inline_storage;
};

template <typename T>
constexpr size_t InlineStorageSize() {

  return (sizeof(T) + sizeof(uintptr_t) - 1) / sizeof(uintptr_t);
}

template <typename T>
struct CallHelpers;
template <typename RetT, typename... ArgT>
struct CallHelpers<RetT(ArgT...)> {

  using return_type = RetT;

  using function_type = RetT(VoidUnion*, ArgT...);

  template <typename F>
  static RetT CallVoidPtr(VoidUnion* vu, ArgT... args) {
    return (*static_cast<F*>(vu->void_ptr))(std::forward<ArgT>(args)...);
  }

  static RetT CallFunPtr(VoidUnion* vu, ArgT... args) {
    return (reinterpret_cast<RetT (*)(ArgT...)>(vu->fun_ptr))(
        std::forward<ArgT>(args)...);
  }

  template <typename F>
  static RetT CallInlineStorage(VoidUnion* vu, ArgT... args) {
    return (*reinterpret_cast<F*>(&vu->inline_storage))(
        std::forward<ArgT>(args)...);
  }
};

}

class UntypedFunction final {
 public:

  enum : size_t {
    kInlineStorageSize = sizeof(webrtc_function_impl::VoidUnion::inline_storage)
  };
  static_assert(kInlineStorageSize ==
                    webrtc_function_impl::kInlineStorageWords *
                        sizeof(uintptr_t),
                "");

  template <size_t N>
  struct TrivialUntypedFunctionArgs {
    static_assert(N >= 1, "");
    static_assert(N <= webrtc_function_impl::kInlineStorageWords, "");

    alignas(std::max_align_t) uintptr_t inline_storage[N];
    webrtc_function_impl::FunVoid* call;
  };
  struct NontrivialUntypedFunctionArgs {
    void* void_ptr;
    webrtc_function_impl::FunVoid* call;
    void (*del)(webrtc_function_impl::VoidUnion*);
  };
  struct FunctionPointerUntypedFunctionArgs {
    webrtc_function_impl::FunVoid* fun_ptr;
    webrtc_function_impl::FunVoid* call;
  };

  template <
      typename Signature,
      typename F,
      typename F_deref = typename std::remove_reference<F>::type,
      typename std::enable_if<

          !std::is_function<
              typename std::remove_pointer<F_deref>::type>::value &&

          !std::is_same<std::nullptr_t,
                        typename std::remove_cv<F>::type>::value &&

          !std::is_same<UntypedFunction,
                        typename std::remove_cv<F_deref>::type>::value &&

          std::is_trivially_move_constructible<F_deref>::value &&
          std::is_trivially_destructible<F_deref>::value &&
          sizeof(F_deref) <= kInlineStorageSize>::type* = nullptr,
      size_t InlineSize = webrtc_function_impl::InlineStorageSize<F_deref>()>
  static TrivialUntypedFunctionArgs<InlineSize> PrepareArgs(F&& f) {

    TrivialUntypedFunctionArgs<InlineSize> args;
    new (&args.inline_storage) F_deref(std::forward<F>(f));
    args.call = reinterpret_cast<webrtc_function_impl::FunVoid*>(
        webrtc_function_impl::CallHelpers<
            Signature>::template CallInlineStorage<F_deref>);
    return args;
  }
  template <size_t InlineSize>
  static UntypedFunction Create(TrivialUntypedFunctionArgs<InlineSize> args) {
    webrtc_function_impl::VoidUnion vu;
    std::memcpy(&vu.inline_storage, args.inline_storage,
                sizeof(args.inline_storage));
    return UntypedFunction(vu, args.call, nullptr);
  }

  template <typename Signature,
            typename F,
            typename F_deref = typename std::remove_reference<F>::type,
            typename std::enable_if<

                !std::is_function<
                    typename std::remove_pointer<F_deref>::type>::value &&

                !std::is_same<std::nullptr_t,
                              typename std::remove_cv<F>::type>::value &&

                !std::is_same<UntypedFunction,
                              typename std::remove_cv<F_deref>::type>::value &&

                !(std::is_trivially_move_constructible<F_deref>::value &&
                  std::is_trivially_destructible<F_deref>::value &&
                  sizeof(F_deref) <= kInlineStorageSize)>::type* = nullptr>
  static NontrivialUntypedFunctionArgs PrepareArgs(F&& f) {

    NontrivialUntypedFunctionArgs args;
    args.void_ptr = new F_deref(std::forward<F>(f));
    args.call = reinterpret_cast<webrtc_function_impl::FunVoid*>(
        webrtc_function_impl::CallHelpers<Signature>::template CallVoidPtr<
            F_deref>);
    args.del = static_cast<void (*)(webrtc_function_impl::VoidUnion*)>(
        [](webrtc_function_impl::VoidUnion* vu) {

          RTC_ASSUME(vu->void_ptr != nullptr);
          delete reinterpret_cast<F_deref*>(vu->void_ptr);
        });
    return args;
  }
  static UntypedFunction Create(NontrivialUntypedFunctionArgs args) {
    webrtc_function_impl::VoidUnion vu;
    vu.void_ptr = args.void_ptr;
    return UntypedFunction(vu, args.call, args.del);
  }

  template <typename Signature>
  static FunctionPointerUntypedFunctionArgs PrepareArgs(Signature* f) {
    FunctionPointerUntypedFunctionArgs args;
    args.fun_ptr = reinterpret_cast<webrtc_function_impl::FunVoid*>(f);
    args.call = reinterpret_cast<webrtc_function_impl::FunVoid*>(
        webrtc_function_impl::CallHelpers<Signature>::CallFunPtr);
    return args;
  }
  static UntypedFunction Create(FunctionPointerUntypedFunctionArgs args) {
    webrtc_function_impl::VoidUnion vu;
    vu.fun_ptr = args.fun_ptr;
    return UntypedFunction(vu, args.fun_ptr == nullptr ? nullptr : args.call,
                           nullptr);
  }

  template <typename Signature, typename F>
  static UntypedFunction Create(F&& f) {
    return Create(PrepareArgs<Signature>(std::forward<F>(f)));
  }

  UntypedFunction() : call_(nullptr), delete_(nullptr) {}

  UntypedFunction(std::nullptr_t)
      : call_(nullptr), delete_(nullptr) {}
  UntypedFunction& operator=(std::nullptr_t) {
    call_ = nullptr;
    if (delete_) {
      delete_(&f_);
      delete_ = nullptr;
    }
    return *this;
  }

  UntypedFunction(const UntypedFunction&) = delete;
  UntypedFunction& operator=(const UntypedFunction&) = delete;

  UntypedFunction(UntypedFunction&& other)
      : f_(other.f_), call_(other.call_), delete_(other.delete_) {
    other.delete_ = nullptr;
  }
  UntypedFunction& operator=(UntypedFunction&& other) {
    if (delete_) {
      delete_(&f_);
    }
    f_ = other.f_;
    call_ = other.call_;
    delete_ = other.delete_;
    other.delete_ = nullptr;
    return *this;
  }

  ~UntypedFunction() {
    if (delete_) {
      delete_(&f_);
    }
  }

  friend void swap(UntypedFunction& a, UntypedFunction& b) {
    using std::swap;
    swap(a.f_, b.f_);
    swap(a.call_, b.call_);
    swap(a.delete_, b.delete_);
  }

  explicit operator bool() const { return call_ != nullptr; }

  template <typename Signature, typename... ArgT>
  typename webrtc_function_impl::CallHelpers<Signature>::return_type Call(
      ArgT&&... args) {
    return reinterpret_cast<
        typename webrtc_function_impl::CallHelpers<Signature>::function_type*>(
        call_)(&f_, std::forward<ArgT>(args)...);
  }

  bool IsTriviallyDestructible() { return delete_ == nullptr; }

 private:
  UntypedFunction(webrtc_function_impl::VoidUnion f,
                  webrtc_function_impl::FunVoid* call,
                  void (*del)(webrtc_function_impl::VoidUnion*))
      : f_(f), call_(call), delete_(del) {}

  webrtc_function_impl::VoidUnion f_;

  webrtc_function_impl::FunVoid* call_;

  void (*delete_)(webrtc_function_impl::VoidUnion*);
};

}

#endif
