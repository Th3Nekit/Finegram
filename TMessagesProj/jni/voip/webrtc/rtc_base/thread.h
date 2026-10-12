/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_THREAD_H_
#define RTC_BASE_THREAD_H_

#include <stdint.h>

#include <list>
#include <map>
#include <memory>
#include <queue>
#include <set>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "absl/strings/string_view.h"

#if defined(WEBRTC_POSIX)
#include <pthread.h>
#endif
#include "absl/base/attributes.h"
#include "absl/functional/any_invocable.h"
#include "api/function_view.h"
#include "api/location.h"
#include "api/task_queue/task_queue_base.h"
#include "api/units/time_delta.h"
#include "rtc_base/checks.h"
#include "rtc_base/platform_thread_types.h"
#include "rtc_base/socket_server.h"
#include "rtc_base/synchronization/mutex.h"
#include "rtc_base/system/rtc_export.h"
#include "rtc_base/thread_annotations.h"

#if defined(WEBRTC_WIN)
#include "rtc_base/win32.h"
#endif

#if RTC_DCHECK_IS_ON

#define RTC_LOG_THREAD_BLOCK_COUNT()                                        \
  rtc::Thread::ScopedCountBlockingCalls blocked_call_count_printer(         \
      [func = __func__](uint32_t actual_block, uint32_t could_block) {      \
        auto total = actual_block + could_block;                            \
        if (total) {                                                        \
          RTC_LOG(LS_WARNING) << "Blocking " << func << ": total=" << total \
                              << " (actual=" << actual_block                \
                              << ", could=" << could_block << ")";          \
        }                                                                   \
      })

#define RTC_DCHECK_BLOCK_COUNT_NO_MORE_THAN(x)                               \
  do {                                                                       \
    blocked_call_count_printer.set_minimum_call_count_for_callback(x + 1);   \
    RTC_DCHECK_LE(blocked_call_count_printer.GetTotalBlockedCallCount(), x); \
  } while (0)
#else
#define RTC_LOG_THREAD_BLOCK_COUNT()
#define RTC_DCHECK_BLOCK_COUNT_NO_MORE_THAN(x)
#endif

namespace rtc {

class Thread;

class RTC_EXPORT ThreadManager {
 public:
  static const int kForever = -1;

  static ThreadManager* Instance();

  static void Add(Thread* message_queue);
  static void Remove(Thread* message_queue);

  static void ProcessAllMessageQueuesForTesting();

  Thread* CurrentThread();
  void SetCurrentThread(Thread* thread);

  void ChangeCurrentThreadForTest(Thread* thread);

  Thread* WrapCurrentThread();
  void UnwrapCurrentThread();

#if RTC_DCHECK_IS_ON

  void RegisterSendAndCheckForCycles(Thread* source, Thread* target);
#endif

 private:
  ThreadManager();
  ~ThreadManager();

  ThreadManager(const ThreadManager&) = delete;
  ThreadManager& operator=(const ThreadManager&) = delete;

  void SetCurrentThreadInternal(Thread* thread);
  void AddInternal(Thread* message_queue);
  void RemoveInternal(Thread* message_queue);
  void ProcessAllMessageQueuesInternal();
#if RTC_DCHECK_IS_ON
  void RemoveFromSendGraph(Thread* thread) RTC_EXCLUSIVE_LOCKS_REQUIRED(crit_);
#endif

  std::vector<Thread*> message_queues_ RTC_GUARDED_BY(crit_);

  webrtc::Mutex crit_;

#if RTC_DCHECK_IS_ON

  std::map<Thread*, std::set<Thread*>> send_graph_ RTC_GUARDED_BY(crit_);
#endif

#if defined(WEBRTC_POSIX)
  pthread_key_t key_;
#endif

#if defined(WEBRTC_WIN)
  const DWORD key_;
#endif
};

class RTC_LOCKABLE RTC_EXPORT Thread : public webrtc::TaskQueueBase {
 public:
  static const int kForever = -1;

  explicit Thread(SocketServer* ss);
  explicit Thread(std::unique_ptr<SocketServer> ss);

  Thread(SocketServer* ss, bool do_init);
  Thread(std::unique_ptr<SocketServer> ss, bool do_init);

  ~Thread() override;

  Thread(const Thread&) = delete;
  Thread& operator=(const Thread&) = delete;

  static std::unique_ptr<Thread> CreateWithSocketServer();
  static std::unique_ptr<Thread> Create();
  static Thread* Current();

  class ScopedDisallowBlockingCalls {
   public:
    ScopedDisallowBlockingCalls();
    ScopedDisallowBlockingCalls(const ScopedDisallowBlockingCalls&) = delete;
    ScopedDisallowBlockingCalls& operator=(const ScopedDisallowBlockingCalls&) =
        delete;
    ~ScopedDisallowBlockingCalls();

   private:
    Thread* const thread_;
    const bool previous_state_;
  };

#if RTC_DCHECK_IS_ON
  class ScopedCountBlockingCalls {
   public:
    ScopedCountBlockingCalls(std::function<void(uint32_t, uint32_t)> callback);
    ScopedCountBlockingCalls(const ScopedDisallowBlockingCalls&) = delete;
    ScopedCountBlockingCalls& operator=(const ScopedDisallowBlockingCalls&) =
        delete;
    ~ScopedCountBlockingCalls();

    uint32_t GetBlockingCallCount() const;
    uint32_t GetCouldBeBlockingCallCount() const;
    uint32_t GetTotalBlockedCallCount() const;

    void set_minimum_call_count_for_callback(uint32_t minimum) {
      min_blocking_calls_for_callback_ = minimum;
    }

   private:
    Thread* const thread_;
    const uint32_t base_blocking_call_count_;
    const uint32_t base_could_be_blocking_call_count_;

    uint32_t min_blocking_calls_for_callback_ = 0;
    std::function<void(uint32_t, uint32_t)> result_callback_;
  };

  uint32_t GetBlockingCallCount() const;
  uint32_t GetCouldBeBlockingCallCount() const;
#endif

  SocketServer* socketserver();

  virtual void Quit();
  virtual bool IsQuitting();
  virtual void Restart();

  virtual bool IsProcessingMessagesForTesting();

  virtual int GetDelay();

  bool empty() const { return size() == 0u; }
  size_t size() const {
    webrtc::MutexLock lock(&mutex_);
    return messages_.size() + delayed_messages_.size();
  }

  bool IsCurrent() const;

  static bool SleepMs(int millis);

  const std::string& name() const { return name_; }
  bool SetName(absl::string_view name, const void* obj);

  void SetDispatchWarningMs(int deadline);

  bool Start();

  virtual void Stop();

  virtual void Run();

  void BlockingCall(
      FunctionView<void()> functor,
      const webrtc::Location& location = webrtc::Location::Current()) {
    BlockingCallImpl(std::move(functor), location);
  }

  template <typename Functor,
            typename ReturnT = std::invoke_result_t<Functor>,
            typename = typename std::enable_if_t<!std::is_void_v<ReturnT>>>
  ReturnT BlockingCall(
      Functor&& functor,
      const webrtc::Location& location = webrtc::Location::Current()) {
    ReturnT result;
    BlockingCall([&] { result = std::forward<Functor>(functor)(); }, location);
    return result;
  }

  void AllowInvokesToThread(Thread* thread);

  void DisallowAllInvokes();

  bool IsInvokeToThreadAllowed(rtc::Thread* target);

  void Delete() override;

  bool ProcessMessages(int cms);

  bool IsOwned();

  bool RunningForTest() { return IsRunning(); }

  bool WrapCurrent();
  void UnwrapCurrent();

  void DisallowBlockingCalls() { SetAllowBlockingCalls(false); }

 protected:
  class CurrentThreadSetter : CurrentTaskQueueSetter {
   public:
    explicit CurrentThreadSetter(Thread* thread)
        : CurrentTaskQueueSetter(thread),
          manager_(rtc::ThreadManager::Instance()),
          previous_(manager_->CurrentThread()) {
      manager_->ChangeCurrentThreadForTest(thread);
    }
    ~CurrentThreadSetter() { manager_->ChangeCurrentThreadForTest(previous_); }

   private:
    rtc::ThreadManager* const manager_;
    rtc::Thread* const previous_;
  };

  struct DelayedMessage {
    bool operator<(const DelayedMessage& dmsg) const {
      return (dmsg.run_time_ms < run_time_ms) ||
             ((dmsg.run_time_ms == run_time_ms) &&
              (dmsg.message_number < message_number));
    }

    int64_t delay_ms;
    int64_t run_time_ms;

    uint32_t message_number;

    mutable absl::AnyInvocable<void() &&> functor;
  };

  void PostTaskImpl(absl::AnyInvocable<void() &&> task,
                    const PostTaskTraits& traits,
                    const webrtc::Location& location) override;
  void PostDelayedTaskImpl(absl::AnyInvocable<void() &&> task,
                           webrtc::TimeDelta delay,
                           const PostDelayedTaskTraits& traits,
                           const webrtc::Location& location) override;

  virtual void BlockingCallImpl(FunctionView<void()> functor,
                                const webrtc::Location& location);

  void DoInit();

  void DoDestroy() RTC_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  void WakeUpSocketServer();

  void SafeWrapCurrent();

  void Join();

  static void AssertBlockingIsAllowedOnCurrentThread();

  friend class ScopedDisallowBlockingCalls;

 private:
  static const int kSlowDispatchLoggingThreshold = 50;

  absl::AnyInvocable<void() &&> Get(int cmsWait);
  void Dispatch(absl::AnyInvocable<void() &&> task);

  bool SetAllowBlockingCalls(bool allow);

#if defined(WEBRTC_WIN)
  static DWORD WINAPI PreRun(LPVOID context);
#else
  static void* PreRun(void* pv);
#endif

  bool WrapCurrentWithThreadManager(ThreadManager* thread_manager,
                                    bool need_synchronize_access);

  bool IsRunning();

  void EnsureIsCurrentTaskQueue();

  void ClearCurrentTaskQueue();

  std::queue<absl::AnyInvocable<void() &&>> messages_ RTC_GUARDED_BY(mutex_);
  std::priority_queue<DelayedMessage> delayed_messages_ RTC_GUARDED_BY(mutex_);
  uint32_t delayed_next_num_ RTC_GUARDED_BY(mutex_);
#if RTC_DCHECK_IS_ON
  uint32_t blocking_call_count_ RTC_GUARDED_BY(this) = 0;
  uint32_t could_be_blocking_call_count_ RTC_GUARDED_BY(this) = 0;
  std::vector<Thread*> allowed_threads_ RTC_GUARDED_BY(this);
  bool invoke_policy_enabled_ RTC_GUARDED_BY(this) = false;
#endif
  mutable webrtc::Mutex mutex_;
  bool fInitialized_;
  bool fDestroyed_;

  std::atomic<int> stop_;

  SocketServer* const ss_;

  std::unique_ptr<SocketServer> own_ss_;

  std::string name_;

#if defined(WEBRTC_POSIX)
  pthread_t thread_ = 0;
#endif

#if defined(WEBRTC_WIN)
  HANDLE thread_ = nullptr;
  DWORD thread_id_ = 0;
#endif

  bool owned_ = true;

  bool blocking_calls_allowed_ = true;

  std::unique_ptr<TaskQueueBase::CurrentTaskQueueSetter>
      task_queue_registration_;

  friend class ThreadManager;

  int dispatch_warning_ms_ RTC_GUARDED_BY(this) = kSlowDispatchLoggingThreshold;
};

class AutoThread : public Thread {
 public:
  AutoThread();
  ~AutoThread() override;

  AutoThread(const AutoThread&) = delete;
  AutoThread& operator=(const AutoThread&) = delete;
};

class AutoSocketServerThread : public Thread {
 public:
  explicit AutoSocketServerThread(SocketServer* ss);
  ~AutoSocketServerThread() override;

  AutoSocketServerThread(const AutoSocketServerThread&) = delete;
  AutoSocketServerThread& operator=(const AutoSocketServerThread&) = delete;

 private:
  rtc::Thread* old_thread_;
};
}

#endif
