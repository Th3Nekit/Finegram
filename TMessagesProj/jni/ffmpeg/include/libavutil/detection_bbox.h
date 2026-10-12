#ifndef AVUTIL_DETECTION_BBOX_H
#define AVUTIL_DETECTION_BBOX_H

#include "rational.h"
#include "avassert.h"
#include "frame.h"

typedef struct AVDetectionBBox {

    int x;
    int y;
    int w;
    int h;

#define AV_DETECTION_BBOX_LABEL_NAME_MAX_SIZE 64

    char detect_label[AV_DETECTION_BBOX_LABEL_NAME_MAX_SIZE];
    AVRational detect_confidence;

#define AV_NUM_DETECTION_BBOX_CLASSIFY 4
    uint32_t classify_count;
    char classify_labels[AV_NUM_DETECTION_BBOX_CLASSIFY][AV_DETECTION_BBOX_LABEL_NAME_MAX_SIZE];
    AVRational classify_confidences[AV_NUM_DETECTION_BBOX_CLASSIFY];
} AVDetectionBBox;

typedef struct AVDetectionBBoxHeader {

    char source[256];

    uint32_t nb_bboxes;

    size_t bboxes_offset;

    size_t bbox_size;
} AVDetectionBBoxHeader;

static av_always_inline AVDetectionBBox *
av_get_detection_bbox(const AVDetectionBBoxHeader *header, unsigned int idx)
{
    av_assert0(idx < header->nb_bboxes);
    return (AVDetectionBBox *)((uint8_t *)header + header->bboxes_offset +
                               idx * header->bbox_size);
}

AVDetectionBBoxHeader *av_detection_bbox_alloc(uint32_t nb_bboxes, size_t *out_size);

AVDetectionBBoxHeader *av_detection_bbox_create_side_data(AVFrame *frame, uint32_t nb_bboxes);
#endif
