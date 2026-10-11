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

// Google Mock - a framework for writing C++ mock classes.
//
// This file implements the ON_CALL() and EXPECT_CALL() macros.
//
// A user can use the ON_CALL() macro to specify the default action of
// a mock method.  The syntax is:
//
//   ON_CALL(mock_object, Method(argument-matchers))
//       .With(multi-argument-matcher)
//       .WillByDefault(action);
//
//  where the .With() clause is optional.
//
// A user can use the EXPECT_CALL() macro to specify an expectation on
// a mock method.  The syntax is:
//
//   EXPECT_CALL(mock_object, Method(argument-matchers))
//       .With(multi-argument-matchers)
//       .Times(cardinality)
//       .InSequence(sequences)
//       .After(expectations)
//       .WillOnce(action)
//       .WillRepeatedly(action)
//       .RetiresOnSaturation();
//
// where all clauses are optional, and .InSequence()/.After()/
// .WillOnce() can appear any number of times.

// IWYU pragma: private, include "gmock/gmock.h"
// IWYU pragma: friend gmock/.*

#ifndef GOOGLEMOCK_INCLUDE_GMOCK_GMOCK_SPEC_BUILDERS_H_
#define GOOGLEMOCK_INCLUDE_GMOCK_GMOCK_SPEC_BUILDERS_H_

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <ostream>
#include <set>
#include <sstream>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "gmock/gmock-actions.h"
#include "gmock/gmock-cardinalities.h"
#include "gmock/gmock-matchers.h"
#include "gmock/internal/gmock-internal-utils.h"
#include "gmock/internal/gmock-port.h"
#include "gtest/gtest.h"

#if GTEST_HAS_EXCEPTIONS
#include <stdexcept>
#endif

GTEST_DISABLE_MSC_WARNINGS_PUSH_(4251 \
                                                                          )

namespace testing {

class Expectation;

class ExpectationSet;

namespace internal {

template <typename F>
class FunctionMocker;

class ExpectationBase;

template <typename F>
class TypedExpectation;

class ExpectationTester;

template <typename MockClass>
class NiceMockImpl;
template <typename MockClass>
class StrictMockImpl;
template <typename MockClass>
class NaggyMockImpl;

GTEST_API_ GTEST_DECLARE_STATIC_MUTEX_(g_gmock_mutex);

class GTEST_API_ UntypedFunctionMockerBase {
 public:
  UntypedFunctionMockerBase();
  virtual ~UntypedFunctionMockerBase();

  bool VerifyAndClearExpectationsLocked()
      GTEST_EXCLUSIVE_LOCK_REQUIRED_(g_gmock_mutex);

  virtual void ClearDefaultActionsLocked()
      GTEST_EXCLUSIVE_LOCK_REQUIRED_(g_gmock_mutex) = 0;

  virtual void UntypedDescribeUninterestingCall(const void* untyped_args,
                                                ::std::ostream* os) const
      GTEST_LOCK_EXCLUDED_(g_gmock_mutex) = 0;

  virtual const ExpectationBase* UntypedFindMatchingExpectation(
      const void* untyped_args, const void** untyped_action, bool* is_excessive,
      ::std::ostream* what, ::std::ostream* why)
      GTEST_LOCK_EXCLUDED_(g_gmock_mutex) = 0;

  virtual void UntypedPrintArgs(const void* untyped_args,
                                ::std::ostream* os) const = 0;

  void RegisterOwner(const void* mock_obj) GTEST_LOCK_EXCLUDED_(g_gmock_mutex);

  void SetOwnerAndName(const void* mock_obj, const char* name)
      GTEST_LOCK_EXCLUDED_(g_gmock_mutex);

  const void* MockObject() const GTEST_LOCK_EXCLUDED_(g_gmock_mutex);

  const char* Name() const GTEST_LOCK_EXCLUDED_(g_gmock_mutex);

 protected:
  typedef std::vector<const void*> UntypedOnCallSpecs;

  using UntypedExpectations = std::vector<std::shared_ptr<ExpectationBase>>;

  struct UninterestingCallCleanupHandler;
  struct FailureCleanupHandler;

  Expectation GetHandleOf(ExpectationBase* exp);

  const void* mock_obj_;

  const char* name_;

  UntypedOnCallSpecs untyped_on_call_specs_;

  UntypedExpectations untyped_expectations_;
};

class UntypedOnCallSpecBase {
 public:

  UntypedOnCallSpecBase(const char* a_file, int a_line)
      : file_(a_file), line_(a_line), last_clause_(kNone) {}

  const char* file() const { return file_; }
  int line() const { return line_; }

 protected:

  enum Clause {

    kNone,
    kWith,
    kWillByDefault
  };

  void AssertSpecProperty(bool property,
                          const std::string& failure_message) const {
    Assert(property, file_, line_, failure_message);
  }

  void ExpectSpecProperty(bool property,
                          const std::string& failure_message) const {
    Expect(property, file_, line_, failure_message);
  }

  const char* file_;
  int line_;

  Clause last_clause_;
};

template <typename F>
class OnCallSpec : public UntypedOnCallSpecBase {
 public:
  typedef typename Function<F>::ArgumentTuple ArgumentTuple;
  typedef typename Function<F>::ArgumentMatcherTuple ArgumentMatcherTuple;

  OnCallSpec(const char* a_file, int a_line,
             const ArgumentMatcherTuple& matchers)
      : UntypedOnCallSpecBase(a_file, a_line),
        matchers_(matchers),

        extra_matcher_(A<const ArgumentTuple&>()) {}

  OnCallSpec& With(const Matcher<const ArgumentTuple&>& m) {

    ExpectSpecProperty(last_clause_ < kWith,
                       ".With() cannot appear "
                       "more than once in an ON_CALL().");
    last_clause_ = kWith;

    extra_matcher_ = m;
    return *this;
  }

  OnCallSpec& WillByDefault(const Action<F>& action) {
    ExpectSpecProperty(last_clause_ < kWillByDefault,
                       ".WillByDefault() must appear "
                       "exactly once in an ON_CALL().");
    last_clause_ = kWillByDefault;

    ExpectSpecProperty(!action.IsDoDefault(),
                       "DoDefault() cannot be used in ON_CALL().");
    action_ = action;
    return *this;
  }

  bool Matches(const ArgumentTuple& args) const {
    return TupleMatches(matchers_, args) && extra_matcher_.Matches(args);
  }

  const Action<F>& GetAction() const {
    AssertSpecProperty(last_clause_ == kWillByDefault,
                       ".WillByDefault() must appear exactly "
                       "once in an ON_CALL().");
    return action_;
  }

 private:

  ArgumentMatcherTuple matchers_;
  Matcher<const ArgumentTuple&> extra_matcher_;
  Action<F> action_;
};

enum CallReaction {
  kAllow,
  kWarn,
  kFail,
};

}

class GTEST_API_ Mock {
 public:

  static void AllowLeak(const void* mock_obj)
      GTEST_LOCK_EXCLUDED_(internal::g_gmock_mutex);

  static bool VerifyAndClearExpectations(void* mock_obj)
      GTEST_LOCK_EXCLUDED_(internal::g_gmock_mutex);

  static bool VerifyAndClear(void* mock_obj)
      GTEST_LOCK_EXCLUDED_(internal::g_gmock_mutex);

  static bool IsNaggy(void* mock_obj)
      GTEST_LOCK_EXCLUDED_(internal::g_gmock_mutex);

  static bool IsNice(void* mock_obj)
      GTEST_LOCK_EXCLUDED_(internal::g_gmock_mutex);

  static bool IsStrict(void* mock_obj)
      GTEST_LOCK_EXCLUDED_(internal::g_gmock_mutex);

 private:
  friend class internal::UntypedFunctionMockerBase;

  template <typename F>
  friend class internal::FunctionMocker;

  template <typename MockClass>
  friend class internal::NiceMockImpl;
  template <typename MockClass>
  friend class internal::NaggyMockImpl;
  template <typename MockClass>
  friend class internal::StrictMockImpl;

  static void AllowUninterestingCalls(uintptr_t mock_obj)
      GTEST_LOCK_EXCLUDED_(internal::g_gmock_mutex);

  static void WarnUninterestingCalls(uintptr_t mock_obj)
      GTEST_LOCK_EXCLUDED_(internal::g_gmock_mutex);

  static void FailUninterestingCalls(uintptr_t mock_obj)
      GTEST_LOCK_EXCLUDED_(internal::g_gmock_mutex);

  static void UnregisterCallReaction(uintptr_t mock_obj)
      GTEST_LOCK_EXCLUDED_(internal::g_gmock_mutex);

  static internal::CallReaction GetReactionOnUninterestingCalls(
      const void* mock_obj) GTEST_LOCK_EXCLUDED_(internal::g_gmock_mutex);

  static bool VerifyAndClearExpectationsLocked(void* mock_obj)
      GTEST_EXCLUSIVE_LOCK_REQUIRED_(internal::g_gmock_mutex);

  static void ClearDefaultActionsLocked(void* mock_obj)
      GTEST_EXCLUSIVE_LOCK_REQUIRED_(internal::g_gmock_mutex);

  static void Register(const void* mock_obj,
                       internal::UntypedFunctionMockerBase* mocker)
      GTEST_LOCK_EXCLUDED_(internal::g_gmock_mutex);

  static void RegisterUseByOnCallOrExpectCall(const void* mock_obj,
                                              const char* file, int line)
      GTEST_LOCK_EXCLUDED_(internal::g_gmock_mutex);

  static void UnregisterLocked(internal::UntypedFunctionMockerBase* mocker)
      GTEST_EXCLUSIVE_LOCK_REQUIRED_(internal::g_gmock_mutex);
};

class GTEST_API_ Expectation {
 public:

  Expectation();
  Expectation(Expectation&&) = default;
  Expectation(const Expectation&) = default;
  Expectation& operator=(Expectation&&) = default;
  Expectation& operator=(const Expectation&) = default;
  ~Expectation();

  Expectation(internal::ExpectationBase& exp);

  bool operator==(const Expectation& rhs) const {
    return expectation_base_ == rhs.expectation_base_;
  }

  bool operator!=(const Expectation& rhs) const { return !(*this == rhs); }

 private:
  friend class ExpectationSet;
  friend class Sequence;
  friend class ::testing::internal::ExpectationBase;
  friend class ::testing::internal::UntypedFunctionMockerBase;

  template <typename F>
  friend class ::testing::internal::FunctionMocker;

  template <typename F>
  friend class ::testing::internal::TypedExpectation;

  class Less {
   public:
    bool operator()(const Expectation& lhs, const Expectation& rhs) const {
      return lhs.expectation_base_.get() < rhs.expectation_base_.get();
    }
  };

  typedef ::std::set<Expectation, Less> Set;

  Expectation(
      const std::shared_ptr<internal::ExpectationBase>& expectation_base);

  const std::shared_ptr<internal::ExpectationBase>& expectation_base() const {
    return expectation_base_;
  }

  std::shared_ptr<internal::ExpectationBase> expectation_base_;
};

class ExpectationSet {
 public:

  typedef Expectation::Set::const_iterator const_iterator;

  typedef Expectation::Set::value_type value_type;

  ExpectationSet() = default;

  ExpectationSet(internal::ExpectationBase& exp) {
    *this += Expectation(exp);
  }

  ExpectationSet(const Expectation& e) {
    *this += e;
  }

  bool operator==(const ExpectationSet& rhs) const {
    return expectations_ == rhs.expectations_;
  }

  bool operator!=(const ExpectationSet& rhs) const { return !(*this == rhs); }

  ExpectationSet& operator+=(const Expectation& e) {
    expectations_.insert(e);
    return *this;
  }

  int size() const { return static_cast<int>(expectations_.size()); }

  const_iterator begin() const { return expectations_.begin(); }
  const_iterator end() const { return expectations_.end(); }

 private:
  Expectation::Set expectations_;
};

class GTEST_API_ Sequence {
 public:

  Sequence() : last_expectation_(new Expectation) {}

  void AddExpectation(const Expectation& expectation) const;

 private:

  std::shared_ptr<Expectation> last_expectation_;
};

class GTEST_API_ InSequence {
 public:
  InSequence();
  ~InSequence();

 private:
  bool sequence_created_;

  InSequence(const InSequence&) = delete;
  InSequence& operator=(const InSequence&) = delete;
};

namespace internal {

GTEST_API_ extern ThreadLocal<Sequence*> g_gmock_implicit_sequence;

class GTEST_API_ ExpectationBase {
 public:

  ExpectationBase(const char* file, int line, const std::string& source_text);

  virtual ~ExpectationBase();

  const char* file() const { return file_; }
  int line() const { return line_; }
  const char* source_text() const { return source_text_.c_str(); }

  const Cardinality& cardinality() const { return cardinality_; }

  void DescribeLocationTo(::std::ostream* os) const {
    *os << FormatFileLocation(file(), line()) << " ";
  }

  void DescribeCallCountTo(::std::ostream* os) const
      GTEST_EXCLUSIVE_LOCK_REQUIRED_(g_gmock_mutex);

  virtual void MaybeDescribeExtraMatcherTo(::std::ostream* os) = 0;

  void UntypedDescription(std::string description) {
    description_ = std::move(description);
  }

 protected:
  friend class ::testing::Expectation;
  friend class UntypedFunctionMockerBase;

  enum Clause {

    kNone,
    kWith,
    kTimes,
    kInSequence,
    kAfter,
    kWillOnce,
    kWillRepeatedly,
    kRetiresOnSaturation
  };

  typedef std::vector<const void*> UntypedActions;

  virtual Expectation GetHandle() = 0;

  void AssertSpecProperty(bool property,
                          const std::string& failure_message) const {
    Assert(property, file_, line_, failure_message);
  }

  void ExpectSpecProperty(bool property,
                          const std::string& failure_message) const {
    Expect(property, file_, line_, failure_message);
  }

  void SpecifyCardinality(const Cardinality& cardinality);

  bool cardinality_specified() const { return cardinality_specified_; }

  void set_cardinality(const Cardinality& a_cardinality) {
    cardinality_ = a_cardinality;
  }

  void RetireAllPreRequisites() GTEST_EXCLUSIVE_LOCK_REQUIRED_(g_gmock_mutex);

  bool is_retired() const GTEST_EXCLUSIVE_LOCK_REQUIRED_(g_gmock_mutex) {
    g_gmock_mutex.AssertHeld();
    return retired_;
  }

  void Retire() GTEST_EXCLUSIVE_LOCK_REQUIRED_(g_gmock_mutex) {
    g_gmock_mutex.AssertHeld();
    retired_ = true;
  }

  const std::string& GetDescription() const { return description_; }

  bool IsSatisfied() const GTEST_EXCLUSIVE_LOCK_REQUIRED_(g_gmock_mutex) {
    g_gmock_mutex.AssertHeld();
    return cardinality().IsSatisfiedByCallCount(call_count_);
  }

  bool IsSaturated() const GTEST_EXCLUSIVE_LOCK_REQUIRED_(g_gmock_mutex) {
    g_gmock_mutex.AssertHeld();
    return cardinality().IsSaturatedByCallCount(call_count_);
  }

  bool IsOverSaturated() const GTEST_EXCLUSIVE_LOCK_REQUIRED_(g_gmock_mutex) {
    g_gmock_mutex.AssertHeld();
    return cardinality().IsOverSaturatedByCallCount(call_count_);
  }

  bool AllPrerequisitesAreSatisfied() const
      GTEST_EXCLUSIVE_LOCK_REQUIRED_(g_gmock_mutex);

  void FindUnsatisfiedPrerequisites(ExpectationSet* result) const
      GTEST_EXCLUSIVE_LOCK_REQUIRED_(g_gmock_mutex);

  int call_count() const GTEST_EXCLUSIVE_LOCK_REQUIRED_(g_gmock_mutex) {
    g_gmock_mutex.AssertHeld();
    return call_count_;
  }

  void IncrementCallCount() GTEST_EXCLUSIVE_LOCK_REQUIRED_(g_gmock_mutex) {
    g_gmock_mutex.AssertHeld();
    call_count_++;
  }

  void CheckActionCountIfNotDone() const GTEST_LOCK_EXCLUDED_(mutex_);

  friend class ::testing::Sequence;
  friend class ::testing::internal::ExpectationTester;

  template <typename Function>
  friend class TypedExpectation;

  void UntypedTimes(const Cardinality& a_cardinality);

  const char* file_;
  int line_;
  const std::string source_text_;
  std::string description_;

  bool cardinality_specified_;
  Cardinality cardinality_;

  ExpectationSet immediate_prerequisites_;

  int call_count_;
  bool retired_;
  UntypedActions untyped_actions_;
  bool extra_matcher_specified_;
  bool repeated_action_specified_;
  bool retires_on_saturation_;
  Clause last_clause_;
  mutable bool action_count_checked_;
  mutable Mutex mutex_;
};

template <typename F>
class TypedExpectation;

template <typename R, typename... Args>
class TypedExpectation<R(Args...)> : public ExpectationBase {
 private:
  using F = R(Args...);

 public:
  typedef typename Function<F>::ArgumentTuple ArgumentTuple;
  typedef typename Function<F>::ArgumentMatcherTuple ArgumentMatcherTuple;
  typedef typename Function<F>::Result Result;

  TypedExpectation(FunctionMocker<F>* owner, const char* a_file, int a_line,
                   const std::string& a_source_text,
                   const ArgumentMatcherTuple& m)
      : ExpectationBase(a_file, a_line, a_source_text),
        owner_(owner),
        matchers_(m),

        extra_matcher_(A<const ArgumentTuple&>()),
        repeated_action_(DoDefault()) {}

  ~TypedExpectation() override {

    CheckActionCountIfNotDone();
    for (UntypedActions::const_iterator it = untyped_actions_.begin();
         it != untyped_actions_.end(); ++it) {
      delete static_cast<const Action<F>*>(*it);
    }
  }

  TypedExpectation& With(const Matcher<const ArgumentTuple&>& m) {
    if (last_clause_ == kWith) {
      ExpectSpecProperty(false,
                         ".With() cannot appear "
                         "more than once in an EXPECT_CALL().");
    } else {
      ExpectSpecProperty(last_clause_ < kWith,
                         ".With() must be the first "
                         "clause in an EXPECT_CALL().");
    }
    last_clause_ = kWith;

    extra_matcher_ = m;
    extra_matcher_specified_ = true;
    return *this;
  }

  TypedExpectation& Description(std::string name) {
    ExpectationBase::UntypedDescription(std::move(name));
    return *this;
  }

  TypedExpectation& Times(const Cardinality& a_cardinality) {
    ExpectationBase::UntypedTimes(a_cardinality);
    return *this;
  }

  TypedExpectation& Times(int n) { return Times(Exactly(n)); }

  TypedExpectation& InSequence(const Sequence& s) {
    ExpectSpecProperty(last_clause_ <= kInSequence,
                       ".InSequence() cannot appear after .After(),"
                       " .WillOnce(), .WillRepeatedly(), or "
                       ".RetiresOnSaturation().");
    last_clause_ = kInSequence;

    s.AddExpectation(GetHandle());
    return *this;
  }
  TypedExpectation& InSequence(const Sequence& s1, const Sequence& s2) {
    return InSequence(s1).InSequence(s2);
  }
  TypedExpectation& InSequence(const Sequence& s1, const Sequence& s2,
                               const Sequence& s3) {
    return InSequence(s1, s2).InSequence(s3);
  }
  TypedExpectation& InSequence(const Sequence& s1, const Sequence& s2,
                               const Sequence& s3, const Sequence& s4) {
    return InSequence(s1, s2, s3).InSequence(s4);
  }
  TypedExpectation& InSequence(const Sequence& s1, const Sequence& s2,
                               const Sequence& s3, const Sequence& s4,
                               const Sequence& s5) {
    return InSequence(s1, s2, s3, s4).InSequence(s5);
  }

  TypedExpectation& After(const ExpectationSet& s) {
    ExpectSpecProperty(last_clause_ <= kAfter,
                       ".After() cannot appear after .WillOnce(),"
                       " .WillRepeatedly(), or "
                       ".RetiresOnSaturation().");
    last_clause_ = kAfter;

    for (ExpectationSet::const_iterator it = s.begin(); it != s.end(); ++it) {
      immediate_prerequisites_ += *it;
    }
    return *this;
  }
  TypedExpectation& After(const ExpectationSet& s1, const ExpectationSet& s2) {
    return After(s1).After(s2);
  }
  TypedExpectation& After(const ExpectationSet& s1, const ExpectationSet& s2,
                          const ExpectationSet& s3) {
    return After(s1, s2).After(s3);
  }
  TypedExpectation& After(const ExpectationSet& s1, const ExpectationSet& s2,
                          const ExpectationSet& s3, const ExpectationSet& s4) {
    return After(s1, s2, s3).After(s4);
  }
  TypedExpectation& After(const ExpectationSet& s1, const ExpectationSet& s2,
                          const ExpectationSet& s3, const ExpectationSet& s4,
                          const ExpectationSet& s5) {
    return After(s1, s2, s3, s4).After(s5);
  }

  TypedExpectation& WillOnce(OnceAction<F> once_action) {

    return WillOnce(Action<F>(ActionAdaptor{
        std::make_shared<OnceAction<F>>(std::move(once_action)),
    }));
  }

  template <int&... ExplicitArgumentBarrier, typename = void>
  TypedExpectation& WillOnce(Action<F> action) {
    ExpectSpecProperty(last_clause_ <= kWillOnce,
                       ".WillOnce() cannot appear after "
                       ".WillRepeatedly() or .RetiresOnSaturation().");
    last_clause_ = kWillOnce;

    untyped_actions_.push_back(new Action<F>(std::move(action)));

    if (!cardinality_specified()) {
      set_cardinality(Exactly(static_cast<int>(untyped_actions_.size())));
    }
    return *this;
  }

  TypedExpectation& WillRepeatedly(const Action<F>& action) {
    if (last_clause_ == kWillRepeatedly) {
      ExpectSpecProperty(false,
                         ".WillRepeatedly() cannot appear "
                         "more than once in an EXPECT_CALL().");
    } else {
      ExpectSpecProperty(last_clause_ < kWillRepeatedly,
                         ".WillRepeatedly() cannot appear "
                         "after .RetiresOnSaturation().");
    }
    last_clause_ = kWillRepeatedly;
    repeated_action_specified_ = true;

    repeated_action_ = action;
    if (!cardinality_specified()) {
      set_cardinality(AtLeast(static_cast<int>(untyped_actions_.size())));
    }

    CheckActionCountIfNotDone();
    return *this;
  }

  TypedExpectation& RetiresOnSaturation() {
    ExpectSpecProperty(last_clause_ < kRetiresOnSaturation,
                       ".RetiresOnSaturation() cannot appear "
                       "more than once.");
    last_clause_ = kRetiresOnSaturation;
    retires_on_saturation_ = true;

    CheckActionCountIfNotDone();
    return *this;
  }

  const ArgumentMatcherTuple& matchers() const { return matchers_; }

  const Matcher<const ArgumentTuple&>& extra_matcher() const {
    return extra_matcher_;
  }

  const Action<F>& repeated_action() const { return repeated_action_; }

  void MaybeDescribeExtraMatcherTo(::std::ostream* os) override {
    if (extra_matcher_specified_) {
      *os << "    Expected args: ";
      extra_matcher_.DescribeTo(os);
      *os << "\n";
    }
  }

 private:
  template <typename Function>
  friend class FunctionMocker;

  struct ActionAdaptor {
    std::shared_ptr<OnceAction<R(Args...)>> once_action;

    R operator()(Args&&... args) const {
      return std::move(*once_action).Call(std::forward<Args>(args)...);
    }
  };

  Expectation GetHandle() override { return owner_->GetHandleOf(this); }

  bool Matches(const ArgumentTuple& args) const
      GTEST_EXCLUSIVE_LOCK_REQUIRED_(g_gmock_mutex) {
    g_gmock_mutex.AssertHeld();
    return TupleMatches(matchers_, args) && extra_matcher_.Matches(args);
  }

  bool ShouldHandleArguments(const ArgumentTuple& args) const
      GTEST_EXCLUSIVE_LOCK_REQUIRED_(g_gmock_mutex) {
    g_gmock_mutex.AssertHeld();

    CheckActionCountIfNotDone();
    return !is_retired() && AllPrerequisitesAreSatisfied() && Matches(args);
  }

  void ExplainMatchResultTo(const ArgumentTuple& args, ::std::ostream* os) const
      GTEST_EXCLUSIVE_LOCK_REQUIRED_(g_gmock_mutex) {
    g_gmock_mutex.AssertHeld();

    if (is_retired()) {
      *os << "         Expected: the expectation is active\n"
          << "           Actual: it is retired\n";
    } else if (!Matches(args)) {
      if (!TupleMatches(matchers_, args)) {
        ExplainMatchFailureTupleTo(matchers_, args, os);
      }
      StringMatchResultListener listener;
      if (!extra_matcher_.MatchAndExplain(args, &listener)) {
        *os << "    Expected args: ";
        extra_matcher_.DescribeTo(os);
        *os << "\n           Actual: don't match";

        internal::PrintIfNotEmpty(listener.str(), os);
        *os << "\n";
      }
    } else if (!AllPrerequisitesAreSatisfied()) {
      *os << "         Expected: all pre-requisites are satisfied\n"
          << "           Actual: the following immediate pre-requisites "
          << "are not satisfied:\n";
      ExpectationSet unsatisfied_prereqs;
      FindUnsatisfiedPrerequisites(&unsatisfied_prereqs);
      int i = 0;
      for (ExpectationSet::const_iterator it = unsatisfied_prereqs.begin();
           it != unsatisfied_prereqs.end(); ++it) {
        it->expectation_base()->DescribeLocationTo(os);
        *os << "pre-requisite #" << i++ << "\n";
      }
      *os << "                   (end of pre-requisites)\n";
    } else {

      *os << "The call matches the expectation.\n";
    }
  }

  const Action<F>& GetCurrentAction(const FunctionMocker<F>* mocker,
                                    const ArgumentTuple& args) const
      GTEST_EXCLUSIVE_LOCK_REQUIRED_(g_gmock_mutex) {
    g_gmock_mutex.AssertHeld();
    const int count = call_count();
    Assert(count >= 1, __FILE__, __LINE__,
           "call_count() is <= 0 when GetCurrentAction() is "
           "called - this should never happen.");

    const int action_count = static_cast<int>(untyped_actions_.size());
    if (action_count > 0 && !repeated_action_specified_ &&
        count > action_count) {

      ::std::stringstream ss;
      DescribeLocationTo(&ss);
      ss << "Actions ran out in " << source_text() << "...\n"
         << "Called " << count << " times, but only " << action_count
         << " WillOnce()" << (action_count == 1 ? " is" : "s are")
         << " specified - ";
      mocker->DescribeDefaultActionTo(args, &ss);
      Log(kWarning, ss.str(), 1);
    }

    return count <= action_count
               ? *static_cast<const Action<F>*>(
                     untyped_actions_[static_cast<size_t>(count - 1)])
               : repeated_action();
  }

  const Action<F>* GetActionForArguments(const FunctionMocker<F>* mocker,
                                         const ArgumentTuple& args,
                                         ::std::ostream* what,
                                         ::std::ostream* why)
      GTEST_EXCLUSIVE_LOCK_REQUIRED_(g_gmock_mutex) {
    g_gmock_mutex.AssertHeld();
    const ::std::string& expectation_description = GetDescription();
    if (IsSaturated()) {

      IncrementCallCount();
      *what << "Mock function ";
      if (!expectation_description.empty()) {
        *what << "\"" << expectation_description << "\" ";
      }
      *what << "called more times than expected - ";
      mocker->DescribeDefaultActionTo(args, what);
      DescribeCallCountTo(why);

      return nullptr;
    }

    IncrementCallCount();
    RetireAllPreRequisites();

    if (retires_on_saturation_ && IsSaturated()) {
      Retire();
    }

    *what << "Mock function ";
    if (!expectation_description.empty()) {
      *what << "\"" << expectation_description << "\" ";
    }
    *what << "call matches " << source_text() << "...\n";
    return &(GetCurrentAction(mocker, args));
  }

  FunctionMocker<F>* const owner_;
  ArgumentMatcherTuple matchers_;
  Matcher<const ArgumentTuple&> extra_matcher_;
  Action<F> repeated_action_;

  TypedExpectation(const TypedExpectation&) = delete;
  TypedExpectation& operator=(const TypedExpectation&) = delete;
};

GTEST_API_ void LogWithLocation(testing::internal::LogSeverity severity,
                                const char* file, int line,
                                const std::string& message);

template <typename F>
class MockSpec {
 public:
  typedef typename internal::Function<F>::ArgumentTuple ArgumentTuple;
  typedef
      typename internal::Function<F>::ArgumentMatcherTuple ArgumentMatcherTuple;

  MockSpec(internal::FunctionMocker<F>* function_mocker,
           const ArgumentMatcherTuple& matchers)
      : function_mocker_(function_mocker), matchers_(matchers) {}

  internal::OnCallSpec<F>& InternalDefaultActionSetAt(const char* file,
                                                      int line, const char* obj,
                                                      const char* call) {
    LogWithLocation(internal::kInfo, file, line,
                    std::string("ON_CALL(") + obj + ", " + call + ") invoked");
    return function_mocker_->AddNewOnCallSpec(file, line, matchers_);
  }

  internal::TypedExpectation<F>& InternalExpectedAt(const char* file, int line,
                                                    const char* obj,
                                                    const char* call) {
    const std::string source_text(std::string("EXPECT_CALL(") + obj + ", " +
                                  call + ")");
    LogWithLocation(internal::kInfo, file, line, source_text + " invoked");
    return function_mocker_->AddNewExpectation(file, line, source_text,
                                               matchers_);
  }

  MockSpec<F>& operator()(const internal::WithoutMatchers&, void* const) {
    return *this;
  }

 private:
  template <typename Function>
  friend class internal::FunctionMocker;

  internal::FunctionMocker<F>* const function_mocker_;

  ArgumentMatcherTuple matchers_;
};

template <typename T>
class ReferenceOrValueWrapper {
 public:

  explicit ReferenceOrValueWrapper(T value) : value_(std::move(value)) {}

  T Unwrap() { return std::move(value_); }

  const T& Peek() const { return value_; }

 private:
  T value_;
};

template <typename T>
class ReferenceOrValueWrapper<T&> {
 public:

  typedef T& reference;
  explicit ReferenceOrValueWrapper(reference ref) : value_ptr_(&ref) {}
  T& Unwrap() { return *value_ptr_; }
  const T& Peek() const { return *value_ptr_; }

 private:
  T* value_ptr_;
};

template <typename T>
void PrintAsActionResult(const T& result, std::ostream& os) {
  os << "\n          Returns: ";

  UniversalPrinter<T>::Print(result, &os);
}

GTEST_API_ void ReportUninterestingCall(CallReaction reaction,
                                        const std::string& msg);

class Cleanup final {
 public:
  explicit Cleanup(std::function<void()> f) : f_(std::move(f)) {}
  ~Cleanup() { f_(); }

 private:
  std::function<void()> f_;
};

struct UntypedFunctionMockerBase::UninterestingCallCleanupHandler {
  CallReaction reaction;
  std::stringstream& ss;

  ~UninterestingCallCleanupHandler() {
    ReportUninterestingCall(reaction, ss.str());
  }
};

struct UntypedFunctionMockerBase::FailureCleanupHandler {
  std::stringstream& ss;
  std::stringstream& why;
  std::stringstream& loc;
  const ExpectationBase* untyped_expectation;
  bool found;
  bool is_excessive;

  ~FailureCleanupHandler() {
    ss << "\n" << why.str();

    if (!found) {

      Expect(false, nullptr, -1, ss.str());
    } else if (is_excessive) {

      Expect(false, untyped_expectation->file(), untyped_expectation->line(),
             ss.str());
    } else {

      Log(kInfo, loc.str() + ss.str(), 2);
    }
  }
};

template <typename F>
class FunctionMocker;

template <typename R, typename... Args>
class FunctionMocker<R(Args...)> final : public UntypedFunctionMockerBase {
  using F = R(Args...);

 public:
  using Result = R;
  using ArgumentTuple = std::tuple<Args...>;
  using ArgumentMatcherTuple = std::tuple<Matcher<Args>...>;

  FunctionMocker() = default;

  FunctionMocker(const FunctionMocker&) = delete;
  FunctionMocker& operator=(const FunctionMocker&) = delete;

  ~FunctionMocker() override GTEST_LOCK_EXCLUDED_(g_gmock_mutex) {
    MutexLock l(&g_gmock_mutex);
    VerifyAndClearExpectationsLocked();
    Mock::UnregisterLocked(this);
    ClearDefaultActionsLocked();
  }

  const OnCallSpec<F>* FindOnCallSpec(const ArgumentTuple& args) const {
    for (UntypedOnCallSpecs::const_reverse_iterator it =
             untyped_on_call_specs_.rbegin();
         it != untyped_on_call_specs_.rend(); ++it) {
      const OnCallSpec<F>* spec = static_cast<const OnCallSpec<F>*>(*it);
      if (spec->Matches(args)) return spec;
    }

    return nullptr;
  }

  Result PerformDefaultAction(ArgumentTuple&& args,
                              const std::string& call_description) const {
    const OnCallSpec<F>* const spec = this->FindOnCallSpec(args);
    if (spec != nullptr) {
      return spec->GetAction().Perform(std::move(args));
    }
    const std::string message =
        call_description +
        "\n    The mock function has no default action "
        "set, and its return type has no default value set.";
#if GTEST_HAS_EXCEPTIONS
    if (!DefaultValue<Result>::Exists()) {
      throw std::runtime_error(message);
    }
#else
    Assert(DefaultValue<Result>::Exists(), "", -1, message);
#endif
    return DefaultValue<Result>::Get();
  }

  void ClearDefaultActionsLocked() override
      GTEST_EXCLUSIVE_LOCK_REQUIRED_(g_gmock_mutex) {
    g_gmock_mutex.AssertHeld();

    UntypedOnCallSpecs specs_to_delete;
    untyped_on_call_specs_.swap(specs_to_delete);

    g_gmock_mutex.Unlock();
    for (UntypedOnCallSpecs::const_iterator it = specs_to_delete.begin();
         it != specs_to_delete.end(); ++it) {
      delete static_cast<const OnCallSpec<F>*>(*it);
    }

    g_gmock_mutex.Lock();
  }

  Result Invoke(Args... args) GTEST_LOCK_EXCLUDED_(g_gmock_mutex) {
    return InvokeWith(ArgumentTuple(std::forward<Args>(args)...));
  }

  MockSpec<F> With(Matcher<Args>... m) {
    return MockSpec<F>(this, ::std::make_tuple(std::move(m)...));
  }

 protected:
  template <typename Function>
  friend class MockSpec;

  OnCallSpec<F>& AddNewOnCallSpec(const char* file, int line,
                                  const ArgumentMatcherTuple& m)
      GTEST_LOCK_EXCLUDED_(g_gmock_mutex) {
    Mock::RegisterUseByOnCallOrExpectCall(MockObject(), file, line);
    OnCallSpec<F>* const on_call_spec = new OnCallSpec<F>(file, line, m);
    untyped_on_call_specs_.push_back(on_call_spec);
    return *on_call_spec;
  }

  TypedExpectation<F>& AddNewExpectation(const char* file, int line,
                                         const std::string& source_text,
                                         const ArgumentMatcherTuple& m)
      GTEST_LOCK_EXCLUDED_(g_gmock_mutex) {
    Mock::RegisterUseByOnCallOrExpectCall(MockObject(), file, line);
    TypedExpectation<F>* const expectation =
        new TypedExpectation<F>(this, file, line, source_text, m);
    const std::shared_ptr<ExpectationBase> untyped_expectation(expectation);

    untyped_expectations_.push_back(untyped_expectation);

    Sequence* const implicit_sequence = g_gmock_implicit_sequence.get();
    if (implicit_sequence != nullptr) {
      implicit_sequence->AddExpectation(Expectation(untyped_expectation));
    }

    return *expectation;
  }

 private:
  template <typename Func>
  friend class TypedExpectation;

  void DescribeDefaultActionTo(const ArgumentTuple& args,
                               ::std::ostream* os) const {
    const OnCallSpec<F>* const spec = FindOnCallSpec(args);

    if (spec == nullptr) {
      *os << (std::is_void<Result>::value ? "returning directly.\n"
                                          : "returning default value.\n");
    } else {
      *os << "taking default action specified at:\n"
          << FormatFileLocation(spec->file(), spec->line()) << "\n";
    }
  }

  void UntypedDescribeUninterestingCall(const void* untyped_args,
                                        ::std::ostream* os) const override
      GTEST_LOCK_EXCLUDED_(g_gmock_mutex) {
    const ArgumentTuple& args =
        *static_cast<const ArgumentTuple*>(untyped_args);
    *os << "Uninteresting mock function call - ";
    DescribeDefaultActionTo(args, os);
    *os << "    Function call: " << Name();
    UniversalPrint(args, os);
  }

  const ExpectationBase* UntypedFindMatchingExpectation(
      const void* untyped_args, const void** untyped_action, bool* is_excessive,
      ::std::ostream* what, ::std::ostream* why) override
      GTEST_LOCK_EXCLUDED_(g_gmock_mutex) {
    const ArgumentTuple& args =
        *static_cast<const ArgumentTuple*>(untyped_args);
    MutexLock l(&g_gmock_mutex);
    TypedExpectation<F>* exp = this->FindMatchingExpectationLocked(args);
    if (exp == nullptr) {
      this->FormatUnexpectedCallMessageLocked(args, what, why);
      return nullptr;
    }

    *is_excessive = exp->IsSaturated();
    const Action<F>* action = exp->GetActionForArguments(this, args, what, why);
    if (action != nullptr && action->IsDoDefault())
      action = nullptr;
    *untyped_action = action;
    return exp;
  }

  void UntypedPrintArgs(const void* untyped_args,
                        ::std::ostream* os) const override {
    const ArgumentTuple& args =
        *static_cast<const ArgumentTuple*>(untyped_args);
    UniversalPrint(args, os);
  }

  TypedExpectation<F>* FindMatchingExpectationLocked(const ArgumentTuple& args)
      const GTEST_EXCLUSIVE_LOCK_REQUIRED_(g_gmock_mutex) {
    g_gmock_mutex.AssertHeld();

    for (typename UntypedExpectations::const_reverse_iterator it =
             untyped_expectations_.rbegin();
         it != untyped_expectations_.rend(); ++it) {
      TypedExpectation<F>* const exp =
          static_cast<TypedExpectation<F>*>(it->get());
      if (exp->ShouldHandleArguments(args)) {
        return exp;
      }
    }
    return nullptr;
  }

  void FormatUnexpectedCallMessageLocked(const ArgumentTuple& args,
                                         ::std::ostream* os,
                                         ::std::ostream* why) const
      GTEST_EXCLUSIVE_LOCK_REQUIRED_(g_gmock_mutex) {
    g_gmock_mutex.AssertHeld();
    *os << "\nUnexpected mock function call - ";
    DescribeDefaultActionTo(args, os);
    PrintTriedExpectationsLocked(args, why);
  }

  void PrintTriedExpectationsLocked(const ArgumentTuple& args,
                                    ::std::ostream* why) const
      GTEST_EXCLUSIVE_LOCK_REQUIRED_(g_gmock_mutex) {
    g_gmock_mutex.AssertHeld();
    const size_t count = untyped_expectations_.size();
    *why << "Google Mock tried the following " << count << " "
         << (count == 1 ? "expectation, but it didn't match"
                        : "expectations, but none matched")
         << ":\n";
    for (size_t i = 0; i < count; i++) {
      TypedExpectation<F>* const expectation =
          static_cast<TypedExpectation<F>*>(untyped_expectations_[i].get());
      *why << "\n";
      expectation->DescribeLocationTo(why);
      if (count > 1) {
        *why << "tried expectation #" << i << ": ";
      }
      *why << expectation->source_text() << "...\n";
      expectation->ExplainMatchResultTo(args, why);
      expectation->DescribeCallCountTo(why);
    }
  }

  R PerformAction(const void* untyped_action, ArgumentTuple&& args,
                  const std::string& call_description) const {
    if (untyped_action == nullptr) {
      return PerformDefaultAction(std::move(args), call_description);
    }

    const Action<F> action = *static_cast<const Action<F>*>(untyped_action);
    return action.Perform(std::move(args));
  }

  template <typename T>
  using can_print_result = internal::conjunction<

      internal::negation<std::is_void<T>>,

      std::is_move_constructible<T>>;

  template <typename T = R,
            typename std::enable_if<can_print_result<T>::value, int>::type = 0>
  R PerformActionAndPrintResult(const void* const untyped_action,
                                ArgumentTuple&& args,
                                const std::string& call_description,
                                std::ostream& os) {
    R result = PerformAction(untyped_action, std::move(args), call_description);

    PrintAsActionResult(result, os);
    return std::forward<R>(result);
  }

  template <typename T = R,
            typename std::enable_if<
                internal::negation<can_print_result<T>>::value, int>::type = 0>
  R PerformActionAndPrintResult(const void* const untyped_action,
                                ArgumentTuple&& args,
                                const std::string& call_description,
                                std::ostream&) {
    return PerformAction(untyped_action, std::move(args), call_description);
  }

  R InvokeWith(ArgumentTuple&& args) GTEST_LOCK_EXCLUDED_(g_gmock_mutex);
};

template <typename R, typename... Args>
R FunctionMocker<R(Args...)>::InvokeWith(ArgumentTuple&& args)
    GTEST_LOCK_EXCLUDED_(g_gmock_mutex) {

  if (untyped_expectations_.size() == 0) {

    const CallReaction reaction =
        Mock::GetReactionOnUninterestingCalls(MockObject());

    const bool need_to_report_uninteresting_call =

        reaction == kAllow ? LogIsVisible(kInfo) :

            reaction == kWarn
            ? LogIsVisible(kWarning)
            :

            true;

    if (!need_to_report_uninteresting_call) {

      return this->PerformDefaultAction(
          std::move(args), "Function call: " + std::string(Name()));
    }

    ::std::stringstream ss;
    this->UntypedDescribeUninterestingCall(&args, &ss);

    const UninterestingCallCleanupHandler report_uninteresting_call = {reaction,
                                                                       ss};

    return PerformActionAndPrintResult(nullptr, std::move(args), ss.str(), ss);
  }

  bool is_excessive = false;
  ::std::stringstream ss;
  ::std::stringstream why;
  ::std::stringstream loc;
  const void* untyped_action = nullptr;

  const ExpectationBase* const untyped_expectation =
      this->UntypedFindMatchingExpectation(&args, &untyped_action,
                                           &is_excessive, &ss, &why);
  const bool found = untyped_expectation != nullptr;

  const bool need_to_report_call =
      !found || is_excessive || LogIsVisible(kInfo);
  if (!need_to_report_call) {

    return PerformAction(untyped_action, std::move(args), "");
  }

  ss << "    Function call: " << Name();
  this->UntypedPrintArgs(&args, &ss);

  if (found && !is_excessive) {
    untyped_expectation->DescribeLocationTo(&loc);
  }

  const FailureCleanupHandler handle_failures = {
      ss, why, loc, untyped_expectation, found, is_excessive};

  return PerformActionAndPrintResult(untyped_action, std::move(args), ss.str(),
                                     ss);
}

}

namespace internal {

template <typename F>
class MockFunction;

template <typename R, typename... Args>
class MockFunction<R(Args...)> {
 public:
  MockFunction(const MockFunction&) = delete;
  MockFunction& operator=(const MockFunction&) = delete;

  std::function<R(Args...)> AsStdFunction() {
    return [this](Args... args) -> R {
      return this->Call(std::forward<Args>(args)...);
    };
  }

  R Call(Args... args) {
    mock_.SetOwnerAndName(this, "Call");
    return mock_.Invoke(std::forward<Args>(args)...);
  }

  MockSpec<R(Args...)> gmock_Call(Matcher<Args>... m) {
    mock_.RegisterOwner(this);
    return mock_.With(std::move(m)...);
  }

  MockSpec<R(Args...)> gmock_Call(const WithoutMatchers&, R (*)(Args...)) {
    return this->gmock_Call(::testing::A<Args>()...);
  }

 protected:
  MockFunction() = default;
  ~MockFunction() = default;

 private:
  FunctionMocker<R(Args...)> mock_;
};

template <typename F, typename = void>
struct SignatureOf;

template <typename R, typename... Args>
struct SignatureOf<R(Args...)> {
  using type = R(Args...);
};

template <template <typename> class C, typename F>
struct SignatureOf<C<F>,
                   typename std::enable_if<std::is_function<F>::value>::type>
    : SignatureOf<F> {};

template <typename F>
using SignatureOfT = typename SignatureOf<F>::type;

}

template <typename F>
class MockFunction : public internal::MockFunction<internal::SignatureOfT<F>> {
  using Base = internal::MockFunction<internal::SignatureOfT<F>>;

 public:
  using Base::Base;
};

using internal::MockSpec;

template <typename T>
inline const T& Const(const T& x) {
  return x;
}

inline Expectation::Expectation(internal::ExpectationBase& exp)
    : expectation_base_(exp.GetHandle().expectation_base()) {}

}

GTEST_DISABLE_MSC_WARNINGS_POP_()

#define GMOCK_ON_CALL_IMPL_(mock_expr, Setter, call)                    \
  ((mock_expr).gmock_##call)(::testing::internal::GetWithoutMatchers(), \
                             nullptr)                                   \
      .Setter(__FILE__, __LINE__, #mock_expr, #call)

#define ON_CALL(obj, call) \
  GMOCK_ON_CALL_IMPL_(obj, InternalDefaultActionSetAt, call)

#define EXPECT_CALL(obj, call) \
  GMOCK_ON_CALL_IMPL_(obj, InternalExpectedAt, call)

#endif
