// Copyright 2011 The Chromium Authors
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

#ifndef BSSL_PKI_PEM_H_
#define BSSL_PKI_PEM_H_

#include <stddef.h>

#include <string>
#include <string_view>
#include <vector>

#include <openssl/base.h>

BSSL_NAMESPACE_BEGIN

class OPENSSL_EXPORT PEMTokenizer {
 public:

  PEMTokenizer(std::string_view str,
               const std::vector<std::string> &allowed_block_types);

  PEMTokenizer(const PEMTokenizer &) = delete;
  PEMTokenizer &operator=(const PEMTokenizer &) = delete;

  ~PEMTokenizer();

  bool GetNext();

  const std::string &block_type() const { return block_type_; }

  const std::string &data() const { return data_; }

 private:
  void Init(std::string_view str,
            const std::vector<std::string> &allowed_block_types);

  struct PEMType;

  std::string_view str_;

  std::string_view::size_type pos_;

  std::string block_type_;

  std::vector<PEMType> block_types_;

  std::string data_;
};

OPENSSL_EXPORT std::string PEMEncode(std::string_view data,
                                     const std::string &type);

BSSL_NAMESPACE_END

#endif
