#ifndef AVUTIL_REPLAYGAIN_H
#define AVUTIL_REPLAYGAIN_H

#include <stdint.h>

typedef struct AVReplayGain {

    int32_t track_gain;

    uint32_t track_peak;

    int32_t album_gain;

    uint32_t album_peak;
} AVReplayGain;

#endif
