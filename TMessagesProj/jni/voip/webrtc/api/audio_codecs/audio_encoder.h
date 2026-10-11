/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_AUDIO_CODECS_AUDIO_ENCODER_H_
#define API_AUDIO_CODECS_AUDIO_ENCODER_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/base/attributes.h"
#include "absl/types/optional.h"
#include "api/array_view.h"
#include "api/call/bitrate_allocation.h"
#include "api/units/data_rate.h"
#include "api/units/time_delta.h"
#include "rtc_base/buffer.h"

namespace webrtc {

class RtcEventLog;

struct ANAStats {
  ANAStats();
  ANAStats(const ANAStats&);
  ~ANAStats();

  absl::optional<uint32_t> bitrate_action_counter;

  absl::optional<uint32_t> channel_action_counter;

  absl::optional<uint32_t> dtx_action_counter;

  absl::optional<uint32_t> fec_action_counter;

  absl::optional<uint32_t> frame_length_increase_counter;

  absl::optional<uint32_t> frame_length_decrease_counter;

  absl::optional<float> uplink_packet_loss_fraction;
};

class AudioEncoder {
 public:

  enum class CodecType {
    kOther = 0,
    kOpus = 1,
    kIsac = 2,
    kPcmA = 3,
    kPcmU = 4,
    kG722 = 5,
    kIlbc = 6,

    kMaxLoggedAudioCodecTypes
  };

  struct EncodedInfoLeaf {
    size_t encoded_bytes = 0;
    uint32_t encoded_timestamp = 0;
    int payload_type = 0;
    bool send_even_if_empty = false;
    bool speech = true;
    CodecType encoder_type = CodecType::kOther;
  };

  struct EncodedInfo : public EncodedInfoLeaf {
    EncodedInfo();
    EncodedInfo(const EncodedInfo&);
    EncodedInfo(EncodedInfo&&);
    ~EncodedInfo();
    EncodedInfo& operator=(const EncodedInfo&);
    EncodedInfo& operator=(EncodedInfo&&);

    std::vector<EncodedInfoLeaf> redundant;
  };

  virtual ~AudioEncoder() = default;

  virtual int SampleRateHz() const = 0;
  virtual size_t NumChannels() const = 0;

  virtual int RtpTimestampRateHz() const;

  virtual size_t Num10MsFramesInNextPacket() const = 0;

  virtual size_t Max10MsFramesInAPacket() const = 0;

  virtual int GetTargetBitrate() const = 0;

  EncodedInfo Encode(uint32_t rtp_timestamp,
                     rtc::ArrayView<const int16_t> audio,
                     rtc::Buffer* encoded);

  virtual void Reset() = 0;

  virtual bool SetFec(bool enable);

  virtual bool SetDtx(bool enable);

  virtual bool GetDtx() const;

  enum class Application { kSpeech, kAudio };
  virtual bool SetApplication(Application application);

  virtual void SetMaxPlaybackRate(int frequency_hz);

  ABSL_DEPRECATED("Use OnReceivedTargetAudioBitrate instead")
  virtual void SetTargetBitrate(int target_bps);

  virtual rtc::ArrayView<std::unique_ptr<AudioEncoder>>
  ReclaimContainedEncoders();

  virtual bool EnableAudioNetworkAdaptor(const std::string& config_string,
                                         RtcEventLog* event_log);

  virtual void DisableAudioNetworkAdaptor();

  virtual void OnReceivedUplinkPacketLossFraction(
      float uplink_packet_loss_fraction);

  ABSL_DEPRECATED("")
  virtual void OnReceivedUplinkRecoverablePacketLossFraction(
      float uplink_recoverable_packet_loss_fraction);

  virtual void OnReceivedTargetAudioBitrate(int target_bps);

  virtual void OnReceivedUplinkBandwidth(int target_audio_bitrate_bps,
                                         absl::optional<int64_t> bwe_period_ms);

  virtual void OnReceivedUplinkAllocation(BitrateAllocationUpdate update);

  virtual void OnReceivedRtt(int rtt_ms);

  virtual void OnReceivedOverhead(size_t overhead_bytes_per_packet);

  virtual void SetReceiverFrameLengthRange(int min_frame_length_ms,
                                           int max_frame_length_ms);

  virtual ANAStats GetANAStats() const;

  virtual absl::optional<std::pair<TimeDelta, TimeDelta>> GetFrameLengthRange()
      const = 0;

  virtual absl::optional<std::pair<DataRate, DataRate>> GetBitrateRange()
      const {
    return absl::nullopt;
  }

  static constexpr int kMaxNumberOfChannels = 24;

 protected:

  virtual EncodedInfo EncodeImpl(uint32_t rtp_timestamp,
                                 rtc::ArrayView<const int16_t> audio,
                                 rtc::Buffer* encoded) = 0;
};
}
#endif
