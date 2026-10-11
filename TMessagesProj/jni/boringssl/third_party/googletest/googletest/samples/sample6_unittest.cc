// Copyright 2008 Google Inc.
// All Rights Reserved.
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

// This sample shows how to test common properties of multiple
// implementations of the same interface (aka interface tests).

// The interface and its implementations are in this header.
#include "prime_tables.h"
#include "gtest/gtest.h"
namespace {

template <class T>
PrimeTable* CreatePrimeTable();

template <>
PrimeTable* CreatePrimeTable<OnTheFlyPrimeTable>() {
  return new OnTheFlyPrimeTable;
}

template <>
PrimeTable* CreatePrimeTable<PreCalculatedPrimeTable>() {
  return new PreCalculatedPrimeTable(10000);
}

template <class T>
class PrimeTableTest : public testing::Test {
 protected:

  PrimeTableTest() : table_(CreatePrimeTable<T>()) {}

  ~PrimeTableTest() override { delete table_; }

  PrimeTable* const table_;
};

using testing::Types;

typedef Types<OnTheFlyPrimeTable, PreCalculatedPrimeTable> Implementations;

TYPED_TEST_SUITE(PrimeTableTest, Implementations);

TYPED_TEST(PrimeTableTest, ReturnsFalseForNonPrimes) {

  EXPECT_FALSE(this->table_->IsPrime(-5));
  EXPECT_FALSE(this->table_->IsPrime(0));
  EXPECT_FALSE(this->table_->IsPrime(1));
  EXPECT_FALSE(this->table_->IsPrime(4));
  EXPECT_FALSE(this->table_->IsPrime(6));
  EXPECT_FALSE(this->table_->IsPrime(100));
}

TYPED_TEST(PrimeTableTest, ReturnsTrueForPrimes) {
  EXPECT_TRUE(this->table_->IsPrime(2));
  EXPECT_TRUE(this->table_->IsPrime(3));
  EXPECT_TRUE(this->table_->IsPrime(5));
  EXPECT_TRUE(this->table_->IsPrime(7));
  EXPECT_TRUE(this->table_->IsPrime(11));
  EXPECT_TRUE(this->table_->IsPrime(131));
}

TYPED_TEST(PrimeTableTest, CanGetNextPrime) {
  EXPECT_EQ(2, this->table_->GetNextPrime(0));
  EXPECT_EQ(3, this->table_->GetNextPrime(2));
  EXPECT_EQ(5, this->table_->GetNextPrime(3));
  EXPECT_EQ(7, this->table_->GetNextPrime(5));
  EXPECT_EQ(11, this->table_->GetNextPrime(7));
  EXPECT_EQ(131, this->table_->GetNextPrime(128));
}

using testing::Types;

template <class T>
class PrimeTableTest2 : public PrimeTableTest<T> {};

TYPED_TEST_SUITE_P(PrimeTableTest2);

TYPED_TEST_P(PrimeTableTest2, ReturnsFalseForNonPrimes) {
  EXPECT_FALSE(this->table_->IsPrime(-5));
  EXPECT_FALSE(this->table_->IsPrime(0));
  EXPECT_FALSE(this->table_->IsPrime(1));
  EXPECT_FALSE(this->table_->IsPrime(4));
  EXPECT_FALSE(this->table_->IsPrime(6));
  EXPECT_FALSE(this->table_->IsPrime(100));
}

TYPED_TEST_P(PrimeTableTest2, ReturnsTrueForPrimes) {
  EXPECT_TRUE(this->table_->IsPrime(2));
  EXPECT_TRUE(this->table_->IsPrime(3));
  EXPECT_TRUE(this->table_->IsPrime(5));
  EXPECT_TRUE(this->table_->IsPrime(7));
  EXPECT_TRUE(this->table_->IsPrime(11));
  EXPECT_TRUE(this->table_->IsPrime(131));
}

TYPED_TEST_P(PrimeTableTest2, CanGetNextPrime) {
  EXPECT_EQ(2, this->table_->GetNextPrime(0));
  EXPECT_EQ(3, this->table_->GetNextPrime(2));
  EXPECT_EQ(5, this->table_->GetNextPrime(3));
  EXPECT_EQ(7, this->table_->GetNextPrime(5));
  EXPECT_EQ(11, this->table_->GetNextPrime(7));
  EXPECT_EQ(131, this->table_->GetNextPrime(128));
}

REGISTER_TYPED_TEST_SUITE_P(
    PrimeTableTest2,

    ReturnsFalseForNonPrimes, ReturnsTrueForPrimes, CanGetNextPrime);

typedef Types<OnTheFlyPrimeTable, PreCalculatedPrimeTable>
    PrimeTableImplementations;
INSTANTIATE_TYPED_TEST_SUITE_P(OnTheFlyAndPreCalculated,
                               PrimeTableTest2,
                               PrimeTableImplementations);

}
