// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file under third_party_mods/chromium or at:
// http://src.chromium.org/svn/trunk/src/LICENSE

#ifndef RTC_BASE_TRACE_EVENT_H_
#define RTC_BASE_TRACE_EVENT_H_

#include <string>

#include "rtc_base/event_tracer.h"

#if defined(TRACE_EVENT0)
#error "Another copy of trace_event.h has already been included."
#endif

#if defined(RTC_DISABLE_TRACE_EVENTS)
#define RTC_TRACE_EVENTS_ENABLED 0
#else
#define RTC_TRACE_EVENTS_ENABLED 1
#endif

#define TRACE_VALUE_TYPE_BOOL (static_cast<unsigned char>(1))
#define TRACE_VALUE_TYPE_UINT (static_cast<unsigned char>(2))
#define TRACE_VALUE_TYPE_INT (static_cast<unsigned char>(3))
#define TRACE_VALUE_TYPE_DOUBLE (static_cast<unsigned char>(4))
#define TRACE_VALUE_TYPE_POINTER (static_cast<unsigned char>(5))
#define TRACE_VALUE_TYPE_STRING (static_cast<unsigned char>(6))
#define TRACE_VALUE_TYPE_COPY_STRING (static_cast<unsigned char>(7))

#if RTC_TRACE_EVENTS_ENABLED

#define TRACE_STR_COPY(str) \
  webrtc::trace_event_internal::TraceStringWithCopy(str)

#define TRACE_DISABLED_BY_DEFAULT(name) "disabled-by-default-" name

#define TRACE_ID_MANGLE(id) \
  webrtc::trace_event_internal::TraceID::ForceMangle(id)

#define TRACE_EVENT0(category, name) \
  INTERNAL_TRACE_EVENT_ADD_SCOPED(category, name)
#define TRACE_EVENT1(category, name, arg1_name, arg1_val) \
  INTERNAL_TRACE_EVENT_ADD_SCOPED(category, name, arg1_name, arg1_val)
#define TRACE_EVENT2(category, name, arg1_name, arg1_val, arg2_name, arg2_val) \
  INTERNAL_TRACE_EVENT_ADD_SCOPED(category, name, arg1_name, arg1_val,         \
                                  arg2_name, arg2_val)

#define TRACE_EVENT_INSTANT0(category, name)                          \
  INTERNAL_TRACE_EVENT_ADD(TRACE_EVENT_PHASE_INSTANT, category, name, \
                           TRACE_EVENT_FLAG_NONE)
#define TRACE_EVENT_INSTANT1(category, name, arg1_name, arg1_val)     \
  INTERNAL_TRACE_EVENT_ADD(TRACE_EVENT_PHASE_INSTANT, category, name, \
                           TRACE_EVENT_FLAG_NONE, arg1_name, arg1_val)
#define TRACE_EVENT_INSTANT2(category, name, arg1_name, arg1_val, arg2_name, \
                             arg2_val)                                       \
  INTERNAL_TRACE_EVENT_ADD(TRACE_EVENT_PHASE_INSTANT, category, name,        \
                           TRACE_EVENT_FLAG_NONE, arg1_name, arg1_val,       \
                           arg2_name, arg2_val)
#define TRACE_EVENT_COPY_INSTANT0(category, name)                     \
  INTERNAL_TRACE_EVENT_ADD(TRACE_EVENT_PHASE_INSTANT, category, name, \
                           TRACE_EVENT_FLAG_COPY)
#define TRACE_EVENT_COPY_INSTANT1(category, name, arg1_name, arg1_val) \
  INTERNAL_TRACE_EVENT_ADD(TRACE_EVENT_PHASE_INSTANT, category, name,  \
                           TRACE_EVENT_FLAG_COPY, arg1_name, arg1_val)
#define TRACE_EVENT_COPY_INSTANT2(category, name, arg1_name, arg1_val, \
                                  arg2_name, arg2_val)                 \
  INTERNAL_TRACE_EVENT_ADD(TRACE_EVENT_PHASE_INSTANT, category, name,  \
                           TRACE_EVENT_FLAG_COPY, arg1_name, arg1_val, \
                           arg2_name, arg2_val)

#define TRACE_EVENT_BEGIN0(category, name)                          \
  INTERNAL_TRACE_EVENT_ADD(TRACE_EVENT_PHASE_BEGIN, category, name, \
                           TRACE_EVENT_FLAG_NONE)
#define TRACE_EVENT_BEGIN1(category, name, arg1_name, arg1_val)     \
  INTERNAL_TRACE_EVENT_ADD(TRACE_EVENT_PHASE_BEGIN, category, name, \
                           TRACE_EVENT_FLAG_NONE, arg1_name, arg1_val)
#define TRACE_EVENT_BEGIN2(category, name, arg1_name, arg1_val, arg2_name, \
                           arg2_val)                                       \
  INTERNAL_TRACE_EVENT_ADD(TRACE_EVENT_PHASE_BEGIN, category, name,        \
                           TRACE_EVENT_FLAG_NONE, arg1_name, arg1_val,     \
                           arg2_name, arg2_val)
#define TRACE_EVENT_COPY_BEGIN0(category, name)                     \
  INTERNAL_TRACE_EVENT_ADD(TRACE_EVENT_PHASE_BEGIN, category, name, \
                           TRACE_EVENT_FLAG_COPY)
#define TRACE_EVENT_COPY_BEGIN1(category, name, arg1_name, arg1_val) \
  INTERNAL_TRACE_EVENT_ADD(TRACE_EVENT_PHASE_BEGIN, category, name,  \
                           TRACE_EVENT_FLAG_COPY, arg1_name, arg1_val)
#define TRACE_EVENT_COPY_BEGIN2(category, name, arg1_name, arg1_val,   \
                                arg2_name, arg2_val)                   \
  INTERNAL_TRACE_EVENT_ADD(TRACE_EVENT_PHASE_BEGIN, category, name,    \
                           TRACE_EVENT_FLAG_COPY, arg1_name, arg1_val, \
                           arg2_name, arg2_val)

#define TRACE_EVENT_END0(category, name)                          \
  INTERNAL_TRACE_EVENT_ADD(TRACE_EVENT_PHASE_END, category, name, \
                           TRACE_EVENT_FLAG_NONE)
#define TRACE_EVENT_END1(category, name, arg1_name, arg1_val)     \
  INTERNAL_TRACE_EVENT_ADD(TRACE_EVENT_PHASE_END, category, name, \
                           TRACE_EVENT_FLAG_NONE, arg1_name, arg1_val)
#define TRACE_EVENT_END2(category, name, arg1_name, arg1_val, arg2_name, \
                         arg2_val)                                       \
  INTERNAL_TRACE_EVENT_ADD(TRACE_EVENT_PHASE_END, category, name,        \
                           TRACE_EVENT_FLAG_NONE, arg1_name, arg1_val,   \
                           arg2_name, arg2_val)
#define TRACE_EVENT_COPY_END0(category, name)                     \
  INTERNAL_TRACE_EVENT_ADD(TRACE_EVENT_PHASE_END, category, name, \
                           TRACE_EVENT_FLAG_COPY)
#define TRACE_EVENT_COPY_END1(category, name, arg1_name, arg1_val) \
  INTERNAL_TRACE_EVENT_ADD(TRACE_EVENT_PHASE_END, category, name,  \
                           TRACE_EVENT_FLAG_COPY, arg1_name, arg1_val)
#define TRACE_EVENT_COPY_END2(category, name, arg1_name, arg1_val, arg2_name, \
                              arg2_val)                                       \
  INTERNAL_TRACE_EVENT_ADD(TRACE_EVENT_PHASE_END, category, name,             \
                           TRACE_EVENT_FLAG_COPY, arg1_name, arg1_val,        \
                           arg2_name, arg2_val)

#define TRACE_COUNTER1(category, name, value)                         \
  INTERNAL_TRACE_EVENT_ADD(TRACE_EVENT_PHASE_COUNTER, category, name, \
                           TRACE_EVENT_FLAG_NONE, "value",            \
                           static_cast<int>(value))
#define TRACE_COPY_COUNTER1(category, name, value)                    \
  INTERNAL_TRACE_EVENT_ADD(TRACE_EVENT_PHASE_COUNTER, category, name, \
                           TRACE_EVENT_FLAG_COPY, "value",            \
                           static_cast<int>(value))

#define TRACE_COUNTER2(category, name, value1_name, value1_val, value2_name, \
                       value2_val)                                           \
  INTERNAL_TRACE_EVENT_ADD(TRACE_EVENT_PHASE_COUNTER, category, name,        \
                           TRACE_EVENT_FLAG_NONE, value1_name,               \
                           static_cast<int>(value1_val), value2_name,        \
                           static_cast<int>(value2_val))
#define TRACE_COPY_COUNTER2(category, name, value1_name, value1_val,  \
                            value2_name, value2_val)                  \
  INTERNAL_TRACE_EVENT_ADD(TRACE_EVENT_PHASE_COUNTER, category, name, \
                           TRACE_EVENT_FLAG_COPY, value1_name,        \
                           static_cast<int>(value1_val), value2_name, \
                           static_cast<int>(value2_val))

#define TRACE_COUNTER_ID1(category, name, id, value)                          \
  INTERNAL_TRACE_EVENT_ADD_WITH_ID(TRACE_EVENT_PHASE_COUNTER, category, name, \
                                   id, TRACE_EVENT_FLAG_NONE, "value",        \
                                   static_cast<int>(value))
#define TRACE_COPY_COUNTER_ID1(category, name, id, value)                     \
  INTERNAL_TRACE_EVENT_ADD_WITH_ID(TRACE_EVENT_PHASE_COUNTER, category, name, \
                                   id, TRACE_EVENT_FLAG_COPY, "value",        \
                                   static_cast<int>(value))

#define TRACE_COUNTER_ID2(category, name, id, value1_name, value1_val,        \
                          value2_name, value2_val)                            \
  INTERNAL_TRACE_EVENT_ADD_WITH_ID(TRACE_EVENT_PHASE_COUNTER, category, name, \
                                   id, TRACE_EVENT_FLAG_NONE, value1_name,    \
                                   static_cast<int>(value1_val), value2_name, \
                                   static_cast<int>(value2_val))
#define TRACE_COPY_COUNTER_ID2(category, name, id, value1_name, value1_val,   \
                               value2_name, value2_val)                       \
  INTERNAL_TRACE_EVENT_ADD_WITH_ID(TRACE_EVENT_PHASE_COUNTER, category, name, \
                                   id, TRACE_EVENT_FLAG_COPY, value1_name,    \
                                   static_cast<int>(value1_val), value2_name, \
                                   static_cast<int>(value2_val))

#define TRACE_EVENT_ASYNC_BEGIN0(category, name, id)                        \
  INTERNAL_TRACE_EVENT_ADD_WITH_ID(TRACE_EVENT_PHASE_ASYNC_BEGIN, category, \
                                   name, id, TRACE_EVENT_FLAG_NONE)
#define TRACE_EVENT_ASYNC_BEGIN1(category, name, id, arg1_name, arg1_val)      \
  INTERNAL_TRACE_EVENT_ADD_WITH_ID(TRACE_EVENT_PHASE_ASYNC_BEGIN, category,    \
                                   name, id, TRACE_EVENT_FLAG_NONE, arg1_name, \
                                   arg1_val)
#define TRACE_EVENT_ASYNC_BEGIN2(category, name, id, arg1_name, arg1_val,      \
                                 arg2_name, arg2_val)                          \
  INTERNAL_TRACE_EVENT_ADD_WITH_ID(TRACE_EVENT_PHASE_ASYNC_BEGIN, category,    \
                                   name, id, TRACE_EVENT_FLAG_NONE, arg1_name, \
                                   arg1_val, arg2_name, arg2_val)
#define TRACE_EVENT_COPY_ASYNC_BEGIN0(category, name, id)                   \
  INTERNAL_TRACE_EVENT_ADD_WITH_ID(TRACE_EVENT_PHASE_ASYNC_BEGIN, category, \
                                   name, id, TRACE_EVENT_FLAG_COPY)
#define TRACE_EVENT_COPY_ASYNC_BEGIN1(category, name, id, arg1_name, arg1_val) \
  INTERNAL_TRACE_EVENT_ADD_WITH_ID(TRACE_EVENT_PHASE_ASYNC_BEGIN, category,    \
                                   name, id, TRACE_EVENT_FLAG_COPY, arg1_name, \
                                   arg1_val)
#define TRACE_EVENT_COPY_ASYNC_BEGIN2(category, name, id, arg1_name, arg1_val, \
                                      arg2_name, arg2_val)                     \
  INTERNAL_TRACE_EVENT_ADD_WITH_ID(TRACE_EVENT_PHASE_ASYNC_BEGIN, category,    \
                                   name, id, TRACE_EVENT_FLAG_COPY, arg1_name, \
                                   arg1_val, arg2_name, arg2_val)

#define TRACE_EVENT_ASYNC_STEP0(category, name, id, step)                   \
  INTERNAL_TRACE_EVENT_ADD_WITH_ID(TRACE_EVENT_PHASE_ASYNC_STEP, category,  \
                                   name, id, TRACE_EVENT_FLAG_NONE, "step", \
                                   step)
#define TRACE_EVENT_ASYNC_STEP1(category, name, id, step, arg1_name, arg1_val) \
  INTERNAL_TRACE_EVENT_ADD_WITH_ID(TRACE_EVENT_PHASE_ASYNC_STEP, category,     \
                                   name, id, TRACE_EVENT_FLAG_NONE, "step",    \
                                   step, arg1_name, arg1_val)
#define TRACE_EVENT_COPY_ASYNC_STEP0(category, name, id, step)              \
  INTERNAL_TRACE_EVENT_ADD_WITH_ID(TRACE_EVENT_PHASE_ASYNC_STEP, category,  \
                                   name, id, TRACE_EVENT_FLAG_COPY, "step", \
                                   step)
#define TRACE_EVENT_COPY_ASYNC_STEP1(category, name, id, step, arg1_name,   \
                                     arg1_val)                              \
  INTERNAL_TRACE_EVENT_ADD_WITH_ID(TRACE_EVENT_PHASE_ASYNC_STEP, category,  \
                                   name, id, TRACE_EVENT_FLAG_COPY, "step", \
                                   step, arg1_name, arg1_val)

#define TRACE_EVENT_ASYNC_END0(category, name, id)                        \
  INTERNAL_TRACE_EVENT_ADD_WITH_ID(TRACE_EVENT_PHASE_ASYNC_END, category, \
                                   name, id, TRACE_EVENT_FLAG_NONE)
#define TRACE_EVENT_ASYNC_END1(category, name, id, arg1_name, arg1_val)        \
  INTERNAL_TRACE_EVENT_ADD_WITH_ID(TRACE_EVENT_PHASE_ASYNC_END, category,      \
                                   name, id, TRACE_EVENT_FLAG_NONE, arg1_name, \
                                   arg1_val)
#define TRACE_EVENT_ASYNC_END2(category, name, id, arg1_name, arg1_val,        \
                               arg2_name, arg2_val)                            \
  INTERNAL_TRACE_EVENT_ADD_WITH_ID(TRACE_EVENT_PHASE_ASYNC_END, category,      \
                                   name, id, TRACE_EVENT_FLAG_NONE, arg1_name, \
                                   arg1_val, arg2_name, arg2_val)
#define TRACE_EVENT_COPY_ASYNC_END0(category, name, id)                   \
  INTERNAL_TRACE_EVENT_ADD_WITH_ID(TRACE_EVENT_PHASE_ASYNC_END, category, \
                                   name, id, TRACE_EVENT_FLAG_COPY)
#define TRACE_EVENT_COPY_ASYNC_END1(category, name, id, arg1_name, arg1_val)   \
  INTERNAL_TRACE_EVENT_ADD_WITH_ID(TRACE_EVENT_PHASE_ASYNC_END, category,      \
                                   name, id, TRACE_EVENT_FLAG_COPY, arg1_name, \
                                   arg1_val)
#define TRACE_EVENT_COPY_ASYNC_END2(category, name, id, arg1_name, arg1_val,   \
                                    arg2_name, arg2_val)                       \
  INTERNAL_TRACE_EVENT_ADD_WITH_ID(TRACE_EVENT_PHASE_ASYNC_END, category,      \
                                   name, id, TRACE_EVENT_FLAG_COPY, arg1_name, \
                                   arg1_val, arg2_name, arg2_val)

#define TRACE_EVENT_API_GET_CATEGORY_ENABLED \
  webrtc::EventTracer::GetCategoryEnabled

#define TRACE_EVENT_API_ADD_TRACE_EVENT webrtc::EventTracer::AddTraceEvent

#define INTERNAL_TRACE_EVENT_UID3(a, b) trace_event_unique_##a##b
#define INTERNAL_TRACE_EVENT_UID2(a, b) INTERNAL_TRACE_EVENT_UID3(a, b)
#define INTERNAL_TRACE_EVENT_UID(name_prefix) \
  INTERNAL_TRACE_EVENT_UID2(name_prefix, __LINE__)

#if WEBRTC_NON_STATIC_TRACE_EVENT_HANDLERS
#define INTERNAL_TRACE_EVENT_INFO_TYPE const unsigned char*
#else
#define INTERNAL_TRACE_EVENT_INFO_TYPE static const unsigned char*
#endif

#define INTERNAL_TRACE_EVENT_GET_CATEGORY_INFO(category)               \
  INTERNAL_TRACE_EVENT_INFO_TYPE INTERNAL_TRACE_EVENT_UID(catstatic) = \
      TRACE_EVENT_API_GET_CATEGORY_ENABLED(category);

#define INTERNAL_TRACE_EVENT_ADD(phase, category, name, flags, ...)        \
  do {                                                                     \
    INTERNAL_TRACE_EVENT_GET_CATEGORY_INFO(category);                      \
    if (*INTERNAL_TRACE_EVENT_UID(catstatic)) {                            \
      webrtc::trace_event_internal::AddTraceEvent(                         \
          phase, INTERNAL_TRACE_EVENT_UID(catstatic), name,                \
          webrtc::trace_event_internal::kNoEventId, flags, ##__VA_ARGS__); \
    }                                                                      \
  } while (0)

#define INTERNAL_TRACE_EVENT_ADD_SCOPED(category, name, ...)                   \
  INTERNAL_TRACE_EVENT_GET_CATEGORY_INFO(category);                            \
  webrtc::trace_event_internal::TraceEndOnScopeClose INTERNAL_TRACE_EVENT_UID( \
      profileScope);                                                           \
  if (*INTERNAL_TRACE_EVENT_UID(catstatic)) {                                  \
    webrtc::trace_event_internal::AddTraceEvent(                               \
        TRACE_EVENT_PHASE_BEGIN, INTERNAL_TRACE_EVENT_UID(catstatic), name,    \
        webrtc::trace_event_internal::kNoEventId, TRACE_EVENT_FLAG_NONE,       \
        ##__VA_ARGS__);                                                        \
    INTERNAL_TRACE_EVENT_UID(profileScope)                                     \
        .Initialize(INTERNAL_TRACE_EVENT_UID(catstatic), name);                \
  }

#define INTERNAL_TRACE_EVENT_ADD_WITH_ID(phase, category, name, id, flags, \
                                         ...)                              \
  do {                                                                     \
    INTERNAL_TRACE_EVENT_GET_CATEGORY_INFO(category);                      \
    if (*INTERNAL_TRACE_EVENT_UID(catstatic)) {                            \
      unsigned char trace_event_flags = flags | TRACE_EVENT_FLAG_HAS_ID;   \
      webrtc::trace_event_internal::TraceID trace_event_trace_id(          \
          id, &trace_event_flags);                                         \
      webrtc::trace_event_internal::AddTraceEvent(                         \
          phase, INTERNAL_TRACE_EVENT_UID(catstatic), name,                \
          trace_event_trace_id.data(), trace_event_flags, ##__VA_ARGS__);  \
    }                                                                      \
  } while (0)

#define TRACE_EVENT_PHASE_BEGIN ('B')
#define TRACE_EVENT_PHASE_END ('E')
#define TRACE_EVENT_PHASE_INSTANT ('I')
#define TRACE_EVENT_PHASE_ASYNC_BEGIN ('S')
#define TRACE_EVENT_PHASE_ASYNC_STEP ('T')
#define TRACE_EVENT_PHASE_ASYNC_END ('F')
#define TRACE_EVENT_PHASE_METADATA ('M')
#define TRACE_EVENT_PHASE_COUNTER ('C')

#define TRACE_EVENT_FLAG_NONE (static_cast<unsigned char>(0))
#define TRACE_EVENT_FLAG_COPY (static_cast<unsigned char>(1 << 0))
#define TRACE_EVENT_FLAG_HAS_ID (static_cast<unsigned char>(1 << 1))
#define TRACE_EVENT_FLAG_MANGLE_ID (static_cast<unsigned char>(1 << 2))

namespace webrtc {
namespace trace_event_internal {

const int kZeroNumArgs = 0;
const unsigned long long kNoEventId = 0;

class TraceID {
 public:
  class ForceMangle {
   public:
    explicit ForceMangle(unsigned long long id) : data_(id) {}
    explicit ForceMangle(unsigned long id) : data_(id) {}
    explicit ForceMangle(unsigned int id) : data_(id) {}
    explicit ForceMangle(unsigned short id) : data_(id) {}
    explicit ForceMangle(unsigned char id) : data_(id) {}
    explicit ForceMangle(long long id)
        : data_(static_cast<unsigned long long>(id)) {}
    explicit ForceMangle(long id)
        : data_(static_cast<unsigned long long>(id)) {}
    explicit ForceMangle(int id) : data_(static_cast<unsigned long long>(id)) {}
    explicit ForceMangle(short id)
        : data_(static_cast<unsigned long long>(id)) {}
    explicit ForceMangle(signed char id)
        : data_(static_cast<unsigned long long>(id)) {}

    unsigned long long data() const { return data_; }

   private:
    unsigned long long data_;
  };

  explicit TraceID(const void* id, unsigned char* flags)
      : data_(
            static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(id))) {
    *flags |= TRACE_EVENT_FLAG_MANGLE_ID;
  }
  explicit TraceID(ForceMangle id, unsigned char* flags) : data_(id.data()) {
    *flags |= TRACE_EVENT_FLAG_MANGLE_ID;
  }
  explicit TraceID(unsigned long long id, unsigned char* flags) : data_(id) {
    (void)flags;
  }
  explicit TraceID(unsigned long id, unsigned char* flags) : data_(id) {
    (void)flags;
  }
  explicit TraceID(unsigned int id, unsigned char* flags) : data_(id) {
    (void)flags;
  }
  explicit TraceID(unsigned short id, unsigned char* flags) : data_(id) {
    (void)flags;
  }
  explicit TraceID(unsigned char id, unsigned char* flags) : data_(id) {
    (void)flags;
  }
  explicit TraceID(long long id, unsigned char* flags)
      : data_(static_cast<unsigned long long>(id)) {
    (void)flags;
  }
  explicit TraceID(long id, unsigned char* flags)
      : data_(static_cast<unsigned long long>(id)) {
    (void)flags;
  }
  explicit TraceID(int id, unsigned char* flags)
      : data_(static_cast<unsigned long long>(id)) {
    (void)flags;
  }
  explicit TraceID(short id, unsigned char* flags)
      : data_(static_cast<unsigned long long>(id)) {
    (void)flags;
  }
  explicit TraceID(signed char id, unsigned char* flags)
      : data_(static_cast<unsigned long long>(id)) {
    (void)flags;
  }

  unsigned long long data() const { return data_; }

 private:
  unsigned long long data_;
};

union TraceValueUnion {
  bool as_bool;
  unsigned long long as_uint;
  long long as_int;
  double as_double;
  const void* as_pointer;
  const char* as_string;
};

class TraceStringWithCopy {
 public:
  explicit TraceStringWithCopy(const char* str) : str_(str) {}
  operator const char*() const { return str_; }

 private:
  const char* str_;
};

#define INTERNAL_DECLARE_SET_TRACE_VALUE(actual_type, union_member,      \
                                         value_type_id)                  \
  static inline void SetTraceValue(actual_type arg, unsigned char* type, \
                                   unsigned long long* value) {          \
    TraceValueUnion type_value;                                          \
    type_value.union_member = arg;                                       \
    *type = value_type_id;                                               \
    *value = type_value.as_uint;                                         \
  }

#define INTERNAL_DECLARE_SET_TRACE_VALUE_INT(actual_type, value_type_id) \
  static inline void SetTraceValue(actual_type arg, unsigned char* type, \
                                   unsigned long long* value) {          \
    *type = value_type_id;                                               \
    *value = static_cast<unsigned long long>(arg);                       \
  }

INTERNAL_DECLARE_SET_TRACE_VALUE_INT(unsigned long long, TRACE_VALUE_TYPE_UINT)
INTERNAL_DECLARE_SET_TRACE_VALUE_INT(unsigned long, TRACE_VALUE_TYPE_UINT)
INTERNAL_DECLARE_SET_TRACE_VALUE_INT(unsigned int, TRACE_VALUE_TYPE_UINT)
INTERNAL_DECLARE_SET_TRACE_VALUE_INT(unsigned short, TRACE_VALUE_TYPE_UINT)
INTERNAL_DECLARE_SET_TRACE_VALUE_INT(unsigned char, TRACE_VALUE_TYPE_UINT)
INTERNAL_DECLARE_SET_TRACE_VALUE_INT(long long, TRACE_VALUE_TYPE_INT)
INTERNAL_DECLARE_SET_TRACE_VALUE_INT(long, TRACE_VALUE_TYPE_INT)
INTERNAL_DECLARE_SET_TRACE_VALUE_INT(int, TRACE_VALUE_TYPE_INT)
INTERNAL_DECLARE_SET_TRACE_VALUE_INT(short, TRACE_VALUE_TYPE_INT)
INTERNAL_DECLARE_SET_TRACE_VALUE_INT(signed char, TRACE_VALUE_TYPE_INT)
INTERNAL_DECLARE_SET_TRACE_VALUE(bool, as_bool, TRACE_VALUE_TYPE_BOOL)
INTERNAL_DECLARE_SET_TRACE_VALUE(double, as_double, TRACE_VALUE_TYPE_DOUBLE)
INTERNAL_DECLARE_SET_TRACE_VALUE(const void*,
                                 as_pointer,
                                 TRACE_VALUE_TYPE_POINTER)
INTERNAL_DECLARE_SET_TRACE_VALUE(const char*,
                                 as_string,
                                 TRACE_VALUE_TYPE_STRING)
INTERNAL_DECLARE_SET_TRACE_VALUE(const TraceStringWithCopy&,
                                 as_string,
                                 TRACE_VALUE_TYPE_COPY_STRING)

#undef INTERNAL_DECLARE_SET_TRACE_VALUE
#undef INTERNAL_DECLARE_SET_TRACE_VALUE_INT

static inline void SetTraceValue(const std::string& arg,
                                 unsigned char* type,
                                 unsigned long long* value) {
  TraceValueUnion type_value;
  type_value.as_string = arg.c_str();
  *type = TRACE_VALUE_TYPE_COPY_STRING;
  *value = type_value.as_uint;
}

static inline void AddTraceEvent(char phase,
                                 const unsigned char* category_enabled,
                                 const char* name,
                                 unsigned long long id,
                                 unsigned char flags) {
  TRACE_EVENT_API_ADD_TRACE_EVENT(phase, category_enabled, name, id,
                                  kZeroNumArgs, nullptr, nullptr, nullptr,
                                  flags);
}

template <class ARG1_TYPE>
static inline void AddTraceEvent(char phase,
                                 const unsigned char* category_enabled,
                                 const char* name,
                                 unsigned long long id,
                                 unsigned char flags,
                                 const char* arg1_name,
                                 const ARG1_TYPE& arg1_val) {
  const int num_args = 1;
  unsigned char arg_types[1];
  unsigned long long arg_values[1];
  SetTraceValue(arg1_val, &arg_types[0], &arg_values[0]);
  TRACE_EVENT_API_ADD_TRACE_EVENT(phase, category_enabled, name, id, num_args,
                                  &arg1_name, arg_types, arg_values, flags);
}

template <class ARG1_TYPE, class ARG2_TYPE>
static inline void AddTraceEvent(char phase,
                                 const unsigned char* category_enabled,
                                 const char* name,
                                 unsigned long long id,
                                 unsigned char flags,
                                 const char* arg1_name,
                                 const ARG1_TYPE& arg1_val,
                                 const char* arg2_name,
                                 const ARG2_TYPE& arg2_val) {
  const int num_args = 2;
  const char* arg_names[2] = {arg1_name, arg2_name};
  unsigned char arg_types[2];
  unsigned long long arg_values[2];
  SetTraceValue(arg1_val, &arg_types[0], &arg_values[0]);
  SetTraceValue(arg2_val, &arg_types[1], &arg_values[1]);
  TRACE_EVENT_API_ADD_TRACE_EVENT(phase, category_enabled, name, id, num_args,
                                  arg_names, arg_types, arg_values, flags);
}

class TraceEndOnScopeClose {
 public:

  TraceEndOnScopeClose() : p_data_(nullptr) {}
  ~TraceEndOnScopeClose() {
    if (p_data_)
      AddEventIfEnabled();
  }

  void Initialize(const unsigned char* category_enabled, const char* name) {
    data_.category_enabled = category_enabled;
    data_.name = name;
    p_data_ = &data_;
  }

 private:

  void AddEventIfEnabled() {

    if (*p_data_->category_enabled) {
      TRACE_EVENT_API_ADD_TRACE_EVENT(TRACE_EVENT_PHASE_END,
                                      p_data_->category_enabled, p_data_->name,
                                      kNoEventId, kZeroNumArgs, nullptr,
                                      nullptr, nullptr, TRACE_EVENT_FLAG_NONE);
    }
  }

  struct Data {
    const unsigned char* category_enabled;
    const char* name;
  };
  Data* p_data_;
  Data data_;
};

}
}
#else

#define RTC_NOOP() \
  do {             \
  } while (0)

#define TRACE_STR_COPY(str) RTC_NOOP()

#define TRACE_DISABLED_BY_DEFAULT(name) "disabled-by-default-" name

#define TRACE_ID_MANGLE(id) 0

#define TRACE_EVENT0(category, name) RTC_NOOP()
#define TRACE_EVENT1(category, name, arg1_name, arg1_val) RTC_NOOP()
#define TRACE_EVENT2(category, name, arg1_name, arg1_val, arg2_name, arg2_val) \
  RTC_NOOP()

#define TRACE_EVENT_INSTANT0(category, name) RTC_NOOP()
#define TRACE_EVENT_INSTANT1(category, name, arg1_name, arg1_val) RTC_NOOP()

#define TRACE_EVENT_INSTANT2(category, name, arg1_name, arg1_val, arg2_name, \
                             arg2_val)                                       \
  RTC_NOOP()

#define TRACE_EVENT_COPY_INSTANT0(category, name) RTC_NOOP()
#define TRACE_EVENT_COPY_INSTANT1(category, name, arg1_name, arg1_val) \
  RTC_NOOP()
#define TRACE_EVENT_COPY_INSTANT2(category, name, arg1_name, arg1_val, \
                                  arg2_name, arg2_val)                 \
  RTC_NOOP()

#define TRACE_EVENT_BEGIN0(category, name) RTC_NOOP()
#define TRACE_EVENT_BEGIN1(category, name, arg1_name, arg1_val) RTC_NOOP()
#define TRACE_EVENT_BEGIN2(category, name, arg1_name, arg1_val, arg2_name, \
                           arg2_val)                                       \
  RTC_NOOP()
#define TRACE_EVENT_COPY_BEGIN0(category, name) RTC_NOOP()
#define TRACE_EVENT_COPY_BEGIN1(category, name, arg1_name, arg1_val) RTC_NOOP()
#define TRACE_EVENT_COPY_BEGIN2(category, name, arg1_name, arg1_val, \
                                arg2_name, arg2_val)                 \
  RTC_NOOP()

#define TRACE_EVENT_END0(category, name) RTC_NOOP()
#define TRACE_EVENT_END1(category, name, arg1_name, arg1_val) RTC_NOOP()
#define TRACE_EVENT_END2(category, name, arg1_name, arg1_val, arg2_name, \
                         arg2_val)                                       \
  RTC_NOOP()
#define TRACE_EVENT_COPY_END0(category, name) RTC_NOOP()
#define TRACE_EVENT_COPY_END1(category, name, arg1_name, arg1_val) RTC_NOOP()
#define TRACE_EVENT_COPY_END2(category, name, arg1_name, arg1_val, arg2_name, \
                              arg2_val)                                       \
  RTC_NOOP()

#define TRACE_COUNTER1(category, name, value) RTC_NOOP()
#define TRACE_COPY_COUNTER1(category, name, value) RTC_NOOP()

#define TRACE_COUNTER2(category, name, value1_name, value1_val, value2_name, \
                       value2_val)                                           \
  RTC_NOOP()
#define TRACE_COPY_COUNTER2(category, name, value1_name, value1_val, \
                            value2_name, value2_val)                 \
  RTC_NOOP()

#define TRACE_COUNTER_ID1(category, name, id, value) RTC_NOOP()
#define TRACE_COPY_COUNTER_ID1(category, name, id, value) RTC_NOOP()

#define TRACE_COUNTER_ID2(category, name, id, value1_name, value1_val, \
                          value2_name, value2_val)                     \
  RTC_NOOP()
#define TRACE_COPY_COUNTER_ID2(category, name, id, value1_name, value1_val, \
                               value2_name, value2_val)                     \
  RTC_NOOP()

#define TRACE_EVENT_ASYNC_BEGIN0(category, name, id) RTC_NOOP()
#define TRACE_EVENT_ASYNC_BEGIN1(category, name, id, arg1_name, arg1_val) \
  RTC_NOOP()
#define TRACE_EVENT_ASYNC_BEGIN2(category, name, id, arg1_name, arg1_val, \
                                 arg2_name, arg2_val)                     \
  RTC_NOOP()
#define TRACE_EVENT_COPY_ASYNC_BEGIN0(category, name, id) RTC_NOOP()
#define TRACE_EVENT_COPY_ASYNC_BEGIN1(category, name, id, arg1_name, arg1_val) \
  RTC_NOOP()
#define TRACE_EVENT_COPY_ASYNC_BEGIN2(category, name, id, arg1_name, arg1_val, \
                                      arg2_name, arg2_val)                     \
  RTC_NOOP()

#define TRACE_EVENT_ASYNC_STEP0(category, name, id, step) RTC_NOOP()
#define TRACE_EVENT_ASYNC_STEP1(category, name, id, step, arg1_name, arg1_val) \
  RTC_NOOP()
#define TRACE_EVENT_COPY_ASYNC_STEP0(category, name, id, step) RTC_NOOP()
#define TRACE_EVENT_COPY_ASYNC_STEP1(category, name, id, step, arg1_name, \
                                     arg1_val)                            \
  RTC_NOOP()

#define TRACE_EVENT_ASYNC_END0(category, name, id) RTC_NOOP()
#define TRACE_EVENT_ASYNC_END1(category, name, id, arg1_name, arg1_val) \
  RTC_NOOP()
#define TRACE_EVENT_ASYNC_END2(category, name, id, arg1_name, arg1_val, \
                               arg2_name, arg2_val)                     \
  RTC_NOOP()
#define TRACE_EVENT_COPY_ASYNC_END0(category, name, id) RTC_NOOP()
#define TRACE_EVENT_COPY_ASYNC_END1(category, name, id, arg1_name, arg1_val) \
  RTC_NOOP()
#define TRACE_EVENT_COPY_ASYNC_END2(category, name, id, arg1_name, arg1_val, \
                                    arg2_name, arg2_val)                     \
  RTC_NOOP()

#define TRACE_EVENT_API_GET_CATEGORY_ENABLED ""

#define TRACE_EVENT_API_ADD_TRACE_EVENT RTC_NOOP()

#endif

#endif
