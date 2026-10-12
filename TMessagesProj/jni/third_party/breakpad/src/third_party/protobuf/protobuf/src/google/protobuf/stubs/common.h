// Protocol Buffers - Google's data interchange format
// Copyright 2008 Google Inc.  All rights reserved.
// http://code.google.com/p/protobuf/
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

// Author: kenton@google.com (Kenton Varda) and others
//
// Contains basic types and utilities used by the rest of the library.

#ifndef GOOGLE_PROTOBUF_COMMON_H__
#define GOOGLE_PROTOBUF_COMMON_H__

#include <assert.h>
#include <stdlib.h>
#include <cstddef>
#include <string>
#include <string.h>
#if defined(__osf__)

#include <inttypes.h>
#elif !defined(_MSC_VER)
#include <stdint.h>
#endif

#if defined(_MSC_VER) && defined(_CPPUNWIND)
  #define PROTOBUF_USE_EXCEPTIONS
#elif defined(__EXCEPTIONS)
  #define PROTOBUF_USE_EXCEPTIONS
#endif
#ifdef PROTOBUF_USE_EXCEPTIONS
#include <exception>
#endif

#if defined(_WIN32) && defined(GetMessage)

inline BOOL GetMessage_Win32(
    LPMSG lpMsg, HWND hWnd,
    UINT wMsgFilterMin, UINT wMsgFilterMax) {
  return GetMessage(lpMsg, hWnd, wMsgFilterMin, wMsgFilterMax);
}
#undef GetMessage
inline BOOL GetMessage(
    LPMSG lpMsg, HWND hWnd,
    UINT wMsgFilterMin, UINT wMsgFilterMax) {
  return GetMessage_Win32(lpMsg, hWnd, wMsgFilterMin, wMsgFilterMax);
}
#endif

namespace std {}

namespace google {
namespace protobuf {

#undef GOOGLE_DISALLOW_EVIL_CONSTRUCTORS
#define GOOGLE_DISALLOW_EVIL_CONSTRUCTORS(TypeName)    \
  TypeName(const TypeName&);                           \
  void operator=(const TypeName&)

#if defined(_MSC_VER) && defined(PROTOBUF_USE_DLLS)
  #ifdef LIBPROTOBUF_EXPORTS
    #define LIBPROTOBUF_EXPORT __declspec(dllexport)
  #else
    #define LIBPROTOBUF_EXPORT __declspec(dllimport)
  #endif
  #ifdef LIBPROTOC_EXPORTS
    #define LIBPROTOC_EXPORT   __declspec(dllexport)
  #else
    #define LIBPROTOC_EXPORT   __declspec(dllimport)
  #endif
#else
  #define LIBPROTOBUF_EXPORT
  #define LIBPROTOC_EXPORT
#endif

namespace internal {

#define GOOGLE_PROTOBUF_VERSION 2004002

#define GOOGLE_PROTOBUF_MIN_LIBRARY_VERSION 2004000

static const int kMinHeaderVersionForLibrary = 2004000;

#define GOOGLE_PROTOBUF_MIN_PROTOC_VERSION 2004000

static const int kMinHeaderVersionForProtoc = 2004000;

void LIBPROTOBUF_EXPORT VerifyVersion(int headerVersion, int minLibraryVersion,
                                      const char* filename);

std::string LIBPROTOBUF_EXPORT VersionString(int version);

}

#define GOOGLE_PROTOBUF_VERIFY_VERSION                                    \
  ::google::protobuf::internal::VerifyVersion(                            \
    GOOGLE_PROTOBUF_VERSION, GOOGLE_PROTOBUF_MIN_LIBRARY_VERSION,         \
    __FILE__)

typedef unsigned int uint;

#ifdef _MSC_VER
typedef __int8  int8;
typedef __int16 int16;
typedef __int32 int32;
typedef __int64 int64;

typedef unsigned __int8  uint8;
typedef unsigned __int16 uint16;
typedef unsigned __int32 uint32;
typedef unsigned __int64 uint64;
#else
typedef int8_t  int8;
typedef int16_t int16;
typedef int32_t int32;
typedef int64_t int64;

typedef uint8_t  uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;
#endif

#undef GOOGLE_LONGLONG
#undef GOOGLE_ULONGLONG
#undef GOOGLE_LL_FORMAT

#ifdef _MSC_VER
#define GOOGLE_LONGLONG(x) x##I64
#define GOOGLE_ULONGLONG(x) x##UI64
#define GOOGLE_LL_FORMAT "I64"  // As in printf("%I64d", ...)
#else
#define GOOGLE_LONGLONG(x) x##LL
#define GOOGLE_ULONGLONG(x) x##ULL
#define GOOGLE_LL_FORMAT "ll"  // As in "%lld". Note that "q" is poor form also.
#endif

static const int32 kint32max = 0x7FFFFFFF;
static const int32 kint32min = -kint32max - 1;
static const int64 kint64max = GOOGLE_LONGLONG(0x7FFFFFFFFFFFFFFF);
static const int64 kint64min = -kint64max - 1;
static const uint32 kuint32max = 0xFFFFFFFFu;
static const uint64 kuint64max = GOOGLE_ULONGLONG(0xFFFFFFFFFFFFFFFF);

#ifndef GOOGLE_ATTRIBUTE_ALWAYS_INLINE
#if defined(__GNUC__) && (__GNUC__ > 3 ||(__GNUC__ == 3 && __GNUC_MINOR__ >= 1))

#define GOOGLE_ATTRIBUTE_ALWAYS_INLINE __attribute__ ((always_inline))
#else

#define GOOGLE_ATTRIBUTE_ALWAYS_INLINE
#endif
#endif

#ifndef GOOGLE_ATTRIBUTE_DEPRECATED
#ifdef __GNUC__

#define GOOGLE_ATTRIBUTE_DEPRECATED __attribute__((deprecated))
#else
#define GOOGLE_ATTRIBUTE_DEPRECATED
#endif
#endif

#ifndef GOOGLE_PREDICT_TRUE
#ifdef __GNUC__

#define GOOGLE_PREDICT_TRUE(x) (__builtin_expect(!!(x), 1))
#else
#define GOOGLE_PREDICT_TRUE
#endif
#endif

#ifndef GOOGLE_SAFE_CONCURRENT_WRITES_BEGIN
#define GOOGLE_SAFE_CONCURRENT_WRITES_BEGIN()
#endif
#ifndef GOOGLE_SAFE_CONCURRENT_WRITES_END
#define GOOGLE_SAFE_CONCURRENT_WRITES_END()
#endif

#undef GOOGLE_ARRAYSIZE
#define GOOGLE_ARRAYSIZE(a) \
  ((sizeof(a) / sizeof(*(a))) / \
   static_cast<size_t>(!(sizeof(a) % sizeof(*(a)))))

namespace internal {

template<typename To, typename From>
inline To implicit_cast(From const &f) {
  return f;
}

template<typename To, typename From>
inline To down_cast(From* f) {

  if (false) {
    implicit_cast<From*, To>(0);
  }

#if !defined(NDEBUG) && !defined(GOOGLE_PROTOBUF_NO_RTTI)
  assert(f == NULL || dynamic_cast<To>(f) != NULL);
#endif
  return static_cast<To>(f);
}

}

using internal::implicit_cast;
using internal::down_cast;

namespace internal {

template <bool>
struct CompileAssert {
};

}

#undef GOOGLE_COMPILE_ASSERT
#define GOOGLE_COMPILE_ASSERT(expr, msg) \
  typedef ::google::protobuf::internal::CompileAssert<(bool(expr))> \
          msg[bool(expr) ? 1 : -1]

namespace internal {

template <class C> class scoped_ptr;
template <class C> class scoped_array;

template <class C>
class scoped_ptr {
 public:

  typedef C element_type;

  explicit scoped_ptr(C* p = NULL) : ptr_(p) { }

  ~scoped_ptr() {
    enum { type_must_be_complete = sizeof(C) };
    delete ptr_;
  }

  void reset(C* p = NULL) {
    if (p != ptr_) {
      enum { type_must_be_complete = sizeof(C) };
      delete ptr_;
      ptr_ = p;
    }
  }

  C& operator*() const {
    assert(ptr_ != NULL);
    return *ptr_;
  }
  C* operator->() const  {
    assert(ptr_ != NULL);
    return ptr_;
  }
  C* get() const { return ptr_; }

  bool operator==(C* p) const { return ptr_ == p; }
  bool operator!=(C* p) const { return ptr_ != p; }

  void swap(scoped_ptr& p2) {
    C* tmp = ptr_;
    ptr_ = p2.ptr_;
    p2.ptr_ = tmp;
  }

  C* release() {
    C* retVal = ptr_;
    ptr_ = NULL;
    return retVal;
  }

 private:
  C* ptr_;

  template <class C2> bool operator==(scoped_ptr<C2> const& p2) const;
  template <class C2> bool operator!=(scoped_ptr<C2> const& p2) const;

  scoped_ptr(const scoped_ptr&);
  void operator=(const scoped_ptr&);
};

template <class C>
class scoped_array {
 public:

  typedef C element_type;

  explicit scoped_array(C* p = NULL) : array_(p) { }

  ~scoped_array() {
    enum { type_must_be_complete = sizeof(C) };
    delete[] array_;
  }

  void reset(C* p = NULL) {
    if (p != array_) {
      enum { type_must_be_complete = sizeof(C) };
      delete[] array_;
      array_ = p;
    }
  }

  C& operator[](std::ptrdiff_t i) const {
    assert(i >= 0);
    assert(array_ != NULL);
    return array_[i];
  }

  C* get() const {
    return array_;
  }

  bool operator==(C* p) const { return array_ == p; }
  bool operator!=(C* p) const { return array_ != p; }

  void swap(scoped_array& p2) {
    C* tmp = array_;
    array_ = p2.array_;
    p2.array_ = tmp;
  }

  C* release() {
    C* retVal = array_;
    array_ = NULL;
    return retVal;
  }

 private:
  C* array_;

  template <class C2> bool operator==(scoped_array<C2> const& p2) const;
  template <class C2> bool operator!=(scoped_array<C2> const& p2) const;

  scoped_array(const scoped_array&);
  void operator=(const scoped_array&);
};

}

using internal::scoped_ptr;
using internal::scoped_array;

enum LogLevel {
  LOGLEVEL_INFO,

  LOGLEVEL_WARNING,

  LOGLEVEL_ERROR,

  LOGLEVEL_FATAL,

#ifdef NDEBUG
  LOGLEVEL_DFATAL = LOGLEVEL_ERROR
#else
  LOGLEVEL_DFATAL = LOGLEVEL_FATAL
#endif
};

namespace internal {

class LogFinisher;

class LIBPROTOBUF_EXPORT LogMessage {
 public:
  LogMessage(LogLevel level, const char* filename, int line);
  ~LogMessage();

  LogMessage& operator<<(const std::string& value);
  LogMessage& operator<<(const char* value);
  LogMessage& operator<<(char value);
  LogMessage& operator<<(int value);
  LogMessage& operator<<(uint value);
  LogMessage& operator<<(long value);
  LogMessage& operator<<(unsigned long value);
  LogMessage& operator<<(double value);

 private:
  friend class LogFinisher;
  void Finish();

  LogLevel level_;
  const char* filename_;
  int line_;
  std::string message_;
};

class LIBPROTOBUF_EXPORT LogFinisher {
 public:
  void operator=(LogMessage& other);
};

}

#undef GOOGLE_LOG
#undef GOOGLE_LOG_IF

#undef GOOGLE_CHECK
#undef GOOGLE_CHECK_EQ
#undef GOOGLE_CHECK_NE
#undef GOOGLE_CHECK_LT
#undef GOOGLE_CHECK_LE
#undef GOOGLE_CHECK_GT
#undef GOOGLE_CHECK_GE

#undef GOOGLE_DLOG
#undef GOOGLE_DCHECK
#undef GOOGLE_DCHECK_EQ
#undef GOOGLE_DCHECK_NE
#undef GOOGLE_DCHECK_LT
#undef GOOGLE_DCHECK_LE
#undef GOOGLE_DCHECK_GT
#undef GOOGLE_DCHECK_GE

#define GOOGLE_LOG(LEVEL)                                                 \
  ::google::protobuf::internal::LogFinisher() =                           \
    ::google::protobuf::internal::LogMessage(                             \
      ::google::protobuf::LOGLEVEL_##LEVEL, __FILE__, __LINE__)
#define GOOGLE_LOG_IF(LEVEL, CONDITION) \
  !(CONDITION) ? (void)0 : GOOGLE_LOG(LEVEL)

#define GOOGLE_CHECK(EXPRESSION) \
  GOOGLE_LOG_IF(FATAL, !(EXPRESSION)) << "CHECK failed: " #EXPRESSION ": "
#define GOOGLE_CHECK_EQ(A, B) GOOGLE_CHECK((A) == (B))
#define GOOGLE_CHECK_NE(A, B) GOOGLE_CHECK((A) != (B))
#define GOOGLE_CHECK_LT(A, B) GOOGLE_CHECK((A) <  (B))
#define GOOGLE_CHECK_LE(A, B) GOOGLE_CHECK((A) <= (B))
#define GOOGLE_CHECK_GT(A, B) GOOGLE_CHECK((A) >  (B))
#define GOOGLE_CHECK_GE(A, B) GOOGLE_CHECK((A) >= (B))

#ifdef NDEBUG

#define GOOGLE_DLOG GOOGLE_LOG_IF(INFO, false)

#define GOOGLE_DCHECK(EXPRESSION) while(false) GOOGLE_CHECK(EXPRESSION)
#define GOOGLE_DCHECK_EQ(A, B) GOOGLE_DCHECK((A) == (B))
#define GOOGLE_DCHECK_NE(A, B) GOOGLE_DCHECK((A) != (B))
#define GOOGLE_DCHECK_LT(A, B) GOOGLE_DCHECK((A) <  (B))
#define GOOGLE_DCHECK_LE(A, B) GOOGLE_DCHECK((A) <= (B))
#define GOOGLE_DCHECK_GT(A, B) GOOGLE_DCHECK((A) >  (B))
#define GOOGLE_DCHECK_GE(A, B) GOOGLE_DCHECK((A) >= (B))

#else

#define GOOGLE_DLOG GOOGLE_LOG

#define GOOGLE_DCHECK    GOOGLE_CHECK
#define GOOGLE_DCHECK_EQ GOOGLE_CHECK_EQ
#define GOOGLE_DCHECK_NE GOOGLE_CHECK_NE
#define GOOGLE_DCHECK_LT GOOGLE_CHECK_LT
#define GOOGLE_DCHECK_LE GOOGLE_CHECK_LE
#define GOOGLE_DCHECK_GT GOOGLE_CHECK_GT
#define GOOGLE_DCHECK_GE GOOGLE_CHECK_GE

#endif

typedef void LogHandler(LogLevel level, const char* filename, int line,
                        const std::string& message);

LIBPROTOBUF_EXPORT LogHandler* SetLogHandler(LogHandler* new_func);

class LIBPROTOBUF_EXPORT LogSilencer {
 public:
  LogSilencer();
  ~LogSilencer();
};

class LIBPROTOBUF_EXPORT Closure {
 public:
  Closure() {}
  virtual ~Closure();

  virtual void Run() = 0;

 private:
  GOOGLE_DISALLOW_EVIL_CONSTRUCTORS(Closure);
};

namespace internal {

class LIBPROTOBUF_EXPORT FunctionClosure0 : public Closure {
 public:
  typedef void (*FunctionType)();

  FunctionClosure0(FunctionType function, bool self_deleting)
    : function_(function), self_deleting_(self_deleting) {}
  ~FunctionClosure0();

  void Run() {
    bool needs_delete = self_deleting_;
    function_();
    if (needs_delete) delete this;
  }

 private:
  FunctionType function_;
  bool self_deleting_;
};

template <typename Class>
class MethodClosure0 : public Closure {
 public:
  typedef void (Class::*MethodType)();

  MethodClosure0(Class* object, MethodType method, bool self_deleting)
    : object_(object), method_(method), self_deleting_(self_deleting) {}
  ~MethodClosure0() {}

  void Run() {
    bool needs_delete = self_deleting_;
    (object_->*method_)();
    if (needs_delete) delete this;
  }

 private:
  Class* object_;
  MethodType method_;
  bool self_deleting_;
};

template <typename Arg1>
class FunctionClosure1 : public Closure {
 public:
  typedef void (*FunctionType)(Arg1 arg1);

  FunctionClosure1(FunctionType function, bool self_deleting,
                   Arg1 arg1)
    : function_(function), self_deleting_(self_deleting),
      arg1_(arg1) {}
  ~FunctionClosure1() {}

  void Run() {
    bool needs_delete = self_deleting_;
    function_(arg1_);
    if (needs_delete) delete this;
  }

 private:
  FunctionType function_;
  bool self_deleting_;
  Arg1 arg1_;
};

template <typename Class, typename Arg1>
class MethodClosure1 : public Closure {
 public:
  typedef void (Class::*MethodType)(Arg1 arg1);

  MethodClosure1(Class* object, MethodType method, bool self_deleting,
                 Arg1 arg1)
    : object_(object), method_(method), self_deleting_(self_deleting),
      arg1_(arg1) {}
  ~MethodClosure1() {}

  void Run() {
    bool needs_delete = self_deleting_;
    (object_->*method_)(arg1_);
    if (needs_delete) delete this;
  }

 private:
  Class* object_;
  MethodType method_;
  bool self_deleting_;
  Arg1 arg1_;
};

template <typename Arg1, typename Arg2>
class FunctionClosure2 : public Closure {
 public:
  typedef void (*FunctionType)(Arg1 arg1, Arg2 arg2);

  FunctionClosure2(FunctionType function, bool self_deleting,
                   Arg1 arg1, Arg2 arg2)
    : function_(function), self_deleting_(self_deleting),
      arg1_(arg1), arg2_(arg2) {}
  ~FunctionClosure2() {}

  void Run() {
    bool needs_delete = self_deleting_;
    function_(arg1_, arg2_);
    if (needs_delete) delete this;
  }

 private:
  FunctionType function_;
  bool self_deleting_;
  Arg1 arg1_;
  Arg2 arg2_;
};

template <typename Class, typename Arg1, typename Arg2>
class MethodClosure2 : public Closure {
 public:
  typedef void (Class::*MethodType)(Arg1 arg1, Arg2 arg2);

  MethodClosure2(Class* object, MethodType method, bool self_deleting,
                 Arg1 arg1, Arg2 arg2)
    : object_(object), method_(method), self_deleting_(self_deleting),
      arg1_(arg1), arg2_(arg2) {}
  ~MethodClosure2() {}

  void Run() {
    bool needs_delete = self_deleting_;
    (object_->*method_)(arg1_, arg2_);
    if (needs_delete) delete this;
  }

 private:
  Class* object_;
  MethodType method_;
  bool self_deleting_;
  Arg1 arg1_;
  Arg2 arg2_;
};

}

inline Closure* NewCallback(void (*function)()) {
  return new internal::FunctionClosure0(function, true);
}

inline Closure* NewPermanentCallback(void (*function)()) {
  return new internal::FunctionClosure0(function, false);
}

template <typename Class>
inline Closure* NewCallback(Class* object, void (Class::*method)()) {
  return new internal::MethodClosure0<Class>(object, method, true);
}

template <typename Class>
inline Closure* NewPermanentCallback(Class* object, void (Class::*method)()) {
  return new internal::MethodClosure0<Class>(object, method, false);
}

template <typename Arg1>
inline Closure* NewCallback(void (*function)(Arg1),
                            Arg1 arg1) {
  return new internal::FunctionClosure1<Arg1>(function, true, arg1);
}

template <typename Arg1>
inline Closure* NewPermanentCallback(void (*function)(Arg1),
                                     Arg1 arg1) {
  return new internal::FunctionClosure1<Arg1>(function, false, arg1);
}

template <typename Class, typename Arg1>
inline Closure* NewCallback(Class* object, void (Class::*method)(Arg1),
                            Arg1 arg1) {
  return new internal::MethodClosure1<Class, Arg1>(object, method, true, arg1);
}

template <typename Class, typename Arg1>
inline Closure* NewPermanentCallback(Class* object, void (Class::*method)(Arg1),
                                     Arg1 arg1) {
  return new internal::MethodClosure1<Class, Arg1>(object, method, false, arg1);
}

template <typename Arg1, typename Arg2>
inline Closure* NewCallback(void (*function)(Arg1, Arg2),
                            Arg1 arg1, Arg2 arg2) {
  return new internal::FunctionClosure2<Arg1, Arg2>(
    function, true, arg1, arg2);
}

template <typename Arg1, typename Arg2>
inline Closure* NewPermanentCallback(void (*function)(Arg1, Arg2),
                                     Arg1 arg1, Arg2 arg2) {
  return new internal::FunctionClosure2<Arg1, Arg2>(
    function, false, arg1, arg2);
}

template <typename Class, typename Arg1, typename Arg2>
inline Closure* NewCallback(Class* object, void (Class::*method)(Arg1, Arg2),
                            Arg1 arg1, Arg2 arg2) {
  return new internal::MethodClosure2<Class, Arg1, Arg2>(
    object, method, true, arg1, arg2);
}

template <typename Class, typename Arg1, typename Arg2>
inline Closure* NewPermanentCallback(
    Class* object, void (Class::*method)(Arg1, Arg2),
    Arg1 arg1, Arg2 arg2) {
  return new internal::MethodClosure2<Class, Arg1, Arg2>(
    object, method, false, arg1, arg2);
}

void LIBPROTOBUF_EXPORT DoNothing();

namespace internal {

class LIBPROTOBUF_EXPORT Mutex {
 public:

  Mutex();

  ~Mutex();

  void Lock();

  void Unlock();

  void AssertHeld();

 private:
  struct Internal;
  Internal* mInternal;

  GOOGLE_DISALLOW_EVIL_CONSTRUCTORS(Mutex);
};

class LIBPROTOBUF_EXPORT MutexLock {
 public:
  explicit MutexLock(Mutex *mu) : mu_(mu) { this->mu_->Lock(); }
  ~MutexLock() { this->mu_->Unlock(); }
 private:
  Mutex *const mu_;
  GOOGLE_DISALLOW_EVIL_CONSTRUCTORS(MutexLock);
};

typedef MutexLock ReaderMutexLock;
typedef MutexLock WriterMutexLock;

class LIBPROTOBUF_EXPORT MutexLockMaybe {
 public:
  explicit MutexLockMaybe(Mutex *mu) :
    mu_(mu) { if (this->mu_ != NULL) { this->mu_->Lock(); } }
  ~MutexLockMaybe() { if (this->mu_ != NULL) { this->mu_->Unlock(); } }
 private:
  Mutex *const mu_;
  GOOGLE_DISALLOW_EVIL_CONSTRUCTORS(MutexLockMaybe);
};

}

using internal::Mutex;
using internal::MutexLock;
using internal::ReaderMutexLock;
using internal::WriterMutexLock;
using internal::MutexLockMaybe;

namespace internal {

template<typename T> struct remove_pointer { typedef T type; };
template<typename T> struct remove_pointer<T*> { typedef T type; };
template<typename T> struct remove_pointer<T* const> { typedef T type; };
template<typename T> struct remove_pointer<T* volatile> { typedef T type; };
template<typename T> struct remove_pointer<T* const volatile> {
  typedef T type; };

LIBPROTOBUF_EXPORT bool IsStructurallyValidUTF8(const char* buf, int len);

}

LIBPROTOBUF_EXPORT void ShutdownProtobufLibrary();

namespace internal {

LIBPROTOBUF_EXPORT void OnShutdown(void (*func)());

}

#ifdef PROTOBUF_USE_EXCEPTIONS
class FatalException : public std::exception {
 public:
  FatalException(const char* filename, int line, const std::string& message)
      : filename_(filename), line_(line), message_(message) {}
  virtual ~FatalException() throw();

  virtual const char* what() const throw();

  const char* filename() const { return filename_; }
  int line() const { return line_; }
  const std::string& message() const { return message_; }

 private:
  const char* filename_;
  const int line_;
  const std::string message_;
};
#endif

using namespace std;

}
}

#endif
