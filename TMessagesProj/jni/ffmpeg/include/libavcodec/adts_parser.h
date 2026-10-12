#ifndef AVCODEC_ADTS_PARSER_H
#define AVCODEC_ADTS_PARSER_H

#include <stddef.h>
#include <stdint.h>

#define AV_AAC_ADTS_HEADER_SIZE 7

int av_adts_header_parse(const uint8_t *buf, uint32_t *samples,
                         uint8_t *frames);

#endif
