/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_SYSTEM_RTC_EXPORT_TEMPLATE_H_
#define RTC_BASE_SYSTEM_RTC_EXPORT_TEMPLATE_H_

#define RTC_EXPORT_TEMPLATE_DECLARE(export)         \
  RTC_EXPORT_TEMPLATE_INVOKE(                       \
      DECLARE,                                      \
      RTC_EXPORT_TEMPLATE_STYLE(export, ), export)  // NOLINT
#define RTC_EXPORT_TEMPLATE_DEFINE(export)          \
  RTC_EXPORT_TEMPLATE_INVOKE(                       \
      DEFINE,                                       \
      RTC_EXPORT_TEMPLATE_STYLE(export, ), export)  // NOLINT

#define RTC_EXPORT_TEMPLATE_INVOKE(which, style, export) \
  RTC_EXPORT_TEMPLATE_INVOKE_2(which, style, export)
#define RTC_EXPORT_TEMPLATE_INVOKE_2(which, style, export) \
  RTC_EXPORT_TEMPLATE_##which##_##style(export, )

#define RTC_EXPORT_TEMPLATE_DECLARE_DEFAULT(export, _) export
#define RTC_EXPORT_TEMPLATE_DEFINE_DEFAULT(export, _)

#define RTC_EXPORT_TEMPLATE_DECLARE_MSVC_HACK(export, _)
#define RTC_EXPORT_TEMPLATE_DEFINE_MSVC_HACK(export, _) export

#define RTC_EXPORT_TEMPLATE_STYLE(export, _) \
  RTC_EXPORT_TEMPLATE_STYLE_2(export, )
#define RTC_EXPORT_TEMPLATE_STYLE_2(export, _) \
  RTC_EXPORT_TEMPLATE_STYLE_3(                 \
      RTC_EXPORT_TEMPLATE_STYLE_MATCH_foj3FJo5StF0OvIzl7oMxA##export)
#define RTC_EXPORT_TEMPLATE_STYLE_3(style) style

#define RTC_EXPORT_TEMPLATE_STYLE_MATCH_foj3FJo5StF0OvIzl7oMxA DEFAULT
#define RTC_EXPORT_TEMPLATE_STYLE_MATCH_foj3FJo5StF0OvIzl7oMxA__attribute__( \
    ...)                                                                     \
  DEFAULT
#define RTC_EXPORT_TEMPLATE_STYLE_MATCH_foj3FJo5StF0OvIzl7oMxA__declspec(arg) \
  RTC_EXPORT_TEMPLATE_STYLE_MATCH_DECLSPEC_##arg

#define RTC_EXPORT_TEMPLATE_STYLE_MATCH_DECLSPEC_dllexport MSVC_HACK
#define RTC_EXPORT_TEMPLATE_STYLE_MATCH_DECLSPEC_dllimport DEFAULT

#define RTC_EXPORT_TEMPLATE_TEST(want, export)                    \
  static_assert(                                                  \
      RTC_EXPORT_TEMPLATE_INVOKE(                                 \
          TEST_##want,                                            \
          RTC_EXPORT_TEMPLATE_STYLE(export, ), export), #export)  // NOLINT
#define RTC_EXPORT_TEMPLATE_TEST_DEFAULT_DEFAULT(...) true
#define RTC_EXPORT_TEMPLATE_TEST_MSVC_HACK_MSVC_HACK(...) true

RTC_EXPORT_TEMPLATE_TEST(DEFAULT, );
RTC_EXPORT_TEMPLATE_TEST(DEFAULT, __attribute__((visibility("default"))));
RTC_EXPORT_TEMPLATE_TEST(MSVC_HACK, __declspec(dllexport));
RTC_EXPORT_TEMPLATE_TEST(DEFAULT, __declspec(dllimport));

#undef RTC_EXPORT_TEMPLATE_TEST
#undef RTC_EXPORT_TEMPLATE_TEST_DEFAULT_DEFAULT
#undef RTC_EXPORT_TEMPLATE_TEST_MSVC_HACK_MSVC_HACK

#endif
