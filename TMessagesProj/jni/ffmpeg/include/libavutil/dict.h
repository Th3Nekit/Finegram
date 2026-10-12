#ifndef AVUTIL_DICT_H
#define AVUTIL_DICT_H

#include <stdint.h>

#define AV_DICT_MATCH_CASE      1
#define AV_DICT_IGNORE_SUFFIX   2

#define AV_DICT_DONT_STRDUP_KEY 4

#define AV_DICT_DONT_STRDUP_VAL 8

#define AV_DICT_DONT_OVERWRITE 16
#define AV_DICT_APPEND         32

#define AV_DICT_MULTIKEY       64
#define AV_DICT_DEDUP         128

typedef struct AVDictionaryEntry {
    char *key;
    char *value;
} AVDictionaryEntry;

typedef struct AVDictionary AVDictionary;

AVDictionaryEntry *av_dict_get(const AVDictionary *m, const char *key,
                               const AVDictionaryEntry *prev, int flags);

const AVDictionaryEntry *av_dict_iterate(const AVDictionary *m,
                                         const AVDictionaryEntry *prev);

int av_dict_count(const AVDictionary *m);

int av_dict_set(AVDictionary **pm, const char *key, const char *value, int flags);

int av_dict_set_int(AVDictionary **pm, const char *key, int64_t value, int flags);

int av_dict_parse_string(AVDictionary **pm, const char *str,
                         const char *key_val_sep, const char *pairs_sep,
                         int flags);

int av_dict_copy(AVDictionary **dst, const AVDictionary *src, int flags);

void av_dict_free(AVDictionary **m);

int av_dict_get_string(const AVDictionary *m, char **buffer,
                       const char key_val_sep, const char pairs_sep);

#endif
