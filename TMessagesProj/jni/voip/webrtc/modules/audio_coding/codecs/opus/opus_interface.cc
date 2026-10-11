/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_coding/codecs/opus/opus_interface.h"

#include <cstdlib>
#include <numeric>

#include "api/array_view.h"
#include "rtc_base/checks.h"
#include "system_wrappers/include/field_trial.h"

enum {
#if WEBRTC_OPUS_SUPPORT_120MS_PTIME

  kWebRtcOpusMaxEncodeFrameSizeMs = 120,
#else

  kWebRtcOpusMaxEncodeFrameSizeMs = 60,
#endif

  kWebRtcOpusMaxDecodeFrameSizeMs = 120,

  kWebRtcOpusPlcFrameSizeMs = 10,
};

constexpr char kPlcUsePrevDecodedSamplesFieldTrial[] =
    "WebRTC-Audio-OpusPlcUsePrevDecodedSamples";

constexpr char kAvoidNoisePumpingDuringDtxFieldTrial[] =
    "WebRTC-Audio-OpusAvoidNoisePumpingDuringDtx";

constexpr char kSetSignalVoiceWithDtxFieldTrial[] =
    "WebRTC-Audio-OpusSetSignalVoiceWithDtx";

static int FrameSizePerChannel(int frame_size_ms, int sample_rate_hz) {
  RTC_DCHECK_GT(frame_size_ms, 0);
  RTC_DCHECK_EQ(frame_size_ms % 10, 0);
  RTC_DCHECK_GT(sample_rate_hz, 0);
  RTC_DCHECK_EQ(sample_rate_hz % 1000, 0);
  return frame_size_ms * (sample_rate_hz / 1000);
}

static int MaxFrameSizePerChannel(int sample_rate_hz) {
  return FrameSizePerChannel(kWebRtcOpusMaxDecodeFrameSizeMs, sample_rate_hz);
}

static int DefaultFrameSizePerChannel(int sample_rate_hz) {
  return FrameSizePerChannel(20, sample_rate_hz);
}

static bool WebRtcOpus_IsHighEnergyRefreshDtxPacket(
    OpusEncInst* inst,
    rtc::ArrayView<const int16_t> frame,
    rtc::ArrayView<const uint8_t> encoded) {
  if (encoded.size() <= 2) {
    return false;
  }
  int number_frames =
      frame.size() / DefaultFrameSizePerChannel(inst->sample_rate_hz);
  if (number_frames > 0 &&
      WebRtcOpus_PacketHasVoiceActivity(encoded.data(), encoded.size()) == 0) {
    const float average_frame_energy =
        std::accumulate(frame.begin(), frame.end(), 0.0f,
                        [](float a, int32_t b) { return a + b * b; }) /
        number_frames;
    if (WebRtcOpus_GetInDtx(inst) == 1 &&
        average_frame_energy >= inst->smooth_energy_non_active_frames * 0.5f) {

      return true;
    }

    if (average_frame_energy < inst->smooth_energy_non_active_frames * 0.5f) {
      inst->smooth_energy_non_active_frames = average_frame_energy;
    } else {
      inst->smooth_energy_non_active_frames +=
          (average_frame_energy - inst->smooth_energy_non_active_frames) *
          0.25f;
    }
  }
  return false;
}

int16_t WebRtcOpus_EncoderCreate(OpusEncInst** inst,
                                 size_t channels,
                                 int32_t application,
                                 int sample_rate_hz) {
  int opus_app;
  if (!inst)
    return -1;

  switch (application) {
    case 0:
      opus_app = OPUS_APPLICATION_VOIP;
      break;
    case 1:
      opus_app = OPUS_APPLICATION_AUDIO;
      break;
    default:
      return -1;
  }

  OpusEncInst* state =
      reinterpret_cast<OpusEncInst*>(calloc(1, sizeof(OpusEncInst)));
  RTC_DCHECK(state);

  int error;
  state->encoder = opus_encoder_create(
      sample_rate_hz, static_cast<int>(channels), opus_app, &error);

  if (error != OPUS_OK || (!state->encoder && !state->multistream_encoder)) {
    WebRtcOpus_EncoderFree(state);
    return -1;
  }

  state->in_dtx_mode = 0;
  state->channels = channels;
  state->sample_rate_hz = sample_rate_hz;
  state->smooth_energy_non_active_frames = 0.0f;
  state->avoid_noise_pumping_during_dtx =
      webrtc::field_trial::IsEnabled(kAvoidNoisePumpingDuringDtxFieldTrial);

  *inst = state;
  return 0;
}

int16_t WebRtcOpus_MultistreamEncoderCreate(
    OpusEncInst** inst,
    size_t channels,
    int32_t application,
    size_t streams,
    size_t coupled_streams,
    const unsigned char* channel_mapping) {
  int opus_app;
  if (!inst)
    return -1;

  switch (application) {
    case 0:
      opus_app = OPUS_APPLICATION_VOIP;
      break;
    case 1:
      opus_app = OPUS_APPLICATION_AUDIO;
      break;
    default:
      return -1;
  }

  OpusEncInst* state =
      reinterpret_cast<OpusEncInst*>(calloc(1, sizeof(OpusEncInst)));
  RTC_DCHECK(state);

  int error;
  const int sample_rate_hz = 48000;
  state->multistream_encoder = opus_multistream_encoder_create(
      sample_rate_hz, channels, streams, coupled_streams, channel_mapping,
      opus_app, &error);

  if (error != OPUS_OK || (!state->encoder && !state->multistream_encoder)) {
    WebRtcOpus_EncoderFree(state);
    return -1;
  }

  state->in_dtx_mode = 0;
  state->channels = channels;
  state->sample_rate_hz = sample_rate_hz;
  state->smooth_energy_non_active_frames = 0.0f;
  state->avoid_noise_pumping_during_dtx = false;

  *inst = state;
  return 0;
}

int16_t WebRtcOpus_EncoderFree(OpusEncInst* inst) {
  if (inst) {
    if (inst->encoder) {
      opus_encoder_destroy(inst->encoder);
    } else {
      opus_multistream_encoder_destroy(inst->multistream_encoder);
    }
    free(inst);
    return 0;
  } else {
    return -1;
  }
}

int WebRtcOpus_Encode(OpusEncInst* inst,
                      const int16_t* audio_in,
                      size_t samples,
                      size_t length_encoded_buffer,
                      uint8_t* encoded) {
  int res;

  if (samples > 48 * kWebRtcOpusMaxEncodeFrameSizeMs) {
    return -1;
  }

  if (inst->encoder) {
    res = opus_encode(inst->encoder, (const opus_int16*)audio_in,
                      static_cast<int>(samples), encoded,
                      static_cast<opus_int32>(length_encoded_buffer));
  } else {
    res = opus_multistream_encode(
        inst->multistream_encoder, (const opus_int16*)audio_in,
        static_cast<int>(samples), encoded,
        static_cast<opus_int32>(length_encoded_buffer));
  }

  if (res <= 0) {
    return -1;
  }

  if (res <= 2) {

    if (inst->in_dtx_mode) {
      return 0;
    } else {
      inst->in_dtx_mode = 1;
      return res;
    }
  }

  if (inst->avoid_noise_pumping_during_dtx && WebRtcOpus_GetUseDtx(inst) == 1 &&
      WebRtcOpus_IsHighEnergyRefreshDtxPacket(
          inst, rtc::MakeArrayView(audio_in, samples),
          rtc::MakeArrayView(encoded, res))) {

    encoded[0] = encoded[0] & 0b11111100;
    inst->in_dtx_mode = 1;

    return 1;
  }
  inst->in_dtx_mode = 0;
  return res;
}

#define ENCODER_CTL(inst, vargs)                \
  (inst->encoder                                \
       ? opus_encoder_ctl(inst->encoder, vargs) \
       : opus_multistream_encoder_ctl(inst->multistream_encoder, vargs))

int16_t WebRtcOpus_SetBitRate(OpusEncInst* inst, int32_t rate) {
  if (inst) {
    return ENCODER_CTL(inst, OPUS_SET_BITRATE(rate));
  } else {
    return -1;
  }
}

int16_t WebRtcOpus_SetPacketLossRate(OpusEncInst* inst, int32_t loss_rate) {
  if (inst) {
    return ENCODER_CTL(inst, OPUS_SET_PACKET_LOSS_PERC(loss_rate));
  } else {
    return -1;
  }
}

int16_t WebRtcOpus_SetMaxPlaybackRate(OpusEncInst* inst, int32_t frequency_hz) {
  opus_int32 set_bandwidth;

  if (!inst)
    return -1;

  if (frequency_hz <= 8000) {
    set_bandwidth = OPUS_BANDWIDTH_NARROWBAND;
  } else if (frequency_hz <= 12000) {
    set_bandwidth = OPUS_BANDWIDTH_MEDIUMBAND;
  } else if (frequency_hz <= 16000) {
    set_bandwidth = OPUS_BANDWIDTH_WIDEBAND;
  } else if (frequency_hz <= 24000) {
    set_bandwidth = OPUS_BANDWIDTH_SUPERWIDEBAND;
  } else {
    set_bandwidth = OPUS_BANDWIDTH_FULLBAND;
  }
  return ENCODER_CTL(inst, OPUS_SET_MAX_BANDWIDTH(set_bandwidth));
}

int16_t WebRtcOpus_GetMaxPlaybackRate(OpusEncInst* const inst,
                                      int32_t* result_hz) {
  if (inst->encoder) {
    if (opus_encoder_ctl(inst->encoder, OPUS_GET_MAX_BANDWIDTH(result_hz)) ==
        OPUS_OK) {
      return 0;
    }
    return -1;
  }

  opus_int32 max_bandwidth;
  int s;
  int ret;

  max_bandwidth = 0;
  ret = OPUS_OK;
  s = 0;
  while (ret == OPUS_OK) {
    OpusEncoder* enc;
    opus_int32 bandwidth;

    ret = ENCODER_CTL(inst, OPUS_MULTISTREAM_GET_ENCODER_STATE(s, &enc));
    if (ret == OPUS_BAD_ARG)
      break;
    if (ret != OPUS_OK)
      return -1;
    if (opus_encoder_ctl(enc, OPUS_GET_MAX_BANDWIDTH(&bandwidth)) != OPUS_OK)
      return -1;

    if (max_bandwidth != 0 && max_bandwidth != bandwidth)
      return -1;

    max_bandwidth = bandwidth;
    s++;
  }
  *result_hz = max_bandwidth;
  return 0;
}

int16_t WebRtcOpus_EnableFec(OpusEncInst* inst) {
  if (inst) {
    return ENCODER_CTL(inst, OPUS_SET_INBAND_FEC(1));
  } else {
    return -1;
  }
}

int16_t WebRtcOpus_DisableFec(OpusEncInst* inst) {
  if (inst) {
    return ENCODER_CTL(inst, OPUS_SET_INBAND_FEC(0));
  } else {
    return -1;
  }
}

int16_t WebRtcOpus_EnableDtx(OpusEncInst* inst) {
  if (inst) {
    if (webrtc::field_trial::IsEnabled(kSetSignalVoiceWithDtxFieldTrial)) {
      int ret = ENCODER_CTL(inst, OPUS_SET_SIGNAL(OPUS_SIGNAL_VOICE));
      if (ret != OPUS_OK) {
        return ret;
      }
    }
    return ENCODER_CTL(inst, OPUS_SET_DTX(1));
  } else {
    return -1;
  }
}

int16_t WebRtcOpus_DisableDtx(OpusEncInst* inst) {
  if (inst) {
    if (webrtc::field_trial::IsEnabled(kSetSignalVoiceWithDtxFieldTrial)) {
      int ret = ENCODER_CTL(inst, OPUS_SET_SIGNAL(OPUS_AUTO));
      if (ret != OPUS_OK) {
        return ret;
      }
    }
    return ENCODER_CTL(inst, OPUS_SET_DTX(0));
  } else {
    return -1;
  }
}

int16_t WebRtcOpus_GetUseDtx(OpusEncInst* inst) {
  if (inst) {
    opus_int32 use_dtx;
    if (ENCODER_CTL(inst, OPUS_GET_DTX(&use_dtx)) == 0) {
      return use_dtx;
    }
  }
  return -1;
}

int16_t WebRtcOpus_EnableCbr(OpusEncInst* inst) {
  if (inst) {
    return ENCODER_CTL(inst, OPUS_SET_VBR(0));
  } else {
    return -1;
  }
}

int16_t WebRtcOpus_DisableCbr(OpusEncInst* inst) {
  if (inst) {
    return ENCODER_CTL(inst, OPUS_SET_VBR(1));
  } else {
    return -1;
  }
}

int16_t WebRtcOpus_SetComplexity(OpusEncInst* inst, int32_t complexity) {
  if (inst) {
    return ENCODER_CTL(inst, OPUS_SET_COMPLEXITY(complexity));
  } else {
    return -1;
  }
}

int32_t WebRtcOpus_GetBandwidth(OpusEncInst* inst) {
  if (!inst) {
    return -1;
  }
  int32_t bandwidth;
  if (ENCODER_CTL(inst, OPUS_GET_BANDWIDTH(&bandwidth)) == 0) {
    return bandwidth;
  } else {
    return -1;
  }
}

int16_t WebRtcOpus_SetBandwidth(OpusEncInst* inst, int32_t bandwidth) {
  if (inst) {
    return ENCODER_CTL(inst, OPUS_SET_BANDWIDTH(bandwidth));
  } else {
    return -1;
  }
}

int16_t WebRtcOpus_SetForceChannels(OpusEncInst* inst, size_t num_channels) {
  if (!inst)
    return -1;
  if (num_channels == 0) {
    return ENCODER_CTL(inst, OPUS_SET_FORCE_CHANNELS(OPUS_AUTO));
  } else if (num_channels == 1 || num_channels == 2) {
    return ENCODER_CTL(inst, OPUS_SET_FORCE_CHANNELS(num_channels));
  } else {
    return -1;
  }
}

int32_t WebRtcOpus_GetInDtx(OpusEncInst* inst) {
  if (!inst) {
    return -1;
  }
#ifdef OPUS_GET_IN_DTX
  int32_t in_dtx;
  if (ENCODER_CTL(inst, OPUS_GET_IN_DTX(&in_dtx)) == 0) {
    return in_dtx;
  }
#endif
  return -1;
}

int16_t WebRtcOpus_DecoderCreate(OpusDecInst** inst,
                                 size_t channels,
                                 int sample_rate_hz) {
  int error;
  OpusDecInst* state;

  if (inst != NULL) {

    state = reinterpret_cast<OpusDecInst*>(calloc(1, sizeof(OpusDecInst)));
    if (state == NULL) {
      return -1;
    }

    state->decoder =
        opus_decoder_create(sample_rate_hz, static_cast<int>(channels), &error);
    if (error == OPUS_OK && state->decoder) {

      state->channels = channels;
      state->sample_rate_hz = sample_rate_hz;
      state->plc_use_prev_decoded_samples =
          webrtc::field_trial::IsEnabled(kPlcUsePrevDecodedSamplesFieldTrial);
      if (state->plc_use_prev_decoded_samples) {
        state->prev_decoded_samples =
            DefaultFrameSizePerChannel(state->sample_rate_hz);
      }
      state->in_dtx_mode = 0;
      *inst = state;
      return 0;
    }

    if (state->decoder) {
      opus_decoder_destroy(state->decoder);
    }
    free(state);
  }
  return -1;
}

int16_t WebRtcOpus_MultistreamDecoderCreate(
    OpusDecInst** inst,
    size_t channels,
    size_t streams,
    size_t coupled_streams,
    const unsigned char* channel_mapping) {
  int error;
  OpusDecInst* state;

  if (inst != NULL) {

    state = reinterpret_cast<OpusDecInst*>(calloc(1, sizeof(OpusDecInst)));
    if (state == NULL) {
      return -1;
    }

    state->multistream_decoder = opus_multistream_decoder_create(
        48000, channels, streams, coupled_streams, channel_mapping, &error);

    if (error == OPUS_OK && state->multistream_decoder) {

      state->channels = channels;
      state->sample_rate_hz = 48000;
      state->plc_use_prev_decoded_samples =
          webrtc::field_trial::IsEnabled(kPlcUsePrevDecodedSamplesFieldTrial);
      if (state->plc_use_prev_decoded_samples) {
        state->prev_decoded_samples =
            DefaultFrameSizePerChannel(state->sample_rate_hz);
      }
      state->in_dtx_mode = 0;
      *inst = state;
      return 0;
    }

    opus_multistream_decoder_destroy(state->multistream_decoder);
    free(state);
  }
  return -1;
}

int16_t WebRtcOpus_DecoderFree(OpusDecInst* inst) {
  if (inst) {
    if (inst->decoder) {
      opus_decoder_destroy(inst->decoder);
    } else if (inst->multistream_decoder) {
      opus_multistream_decoder_destroy(inst->multistream_decoder);
    }
    free(inst);
    return 0;
  } else {
    return -1;
  }
}

size_t WebRtcOpus_DecoderChannels(OpusDecInst* inst) {
  return inst->channels;
}

void WebRtcOpus_DecoderInit(OpusDecInst* inst) {
  if (inst->decoder) {
    opus_decoder_ctl(inst->decoder, OPUS_RESET_STATE);
  } else {
    opus_multistream_decoder_ctl(inst->multistream_decoder, OPUS_RESET_STATE);
  }
  inst->in_dtx_mode = 0;
}

static int16_t DetermineAudioType(OpusDecInst* inst, size_t encoded_bytes) {

  if (encoded_bytes == 0 && inst->in_dtx_mode) {
    return 2;
  } else if (encoded_bytes == 1 || encoded_bytes == 2) {

    inst->in_dtx_mode = 1;
    return 2;
  } else {
    inst->in_dtx_mode = 0;
    return 0;
  }
}

static int DecodeNative(OpusDecInst* inst,
                        const uint8_t* encoded,
                        size_t encoded_bytes,
                        int frame_size,
                        int16_t* decoded,
                        int16_t* audio_type,
                        int decode_fec) {
  int res = -1;
  if (inst->decoder) {
    res = opus_decode(
        inst->decoder, encoded, static_cast<opus_int32>(encoded_bytes),
        reinterpret_cast<opus_int16*>(decoded), frame_size, decode_fec);
  } else {
    res = opus_multistream_decode(inst->multistream_decoder, encoded,
                                  static_cast<opus_int32>(encoded_bytes),
                                  reinterpret_cast<opus_int16*>(decoded),
                                  frame_size, decode_fec);
  }

  if (res <= 0)
    return -1;

  *audio_type = DetermineAudioType(inst, encoded_bytes);

  return res;
}

static int DecodePlc(OpusDecInst* inst, int16_t* decoded) {
  int16_t audio_type = 0;
  int decoded_samples;
  int plc_samples =
      FrameSizePerChannel(kWebRtcOpusPlcFrameSizeMs, inst->sample_rate_hz);

  if (inst->plc_use_prev_decoded_samples) {

    plc_samples = inst->prev_decoded_samples;
    const int max_samples_per_channel =
        MaxFrameSizePerChannel(inst->sample_rate_hz);
    plc_samples = plc_samples <= max_samples_per_channel
                      ? plc_samples
                      : max_samples_per_channel;
  }
  decoded_samples =
      DecodeNative(inst, NULL, 0, plc_samples, decoded, &audio_type, 0);
  if (decoded_samples < 0) {
    return -1;
  }

  return decoded_samples;
}

int WebRtcOpus_Decode(OpusDecInst* inst,
                      const uint8_t* encoded,
                      size_t encoded_bytes,
                      int16_t* decoded,
                      int16_t* audio_type) {
  int decoded_samples;

  if (encoded_bytes == 0) {
    *audio_type = DetermineAudioType(inst, encoded_bytes);
    decoded_samples = DecodePlc(inst, decoded);
  } else {
    decoded_samples = DecodeNative(inst, encoded, encoded_bytes,
                                   MaxFrameSizePerChannel(inst->sample_rate_hz),
                                   decoded, audio_type, 0);
  }
  if (decoded_samples < 0) {
    return -1;
  }

  if (inst->plc_use_prev_decoded_samples) {

    inst->prev_decoded_samples = decoded_samples;
  }

  return decoded_samples;
}

int WebRtcOpus_DecodeFec(OpusDecInst* inst,
                         const uint8_t* encoded,
                         size_t encoded_bytes,
                         int16_t* decoded,
                         int16_t* audio_type) {
  int decoded_samples;
  int fec_samples;

  if (WebRtcOpus_PacketHasFec(encoded, encoded_bytes) != 1) {
    return 0;
  }

  fec_samples =
      opus_packet_get_samples_per_frame(encoded, inst->sample_rate_hz);

  decoded_samples = DecodeNative(inst, encoded, encoded_bytes, fec_samples,
                                 decoded, audio_type, 1);
  if (decoded_samples < 0) {
    return -1;
  }

  return decoded_samples;
}

int WebRtcOpus_DurationEst(OpusDecInst* inst,
                           const uint8_t* payload,
                           size_t payload_length_bytes) {
  if (payload_length_bytes == 0) {

    return WebRtcOpus_PlcDuration(inst);
  }

  int frames, samples;
  frames = opus_packet_get_nb_frames(
      payload, static_cast<opus_int32>(payload_length_bytes));
  if (frames < 0) {

    return 0;
  }
  samples =
      frames * opus_packet_get_samples_per_frame(payload, inst->sample_rate_hz);
  if (samples > 120 * inst->sample_rate_hz / 1000) {

    return 0;
  }
  return samples;
}

int WebRtcOpus_PlcDuration(OpusDecInst* inst) {
  if (inst->plc_use_prev_decoded_samples) {

    const int plc_samples = inst->prev_decoded_samples;
    const int max_samples_per_channel =
        MaxFrameSizePerChannel(inst->sample_rate_hz);
    return plc_samples <= max_samples_per_channel ? plc_samples
                                                  : max_samples_per_channel;
  }
  return FrameSizePerChannel(kWebRtcOpusPlcFrameSizeMs, inst->sample_rate_hz);
}

int WebRtcOpus_FecDurationEst(const uint8_t* payload,
                              size_t payload_length_bytes,
                              int sample_rate_hz) {
  if (WebRtcOpus_PacketHasFec(payload, payload_length_bytes) != 1) {
    return 0;
  }
  const int samples =
      opus_packet_get_samples_per_frame(payload, sample_rate_hz);
  const int samples_per_ms = sample_rate_hz / 1000;
  if (samples < 10 * samples_per_ms || samples > 120 * samples_per_ms) {

    return 0;
  }
  return samples;
}

int WebRtcOpus_NumSilkFrames(const uint8_t* payload) {

  int payload_length_ms =
      opus_packet_get_samples_per_frame(payload, 48000) / 48;
  if (payload_length_ms < 10)
    payload_length_ms = 10;

  int silk_frames;
  switch (payload_length_ms) {
    case 10:
    case 20:
      silk_frames = 1;
      break;
    case 40:
      silk_frames = 2;
      break;
    case 60:
      silk_frames = 3;
      break;
    default:
      return 0;
  }
  return silk_frames;
}

int WebRtcOpus_PacketHasFec(const uint8_t* payload,
                            size_t payload_length_bytes) {
  if (payload == NULL || payload_length_bytes == 0)
    return 0;

  if (payload[0] & 0x80)
    return 0;

  int silk_frames = WebRtcOpus_NumSilkFrames(payload);
  if (silk_frames == 0)
    return 0;

  const int channels = opus_packet_get_nb_channels(payload);
  RTC_DCHECK(channels == 1 || channels == 2);

  opus_int16 frame_sizes[48];
  const unsigned char* frame_data[48];

  if (opus_packet_parse(payload, static_cast<opus_int32>(payload_length_bytes),
                        NULL, frame_data, frame_sizes, NULL) < 0) {
    return 0;
  }

  if (frame_sizes[0] < 1) {
    return 0;
  }

  for (int n = 0; n < channels; n++) {

    if (frame_data[0][0] & (0x80 >> ((n + 1) * (silk_frames + 1) - 1)))
      return 1;
  }

  return 0;
}

int WebRtcOpus_PacketHasVoiceActivity(const uint8_t* payload,
                                      size_t payload_length_bytes) {
  if (payload == NULL || payload_length_bytes == 0)
    return 0;

  if (payload[0] & 0x80)
    return -1;

  int silk_frames = WebRtcOpus_NumSilkFrames(payload);
  if (silk_frames == 0)
    return -1;

  const int channels = opus_packet_get_nb_channels(payload);
  RTC_DCHECK(channels == 1 || channels == 2);

  opus_int16 frame_sizes[48];
  const unsigned char* frame_data[48];

  int frames =
      opus_packet_parse(payload, static_cast<opus_int32>(payload_length_bytes),
                        NULL, frame_data, frame_sizes, NULL);
  if (frames < 0)
    return -1;

  for (int frame = 0; frame < frames; frame++) {
    if (frame_sizes[frame] < 1) {
      continue;
    }
    if (frame_data[frame][0] >> (8 - silk_frames))
      return 1;
    if (channels == 2 &&
        (frame_data[frame][0] << (silk_frames + 1)) >> (8 - silk_frames))
      return 1;
  }

  return 0;
}
