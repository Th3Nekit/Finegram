#ifndef AVUTIL_TDRDI_H
#define AVUTIL_TDRDI_H

#include <stddef.h>
#include <stdint.h>

#include "libavutil/avassert.h"

#define AV_TDRDI_MAX_NUM_REF_DISPLAY 32

typedef struct AV3DReferenceDisplaysInfo {

    uint8_t prec_ref_display_width;

    uint8_t ref_viewing_distance_flag;

    uint8_t prec_ref_viewing_dist;

    uint8_t num_ref_displays;

    size_t entries_offset;

    size_t entry_size;
} AV3DReferenceDisplaysInfo;

typedef struct AV3DReferenceDisplay {

    uint16_t left_view_id;

    uint16_t right_view_id;

    uint8_t exponent_ref_display_width;

    uint8_t mantissa_ref_display_width;

    uint8_t exponent_ref_viewing_distance;

    uint8_t mantissa_ref_viewing_distance;

    uint8_t additional_shift_present_flag;

    int16_t num_sample_shift;
} AV3DReferenceDisplay;

static av_always_inline AV3DReferenceDisplay*
av_tdrdi_get_display(AV3DReferenceDisplaysInfo *tdrdi, unsigned int idx)
{
    av_assert0(idx < tdrdi->num_ref_displays);
    return (AV3DReferenceDisplay *)((uint8_t *)tdrdi + tdrdi->entries_offset +
                                    idx * tdrdi->entry_size);
}

AV3DReferenceDisplaysInfo *av_tdrdi_alloc(unsigned int nb_displays, size_t *size);

#endif
