/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_HTTP_COMMON_H_
#define RTC_BASE_HTTP_COMMON_H_

#include <string>

#include "absl/strings/string_view.h"

namespace rtc {

class CryptString;
class SocketAddress;

struct HttpAuthContext {
  std::string auth_method;
  HttpAuthContext(absl::string_view auth) : auth_method(auth) {}
  virtual ~HttpAuthContext() {}
};

enum HttpAuthResult { HAR_RESPONSE, HAR_IGNORE, HAR_CREDENTIALS, HAR_ERROR };

HttpAuthResult HttpAuthenticate(absl::string_view challenge,
                                const SocketAddress& server,
                                absl::string_view method,
                                absl::string_view uri,
                                absl::string_view username,
                                const CryptString& password,
                                HttpAuthContext*& context,
                                std::string& response,
                                std::string& auth_method);

}

#endif
