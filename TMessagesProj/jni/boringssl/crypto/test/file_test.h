// Copyright 2015 The BoringSSL Authors
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

#ifndef OPENSSL_HEADER_CRYPTO_TEST_FILE_TEST_H
#define OPENSSL_HEADER_CRYPTO_TEST_FILE_TEST_H

#include <openssl/base.h>

#include <stdint.h>

#include <functional>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

class FileTest;
typedef bool (*FileTestFunc)(FileTest *t, void *arg);

class FileTest {
 public:
  enum ReadResult {
    kReadSuccess,
    kReadEOF,
    kReadError,
  };

  class LineReader {
   public:
    virtual ~LineReader() {}
    virtual ReadResult ReadLine(char *out, size_t len) = 0;
  };

  struct Options {

    const char *path = nullptr;

    FileTestFunc callback = nullptr;

    void *arg = nullptr;

    bool silent = false;

    std::function<void(const std::string&)> comment_callback;

    bool is_kas_test = false;
  };

  explicit FileTest(std::unique_ptr<LineReader> reader,
                    std::function<void(const std::string &)> comment_callback,
                    bool is_kas_test);
  ~FileTest();

  ReadResult ReadNext();

  void PrintLine(const char *format, ...) OPENSSL_PRINTF_FORMAT_FUNC(2, 3);

  unsigned start_line() const { return start_line_; }

  const std::string &GetType();

  const std::string &GetParameter();

  bool HasAttribute(const std::string &key);

  bool GetAttribute(std::string *out_value, const std::string &key);

  const std::string &GetAttributeOrDie(const std::string &key);

  void IgnoreAttribute(const std::string &key) { HasAttribute(key); }

  bool GetBytes(std::vector<uint8_t> *out, const std::string &key);

  bool IsAtNewInstructionBlock() const;

  bool HasInstruction(const std::string &key);

  void IgnoreInstruction(const std::string &key) { HasInstruction(key); }

  void IgnoreAllUnusedInstructions();

  bool GetInstruction(std::string *out_value, const std::string &key);

  const std::string &GetInstructionOrDie(const std::string &key);

  bool GetInstructionBytes(std::vector<uint8_t> *out, const std::string &key);

  const std::string &CurrentTestToString() const;

  void InjectInstruction(const std::string &key, const std::string &value);

  void SkipCurrent();

 private:
  void ClearTest();
  void ClearInstructions();
  void OnKeyUsed(const std::string &key);
  void OnInstructionUsed(const std::string &key);
  bool ConvertToBytes(std::vector<uint8_t> *out, const std::string &value);

  std::unique_ptr<LineReader> reader_;

  unsigned line_ = 0;

  unsigned start_line_ = 0;

  std::string type_;

  std::string parameter_;

  std::map<std::string, size_t> attribute_count_;

  std::map<std::string, std::string> attributes_;

  std::map<std::string, std::string> instructions_;

  std::set<std::string> unused_attributes_;

  std::set<std::string> unused_instructions_;

  std::string current_test_;

  bool is_at_new_instruction_block_ = false;
  bool seen_non_comment_ = false;
  bool is_kas_test_ = false;

  std::function<void(const std::string&)> comment_callback_;

  FileTest(const FileTest &) = delete;
  FileTest &operator=(const FileTest &) = delete;
};

int FileTestMain(FileTestFunc run_test, void *arg, const char *path);

int FileTestMain(const FileTest::Options &opts);

void FileTestGTest(const char *path, std::function<void(FileTest *)> run_test);

#endif
