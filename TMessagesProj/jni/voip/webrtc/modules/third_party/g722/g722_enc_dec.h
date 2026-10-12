/*
 * SpanDSP - a series of DSP components for telephony
 *
 * g722.h - The ITU G.722 codec.
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2005 Steve Underwood
 *
 *  Despite my general liking of the GPL, I place my own contributions
 *  to this code in the public domain for the benefit of all mankind -
 *  even the slimy ones who might try to proprietize my work and use it
 *  to my detriment.
 *
 * Based on a single channel G.722 codec which is:
 *
 *****    Copyright (c) CMU    1993      *****
 * Computer Science, Speech Group
 * Chengxiang Lu and Alex Hauptmann
 *
 * $Id: g722.h,v 1.10 2006/06/16 12:45:53 steveu Exp $
 *
 * Modifications for WebRtc, 2011/04/28, by tlegrand:
 * -Changed to use WebRtc types
 * -Added new defines for minimum and maximum values of short int
 */

/*! \file */

#ifndef MODULES_THIRD_PARTY_G722_G722_H_
#define MODULES_THIRD_PARTY_G722_G722_H_

#include <stddef.h>
#include <stdint.h>

#define WEBRTC_INT16_MAX 32767
#define WEBRTC_INT16_MIN -32768

enum { G722_SAMPLE_RATE_8000 = 0x0001, G722_PACKED = 0x0002 };

typedef struct {

  int itu_test_mode;

  int packed;

  int eight_k;

  int bits_per_sample;

  int x[24];

  struct {
    int s;
    int sp;
    int sz;
    int r[3];
    int a[3];
    int ap[3];
    int p[3];
    int d[7];
    int b[7];
    int bp[7];
    int sg[7];
    int nb;
    int det;
  } band[2];

  unsigned int in_buffer;
  int in_bits;
  unsigned int out_buffer;
  int out_bits;
} G722EncoderState;

typedef struct {

  int itu_test_mode;

  int packed;

  int eight_k;

  int bits_per_sample;

  int x[24];

  struct {
    int s;
    int sp;
    int sz;
    int r[3];
    int a[3];
    int ap[3];
    int p[3];
    int d[7];
    int b[7];
    int bp[7];
    int sg[7];
    int nb;
    int det;
  } band[2];

  unsigned int in_buffer;
  int in_bits;
  unsigned int out_buffer;
  int out_bits;
} G722DecoderState;

#ifdef __cplusplus
extern "C" {
#endif

G722EncoderState* WebRtc_g722_encode_init(G722EncoderState* s,
                                          int rate,
                                          int options);
int WebRtc_g722_encode_release(G722EncoderState* s);
size_t WebRtc_g722_encode(G722EncoderState* s,
                          uint8_t g722_data[],
                          const int16_t amp[],
                          size_t len);

G722DecoderState* WebRtc_g722_decode_init(G722DecoderState* s,
                                          int rate,
                                          int options);
int WebRtc_g722_decode_release(G722DecoderState* s);
size_t WebRtc_g722_decode(G722DecoderState* s,
                          int16_t amp[],
                          const uint8_t g722_data[],
                          size_t len);

#ifdef __cplusplus
}
#endif

#endif
