/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef P2P_BASE_PSEUDO_TCP_H_
#define P2P_BASE_PSEUDO_TCP_H_

#include <stddef.h>
#include <stdint.h>

#include <list>
#include <memory>

#include "rtc_base/synchronization/mutex.h"
#include "rtc_base/system/rtc_export.h"

namespace cricket {

class PseudoTcp;

class IPseudoTcpNotify {
 public:

  virtual void OnTcpOpen(PseudoTcp* tcp) = 0;
  virtual void OnTcpReadable(PseudoTcp* tcp) = 0;
  virtual void OnTcpWriteable(PseudoTcp* tcp) = 0;
  virtual void OnTcpClosed(PseudoTcp* tcp, uint32_t error) = 0;

  enum WriteResult { WR_SUCCESS, WR_TOO_LARGE, WR_FAIL };
  virtual WriteResult TcpWritePacket(PseudoTcp* tcp,
                                     const char* buffer,
                                     size_t len) = 0;

 protected:
  virtual ~IPseudoTcpNotify() {}
};

class RTC_EXPORT PseudoTcp {
 public:
  static uint32_t Now();

  PseudoTcp(IPseudoTcpNotify* notify, uint32_t conv);
  virtual ~PseudoTcp();

  int Connect();
  int Recv(char* buffer, size_t len);
  int Send(const char* buffer, size_t len);
  void Close(bool force);
  int GetError();

  enum TcpState {
    TCP_LISTEN,
    TCP_SYN_SENT,
    TCP_SYN_RECEIVED,
    TCP_ESTABLISHED,
    TCP_CLOSED
  };
  TcpState State() const { return m_state; }

  void NotifyMTU(uint16_t mtu);

  void NotifyClock(uint32_t now);

  bool NotifyPacket(const char* buffer, size_t len);

  bool GetNextClock(uint32_t now, long& timeout);

  enum Option {
    OPT_NODELAY,
    OPT_ACKDELAY,
    OPT_RCVBUF,
    OPT_SNDBUF,
  };
  void GetOption(Option opt, int* value);
  void SetOption(Option opt, int value);

  uint32_t GetCongestionWindow() const;

  uint32_t GetBytesInFlight() const;

  uint32_t GetBytesBufferedNotSent() const;

  uint32_t GetRoundTripTimeEstimateMs() const;

 protected:
  enum SendFlags { sfNone, sfDelayedAck, sfImmediateAck };

  struct Segment {
    uint32_t conv, seq, ack;
    uint8_t flags;
    uint16_t wnd;
    const char* data;
    uint32_t len;
    uint32_t tsval, tsecr;
  };

  struct SSegment {
    SSegment(uint32_t s, uint32_t l, bool c)
        : seq(s), len(l),                xmit(0), bCtrl(c) {}
    uint32_t seq, len;

    uint8_t xmit;
    bool bCtrl;
  };
  typedef std::list<SSegment> SList;

  struct RSegment {
    uint32_t seq, len;
  };

  uint32_t queue(const char* data, uint32_t len, bool bCtrl);

  IPseudoTcpNotify::WriteResult packet(uint32_t seq,
                                       uint8_t flags,
                                       uint32_t offset,
                                       uint32_t len);
  bool parse(const uint8_t* buffer, uint32_t size);

  void attemptSend(SendFlags sflags = sfNone);

  void closedown(uint32_t err = 0);

  bool clock_check(uint32_t now, long& nTimeout);

  bool process(Segment& seg);
  bool transmit(const SList::iterator& seg, uint32_t now);

  void adjustMTU();

 protected:

  bool isReceiveBufferFull() const;

  void disableWindowScale();

 private:

  void queueConnectMessage();

  void parseOptions(const char* data, uint32_t len);

  void applyOption(char kind, const char* data, uint32_t len);

  void applyWindowScaleOption(uint8_t scale_factor);

  void resizeSendBuffer(uint32_t new_size);

  void resizeReceiveBuffer(uint32_t new_size);

  class LockedFifoBuffer final {
   public:
    explicit LockedFifoBuffer(size_t size);
    ~LockedFifoBuffer();

    size_t GetBuffered() const;
    bool SetCapacity(size_t size);
    bool ReadOffset(void* buffer,
                    size_t bytes,
                    size_t offset,
                    size_t* bytes_read);
    bool WriteOffset(const void* buffer,
                     size_t bytes,
                     size_t offset,
                     size_t* bytes_written);
    bool Read(void* buffer, size_t bytes, size_t* bytes_read);
    bool Write(const void* buffer, size_t bytes, size_t* bytes_written);
    void ConsumeReadData(size_t size);
    void ConsumeWriteBuffer(size_t size);
    bool GetWriteRemaining(size_t* size) const;

   private:
    bool ReadOffsetLocked(void* buffer,
                          size_t bytes,
                          size_t offset,
                          size_t* bytes_read)
        RTC_EXCLUSIVE_LOCKS_REQUIRED(mutex_);
    bool WriteOffsetLocked(const void* buffer,
                           size_t bytes,
                           size_t offset,
                           size_t* bytes_written)
        RTC_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

    std::unique_ptr<char[]> buffer_ RTC_GUARDED_BY(mutex_);

    size_t buffer_length_ RTC_GUARDED_BY(mutex_);

    size_t data_length_ RTC_GUARDED_BY(mutex_);

    size_t read_position_ RTC_GUARDED_BY(mutex_);
    mutable webrtc::Mutex mutex_;
  };

  IPseudoTcpNotify* m_notify;
  enum Shutdown { SD_NONE, SD_GRACEFUL, SD_FORCEFUL } m_shutdown;
  int m_error;

  TcpState m_state;
  uint32_t m_conv;
  bool m_bReadEnable, m_bWriteEnable, m_bOutgoing;
  uint32_t m_lasttraffic;

  typedef std::list<RSegment> RList;
  RList m_rlist;
  uint32_t m_rbuf_len, m_rcv_nxt, m_rcv_wnd, m_lastrecv;
  uint8_t m_rwnd_scale;
  LockedFifoBuffer m_rbuf;

  SList m_slist;
  uint32_t m_sbuf_len, m_snd_nxt, m_snd_wnd, m_lastsend, m_snd_una;
  uint8_t m_swnd_scale;
  LockedFifoBuffer m_sbuf;

  uint32_t m_mss, m_msslevel, m_largest, m_mtu_advise;

  uint32_t m_rto_base;

  uint32_t m_ts_recent, m_ts_lastack;

  uint32_t m_rx_rttvar, m_rx_srtt, m_rx_rto;

  uint32_t m_ssthresh, m_cwnd;
  uint8_t m_dup_acks;
  uint32_t m_recover;
  uint32_t m_t_ack;

  bool m_use_nagling;
  uint32_t m_ack_delay;

  bool m_support_wnd_scale;
};

}

#endif
