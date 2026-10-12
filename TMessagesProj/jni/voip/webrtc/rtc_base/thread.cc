/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/thread.h"

#include "absl/strings/string_view.h"
#include "api/task_queue/task_queue_base.h"
#include "api/units/time_delta.h"
#include "rtc_base/socket_server.h"

#if defined(WEBRTC_WIN)
#include <comdef.h>
#elif defined(WEBRTC_POSIX)
#include <time.h>
#else
#error "Either WEBRTC_WIN or WEBRTC_POSIX needs to be defined."
#endif

#if defined(WEBRTC_WIN)

#pragma warning(disable : 4722)
#endif

#include <stdio.h>

#include <utility>

#include "absl/algorithm/container.h"
#include "absl/cleanup/cleanup.h"
#include "api/sequence_checker.h"
#include "rtc_base/checks.h"
#include "rtc_base/event.h"
#include "rtc_base/internal/default_socket_server.h"
#include "rtc_base/logging.h"
#include "rtc_base/null_socket_server.h"
#include "rtc_base/synchronization/mutex.h"
#include "rtc_base/time_utils.h"
#include "rtc_base/trace_event.h"

#if defined(WEBRTC_MAC)
#include "rtc_base/system/cocoa_threading.h"

extern "C" {
void* objc_autoreleasePoolPush(void);
void objc_autoreleasePoolPop(void* pool);
}

namespace {
class ScopedAutoReleasePool {
 public:
  ScopedAutoReleasePool() : pool_(objc_autoreleasePoolPush()) {}
  ~ScopedAutoReleasePool() { objc_autoreleasePoolPop(pool_); }

 private:
  void* const pool_;
};
}
#endif

namespace rtc {

using ::webrtc::MutexLock;
using ::webrtc::TimeDelta;

ThreadManager* ThreadManager::Instance() {
  static ThreadManager* const thread_manager = new ThreadManager();
  return thread_manager;
}

ThreadManager::~ThreadManager() {

  RTC_DCHECK_NOTREACHED() << "ThreadManager should never be destructed.";
}

void ThreadManager::Add(Thread* message_queue) {
  return Instance()->AddInternal(message_queue);
}
void ThreadManager::AddInternal(Thread* message_queue) {
  MutexLock cs(&crit_);
  message_queues_.push_back(message_queue);
}

void ThreadManager::Remove(Thread* message_queue) {
  return Instance()->RemoveInternal(message_queue);
}
void ThreadManager::RemoveInternal(Thread* message_queue) {
  {
    MutexLock cs(&crit_);
    std::vector<Thread*>::iterator iter;
    iter = absl::c_find(message_queues_, message_queue);
    if (iter != message_queues_.end()) {
      message_queues_.erase(iter);
    }
#if RTC_DCHECK_IS_ON
    RemoveFromSendGraph(message_queue);
#endif
  }
}

#if RTC_DCHECK_IS_ON
void ThreadManager::RemoveFromSendGraph(Thread* thread) {
  for (auto it = send_graph_.begin(); it != send_graph_.end();) {
    if (it->first == thread) {
      it = send_graph_.erase(it);
    } else {
      it->second.erase(thread);
      ++it;
    }
  }
}

void ThreadManager::RegisterSendAndCheckForCycles(Thread* source,
                                                  Thread* target) {
  RTC_DCHECK(source);
  RTC_DCHECK(target);

  MutexLock cs(&crit_);
  std::deque<Thread*> all_targets({target});

  for (size_t i = 0; i < all_targets.size(); i++) {
    const auto& targets = send_graph_[all_targets[i]];
    all_targets.insert(all_targets.end(), targets.begin(), targets.end());
  }
  RTC_CHECK_EQ(absl::c_count(all_targets, source), 0)
      << " send loop between " << source->name() << " and " << target->name();

  send_graph_[source].insert(target);
}
#endif

void ThreadManager::ProcessAllMessageQueuesForTesting() {
  return Instance()->ProcessAllMessageQueuesInternal();
}

void ThreadManager::ProcessAllMessageQueuesInternal() {

  std::atomic<int> queues_not_done(0);

  {
    MutexLock cs(&crit_);
    for (Thread* queue : message_queues_) {
      if (!queue->IsProcessingMessagesForTesting()) {

        continue;
      }
      queues_not_done.fetch_add(1);

      absl::Cleanup sub = [&queues_not_done] { queues_not_done.fetch_sub(1); };

      queue->PostDelayedTask([sub = std::move(sub)] {}, TimeDelta::Zero());
    }
  }

  rtc::Thread* current = rtc::Thread::Current();

  while (queues_not_done.load() > 0) {
    if (current) {
      current->ProcessMessages(0);
    }
  }
}

Thread* Thread::Current() {
  ThreadManager* manager = ThreadManager::Instance();
  Thread* thread = manager->CurrentThread();

  return thread;
}

#if defined(WEBRTC_POSIX)
ThreadManager::ThreadManager() {
#if defined(WEBRTC_MAC)
  InitCocoaMultiThreading();
#endif
  pthread_key_create(&key_, nullptr);
}

Thread* ThreadManager::CurrentThread() {
  return static_cast<Thread*>(pthread_getspecific(key_));
}

void ThreadManager::SetCurrentThreadInternal(Thread* thread) {
  pthread_setspecific(key_, thread);
}
#endif

#if defined(WEBRTC_WIN)
ThreadManager::ThreadManager() : key_(TlsAlloc()) {}

Thread* ThreadManager::CurrentThread() {
  return static_cast<Thread*>(TlsGetValue(key_));
}

void ThreadManager::SetCurrentThreadInternal(Thread* thread) {
  TlsSetValue(key_, thread);
}
#endif

void ThreadManager::SetCurrentThread(Thread* thread) {
#if RTC_DLOG_IS_ON
  if (CurrentThread() && thread) {
    RTC_DLOG(LS_ERROR) << "SetCurrentThread: Overwriting an existing value?";
  }
#endif

  if (thread) {
    thread->EnsureIsCurrentTaskQueue();
  } else {
    Thread* current = CurrentThread();
    if (current) {

      current->ClearCurrentTaskQueue();
    }
  }

  SetCurrentThreadInternal(thread);
}

void rtc::ThreadManager::ChangeCurrentThreadForTest(rtc::Thread* thread) {
  SetCurrentThreadInternal(thread);
}

Thread* ThreadManager::WrapCurrentThread() {
  Thread* result = CurrentThread();
  if (nullptr == result) {
    result = new Thread(CreateDefaultSocketServer());
    result->WrapCurrentWithThreadManager(this, true);
  }
  return result;
}

void ThreadManager::UnwrapCurrentThread() {
  Thread* t = CurrentThread();
  if (t && !(t->IsOwned())) {
    t->UnwrapCurrent();
    delete t;
  }
}

Thread::ScopedDisallowBlockingCalls::ScopedDisallowBlockingCalls()
    : thread_(Thread::Current()),
      previous_state_(thread_->SetAllowBlockingCalls(false)) {}

Thread::ScopedDisallowBlockingCalls::~ScopedDisallowBlockingCalls() {
  RTC_DCHECK(thread_->IsCurrent());
  thread_->SetAllowBlockingCalls(previous_state_);
}

#if RTC_DCHECK_IS_ON
Thread::ScopedCountBlockingCalls::ScopedCountBlockingCalls(
    std::function<void(uint32_t, uint32_t)> callback)
    : thread_(Thread::Current()),
      base_blocking_call_count_(thread_->GetBlockingCallCount()),
      base_could_be_blocking_call_count_(
          thread_->GetCouldBeBlockingCallCount()),
      result_callback_(std::move(callback)) {}

Thread::ScopedCountBlockingCalls::~ScopedCountBlockingCalls() {
  if (GetTotalBlockedCallCount() >= min_blocking_calls_for_callback_) {
    result_callback_(GetBlockingCallCount(), GetCouldBeBlockingCallCount());
  }
}

uint32_t Thread::ScopedCountBlockingCalls::GetBlockingCallCount() const {
  return thread_->GetBlockingCallCount() - base_blocking_call_count_;
}

uint32_t Thread::ScopedCountBlockingCalls::GetCouldBeBlockingCallCount() const {
  return thread_->GetCouldBeBlockingCallCount() -
         base_could_be_blocking_call_count_;
}

uint32_t Thread::ScopedCountBlockingCalls::GetTotalBlockedCallCount() const {
  return GetBlockingCallCount() + GetCouldBeBlockingCallCount();
}
#endif

Thread::Thread(SocketServer* ss) : Thread(ss,             true) {}

Thread::Thread(std::unique_ptr<SocketServer> ss)
    : Thread(std::move(ss),             true) {}

Thread::Thread(SocketServer* ss, bool do_init)
    : delayed_next_num_(0),
      fInitialized_(false),
      fDestroyed_(false),
      stop_(0),
      ss_(ss) {
  RTC_DCHECK(ss);
  ss_->SetMessageQueue(this);
  SetName("Thread", this);
  if (do_init) {
    DoInit();
  }
}

Thread::Thread(std::unique_ptr<SocketServer> ss, bool do_init)
    : Thread(ss.get(), do_init) {
  own_ss_ = std::move(ss);
}

Thread::~Thread() {
  Stop();
  DoDestroy();
}

void Thread::DoInit() {
  if (fInitialized_) {
    return;
  }

  fInitialized_ = true;
  ThreadManager::Add(this);
}

void Thread::DoDestroy() {
  if (fDestroyed_) {
    return;
  }

  fDestroyed_ = true;

  if (ss_) {
    ss_->SetMessageQueue(nullptr);
  }
  ThreadManager::Remove(this);

  CurrentTaskQueueSetter set_current(this);
  messages_ = {};
  delayed_messages_ = {};
}

SocketServer* Thread::socketserver() {
  return ss_;
}

void Thread::WakeUpSocketServer() {
  ss_->WakeUp();
}

void Thread::Quit() {
  stop_.store(1, std::memory_order_release);
  WakeUpSocketServer();
}

bool Thread::IsQuitting() {
  return stop_.load(std::memory_order_acquire) != 0;
}

void Thread::Restart() {
  stop_.store(0, std::memory_order_release);
}

absl::AnyInvocable<void() &&> Thread::Get(int cmsWait) {

  int64_t cmsTotal = cmsWait;
  int64_t cmsElapsed = 0;
  int64_t msStart = TimeMillis();
  int64_t msCurrent = msStart;
  while (true) {

    int64_t cmsDelayNext = kForever;
    {

      MutexLock lock(&mutex_);

      while (!delayed_messages_.empty()) {
        if (msCurrent < delayed_messages_.top().run_time_ms) {
          cmsDelayNext =
              TimeDiff(delayed_messages_.top().run_time_ms, msCurrent);
          break;
        }
        messages_.push(std::move(delayed_messages_.top().functor));
        delayed_messages_.pop();
      }

      if (!messages_.empty()) {
        absl::AnyInvocable<void()&&> task = std::move(messages_.front());
        messages_.pop();
        return task;
      }
    }

    if (IsQuitting())
      break;

    int64_t cmsNext;
    if (cmsWait == kForever) {
      cmsNext = cmsDelayNext;
    } else {
      cmsNext = std::max<int64_t>(0, cmsTotal - cmsElapsed);
      if ((cmsDelayNext != kForever) && (cmsDelayNext < cmsNext))
        cmsNext = cmsDelayNext;
    }

    {

      if (!ss_->Wait(cmsNext == kForever ? SocketServer::kForever
                                         : webrtc::TimeDelta::Millis(cmsNext),
                                    true))
        return nullptr;
    }

    msCurrent = TimeMillis();
    cmsElapsed = TimeDiff(msCurrent, msStart);
    if (cmsWait != kForever) {
      if (cmsElapsed >= cmsWait)
        return nullptr;
    }
  }
  return nullptr;
}

void Thread::PostTaskImpl(absl::AnyInvocable<void() &&> task,
                          const PostTaskTraits& traits,
                          const webrtc::Location& location) {
  if (IsQuitting()) {
    return;
  }

  {
    MutexLock lock(&mutex_);
    messages_.push(std::move(task));
  }
  WakeUpSocketServer();
}

void Thread::PostDelayedTaskImpl(absl::AnyInvocable<void() &&> task,
                                 webrtc::TimeDelta delay,
                                 const PostDelayedTaskTraits& traits,
                                 const webrtc::Location& location) {
  if (IsQuitting()) {
    return;
  }

  int64_t delay_ms = delay.RoundUpTo(webrtc::TimeDelta::Millis(1)).ms<int>();
  int64_t run_time_ms = TimeAfter(delay_ms);
  {
    MutexLock lock(&mutex_);
    delayed_messages_.push({.delay_ms = delay_ms,
                            .run_time_ms = run_time_ms,
                            .message_number = delayed_next_num_,
                            .functor = std::move(task)});

    ++delayed_next_num_;
    RTC_DCHECK_NE(0, delayed_next_num_);
  }
  WakeUpSocketServer();
}

int Thread::GetDelay() {
  MutexLock lock(&mutex_);

  if (!messages_.empty())
    return 0;

  if (!delayed_messages_.empty()) {
    int delay = TimeUntil(delayed_messages_.top().run_time_ms);
    if (delay < 0)
      delay = 0;
    return delay;
  }

  return kForever;
}

void Thread::Dispatch(absl::AnyInvocable<void() &&> task) {
  TRACE_EVENT0("webrtc", "Thread::Dispatch");
  RTC_DCHECK_RUN_ON(this);
  int64_t start_time = TimeMillis();
  std::move(task)();
  int64_t end_time = TimeMillis();
  int64_t diff = TimeDiff(end_time, start_time);
  if (diff >= dispatch_warning_ms_) {
    RTC_LOG(LS_INFO) << "Message to " << name() << " took " << diff
                     << "ms to dispatch.";

    dispatch_warning_ms_ = diff + 1;
  }
}

bool Thread::IsCurrent() const {
  return ThreadManager::Instance()->CurrentThread() == this;
}

std::unique_ptr<Thread> Thread::CreateWithSocketServer() {
  return std::unique_ptr<Thread>(new Thread(CreateDefaultSocketServer()));
}

std::unique_ptr<Thread> Thread::Create() {
  return std::unique_ptr<Thread>(
      new Thread(std::unique_ptr<SocketServer>(new NullSocketServer())));
}

bool Thread::SleepMs(int milliseconds) {
  AssertBlockingIsAllowedOnCurrentThread();

#if defined(WEBRTC_WIN)
  ::Sleep(milliseconds);
  return true;
#else

  struct timespec ts;
  ts.tv_sec = milliseconds / 1000;
  ts.tv_nsec = (milliseconds % 1000) * 1000000;
  int ret = nanosleep(&ts, nullptr);
  if (ret != 0) {
    RTC_LOG_ERR(LS_WARNING) << "nanosleep() returning early";
    return false;
  }
  return true;
#endif
}

bool Thread::SetName(absl::string_view name, const void* obj) {
  RTC_DCHECK(!IsRunning());

  name_ = std::string(name);
  if (obj) {

    char buf[30];
    snprintf(buf, sizeof(buf), " 0x%p", obj);
    name_ += buf;
  }
  return true;
}

void Thread::SetDispatchWarningMs(int deadline) {
  if (!IsCurrent()) {
    PostTask([this, deadline]() { SetDispatchWarningMs(deadline); });
    return;
  }
  RTC_DCHECK_RUN_ON(this);
  dispatch_warning_ms_ = deadline;
}

bool Thread::Start() {
  RTC_DCHECK(!IsRunning());

  if (IsRunning())
    return false;

  Restart();

  ThreadManager::Instance();

  owned_ = true;

#if defined(WEBRTC_WIN)
  thread_ = CreateThread(nullptr, 0, PreRun, this, 0, &thread_id_);
  if (!thread_) {
    return false;
  }
#elif defined(WEBRTC_POSIX)
  pthread_attr_t attr;
  pthread_attr_init(&attr);

  int error_code = pthread_create(&thread_, &attr, PreRun, this);
  if (0 != error_code) {
    RTC_LOG(LS_ERROR) << "Unable to create pthread, error " << error_code;
    thread_ = 0;
    return false;
  }
  RTC_DCHECK(thread_);
#endif
  return true;
}

bool Thread::WrapCurrent() {
  return WrapCurrentWithThreadManager(ThreadManager::Instance(), true);
}

void Thread::UnwrapCurrent() {

  ThreadManager::Instance()->SetCurrentThread(nullptr);
#if defined(WEBRTC_WIN)
  if (thread_ != nullptr) {
    if (!CloseHandle(thread_)) {
      RTC_LOG_GLE(LS_ERROR)
          << "When unwrapping thread, failed to close handle.";
    }
    thread_ = nullptr;
    thread_id_ = 0;
  }
#elif defined(WEBRTC_POSIX)
  thread_ = 0;
#endif
}

void Thread::SafeWrapCurrent() {
  WrapCurrentWithThreadManager(ThreadManager::Instance(), false);
}

void Thread::Join() {
  if (!IsRunning())
    return;

  RTC_DCHECK(!IsCurrent());
  if (Current() && !Current()->blocking_calls_allowed_) {
    RTC_LOG(LS_WARNING) << "Waiting for the thread to join, "
                           "but blocking calls have been disallowed";
  }

#if defined(WEBRTC_WIN)
  RTC_DCHECK(thread_ != nullptr);
  WaitForSingleObject(thread_, INFINITE);
  CloseHandle(thread_);
  thread_ = nullptr;
  thread_id_ = 0;
#elif defined(WEBRTC_POSIX)
  pthread_join(thread_, nullptr);
  thread_ = 0;
#endif
}

bool Thread::SetAllowBlockingCalls(bool allow) {
  RTC_DCHECK(IsCurrent());
  bool previous = blocking_calls_allowed_;
  blocking_calls_allowed_ = allow;
  return previous;
}

void Thread::AssertBlockingIsAllowedOnCurrentThread() {
#if !defined(NDEBUG)
  Thread* current = Thread::Current();
  RTC_DCHECK(!current || current->blocking_calls_allowed_);
#endif
}

#if defined(WEBRTC_WIN)
DWORD WINAPI Thread::PreRun(LPVOID pv) {
#else
void* Thread::PreRun(void* pv) {
#endif
  Thread* thread = static_cast<Thread*>(pv);
  ThreadManager::Instance()->SetCurrentThread(thread);
  rtc::SetCurrentThreadName(thread->name_.c_str());
#if defined(WEBRTC_MAC)
  ScopedAutoReleasePool pool;
#endif
  thread->Run();

  ThreadManager::Instance()->SetCurrentThread(nullptr);
#ifdef WEBRTC_WIN
  return 0;
#else
  return nullptr;
#endif
}

void Thread::Run() {
  ProcessMessages(kForever);
}

bool Thread::IsOwned() {
  RTC_DCHECK(IsRunning());
  return owned_;
}

void Thread::Stop() {
  Thread::Quit();
  Join();
}

void Thread::BlockingCallImpl(rtc::FunctionView<void()> functor,
                              const webrtc::Location& location) {
  TRACE_EVENT0("webrtc", "Thread::BlockingCall");

  RTC_DCHECK(!IsQuitting());
  if (IsQuitting())
    return;

  if (IsCurrent()) {
#if RTC_DCHECK_IS_ON
    RTC_DCHECK(this->IsInvokeToThreadAllowed(this));
    RTC_DCHECK_RUN_ON(this);
    could_be_blocking_call_count_++;
#endif
    functor();
    return;
  }

#if RTC_DCHECK_IS_ON
  if (Thread* current_thread = Thread::Current()) {
    RTC_DCHECK_RUN_ON(current_thread);
    RTC_DCHECK(current_thread->blocking_calls_allowed_);
    current_thread->blocking_call_count_++;
    RTC_DCHECK(current_thread->IsInvokeToThreadAllowed(this));
    ThreadManager::Instance()->RegisterSendAndCheckForCycles(current_thread,
                                                             this);
  }
#endif

  Event done;
  absl::Cleanup cleanup = [&done] { done.Set(); };
  PostTask([functor, cleanup = std::move(cleanup)] { functor(); });
  done.Wait(Event::kForever);
}

void Thread::EnsureIsCurrentTaskQueue() {
  task_queue_registration_ =
      std::make_unique<TaskQueueBase::CurrentTaskQueueSetter>(this);
}

void Thread::ClearCurrentTaskQueue() {
  task_queue_registration_.reset();
}

void Thread::AllowInvokesToThread(Thread* thread) {
#if (!defined(NDEBUG) || RTC_DCHECK_IS_ON)
  if (!IsCurrent()) {
    PostTask([thread, this]() { AllowInvokesToThread(thread); });
    return;
  }
  RTC_DCHECK_RUN_ON(this);
  allowed_threads_.push_back(thread);
  invoke_policy_enabled_ = true;
#endif
}

void Thread::DisallowAllInvokes() {
#if (!defined(NDEBUG) || RTC_DCHECK_IS_ON)
  if (!IsCurrent()) {
    PostTask([this]() { DisallowAllInvokes(); });
    return;
  }
  RTC_DCHECK_RUN_ON(this);
  allowed_threads_.clear();
  invoke_policy_enabled_ = true;
#endif
}

#if RTC_DCHECK_IS_ON
uint32_t Thread::GetBlockingCallCount() const {
  RTC_DCHECK_RUN_ON(this);
  return blocking_call_count_;
}
uint32_t Thread::GetCouldBeBlockingCallCount() const {
  RTC_DCHECK_RUN_ON(this);
  return could_be_blocking_call_count_;
}
#endif

bool Thread::IsInvokeToThreadAllowed(rtc::Thread* target) {
#if (!defined(NDEBUG) || RTC_DCHECK_IS_ON)
  RTC_DCHECK_RUN_ON(this);
  if (!invoke_policy_enabled_) {
    return true;
  }
  for (const auto* thread : allowed_threads_) {
    if (thread == target) {
      return true;
    }
  }
  return false;
#else
  return true;
#endif
}

void Thread::Delete() {
  Stop();
  delete this;
}

bool Thread::IsProcessingMessagesForTesting() {
  return (owned_ || IsCurrent()) && !IsQuitting();
}

bool Thread::ProcessMessages(int cmsLoop) {

  RTC_DCHECK(GetClockForTesting() == nullptr || cmsLoop == 0 ||
             cmsLoop == kForever);
  int64_t msEnd = (kForever == cmsLoop) ? 0 : TimeAfter(cmsLoop);
  int cmsNext = cmsLoop;

  while (true) {
#if defined(WEBRTC_MAC)
    ScopedAutoReleasePool pool;
#endif
    absl::AnyInvocable<void()&&> task = Get(cmsNext);
    if (!task)
      return !IsQuitting();
    Dispatch(std::move(task));

    if (cmsLoop != kForever) {
      cmsNext = static_cast<int>(TimeUntil(msEnd));
      if (cmsNext < 0)
        return true;
    }
  }
}

bool Thread::WrapCurrentWithThreadManager(ThreadManager* thread_manager,
                                          bool need_synchronize_access) {
  RTC_DCHECK(!IsRunning());

#if defined(WEBRTC_WIN)
  if (need_synchronize_access) {

    thread_ = OpenThread(SYNCHRONIZE, FALSE, GetCurrentThreadId());
    if (!thread_) {
      RTC_LOG_GLE(LS_ERROR) << "Unable to get handle to thread.";
      return false;
    }
    thread_id_ = GetCurrentThreadId();
  }
#elif defined(WEBRTC_POSIX)
  thread_ = pthread_self();
#endif
  owned_ = false;
  thread_manager->SetCurrentThread(this);
  return true;
}

bool Thread::IsRunning() {
#if defined(WEBRTC_WIN)
  return thread_ != nullptr;
#elif defined(WEBRTC_POSIX)
  return thread_ != 0;
#endif
}

AutoThread::AutoThread()
    : Thread(CreateDefaultSocketServer(),             false) {
  if (!ThreadManager::Instance()->CurrentThread()) {

    DoInit();
    ThreadManager::Instance()->SetCurrentThread(this);
  }
}

AutoThread::~AutoThread() {
  Stop();
  DoDestroy();
  if (ThreadManager::Instance()->CurrentThread() == this) {
    ThreadManager::Instance()->SetCurrentThread(nullptr);
  }
}

AutoSocketServerThread::AutoSocketServerThread(SocketServer* ss)
    : Thread(ss,             false) {
  DoInit();
  old_thread_ = ThreadManager::Instance()->CurrentThread();

  rtc::ThreadManager::Instance()->SetCurrentThread(nullptr);
  rtc::ThreadManager::Instance()->SetCurrentThread(this);
  if (old_thread_) {
    ThreadManager::Remove(old_thread_);
  }
}

AutoSocketServerThread::~AutoSocketServerThread() {
  RTC_DCHECK(ThreadManager::Instance()->CurrentThread() == this);

  Stop();
  DoDestroy();
  rtc::ThreadManager::Instance()->SetCurrentThread(nullptr);
  rtc::ThreadManager::Instance()->SetCurrentThread(old_thread_);
  if (old_thread_) {
    ThreadManager::Add(old_thread_);
  }
}

}
