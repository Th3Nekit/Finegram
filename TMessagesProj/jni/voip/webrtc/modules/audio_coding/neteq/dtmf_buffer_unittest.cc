/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_coding/neteq/dtmf_buffer.h"

#ifdef WIN32
#include <winsock2.h>
#else
#include <arpa/inet.h>
#endif

#include <iostream>

#include "test/gtest.h"

#define LEGACY_BITEXACT

namespace webrtc {

static int sample_rate_hz = 8000;

static uint32_t MakeDtmfPayload(int event, bool end, int volume, int duration) {
  uint32_t payload = 0;

  payload |= (event & 0x00FF) << 24;
  payload |= (end ? 0x00800000 : 0x00000000);
  payload |= (volume & 0x003F) << 16;
  payload |= (duration & 0xFFFF);
  payload = ntohl(payload);
  return payload;
}

static bool EqualEvents(const DtmfEvent& a, const DtmfEvent& b) {
  return (a.duration == b.duration && a.end_bit == b.end_bit &&
          a.event_no == b.event_no && a.timestamp == b.timestamp &&
          a.volume == b.volume);
}

TEST(DtmfBuffer, CreateAndDestroy) {
  DtmfBuffer* buffer = new DtmfBuffer(sample_rate_hz);
  delete buffer;
}

TEST(DtmfBuffer, ParseEvent) {
  int event_no = 7;
  bool end_bit = true;
  int volume = 17;
  int duration = 4711;
  uint32_t timestamp = 0x12345678;
  uint32_t payload = MakeDtmfPayload(event_no, end_bit, volume, duration);
  uint8_t* payload_ptr = reinterpret_cast<uint8_t*>(&payload);
  DtmfEvent event;
  EXPECT_EQ(DtmfBuffer::kOK, DtmfBuffer::ParseEvent(timestamp, payload_ptr,
                                                    sizeof(payload), &event));
  EXPECT_EQ(duration, event.duration);
  EXPECT_EQ(end_bit, event.end_bit);
  EXPECT_EQ(event_no, event.event_no);
  EXPECT_EQ(timestamp, event.timestamp);
  EXPECT_EQ(volume, event.volume);

  EXPECT_EQ(DtmfBuffer::kPayloadTooShort,
            DtmfBuffer::ParseEvent(timestamp, payload_ptr, 3, &event));
}

TEST(DtmfBuffer, SimpleInsertAndGet) {
  int event_no = 7;
  bool end_bit = true;
  int volume = 17;
  int duration = 4711;
  uint32_t timestamp = 0x12345678;
  DtmfEvent event(timestamp, event_no, volume, duration, end_bit);
  DtmfBuffer buffer(sample_rate_hz);
  EXPECT_EQ(DtmfBuffer::kOK, buffer.InsertEvent(event));
  EXPECT_EQ(1u, buffer.Length());
  EXPECT_FALSE(buffer.Empty());
  DtmfEvent out_event;

  EXPECT_FALSE(buffer.GetEvent(timestamp - 10, &out_event));
  EXPECT_EQ(1u, buffer.Length());
  EXPECT_FALSE(buffer.Empty());

  EXPECT_TRUE(buffer.GetEvent(timestamp, &out_event));
  EXPECT_TRUE(EqualEvents(event, out_event));
  EXPECT_EQ(1u, buffer.Length());
  EXPECT_FALSE(buffer.Empty());

  EXPECT_TRUE(buffer.GetEvent(timestamp + duration / 2, &out_event));
  EXPECT_TRUE(EqualEvents(event, out_event));
  EXPECT_EQ(1u, buffer.Length());
  EXPECT_FALSE(buffer.Empty());

#ifdef LEGACY_BITEXACT
  EXPECT_TRUE(buffer.GetEvent(timestamp + duration + 10, &out_event));
#endif
  EXPECT_FALSE(buffer.GetEvent(timestamp + duration + 10, &out_event));
  EXPECT_EQ(0u, buffer.Length());
  EXPECT_TRUE(buffer.Empty());
}

TEST(DtmfBuffer, MergingPackets) {
  int event_no = 0;
  bool end_bit = false;
  int volume = 17;
  int duration = 80;
  uint32_t timestamp = 0x12345678;
  DtmfEvent event(timestamp, event_no, volume, duration, end_bit);
  DtmfBuffer buffer(sample_rate_hz);
  EXPECT_EQ(DtmfBuffer::kOK, buffer.InsertEvent(event));

  event.duration += 80;
  EXPECT_EQ(DtmfBuffer::kOK, buffer.InsertEvent(event));

  event.duration += 80;
  event.end_bit = true;
  EXPECT_EQ(DtmfBuffer::kOK, buffer.InsertEvent(event));

  EXPECT_EQ(1u, buffer.Length());

  DtmfEvent out_event;
  EXPECT_TRUE(buffer.GetEvent(timestamp, &out_event));
  EXPECT_TRUE(EqualEvents(event, out_event));
}

TEST(DtmfBuffer, OverlappingEvents) {
  int event_no = 0;
  bool end_bit = true;
  int volume = 1;
  int duration = 80;
  uint32_t timestamp = 0x12345678 + 80;
  DtmfEvent short_event(timestamp, event_no, volume, duration, end_bit);
  DtmfBuffer buffer(sample_rate_hz);
  EXPECT_EQ(DtmfBuffer::kOK, buffer.InsertEvent(short_event));

  event_no = 10;
  end_bit = false;
  timestamp = 0x12345678;
  DtmfEvent long_event(timestamp, event_no, volume, duration, end_bit);
  EXPECT_EQ(DtmfBuffer::kOK, buffer.InsertEvent(long_event));

  long_event.duration += 80;
  EXPECT_EQ(DtmfBuffer::kOK, buffer.InsertEvent(long_event));

  long_event.duration += 80;
  long_event.end_bit = true;
  EXPECT_EQ(DtmfBuffer::kOK, buffer.InsertEvent(long_event));

  EXPECT_EQ(2u, buffer.Length());

  DtmfEvent out_event;

  EXPECT_TRUE(buffer.GetEvent(timestamp, &out_event));
  EXPECT_TRUE(EqualEvents(long_event, out_event));

#ifdef LEGACY_BITEXACT
  EXPECT_TRUE(
      buffer.GetEvent(timestamp + long_event.duration + 10, &out_event));
  EXPECT_TRUE(EqualEvents(long_event, out_event));
  EXPECT_TRUE(
      buffer.GetEvent(timestamp + long_event.duration + 10, &out_event));
  EXPECT_TRUE(EqualEvents(short_event, out_event));
#else
  EXPECT_FALSE(
      buffer.GetEvent(timestamp + long_event.duration + 10, &out_event));
#endif
  EXPECT_TRUE(buffer.Empty());
}

TEST(DtmfBuffer, ExtrapolationTime) {
  int event_no = 0;
  bool end_bit = false;
  int volume = 1;
  int duration = 80;
  uint32_t timestamp = 0x12345678;
  DtmfEvent event1(timestamp, event_no, volume, duration, end_bit);
  DtmfBuffer buffer(sample_rate_hz);
  EXPECT_EQ(DtmfBuffer::kOK, buffer.InsertEvent(event1));
  EXPECT_EQ(1u, buffer.Length());

  DtmfEvent out_event;

  EXPECT_TRUE(buffer.GetEvent(timestamp, &out_event));
  EXPECT_TRUE(EqualEvents(event1, out_event));

  uint32_t timestamp_now = timestamp + duration + 100;
  EXPECT_TRUE(buffer.GetEvent(timestamp_now, &out_event));
  EXPECT_TRUE(EqualEvents(event1, out_event));

  timestamp += duration;
  event_no = 1;
  DtmfEvent event2(timestamp, event_no, volume, duration, end_bit);
  EXPECT_EQ(DtmfBuffer::kOK, buffer.InsertEvent(event2));
  EXPECT_EQ(2u, buffer.Length());

  EXPECT_TRUE(buffer.GetEvent(timestamp_now, &out_event));
  EXPECT_TRUE(EqualEvents(event2, out_event));

  EXPECT_EQ(1u, buffer.Length());

  timestamp_now = timestamp + duration + 600;
#ifdef LEGACY_BITEXACT
  EXPECT_TRUE(buffer.GetEvent(timestamp_now, &out_event));
#endif
  EXPECT_FALSE(buffer.GetEvent(timestamp_now, &out_event));
  EXPECT_TRUE(buffer.Empty());
}

TEST(DtmfBuffer, TimestampWraparound) {
  int event_no = 0;
  bool end_bit = true;
  int volume = 1;
  int duration = 80;
  uint32_t timestamp1 = 0xFFFFFFFF - duration;
  DtmfEvent event1(timestamp1, event_no, volume, duration, end_bit);
  uint32_t timestamp2 = 0;
  DtmfEvent event2(timestamp2, event_no, volume, duration, end_bit);
  DtmfBuffer buffer(sample_rate_hz);
  EXPECT_EQ(DtmfBuffer::kOK, buffer.InsertEvent(event1));
  EXPECT_EQ(DtmfBuffer::kOK, buffer.InsertEvent(event2));
  EXPECT_EQ(2u, buffer.Length());
  DtmfEvent out_event;
  EXPECT_TRUE(buffer.GetEvent(timestamp1, &out_event));
  EXPECT_TRUE(EqualEvents(event1, out_event));
#ifdef LEGACY_BITEXACT
  EXPECT_EQ(1u, buffer.Length());
#else
  EXPECT_EQ(2u, buffer.Length());
#endif

  buffer.Flush();

  EXPECT_EQ(DtmfBuffer::kOK, buffer.InsertEvent(event2));
  EXPECT_EQ(DtmfBuffer::kOK, buffer.InsertEvent(event1));
  EXPECT_EQ(2u, buffer.Length());
  EXPECT_TRUE(buffer.GetEvent(timestamp1, &out_event));
  EXPECT_TRUE(EqualEvents(event1, out_event));
#ifdef LEGACY_BITEXACT
  EXPECT_EQ(1u, buffer.Length());
#else
  EXPECT_EQ(2u, buffer.Length());
#endif
}

TEST(DtmfBuffer, InvalidEvents) {
  int event_no = 0;
  bool end_bit = true;
  int volume = 1;
  int duration = 80;
  uint32_t timestamp = 0x12345678;
  DtmfEvent event(timestamp, event_no, volume, duration, end_bit);
  DtmfBuffer buffer(sample_rate_hz);

  event.event_no = -1;
  EXPECT_EQ(DtmfBuffer::kInvalidEventParameters, buffer.InsertEvent(event));
  event.event_no = 16;
  EXPECT_EQ(DtmfBuffer::kInvalidEventParameters, buffer.InsertEvent(event));
  event.event_no = 0;

  event.volume = -1;
  EXPECT_EQ(DtmfBuffer::kInvalidEventParameters, buffer.InsertEvent(event));
  event.volume = 64;
  EXPECT_EQ(DtmfBuffer::kInvalidEventParameters, buffer.InsertEvent(event));
  event.volume = 0;

  event.duration = -1;
  EXPECT_EQ(DtmfBuffer::kInvalidEventParameters, buffer.InsertEvent(event));
  event.duration = 0;
  EXPECT_EQ(DtmfBuffer::kInvalidEventParameters, buffer.InsertEvent(event));
  event.duration = 0xFFFF + 1;
  EXPECT_EQ(DtmfBuffer::kInvalidEventParameters, buffer.InsertEvent(event));
  event.duration = 1;

  EXPECT_EQ(DtmfBuffer::kOK, buffer.InsertEvent(event));
}
}
