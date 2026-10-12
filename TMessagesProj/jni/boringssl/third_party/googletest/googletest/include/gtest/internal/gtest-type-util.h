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

// Type utilities needed for implementing typed and type-parameterized
// tests.

// IWYU pragma: private, include "gtest/gtest.h"
// IWYU pragma: friend gtest/.*
// IWYU pragma: friend gmock/.*

#ifndef GOOGLETEST_INCLUDE_GTEST_INTERNAL_GTEST_TYPE_UTIL_H_
#define GOOGLETEST_INCLUDE_GTEST_INTERNAL_GTEST_TYPE_UTIL_H_

#include <string>
#include <type_traits>
#include <typeinfo>

#include "gtest/internal/gtest-port.h"

#if GTEST_HAS_CXXABI_H_
#include <cxxabi.h>
#elif defined(__HP_aCC)
#include <acxx_demangle.h>
#endif

namespace testing {
namespace internal {

inline std::string CanonicalizeForStdLibVersioning(std::string s) {
  static const char prefix[] = "std::__";
  if (s.compare(0, strlen(prefix), prefix) == 0) {
    std::string::size_type end = s.find("::", strlen(prefix));
    if (end != s.npos) {

      s.erase(strlen("std"), end - strlen("std"));
    }
  }

  static const char to_search[] = ", ";
  const char replace_char = ',';
  size_t pos = 0;
  while (true) {

    pos = s.find(to_search, pos);
    if (pos == std::string::npos) {
      break;
    }

    s.replace(pos, strlen(to_search), 1, replace_char);
    ++pos;
  }
  return s;
}

#if GTEST_HAS_RTTI

inline std::string GetTypeName(const std::type_info& type) {
  const char* const name = type.name();
#if GTEST_HAS_CXXABI_H_ || defined(__HP_aCC)
  int status = 0;

#if GTEST_HAS_CXXABI_H_
  using abi::__cxa_demangle;
#endif
  char* const readable_name = __cxa_demangle(name, nullptr, nullptr, &status);
  const std::string name_str(status == 0 ? readable_name : name);
  free(readable_name);
  return CanonicalizeForStdLibVersioning(name_str);
#elif defined(_MSC_VER)

  std::string s = name;

  if (s.rfind("struct ", 0) == 0) {
    s = s.substr(strlen("struct "));
  } else if (s.rfind("class ", 0) == 0) {
    s = s.substr(strlen("class "));
  }
  return s;
#else
  return name;
#endif
}
#endif

template <typename T>
std::string GetTypeName() {
#if GTEST_HAS_RTTI
  return GetTypeName(typeid(T));
#else
  return "<type>";
#endif
}

struct None {};

#define GTEST_TEMPLATE_ \
  template <typename T> \
  class

template <GTEST_TEMPLATE_ Tmpl>
struct TemplateSel {
  template <typename T>
  struct Bind {
    typedef Tmpl<T> type;
  };
};

#define GTEST_BIND_(TmplSel, T) TmplSel::template Bind<T>::type

template <GTEST_TEMPLATE_ Head_, GTEST_TEMPLATE_... Tail_>
struct Templates {
  using Head = TemplateSel<Head_>;
  using Tail = Templates<Tail_...>;
};

template <GTEST_TEMPLATE_ Head_>
struct Templates<Head_> {
  using Head = TemplateSel<Head_>;
  using Tail = None;
};

template <typename Head_, typename... Tail_>
struct Types {
  using Head = Head_;
  using Tail = Types<Tail_...>;
};

template <typename Head_>
struct Types<Head_> {
  using Head = Head_;
  using Tail = None;
};

template <typename... Ts>
struct ProxyTypeList {
  using type = Types<Ts...>;
};

template <typename>
struct is_proxy_type_list : std::false_type {};

template <typename... Ts>
struct is_proxy_type_list<ProxyTypeList<Ts...>> : std::true_type {};

template <typename T>
struct GenerateTypeList {
 private:
  using proxy = typename std::conditional<is_proxy_type_list<T>::value, T,
                                          ProxyTypeList<T>>::type;

 public:
  using type = typename proxy::type;
};

}

template <typename... Ts>
using Types = internal::ProxyTypeList<Ts...>;

}

#endif
