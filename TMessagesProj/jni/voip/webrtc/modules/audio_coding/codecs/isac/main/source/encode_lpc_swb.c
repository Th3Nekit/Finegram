/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

/*
 * code_LPC_UB.c
 *
 * This file contains definition of functions used to
 * encode LPC parameters (Shape & gain) of the upper band.
 *
 */

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "modules/audio_coding/codecs/isac/main/source/encode_lpc_swb.h"
#include "modules/audio_coding/codecs/isac/main/source/lpc_gain_swb_tables.h"
#include "modules/audio_coding/codecs/isac/main/source/lpc_shape_swb12_tables.h"
#include "modules/audio_coding/codecs/isac/main/source/lpc_shape_swb16_tables.h"
#include "modules/audio_coding/codecs/isac/main/source/settings.h"

int16_t
WebRtcIsac_RemoveLarMean(
    double* lar,
    int16_t bandwidth)
{
  int16_t coeffCntr;
  int16_t vecCntr;
  int16_t numVec;
  const double* meanLAR;
  switch(bandwidth)
  {
    case isac12kHz:
      {
        numVec = UB_LPC_VEC_PER_FRAME;
        meanLAR = WebRtcIsac_kMeanLarUb12;
        break;
      }
    case isac16kHz:
      {
        numVec = UB16_LPC_VEC_PER_FRAME;
        meanLAR = WebRtcIsac_kMeanLarUb16;
        break;
      }
    default:
      return -1;
  }

  for(vecCntr = 0; vecCntr < numVec; vecCntr++)
  {
    for(coeffCntr = 0; coeffCntr < UB_LPC_ORDER; coeffCntr++)
    {

      *lar++ -= meanLAR[coeffCntr];
    }
  }
  return 0;
}

int16_t
WebRtcIsac_DecorrelateIntraVec(
    const double* data,
    double*       out,
    int16_t bandwidth)
{
  const double* ptrData;
  const double* ptrRow;
  int16_t rowCntr;
  int16_t colCntr;
  int16_t larVecCntr;
  int16_t numVec;
  const double* decorrMat;
  switch(bandwidth)
  {
    case isac12kHz:
      {
        decorrMat = &WebRtcIsac_kIntraVecDecorrMatUb12[0][0];
        numVec = UB_LPC_VEC_PER_FRAME;
        break;
      }
    case isac16kHz:
      {
        decorrMat = &WebRtcIsac_kIintraVecDecorrMatUb16[0][0];
        numVec = UB16_LPC_VEC_PER_FRAME;
        break;
      }
    default:
      return -1;
  }

  ptrData = data;
  for(larVecCntr = 0; larVecCntr < numVec; larVecCntr++)
  {
    for(rowCntr = 0; rowCntr < UB_LPC_ORDER; rowCntr++)
    {
      ptrRow = &decorrMat[rowCntr * UB_LPC_ORDER];
      *out = 0;
      for(colCntr = 0; colCntr < UB_LPC_ORDER; colCntr++)
      {
        *out += ptrData[colCntr] * ptrRow[colCntr];
      }
      out++;
    }
    ptrData += UB_LPC_ORDER;
  }
  return 0;
}

int16_t
WebRtcIsac_DecorrelateInterVec(
    const double* data,
    double* out,
    int16_t bandwidth)
{
  int16_t coeffCntr;
  int16_t rowCntr;
  int16_t colCntr;
  const double* decorrMat;
  int16_t interVecDim;

  switch(bandwidth)
  {
    case isac12kHz:
      {
        decorrMat = &WebRtcIsac_kInterVecDecorrMatUb12[0][0];
        interVecDim = UB_LPC_VEC_PER_FRAME;
        break;
      }
    case isac16kHz:
      {
        decorrMat = &WebRtcIsac_kInterVecDecorrMatUb16[0][0];
        interVecDim = UB16_LPC_VEC_PER_FRAME;
        break;
      }
    default:
      return -1;
  }

  for(coeffCntr = 0; coeffCntr < UB_LPC_ORDER; coeffCntr++)
  {
    for(colCntr = 0; colCntr < interVecDim; colCntr++)
    {
      out[coeffCntr + colCntr * UB_LPC_ORDER] = 0;
      for(rowCntr = 0; rowCntr < interVecDim; rowCntr++)
      {
        out[coeffCntr + colCntr * UB_LPC_ORDER] +=
            data[coeffCntr + rowCntr * UB_LPC_ORDER] *
            decorrMat[rowCntr * interVecDim + colCntr];
      }
    }
  }
  return 0;
}

double
WebRtcIsac_QuantizeUncorrLar(
    double* data,
    int* recIdx,
    int16_t bandwidth)
{
  int16_t cntr;
  int32_t idx;
  int16_t interVecDim;
  const double* leftRecPoint;
  double quantizationStepSize;
  const int16_t* numQuantCell;
  switch(bandwidth)
  {
    case isac12kHz:
      {
        leftRecPoint         = WebRtcIsac_kLpcShapeLeftRecPointUb12;
        quantizationStepSize = WebRtcIsac_kLpcShapeQStepSizeUb12;
        numQuantCell         = WebRtcIsac_kLpcShapeNumRecPointUb12;
        interVecDim          = UB_LPC_VEC_PER_FRAME;
        break;
      }
    case isac16kHz:
      {
        leftRecPoint         = WebRtcIsac_kLpcShapeLeftRecPointUb16;
        quantizationStepSize = WebRtcIsac_kLpcShapeQStepSizeUb16;
        numQuantCell         = WebRtcIsac_kLpcShapeNumRecPointUb16;
        interVecDim          = UB16_LPC_VEC_PER_FRAME;
        break;
      }
    default:
      return -1;
  }

  for(cntr = 0; cntr < UB_LPC_ORDER * interVecDim; cntr++)
  {
    idx = (int32_t)floor((*data - leftRecPoint[cntr]) /
                               quantizationStepSize + 0.5);
    if(idx < 0)
    {
      idx = 0;
    }
    else if(idx >= numQuantCell[cntr])
    {
      idx = numQuantCell[cntr] - 1;
    }

    *data++ = leftRecPoint[cntr] + idx * quantizationStepSize;
    *recIdx++ = idx;
  }
  return 0;
}

int16_t
WebRtcIsac_DequantizeLpcParam(
    const int* idx,
    double*    out,
    int16_t bandwidth)
{
  int16_t cntr;
  int16_t interVecDim;
  const double* leftRecPoint;
  double quantizationStepSize;

  switch(bandwidth)
  {
    case isac12kHz:
      {
        leftRecPoint =         WebRtcIsac_kLpcShapeLeftRecPointUb12;
        quantizationStepSize = WebRtcIsac_kLpcShapeQStepSizeUb12;
        interVecDim =          UB_LPC_VEC_PER_FRAME;
        break;
      }
    case isac16kHz:
      {
        leftRecPoint =         WebRtcIsac_kLpcShapeLeftRecPointUb16;
        quantizationStepSize = WebRtcIsac_kLpcShapeQStepSizeUb16;
        interVecDim =          UB16_LPC_VEC_PER_FRAME;
        break;
      }
    default:
      return -1;
  }

  for(cntr = 0; cntr < UB_LPC_ORDER * interVecDim; cntr++)
  {
    *out++ = leftRecPoint[cntr] + *idx++ * quantizationStepSize;
  }
  return 0;
}

int16_t
WebRtcIsac_CorrelateIntraVec(
    const double* data,
    double*       out,
    int16_t bandwidth)
{
  int16_t vecCntr;
  int16_t rowCntr;
  int16_t colCntr;
  int16_t numVec;
  const double* ptrData;
  const double* intraVecDecorrMat;

  switch(bandwidth)
  {
    case isac12kHz:
      {
        numVec            = UB_LPC_VEC_PER_FRAME;
        intraVecDecorrMat = &WebRtcIsac_kIntraVecDecorrMatUb12[0][0];
        break;
      }
    case isac16kHz:
      {
        numVec            = UB16_LPC_VEC_PER_FRAME;
        intraVecDecorrMat = &WebRtcIsac_kIintraVecDecorrMatUb16[0][0];
        break;
      }
    default:
      return -1;
  }

  ptrData = data;
  for(vecCntr = 0; vecCntr < numVec; vecCntr++)
  {
    for(colCntr = 0; colCntr < UB_LPC_ORDER; colCntr++)
    {
      *out = 0;
      for(rowCntr = 0; rowCntr < UB_LPC_ORDER; rowCntr++)
      {
        *out += ptrData[rowCntr] *
            intraVecDecorrMat[rowCntr * UB_LPC_ORDER + colCntr];
      }
      out++;
    }
    ptrData += UB_LPC_ORDER;
  }
  return 0;
}

int16_t
WebRtcIsac_CorrelateInterVec(
    const double* data,
    double*       out,
    int16_t bandwidth)
{
  int16_t coeffCntr;
  int16_t rowCntr;
  int16_t colCntr;
  int16_t interVecDim;
  double myVec[UB16_LPC_VEC_PER_FRAME] = {0.0};
  const double* interVecDecorrMat;

  switch(bandwidth)
  {
    case isac12kHz:
      {
        interVecDim       = UB_LPC_VEC_PER_FRAME;
        interVecDecorrMat = &WebRtcIsac_kInterVecDecorrMatUb12[0][0];
        break;
      }
    case isac16kHz:
      {
        interVecDim       = UB16_LPC_VEC_PER_FRAME;
        interVecDecorrMat = &WebRtcIsac_kInterVecDecorrMatUb16[0][0];
        break;
      }
    default:
      return -1;
  }

  for(coeffCntr = 0; coeffCntr < UB_LPC_ORDER; coeffCntr++)
  {
    for(rowCntr = 0; rowCntr < interVecDim; rowCntr++)
    {
      myVec[rowCntr] = 0;
      for(colCntr = 0; colCntr < interVecDim; colCntr++)
      {
        myVec[rowCntr] += data[coeffCntr + colCntr * UB_LPC_ORDER] *
            interVecDecorrMat[rowCntr * interVecDim + colCntr];

      }
    }

    for(rowCntr = 0; rowCntr < interVecDim; rowCntr++)
    {
      out[coeffCntr + rowCntr * UB_LPC_ORDER] = myVec[rowCntr];
    }
  }
  return 0;
}

int16_t
WebRtcIsac_AddLarMean(
    double* data,
    int16_t bandwidth)
{
  int16_t coeffCntr;
  int16_t vecCntr;
  int16_t numVec;
  const double* meanLAR;

  switch(bandwidth)
  {
    case isac12kHz:
      {
        numVec = UB_LPC_VEC_PER_FRAME;
        meanLAR = WebRtcIsac_kMeanLarUb12;
        break;
      }
    case isac16kHz:
      {
        numVec = UB16_LPC_VEC_PER_FRAME;
        meanLAR = WebRtcIsac_kMeanLarUb16;
        break;
      }
    default:
      return -1;
  }

  for(vecCntr = 0; vecCntr < numVec; vecCntr++)
  {
    for(coeffCntr = 0; coeffCntr < UB_LPC_ORDER; coeffCntr++)
    {
      *data++ += meanLAR[coeffCntr];
    }
  }
  return 0;
}

int16_t
WebRtcIsac_ToLogDomainRemoveMean(
    double* data)
{
  int16_t coeffCntr;
  for(coeffCntr = 0; coeffCntr < UB_LPC_GAIN_DIM; coeffCntr++)
  {
    data[coeffCntr] = log(data[coeffCntr]) - WebRtcIsac_kMeanLpcGain;
  }
  return 0;
}

int16_t WebRtcIsac_DecorrelateLPGain(
    const double* data,
    double* out)
{
  int16_t rowCntr;
  int16_t colCntr;

  for(colCntr = 0; colCntr < UB_LPC_GAIN_DIM; colCntr++)
  {
    *out = 0;
    for(rowCntr = 0; rowCntr < UB_LPC_GAIN_DIM; rowCntr++)
    {
      *out += data[rowCntr] * WebRtcIsac_kLpcGainDecorrMat[rowCntr][colCntr];
    }
    out++;
  }
  return 0;
}

double WebRtcIsac_QuantizeLpcGain(
    double* data,
    int*    idx)
{
  int16_t coeffCntr;
  for(coeffCntr = 0; coeffCntr < UB_LPC_GAIN_DIM; coeffCntr++)
  {
    *idx = (int)floor((*data - WebRtcIsac_kLeftRecPointLpcGain[coeffCntr]) /
                                WebRtcIsac_kQSizeLpcGain + 0.5);

    if(*idx < 0)
    {
      *idx = 0;
    }
    else if(*idx >= WebRtcIsac_kNumQCellLpcGain[coeffCntr])
    {
      *idx = WebRtcIsac_kNumQCellLpcGain[coeffCntr] - 1;
    }
    *data = WebRtcIsac_kLeftRecPointLpcGain[coeffCntr] + *idx *
        WebRtcIsac_kQSizeLpcGain;

    data++;
    idx++;
  }
  return 0;
}

int16_t WebRtcIsac_DequantizeLpcGain(
    const int* idx,
    double*    out)
{
  int16_t coeffCntr;
  for(coeffCntr = 0; coeffCntr < UB_LPC_GAIN_DIM; coeffCntr++)
  {
    *out = WebRtcIsac_kLeftRecPointLpcGain[coeffCntr] + *idx *
        WebRtcIsac_kQSizeLpcGain;
    out++;
    idx++;
  }
  return 0;
}

int16_t WebRtcIsac_CorrelateLpcGain(
    const double* data,
    double* out)
{
  int16_t rowCntr;
  int16_t colCntr;

  for(rowCntr = 0; rowCntr < UB_LPC_GAIN_DIM; rowCntr++)
  {
    *out = 0;
    for(colCntr = 0; colCntr < UB_LPC_GAIN_DIM; colCntr++)
    {
      *out += WebRtcIsac_kLpcGainDecorrMat[rowCntr][colCntr] * data[colCntr];
    }
    out++;
  }

  return 0;
}

int16_t WebRtcIsac_AddMeanToLinearDomain(
    double* lpcGains)
{
  int16_t coeffCntr;
  for(coeffCntr = 0; coeffCntr < UB_LPC_GAIN_DIM; coeffCntr++)
  {
    lpcGains[coeffCntr] = exp(lpcGains[coeffCntr] + WebRtcIsac_kMeanLpcGain);
  }
  return 0;
}
