// Copyright 2007, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// The Google C++ Testing and Mocking Framework (Google Test)
//
// This file implements just enough of the matcher interface to allow
// EXPECT_DEATH and friends to accept a matcher argument.

// IWYU pragma: private, include "gtest/gtest.h"
// IWYU pragma: friend gtest/.*
// IWYU pragma: friend gmock/.*

#ifndef GOOGLETEST_INCLUDE_GTEST_GTEST_MATCHERS_H_
#define GOOGLETEST_INCLUDE_GTEST_GTEST_MATCHERS_H_

#include <atomic>
#include <functional>
#include <memory>
#include <ostream>
#include <string>
#include <type_traits>

#include "gtest/gtest-printers.h"
#include "gtest/internal/gtest-internal.h"
#include "gtest/internal/gtest-port.h"

#if defined(_MSC_VER) && _MSC_VER >= 1915
#define GTEST_MAYBE_5046_ 5046
#else
#define GTEST_MAYBE_5046_
#endif

GTEST_DISABLE_MSC_WARNINGS_PUSH_(
    4251 GTEST_MAYBE_5046_

                                                                 )

namespace testing {

class MatchResultListener {
 public:

  explicit MatchResultListener(::std::ostream* os) : stream_(os) {}
  virtual ~MatchResultListener() = 0;

  template <typename T>
  MatchResultListener& operator<<(const T& x) {
    if (stream_ != nullptr) *stream_ << x;
    return *this;
  }

  ::std::ostream* stream() { return stream_; }

  bool IsInterested() const { return stream_ != nullptr; }

 private:
  ::std::ostream* const stream_;

  MatchResultListener(const MatchResultListener&) = delete;
  MatchResultListener& operator=(const MatchResultListener&) = delete;
};

inline MatchResultListener::~MatchResultListener() = default;

class GTEST_API_ MatcherDescriberInterface {
 public:
  virtual ~MatcherDescriberInterface() = default;

  virtual void DescribeTo(::std::ostream* os) const = 0;

  virtual void DescribeNegationTo(::std::ostream* os) const {
    *os << "not (";
    DescribeTo(os);
    *os << ")";
  }
};

template <typename T>
class MatcherInterface : public MatcherDescriberInterface {
 public:

  virtual bool MatchAndExplain(T x, MatchResultListener* listener) const = 0;

};

namespace internal {

class DummyMatchResultListener : public MatchResultListener {
 public:
  DummyMatchResultListener() : MatchResultListener(nullptr) {}

 private:
  DummyMatchResultListener(const DummyMatchResultListener&) = delete;
  DummyMatchResultListener& operator=(const DummyMatchResultListener&) = delete;
};

class StreamMatchResultListener : public MatchResultListener {
 public:
  explicit StreamMatchResultListener(::std::ostream* os)
      : MatchResultListener(os) {}

 private:
  StreamMatchResultListener(const StreamMatchResultListener&) = delete;
  StreamMatchResultListener& operator=(const StreamMatchResultListener&) =
      delete;
};

struct SharedPayloadBase {
  std::atomic<int> ref{1};
  void Ref() { ref.fetch_add(1, std::memory_order_relaxed); }
  bool Unref() { return ref.fetch_sub(1, std::memory_order_acq_rel) == 1; }
};

template <typename T>
struct SharedPayload : SharedPayloadBase {
  explicit SharedPayload(const T& v) : value(v) {}
  explicit SharedPayload(T&& v) : value(std::move(v)) {}

  static void Destroy(SharedPayloadBase* shared) {
    delete static_cast<SharedPayload*>(shared);
  }

  T value;
};

template <typename T>
class MatcherBase : private MatcherDescriberInterface {
 public:

  bool MatchAndExplain(const T& x, MatchResultListener* listener) const {
    GTEST_CHECK_(vtable_ != nullptr);
    return vtable_->match_and_explain(*this, x, listener);
  }

  bool Matches(const T& x) const {
    DummyMatchResultListener dummy;
    return MatchAndExplain(x, &dummy);
  }

  void DescribeTo(::std::ostream* os) const final {
    GTEST_CHECK_(vtable_ != nullptr);
    vtable_->describe(*this, os, false);
  }

  void DescribeNegationTo(::std::ostream* os) const final {
    GTEST_CHECK_(vtable_ != nullptr);
    vtable_->describe(*this, os, true);
  }

  void ExplainMatchResultTo(const T& x, ::std::ostream* os) const {
    StreamMatchResultListener listener(os);
    MatchAndExplain(x, &listener);
  }

  const MatcherDescriberInterface* GetDescriber() const {
    if (vtable_ == nullptr) return nullptr;
    return vtable_->get_describer(*this);
  }

 protected:
  MatcherBase() : vtable_(nullptr), buffer_() {}

  template <typename U>
  explicit MatcherBase(const MatcherInterface<U>* impl)
      : vtable_(nullptr), buffer_() {
    Init(impl);
  }

  template <typename M, typename = typename std::remove_reference<
                            M>::type::is_gtest_matcher>
  MatcherBase(M&& m) : vtable_(nullptr), buffer_() {
    Init(std::forward<M>(m));
  }

  MatcherBase(const MatcherBase& other)
      : vtable_(other.vtable_), buffer_(other.buffer_) {
    if (IsShared()) buffer_.shared->Ref();
  }

  MatcherBase& operator=(const MatcherBase& other) {
    if (this == &other) return *this;
    Destroy();
    vtable_ = other.vtable_;
    buffer_ = other.buffer_;
    if (IsShared()) buffer_.shared->Ref();
    return *this;
  }

  MatcherBase(MatcherBase&& other)
      : vtable_(other.vtable_), buffer_(other.buffer_) {
    other.vtable_ = nullptr;
  }

  MatcherBase& operator=(MatcherBase&& other) {
    if (this == &other) return *this;
    Destroy();
    vtable_ = other.vtable_;
    buffer_ = other.buffer_;
    other.vtable_ = nullptr;
    return *this;
  }

  ~MatcherBase() override { Destroy(); }

 private:
  struct VTable {
    bool (*match_and_explain)(const MatcherBase&, const T&,
                              MatchResultListener*);
    void (*describe)(const MatcherBase&, std::ostream*, bool negation);

    const MatcherDescriberInterface* (*get_describer)(const MatcherBase&);

    void (*shared_destroy)(SharedPayloadBase*);
  };

  bool IsShared() const {
    return vtable_ != nullptr && vtable_->shared_destroy != nullptr;
  }

  template <typename P>
  static auto MatchAndExplainImpl(const MatcherBase& m, const T& value,
                                  MatchResultListener* listener)
      -> decltype(P::Get(m).MatchAndExplain(value, listener->stream())) {
    return P::Get(m).MatchAndExplain(value, listener->stream());
  }

  template <typename P>
  static auto MatchAndExplainImpl(const MatcherBase& m, const T& value,
                                  MatchResultListener* listener)
      -> decltype(P::Get(m).MatchAndExplain(value, listener)) {
    return P::Get(m).MatchAndExplain(value, listener);
  }

  template <typename P>
  static void DescribeImpl(const MatcherBase& m, std::ostream* os,
                           bool negation) {
    if (negation) {
      P::Get(m).DescribeNegationTo(os);
    } else {
      P::Get(m).DescribeTo(os);
    }
  }

  template <typename P>
  static const MatcherDescriberInterface* GetDescriberImpl(
      const MatcherBase& m) {

    return std::get<(
        std::is_convertible<decltype(&P::Get(m)),
                            const MatcherDescriberInterface*>::value
            ? 1
            : 0)>(std::make_tuple(&m, &P::Get(m)));
  }

  template <typename P>
  const VTable* GetVTable() {
    static constexpr VTable kVTable = {&MatchAndExplainImpl<P>,
                                       &DescribeImpl<P>, &GetDescriberImpl<P>,
                                       P::shared_destroy};
    return &kVTable;
  }

  union Buffer {

    void* ptr;
    double d;
    int64_t i;

    SharedPayloadBase* shared;
  };

  void Destroy() {
    if (IsShared() && buffer_.shared->Unref()) {
      vtable_->shared_destroy(buffer_.shared);
    }
  }

  template <typename M>
  static constexpr bool IsInlined() {
    return sizeof(M) <= sizeof(Buffer) && alignof(M) <= alignof(Buffer) &&
           std::is_trivially_copy_constructible<M>::value &&
           std::is_trivially_destructible<M>::value;
  }

  template <typename M, bool = MatcherBase::IsInlined<M>()>
  struct ValuePolicy {
    static const M& Get(const MatcherBase& m) {

      const M* ptr =
          static_cast<const M*>(static_cast<const void*>(&m.buffer_));
      return *ptr;
    }
    static void Init(MatcherBase& m, M impl) {
      ::new (static_cast<void*>(&m.buffer_)) M(impl);
    }
    static constexpr auto shared_destroy = nullptr;
  };

  template <typename M>
  struct ValuePolicy<M, false> {
    using Shared = SharedPayload<M>;
    static const M& Get(const MatcherBase& m) {
      return static_cast<Shared*>(m.buffer_.shared)->value;
    }
    template <typename Arg>
    static void Init(MatcherBase& m, Arg&& arg) {
      m.buffer_.shared = new Shared(std::forward<Arg>(arg));
    }
    static constexpr auto shared_destroy = &Shared::Destroy;
  };

  template <typename U, bool B>
  struct ValuePolicy<const MatcherInterface<U>*, B> {
    using M = const MatcherInterface<U>;
    using Shared = SharedPayload<std::unique_ptr<M>>;
    static const M& Get(const MatcherBase& m) {
      return *static_cast<Shared*>(m.buffer_.shared)->value;
    }
    static void Init(MatcherBase& m, M* impl) {
      m.buffer_.shared = new Shared(std::unique_ptr<M>(impl));
    }

    static constexpr auto shared_destroy = &Shared::Destroy;
  };

  template <typename M>
  void Init(M&& m) {
    using MM = typename std::decay<M>::type;
    using Policy = ValuePolicy<MM>;
    vtable_ = GetVTable<Policy>();
    Policy::Init(*this, std::forward<M>(m));
  }

  const VTable* vtable_;
  Buffer buffer_;
};

}

template <typename T>
class Matcher : public internal::MatcherBase<T> {
 public:

  explicit Matcher() {}

  explicit Matcher(const MatcherInterface<const T&>* impl)
      : internal::MatcherBase<T>(impl) {}

  template <typename U>
  explicit Matcher(
      const MatcherInterface<U>* impl,
      typename std::enable_if<!std::is_same<U, const U&>::value>::type* =
          nullptr)
      : internal::MatcherBase<T>(impl) {}

  template <typename M, typename = typename std::remove_reference<
                            M>::type::is_gtest_matcher>
  Matcher(M&& m) : internal::MatcherBase<T>(std::forward<M>(m)) {}

  Matcher(T value);
};

template <>
class GTEST_API_ Matcher<const std::string&>
    : public internal::MatcherBase<const std::string&> {
 public:
  Matcher() = default;

  explicit Matcher(const MatcherInterface<const std::string&>* impl)
      : internal::MatcherBase<const std::string&>(impl) {}

  template <typename M, typename = typename std::remove_reference<
                            M>::type::is_gtest_matcher>
  Matcher(M&& m)
      : internal::MatcherBase<const std::string&>(std::forward<M>(m)) {}

  Matcher(const std::string& s);

  Matcher(const char* s);
};

template <>
class GTEST_API_ Matcher<std::string>
    : public internal::MatcherBase<std::string> {
 public:
  Matcher() = default;

  explicit Matcher(const MatcherInterface<const std::string&>* impl)
      : internal::MatcherBase<std::string>(impl) {}
  explicit Matcher(const MatcherInterface<std::string>* impl)
      : internal::MatcherBase<std::string>(impl) {}

  template <typename M, typename = typename std::remove_reference<
                            M>::type::is_gtest_matcher>
  Matcher(M&& m)
      : internal::MatcherBase<std::string>(std::forward<M>(m)) {}

  Matcher(const std::string& s);

  Matcher(const char* s);
};

#if GTEST_INTERNAL_HAS_STRING_VIEW

template <>
class GTEST_API_ Matcher<const internal::StringView&>
    : public internal::MatcherBase<const internal::StringView&> {
 public:
  Matcher() = default;

  explicit Matcher(const MatcherInterface<const internal::StringView&>* impl)
      : internal::MatcherBase<const internal::StringView&>(impl) {}

  template <typename M, typename = typename std::remove_reference<
                            M>::type::is_gtest_matcher>
  Matcher(M&& m)
      : internal::MatcherBase<const internal::StringView&>(std::forward<M>(m)) {
  }

  Matcher(const std::string& s);

  Matcher(const char* s);

  Matcher(internal::StringView s);
};

template <>
class GTEST_API_ Matcher<internal::StringView>
    : public internal::MatcherBase<internal::StringView> {
 public:
  Matcher() = default;

  explicit Matcher(const MatcherInterface<const internal::StringView&>* impl)
      : internal::MatcherBase<internal::StringView>(impl) {}
  explicit Matcher(const MatcherInterface<internal::StringView>* impl)
      : internal::MatcherBase<internal::StringView>(impl) {}

  template <typename M, typename = typename std::remove_reference<
                            M>::type::is_gtest_matcher>
  Matcher(M&& m)
      : internal::MatcherBase<internal::StringView>(std::forward<M>(m)) {}

  Matcher(const std::string& s);

  Matcher(const char* s);

  Matcher(internal::StringView s);
};
#endif

template <typename T>
std::ostream& operator<<(std::ostream& os, const Matcher<T>& matcher) {
  matcher.DescribeTo(&os);
  return os;
}

template <class Impl>
class PolymorphicMatcher {
 public:
  explicit PolymorphicMatcher(const Impl& an_impl) : impl_(an_impl) {}

  Impl& mutable_impl() { return impl_; }

  const Impl& impl() const { return impl_; }

  template <typename T>
  operator Matcher<T>() const {
    return Matcher<T>(new MonomorphicImpl<const T&>(impl_));
  }

 private:
  template <typename T>
  class MonomorphicImpl : public MatcherInterface<T> {
   public:
    explicit MonomorphicImpl(const Impl& impl) : impl_(impl) {}

    void DescribeTo(::std::ostream* os) const override { impl_.DescribeTo(os); }

    void DescribeNegationTo(::std::ostream* os) const override {
      impl_.DescribeNegationTo(os);
    }

    bool MatchAndExplain(T x, MatchResultListener* listener) const override {
      return impl_.MatchAndExplain(x, listener);
    }

   private:
    const Impl impl_;
  };

  Impl impl_;
};

template <typename T>
inline Matcher<T> MakeMatcher(const MatcherInterface<T>* impl) {
  return Matcher<T>(impl);
}

template <class Impl>
inline PolymorphicMatcher<Impl> MakePolymorphicMatcher(const Impl& impl) {
  return PolymorphicMatcher<Impl>(impl);
}

namespace internal {

template <typename D, typename Rhs, typename Op>
class ComparisonBase {
 public:
  explicit ComparisonBase(const Rhs& rhs) : rhs_(rhs) {}

  using is_gtest_matcher = void;

  template <typename Lhs>
  bool MatchAndExplain(const Lhs& lhs, std::ostream*) const {
    return Op()(lhs, Unwrap(rhs_));
  }
  void DescribeTo(std::ostream* os) const {
    *os << D::Desc() << " ";
    UniversalPrint(Unwrap(rhs_), os);
  }
  void DescribeNegationTo(std::ostream* os) const {
    *os << D::NegatedDesc() << " ";
    UniversalPrint(Unwrap(rhs_), os);
  }

 private:
  template <typename T>
  static const T& Unwrap(const T& v) {
    return v;
  }
  template <typename T>
  static const T& Unwrap(std::reference_wrapper<T> v) {
    return v;
  }

  Rhs rhs_;
};

template <typename Rhs>
class EqMatcher : public ComparisonBase<EqMatcher<Rhs>, Rhs, std::equal_to<>> {
 public:
  explicit EqMatcher(const Rhs& rhs)
      : ComparisonBase<EqMatcher<Rhs>, Rhs, std::equal_to<>>(rhs) {}
  static const char* Desc() { return "is equal to"; }
  static const char* NegatedDesc() { return "isn't equal to"; }
};
template <typename Rhs>
class NeMatcher
    : public ComparisonBase<NeMatcher<Rhs>, Rhs, std::not_equal_to<>> {
 public:
  explicit NeMatcher(const Rhs& rhs)
      : ComparisonBase<NeMatcher<Rhs>, Rhs, std::not_equal_to<>>(rhs) {}
  static const char* Desc() { return "isn't equal to"; }
  static const char* NegatedDesc() { return "is equal to"; }
};
template <typename Rhs>
class LtMatcher : public ComparisonBase<LtMatcher<Rhs>, Rhs, std::less<>> {
 public:
  explicit LtMatcher(const Rhs& rhs)
      : ComparisonBase<LtMatcher<Rhs>, Rhs, std::less<>>(rhs) {}
  static const char* Desc() { return "is <"; }
  static const char* NegatedDesc() { return "isn't <"; }
};
template <typename Rhs>
class GtMatcher : public ComparisonBase<GtMatcher<Rhs>, Rhs, std::greater<>> {
 public:
  explicit GtMatcher(const Rhs& rhs)
      : ComparisonBase<GtMatcher<Rhs>, Rhs, std::greater<>>(rhs) {}
  static const char* Desc() { return "is >"; }
  static const char* NegatedDesc() { return "isn't >"; }
};
template <typename Rhs>
class LeMatcher
    : public ComparisonBase<LeMatcher<Rhs>, Rhs, std::less_equal<>> {
 public:
  explicit LeMatcher(const Rhs& rhs)
      : ComparisonBase<LeMatcher<Rhs>, Rhs, std::less_equal<>>(rhs) {}
  static const char* Desc() { return "is <="; }
  static const char* NegatedDesc() { return "isn't <="; }
};
template <typename Rhs>
class GeMatcher
    : public ComparisonBase<GeMatcher<Rhs>, Rhs, std::greater_equal<>> {
 public:
  explicit GeMatcher(const Rhs& rhs)
      : ComparisonBase<GeMatcher<Rhs>, Rhs, std::greater_equal<>>(rhs) {}
  static const char* Desc() { return "is >="; }
  static const char* NegatedDesc() { return "isn't >="; }
};

template <typename T, typename = typename std::enable_if<
                          std::is_constructible<std::string, T>::value>::type>
using StringLike = T;

class MatchesRegexMatcher {
 public:
  MatchesRegexMatcher(const RE* regex, bool full_match)
      : regex_(regex), full_match_(full_match) {}

#if GTEST_INTERNAL_HAS_STRING_VIEW
  bool MatchAndExplain(const internal::StringView& s,
                       MatchResultListener* listener) const {
    return MatchAndExplain(std::string(s), listener);
  }
#endif

  template <typename CharType>
  bool MatchAndExplain(CharType* s, MatchResultListener* listener) const {
    return s != nullptr && MatchAndExplain(std::string(s), listener);
  }

  template <class MatcheeStringType>
  bool MatchAndExplain(const MatcheeStringType& s,
                       MatchResultListener*               ) const {
    const std::string s2(s);
    return full_match_ ? RE::FullMatch(s2, *regex_)
                       : RE::PartialMatch(s2, *regex_);
  }

  void DescribeTo(::std::ostream* os) const {
    *os << (full_match_ ? "matches" : "contains") << " regular expression ";
    UniversalPrinter<std::string>::Print(regex_->pattern(), os);
  }

  void DescribeNegationTo(::std::ostream* os) const {
    *os << "doesn't " << (full_match_ ? "match" : "contain")
        << " regular expression ";
    UniversalPrinter<std::string>::Print(regex_->pattern(), os);
  }

 private:
  const std::shared_ptr<const RE> regex_;
  const bool full_match_;
};
}

inline PolymorphicMatcher<internal::MatchesRegexMatcher> MatchesRegex(
    const internal::RE* regex) {
  return MakePolymorphicMatcher(internal::MatchesRegexMatcher(regex, true));
}
template <typename T = std::string>
PolymorphicMatcher<internal::MatchesRegexMatcher> MatchesRegex(
    const internal::StringLike<T>& regex) {
  return MatchesRegex(new internal::RE(std::string(regex)));
}

inline PolymorphicMatcher<internal::MatchesRegexMatcher> ContainsRegex(
    const internal::RE* regex) {
  return MakePolymorphicMatcher(internal::MatchesRegexMatcher(regex, false));
}
template <typename T = std::string>
PolymorphicMatcher<internal::MatchesRegexMatcher> ContainsRegex(
    const internal::StringLike<T>& regex) {
  return ContainsRegex(new internal::RE(std::string(regex)));
}

template <typename T>
inline internal::EqMatcher<T> Eq(T x) {
  return internal::EqMatcher<T>(x);
}

template <typename T>
Matcher<T>::Matcher(T value) {
  *this = Eq(value);
}

template <typename Lhs, typename Rhs>
inline Matcher<Lhs> TypedEq(const Rhs& rhs) {
  return Eq(rhs);
}

template <typename Rhs>
inline internal::GeMatcher<Rhs> Ge(Rhs x) {
  return internal::GeMatcher<Rhs>(x);
}

template <typename Rhs>
inline internal::GtMatcher<Rhs> Gt(Rhs x) {
  return internal::GtMatcher<Rhs>(x);
}

template <typename Rhs>
inline internal::LeMatcher<Rhs> Le(Rhs x) {
  return internal::LeMatcher<Rhs>(x);
}

template <typename Rhs>
inline internal::LtMatcher<Rhs> Lt(Rhs x) {
  return internal::LtMatcher<Rhs>(x);
}

template <typename Rhs>
inline internal::NeMatcher<Rhs> Ne(Rhs x) {
  return internal::NeMatcher<Rhs>(x);
}
}

GTEST_DISABLE_MSC_WARNINGS_POP_()

#endif
