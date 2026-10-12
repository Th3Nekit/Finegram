/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

/*!\defgroup codec Common Algorithm Interface
 * This abstraction allows applications to easily support multiple video
 * formats with minimal code duplication. This section describes the interface
 * common to all codecs (both encoders and decoders).
 * @{
 */

/*!\file
 * \brief Describes the codec algorithm interface to applications.
 *
 * This file describes the interface between an application and a
 * video codec algorithm.
 *
 * An application instantiates a specific codec instance by using
 * vpx_codec_dec_init() or vpx_codec_enc_init() and a pointer to the
 * algorithm's interface structure:
 *     <pre>
 *     my_app.c:
 *       extern vpx_codec_iface_t my_codec;
 *       {
 *           vpx_codec_ctx_t algo;
 *           int threads = 4;
 *           vpx_codec_dec_cfg_t cfg = { threads, 0, 0 };
 *           res = vpx_codec_dec_init(&algo, &my_codec, &cfg, 0);
 *       }
 *     </pre>
 *
 * Once initialized, the instance is manged using other functions from
 * the vpx_codec_* family.
 */
#ifndef VPX_VPX_VPX_CODEC_H_
#define VPX_VPX_VPX_CODEC_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "./vpx_image.h"
#include "./vpx_integer.h"

#ifndef VPX_DEPRECATED
#if defined(__GNUC__)
#define VPX_DEPRECATED __attribute__((deprecated))
#elif defined(_MSC_VER)
#define VPX_DEPRECATED
#else
#define VPX_DEPRECATED
#endif
#endif

#ifndef VPX_DECLSPEC_DEPRECATED
#if defined(__GNUC__)
#define VPX_DECLSPEC_DEPRECATED
#elif defined(_MSC_VER)

#define VPX_DECLSPEC_DEPRECATED __declspec(deprecated)
#else
#define VPX_DECLSPEC_DEPRECATED
#endif
#endif

#ifndef VPX_UNUSED
#if defined(__GNUC__) || defined(__clang__)
#define VPX_UNUSED __attribute__((unused))
#else
#define VPX_UNUSED
#endif
#endif

#define VPX_CODEC_ABI_VERSION (4 + VPX_IMAGE_ABI_VERSION)

typedef enum {

  VPX_CODEC_OK,

  VPX_CODEC_ERROR,

  VPX_CODEC_MEM_ERROR,

  VPX_CODEC_ABI_MISMATCH,

  VPX_CODEC_INCAPABLE,

  VPX_CODEC_UNSUP_BITSTREAM,

  VPX_CODEC_UNSUP_FEATURE,

  VPX_CODEC_CORRUPT_FRAME,

  VPX_CODEC_INVALID_PARAM,

  VPX_CODEC_LIST_END

} vpx_codec_err_t;

typedef long vpx_codec_caps_t;
#define VPX_CODEC_CAP_DECODER 0x1
#define VPX_CODEC_CAP_ENCODER 0x2

#define VPX_CODEC_CAP_HIGHBITDEPTH 0x4

typedef long vpx_codec_flags_t;

typedef const struct vpx_codec_iface vpx_codec_iface_t;

typedef struct vpx_codec_priv vpx_codec_priv_t;

typedef const void *vpx_codec_iter_t;

typedef struct vpx_codec_ctx {
  const char *name;
  vpx_codec_iface_t *iface;
  vpx_codec_err_t err;
  const char *err_detail;
  vpx_codec_flags_t init_flags;
  union {

    const struct vpx_codec_dec_cfg *dec;

    const struct vpx_codec_enc_cfg *enc;
    const void *raw;
  } config;
  vpx_codec_priv_t *priv;
} vpx_codec_ctx_t;

typedef enum vpx_bit_depth {
  VPX_BITS_8 = 8,
  VPX_BITS_10 = 10,
  VPX_BITS_12 = 12,
} vpx_bit_depth_t;

int vpx_codec_version(void);
#define VPX_VERSION_MAJOR(v) \
  (((v) >> 16) & 0xff)
#define VPX_VERSION_MINOR(v) \
  (((v) >> 8) & 0xff)
#define VPX_VERSION_PATCH(v) \
  (((v) >> 0) & 0xff)

#define vpx_codec_version_major() ((vpx_codec_version() >> 16) & 0xff)

#define vpx_codec_version_minor() ((vpx_codec_version() >> 8) & 0xff)

#define vpx_codec_version_patch() ((vpx_codec_version() >> 0) & 0xff)

const char *vpx_codec_version_str(void);

const char *vpx_codec_version_extra_str(void);

const char *vpx_codec_build_config(void);

const char *vpx_codec_iface_name(vpx_codec_iface_t *iface);

const char *vpx_codec_err_to_string(vpx_codec_err_t err);

const char *vpx_codec_error(const vpx_codec_ctx_t *ctx);

const char *vpx_codec_error_detail(const vpx_codec_ctx_t *ctx);

vpx_codec_err_t vpx_codec_destroy(vpx_codec_ctx_t *ctx);

vpx_codec_caps_t vpx_codec_get_caps(vpx_codec_iface_t *iface);

vpx_codec_err_t vpx_codec_control_(vpx_codec_ctx_t *ctx, int ctrl_id, ...);
#if defined(VPX_DISABLE_CTRL_TYPECHECKS) && VPX_DISABLE_CTRL_TYPECHECKS
#define vpx_codec_control(ctx, id, data) vpx_codec_control_(ctx, id, data)
#define VPX_CTRL_USE_TYPE(id, typ)
#define VPX_CTRL_USE_TYPE_DEPRECATED(id, typ)
#define VPX_CTRL_VOID(id, typ)

#else

#define vpx_codec_control(ctx, id, data) \
  vpx_codec_control_##id(ctx, id, data)

#define VPX_CTRL_USE_TYPE(id, typ)                                           \
  static vpx_codec_err_t vpx_codec_control_##id(vpx_codec_ctx_t *, int, typ) \
      VPX_UNUSED;                                                            \
                                                                             \
  static vpx_codec_err_t vpx_codec_control_##id(vpx_codec_ctx_t *ctx,        \
                                                int ctrl_id, typ data) {     \
    return vpx_codec_control_(ctx, ctrl_id, data);                           \
  }                       

#define VPX_CTRL_USE_TYPE_DEPRECATED(id, typ)                            \
  VPX_DECLSPEC_DEPRECATED static vpx_codec_err_t vpx_codec_control_##id( \
      vpx_codec_ctx_t *, int, typ) VPX_DEPRECATED VPX_UNUSED;            \
                                                                         \
  VPX_DECLSPEC_DEPRECATED static vpx_codec_err_t vpx_codec_control_##id( \
      vpx_codec_ctx_t *ctx, int ctrl_id, typ data) {                     \
    return vpx_codec_control_(ctx, ctrl_id, data);                       \
  }                       

#define VPX_CTRL_VOID(id)                                               \
  static vpx_codec_err_t vpx_codec_control_##id(vpx_codec_ctx_t *, int) \
      VPX_UNUSED;                                                       \
                                                                        \
  static vpx_codec_err_t vpx_codec_control_##id(vpx_codec_ctx_t *ctx,   \
                                                int ctrl_id) {          \
    return vpx_codec_control_(ctx, ctrl_id);                            \
  }                       

#endif

#ifdef __cplusplus
}
#endif
#endif
