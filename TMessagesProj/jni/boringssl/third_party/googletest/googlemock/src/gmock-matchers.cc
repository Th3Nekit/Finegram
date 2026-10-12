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
// This file implements Matcher<const string&>, Matcher<string>, and
// utilities for defining matchers.

#include "gmock/gmock-matchers.h"

#include <string.h>

#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace testing {
namespace internal {

GTEST_API_ std::string FormatMatcherDescription(
    bool negation, const char* matcher_name,
    const std::vector<const char*>& param_names, const Strings& param_values) {
  std::string result = ConvertIdentifierNameToWords(matcher_name);
  if (!param_values.empty()) {
    result += " " + JoinAsKeyValueTuple(param_names, param_values);
  }
  return negation ? "not (" + result + ")" : result;
}

class MaxBipartiteMatchState {
 public:
  explicit MaxBipartiteMatchState(const MatchMatrix& graph)
      : graph_(&graph),
        left_(graph_->LhsSize(), kUnused),
        right_(graph_->RhsSize(), kUnused) {}

  ElementMatcherPairs Compute() {

    ::std::vector<char> seen;

    for (size_t ilhs = 0; ilhs < graph_->LhsSize(); ++ilhs) {

      GTEST_CHECK_(left_[ilhs] == kUnused)
          << "ilhs: " << ilhs << ", left_[ilhs]: " << left_[ilhs];

      seen.assign(graph_->RhsSize(), 0);
      TryAugment(ilhs, &seen);
    }
    ElementMatcherPairs result;
    for (size_t ilhs = 0; ilhs < left_.size(); ++ilhs) {
      size_t irhs = left_[ilhs];
      if (irhs == kUnused) continue;
      result.push_back(ElementMatcherPair(ilhs, irhs));
    }
    return result;
  }

 private:
  static const size_t kUnused = static_cast<size_t>(-1);

  bool TryAugment(size_t ilhs, ::std::vector<char>* seen) {
    for (size_t irhs = 0; irhs < graph_->RhsSize(); ++irhs) {
      if ((*seen)[irhs]) continue;
      if (!graph_->HasEdge(ilhs, irhs)) continue;

      (*seen)[irhs] = 1;

      if (right_[irhs] == kUnused || TryAugment(right_[irhs], seen)) {

        left_[ilhs] = irhs;
        right_[irhs] = ilhs;
        return true;
      }
    }
    return false;
  }

  const MatchMatrix* graph_;

  ::std::vector<size_t> left_;
  ::std::vector<size_t> right_;
};

const size_t MaxBipartiteMatchState::kUnused;

GTEST_API_ ElementMatcherPairs FindMaxBipartiteMatching(const MatchMatrix& g) {
  return MaxBipartiteMatchState(g).Compute();
}

static void LogElementMatcherPairVec(const ElementMatcherPairs& pairs,
                                     ::std::ostream* stream) {
  typedef ElementMatcherPairs::const_iterator Iter;
  ::std::ostream& os = *stream;
  os << "{";
  const char* sep = "";
  for (Iter it = pairs.begin(); it != pairs.end(); ++it) {
    os << sep << "\n  (" << "element #" << it->first << ", " << "matcher #"
       << it->second << ")";
    sep = ",";
  }
  os << "\n}";
}

bool MatchMatrix::NextGraph() {
  for (size_t ilhs = 0; ilhs < LhsSize(); ++ilhs) {
    for (size_t irhs = 0; irhs < RhsSize(); ++irhs) {
      char& b = matched_[SpaceIndex(ilhs, irhs)];
      if (!b) {
        b = 1;
        return true;
      }
      b = 0;
    }
  }
  return false;
}

void MatchMatrix::Randomize() {
  for (size_t ilhs = 0; ilhs < LhsSize(); ++ilhs) {
    for (size_t irhs = 0; irhs < RhsSize(); ++irhs) {
      char& b = matched_[SpaceIndex(ilhs, irhs)];
      b = static_cast<char>(rand() & 1);
    }
  }
}

std::string MatchMatrix::DebugString() const {
  ::std::stringstream ss;
  const char* sep = "";
  for (size_t i = 0; i < LhsSize(); ++i) {
    ss << sep;
    for (size_t j = 0; j < RhsSize(); ++j) {
      ss << HasEdge(i, j);
    }
    sep = ";";
  }
  return ss.str();
}

void UnorderedElementsAreMatcherImplBase::DescribeToImpl(
    ::std::ostream* os) const {
  switch (match_flags()) {
    case UnorderedMatcherRequire::ExactMatch:
      if (matcher_describers_.empty()) {
        *os << "is empty";
        return;
      }
      if (matcher_describers_.size() == 1) {
        *os << "has " << Elements(1) << " and that element ";
        matcher_describers_[0]->DescribeTo(os);
        return;
      }
      *os << "has " << Elements(matcher_describers_.size())
          << " and there exists some permutation of elements such that:\n";
      break;
    case UnorderedMatcherRequire::Superset:
      *os << "a surjection from elements to requirements exists such that:\n";
      break;
    case UnorderedMatcherRequire::Subset:
      *os << "an injection from elements to requirements exists such that:\n";
      break;
  }

  const char* sep = "";
  for (size_t i = 0; i != matcher_describers_.size(); ++i) {
    *os << sep;
    if (match_flags() == UnorderedMatcherRequire::ExactMatch) {
      *os << " - element #" << i << " ";
    } else {
      *os << " - an element ";
    }
    matcher_describers_[i]->DescribeTo(os);
    if (match_flags() == UnorderedMatcherRequire::ExactMatch) {
      sep = ", and\n";
    } else {
      sep = "\n";
    }
  }
}

void UnorderedElementsAreMatcherImplBase::DescribeNegationToImpl(
    ::std::ostream* os) const {
  switch (match_flags()) {
    case UnorderedMatcherRequire::ExactMatch:
      if (matcher_describers_.empty()) {
        *os << "isn't empty";
        return;
      }
      if (matcher_describers_.size() == 1) {
        *os << "doesn't have " << Elements(1) << ", or has " << Elements(1)
            << " that ";
        matcher_describers_[0]->DescribeNegationTo(os);
        return;
      }
      *os << "doesn't have " << Elements(matcher_describers_.size())
          << ", or there exists no permutation of elements such that:\n";
      break;
    case UnorderedMatcherRequire::Superset:
      *os << "no surjection from elements to requirements exists such that:\n";
      break;
    case UnorderedMatcherRequire::Subset:
      *os << "no injection from elements to requirements exists such that:\n";
      break;
  }
  const char* sep = "";
  for (size_t i = 0; i != matcher_describers_.size(); ++i) {
    *os << sep;
    if (match_flags() == UnorderedMatcherRequire::ExactMatch) {
      *os << " - element #" << i << " ";
    } else {
      *os << " - an element ";
    }
    matcher_describers_[i]->DescribeTo(os);
    if (match_flags() == UnorderedMatcherRequire::ExactMatch) {
      sep = ", and\n";
    } else {
      sep = "\n";
    }
  }
}

bool UnorderedElementsAreMatcherImplBase::VerifyMatchMatrix(
    const ::std::vector<std::string>& element_printouts,
    const MatchMatrix& matrix, MatchResultListener* listener) const {
  if (matrix.LhsSize() == 0 && matrix.RhsSize() == 0) {
    return true;
  }

  const bool is_exact_match_with_size_discrepency =
      match_flags() == UnorderedMatcherRequire::ExactMatch &&
      matrix.LhsSize() != matrix.RhsSize();
  if (is_exact_match_with_size_discrepency) {

    if (matrix.LhsSize() != 0 && listener->IsInterested()) {
      *listener << "which has " << Elements(matrix.LhsSize()) << "\n";
    }
  }

  bool result = !is_exact_match_with_size_discrepency;
  ::std::vector<char> element_matched(matrix.LhsSize(), 0);
  ::std::vector<char> matcher_matched(matrix.RhsSize(), 0);

  for (size_t ilhs = 0; ilhs < matrix.LhsSize(); ilhs++) {
    for (size_t irhs = 0; irhs < matrix.RhsSize(); irhs++) {
      char matched = matrix.HasEdge(ilhs, irhs);
      element_matched[ilhs] |= matched;
      matcher_matched[irhs] |= matched;
    }
  }

  if (match_flags() & UnorderedMatcherRequire::Superset) {
    const char* sep =
        "where the following matchers don't match any elements:\n";
    for (size_t mi = 0; mi < matcher_matched.size(); ++mi) {
      if (matcher_matched[mi]) continue;
      result = false;
      if (listener->IsInterested()) {
        *listener << sep << "matcher #" << mi << ": ";
        matcher_describers_[mi]->DescribeTo(listener->stream());
        sep = ",\n";
      }
    }
  }

  if (match_flags() & UnorderedMatcherRequire::Subset) {
    const char* sep =
        "where the following elements don't match any matchers:\n";
    const char* outer_sep = "";
    if (!result) {
      outer_sep = "\nand ";
    }
    for (size_t ei = 0; ei < element_matched.size(); ++ei) {
      if (element_matched[ei]) continue;
      result = false;
      if (listener->IsInterested()) {
        *listener << outer_sep << sep << "element #" << ei << ": "
                  << element_printouts[ei];
        sep = ",\n";
        outer_sep = "";
      }
    }
  }
  return result;
}

bool UnorderedElementsAreMatcherImplBase::FindPairing(
    const MatchMatrix& matrix, MatchResultListener* listener) const {
  ElementMatcherPairs matches = FindMaxBipartiteMatching(matrix);

  size_t max_flow = matches.size();
  if ((match_flags() & UnorderedMatcherRequire::Superset) &&
      max_flow < matrix.RhsSize()) {
    if (listener->IsInterested()) {
      *listener << "where no permutation of the elements can satisfy all "
                   "matchers, and the closest match is "
                << max_flow << " of " << matrix.RhsSize()
                << " matchers with the pairings:\n";
      LogElementMatcherPairVec(matches, listener->stream());
    }
    return false;
  }
  if ((match_flags() & UnorderedMatcherRequire::Subset) &&
      max_flow < matrix.LhsSize()) {
    if (listener->IsInterested()) {
      *listener
          << "where not all elements can be matched, and the closest match is "
          << max_flow << " of " << matrix.RhsSize()
          << " matchers with the pairings:\n";
      LogElementMatcherPairVec(matches, listener->stream());
    }
    return false;
  }

  if (matches.size() > 1) {
    if (listener->IsInterested()) {
      const char* sep = "where:\n";
      for (size_t mi = 0; mi < matches.size(); ++mi) {
        *listener << sep << " - element #" << matches[mi].first
                  << " is matched by matcher #" << matches[mi].second;
        sep = ",\n";
      }
    }
  }
  return true;
}

}
}
