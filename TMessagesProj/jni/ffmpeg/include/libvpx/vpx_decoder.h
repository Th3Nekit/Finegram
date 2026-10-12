/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef VPX_VPX_VPX_DECODER_H_
#define VPX_VPX_VPX_DECODER_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "./vpx_codec.h"
#include "./vpx_frame_buffer.h"

#define VPX_DECODER_ABI_VERSION \
  (3 + VPX_CODEC_ABI_VERSION)

#define VPX_CODEC_CAP_PUT_SLICE 0x10000
#define VPX_CODEC_CAP_PUT_FRAME 0x20000
#define VPX_CODEC_CAP_POSTPROC 0x40000

#define VPX_CODEC_CAP_ERROR_CONCEALMENT 0x80000

#define VPX_CODEC_CAP_INPUT_FRAGMENTS 0x100000

#define VPX_CODEC_CAP_FRAME_THREADING 0x200000

#define VPX_CODEC_CAP_EXTERNAL_FRAME_BUFFER 0x400000

#define VPX_CODEC_USE_POSTPROC 0x10000

#define VPX_CODEC_USE_ERROR_CONCEALMENT 0x20000

#define VPX_CODEC_USE_INPUT_FRAGMENTS 0x40000

#define VPX_CODEC_USE_FRAME_THREADING 0x80000

typedef struct vpx_codec_stream_info {
  unsigned int sz;
  unsigned int w;
  unsigned int h;
  unsigned int is_kf;
} vpx_codec_stream_info_t;

typedef struct vpx_codec_dec_cfg {
  unsigned int threads;
  unsigned int w;
  unsigned int h;
} vpx_codec_dec_cfg_t;

vpx_codec_err_t vpx_codec_dec_init_ver(vpx_codec_ctx_t *ctx,
                                       vpx_codec_iface_t *iface,
                                       const vpx_codec_dec_cfg_t *cfg,
                                       vpx_codec_flags_t flags, int ver);

#define vpx_codec_dec_init(ctx, iface, cfg, flags) \
  vpx_codec_dec_init_ver(ctx, iface, cfg, flags, VPX_DECODER_ABI_VERSION)

vpx_codec_err_t vpx_codec_peek_stream_info(vpx_codec_iface_t *iface,
                                           const uint8_t *data,
                                           unsigned int data_sz,
                                           vpx_codec_stream_info_t *si);

vpx_codec_err_t vpx_codec_get_stream_info(vpx_codec_ctx_t *ctx,
                                          vpx_codec_stream_info_t *si);

vpx_codec_err_t vpx_codec_decode(vpx_codec_ctx_t *ctx, const uint8_t *data,
                                 unsigned int data_sz, void *user_priv,
                                 long deadline);

vpx_image_t *vpx_codec_get_frame(vpx_codec_ctx_t *ctx, vpx_codec_iter_t *iter);

typedef void (*vpx_codec_put_frame_cb_fn_t)(void *user_priv,
                                            const vpx_image_t *img);

vpx_codec_err_t vpx_codec_register_put_frame_cb(vpx_codec_ctx_t *ctx,
                                                vpx_codec_put_frame_cb_fn_t cb,
                                                void *user_priv);

typedef void (*vpx_codec_put_slice_cb_fn_t)(void *user_priv,
                                            const vpx_image_t *img,
                                            const vpx_image_rect_t *valid,
                                            const vpx_image_rect_t *update);

vpx_codec_err_t vpx_codec_register_put_slice_cb(vpx_codec_ctx_t *ctx,
                                                vpx_codec_put_slice_cb_fn_t cb,
                                                void *user_priv);

vpx_codec_err_t vpx_codec_set_frame_buffer_functions(
    vpx_codec_ctx_t *ctx, vpx_get_frame_buffer_cb_fn_t cb_get,
    vpx_release_frame_buffer_cb_fn_t cb_release, void *cb_priv);

#ifdef __cplusplus
}
#endif
#endif
