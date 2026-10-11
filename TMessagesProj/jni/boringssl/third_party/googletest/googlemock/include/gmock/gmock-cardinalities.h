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
// This file implements some commonly used cardinalities.  More
// cardinalities can be defined by the user implementing the
// CardinalityInterface interface if necessary.

// IWYU pragma: private, include "gmock/gmock.h"
// IWYU pragma: friend gmock/.*

#ifndef GOOGLEMOCK_INCLUDE_GMOCK_GMOCK_CARDINALITIES_H_
#define GOOGLEMOCK_INCLUDE_GMOCK_GMOCK_CARDINALITIES_H_

#include <limits.h>

#include <memory>
#include <ostream>

#include "gmock/internal/gmock-port.h"
#include "gtest/gtest.h"

GTEST_DISABLE_MSC_WARNINGS_PUSH_(4251 \
                                                                          )

namespace testing {

class CardinalityInterface {
 public:
  virtual ~CardinalityInterface() = default;

  virtual int ConservativeLowerBound() const { return 0; }
  virtual int ConservativeUpperBound() const { return INT_MAX; }

  virtual bool IsSatisfiedByCallCount(int call_count) const = 0;

  virtual bool IsSaturatedByCallCount(int call_count) const = 0;

  virtual void DescribeTo(::std::ostream* os) const = 0;
};

class GTEST_API_ Cardinality {
 public:

  Cardinality() = default;

  explicit Cardinality(const CardinalityInterface* impl) : impl_(impl) {}

  int ConservativeLowerBound() const { return impl_->ConservativeLowerBound(); }
  int ConservativeUpperBound() const { return impl_->ConservativeUpperBound(); }

  bool IsSatisfiedByCallCount(int call_count) const {
    return impl_->IsSatisfiedByCallCount(call_count);
  }

  bool IsSaturatedByCallCount(int call_count) const {
    return impl_->IsSaturatedByCallCount(call_count);
  }

  bool IsOverSaturatedByCallCount(int call_count) const {
    return impl_->IsSaturatedByCallCount(call_count) &&
           !impl_->IsSatisfiedByCallCount(call_count);
  }

  void DescribeTo(::std::ostream* os) const { impl_->DescribeTo(os); }

  static void DescribeActualCallCountTo(int actual_call_count,
                                        ::std::ostream* os);

 private:
  std::shared_ptr<const CardinalityInterface> impl_;
};

GTEST_API_ Cardinality AtLeast(int n);

GTEST_API_ Cardinality AtMost(int n);

GTEST_API_ Cardinality AnyNumber();

GTEST_API_ Cardinality Between(int min, int max);

GTEST_API_ Cardinality Exactly(int n);

inline Cardinality MakeCardinality(const CardinalityInterface* c) {
  return Cardinality(c);
}

}

GTEST_DISABLE_MSC_WARNINGS_POP_()

#endif
