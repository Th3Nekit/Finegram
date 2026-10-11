/*
 * MXF SMPTE-436M VBI/ANC parsing functions
 * Copyright (c) 2025 Jacob Lifshay
 *
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef AVCODEC_SMPTE_436M_H
#define AVCODEC_SMPTE_436M_H

#include <stdint.h>

typedef struct AVSmpte436mAncIterator {
    uint16_t       anc_packets_left;
    int            size_left;
    const uint8_t *data_left;
} AVSmpte436mAncIterator;

typedef enum AVSmpte436mWrappingType
{
    AV_SMPTE_436M_WRAPPING_TYPE_VANC_FRAME             = 1,
    AV_SMPTE_436M_WRAPPING_TYPE_VANC_FIELD_1           = 2,
    AV_SMPTE_436M_WRAPPING_TYPE_VANC_FIELD_2           = 3,
    AV_SMPTE_436M_WRAPPING_TYPE_VANC_PROGRESSIVE_FRAME = 4,
    AV_SMPTE_436M_WRAPPING_TYPE_HANC_FRAME             = 0x11,
    AV_SMPTE_436M_WRAPPING_TYPE_HANC_FIELD_1           = 0x12,
    AV_SMPTE_436M_WRAPPING_TYPE_HANC_FIELD_2           = 0x13,
    AV_SMPTE_436M_WRAPPING_TYPE_HANC_PROGRESSIVE_FRAME = 0x14,

    AV_SMPTE_436M_WRAPPING_TYPE_MAX = 0xFF,
} AVSmpte436mWrappingType;

typedef enum AVSmpte436mPayloadSampleCoding
{

    AV_SMPTE_436M_PAYLOAD_SAMPLE_CODING_1BIT_LUMA = 1,

    AV_SMPTE_436M_PAYLOAD_SAMPLE_CODING_1BIT_COLOR_DIFF = 2,

    AV_SMPTE_436M_PAYLOAD_SAMPLE_CODING_1BIT_LUMA_AND_COLOR_DIFF = 3,

    AV_SMPTE_436M_PAYLOAD_SAMPLE_CODING_8BIT_LUMA = 4,

    AV_SMPTE_436M_PAYLOAD_SAMPLE_CODING_8BIT_COLOR_DIFF = 5,

    AV_SMPTE_436M_PAYLOAD_SAMPLE_CODING_8BIT_LUMA_AND_COLOR_DIFF = 6,

    AV_SMPTE_436M_PAYLOAD_SAMPLE_CODING_10BIT_LUMA = 7,

    AV_SMPTE_436M_PAYLOAD_SAMPLE_CODING_10BIT_COLOR_DIFF = 8,

    AV_SMPTE_436M_PAYLOAD_SAMPLE_CODING_10BIT_LUMA_AND_COLOR_DIFF = 9,

    AV_SMPTE_436M_PAYLOAD_SAMPLE_CODING_8BIT_LUMA_WITH_PARITY_ERROR = 10,

    AV_SMPTE_436M_PAYLOAD_SAMPLE_CODING_8BIT_COLOR_DIFF_WITH_PARITY_ERROR = 11,

    AV_SMPTE_436M_PAYLOAD_SAMPLE_CODING_8BIT_LUMA_AND_COLOR_DIFF_WITH_PARITY_ERROR = 12,

    AV_SMPTE_436M_PAYLOAD_SAMPLE_CODING_MAX = 0xFF,
} AVSmpte436mPayloadSampleCoding;

#define AV_SMPTE_291M_ANC_PAYLOAD_CAPACITY 0xFF

typedef struct AVSmpte291mAnc8bit {
    uint8_t did;
    uint8_t sdid_or_dbn;
    uint8_t data_count;
    uint8_t payload[AV_SMPTE_291M_ANC_PAYLOAD_CAPACITY];
    uint8_t checksum;
} AVSmpte291mAnc8bit;

#define AV_SMPTE_436M_CODED_ANC_SAMPLE_CAPACITY                                                     \
    (AV_SMPTE_291M_ANC_PAYLOAD_CAPACITY + 4)

#define AV_SMPTE_436M_CODED_ANC_PAYLOAD_CAPACITY (((AV_SMPTE_436M_CODED_ANC_SAMPLE_CAPACITY + 2) / 3) * 4)

typedef struct AVSmpte436mCodedAnc {
    uint16_t                       line_number;
    AVSmpte436mWrappingType        wrapping_type;
    AVSmpte436mPayloadSampleCoding payload_sample_coding;
    uint16_t                       payload_sample_count;
    uint32_t                       payload_array_length;

    uint8_t payload[AV_SMPTE_436M_CODED_ANC_PAYLOAD_CAPACITY];
} AVSmpte436mCodedAnc;

int av_smpte_436m_coded_anc_validate(const AVSmpte436mCodedAnc *anc);

int av_smpte_436m_anc_encode(uint8_t *out, int size, int anc_packet_count, const AVSmpte436mCodedAnc *anc_packets);

struct AVPacket;

int av_smpte_436m_anc_append(struct AVPacket *pkt, int anc_packet_count, const AVSmpte436mCodedAnc *anc_packets);

int av_smpte_436m_anc_iter_init(AVSmpte436mAncIterator *iter, const uint8_t *buf, int buf_size);

int av_smpte_436m_anc_iter_next(AVSmpte436mAncIterator *iter, AVSmpte436mCodedAnc *anc);

int av_smpte_436m_coded_anc_payload_size(AVSmpte436mPayloadSampleCoding sample_coding, uint16_t sample_count);

int av_smpte_291m_anc_8bit_decode(AVSmpte291mAnc8bit            *out,
                                  AVSmpte436mPayloadSampleCoding sample_coding,
                                  uint16_t                       sample_count,
                                  const uint8_t                 *payload,
                                  void                          *log_ctx);

void av_smpte_291m_anc_8bit_fill_checksum(AVSmpte291mAnc8bit *anc);

int av_smpte_291m_anc_8bit_get_sample_count(const AVSmpte291mAnc8bit      *anc,
                                            AVSmpte436mPayloadSampleCoding sample_coding,
                                            void                          *log_ctx);

int av_smpte_291m_anc_8bit_encode(AVSmpte436mCodedAnc           *out,
                                  uint16_t                       line_number,
                                  AVSmpte436mWrappingType        wrapping_type,
                                  AVSmpte436mPayloadSampleCoding sample_coding,
                                  const AVSmpte291mAnc8bit      *payload,
                                  void                          *log_ctx);

#define AV_SMPTE_291M_ANC_DID_CTA_708 0x61

#define AV_SMPTE_291M_ANC_SDID_CTA_708 0x1

int av_smpte_291m_anc_8bit_extract_cta_708(const AVSmpte291mAnc8bit *anc, uint8_t *cc_data, void *log_ctx);

#endif
