/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

/*!\defgroup vp8_decoder WebM VP8/VP9 Decoder
 * \ingroup vp8
 *
 * @{
 */
/*!\file
 * \brief Provides definitions for using VP8 or VP9 within the vpx Decoder
 *        interface.
 */
#ifndef VPX_VPX_VP8DX_H_
#define VPX_VPX_VP8DX_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "./vp8.h"

extern vpx_codec_iface_t vpx_codec_vp8_dx_algo;

extern vpx_codec_iface_t *vpx_codec_vp8_dx(void);

extern vpx_codec_iface_t vpx_codec_vp9_dx_algo;

extern vpx_codec_iface_t *vpx_codec_vp9_dx(void);

enum vp8_dec_control_id {

  VP8D_GET_LAST_REF_UPDATES = VP8_DECODER_CTRL_ID_START,

  VP8D_GET_FRAME_CORRUPTED,

  VP8D_GET_LAST_REF_USED,

  VPXD_SET_DECRYPTOR,
  VP8D_SET_DECRYPTOR = VPXD_SET_DECRYPTOR,

  VP9D_GET_FRAME_SIZE,

  VP9D_GET_DISPLAY_SIZE,

  VP9D_GET_BIT_DEPTH,

  VP9_SET_BYTE_ALIGNMENT,

  VP9_INVERT_TILE_DECODE_ORDER,

  VP9_SET_SKIP_LOOP_FILTER,

  VP9_DECODE_SVC_SPATIAL_LAYER,

  VPXD_GET_LAST_QUANTIZER,

  VP9D_SET_ROW_MT,

  VP9D_SET_LOOP_FILTER_OPT,

  VP8_DECODER_CTRL_ID_MAX
};

typedef void (*vpx_decrypt_cb)(void *decrypt_state, const unsigned char *input,
                               unsigned char *output, int count);

typedef struct vpx_decrypt_init {

  vpx_decrypt_cb decrypt_cb;

  void *decrypt_state;
} vpx_decrypt_init;

VPX_CTRL_USE_TYPE(VP8D_GET_LAST_REF_UPDATES, int *)
#define VPX_CTRL_VP8D_GET_LAST_REF_UPDATES
VPX_CTRL_USE_TYPE(VP8D_GET_FRAME_CORRUPTED, int *)
#define VPX_CTRL_VP8D_GET_FRAME_CORRUPTED
VPX_CTRL_USE_TYPE(VP8D_GET_LAST_REF_USED, int *)
#define VPX_CTRL_VP8D_GET_LAST_REF_USED
VPX_CTRL_USE_TYPE(VPXD_SET_DECRYPTOR, vpx_decrypt_init *)
#define VPX_CTRL_VPXD_SET_DECRYPTOR
VPX_CTRL_USE_TYPE(VP8D_SET_DECRYPTOR, vpx_decrypt_init *)
#define VPX_CTRL_VP8D_SET_DECRYPTOR
VPX_CTRL_USE_TYPE(VP9D_GET_FRAME_SIZE, int *)
#define VPX_CTRL_VP9D_GET_FRAME_SIZE
VPX_CTRL_USE_TYPE(VP9D_GET_DISPLAY_SIZE, int *)
#define VPX_CTRL_VP9D_GET_DISPLAY_SIZE
VPX_CTRL_USE_TYPE(VP9D_GET_BIT_DEPTH, unsigned int *)
#define VPX_CTRL_VP9D_GET_BIT_DEPTH
VPX_CTRL_USE_TYPE(VP9_SET_BYTE_ALIGNMENT, int)
#define VPX_CTRL_VP9_SET_BYTE_ALIGNMENT
VPX_CTRL_USE_TYPE(VP9_INVERT_TILE_DECODE_ORDER, int)
#define VPX_CTRL_VP9_INVERT_TILE_DECODE_ORDER
VPX_CTRL_USE_TYPE(VP9_SET_SKIP_LOOP_FILTER, int)
#define VPX_CTRL_VP9_SET_SKIP_LOOP_FILTER
VPX_CTRL_USE_TYPE(VP9_DECODE_SVC_SPATIAL_LAYER, int)
#define VPX_CTRL_VP9_DECODE_SVC_SPATIAL_LAYER
VPX_CTRL_USE_TYPE(VPXD_GET_LAST_QUANTIZER, int *)
#define VPX_CTRL_VPXD_GET_LAST_QUANTIZER
VPX_CTRL_USE_TYPE(VP9D_SET_ROW_MT, int)
#define VPX_CTRL_VP9_DECODE_SET_ROW_MT
VPX_CTRL_USE_TYPE(VP9D_SET_LOOP_FILTER_OPT, int)
#define VPX_CTRL_VP9_SET_LOOP_FILTER_OPT

#ifdef __cplusplus
}
#endif

#endif
