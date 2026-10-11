// Copyright 2017 The OpenSSL Project Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// Tests for X509 time functions.

#include <openssl/x509.h>

#include <string.h>
#include <time.h>

#include <gtest/gtest.h>
#include <openssl/asn1.h>

namespace {

struct TestData {
  const char *data;
  int type;
  int64_t cmp_time;

  int expected;
};

static TestData kX509CmpTests[] = {
    {
        "20170217180154Z",
        V_ASN1_GENERALIZEDTIME,

        1487354514,
        -1,
    },
    {
        "20170217180154Z",
        V_ASN1_GENERALIZEDTIME,

        1487354515,
        -1,
    },
    {
        "20170217180154Z",
        V_ASN1_GENERALIZEDTIME,

        1487354513,
        1,
    },

    {
        "170217180154Z",
        V_ASN1_UTCTIME,

        1487354514,
        -1,
    },
    {
        "170217180154Z",
        V_ASN1_UTCTIME,

        1487354515,
        -1,
    },
    {
        "170217180154Z",
        V_ASN1_UTCTIME,

        1487354513,
        1,
    },

    {
        "990217180154Z",
        V_ASN1_UTCTIME,

        919274514,
        -1,
    },
    {
        "990217180154Z",
        V_ASN1_UTCTIME,

        919274515,
        -1,
    },
    {
        "990217180154Z",
        V_ASN1_UTCTIME,

        919274513,
        1,
    },

    {

        "20170217180154",
        V_ASN1_GENERALIZEDTIME,
        0,
        0,
    },
    {

        "170217180154",
        V_ASN1_UTCTIME,
        0,
        0,
    },
    {

        "201702171801Z",
        V_ASN1_GENERALIZEDTIME,
        0,
        0,
    },
    {

        "1702171801Z",
        V_ASN1_UTCTIME,
        0,
        0,
    },
    {

        "20170217180154.001Z",
        V_ASN1_GENERALIZEDTIME,
        0,
        0,
    },
    {

        "170217180154.001Z",
        V_ASN1_UTCTIME,
        0,
        0,
    },
    {

        "20170217180154+0100",
        V_ASN1_GENERALIZEDTIME,
        0,
        0,
    },
    {

        "170217180154+0100",
        V_ASN1_UTCTIME,
        0,
        0,
    },
    {

        "2017021718015400Z",
        V_ASN1_GENERALIZEDTIME,
        0,
        0,
    },
    {

        "17021718015400Z",
        V_ASN1_UTCTIME,
        0,
        0,
    },
    {

        "2017021718015aZ",
        V_ASN1_GENERALIZEDTIME,
        0,
        0,
    },
    {

        "17021718015aZ",
        V_ASN1_UTCTIME,
        0,
        0,
    },
    {

        "20170217180154Zlongtrailinggarbage",
        V_ASN1_GENERALIZEDTIME,
        0,
        0,
    },
    {

        "170217180154Zlongtrailinggarbage",
        V_ASN1_UTCTIME,
        0,
        0,
    },
    {

        "20170217180154Z",
        V_ASN1_UTCTIME,
        0,
        0,
    },
    {

        "170217180154Z",
        V_ASN1_GENERALIZEDTIME,
        0,
        0,
    },
    {

        "20170217180154Z",
        V_ASN1_OCTET_STRING,
        0,
        0,
    },

    {
        "99991231235959Z", V_ASN1_GENERALIZEDTIME,

        253402300799,
        -1,
    },
    {
        "99991231235959Z", V_ASN1_GENERALIZEDTIME,

        253402300800,
        -1,
    },
    {
        "99991231235959Z",
        V_ASN1_GENERALIZEDTIME,

        253402300798,
        1,
    },
    {
        "700101000000Z",
        V_ASN1_UTCTIME,

        0,
        -1,
    },
    {
        "700101000000Z",
        V_ASN1_UTCTIME,

        -1,
        1,
    },
    {
        "700101000000Z",
        V_ASN1_UTCTIME,

        1,
        -1,
    },
    {
        "690621025615Z",
        V_ASN1_UTCTIME,

        -16751025,
        -1,
    },
    {
        "690621025615Z",
        V_ASN1_UTCTIME,

        -16751026,
        1,
    },
    {
        "690621025615Z",
        V_ASN1_UTCTIME,

        -16751024,
        -1,
    },
    {
        "00000101000000Z",
        V_ASN1_GENERALIZEDTIME,

        -62167219200,
        -1,
    },
    {
        "00000101000000Z",
        V_ASN1_GENERALIZEDTIME,

        -62167219199,
        -1,
    },

};

TEST(X509TimeTest, TestCmpTime) {
  for (auto &test : kX509CmpTests) {
    SCOPED_TRACE(test.data);

    bssl::UniquePtr<ASN1_STRING> t(ASN1_STRING_type_new(test.type));
    ASSERT_TRUE(t);
    ASSERT_TRUE(ASN1_STRING_set(t.get(), test.data, strlen(test.data)));

    EXPECT_EQ(test.expected, X509_cmp_time_posix(t.get(), test.cmp_time));
  }
}

TEST(X509TimeTest, TestCmpTimeCurrent) {
  time_t now = time(NULL);

  bssl::UniquePtr<ASN1_TIME> asn1_before(ASN1_TIME_adj(NULL, now, -1, 0));
  bssl::UniquePtr<ASN1_TIME> asn1_after(ASN1_TIME_adj(NULL, now, 1, 0));

  ASSERT_EQ(-1, X509_cmp_time(asn1_before.get(), NULL));
  ASSERT_EQ(1, X509_cmp_time(asn1_after.get(), NULL));
}

}
