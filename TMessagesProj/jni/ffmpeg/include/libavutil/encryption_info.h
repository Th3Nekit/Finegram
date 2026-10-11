#ifndef AVUTIL_ENCRYPTION_INFO_H
#define AVUTIL_ENCRYPTION_INFO_H

#include <stddef.h>
#include <stdint.h>

typedef struct AVSubsampleEncryptionInfo {

    unsigned int bytes_of_clear_data;

    unsigned int bytes_of_protected_data;
} AVSubsampleEncryptionInfo;

typedef struct AVEncryptionInfo {

    uint32_t scheme;

    uint32_t crypt_byte_block;

    uint32_t skip_byte_block;

    uint8_t *key_id;
    uint32_t key_id_size;

    uint8_t *iv;
    uint32_t iv_size;

    AVSubsampleEncryptionInfo *subsamples;
    uint32_t subsample_count;
} AVEncryptionInfo;

typedef struct AVEncryptionInitInfo {

    uint8_t* system_id;
    uint32_t system_id_size;

    uint8_t** key_ids;

    uint32_t num_key_ids;

    uint32_t key_id_size;

    uint8_t* data;
    uint32_t data_size;

    struct AVEncryptionInitInfo *next;
} AVEncryptionInitInfo;

AVEncryptionInfo *av_encryption_info_alloc(uint32_t subsample_count, uint32_t key_id_size, uint32_t iv_size);

AVEncryptionInfo *av_encryption_info_clone(const AVEncryptionInfo *info);

void av_encryption_info_free(AVEncryptionInfo *info);

AVEncryptionInfo *av_encryption_info_get_side_data(const uint8_t *side_data, size_t side_data_size);

uint8_t *av_encryption_info_add_side_data(
      const AVEncryptionInfo *info, size_t *side_data_size);

AVEncryptionInitInfo *av_encryption_init_info_alloc(
    uint32_t system_id_size, uint32_t num_key_ids, uint32_t key_id_size, uint32_t data_size);

void av_encryption_init_info_free(AVEncryptionInitInfo* info);

AVEncryptionInitInfo *av_encryption_init_info_get_side_data(
    const uint8_t* side_data, size_t side_data_size);

uint8_t *av_encryption_init_info_add_side_data(
    const AVEncryptionInitInfo *info, size_t *side_data_size);

#endif
