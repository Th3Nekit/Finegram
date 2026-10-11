// Copyright 2021 The BoringSSL Authors
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

#include <openssl/base.h>

#include <functional>
#include <memory>
#include <vector>

#include <openssl/span.h>

namespace bssl {
namespace acvp {

constexpr size_t kMaxArgs = 9;

constexpr size_t kMaxNameLength = 30;

class RequestBuffer {
 public:
  virtual ~RequestBuffer();

  static std::unique_ptr<RequestBuffer> New();
};

Span<const Span<const uint8_t>> ParseArgsFromFd(int fd, RequestBuffer *buffer);

bool WriteReplyToFd(int fd, const std::vector<Span<const uint8_t>> &spans);

bool WriteReplyToBuffer(const std::vector<Span<const uint8_t>> &spans);

bool FlushBuffer(int fd);

typedef std::function<bool(const std::vector<Span<const uint8_t>> &)>
    ReplyCallback;

typedef bool (*Handler)(const Span<const uint8_t> args[],
                        ReplyCallback write_reply);

Handler FindHandler(Span<const Span<const uint8_t>> args);

}
}
