// Copyright 2015 The Chromium Authors
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

#include "verify_name_match.h"

#include <gtest/gtest.h>
#include "string_util.h"
#include "test_helpers.h"

BSSL_NAMESPACE_BEGIN
namespace {

::testing::AssertionResult LoadTestData(const std::string &prefix,
                                        const std::string &value_type,
                                        const std::string &suffix,
                                        std::string *result) {
  std::string path = "testdata/verify_name_match_unittest/names/" + prefix +
                     "-" + value_type + "-" + suffix + ".pem";

  const PemBlockMapping mappings[] = {
      {"NAME", result},
  };

  return ReadTestDataFromPemFile(path, mappings);
}

bool TypesAreComparable(const std::string &type_1, const std::string &type_2) {
  if (type_1 == type_2) {
    return true;
  }
  if ((type_1 == "PRINTABLESTRING" || type_1 == "UTF8" ||
       type_1 == "BMPSTRING" || type_1 == "UNIVERSALSTRING") &&
      (type_2 == "PRINTABLESTRING" || type_2 == "UTF8" ||
       type_2 == "BMPSTRING" || type_2 == "UNIVERSALSTRING")) {
    return true;
  }
  return false;
}

static const char *kValueTypes[] = {"PRINTABLESTRING", "T61STRING", "UTF8",
                                    "BMPSTRING", "UNIVERSALSTRING"};

static const char *kUnicodeBMPValueTypes[] = {"UTF8", "BMPSTRING",
                                              "UNIVERSALSTRING"};

static const char *kUnicodeSupplementaryValueTypes[] = {"UTF8",
                                                        "UNIVERSALSTRING"};

static const char *kMangleTypes[] = {"unmangled", "case_swap",
                                     "extra_whitespace"};

}

class VerifyNameMatchSimpleTest
    : public ::testing::TestWithParam<
          ::testing::tuple<const char *, const char *>> {
 public:
  std::string value_type() const { return ::testing::get<0>(GetParam()); }
  std::string suffix() const { return ::testing::get<1>(GetParam()); }
};

TEST_P(VerifyNameMatchSimpleTest, ExactEquality) {
  std::string der;
  ASSERT_TRUE(LoadTestData("ascii", value_type(), suffix(), &der));
  EXPECT_TRUE(VerifyNameMatch(SequenceValueFromString(der),
                              SequenceValueFromString(der)));

  std::string der_extra_attr;
  ASSERT_TRUE(LoadTestData("ascii", value_type(), suffix() + "-extra_attr",
                           &der_extra_attr));
  EXPECT_TRUE(VerifyNameMatch(SequenceValueFromString(der_extra_attr),
                              SequenceValueFromString(der_extra_attr)));

  std::string der_extra_rdn;
  ASSERT_TRUE(LoadTestData("ascii", value_type(), suffix() + "-extra_rdn",
                           &der_extra_rdn));
  EXPECT_TRUE(VerifyNameMatch(SequenceValueFromString(der_extra_rdn),
                              SequenceValueFromString(der_extra_rdn)));
}

TEST_P(VerifyNameMatchSimpleTest, ExtraAttrDoesNotMatch) {
  std::string der;
  ASSERT_TRUE(LoadTestData("ascii", value_type(), suffix(), &der));
  std::string der_extra_attr;
  ASSERT_TRUE(LoadTestData("ascii", value_type(), suffix() + "-extra_attr",
                           &der_extra_attr));
  EXPECT_FALSE(VerifyNameMatch(SequenceValueFromString(der),
                               SequenceValueFromString(der_extra_attr)));
  EXPECT_FALSE(VerifyNameMatch(SequenceValueFromString(der_extra_attr),
                               SequenceValueFromString(der)));
}

TEST_P(VerifyNameMatchSimpleTest, DupeAttrDoesNotMatch) {
  std::string der_dupe_attr;
  ASSERT_TRUE(LoadTestData("ascii", value_type(), suffix() + "-dupe_attr",
                           &der_dupe_attr));
  std::string der_extra_attr;
  ASSERT_TRUE(LoadTestData("ascii", value_type(), suffix() + "-extra_attr",
                           &der_extra_attr));
  EXPECT_FALSE(VerifyNameMatch(SequenceValueFromString(der_dupe_attr),
                               SequenceValueFromString(der_extra_attr)));
  EXPECT_FALSE(VerifyNameMatch(SequenceValueFromString(der_extra_attr),
                               SequenceValueFromString(der_dupe_attr)));

  EXPECT_TRUE(VerifyNameMatch(SequenceValueFromString(der_dupe_attr),
                              SequenceValueFromString(der_dupe_attr)));
}

TEST_P(VerifyNameMatchSimpleTest, ExtraRdnDoesNotMatch) {
  std::string der;
  ASSERT_TRUE(LoadTestData("ascii", value_type(), suffix(), &der));
  std::string der_extra_rdn;
  ASSERT_TRUE(LoadTestData("ascii", value_type(), suffix() + "-extra_rdn",
                           &der_extra_rdn));
  EXPECT_FALSE(VerifyNameMatch(SequenceValueFromString(der),
                               SequenceValueFromString(der_extra_rdn)));
  EXPECT_FALSE(VerifyNameMatch(SequenceValueFromString(der_extra_rdn),
                               SequenceValueFromString(der)));
}

INSTANTIATE_TEST_SUITE_P(InstantiationName, VerifyNameMatchSimpleTest,
                         ::testing::Combine(::testing::ValuesIn(kValueTypes),
                                            ::testing::ValuesIn(kMangleTypes)));

class VerifyNameMatchNormalizationTest
    : public ::testing::TestWithParam<::testing::tuple<bool, const char *>> {
 public:
  bool expected_result() const { return ::testing::get<0>(GetParam()); }
  std::string value_type() const { return ::testing::get<1>(GetParam()); }
};

TEST_P(VerifyNameMatchNormalizationTest, CaseInsensitivity) {
  std::string normal;
  ASSERT_TRUE(LoadTestData("ascii", value_type(), "unmangled", &normal));
  std::string case_swap;
  ASSERT_TRUE(LoadTestData("ascii", value_type(), "case_swap", &case_swap));
  EXPECT_EQ(expected_result(),
            VerifyNameMatch(SequenceValueFromString(normal),
                            SequenceValueFromString(case_swap)));
  EXPECT_EQ(expected_result(),
            VerifyNameMatch(SequenceValueFromString(case_swap),
                            SequenceValueFromString(normal)));
}

TEST_P(VerifyNameMatchNormalizationTest, CollapseWhitespace) {
  std::string normal;
  ASSERT_TRUE(LoadTestData("ascii", value_type(), "unmangled", &normal));
  std::string whitespace;
  ASSERT_TRUE(
      LoadTestData("ascii", value_type(), "extra_whitespace", &whitespace));
  EXPECT_EQ(expected_result(),
            VerifyNameMatch(SequenceValueFromString(normal),
                            SequenceValueFromString(whitespace)));
  EXPECT_EQ(expected_result(),
            VerifyNameMatch(SequenceValueFromString(whitespace),
                            SequenceValueFromString(normal)));
}

INSTANTIATE_TEST_SUITE_P(
    InstantiationName, VerifyNameMatchNormalizationTest,
    ::testing::Values(
        ::testing::make_tuple(true,
                              static_cast<const char *>("PRINTABLESTRING")),
        ::testing::make_tuple(false, static_cast<const char *>("T61STRING")),
        ::testing::make_tuple(true, static_cast<const char *>("UTF8")),
        ::testing::make_tuple(true, static_cast<const char *>("BMPSTRING")),
        ::testing::make_tuple(true,
                              static_cast<const char *>("UNIVERSALSTRING"))));

class VerifyNameMatchDifferingTypesTest
    : public ::testing::TestWithParam<
          ::testing::tuple<const char *, const char *>> {
 public:
  std::string value_type_1() const { return ::testing::get<0>(GetParam()); }
  std::string value_type_2() const { return ::testing::get<1>(GetParam()); }
};

TEST_P(VerifyNameMatchDifferingTypesTest, NormalizableTypesAreEqual) {
  std::string der_1;
  ASSERT_TRUE(LoadTestData("ascii", value_type_1(), "unmangled", &der_1));
  std::string der_2;
  ASSERT_TRUE(LoadTestData("ascii", value_type_2(), "unmangled", &der_2));
  if (TypesAreComparable(value_type_1(), value_type_2())) {
    EXPECT_TRUE(VerifyNameMatch(SequenceValueFromString(der_1),
                                SequenceValueFromString(der_2)));
  } else {
    EXPECT_FALSE(VerifyNameMatch(SequenceValueFromString(der_1),
                                 SequenceValueFromString(der_2)));
  }
}

TEST_P(VerifyNameMatchDifferingTypesTest, NormalizableTypesInSubtrees) {
  std::string der_1;
  ASSERT_TRUE(LoadTestData("ascii", value_type_1(), "unmangled", &der_1));
  std::string der_1_extra_rdn;
  ASSERT_TRUE(LoadTestData("ascii", value_type_1(), "unmangled-extra_rdn",
                           &der_1_extra_rdn));
  std::string der_1_extra_attr;
  ASSERT_TRUE(LoadTestData("ascii", value_type_1(), "unmangled-extra_attr",
                           &der_1_extra_attr));
  std::string der_2;
  ASSERT_TRUE(LoadTestData("ascii", value_type_2(), "unmangled", &der_2));
  std::string der_2_extra_rdn;
  ASSERT_TRUE(LoadTestData("ascii", value_type_2(), "unmangled-extra_rdn",
                           &der_2_extra_rdn));
  std::string der_2_extra_attr;
  ASSERT_TRUE(LoadTestData("ascii", value_type_2(), "unmangled-extra_attr",
                           &der_2_extra_attr));

  if (TypesAreComparable(value_type_1(), value_type_2())) {
    EXPECT_TRUE(VerifyNameInSubtree(SequenceValueFromString(der_1),
                                    SequenceValueFromString(der_2)));
    EXPECT_TRUE(VerifyNameInSubtree(SequenceValueFromString(der_2),
                                    SequenceValueFromString(der_1)));
    EXPECT_TRUE(VerifyNameInSubtree(SequenceValueFromString(der_1_extra_rdn),
                                    SequenceValueFromString(der_2)));
    EXPECT_TRUE(VerifyNameInSubtree(SequenceValueFromString(der_2_extra_rdn),
                                    SequenceValueFromString(der_1)));
  } else {
    EXPECT_FALSE(VerifyNameInSubtree(SequenceValueFromString(der_1),
                                     SequenceValueFromString(der_2)));
    EXPECT_FALSE(VerifyNameInSubtree(SequenceValueFromString(der_2),
                                     SequenceValueFromString(der_1)));
    EXPECT_FALSE(VerifyNameInSubtree(SequenceValueFromString(der_1_extra_rdn),
                                     SequenceValueFromString(der_2)));
    EXPECT_FALSE(VerifyNameInSubtree(SequenceValueFromString(der_2_extra_rdn),
                                     SequenceValueFromString(der_1)));
  }

  EXPECT_FALSE(VerifyNameInSubtree(SequenceValueFromString(der_1),
                                   SequenceValueFromString(der_2_extra_rdn)));
  EXPECT_FALSE(VerifyNameInSubtree(SequenceValueFromString(der_2),
                                   SequenceValueFromString(der_1_extra_rdn)));
  EXPECT_FALSE(VerifyNameInSubtree(SequenceValueFromString(der_1_extra_attr),
                                   SequenceValueFromString(der_2)));
  EXPECT_FALSE(VerifyNameInSubtree(SequenceValueFromString(der_2_extra_attr),
                                   SequenceValueFromString(der_1)));
  EXPECT_FALSE(VerifyNameInSubtree(SequenceValueFromString(der_1),
                                   SequenceValueFromString(der_2_extra_attr)));
  EXPECT_FALSE(VerifyNameInSubtree(SequenceValueFromString(der_2),
                                   SequenceValueFromString(der_1_extra_attr)));
}

INSTANTIATE_TEST_SUITE_P(InstantiationName, VerifyNameMatchDifferingTypesTest,
                         ::testing::Combine(::testing::ValuesIn(kValueTypes),
                                            ::testing::ValuesIn(kValueTypes)));

class VerifyNameMatchUnicodeConversionTest
    : public ::testing::TestWithParam<::testing::tuple<
          const char *, ::testing::tuple<const char *, const char *>>> {
 public:
  std::string prefix() const { return ::testing::get<0>(GetParam()); }
  std::string value_type_1() const {
    return ::testing::get<0>(::testing::get<1>(GetParam()));
  }
  std::string value_type_2() const {
    return ::testing::get<1>(::testing::get<1>(GetParam()));
  }
};

TEST_P(VerifyNameMatchUnicodeConversionTest, UnicodeConversionsAreEqual) {
  std::string der_1;
  ASSERT_TRUE(LoadTestData(prefix(), value_type_1(), "unmangled", &der_1));
  std::string der_2;
  ASSERT_TRUE(LoadTestData(prefix(), value_type_2(), "unmangled", &der_2));
  EXPECT_TRUE(VerifyNameMatch(SequenceValueFromString(der_1),
                              SequenceValueFromString(der_2)));
}

INSTANTIATE_TEST_SUITE_P(
    BMPConversion, VerifyNameMatchUnicodeConversionTest,
    ::testing::Combine(
        ::testing::Values("unicode_bmp"),
        ::testing::Combine(::testing::ValuesIn(kUnicodeBMPValueTypes),
                           ::testing::ValuesIn(kUnicodeBMPValueTypes))));

INSTANTIATE_TEST_SUITE_P(
    SMPConversion, VerifyNameMatchUnicodeConversionTest,
    ::testing::Combine(
        ::testing::Values("unicode_supplementary"),
        ::testing::Combine(
            ::testing::ValuesIn(kUnicodeSupplementaryValueTypes),
            ::testing::ValuesIn(kUnicodeSupplementaryValueTypes))));

TEST(VerifyNameMatchInvalidDataTest, FailOnInvalidPrintableStringChars) {
  std::string der;
  ASSERT_TRUE(LoadTestData("ascii", "PRINTABLESTRING", "unmangled", &der));

  size_t replace_location = der.find("0123456789");
  ASSERT_NE(std::string::npos, replace_location);
  for (int c = 0; c < 256; ++c) {
    SCOPED_TRACE(c);
    if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
        (c >= '0' && c <= '9')) {
      continue;
    }
    switch (c) {
      case ' ':
      case '\'':
      case '(':
      case ')':
      case '*':
      case '+':
      case ',':
      case '-':
      case '.':
      case '/':
      case ':':
      case '=':
      case '?':
        continue;
    }
    der.replace(replace_location, 1, 1, c);

    EXPECT_FALSE(VerifyNameMatch(SequenceValueFromString(der),
                                 SequenceValueFromString(der)));
    std::string normalized_der;
    CertErrors errors;
    EXPECT_FALSE(
        NormalizeName(SequenceValueFromString(der), &normalized_der, &errors));
  }
}

TEST(VerifyNameMatchInvalidDataTest, FailOnInvalidIA5StringChars) {
  std::string der;
  ASSERT_TRUE(LoadTestData("ascii", "mixed", "rdn_dupetype_sorting_1", &der));

  size_t replace_location = der.find("eXaMple");
  ASSERT_NE(std::string::npos, replace_location);
  for (int c = 0; c < 256; ++c) {
    SCOPED_TRACE(c);
    der.replace(replace_location, 1, 1, c);
    bool expected_result = (c <= 127);
    EXPECT_EQ(expected_result, VerifyNameMatch(SequenceValueFromString(der),
                                               SequenceValueFromString(der)));
    std::string normalized_der;
    CertErrors errors;
    EXPECT_EQ(expected_result, NormalizeName(SequenceValueFromString(der),
                                             &normalized_der, &errors));
  }
}

TEST(VerifyNameMatchInvalidDataTest, FailOnAttributeTypeAndValueExtraData) {
  std::string invalid;
  ASSERT_TRUE(
      LoadTestData("invalid", "AttributeTypeAndValue", "extradata", &invalid));

  EXPECT_FALSE(VerifyNameMatch(SequenceValueFromString(invalid),
                               SequenceValueFromString(invalid)));
  std::string normalized_der;
  CertErrors errors;
  EXPECT_FALSE(NormalizeName(SequenceValueFromString(invalid), &normalized_der,
                             &errors));
}

TEST(VerifyNameMatchInvalidDataTest, FailOnAttributeTypeAndValueShort) {
  std::string invalid;
  ASSERT_TRUE(LoadTestData("invalid", "AttributeTypeAndValue", "onlyOneElement",
                           &invalid));

  EXPECT_FALSE(VerifyNameMatch(SequenceValueFromString(invalid),
                               SequenceValueFromString(invalid)));
  std::string normalized_der;
  CertErrors errors;
  EXPECT_FALSE(NormalizeName(SequenceValueFromString(invalid), &normalized_der,
                             &errors));
}

TEST(VerifyNameMatchInvalidDataTest, FailOnAttributeTypeAndValueEmpty) {
  std::string invalid;
  ASSERT_TRUE(
      LoadTestData("invalid", "AttributeTypeAndValue", "empty", &invalid));

  EXPECT_FALSE(VerifyNameMatch(SequenceValueFromString(invalid),
                               SequenceValueFromString(invalid)));
  std::string normalized_der;
  CertErrors errors;
  EXPECT_FALSE(NormalizeName(SequenceValueFromString(invalid), &normalized_der,
                             &errors));
}

TEST(VerifyNameMatchInvalidDataTest, FailOnBadAttributeType) {
  std::string invalid;
  ASSERT_TRUE(LoadTestData("invalid", "AttributeTypeAndValue",
                           "badAttributeType", &invalid));

  EXPECT_FALSE(VerifyNameMatch(SequenceValueFromString(invalid),
                               SequenceValueFromString(invalid)));
  std::string normalized_der;
  CertErrors errors;
  EXPECT_FALSE(NormalizeName(SequenceValueFromString(invalid), &normalized_der,
                             &errors));
}

TEST(VerifyNameMatchInvalidDataTest, FailOnAttributeTypeAndValueNotSequence) {
  std::string invalid;
  ASSERT_TRUE(LoadTestData("invalid", "AttributeTypeAndValue", "setNotSequence",
                           &invalid));

  EXPECT_FALSE(VerifyNameMatch(SequenceValueFromString(invalid),
                               SequenceValueFromString(invalid)));
  std::string normalized_der;
  CertErrors errors;
  EXPECT_FALSE(NormalizeName(SequenceValueFromString(invalid), &normalized_der,
                             &errors));
}

TEST(VerifyNameMatchInvalidDataTest, FailOnRdnNotSet) {
  std::string invalid;
  ASSERT_TRUE(LoadTestData("invalid", "RDN", "sequenceInsteadOfSet", &invalid));

  EXPECT_FALSE(VerifyNameMatch(SequenceValueFromString(invalid),
                               SequenceValueFromString(invalid)));
  std::string normalized_der;
  CertErrors errors;
  EXPECT_FALSE(NormalizeName(SequenceValueFromString(invalid), &normalized_der,
                             &errors));
}

TEST(VerifyNameMatchInvalidDataTest, FailOnEmptyRdn) {
  std::string invalid;
  ASSERT_TRUE(LoadTestData("invalid", "RDN", "empty", &invalid));

  EXPECT_FALSE(VerifyNameMatch(SequenceValueFromString(invalid),
                               SequenceValueFromString(invalid)));
  std::string normalized_der;
  CertErrors errors;
  EXPECT_FALSE(NormalizeName(SequenceValueFromString(invalid), &normalized_der,
                             &errors));
}

TEST(VerifyNameMatchInvalidDataTest, FailOnBmpStringSurrogates) {
  std::string normal;
  ASSERT_TRUE(LoadTestData("unicode_bmp", "BMPSTRING", "unmangled", &normal));

  size_t replace_location = normal.find("\x67\x71\x4e\xac");
  ASSERT_NE(std::string::npos, replace_location);

  std::string invalid =
      normal.replace(replace_location, 4, std::string("\xd8\x35\xdc\x00", 4));

  EXPECT_FALSE(VerifyNameMatch(SequenceValueFromString(invalid),
                               SequenceValueFromString(invalid)));
  std::string normalized_der;
  CertErrors errors;
  EXPECT_FALSE(NormalizeName(SequenceValueFromString(invalid), &normalized_der,
                             &errors));
}

TEST(VerifyNameMatchTest, EmptyNameMatching) {
  std::string empty;
  ASSERT_TRUE(LoadTestData("valid", "Name", "empty", &empty));

  EXPECT_TRUE(VerifyNameMatch(SequenceValueFromString(empty),
                              SequenceValueFromString(empty)));

  std::string normalized_empty_der;
  CertErrors errors;
  EXPECT_TRUE(NormalizeName(SequenceValueFromString(empty),
                            &normalized_empty_der, &errors));
  EXPECT_EQ(SequenceValueFromString(empty), der::Input(normalized_empty_der));

  std::string non_empty;
  ASSERT_TRUE(
      LoadTestData("ascii", "PRINTABLESTRING", "unmangled", &non_empty));
  EXPECT_FALSE(VerifyNameMatch(SequenceValueFromString(empty),
                               SequenceValueFromString(non_empty)));
  EXPECT_FALSE(VerifyNameMatch(SequenceValueFromString(non_empty),
                               SequenceValueFromString(empty)));
}

TEST(VerifyNameMatchRDNSorting, Simple) {
  std::string a;
  ASSERT_TRUE(LoadTestData("ascii", "PRINTABLESTRING", "rdn_sorting_1", &a));
  std::string b;
  ASSERT_TRUE(LoadTestData("ascii", "PRINTABLESTRING", "rdn_sorting_2", &b));
  EXPECT_TRUE(
      VerifyNameMatch(SequenceValueFromString(a), SequenceValueFromString(b)));
  EXPECT_TRUE(
      VerifyNameMatch(SequenceValueFromString(b), SequenceValueFromString(a)));
}

TEST(VerifyNameMatchRDNSorting, DuplicateTypes) {
  std::string a;
  ASSERT_TRUE(LoadTestData("ascii", "mixed", "rdn_dupetype_sorting_1", &a));
  std::string b;
  ASSERT_TRUE(LoadTestData("ascii", "mixed", "rdn_dupetype_sorting_2", &b));
  EXPECT_TRUE(
      VerifyNameMatch(SequenceValueFromString(a), SequenceValueFromString(b)));
  EXPECT_TRUE(
      VerifyNameMatch(SequenceValueFromString(b), SequenceValueFromString(a)));
}

TEST(VerifyNameInSubtreeInvalidDataTest, FailOnEmptyRdn) {
  std::string valid;
  ASSERT_TRUE(LoadTestData("ascii", "PRINTABLESTRING", "unmangled", &valid));
  std::string invalid;
  ASSERT_TRUE(LoadTestData("invalid", "RDN", "empty", &invalid));

  EXPECT_FALSE(VerifyNameInSubtree(SequenceValueFromString(valid),
                                   SequenceValueFromString(invalid)));
  EXPECT_FALSE(VerifyNameInSubtree(SequenceValueFromString(invalid),
                                   SequenceValueFromString(valid)));
  EXPECT_FALSE(VerifyNameInSubtree(SequenceValueFromString(invalid),
                                   SequenceValueFromString(invalid)));
}

TEST(VerifyNameInSubtreeTest, EmptyNameMatching) {
  std::string empty;
  ASSERT_TRUE(LoadTestData("valid", "Name", "empty", &empty));
  std::string non_empty;
  ASSERT_TRUE(
      LoadTestData("ascii", "PRINTABLESTRING", "unmangled", &non_empty));

  EXPECT_TRUE(VerifyNameInSubtree(SequenceValueFromString(empty),
                                  SequenceValueFromString(empty)));

  EXPECT_TRUE(VerifyNameInSubtree(SequenceValueFromString(non_empty),
                                  SequenceValueFromString(empty)));

  EXPECT_FALSE(VerifyNameInSubtree(SequenceValueFromString(empty),
                                   SequenceValueFromString(non_empty)));
}

TEST(NameNormalizationTest, TestEverything) {
  std::string expected_normalized_der;
  ASSERT_TRUE(
      LoadTestData("unicode", "mixed", "normalized", &expected_normalized_der));

  std::string raw_der;
  ASSERT_TRUE(LoadTestData("unicode", "mixed", "unnormalized", &raw_der));
  std::string normalized_der;
  CertErrors errors;
  ASSERT_TRUE(NormalizeName(SequenceValueFromString(raw_der), &normalized_der,
                            &errors));
  EXPECT_EQ(SequenceValueFromString(expected_normalized_der),
            der::Input(normalized_der));

  std::string renormalized_der;
  ASSERT_TRUE(
      NormalizeName(der::Input(normalized_der), &renormalized_der, &errors));
  EXPECT_EQ(normalized_der, renormalized_der);
}

TEST(NameNormalizationTest, NormalizeCustom) {
  std::string raw_der;
  ASSERT_TRUE(LoadTestData("custom", "custom", "normalized", &raw_der));

  std::string normalized_der;
  CertErrors errors;
  ASSERT_TRUE(NormalizeName(SequenceValueFromString(raw_der), &normalized_der,
                            &errors));
  EXPECT_EQ(SequenceValueFromString(raw_der), der::Input(normalized_der));
}

BSSL_NAMESPACE_END
