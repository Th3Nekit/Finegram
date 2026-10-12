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
// This file implements the spec builder syntax (ON_CALL and
// EXPECT_CALL).

#include "gmock/gmock-spec-builders.h"

#include <stdlib.h>

#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "gtest/internal/gtest-port.h"

#if defined(GTEST_OS_CYGWIN) || defined(GTEST_OS_LINUX) || defined(GTEST_OS_MAC)
#include <unistd.h>
#endif
#ifdef GTEST_OS_QURT
#include <qurt_event.h>
#endif

#if defined(_MSC_VER) && (_MSC_VER == 1900)
GTEST_DISABLE_MSC_WARNINGS_PUSH_(4800)
#endif

namespace testing {
namespace internal {

GTEST_API_ GTEST_DEFINE_STATIC_MUTEX_(g_gmock_mutex);

GTEST_API_ void LogWithLocation(testing::internal::LogSeverity severity,
                                const char* file, int line,
                                const std::string& message) {
  ::std::ostringstream s;
  s << internal::FormatFileLocation(file, line) << " " << message
    << ::std::endl;
  Log(severity, s.str(), 0);
}

ExpectationBase::ExpectationBase(const char* a_file, int a_line,
                                 const std::string& a_source_text)
    : file_(a_file),
      line_(a_line),
      source_text_(a_source_text),
      cardinality_specified_(false),
      cardinality_(Exactly(1)),
      call_count_(0),
      retired_(false),
      extra_matcher_specified_(false),
      repeated_action_specified_(false),
      retires_on_saturation_(false),
      last_clause_(kNone),
      action_count_checked_(false) {}

ExpectationBase::~ExpectationBase() = default;

void ExpectationBase::SpecifyCardinality(const Cardinality& a_cardinality) {
  cardinality_specified_ = true;
  cardinality_ = a_cardinality;
}

void ExpectationBase::RetireAllPreRequisites()
    GTEST_EXCLUSIVE_LOCK_REQUIRED_(g_gmock_mutex) {
  if (is_retired()) {

    return;
  }

  ::std::vector<ExpectationBase*> expectations(1, this);
  while (!expectations.empty()) {
    ExpectationBase* exp = expectations.back();
    expectations.pop_back();

    for (ExpectationSet::const_iterator it =
             exp->immediate_prerequisites_.begin();
         it != exp->immediate_prerequisites_.end(); ++it) {
      ExpectationBase* next = it->expectation_base().get();
      if (!next->is_retired()) {
        next->Retire();
        expectations.push_back(next);
      }
    }
  }
}

bool ExpectationBase::AllPrerequisitesAreSatisfied() const
    GTEST_EXCLUSIVE_LOCK_REQUIRED_(g_gmock_mutex) {
  g_gmock_mutex.AssertHeld();
  ::std::vector<const ExpectationBase*> expectations(1, this);
  while (!expectations.empty()) {
    const ExpectationBase* exp = expectations.back();
    expectations.pop_back();

    for (ExpectationSet::const_iterator it =
             exp->immediate_prerequisites_.begin();
         it != exp->immediate_prerequisites_.end(); ++it) {
      const ExpectationBase* next = it->expectation_base().get();
      if (!next->IsSatisfied()) return false;
      expectations.push_back(next);
    }
  }
  return true;
}

void ExpectationBase::FindUnsatisfiedPrerequisites(ExpectationSet* result) const
    GTEST_EXCLUSIVE_LOCK_REQUIRED_(g_gmock_mutex) {
  g_gmock_mutex.AssertHeld();
  ::std::vector<const ExpectationBase*> expectations(1, this);
  while (!expectations.empty()) {
    const ExpectationBase* exp = expectations.back();
    expectations.pop_back();

    for (ExpectationSet::const_iterator it =
             exp->immediate_prerequisites_.begin();
         it != exp->immediate_prerequisites_.end(); ++it) {
      const ExpectationBase* next = it->expectation_base().get();

      if (next->IsSatisfied()) {

        if (next->call_count_ == 0) {
          expectations.push_back(next);
        }
      } else {

        *result += *it;
      }
    }
  }
}

void ExpectationBase::DescribeCallCountTo(::std::ostream* os) const
    GTEST_EXCLUSIVE_LOCK_REQUIRED_(g_gmock_mutex) {
  g_gmock_mutex.AssertHeld();

  *os << "         Expected: to be ";
  cardinality().DescribeTo(os);
  *os << "\n           Actual: ";
  Cardinality::DescribeActualCallCountTo(call_count(), os);

  *os << " - "
      << (IsOverSaturated() ? "over-saturated"
          : IsSaturated()   ? "saturated"
          : IsSatisfied()   ? "satisfied"
                            : "unsatisfied")
      << " and " << (is_retired() ? "retired" : "active");
}

void ExpectationBase::CheckActionCountIfNotDone() const
    GTEST_LOCK_EXCLUDED_(mutex_) {
  bool should_check = false;
  {
    MutexLock l(&mutex_);
    if (!action_count_checked_) {
      action_count_checked_ = true;
      should_check = true;
    }
  }

  if (should_check) {
    if (!cardinality_specified_) {

      return;
    }

    const int action_count = static_cast<int>(untyped_actions_.size());
    const int upper_bound = cardinality().ConservativeUpperBound();
    const int lower_bound = cardinality().ConservativeLowerBound();
    bool too_many;

    if (action_count > upper_bound ||
        (action_count == upper_bound && repeated_action_specified_)) {
      too_many = true;
    } else if (0 < action_count && action_count < lower_bound &&
               !repeated_action_specified_) {
      too_many = false;
    } else {
      return;
    }

    ::std::stringstream ss;
    DescribeLocationTo(&ss);
    ss << "Too " << (too_many ? "many" : "few") << " actions specified in "
       << source_text() << "...\n"
       << "Expected to be ";
    cardinality().DescribeTo(&ss);
    ss << ", but has " << (too_many ? "" : "only ") << action_count
       << " WillOnce()" << (action_count == 1 ? "" : "s");
    if (repeated_action_specified_) {
      ss << " and a WillRepeatedly()";
    }
    ss << ".";
    Log(kWarning, ss.str(), -1);
  }
}

void ExpectationBase::UntypedTimes(const Cardinality& a_cardinality) {
  if (last_clause_ == kTimes) {
    ExpectSpecProperty(false,
                       ".Times() cannot appear "
                       "more than once in an EXPECT_CALL().");
  } else {
    ExpectSpecProperty(
        last_clause_ < kTimes,
        ".Times() may only appear *before* .InSequence(), .WillOnce(), "
        ".WillRepeatedly(), or .RetiresOnSaturation(), not after.");
  }
  last_clause_ = kTimes;

  SpecifyCardinality(a_cardinality);
}

GTEST_API_ ThreadLocal<Sequence*> g_gmock_implicit_sequence;

void ReportUninterestingCall(CallReaction reaction, const std::string& msg) {

  const int stack_frames_to_skip =
      GMOCK_FLAG_GET(verbose) == kInfoVerbosity ? 3 : -1;
  switch (reaction) {
    case kAllow:
      Log(kInfo, msg, stack_frames_to_skip);
      break;
    case kWarn:
      Log(kWarning,
          msg +
              "\nNOTE: You can safely ignore the above warning unless this "
              "call should not happen.  Do not suppress it by blindly adding "
              "an EXPECT_CALL() if you don't mean to enforce the call.  "
              "See "
              "https://github.com/google/googletest/blob/main/docs/"
              "gmock_cook_book.md#"
              "knowing-when-to-expect-useoncall for details.\n",
          stack_frames_to_skip);
      break;
    default:
      Expect(false, nullptr, -1, msg);
  }
}

UntypedFunctionMockerBase::UntypedFunctionMockerBase()
    : mock_obj_(nullptr), name_("") {}

UntypedFunctionMockerBase::~UntypedFunctionMockerBase() = default;

void UntypedFunctionMockerBase::RegisterOwner(const void* mock_obj)
    GTEST_LOCK_EXCLUDED_(g_gmock_mutex) {
  {
    MutexLock l(&g_gmock_mutex);
    mock_obj_ = mock_obj;
  }
  Mock::Register(mock_obj, this);
}

void UntypedFunctionMockerBase::SetOwnerAndName(const void* mock_obj,
                                                const char* name)
    GTEST_LOCK_EXCLUDED_(g_gmock_mutex) {

  MutexLock l(&g_gmock_mutex);
  mock_obj_ = mock_obj;
  name_ = name;
}

const void* UntypedFunctionMockerBase::MockObject() const
    GTEST_LOCK_EXCLUDED_(g_gmock_mutex) {
  const void* mock_obj;
  {

    MutexLock l(&g_gmock_mutex);
    Assert(mock_obj_ != nullptr, __FILE__, __LINE__,
           "MockObject() must not be called before RegisterOwner() or "
           "SetOwnerAndName() has been called.");
    mock_obj = mock_obj_;
  }
  return mock_obj;
}

const char* UntypedFunctionMockerBase::Name() const
    GTEST_LOCK_EXCLUDED_(g_gmock_mutex) {
  const char* name;
  {

    MutexLock l(&g_gmock_mutex);
    Assert(name_ != nullptr, __FILE__, __LINE__,
           "Name() must not be called before SetOwnerAndName() has "
           "been called.");
    name = name_;
  }
  return name;
}

Expectation UntypedFunctionMockerBase::GetHandleOf(ExpectationBase* exp) {

  for (UntypedExpectations::const_iterator it = untyped_expectations_.begin();
       it != untyped_expectations_.end(); ++it) {
    if (it->get() == exp) {
      return Expectation(*it);
    }
  }

  Assert(false, __FILE__, __LINE__, "Cannot find expectation.");
  return Expectation();

}

bool UntypedFunctionMockerBase::VerifyAndClearExpectationsLocked()
    GTEST_EXCLUSIVE_LOCK_REQUIRED_(g_gmock_mutex) {
  g_gmock_mutex.AssertHeld();
  bool expectations_met = true;
  for (UntypedExpectations::const_iterator it = untyped_expectations_.begin();
       it != untyped_expectations_.end(); ++it) {
    ExpectationBase* const untyped_expectation = it->get();
    if (untyped_expectation->IsOverSaturated()) {

      expectations_met = false;
    } else if (!untyped_expectation->IsSatisfied()) {
      expectations_met = false;
      ::std::stringstream ss;

      const ::std::string& expectation_name =
          untyped_expectation->GetDescription();
      ss << "Actual function ";
      if (!expectation_name.empty()) {
        ss << "\"" << expectation_name << "\" ";
      }
      ss << "call count doesn't match " << untyped_expectation->source_text()
         << "...\n";

      untyped_expectation->MaybeDescribeExtraMatcherTo(&ss);
      untyped_expectation->DescribeCallCountTo(&ss);
      Expect(false, untyped_expectation->file(), untyped_expectation->line(),
             ss.str());
    }
  }

  UntypedExpectations expectations_to_delete;
  untyped_expectations_.swap(expectations_to_delete);

  g_gmock_mutex.Unlock();
  expectations_to_delete.clear();
  g_gmock_mutex.Lock();

  return expectations_met;
}

static CallReaction intToCallReaction(int mock_behavior) {
  if (mock_behavior >= kAllow && mock_behavior <= kFail) {
    return static_cast<internal::CallReaction>(mock_behavior);
  }
  return kWarn;
}

}

namespace {

typedef std::set<internal::UntypedFunctionMockerBase*> FunctionMockers;

struct MockObjectState {
  MockObjectState()
      : first_used_file(nullptr), first_used_line(-1), leakable(false) {}

  const char* first_used_file;
  int first_used_line;
  ::std::string first_used_test_suite;
  ::std::string first_used_test;
  bool leakable;
  FunctionMockers function_mockers;
};

class MockObjectRegistry {
 public:

  typedef std::map<const void*, MockObjectState> StateMap;

  ~MockObjectRegistry() {
    if (!GMOCK_FLAG_GET(catch_leaked_mocks)) return;
    internal::MutexLock l(&internal::g_gmock_mutex);

    int leaked_count = 0;
    for (StateMap::const_iterator it = states_.begin(); it != states_.end();
         ++it) {
      if (it->second.leakable)
        continue;

      std::cout << "\n";
      const MockObjectState& state = it->second;
      std::cout << internal::FormatFileLocation(state.first_used_file,
                                                state.first_used_line);
      std::cout << " ERROR: this mock object";
      if (!state.first_used_test.empty()) {
        std::cout << " (used in test " << state.first_used_test_suite << "."
                  << state.first_used_test << ")";
      }
      std::cout << " should be deleted but never is. Its address is @"
                << it->first << ".";
      leaked_count++;
    }
    if (leaked_count > 0) {
      std::cout << "\nERROR: " << leaked_count << " leaked mock "
                << (leaked_count == 1 ? "object" : "objects")
                << " found at program exit. Expectations on a mock object are "
                   "verified when the object is destructed. Leaking a mock "
                   "means that its expectations aren't verified, which is "
                   "usually a test bug. If you really intend to leak a mock, "
                   "you can suppress this error using "
                   "testing::Mock::AllowLeak(mock_object), or you may use a "
                   "fake or stub instead of a mock.\n";
      std::cout.flush();
      ::std::cerr.flush();

#ifdef GTEST_OS_QURT
      qurt_exception_raise_fatal();
#else
      _Exit(1);

#endif
    }
  }

  StateMap& states() { return states_; }

 private:
  StateMap states_;
};

MockObjectRegistry g_mock_object_registry;

std::unordered_map<uintptr_t, internal::CallReaction>&
UninterestingCallReactionMap() {
  static auto* map = new std::unordered_map<uintptr_t, internal::CallReaction>;
  return *map;
}

void SetReactionOnUninterestingCalls(uintptr_t mock_obj,
                                     internal::CallReaction reaction)
    GTEST_LOCK_EXCLUDED_(internal::g_gmock_mutex) {
  internal::MutexLock l(&internal::g_gmock_mutex);
  UninterestingCallReactionMap()[mock_obj] = reaction;
}

}

void Mock::AllowUninterestingCalls(uintptr_t mock_obj)
    GTEST_LOCK_EXCLUDED_(internal::g_gmock_mutex) {
  SetReactionOnUninterestingCalls(mock_obj, internal::kAllow);
}

void Mock::WarnUninterestingCalls(uintptr_t mock_obj)
    GTEST_LOCK_EXCLUDED_(internal::g_gmock_mutex) {
  SetReactionOnUninterestingCalls(mock_obj, internal::kWarn);
}

void Mock::FailUninterestingCalls(uintptr_t mock_obj)
    GTEST_LOCK_EXCLUDED_(internal::g_gmock_mutex) {
  SetReactionOnUninterestingCalls(mock_obj, internal::kFail);
}

void Mock::UnregisterCallReaction(uintptr_t mock_obj)
    GTEST_LOCK_EXCLUDED_(internal::g_gmock_mutex) {
  internal::MutexLock l(&internal::g_gmock_mutex);
  UninterestingCallReactionMap().erase(static_cast<uintptr_t>(mock_obj));
}

internal::CallReaction Mock::GetReactionOnUninterestingCalls(
    const void* mock_obj) GTEST_LOCK_EXCLUDED_(internal::g_gmock_mutex) {
  internal::MutexLock l(&internal::g_gmock_mutex);
  return (UninterestingCallReactionMap().count(
              reinterpret_cast<uintptr_t>(mock_obj)) == 0)
             ? internal::intToCallReaction(
                   GMOCK_FLAG_GET(default_mock_behavior))
             : UninterestingCallReactionMap()[reinterpret_cast<uintptr_t>(
                   mock_obj)];
}

void Mock::AllowLeak(const void* mock_obj)
    GTEST_LOCK_EXCLUDED_(internal::g_gmock_mutex) {
  internal::MutexLock l(&internal::g_gmock_mutex);
  g_mock_object_registry.states()[mock_obj].leakable = true;
}

bool Mock::VerifyAndClearExpectations(void* mock_obj)
    GTEST_LOCK_EXCLUDED_(internal::g_gmock_mutex) {
  internal::MutexLock l(&internal::g_gmock_mutex);
  return VerifyAndClearExpectationsLocked(mock_obj);
}

bool Mock::VerifyAndClear(void* mock_obj)
    GTEST_LOCK_EXCLUDED_(internal::g_gmock_mutex) {
  internal::MutexLock l(&internal::g_gmock_mutex);
  ClearDefaultActionsLocked(mock_obj);
  return VerifyAndClearExpectationsLocked(mock_obj);
}

bool Mock::VerifyAndClearExpectationsLocked(void* mock_obj)
    GTEST_EXCLUSIVE_LOCK_REQUIRED_(internal::g_gmock_mutex) {
  internal::g_gmock_mutex.AssertHeld();
  if (g_mock_object_registry.states().count(mock_obj) == 0) {

    return true;
  }

  bool expectations_met = true;
  FunctionMockers& mockers =
      g_mock_object_registry.states()[mock_obj].function_mockers;
  for (FunctionMockers::const_iterator it = mockers.begin();
       it != mockers.end(); ++it) {
    if (!(*it)->VerifyAndClearExpectationsLocked()) {
      expectations_met = false;
    }
  }

  return expectations_met;
}

bool Mock::IsNaggy(void* mock_obj)
    GTEST_LOCK_EXCLUDED_(internal::g_gmock_mutex) {
  return Mock::GetReactionOnUninterestingCalls(mock_obj) == internal::kWarn;
}
bool Mock::IsNice(void* mock_obj)
    GTEST_LOCK_EXCLUDED_(internal::g_gmock_mutex) {
  return Mock::GetReactionOnUninterestingCalls(mock_obj) == internal::kAllow;
}
bool Mock::IsStrict(void* mock_obj)
    GTEST_LOCK_EXCLUDED_(internal::g_gmock_mutex) {
  return Mock::GetReactionOnUninterestingCalls(mock_obj) == internal::kFail;
}

void Mock::Register(const void* mock_obj,
                    internal::UntypedFunctionMockerBase* mocker)
    GTEST_LOCK_EXCLUDED_(internal::g_gmock_mutex) {
  internal::MutexLock l(&internal::g_gmock_mutex);
  g_mock_object_registry.states()[mock_obj].function_mockers.insert(mocker);
}

void Mock::RegisterUseByOnCallOrExpectCall(const void* mock_obj,
                                           const char* file, int line)
    GTEST_LOCK_EXCLUDED_(internal::g_gmock_mutex) {
  internal::MutexLock l(&internal::g_gmock_mutex);
  MockObjectState& state = g_mock_object_registry.states()[mock_obj];
  if (state.first_used_file == nullptr) {
    state.first_used_file = file;
    state.first_used_line = line;
    const TestInfo* const test_info =
        UnitTest::GetInstance()->current_test_info();
    if (test_info != nullptr) {
      state.first_used_test_suite = test_info->test_suite_name();
      state.first_used_test = test_info->name();
    }
  }
}

void Mock::UnregisterLocked(internal::UntypedFunctionMockerBase* mocker)
    GTEST_EXCLUSIVE_LOCK_REQUIRED_(internal::g_gmock_mutex) {
  internal::g_gmock_mutex.AssertHeld();
  for (MockObjectRegistry::StateMap::iterator it =
           g_mock_object_registry.states().begin();
       it != g_mock_object_registry.states().end(); ++it) {
    FunctionMockers& mockers = it->second.function_mockers;
    if (mockers.erase(mocker) > 0) {

      if (mockers.empty()) {
        g_mock_object_registry.states().erase(it);
      }
      return;
    }
  }
}

void Mock::ClearDefaultActionsLocked(void* mock_obj)
    GTEST_EXCLUSIVE_LOCK_REQUIRED_(internal::g_gmock_mutex) {
  internal::g_gmock_mutex.AssertHeld();

  if (g_mock_object_registry.states().count(mock_obj) == 0) {

    return;
  }

  FunctionMockers& mockers =
      g_mock_object_registry.states()[mock_obj].function_mockers;
  for (FunctionMockers::const_iterator it = mockers.begin();
       it != mockers.end(); ++it) {
    (*it)->ClearDefaultActionsLocked();
  }

}

Expectation::Expectation() = default;

Expectation::Expectation(
    const std::shared_ptr<internal::ExpectationBase>& an_expectation_base)
    : expectation_base_(an_expectation_base) {}

Expectation::~Expectation() = default;

void Sequence::AddExpectation(const Expectation& expectation) const {
  if (*last_expectation_ != expectation) {
    if (last_expectation_->expectation_base() != nullptr) {
      expectation.expectation_base()->immediate_prerequisites_ +=
          *last_expectation_;
    }
    *last_expectation_ = expectation;
  }
}

InSequence::InSequence() {
  if (internal::g_gmock_implicit_sequence.get() == nullptr) {
    internal::g_gmock_implicit_sequence.set(new Sequence);
    sequence_created_ = true;
  } else {
    sequence_created_ = false;
  }
}

InSequence::~InSequence() {
  if (sequence_created_) {
    delete internal::g_gmock_implicit_sequence.get();
    internal::g_gmock_implicit_sequence.set(nullptr);
  }
}

}

#if defined(_MSC_VER) && (_MSC_VER == 1900)
GTEST_DISABLE_MSC_WARNINGS_POP_()
#endif
