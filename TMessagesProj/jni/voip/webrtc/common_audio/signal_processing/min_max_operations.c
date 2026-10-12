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
 * This file contains the implementation of functions
 * WebRtcSpl_MaxAbsValueW16C()
 * WebRtcSpl_MaxAbsValueW32C()
 * WebRtcSpl_MaxValueW16C()
 * WebRtcSpl_MaxValueW32C()
 * WebRtcSpl_MinValueW16C()
 * WebRtcSpl_MinValueW32C()
 * WebRtcSpl_MaxAbsIndexW16()
 * WebRtcSpl_MaxIndexW16()
 * WebRtcSpl_MaxIndexW32()
 * WebRtcSpl_MinIndexW16()
 * WebRtcSpl_MinIndexW32()
 *
 */

#include <stdlib.h>

#include "rtc_base/checks.h"
#include "common_audio/signal_processing/include/signal_processing_library.h"

int16_t WebRtcSpl_MaxAbsValueW16C(const int16_t* vector, size_t length) {
  size_t i = 0;
  int absolute = 0, maximum = 0;

  RTC_DCHECK_GT(length, 0);

  for (i = 0; i < length; i++) {
    absolute = abs((int)vector[i]);

    if (absolute > maximum) {
      maximum = absolute;
    }
  }

  if (maximum > WEBRTC_SPL_WORD16_MAX) {
    maximum = WEBRTC_SPL_WORD16_MAX;
  }

  return (int16_t)maximum;
}

int32_t WebRtcSpl_MaxAbsValueW32C(const int32_t* vector, size_t length) {

  uint32_t absolute = 0, maximum = 0;
  size_t i = 0;

  RTC_DCHECK_GT(length, 0);

  for (i = 0; i < length; i++) {
    absolute = abs((int)vector[i]);
    if (absolute > maximum) {
      maximum = absolute;
    }
  }

  maximum = WEBRTC_SPL_MIN(maximum, WEBRTC_SPL_WORD32_MAX);

  return (int32_t)maximum;
}

int16_t WebRtcSpl_MaxValueW16C(const int16_t* vector, size_t length) {
  int16_t maximum = WEBRTC_SPL_WORD16_MIN;
  size_t i = 0;

  RTC_DCHECK_GT(length, 0);

  for (i = 0; i < length; i++) {
    if (vector[i] > maximum)
      maximum = vector[i];
  }
  return maximum;
}

int32_t WebRtcSpl_MaxValueW32C(const int32_t* vector, size_t length) {
  int32_t maximum = WEBRTC_SPL_WORD32_MIN;
  size_t i = 0;

  RTC_DCHECK_GT(length, 0);

  for (i = 0; i < length; i++) {
    if (vector[i] > maximum)
      maximum = vector[i];
  }
  return maximum;
}

int16_t WebRtcSpl_MinValueW16C(const int16_t* vector, size_t length) {
  int16_t minimum = WEBRTC_SPL_WORD16_MAX;
  size_t i = 0;

  RTC_DCHECK_GT(length, 0);

  for (i = 0; i < length; i++) {
    if (vector[i] < minimum)
      minimum = vector[i];
  }
  return minimum;
}

int32_t WebRtcSpl_MinValueW32C(const int32_t* vector, size_t length) {
  int32_t minimum = WEBRTC_SPL_WORD32_MAX;
  size_t i = 0;

  RTC_DCHECK_GT(length, 0);

  for (i = 0; i < length; i++) {
    if (vector[i] < minimum)
      minimum = vector[i];
  }
  return minimum;
}

size_t WebRtcSpl_MaxAbsIndexW16(const int16_t* vector, size_t length) {

  size_t i = 0, index = 0;
  int absolute = 0, maximum = 0;

  RTC_DCHECK_GT(length, 0);

  for (i = 0; i < length; i++) {
    absolute = abs((int)vector[i]);

    if (absolute > maximum) {
      maximum = absolute;
      index = i;
    }
  }

  return index;
}

int16_t WebRtcSpl_MaxAbsElementW16(const int16_t* vector, size_t length) {
  int16_t min_val, max_val;
  WebRtcSpl_MinMaxW16(vector, length, &min_val, &max_val);
  if (min_val == max_val || min_val < -max_val) {
    return min_val;
  }
  return max_val;
}

size_t WebRtcSpl_MaxIndexW16(const int16_t* vector, size_t length) {
  size_t i = 0, index = 0;
  int16_t maximum = WEBRTC_SPL_WORD16_MIN;

  RTC_DCHECK_GT(length, 0);

  for (i = 0; i < length; i++) {
    if (vector[i] > maximum) {
      maximum = vector[i];
      index = i;
    }
  }

  return index;
}

size_t WebRtcSpl_MaxIndexW32(const int32_t* vector, size_t length) {
  size_t i = 0, index = 0;
  int32_t maximum = WEBRTC_SPL_WORD32_MIN;

  RTC_DCHECK_GT(length, 0);

  for (i = 0; i < length; i++) {
    if (vector[i] > maximum) {
      maximum = vector[i];
      index = i;
    }
  }

  return index;
}

size_t WebRtcSpl_MinIndexW16(const int16_t* vector, size_t length) {
  size_t i = 0, index = 0;
  int16_t minimum = WEBRTC_SPL_WORD16_MAX;

  RTC_DCHECK_GT(length, 0);

  for (i = 0; i < length; i++) {
    if (vector[i] < minimum) {
      minimum = vector[i];
      index = i;
    }
  }

  return index;
}

size_t WebRtcSpl_MinIndexW32(const int32_t* vector, size_t length) {
  size_t i = 0, index = 0;
  int32_t minimum = WEBRTC_SPL_WORD32_MAX;

  RTC_DCHECK_GT(length, 0);

  for (i = 0; i < length; i++) {
    if (vector[i] < minimum) {
      minimum = vector[i];
      index = i;
    }
  }

  return index;
}

void WebRtcSpl_MinMaxW16(const int16_t* vector, size_t length,
                         int16_t* min_val, int16_t* max_val) {
#if defined(WEBRTC_HAS_NEON)
  return WebRtcSpl_MinMaxW16Neon(vector, length, min_val, max_val);
#else
  int16_t minimum = WEBRTC_SPL_WORD16_MAX;
  int16_t maximum = WEBRTC_SPL_WORD16_MIN;
  size_t i = 0;

  RTC_DCHECK_GT(length, 0);

  for (i = 0; i < length; i++) {
    if (vector[i] < minimum)
      minimum = vector[i];
    if (vector[i] > maximum)
      maximum = vector[i];
  }
  *min_val = minimum;
  *max_val = maximum;
#endif
}
