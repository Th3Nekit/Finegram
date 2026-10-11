// Copyright 1995-2016 The OpenSSL Project Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <openssl/lhash.h>

#include <assert.h>
#include <limits.h>
#include <string.h>

#include <openssl/mem.h>

#include "../internal.h"
#include "internal.h"

static const size_t kMinNumBuckets = 16;

static const size_t kMaxAverageChainLength = 2;
static const size_t kMinAverageChainLength = 1;

typedef struct lhash_item_st {
  void *data;
  struct lhash_item_st *next;

  uint32_t hash;
} LHASH_ITEM;

struct lhash_st {

  size_t num_items;

  LHASH_ITEM **buckets;

  size_t num_buckets;

  unsigned callback_depth;

  lhash_cmp_func comp;
  lhash_hash_func hash;
};

_LHASH *OPENSSL_lh_new(lhash_hash_func hash, lhash_cmp_func comp) {
  _LHASH *ret = reinterpret_cast<_LHASH *>(OPENSSL_zalloc(sizeof(_LHASH)));
  if (ret == NULL) {
    return NULL;
  }

  ret->num_buckets = kMinNumBuckets;
  ret->buckets = reinterpret_cast<LHASH_ITEM **>(
      OPENSSL_calloc(ret->num_buckets, sizeof(LHASH_ITEM *)));
  if (ret->buckets == NULL) {
    OPENSSL_free(ret);
    return NULL;
  }

  ret->comp = comp;
  ret->hash = hash;
  return ret;
}

void OPENSSL_lh_free(_LHASH *lh) {
  if (lh == NULL) {
    return;
  }

  for (size_t i = 0; i < lh->num_buckets; i++) {
    LHASH_ITEM *next;
    for (LHASH_ITEM *n = lh->buckets[i]; n != NULL; n = next) {
      next = n->next;
      OPENSSL_free(n);
    }
  }

  OPENSSL_free(lh->buckets);
  OPENSSL_free(lh);
}

size_t OPENSSL_lh_num_items(const _LHASH *lh) { return lh->num_items; }

static LHASH_ITEM **get_next_ptr_and_hash(const _LHASH *lh, uint32_t *out_hash,
                                          const void *data,
                                          lhash_hash_func_helper call_hash_func,
                                          lhash_cmp_func_helper call_cmp_func) {
  const uint32_t hash = call_hash_func(lh->hash, data);
  if (out_hash != NULL) {
    *out_hash = hash;
  }

  LHASH_ITEM **ret = &lh->buckets[hash % lh->num_buckets];
  for (LHASH_ITEM *cur = *ret; cur != NULL; cur = *ret) {
    if (call_cmp_func(lh->comp, cur->data, data) == 0) {
      break;
    }
    ret = &cur->next;
  }

  return ret;
}

static LHASH_ITEM **get_next_ptr_by_key(const _LHASH *lh, const void *key,
                                        uint32_t key_hash,
                                        int (*cmp_key)(const void *key,
                                                       const void *value)) {
  LHASH_ITEM **ret = &lh->buckets[key_hash % lh->num_buckets];
  for (LHASH_ITEM *cur = *ret; cur != NULL; cur = *ret) {
    if (cmp_key(key, cur->data) == 0) {
      break;
    }
    ret = &cur->next;
  }

  return ret;
}

void *OPENSSL_lh_retrieve(const _LHASH *lh, const void *data,
                          lhash_hash_func_helper call_hash_func,
                          lhash_cmp_func_helper call_cmp_func) {
  LHASH_ITEM **next_ptr =
      get_next_ptr_and_hash(lh, NULL, data, call_hash_func, call_cmp_func);
  return *next_ptr == NULL ? NULL : (*next_ptr)->data;
}

void *OPENSSL_lh_retrieve_key(const _LHASH *lh, const void *key,
                              uint32_t key_hash,
                              int (*cmp_key)(const void *key,
                                             const void *value)) {
  LHASH_ITEM **next_ptr = get_next_ptr_by_key(lh, key, key_hash, cmp_key);
  return *next_ptr == NULL ? NULL : (*next_ptr)->data;
}

static void lh_rebucket(_LHASH *lh, const size_t new_num_buckets) {
  LHASH_ITEM **new_buckets, *cur, *next;
  size_t i, alloc_size;

  alloc_size = sizeof(LHASH_ITEM *) * new_num_buckets;
  if (alloc_size / sizeof(LHASH_ITEM *) != new_num_buckets) {
    return;
  }

  new_buckets = reinterpret_cast<LHASH_ITEM **>(OPENSSL_zalloc(alloc_size));
  if (new_buckets == NULL) {
    return;
  }

  for (i = 0; i < lh->num_buckets; i++) {
    for (cur = lh->buckets[i]; cur != NULL; cur = next) {
      const size_t new_bucket = cur->hash % new_num_buckets;
      next = cur->next;
      cur->next = new_buckets[new_bucket];
      new_buckets[new_bucket] = cur;
    }
  }

  OPENSSL_free(lh->buckets);

  lh->num_buckets = new_num_buckets;
  lh->buckets = new_buckets;
}

static void lh_maybe_resize(_LHASH *lh) {
  size_t avg_chain_length;

  if (lh->callback_depth > 0) {

    return;
  }

  assert(lh->num_buckets >= kMinNumBuckets);
  avg_chain_length = lh->num_items / lh->num_buckets;

  if (avg_chain_length > kMaxAverageChainLength) {
    const size_t new_num_buckets = lh->num_buckets * 2;

    if (new_num_buckets > lh->num_buckets) {
      lh_rebucket(lh, new_num_buckets);
    }
  } else if (avg_chain_length < kMinAverageChainLength &&
             lh->num_buckets > kMinNumBuckets) {
    size_t new_num_buckets = lh->num_buckets / 2;

    if (new_num_buckets < kMinNumBuckets) {
      new_num_buckets = kMinNumBuckets;
    }

    lh_rebucket(lh, new_num_buckets);
  }
}

int OPENSSL_lh_insert(_LHASH *lh, void **old_data, void *data,
                      lhash_hash_func_helper call_hash_func,
                      lhash_cmp_func_helper call_cmp_func) {
  uint32_t hash;
  LHASH_ITEM **next_ptr, *item;

  *old_data = NULL;
  next_ptr =
      get_next_ptr_and_hash(lh, &hash, data, call_hash_func, call_cmp_func);

  if (*next_ptr != NULL) {

    *old_data = (*next_ptr)->data;
    (*next_ptr)->data = data;
    return 1;
  }

  item = reinterpret_cast<LHASH_ITEM *>(OPENSSL_malloc(sizeof(LHASH_ITEM)));
  if (item == NULL) {
    return 0;
  }

  item->data = data;
  item->hash = hash;
  item->next = NULL;
  *next_ptr = item;
  lh->num_items++;
  lh_maybe_resize(lh);

  return 1;
}

void *OPENSSL_lh_delete(_LHASH *lh, const void *data,
                        lhash_hash_func_helper call_hash_func,
                        lhash_cmp_func_helper call_cmp_func) {
  LHASH_ITEM **next_ptr, *item, *ret;

  next_ptr =
      get_next_ptr_and_hash(lh, NULL, data, call_hash_func, call_cmp_func);

  if (*next_ptr == NULL) {

    return NULL;
  }

  item = *next_ptr;
  *next_ptr = item->next;
  ret = reinterpret_cast<LHASH_ITEM *>(item->data);
  OPENSSL_free(item);

  lh->num_items--;
  lh_maybe_resize(lh);

  return ret;
}

void OPENSSL_lh_doall_arg(_LHASH *lh, void (*func)(void *, void *), void *arg) {
  if (lh == NULL) {
    return;
  }

  if (lh->callback_depth < UINT_MAX) {

    lh->callback_depth++;
  }

  for (size_t i = 0; i < lh->num_buckets; i++) {
    LHASH_ITEM *next;
    for (LHASH_ITEM *cur = lh->buckets[i]; cur != NULL; cur = next) {
      next = cur->next;
      func(cur->data, arg);
    }
  }

  if (lh->callback_depth < UINT_MAX) {
    lh->callback_depth--;
  }

  lh_maybe_resize(lh);
}
