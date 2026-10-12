/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_AECM_AECM_DEFINES_H_
#define MODULES_AUDIO_PROCESSING_AECM_AECM_DEFINES_H_

#define AECM_DYNAMIC_Q

#define FRAME_LEN 80

#define PART_LEN 64
#define PART_LEN_SHIFT 7

#define PART_LEN1 (PART_LEN + 1)
#define PART_LEN2 (PART_LEN << 1)
#define PART_LEN4 (PART_LEN << 2)
#define FAR_BUF_LEN PART_LEN4
#define MAX_DELAY 100

#define CONV_LEN 512
#define CONV_LEN2 (CONV_LEN << 1)

#define MAX_BUF_LEN 64
#define FAR_ENERGY_MIN 1025

#define FAR_ENERGY_DIFF 929

#define ENERGY_DEV_OFFSET 0
#define ENERGY_DEV_TOL 400
#define FAR_ENERGY_VAD_REGION 230

#define MU_MIN 10

#define MU_MAX 1

#define MU_DIFF 9

#define MIN_MSE_COUNT 20

#define MIN_MSE_DIFF 29

#define MSE_RESOLUTION 5
#define RESOLUTION_CHANNEL16 12
#define RESOLUTION_CHANNEL32 28
#define CHANNEL_VAD 16

#define RESOLUTION_SUPGAIN 8
#define SUPGAIN_DEFAULT (1 << RESOLUTION_SUPGAIN)
#define SUPGAIN_ERROR_PARAM_A 3072

#define SUPGAIN_ERROR_PARAM_B 1536

#define SUPGAIN_ERROR_PARAM_D SUPGAIN_DEFAULT

#define SUPGAIN_EPC_DT 200

#define CORR_WIDTH 31
#define CORR_MAX 16
#define CORR_MAX_BUF 63
#define CORR_DEV 4
#define CORR_MAX_LEVEL 20
#define CORR_MAX_LOW 4
#define CORR_BUF_LEN (CORR_MAX << 1) + 1

#define ONE_Q14 (1 << 14)

#define NLP_COMP_LOW 3277
#define NLP_COMP_HIGH ONE_Q14

#endif
