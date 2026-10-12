/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_coding/codecs/isac/main/source/arith_routines.h"
#include "modules/audio_coding/codecs/isac/main/source/settings.h"

int WebRtcIsac_EncTerminate(Bitstr *streamdata)
{
  uint8_t *stream_ptr;

  stream_ptr = streamdata->stream + streamdata->stream_index;

  if ( streamdata->W_upper > 0x01FFFFFF )
  {
    streamdata->streamval += 0x01000000;

    if (streamdata->streamval < 0x01000000)
    {

      while ( !(++(*--stream_ptr)) );

      stream_ptr = streamdata->stream + streamdata->stream_index;
    }

    *stream_ptr++ = (uint8_t) (streamdata->streamval >> 24);
  }
  else
  {
    streamdata->streamval += 0x00010000;

    if (streamdata->streamval < 0x00010000)
    {

      while ( !(++(*--stream_ptr)) );

      stream_ptr = streamdata->stream + streamdata->stream_index;
    }

    *stream_ptr++ = (uint8_t) (streamdata->streamval >> 24);
    *stream_ptr++ = (uint8_t) ((streamdata->streamval >> 16) & 0x00FF);
  }

  return (int)(stream_ptr - streamdata->stream);
}
