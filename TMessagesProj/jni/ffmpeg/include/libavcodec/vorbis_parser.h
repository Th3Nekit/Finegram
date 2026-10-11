#ifndef AVCODEC_VORBIS_PARSER_H
#define AVCODEC_VORBIS_PARSER_H

#include <stdint.h>

typedef struct AVVorbisParseContext AVVorbisParseContext;

AVVorbisParseContext *av_vorbis_parse_init(const uint8_t *extradata,
                                           int extradata_size);

void av_vorbis_parse_free(AVVorbisParseContext **s);

#define VORBIS_FLAG_HEADER  0x00000001
#define VORBIS_FLAG_COMMENT 0x00000002
#define VORBIS_FLAG_SETUP   0x00000004

int av_vorbis_parse_frame_flags(AVVorbisParseContext *s, const uint8_t *buf,
                                int buf_size, int *flags);

int av_vorbis_parse_frame(AVVorbisParseContext *s, const uint8_t *buf,
                          int buf_size);

void av_vorbis_parse_reset(AVVorbisParseContext *s);

#endif
