/*
 *  Copyright 2012 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/third_party/sigslot/sigslot.h"

#include "test/gtest.h"

static bool TemplateIsST(const sigslot::single_threaded* p) {
  return true;
}

static bool TemplateIsMT(const sigslot::multi_threaded_local* p) {
  return true;
}

class SigslotDefault : public ::testing::Test, public sigslot::has_slots<> {
 protected:
  sigslot::signal0<> signal_;
};

template <class slot_policy = sigslot::single_threaded,
          class signal_policy = sigslot::single_threaded>
class SigslotReceiver : public sigslot::has_slots<slot_policy> {
 public:
  SigslotReceiver() : signal_(nullptr), signal_count_(0) {}
  ~SigslotReceiver() {}

  SigslotReceiver(const SigslotReceiver&) = default;

  void Connect(sigslot::signal0<signal_policy>* signal) {
    if (!signal)
      return;
    Disconnect();
    signal_ = signal;
    signal->connect(this,
                    &SigslotReceiver<slot_policy, signal_policy>::OnSignal);
  }
  void Disconnect() {
    if (!signal_)
      return;
    signal_->disconnect(this);
    signal_ = nullptr;
  }
  void OnSignal() { ++signal_count_; }
  int signal_count() { return signal_count_; }

 private:
  sigslot::signal0<signal_policy>* signal_;
  int signal_count_;
};

template <class slot_policy = sigslot::single_threaded,
          class mt_signal_policy = sigslot::multi_threaded_local>
class SigslotSlotTest : public ::testing::Test {
 protected:
  SigslotSlotTest() {
    mt_signal_policy mt_policy;
    TemplateIsMT(&mt_policy);
  }

  virtual void SetUp() { Connect(); }
  virtual void TearDown() { Disconnect(); }

  void Disconnect() {
    st_receiver_.Disconnect();
    mt_receiver_.Disconnect();
  }

  void Connect() {
    st_receiver_.Connect(&SignalSTLoopback);
    mt_receiver_.Connect(&SignalMTLoopback);
  }

  int st_loop_back_count() { return st_receiver_.signal_count(); }
  int mt_loop_back_count() { return mt_receiver_.signal_count(); }

  sigslot::signal0<> SignalSTLoopback;
  SigslotReceiver<slot_policy, sigslot::single_threaded> st_receiver_;
  sigslot::signal0<mt_signal_policy> SignalMTLoopback;
  SigslotReceiver<slot_policy, mt_signal_policy> mt_receiver_;
};

typedef SigslotSlotTest<> SigslotSTSlotTest;
typedef SigslotSlotTest<sigslot::multi_threaded_local,
                        sigslot::multi_threaded_local>
    SigslotMTSlotTest;

class multi_threaded_local_fake : public sigslot::multi_threaded_local {
 public:
  multi_threaded_local_fake() : lock_count_(0), unlock_count_(0) {}

  void lock() { ++lock_count_; }
  void unlock() { ++unlock_count_; }

  int lock_count() { return lock_count_; }

  bool InCriticalSection() { return lock_count_ != unlock_count_; }

 protected:
  int lock_count_;
  int unlock_count_;
};

typedef SigslotSlotTest<multi_threaded_local_fake, multi_threaded_local_fake>
    SigslotMTLockBase;

class SigslotMTLockTest : public SigslotMTLockBase {
 protected:
  SigslotMTLockTest() {}

  void SetUp() override {
    EXPECT_EQ(0, SlotLockCount());
    SigslotMTLockBase::SetUp();

    EXPECT_EQ(1, SlotLockCount());
  }
  void TearDown() override {
    const int previous_lock_count = SlotLockCount();
    SigslotMTLockBase::TearDown();

    EXPECT_EQ(previous_lock_count + 1, SlotLockCount());
  }

  int SlotLockCount() { return mt_receiver_.lock_count(); }
  void Signal() { SignalMTLoopback(); }
  int SignalLockCount() { return SignalMTLoopback.lock_count(); }
  int signal_count() { return mt_loop_back_count(); }
  bool InCriticalSection() { return SignalMTLoopback.InCriticalSection(); }
};

TEST_F(SigslotDefault, DefaultIsST) {
  EXPECT_TRUE(TemplateIsST(this));
  EXPECT_TRUE(TemplateIsST(&signal_));
}

TEST_F(SigslotSTSlotTest, STLoopbackTest) {
  SignalSTLoopback();
  EXPECT_EQ(1, st_loop_back_count());
  EXPECT_EQ(0, mt_loop_back_count());
}

TEST_F(SigslotSTSlotTest, MTLoopbackTest) {
  SignalMTLoopback();
  EXPECT_EQ(1, mt_loop_back_count());
  EXPECT_EQ(0, st_loop_back_count());
}

TEST_F(SigslotSTSlotTest, AllLoopbackTest) {
  SignalSTLoopback();
  SignalMTLoopback();
  EXPECT_EQ(1, mt_loop_back_count());
  EXPECT_EQ(1, st_loop_back_count());
}

TEST_F(SigslotSTSlotTest, Reconnect) {
  SignalSTLoopback();
  SignalMTLoopback();
  EXPECT_EQ(1, mt_loop_back_count());
  EXPECT_EQ(1, st_loop_back_count());
  Disconnect();
  SignalSTLoopback();
  SignalMTLoopback();
  EXPECT_EQ(1, mt_loop_back_count());
  EXPECT_EQ(1, st_loop_back_count());
  Connect();
  SignalSTLoopback();
  SignalMTLoopback();
  EXPECT_EQ(2, mt_loop_back_count());
  EXPECT_EQ(2, st_loop_back_count());
}

TEST_F(SigslotMTSlotTest, STLoopbackTest) {
  SignalSTLoopback();
  EXPECT_EQ(1, st_loop_back_count());
  EXPECT_EQ(0, mt_loop_back_count());
}

TEST_F(SigslotMTSlotTest, MTLoopbackTest) {
  SignalMTLoopback();
  EXPECT_EQ(1, mt_loop_back_count());
  EXPECT_EQ(0, st_loop_back_count());
}

TEST_F(SigslotMTSlotTest, AllLoopbackTest) {
  SignalMTLoopback();
  SignalSTLoopback();
  EXPECT_EQ(1, st_loop_back_count());
  EXPECT_EQ(1, mt_loop_back_count());
}

TEST_F(SigslotMTLockTest, LockSanity) {
  const int lock_count = SignalLockCount();
  Signal();
  EXPECT_FALSE(InCriticalSection());
  EXPECT_EQ(lock_count + 1, SignalLockCount());
  EXPECT_EQ(1, signal_count());
}

TEST(SigslotDestructionOrder, SignalFirst) {
  sigslot::signal0<>* signal = new sigslot::signal0<>;
  SigslotReceiver<>* receiver = new SigslotReceiver<>();
  receiver->Connect(signal);
  (*signal)();
  EXPECT_EQ(1, receiver->signal_count());
  delete signal;
  delete receiver;
}

TEST(SigslotDestructionOrder, SlotFirst) {
  sigslot::signal0<>* signal = new sigslot::signal0<>;
  SigslotReceiver<>* receiver = new SigslotReceiver<>();
  receiver->Connect(signal);
  (*signal)();
  EXPECT_EQ(1, receiver->signal_count());

  delete receiver;
  (*signal)();
  delete signal;
}

TEST(SigslotTest, CopyConnectedSignal) {
  sigslot::signal<> signal;
  SigslotReceiver<> receiver;
  receiver.Connect(&signal);

  sigslot::signal<> copied_signal(signal);
  copied_signal();
  EXPECT_EQ(1, receiver.signal_count());
}

TEST(SigslotTest, CopyConnectedSlot) {
  sigslot::signal<> signal;
  SigslotReceiver<> receiver;
  receiver.Connect(&signal);

  SigslotReceiver<> copied_receiver(receiver);
  signal();
  EXPECT_EQ(1, copied_receiver.signal_count());
}

class Disconnector : public sigslot::has_slots<> {
 public:
  Disconnector(SigslotReceiver<>* receiver1, SigslotReceiver<>* receiver2)
      : receiver1_(receiver1), receiver2_(receiver2) {}

  void Connect(sigslot::signal<>* signal) {
    signal_ = signal;
    signal->connect(this, &Disconnector::Disconnect);
  }

 private:
  void Disconnect() {
    receiver1_->Disconnect();
    receiver2_->Disconnect();
    signal_->disconnect(this);
  }

  sigslot::signal<>* signal_;
  SigslotReceiver<>* receiver1_;
  SigslotReceiver<>* receiver2_;
};

TEST(SigslotTest, DisconnectFromSignalWhileFiring) {
  sigslot::signal<> signal;
  SigslotReceiver<> receiver1;
  SigslotReceiver<> receiver2;
  SigslotReceiver<> receiver3;
  Disconnector disconnector(&receiver1, &receiver2);

  receiver1.Connect(&signal);
  disconnector.Connect(&signal);
  receiver2.Connect(&signal);
  receiver3.Connect(&signal);
  signal();

  EXPECT_EQ(1, receiver1.signal_count());
  EXPECT_EQ(0, receiver2.signal_count());
  EXPECT_EQ(1, receiver3.signal_count());
}

class Disconnector2 : public sigslot::has_slots<> {
 public:
  void Connect(sigslot::signal<>* signal) {
    signal_ = signal;
    signal->connect(this, &Disconnector2::Disconnect);
  }

 private:
  void Disconnect() { signal_->disconnect_all(); }

  sigslot::signal<>* signal_;
};

TEST(SigslotTest, CallDisconnectAllWhileSignalFiring) {
  sigslot::signal<> signal;
  SigslotReceiver<> receiver1;
  SigslotReceiver<> receiver2;
  Disconnector2 disconnector;

  receiver1.Connect(&signal);
  disconnector.Connect(&signal);
  receiver2.Connect(&signal);
  signal();

  EXPECT_EQ(1, receiver1.signal_count());
  EXPECT_EQ(0, receiver2.signal_count());
}
