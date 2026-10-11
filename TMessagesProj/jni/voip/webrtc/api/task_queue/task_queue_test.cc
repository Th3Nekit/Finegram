/*
 *  Copyright 2019 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "api/task_queue/task_queue_test.h"

#include <memory>

#include "absl/cleanup/cleanup.h"
#include "absl/strings/string_view.h"
#include "api/task_queue/task_queue_base.h"
#include "api/units/time_delta.h"
#include "rtc_base/event.h"
#include "rtc_base/ref_counter.h"
#include "rtc_base/time_utils.h"

namespace webrtc {
namespace {

void SleepFor(TimeDelta duration) {
  rtc::ScopedAllowBaseSyncPrimitivesForTesting allow;
  rtc::Event event;
  event.Wait(duration);
}

std::unique_ptr<TaskQueueBase, TaskQueueDeleter> CreateTaskQueue(
    const std::unique_ptr<webrtc::TaskQueueFactory>& factory,
    absl::string_view task_queue_name,
    TaskQueueFactory::Priority priority = TaskQueueFactory::Priority::NORMAL) {
  return factory->CreateTaskQueue(task_queue_name, priority);
}

TEST_P(TaskQueueTest, Construct) {
  std::unique_ptr<webrtc::TaskQueueFactory> factory = GetParam()(nullptr);
  auto queue = CreateTaskQueue(factory, "Construct");
  EXPECT_FALSE(queue->IsCurrent());
}

TEST_P(TaskQueueTest, PostAndCheckCurrent) {
  std::unique_ptr<webrtc::TaskQueueFactory> factory = GetParam()(nullptr);
  rtc::Event event;
  auto queue = CreateTaskQueue(factory, "PostAndCheckCurrent");

  EXPECT_FALSE(queue->IsCurrent());

  queue->PostTask([&event, &queue] {
    EXPECT_TRUE(queue->IsCurrent());
    event.Set();
  });
  EXPECT_TRUE(event.Wait(TimeDelta::Seconds(1)));
}

TEST_P(TaskQueueTest, PostCustomTask) {
  std::unique_ptr<webrtc::TaskQueueFactory> factory = GetParam()(nullptr);
  rtc::Event ran;
  auto queue = CreateTaskQueue(factory, "PostCustomImplementation");

  class CustomTask {
   public:
    explicit CustomTask(rtc::Event* ran) : ran_(ran) {}

    void operator()() { ran_->Set(); }

   private:
    rtc::Event* const ran_;
  } my_task(&ran);

  queue->PostTask(my_task);
  EXPECT_TRUE(ran.Wait(TimeDelta::Seconds(1)));
}

TEST_P(TaskQueueTest, PostDelayedZero) {
  std::unique_ptr<webrtc::TaskQueueFactory> factory = GetParam()(nullptr);
  rtc::Event event;
  auto queue = CreateTaskQueue(factory, "PostDelayedZero");

  queue->PostDelayedTask([&event] { event.Set(); }, TimeDelta::Zero());
  EXPECT_TRUE(event.Wait(TimeDelta::Seconds(1)));
}

TEST_P(TaskQueueTest, PostFromQueue) {
  std::unique_ptr<webrtc::TaskQueueFactory> factory = GetParam()(nullptr);
  rtc::Event event;
  auto queue = CreateTaskQueue(factory, "PostFromQueue");

  queue->PostTask(
      [&event, &queue] { queue->PostTask([&event] { event.Set(); }); });
  EXPECT_TRUE(event.Wait(TimeDelta::Seconds(1)));
}

TEST_P(TaskQueueTest, PostDelayed) {
  std::unique_ptr<webrtc::TaskQueueFactory> factory = GetParam()(nullptr);
  rtc::Event event;
  auto queue =
      CreateTaskQueue(factory, "PostDelayed", TaskQueueFactory::Priority::HIGH);

  int64_t start = rtc::TimeMillis();
  queue->PostDelayedTask(
      [&event, &queue] {
        EXPECT_TRUE(queue->IsCurrent());
        event.Set();
      },
      TimeDelta::Millis(100));
  EXPECT_TRUE(event.Wait(TimeDelta::Seconds(1)));
  int64_t end = rtc::TimeMillis();

  EXPECT_GE(end - start, 90u);
  EXPECT_NEAR(end - start, 190u, 100u);
}

TEST_P(TaskQueueTest, PostMultipleDelayed) {
  std::unique_ptr<webrtc::TaskQueueFactory> factory = GetParam()(nullptr);
  auto queue = CreateTaskQueue(factory, "PostMultipleDelayed");

  std::vector<rtc::Event> events(100);
  for (int i = 0; i < 100; ++i) {
    rtc::Event* event = &events[i];
    queue->PostDelayedTask(
        [event, &queue] {
          EXPECT_TRUE(queue->IsCurrent());
          event->Set();
        },
        TimeDelta::Millis(i));
  }

  for (rtc::Event& e : events)
    EXPECT_TRUE(e.Wait(TimeDelta::Seconds(1)));
}

TEST_P(TaskQueueTest, PostDelayedAfterDestruct) {
  std::unique_ptr<webrtc::TaskQueueFactory> factory = GetParam()(nullptr);
  rtc::Event run;
  rtc::Event deleted;
  auto queue = CreateTaskQueue(factory, "PostDelayedAfterDestruct");
  absl::Cleanup cleanup = [&deleted] { deleted.Set(); };
  queue->PostDelayedTask([&run, cleanup = std::move(cleanup)] { run.Set(); },
                         TimeDelta::Millis(100));

  queue = nullptr;

  EXPECT_TRUE(deleted.Wait(TimeDelta::Seconds(1)));
  EXPECT_FALSE(run.Wait(TimeDelta::Zero()));
}

TEST_P(TaskQueueTest, PostDelayedHighPrecisionAfterDestruct) {
  std::unique_ptr<webrtc::TaskQueueFactory> factory = GetParam()(nullptr);
  rtc::Event run;
  rtc::Event deleted;
  auto queue =
      CreateTaskQueue(factory, "PostDelayedHighPrecisionAfterDestruct");
  absl::Cleanup cleanup = [&deleted] { deleted.Set(); };
  queue->PostDelayedHighPrecisionTask(
      [&run, cleanup = std::move(cleanup)] { run.Set(); },
      TimeDelta::Millis(100));

  queue = nullptr;

  EXPECT_TRUE(deleted.Wait(TimeDelta::Seconds(1)));
  EXPECT_FALSE(run.Wait(TimeDelta::Zero()));
}

TEST_P(TaskQueueTest, PostedUnexecutedClosureDestroyedOnTaskQueue) {
  std::unique_ptr<webrtc::TaskQueueFactory> factory = GetParam()(nullptr);
  auto queue =
      CreateTaskQueue(factory, "PostedUnexecutedClosureDestroyedOnTaskQueue");
  TaskQueueBase* queue_ptr = queue.get();
  queue->PostTask([] { SleepFor(TimeDelta::Millis(100)); });

  SleepFor(TimeDelta::Millis(10));
  rtc::Event finished;

  auto cleanup = absl::Cleanup([queue_ptr, &finished] {
    EXPECT_EQ(queue_ptr, TaskQueueBase::Current());
    finished.Set();
  });
  queue->PostTask([cleanup = std::move(cleanup)] {});
  queue = nullptr;
  finished.Wait(TimeDelta::Seconds(1));
}

TEST_P(TaskQueueTest, PostedClosureDestroyedOnTaskQueue) {
  std::unique_ptr<webrtc::TaskQueueFactory> factory = GetParam()(nullptr);
  auto queue = CreateTaskQueue(factory, "PostedClosureDestroyedOnTaskQueue");
  TaskQueueBase* queue_ptr = queue.get();
  rtc::Event finished;
  auto cleanup = absl::Cleanup([queue_ptr, &finished] {
    EXPECT_EQ(queue_ptr, TaskQueueBase::Current());
    finished.Set();
  });

  queue->PostTask([cleanup = std::move(cleanup)] {});
  queue = nullptr;
  finished.Wait(TimeDelta::Seconds(1));
}

TEST_P(TaskQueueTest, PostedExecutedClosureDestroyedOnTaskQueue) {
  std::unique_ptr<webrtc::TaskQueueFactory> factory = GetParam()(nullptr);
  auto queue =
      CreateTaskQueue(factory, "PostedExecutedClosureDestroyedOnTaskQueue");
  TaskQueueBase* queue_ptr = queue.get();

  rtc::Event finished;
  queue->PostTask([cleanup = absl::Cleanup([queue_ptr, &finished] {
                     EXPECT_EQ(queue_ptr, TaskQueueBase::Current());
                     finished.Set();
                   })] {});
  finished.Wait(TimeDelta::Seconds(1));
}

TEST_P(TaskQueueTest, PostAndReuse) {
  std::unique_ptr<webrtc::TaskQueueFactory> factory = GetParam()(nullptr);
  rtc::Event event;
  auto post_queue = CreateTaskQueue(factory, "PostQueue");
  auto reply_queue = CreateTaskQueue(factory, "ReplyQueue");

  int call_count = 0;

  class ReusedTask {
   public:
    ReusedTask(int* counter, TaskQueueBase* reply_queue, rtc::Event* event)
        : counter_(*counter), reply_queue_(reply_queue), event_(*event) {
      EXPECT_EQ(counter_, 0);
    }
    ReusedTask(ReusedTask&&) = default;
    ReusedTask& operator=(ReusedTask&&) = delete;

    void operator()() && {
      if (++counter_ == 1) {
        reply_queue_->PostTask(std::move(*this));

      } else {
        EXPECT_EQ(counter_, 2);
        EXPECT_TRUE(reply_queue_->IsCurrent());
        event_.Set();
      }
    }

   private:
    int& counter_;
    TaskQueueBase* const reply_queue_;
    rtc::Event& event_;
  };

  ReusedTask task(&call_count, reply_queue.get(), &event);
  post_queue->PostTask(std::move(task));
  EXPECT_TRUE(event.Wait(TimeDelta::Seconds(1)));
}

TEST_P(TaskQueueTest, PostALot) {

  class BlockingCounter {
   public:
    explicit BlockingCounter(int initial_count) : count_(initial_count) {}

    void DecrementCount() {
      if (count_.DecRef() == webrtc::RefCountReleaseStatus::kDroppedLastRef) {
        event_.Set();
      }
    }
    bool Wait(TimeDelta give_up_after) { return event_.Wait(give_up_after); }

   private:
    webrtc_impl::RefCounter count_;
    rtc::Event event_;
  };

  std::unique_ptr<webrtc::TaskQueueFactory> factory = GetParam()(nullptr);
  static constexpr int kTaskCount = 0xffff;
  rtc::Event posting_done;
  BlockingCounter all_destroyed(kTaskCount);

  int tasks_executed = 0;
  auto task_queue = CreateTaskQueue(factory, "PostALot");

  task_queue->PostTask([&] {

    for (int i = 0; i < kTaskCount; ++i) {
      absl::Cleanup cleanup = [&] { all_destroyed.DecrementCount(); };
      task_queue->PostTask([&tasks_executed, cleanup = std::move(cleanup)] {
        ++tasks_executed;
      });
    }

    posting_done.Set();
  });

  posting_done.Wait(rtc::Event::kForever);

  task_queue = nullptr;

  EXPECT_TRUE(all_destroyed.Wait(TimeDelta::Minutes(1)));
  EXPECT_LE(tasks_executed, kTaskCount);
}

TEST_P(TaskQueueTest, PostTwoWithSharedUnprotectedState) {
  std::unique_ptr<webrtc::TaskQueueFactory> factory = GetParam()(nullptr);
  struct SharedState {

    int state = 0;
  } state;

  auto queue = CreateTaskQueue(factory, "PostTwoWithSharedUnprotectedState");
  rtc::Event done;
  queue->PostTask([&state, &queue, &done] {

    queue->PostTask([&state] { state.state = 1; });
    queue->PostTask([&state, &done] {
      EXPECT_EQ(state.state, 1);
      done.Set();
    });

    EXPECT_EQ(state.state, 0);
  });
  EXPECT_TRUE(done.Wait(TimeDelta::Seconds(1)));
}

GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(TaskQueueTest);

}
}
