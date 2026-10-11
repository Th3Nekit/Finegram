//
// Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
//
// Borrowed from
// https://code.google.com/p/gperftools/source/browse/src/base/thread_annotations.h
// but adapted for clang attributes instead of the gcc.
//
// This header file contains the macro definitions for thread safety
// annotations that allow the developers to document the locking policies
// of their multi-threaded code. The annotations can also help program
// analysis tools to identify potential thread safety issues.

#ifndef RTC_BASE_THREAD_ANNOTATIONS_H_
#define RTC_BASE_THREAD_ANNOTATIONS_H_

#if defined(__clang__) && (!defined(SWIG))
#define RTC_THREAD_ANNOTATION_ATTRIBUTE__(x) __attribute__((x))
#else
#define RTC_THREAD_ANNOTATION_ATTRIBUTE__(x)
#endif

#define RTC_GUARDED_BY(x) RTC_THREAD_ANNOTATION_ATTRIBUTE__(guarded_by(x))

#define RTC_PT_GUARDED_BY(x) RTC_THREAD_ANNOTATION_ATTRIBUTE__(pt_guarded_by(x))

#define RTC_ACQUIRED_AFTER(x) \
  RTC_THREAD_ANNOTATION_ATTRIBUTE__(acquired_after(x))
#define RTC_ACQUIRED_BEFORE(x) \
  RTC_THREAD_ANNOTATION_ATTRIBUTE__(acquired_before(x))

#define RTC_EXCLUSIVE_LOCKS_REQUIRED(...) \
  RTC_THREAD_ANNOTATION_ATTRIBUTE__(exclusive_locks_required(__VA_ARGS__))
#define RTC_SHARED_LOCKS_REQUIRED(...) \
  RTC_THREAD_ANNOTATION_ATTRIBUTE__(shared_locks_required(__VA_ARGS__))

#define RTC_LOCKS_EXCLUDED(...) \
  RTC_THREAD_ANNOTATION_ATTRIBUTE__(locks_excluded(__VA_ARGS__))

#define RTC_LOCK_RETURNED(x) RTC_THREAD_ANNOTATION_ATTRIBUTE__(lock_returned(x))

#define RTC_LOCKABLE RTC_THREAD_ANNOTATION_ATTRIBUTE__(lockable)

#define RTC_SCOPED_LOCKABLE RTC_THREAD_ANNOTATION_ATTRIBUTE__(scoped_lockable)

#define RTC_EXCLUSIVE_LOCK_FUNCTION(...) \
  RTC_THREAD_ANNOTATION_ATTRIBUTE__(exclusive_lock_function(__VA_ARGS__))

#define RTC_SHARED_LOCK_FUNCTION(...) \
  RTC_THREAD_ANNOTATION_ATTRIBUTE__(shared_lock_function(__VA_ARGS__))

#define RTC_EXCLUSIVE_TRYLOCK_FUNCTION(...) \
  RTC_THREAD_ANNOTATION_ATTRIBUTE__(exclusive_trylock_function(__VA_ARGS__))

#define RTC_SHARED_TRYLOCK_FUNCTION(...) \
  RTC_THREAD_ANNOTATION_ATTRIBUTE__(shared_trylock_function(__VA_ARGS__))

#define RTC_UNLOCK_FUNCTION(...) \
  RTC_THREAD_ANNOTATION_ATTRIBUTE__(unlock_function(__VA_ARGS__))

#define RTC_ASSERT_EXCLUSIVE_LOCK(...) \
  RTC_THREAD_ANNOTATION_ATTRIBUTE__(assert_exclusive_lock(__VA_ARGS__))

#define RTC_NO_THREAD_SAFETY_ANALYSIS \
  RTC_THREAD_ANNOTATION_ATTRIBUTE__(no_thread_safety_analysis)

#endif
