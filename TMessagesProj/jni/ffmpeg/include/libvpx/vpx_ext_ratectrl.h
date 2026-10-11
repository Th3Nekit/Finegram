/*
 *  Copyright (c) 2020 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

/*!\file
 * \brief Defines structs and callbacks needed for external rate control.
 *
 */
#ifndef VPX_VPX_VPX_EXT_RATECTRL_H_
#define VPX_VPX_VPX_EXT_RATECTRL_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "./vpx_integer.h"
#include "./vpx_tpl.h"

#define VPX_EXT_RATECTRL_ABI_VERSION (7 + VPX_TPL_ABI_VERSION)

#define VPX_RC_MAX_STATIC_GF_GROUP_LENGTH 250

#define VPX_RC_MAX_REF_FRAMES 4

typedef enum vpx_rc_type {

  VPX_RC_NONE = 0,

  VPX_RC_QP = 1 << 0,

  VPX_RC_GOP = 1 << 1,

  VPX_RC_RDMULT = 1 << 2,

  VPX_RC_GOP_QP = VPX_RC_QP | VPX_RC_GOP,

  VPX_RC_GOP_QP_RDMULT = VPX_RC_QP | VPX_RC_GOP | VPX_RC_RDMULT
} vpx_rc_type_t;

typedef enum vpx_ext_rc_mode {
  VPX_RC_QMODE = 0,
  VPX_RC_VBR = 1,
  VPX_RC_CQ = 2,
} vpx_ext_rc_mode_t;

typedef enum vpx_rc_frame_update_type {
  VPX_RC_INVALID_UPDATE_TYPE = -1,
  VPX_RC_KF_UPDATE = 0,
  VPX_RC_LF_UPDATE = 1,
  VPX_RC_GF_UPDATE = 2,
  VPX_RC_ARF_UPDATE = 3,
  VPX_RC_OVERLAY_UPDATE = 4,
  VPX_RC_MID_OVERLAY_UPDATE = 5,
  VPX_RC_USE_BUF_FRAME = 6,
} vpx_rc_frame_update_type_t;

typedef enum vpx_rc_ref_name {
  VPX_RC_INVALID_REF_FRAME = -1,
  VPX_RC_INTRA_FRAME = 0,
  VPX_RC_LAST_FRAME = 1,
  VPX_RC_GOLDEN_FRAME = 2,
  VPX_RC_ALTREF_FRAME = 3,
} vpx_rc_ref_name_t;

typedef void *vpx_rc_model_t;

#define VPX_DEFAULT_Q -1

#define VPX_DEFAULT_RDMULT -1

typedef struct sb_parameters {
  int q_index;
  int rdmult;
} sb_params;

typedef struct vpx_rc_encodeframe_decision {
  int q_index;
  int rdmult;
  int delta_q_uv;

  sb_params *sb_params_list;
} vpx_rc_encodeframe_decision_t;

typedef struct vpx_rc_encodeframe_info {

  int frame_type;
  int show_index;
  int coding_index;

  int gop_index;
  int ref_frame_coding_indexes[3];

  int ref_frame_valid_list[3];

  int gop_size;

  int use_alt_ref;
} vpx_rc_encodeframe_info_t;

typedef struct vpx_rc_encodeframe_result {
  int64_t bit_count;
  int actual_encoding_qindex;
} vpx_rc_encodeframe_result_t;

typedef enum vpx_rc_status {
  VPX_RC_OK = 0,
  VPX_RC_ERROR = 1,
} vpx_rc_status_t;

typedef struct vpx_rc_frame_stats {

  double frame;

  double weight;

  double intra_error;

  double coded_error;

  double sr_coded_error;

  double frame_noise_energy;

  double pcnt_inter;

  double pcnt_motion;

  double pcnt_second_ref;

  double pcnt_neutral;

  double pcnt_intra_low;

  double pcnt_intra_high;

  double intra_skip_pct;

  double intra_smooth_pct;

  double inactive_zone_rows;

  double inactive_zone_cols;

  double MVr;

  double mvr_abs;

  double MVc;

  double mvc_abs;

  double MVrv;

  double MVcv;

  double mv_in_out_count;

  double duration;

  double count;

  double new_mv_count;
} vpx_rc_frame_stats_t;

typedef struct vpx_rc_firstpass_stats {

  vpx_rc_frame_stats_t *frame_stats;

  int num_frames;
} vpx_rc_firstpass_stats_t;

typedef struct vpx_rc_config {
  int frame_width;
  int frame_height;
  int show_frame_count;
  int max_gf_interval;
  int min_gf_interval;

  int target_bitrate_kbps;
  int frame_rate_num;
  int frame_rate_den;

  vpx_ext_rc_mode_t rc_mode;
  int overshoot_percent;
  int undershoot_percent;
  int min_base_q_index;
  int max_base_q_index;
  int base_qp;
} vpx_rc_config_t;

typedef struct vpx_rc_ref_frame {

  int index[VPX_RC_MAX_REF_FRAMES];

  vpx_rc_ref_name_t name[VPX_RC_MAX_REF_FRAMES];
} vpx_rc_ref_frame_t;

typedef struct vpx_rc_gop_decision {
  int gop_coding_frames;
  int use_alt_ref;
  int use_key_frame;

  vpx_rc_frame_update_type_t update_type[VPX_RC_MAX_STATIC_GF_GROUP_LENGTH + 2];

  int update_ref_index[VPX_RC_MAX_STATIC_GF_GROUP_LENGTH + 2];

  vpx_rc_ref_frame_t ref_frame_list[VPX_RC_MAX_STATIC_GF_GROUP_LENGTH + 2];
} vpx_rc_gop_decision_t;

typedef struct vpx_rc_key_frame_decision {
  int key_frame_show_index;
  int key_frame_group_size;
} vpx_rc_key_frame_decision_t;

typedef vpx_rc_status_t (*vpx_rc_create_model_cb_fn_t)(
    void *priv, const vpx_rc_config_t *ratectrl_config,
    vpx_rc_model_t *rate_ctrl_model_ptr);

typedef vpx_rc_status_t (*vpx_rc_send_firstpass_stats_cb_fn_t)(
    vpx_rc_model_t rate_ctrl_model,
    const vpx_rc_firstpass_stats_t *first_pass_stats);

typedef vpx_rc_status_t (*vpx_rc_send_tpl_gop_stats_cb_fn_t)(
    vpx_rc_model_t rate_ctrl_model, const VpxTplGopStats *tpl_gop_stats);

typedef vpx_rc_status_t (*vpx_rc_get_encodeframe_decision_cb_fn_t)(
    vpx_rc_model_t rate_ctrl_model, const int frame_gop_index,
    vpx_rc_encodeframe_decision_t *frame_decision);

typedef vpx_rc_status_t (*vpx_rc_update_encodeframe_result_cb_fn_t)(
    vpx_rc_model_t rate_ctrl_model,
    const vpx_rc_encodeframe_result_t *encode_frame_result);

typedef vpx_rc_status_t (*vpx_rc_get_key_frame_decision_cb_fn_t)(
    vpx_rc_model_t rate_ctrl_model,
    vpx_rc_key_frame_decision_t *key_frame_decision);

typedef vpx_rc_status_t (*vpx_rc_get_gop_decision_cb_fn_t)(
    vpx_rc_model_t rate_ctrl_model, vpx_rc_gop_decision_t *gop_decision);

typedef vpx_rc_status_t (*vpx_rc_get_frame_rdmult_cb_fn_t)(
    vpx_rc_model_t rate_ctrl_model, const vpx_rc_encodeframe_info_t *frame_info,
    int *rdmult);

typedef vpx_rc_status_t (*vpx_rc_delete_model_cb_fn_t)(
    vpx_rc_model_t rate_ctrl_model);

typedef struct vpx_rc_funcs {

  vpx_rc_type_t rc_type;

  vpx_rc_create_model_cb_fn_t create_model;

  vpx_rc_send_firstpass_stats_cb_fn_t send_firstpass_stats;

  vpx_rc_send_tpl_gop_stats_cb_fn_t send_tpl_gop_stats;

  vpx_rc_get_encodeframe_decision_cb_fn_t get_encodeframe_decision;

  vpx_rc_update_encodeframe_result_cb_fn_t update_encodeframe_result;

  vpx_rc_get_key_frame_decision_cb_fn_t get_key_frame_decision;

  vpx_rc_get_gop_decision_cb_fn_t get_gop_decision;

  vpx_rc_get_frame_rdmult_cb_fn_t get_frame_rdmult;

  vpx_rc_delete_model_cb_fn_t delete_model;

  const char *rate_ctrl_log_path;

  void *priv;
} vpx_rc_funcs_t;

#ifdef __cplusplus
}
#endif

#endif
