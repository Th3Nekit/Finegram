/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_CODING_NETEQ_DECODER_DATABASE_H_
#define MODULES_AUDIO_CODING_NETEQ_DECODER_DATABASE_H_

#include <map>
#include <memory>
#include <string>

#include "absl/strings/string_view.h"
#include "api/audio_codecs/audio_decoder_factory.h"
#include "api/audio_codecs/audio_format.h"
#include "api/scoped_refptr.h"
#include "modules/audio_coding/codecs/cng/webrtc_cng.h"
#include "modules/audio_coding/neteq/packet.h"

namespace webrtc {

class DecoderDatabase {
 public:
  enum DatabaseReturnCodes {
    kOK = 0,
    kInvalidRtpPayloadType = -1,
    kCodecNotSupported = -2,
    kInvalidSampleRate = -3,
    kDecoderExists = -4,
    kDecoderNotFound = -5,
    kInvalidPointer = -6
  };

  class DecoderInfo {
   public:
    DecoderInfo(const SdpAudioFormat& audio_format,
                absl::optional<AudioCodecPairId> codec_pair_id,
                AudioDecoderFactory* factory,
                absl::string_view codec_name);
    explicit DecoderInfo(const SdpAudioFormat& audio_format,
                         absl::optional<AudioCodecPairId> codec_pair_id,
                         AudioDecoderFactory* factory = nullptr);
    DecoderInfo(DecoderInfo&&);
    ~DecoderInfo();

    AudioDecoder* GetDecoder() const;

    void DropDecoder() const { decoder_.reset(); }

    int SampleRateHz() const {
      if (IsDtmf()) {

        return audio_format_.clockrate_hz;
      }
      const AudioDecoder* decoder = GetDecoder();
      RTC_DCHECK_EQ(1, !!decoder + !!cng_decoder_);
      return decoder ? decoder->SampleRateHz() : cng_decoder_->sample_rate_hz;
    }

    const SdpAudioFormat& GetFormat() const { return audio_format_; }

    bool IsComfortNoise() const {
      RTC_DCHECK_EQ(!!cng_decoder_, subtype_ == Subtype::kComfortNoise);
      return subtype_ == Subtype::kComfortNoise;
    }

    bool IsDtmf() const { return subtype_ == Subtype::kDtmf; }

    bool IsRed() const { return subtype_ == Subtype::kRed; }

    bool IsType(absl::string_view name) const;

    const std::string& get_name() const { return name_; }

   private:

    const std::string name_;

    const SdpAudioFormat audio_format_;
    const absl::optional<AudioCodecPairId> codec_pair_id_;
    AudioDecoderFactory* const factory_;
    mutable std::unique_ptr<AudioDecoder> decoder_;

    struct CngDecoder {
      static absl::optional<CngDecoder> Create(const SdpAudioFormat& format);
      int sample_rate_hz;
    };
    const absl::optional<CngDecoder> cng_decoder_;

    enum class Subtype : int8_t { kNormal, kComfortNoise, kDtmf, kRed };

    static Subtype SubtypeFromFormat(const SdpAudioFormat& format);

    const Subtype subtype_;
  };

  static const uint8_t kRtpPayloadTypeError = 0xFF;

  DecoderDatabase(
      const rtc::scoped_refptr<AudioDecoderFactory>& decoder_factory,
      absl::optional<AudioCodecPairId> codec_pair_id);

  virtual ~DecoderDatabase();

  DecoderDatabase(const DecoderDatabase&) = delete;
  DecoderDatabase& operator=(const DecoderDatabase&) = delete;

  virtual bool Empty() const;

  virtual int Size() const;

  virtual std::vector<int> SetCodecs(
      const std::map<int, SdpAudioFormat>& codecs);

  virtual int RegisterPayload(int rtp_payload_type,
                              const SdpAudioFormat& audio_format);

  virtual int Remove(uint8_t rtp_payload_type);

  virtual void RemoveAll();

  virtual const DecoderInfo* GetDecoderInfo(uint8_t rtp_payload_type) const;

  virtual int SetActiveDecoder(uint8_t rtp_payload_type, bool* new_decoder);

  virtual AudioDecoder* GetActiveDecoder() const;

  virtual int SetActiveCngDecoder(uint8_t rtp_payload_type);

  virtual ComfortNoiseDecoder* GetActiveCngDecoder() const;

  AudioDecoder* GetDecoder(uint8_t rtp_payload_type) const;

  bool IsComfortNoise(uint8_t rtp_payload_type) const;

  bool IsDtmf(uint8_t rtp_payload_type) const;

  bool IsRed(uint8_t rtp_payload_type) const;

  int CheckPayloadTypes(const PacketList& packet_list) const;

 private:
  typedef std::map<uint8_t, DecoderInfo> DecoderMap;

  DecoderMap decoders_;
  int active_decoder_type_;
  int active_cng_decoder_type_;
  mutable std::unique_ptr<ComfortNoiseDecoder> active_cng_decoder_;
  rtc::scoped_refptr<AudioDecoderFactory> decoder_factory_;
  const absl::optional<AudioCodecPairId> codec_pair_id_;
};

}
#endif
