/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

/*
 * This header file includes all of the fix point signal processing library
 * (SPL) function descriptions and declarations. For specific function calls,
 * see bottom of file.
 */

#ifndef COMMON_AUDIO_SIGNAL_PROCESSING_INCLUDE_SIGNAL_PROCESSING_LIBRARY_H_
#define COMMON_AUDIO_SIGNAL_PROCESSING_INCLUDE_SIGNAL_PROCESSING_LIBRARY_H_

#include <string.h>

#include "common_audio/signal_processing/dot_product_with_scale.h"

#define WEBRTC_SPL_WORD16_MAX 32767
#define WEBRTC_SPL_WORD16_MIN -32768
#define WEBRTC_SPL_WORD32_MAX (int32_t)0x7fffffff
#define WEBRTC_SPL_WORD32_MIN (int32_t)0x80000000
#define WEBRTC_SPL_MAX_LPC_ORDER 14
#define WEBRTC_SPL_MIN(A, B) (A < B ? A : B)  // Get min value
#define WEBRTC_SPL_MAX(A, B) (A > B ? A : B)  // Get max value

#define WEBRTC_SPL_ABS_W16(a) (((int16_t)a >= 0) ? ((int16_t)a) : -((int16_t)a))
#define WEBRTC_SPL_ABS_W32(a) (((int32_t)a >= 0) ? ((int32_t)a) : -((int32_t)a))

#define WEBRTC_SPL_MUL(a, b) ((int32_t)((int32_t)(a) * (int32_t)(b)))
#define WEBRTC_SPL_UMUL(a, b) ((uint32_t)((uint32_t)(a) * (uint32_t)(b)))
#define WEBRTC_SPL_UMUL_32_16(a, b) ((uint32_t)((uint32_t)(a) * (uint16_t)(b)))
#define WEBRTC_SPL_MUL_16_U16(a, b) ((int32_t)(int16_t)(a) * (uint16_t)(b))

#ifndef WEBRTC_ARCH_ARM_V7

#ifndef MIPS32_LE

#define WEBRTC_SPL_MUL_16_16(a, b) ((int32_t)(((int16_t)(a)) * ((int16_t)(b))))
#define WEBRTC_SPL_MUL_16_32_RSFT16(a, b) \
        (WEBRTC_SPL_MUL_16_16(a, b >> 16) +     \
        ((WEBRTC_SPL_MUL_16_16(a, (b & 0xffff) >> 1) + 0x4000) >> 15))
#endif
#endif

#define WEBRTC_SPL_MUL_16_32_RSFT11(a, b)          \
        (WEBRTC_SPL_MUL_16_16(a, (b) >> 16) * (1 << 5) + \
        (((WEBRTC_SPL_MUL_16_U16(a, (uint16_t)(b)) >> 1) + 0x0200) >> 10))
#define WEBRTC_SPL_MUL_16_32_RSFT14(a, b)          \
        (WEBRTC_SPL_MUL_16_16(a, (b) >> 16) * (1 << 2) + \
        (((WEBRTC_SPL_MUL_16_U16(a, (uint16_t)(b)) >> 1) + 0x1000) >> 13))
#define WEBRTC_SPL_MUL_16_32_RSFT15(a, b)            \
        ((WEBRTC_SPL_MUL_16_16(a, (b) >> 16) * (1 << 1)) + \
        (((WEBRTC_SPL_MUL_16_U16(a, (uint16_t)(b)) >> 1) + 0x2000) >> 14))

#define WEBRTC_SPL_MUL_16_16_RSFT(a, b, c) (WEBRTC_SPL_MUL_16_16(a, b) >> (c))

#define WEBRTC_SPL_MUL_16_16_RSFT_WITH_ROUND(a, b, c) \
  ((WEBRTC_SPL_MUL_16_16(a, b) + ((int32_t)(((int32_t)1) << ((c)-1)))) >> (c))

#define WEBRTC_SPL_SCALEDIFF32(A, B, C) \
  (C + (B >> 16) * A + (((uint32_t)(B & 0x0000FFFF) * A) >> 16))

#define WEBRTC_SPL_SAT(a, b, c) (b > a ? a : b < c ? c : b)

#define WEBRTC_SPL_SHIFT_W32(x, c) ((c) >= 0 ? (x) * (1 << (c)) : (x) >> -(c))

#define WEBRTC_SPL_LSHIFT_W32(x, c) ((x) << (c))

#define WEBRTC_SPL_RSHIFT_U32(x, c) ((uint32_t)(x) >> (c))

#define WEBRTC_SPL_RAND(a) ((int16_t)((((int16_t)a * 18816) >> 7) & 0x00007fff))

#ifdef __cplusplus
extern "C" {
#endif

#define WEBRTC_SPL_MEMCPY_W16(v1, v2, length) \
  memcpy(v1, v2, (length) * sizeof(int16_t))

#include "common_audio/signal_processing/include/spl_inl.h"

#include "common_audio/third_party/spl_sqrt_floor/spl_sqrt_floor.h"

int16_t WebRtcSpl_GetScalingSquare(int16_t* in_vector,
                                   size_t in_vector_length,
                                   size_t times);

void WebRtcSpl_MemSetW16(int16_t* vector,
                         int16_t set_value,
                         size_t vector_length);
void WebRtcSpl_MemSetW32(int32_t* vector,
                         int32_t set_value,
                         size_t vector_length);
void WebRtcSpl_MemCpyReversedOrder(int16_t* out_vector,
                                   int16_t* in_vector,
                                   size_t vector_length);
void WebRtcSpl_CopyFromEndW16(const int16_t* in_vector,
                              size_t in_vector_length,
                              size_t samples,
                              int16_t* out_vector);
void WebRtcSpl_ZerosArrayW16(int16_t* vector, size_t vector_length);
void WebRtcSpl_ZerosArrayW32(int32_t* vector, size_t vector_length);

typedef int16_t (*MaxAbsValueW16)(const int16_t* vector, size_t length);
extern const MaxAbsValueW16 WebRtcSpl_MaxAbsValueW16;
int16_t WebRtcSpl_MaxAbsValueW16C(const int16_t* vector, size_t length);
#if defined(WEBRTC_HAS_NEON)
int16_t WebRtcSpl_MaxAbsValueW16Neon(const int16_t* vector, size_t length);
#endif
#if defined(MIPS32_LE)
int16_t WebRtcSpl_MaxAbsValueW16_mips(const int16_t* vector, size_t length);
#endif

typedef int32_t (*MaxAbsValueW32)(const int32_t* vector, size_t length);
extern const MaxAbsValueW32 WebRtcSpl_MaxAbsValueW32;
int32_t WebRtcSpl_MaxAbsValueW32C(const int32_t* vector, size_t length);
#if defined(WEBRTC_HAS_NEON)
int32_t WebRtcSpl_MaxAbsValueW32Neon(const int32_t* vector, size_t length);
#endif
#if defined(MIPS_DSP_R1_LE)
int32_t WebRtcSpl_MaxAbsValueW32_mips(const int32_t* vector, size_t length);
#endif

typedef int16_t (*MaxValueW16)(const int16_t* vector, size_t length);
extern const MaxValueW16 WebRtcSpl_MaxValueW16;
int16_t WebRtcSpl_MaxValueW16C(const int16_t* vector, size_t length);
#if defined(WEBRTC_HAS_NEON)
int16_t WebRtcSpl_MaxValueW16Neon(const int16_t* vector, size_t length);
#endif
#if defined(MIPS32_LE)
int16_t WebRtcSpl_MaxValueW16_mips(const int16_t* vector, size_t length);
#endif

typedef int32_t (*MaxValueW32)(const int32_t* vector, size_t length);
extern const MaxValueW32 WebRtcSpl_MaxValueW32;
int32_t WebRtcSpl_MaxValueW32C(const int32_t* vector, size_t length);
#if defined(WEBRTC_HAS_NEON)
int32_t WebRtcSpl_MaxValueW32Neon(const int32_t* vector, size_t length);
#endif
#if defined(MIPS32_LE)
int32_t WebRtcSpl_MaxValueW32_mips(const int32_t* vector, size_t length);
#endif

typedef int16_t (*MinValueW16)(const int16_t* vector, size_t length);
extern const MinValueW16 WebRtcSpl_MinValueW16;
int16_t WebRtcSpl_MinValueW16C(const int16_t* vector, size_t length);
#if defined(WEBRTC_HAS_NEON)
int16_t WebRtcSpl_MinValueW16Neon(const int16_t* vector, size_t length);
#endif
#if defined(MIPS32_LE)
int16_t WebRtcSpl_MinValueW16_mips(const int16_t* vector, size_t length);
#endif

typedef int32_t (*MinValueW32)(const int32_t* vector, size_t length);
extern const MinValueW32 WebRtcSpl_MinValueW32;
int32_t WebRtcSpl_MinValueW32C(const int32_t* vector, size_t length);
#if defined(WEBRTC_HAS_NEON)
int32_t WebRtcSpl_MinValueW32Neon(const int32_t* vector, size_t length);
#endif
#if defined(MIPS32_LE)
int32_t WebRtcSpl_MinValueW32_mips(const int32_t* vector, size_t length);
#endif

void WebRtcSpl_MinMaxW16(const int16_t* vector,
                         size_t length,
                         int16_t* min_val,
                         int16_t* max_val);
#if defined(WEBRTC_HAS_NEON)
void WebRtcSpl_MinMaxW16Neon(const int16_t* vector,
                             size_t length,
                             int16_t* min_val,
                             int16_t* max_val);
#endif

size_t WebRtcSpl_MaxAbsIndexW16(const int16_t* vector, size_t length);

int16_t WebRtcSpl_MaxAbsElementW16(const int16_t* vector, size_t length);

size_t WebRtcSpl_MaxIndexW16(const int16_t* vector, size_t length);

size_t WebRtcSpl_MaxIndexW32(const int32_t* vector, size_t length);

size_t WebRtcSpl_MinIndexW16(const int16_t* vector, size_t length);

size_t WebRtcSpl_MinIndexW32(const int32_t* vector, size_t length);

void WebRtcSpl_VectorBitShiftW16(int16_t* out_vector,
                                 size_t vector_length,
                                 const int16_t* in_vector,
                                 int16_t right_shifts);
void WebRtcSpl_VectorBitShiftW32(int32_t* out_vector,
                                 size_t vector_length,
                                 const int32_t* in_vector,
                                 int16_t right_shifts);
void WebRtcSpl_VectorBitShiftW32ToW16(int16_t* out_vector,
                                      size_t vector_length,
                                      const int32_t* in_vector,
                                      int right_shifts);
void WebRtcSpl_ScaleVector(const int16_t* in_vector,
                           int16_t* out_vector,
                           int16_t gain,
                           size_t vector_length,
                           int16_t right_shifts);
void WebRtcSpl_ScaleVectorWithSat(const int16_t* in_vector,
                                  int16_t* out_vector,
                                  int16_t gain,
                                  size_t vector_length,
                                  int16_t right_shifts);
void WebRtcSpl_ScaleAndAddVectors(const int16_t* in_vector1,
                                  int16_t gain1,
                                  int right_shifts1,
                                  const int16_t* in_vector2,
                                  int16_t gain2,
                                  int right_shifts2,
                                  int16_t* out_vector,
                                  size_t vector_length);

typedef int (*ScaleAndAddVectorsWithRound)(const int16_t* in_vector1,
                                           int16_t in_vector1_scale,
                                           const int16_t* in_vector2,
                                           int16_t in_vector2_scale,
                                           int right_shifts,
                                           int16_t* out_vector,
                                           size_t length);
extern const ScaleAndAddVectorsWithRound WebRtcSpl_ScaleAndAddVectorsWithRound;
int WebRtcSpl_ScaleAndAddVectorsWithRoundC(const int16_t* in_vector1,
                                           int16_t in_vector1_scale,
                                           const int16_t* in_vector2,
                                           int16_t in_vector2_scale,
                                           int right_shifts,
                                           int16_t* out_vector,
                                           size_t length);
#if defined(MIPS_DSP_R1_LE)
int WebRtcSpl_ScaleAndAddVectorsWithRound_mips(const int16_t* in_vector1,
                                               int16_t in_vector1_scale,
                                               const int16_t* in_vector2,
                                               int16_t in_vector2_scale,
                                               int right_shifts,
                                               int16_t* out_vector,
                                               size_t length);
#endif

void WebRtcSpl_ReverseOrderMultArrayElements(int16_t* out_vector,
                                             const int16_t* in_vector,
                                             const int16_t* window,
                                             size_t vector_length,
                                             int16_t right_shifts);
void WebRtcSpl_ElementwiseVectorMult(int16_t* out_vector,
                                     const int16_t* in_vector,
                                     const int16_t* window,
                                     size_t vector_length,
                                     int16_t right_shifts);
void WebRtcSpl_AddVectorsAndShift(int16_t* out_vector,
                                  const int16_t* in_vector1,
                                  const int16_t* in_vector2,
                                  size_t vector_length,
                                  int16_t right_shifts);
void WebRtcSpl_AddAffineVectorToVector(int16_t* out_vector,
                                       const int16_t* in_vector,
                                       int16_t gain,
                                       int32_t add_constant,
                                       int16_t right_shifts,
                                       size_t vector_length);
void WebRtcSpl_AffineTransformVector(int16_t* out_vector,
                                     const int16_t* in_vector,
                                     int16_t gain,
                                     int32_t add_constant,
                                     int16_t right_shifts,
                                     size_t vector_length);

size_t WebRtcSpl_AutoCorrelation(const int16_t* in_vector,
                                 size_t in_vector_length,
                                 size_t order,
                                 int32_t* result,
                                 int* scale);

int16_t WebRtcSpl_LevinsonDurbin(const int32_t* auto_corr,
                                 int16_t* lpc_coef,
                                 int16_t* refl_coef,
                                 size_t order);

void WebRtcSpl_ReflCoefToLpc(const int16_t* refl_coef,
                             int use_order,
                             int16_t* lpc_coef);

void WebRtcSpl_LpcToReflCoef(int16_t* lpc_coef,
                             int use_order,
                             int16_t* refl_coef);

void WebRtcSpl_AutoCorrToReflCoef(const int32_t* auto_corr,
                                  int use_order,
                                  int16_t* refl_coef);

typedef void (*CrossCorrelation)(int32_t* cross_correlation,
                                 const int16_t* seq1,
                                 const int16_t* seq2,
                                 size_t dim_seq,
                                 size_t dim_cross_correlation,
                                 int right_shifts,
                                 int step_seq2);
extern const CrossCorrelation WebRtcSpl_CrossCorrelation;
void WebRtcSpl_CrossCorrelationC(int32_t* cross_correlation,
                                 const int16_t* seq1,
                                 const int16_t* seq2,
                                 size_t dim_seq,
                                 size_t dim_cross_correlation,
                                 int right_shifts,
                                 int step_seq2);
#if defined(WEBRTC_HAS_NEON)
void WebRtcSpl_CrossCorrelationNeon(int32_t* cross_correlation,
                                    const int16_t* seq1,
                                    const int16_t* seq2,
                                    size_t dim_seq,
                                    size_t dim_cross_correlation,
                                    int right_shifts,
                                    int step_seq2);
#endif
#if defined(MIPS32_LE)
void WebRtcSpl_CrossCorrelation_mips(int32_t* cross_correlation,
                                     const int16_t* seq1,
                                     const int16_t* seq2,
                                     size_t dim_seq,
                                     size_t dim_cross_correlation,
                                     int right_shifts,
                                     int step_seq2);
#endif

void WebRtcSpl_GetHanningWindow(int16_t* window, size_t size);

void WebRtcSpl_SqrtOfOneMinusXSquared(int16_t* in_vector,
                                      size_t vector_length,
                                      int16_t* out_vector);

int16_t WebRtcSpl_RandU(uint32_t* seed);
int16_t WebRtcSpl_RandN(uint32_t* seed);
int16_t WebRtcSpl_RandUArray(int16_t* vector,
                             int16_t vector_length,
                             uint32_t* seed);

int32_t WebRtcSpl_Sqrt(int32_t value);

uint32_t WebRtcSpl_DivU32U16(uint32_t num, uint16_t den);
int32_t WebRtcSpl_DivW32W16(int32_t num, int16_t den);
int16_t WebRtcSpl_DivW32W16ResW16(int32_t num, int16_t den);
int32_t WebRtcSpl_DivResultInQ31(int32_t num, int32_t den);
int32_t WebRtcSpl_DivW32HiLow(int32_t num, int16_t den_hi, int16_t den_low);

int32_t WebRtcSpl_Energy(int16_t* vector,
                         size_t vector_length,
                         int* scale_factor);

size_t WebRtcSpl_FilterAR(const int16_t* ar_coef,
                          size_t ar_coef_length,
                          const int16_t* in_vector,
                          size_t in_vector_length,
                          int16_t* filter_state,
                          size_t filter_state_length,
                          int16_t* filter_state_low,
                          size_t filter_state_low_length,
                          int16_t* out_vector,
                          int16_t* out_vector_low,
                          size_t out_vector_low_length);

void WebRtcSpl_FilterMAFastQ12(const int16_t* in_vector,
                               int16_t* out_vector,
                               const int16_t* ma_coef,
                               size_t ma_coef_length,
                               size_t vector_length);

void WebRtcSpl_FilterARFastQ12(const int16_t* data_in,
                               int16_t* data_out,
                               const int16_t* __restrict coefficients,
                               size_t coefficients_length,
                               size_t data_length);

typedef int (*DownsampleFast)(const int16_t* data_in,
                              size_t data_in_length,
                              int16_t* data_out,
                              size_t data_out_length,
                              const int16_t* __restrict coefficients,
                              size_t coefficients_length,
                              int factor,
                              size_t delay);
extern const DownsampleFast WebRtcSpl_DownsampleFast;
int WebRtcSpl_DownsampleFastC(const int16_t* data_in,
                              size_t data_in_length,
                              int16_t* data_out,
                              size_t data_out_length,
                              const int16_t* __restrict coefficients,
                              size_t coefficients_length,
                              int factor,
                              size_t delay);
#if defined(WEBRTC_HAS_NEON)
int WebRtcSpl_DownsampleFastNeon(const int16_t* data_in,
                                 size_t data_in_length,
                                 int16_t* data_out,
                                 size_t data_out_length,
                                 const int16_t* __restrict coefficients,
                                 size_t coefficients_length,
                                 int factor,
                                 size_t delay);
#endif
#if defined(MIPS32_LE)
int WebRtcSpl_DownsampleFast_mips(const int16_t* data_in,
                                  size_t data_in_length,
                                  int16_t* data_out,
                                  size_t data_out_length,
                                  const int16_t* __restrict coefficients,
                                  size_t coefficients_length,
                                  int factor,
                                  size_t delay);
#endif

int WebRtcSpl_ComplexFFT(int16_t vector[], int stages, int mode);
int WebRtcSpl_ComplexIFFT(int16_t vector[], int stages, int mode);

void WebRtcSpl_ComplexBitReverse(int16_t* __restrict complex_data, int stages);

typedef struct {
  int32_t S_22_44[8];
  int32_t S_44_32[8];
  int32_t S_32_16[8];
} WebRtcSpl_State22khzTo16khz;

void WebRtcSpl_Resample22khzTo16khz(const int16_t* in,
                                    int16_t* out,
                                    WebRtcSpl_State22khzTo16khz* state,
                                    int32_t* tmpmem);

void WebRtcSpl_ResetResample22khzTo16khz(WebRtcSpl_State22khzTo16khz* state);

typedef struct {
  int32_t S_16_32[8];
  int32_t S_32_22[8];
} WebRtcSpl_State16khzTo22khz;

void WebRtcSpl_Resample16khzTo22khz(const int16_t* in,
                                    int16_t* out,
                                    WebRtcSpl_State16khzTo22khz* state,
                                    int32_t* tmpmem);

void WebRtcSpl_ResetResample16khzTo22khz(WebRtcSpl_State16khzTo22khz* state);

typedef struct {
  int32_t S_22_22[16];
  int32_t S_22_16[8];
  int32_t S_16_8[8];
} WebRtcSpl_State22khzTo8khz;

void WebRtcSpl_Resample22khzTo8khz(const int16_t* in,
                                   int16_t* out,
                                   WebRtcSpl_State22khzTo8khz* state,
                                   int32_t* tmpmem);

void WebRtcSpl_ResetResample22khzTo8khz(WebRtcSpl_State22khzTo8khz* state);

typedef struct {
  int32_t S_8_16[8];
  int32_t S_16_11[8];
  int32_t S_11_22[8];
} WebRtcSpl_State8khzTo22khz;

void WebRtcSpl_Resample8khzTo22khz(const int16_t* in,
                                   int16_t* out,
                                   WebRtcSpl_State8khzTo22khz* state,
                                   int32_t* tmpmem);

void WebRtcSpl_ResetResample8khzTo22khz(WebRtcSpl_State8khzTo22khz* state);

void WebRtcSpl_Resample48khzTo32khz(const int32_t* In, int32_t* Out, size_t K);

void WebRtcSpl_Resample32khzTo24khz(const int32_t* In, int32_t* Out, size_t K);

void WebRtcSpl_Resample44khzTo32khz(const int32_t* In, int32_t* Out, size_t K);

typedef struct {
  int32_t S_48_48[16];
  int32_t S_48_32[8];
  int32_t S_32_16[8];
} WebRtcSpl_State48khzTo16khz;

void WebRtcSpl_Resample48khzTo16khz(const int16_t* in,
                                    int16_t* out,
                                    WebRtcSpl_State48khzTo16khz* state,
                                    int32_t* tmpmem);

void WebRtcSpl_ResetResample48khzTo16khz(WebRtcSpl_State48khzTo16khz* state);

typedef struct {
  int32_t S_16_32[8];
  int32_t S_32_24[8];
  int32_t S_24_48[8];
} WebRtcSpl_State16khzTo48khz;

void WebRtcSpl_Resample16khzTo48khz(const int16_t* in,
                                    int16_t* out,
                                    WebRtcSpl_State16khzTo48khz* state,
                                    int32_t* tmpmem);

void WebRtcSpl_ResetResample16khzTo48khz(WebRtcSpl_State16khzTo48khz* state);

typedef struct {
  int32_t S_48_24[8];
  int32_t S_24_24[16];
  int32_t S_24_16[8];
  int32_t S_16_8[8];
} WebRtcSpl_State48khzTo8khz;

void WebRtcSpl_Resample48khzTo8khz(const int16_t* in,
                                   int16_t* out,
                                   WebRtcSpl_State48khzTo8khz* state,
                                   int32_t* tmpmem);

void WebRtcSpl_ResetResample48khzTo8khz(WebRtcSpl_State48khzTo8khz* state);

typedef struct {
  int32_t S_8_16[8];
  int32_t S_16_12[8];
  int32_t S_12_24[8];
  int32_t S_24_48[8];
} WebRtcSpl_State8khzTo48khz;

void WebRtcSpl_Resample8khzTo48khz(const int16_t* in,
                                   int16_t* out,
                                   WebRtcSpl_State8khzTo48khz* state,
                                   int32_t* tmpmem);

void WebRtcSpl_ResetResample8khzTo48khz(WebRtcSpl_State8khzTo48khz* state);

void WebRtcSpl_DownsampleBy2(const int16_t* in,
                             size_t len,
                             int16_t* out,
                             int32_t* filtState);

void WebRtcSpl_UpsampleBy2(const int16_t* in,
                           size_t len,
                           int16_t* out,
                           int32_t* filtState);

void WebRtcSpl_AnalysisQMF(const int16_t* in_data,
                           size_t in_data_length,
                           int16_t* low_band,
                           int16_t* high_band,
                           int32_t* filter_state1,
                           int32_t* filter_state2);
void WebRtcSpl_SynthesisQMF(const int16_t* low_band,
                            const int16_t* high_band,
                            size_t band_length,
                            int16_t* out_data,
                            int32_t* filter_state1,
                            int32_t* filter_state2);

#ifdef __cplusplus
}
#endif
#endif
