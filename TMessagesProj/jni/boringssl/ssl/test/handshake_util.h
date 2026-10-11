// Copyright 2018 The BoringSSL Authors
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

#ifndef HEADER_TEST_HANDSHAKE
#define HEADER_TEST_HANDSHAKE

#include <functional>

#include <openssl/base.h>

#include "settings_writer.h"

#if defined(OPENSSL_LINUX) && !defined(OPENSSL_ANDROID)
#define HANDSHAKER_SUPPORTED
#endif

bool RetryAsync(SSL *ssl, int ret);

int CheckIdempotentError(const char *name, SSL *ssl, std::function<int()> func);

#if defined(HANDSHAKER_SUPPORTED)

bool DoSplitHandshake(bssl::UniquePtr<SSL> *ssl, SettingsWriter *writer,
                      bool is_resume);

bool GetHandshakeHint(SSL *ssl, SettingsWriter *writer, bool is_resume,
                      const SSL_CLIENT_HELLO *client_hello);

constexpr char kControlMsgWantRead = 'R';
constexpr char kControlMsgWriteCompleted = 'W';
constexpr char kControlMsgDone = 'H';
constexpr char kControlMsgError = 'E';

constexpr int kFdControl = 3;
constexpr int kFdProxyToHandshaker = 4;
constexpr int kFdHandshakerToProxy = 5;
#endif

#endif
