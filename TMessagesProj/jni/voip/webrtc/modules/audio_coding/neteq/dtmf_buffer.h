/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_CODING_NETEQ_DTMF_BUFFER_H_
#define MODULES_AUDIO_CODING_NETEQ_DTMF_BUFFER_H_

#include <stddef.h>
#include <stdint.h>

#include <list>

namespace webrtc {

struct DtmfEvent {
  uint32_t timestamp;
  int event_no;
  int volume;
  int duration;
  bool end_bit;

  DtmfEvent()
      : timestamp(0), event_no(0), volume(0), duration(0), end_bit(false) {}
  DtmfEvent(uint32_t ts, int ev, int vol, int dur, bool end)
      : timestamp(ts), event_no(ev), volume(vol), duration(dur), end_bit(end) {}
};

class DtmfBuffer {
 public:
  enum BufferReturnCodes {
    kOK = 0,
    kInvalidPointer,
    kPayloadTooShort,
    kInvalidEventParameters,
    kInvalidSampleRate
  };

  explicit DtmfBuffer(int fs_hz);

  virtual ~DtmfBuffer();

  DtmfBuffer(const DtmfBuffer&) = delete;
  DtmfBuffer& operator=(const DtmfBuffer&) = delete;

  virtual void Flush();

  static int ParseEvent(uint32_t rtp_timestamp,
                        const uint8_t* payload,
                        size_t payload_length_bytes,
                        DtmfEvent* event);

  virtual int InsertEvent(const DtmfEvent& event);

  virtual bool GetEvent(uint32_t current_timestamp, DtmfEvent* event);

  virtual size_t Length() const;

  virtual bool Empty() const;

  virtual int SetSampleRate(int fs_hz);

 private:
  typedef std::list<DtmfEvent> DtmfList;

  int max_extrapolation_samples_;
  int frame_len_samples_;

  static bool SameEvent(const DtmfEvent& a, const DtmfEvent& b);

  bool MergeEvents(DtmfList::iterator it, const DtmfEvent& event);

  static bool CompareEvents(const DtmfEvent& a, const DtmfEvent& b);

  DtmfList buffer_;
};

}
#endif
