// Copyright 2022 The BoringSSL Authors
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

#include <openssl/x509.h>

#include <assert.h>

#include <openssl/mem.h>
#include <openssl/obj.h>
#include <openssl/stack.h>

#include "../internal.h"
#include "internal.h"

typedef struct x509_policy_node_st {

  ASN1_OBJECT *policy;

  STACK_OF(ASN1_OBJECT) *parent_policies;

  int mapped;

  int reachable;
} X509_POLICY_NODE;

DEFINE_STACK_OF(X509_POLICY_NODE)

typedef struct x509_policy_level_st {

  STACK_OF(X509_POLICY_NODE) *nodes;

  int has_any_policy;
} X509_POLICY_LEVEL;

DEFINE_STACK_OF(X509_POLICY_LEVEL)

static int is_any_policy(const ASN1_OBJECT *obj) {
  return OBJ_obj2nid(obj) == NID_any_policy;
}

static void x509_policy_node_free(X509_POLICY_NODE *node) {
  if (node != NULL) {
    ASN1_OBJECT_free(node->policy);
    sk_ASN1_OBJECT_pop_free(node->parent_policies, ASN1_OBJECT_free);
    OPENSSL_free(node);
  }
}

static X509_POLICY_NODE *x509_policy_node_new(const ASN1_OBJECT *policy) {
  assert(!is_any_policy(policy));
  X509_POLICY_NODE *node = reinterpret_cast<X509_POLICY_NODE *>(
      OPENSSL_zalloc(sizeof(X509_POLICY_NODE)));
  if (node == NULL) {
    return NULL;
  }
  node->policy = OBJ_dup(policy);
  node->parent_policies = sk_ASN1_OBJECT_new_null();
  if (node->policy == NULL || node->parent_policies == NULL) {
    x509_policy_node_free(node);
    return NULL;
  }
  return node;
}

static int x509_policy_node_cmp(const X509_POLICY_NODE *const *a,
                                const X509_POLICY_NODE *const *b) {
  return OBJ_cmp((*a)->policy, (*b)->policy);
}

static void x509_policy_level_free(X509_POLICY_LEVEL *level) {
  if (level != NULL) {
    sk_X509_POLICY_NODE_pop_free(level->nodes, x509_policy_node_free);
    OPENSSL_free(level);
  }
}

static X509_POLICY_LEVEL *x509_policy_level_new(void) {
  X509_POLICY_LEVEL *level = reinterpret_cast<X509_POLICY_LEVEL *>(
      OPENSSL_zalloc(sizeof(X509_POLICY_LEVEL)));
  if (level == NULL) {
    return NULL;
  }
  level->nodes = sk_X509_POLICY_NODE_new(x509_policy_node_cmp);
  if (level->nodes == NULL) {
    x509_policy_level_free(level);
    return NULL;
  }
  return level;
}

static int x509_policy_level_is_empty(const X509_POLICY_LEVEL *level) {
  return !level->has_any_policy && sk_X509_POLICY_NODE_num(level->nodes) == 0;
}

static void x509_policy_level_clear(X509_POLICY_LEVEL *level) {
  level->has_any_policy = 0;
  for (size_t i = 0; i < sk_X509_POLICY_NODE_num(level->nodes); i++) {
    x509_policy_node_free(sk_X509_POLICY_NODE_value(level->nodes, i));
  }
  sk_X509_POLICY_NODE_zero(level->nodes);
}

static X509_POLICY_NODE *x509_policy_level_find(X509_POLICY_LEVEL *level,
                                                const ASN1_OBJECT *policy) {
  assert(sk_X509_POLICY_NODE_is_sorted(level->nodes));
  X509_POLICY_NODE node;
  node.policy = (ASN1_OBJECT *)policy;
  size_t idx;
  if (!sk_X509_POLICY_NODE_find(level->nodes, &idx, &node)) {
    return NULL;
  }
  return sk_X509_POLICY_NODE_value(level->nodes, idx);
}

static int x509_policy_level_add_nodes(X509_POLICY_LEVEL *level,
                                       STACK_OF(X509_POLICY_NODE) *nodes) {
  for (size_t i = 0; i < sk_X509_POLICY_NODE_num(nodes); i++) {
    X509_POLICY_NODE *node = sk_X509_POLICY_NODE_value(nodes, i);
    if (!sk_X509_POLICY_NODE_push(level->nodes, node)) {
      return 0;
    }
    sk_X509_POLICY_NODE_set(nodes, i, NULL);
  }
  sk_X509_POLICY_NODE_sort(level->nodes);

#if !defined(NDEBUG)

  for (size_t i = 1; i < sk_X509_POLICY_NODE_num(level->nodes); i++) {
    assert(OBJ_cmp(sk_X509_POLICY_NODE_value(level->nodes, i - 1)->policy,
                   sk_X509_POLICY_NODE_value(level->nodes, i)->policy) != 0);
  }
#endif
  return 1;
}

static int policyinfo_cmp(const POLICYINFO *const *a,
                          const POLICYINFO *const *b) {
  return OBJ_cmp((*a)->policyid, (*b)->policyid);
}

static int delete_if_not_in_policies(X509_POLICY_NODE *node, void *data) {
  const CERTIFICATEPOLICIES *policies =
      reinterpret_cast<CERTIFICATEPOLICIES *>(data);
  assert(sk_POLICYINFO_is_sorted(policies));
  POLICYINFO info;
  info.policyid = node->policy;
  if (sk_POLICYINFO_find(policies, NULL, &info)) {
    return 0;
  }
  x509_policy_node_free(node);
  return 1;
}

static int process_certificate_policies(const X509 *x509,
                                        X509_POLICY_LEVEL *level,
                                        int any_policy_allowed) {
  int ret = 0;
  int critical;
  STACK_OF(X509_POLICY_NODE) *new_nodes = NULL;
  CERTIFICATEPOLICIES *policies = reinterpret_cast<CERTIFICATEPOLICIES *>(
      X509_get_ext_d2i(x509, NID_certificate_policies, &critical, NULL));

  {
    if (policies == NULL) {
      if (critical != -1) {
        return 0;
      }

      x509_policy_level_clear(level);
      return 1;
    }

    if (sk_POLICYINFO_num(policies) == 0) {
      OPENSSL_PUT_ERROR(X509, X509_R_INVALID_POLICY_EXTENSION);
      goto err;
    }

    sk_POLICYINFO_set_cmp_func(policies, policyinfo_cmp);
    sk_POLICYINFO_sort(policies);
    int cert_has_any_policy = 0;
    for (size_t i = 0; i < sk_POLICYINFO_num(policies); i++) {
      const POLICYINFO *policy = sk_POLICYINFO_value(policies, i);
      if (is_any_policy(policy->policyid)) {
        cert_has_any_policy = 1;
      }
      if (i > 0 && OBJ_cmp(sk_POLICYINFO_value(policies, i - 1)->policyid,
                           policy->policyid) == 0) {

        OPENSSL_PUT_ERROR(X509, X509_R_INVALID_POLICY_EXTENSION);
        goto err;
      }
    }

    const int previous_level_has_any_policy = level->has_any_policy;

    if (!cert_has_any_policy || !any_policy_allowed) {
      sk_X509_POLICY_NODE_delete_if(level->nodes, delete_if_not_in_policies,
                                    policies);
      level->has_any_policy = 0;
    }

    if (previous_level_has_any_policy) {
      new_nodes = sk_X509_POLICY_NODE_new_null();
      if (new_nodes == NULL) {
        goto err;
      }
      for (size_t i = 0; i < sk_POLICYINFO_num(policies); i++) {
        const POLICYINFO *policy = sk_POLICYINFO_value(policies, i);

        if (!is_any_policy(policy->policyid) &&
            x509_policy_level_find(level, policy->policyid) == NULL) {
          X509_POLICY_NODE *node = x509_policy_node_new(policy->policyid);
          if (node == NULL ||
              !sk_X509_POLICY_NODE_push(new_nodes, node)) {
            x509_policy_node_free(node);
            goto err;
          }
        }
      }
      if (!x509_policy_level_add_nodes(level, new_nodes)) {
        goto err;
      }
    }

    ret = 1;
  }

err:
  sk_X509_POLICY_NODE_pop_free(new_nodes, x509_policy_node_free);
  CERTIFICATEPOLICIES_free(policies);
  return ret;
}

static int compare_issuer_policy(const POLICY_MAPPING *const *a,
                                 const POLICY_MAPPING *const *b) {
  return OBJ_cmp((*a)->issuerDomainPolicy, (*b)->issuerDomainPolicy);
}

static int compare_subject_policy(const POLICY_MAPPING *const *a,
                                  const POLICY_MAPPING *const *b) {
  return OBJ_cmp((*a)->subjectDomainPolicy, (*b)->subjectDomainPolicy);
}

static int delete_if_mapped(X509_POLICY_NODE *node, void *data) {
  const POLICY_MAPPINGS *mappings = reinterpret_cast<POLICY_MAPPINGS *>(data);

  assert(sk_POLICY_MAPPING_is_sorted(mappings));
  POLICY_MAPPING mapping;
  mapping.issuerDomainPolicy = node->policy;
  if (!sk_POLICY_MAPPING_find(mappings,               NULL, &mapping)) {
    return 0;
  }
  x509_policy_node_free(node);
  return 1;
}

static X509_POLICY_LEVEL *process_policy_mappings(const X509 *cert,
                                                  X509_POLICY_LEVEL *level,
                                                  int mapping_allowed) {
  int ok = 0;
  STACK_OF(X509_POLICY_NODE) *new_nodes = NULL;
  X509_POLICY_LEVEL *next = NULL;
  int critical;
  POLICY_MAPPINGS *mappings = reinterpret_cast<POLICY_MAPPINGS *>(
      X509_get_ext_d2i(cert, NID_policy_mappings, &critical, NULL));

  {
    if (mappings == NULL && critical != -1) {

      goto err;
    }

    if (mappings != NULL) {

      if (sk_POLICY_MAPPING_num(mappings) == 0) {
        OPENSSL_PUT_ERROR(X509, X509_R_INVALID_POLICY_EXTENSION);
        goto err;
      }

      for (size_t i = 0; i < sk_POLICY_MAPPING_num(mappings); i++) {
        POLICY_MAPPING *mapping = sk_POLICY_MAPPING_value(mappings, i);
        if (is_any_policy(mapping->issuerDomainPolicy) ||
            is_any_policy(mapping->subjectDomainPolicy)) {
          goto err;
        }
      }

      sk_POLICY_MAPPING_set_cmp_func(mappings, compare_issuer_policy);
      sk_POLICY_MAPPING_sort(mappings);

      if (mapping_allowed) {

        new_nodes = sk_X509_POLICY_NODE_new_null();
        if (new_nodes == NULL) {
          goto err;
        }
        const ASN1_OBJECT *last_policy = NULL;
        for (size_t i = 0; i < sk_POLICY_MAPPING_num(mappings); i++) {
          const POLICY_MAPPING *mapping = sk_POLICY_MAPPING_value(mappings, i);

          if (last_policy != NULL &&
              OBJ_cmp(mapping->issuerDomainPolicy, last_policy) == 0) {
            continue;
          }
          last_policy = mapping->issuerDomainPolicy;

          X509_POLICY_NODE *node =
              x509_policy_level_find(level, mapping->issuerDomainPolicy);
          if (node == NULL) {
            if (!level->has_any_policy) {
              continue;
            }
            node = x509_policy_node_new(mapping->issuerDomainPolicy);
            if (node == NULL ||
                !sk_X509_POLICY_NODE_push(new_nodes, node)) {
              x509_policy_node_free(node);
              goto err;
            }
          }
          node->mapped = 1;
        }
        if (!x509_policy_level_add_nodes(level, new_nodes)) {
          goto err;
        }
      } else {

        sk_X509_POLICY_NODE_delete_if(level->nodes, delete_if_mapped, mappings);
        sk_POLICY_MAPPING_pop_free(mappings, POLICY_MAPPING_free);
        mappings = NULL;
      }
    }

    if (mappings == NULL) {
      mappings = sk_POLICY_MAPPING_new_null();
      if (mappings == NULL) {
        goto err;
      }
    }
    for (size_t i = 0; i < sk_X509_POLICY_NODE_num(level->nodes); i++) {
      X509_POLICY_NODE *node = sk_X509_POLICY_NODE_value(level->nodes, i);
      if (!node->mapped) {
        POLICY_MAPPING *mapping = POLICY_MAPPING_new();
        if (mapping == NULL) {
          goto err;
        }
        mapping->issuerDomainPolicy = OBJ_dup(node->policy);
        mapping->subjectDomainPolicy = OBJ_dup(node->policy);
        if (mapping->issuerDomainPolicy == NULL ||
            mapping->subjectDomainPolicy == NULL ||
            !sk_POLICY_MAPPING_push(mappings, mapping)) {
          POLICY_MAPPING_free(mapping);
          goto err;
        }
      }
    }

    sk_POLICY_MAPPING_set_cmp_func(mappings, compare_subject_policy);
    sk_POLICY_MAPPING_sort(mappings);

    next = x509_policy_level_new();
    if (next == NULL) {
      goto err;
    }
    next->has_any_policy = level->has_any_policy;

    X509_POLICY_NODE *last_node = NULL;
    for (size_t i = 0; i < sk_POLICY_MAPPING_num(mappings); i++) {
      POLICY_MAPPING *mapping = sk_POLICY_MAPPING_value(mappings, i);

      if (!level->has_any_policy &&
          x509_policy_level_find(level, mapping->issuerDomainPolicy) == NULL) {
        continue;
      }

      if (last_node == NULL ||
          OBJ_cmp(last_node->policy, mapping->subjectDomainPolicy) != 0) {
        last_node = x509_policy_node_new(mapping->subjectDomainPolicy);
        if (last_node == NULL ||
            !sk_X509_POLICY_NODE_push(next->nodes, last_node)) {
          x509_policy_node_free(last_node);
          goto err;
        }
      }

      if (!sk_ASN1_OBJECT_push(last_node->parent_policies,
                               mapping->issuerDomainPolicy)) {
        goto err;
      }
      mapping->issuerDomainPolicy = NULL;
    }

    sk_X509_POLICY_NODE_sort(next->nodes);
    ok = 1;
  }

err:
  if (!ok) {
    x509_policy_level_free(next);
    next = NULL;
  }

  sk_POLICY_MAPPING_pop_free(mappings, POLICY_MAPPING_free);
  sk_X509_POLICY_NODE_pop_free(new_nodes, x509_policy_node_free);
  return next;
}

static int apply_skip_certs(const ASN1_INTEGER *skip_certs, size_t *value) {
  if (skip_certs == NULL) {
    return 1;
  }

  if (skip_certs->type & V_ASN1_NEG) {
    OPENSSL_PUT_ERROR(X509, X509_R_INVALID_POLICY_EXTENSION);
    return 0;
  }

  uint64_t u64;
  if (ASN1_INTEGER_get_uint64(&u64, skip_certs) && u64 < *value) {
    *value = (size_t)u64;
  }
  ERR_clear_error();
  return 1;
}

static int process_policy_constraints(const X509 *x509, size_t *explicit_policy,
                                      size_t *policy_mapping,
                                      size_t *inhibit_any_policy) {
  int critical;
  POLICY_CONSTRAINTS *constraints = reinterpret_cast<POLICY_CONSTRAINTS *>(
      X509_get_ext_d2i(x509, NID_policy_constraints, &critical, NULL));
  if (constraints == NULL && critical != -1) {
    return 0;
  }
  if (constraints != NULL) {
    if (constraints->requireExplicitPolicy == NULL &&
        constraints->inhibitPolicyMapping == NULL) {

      OPENSSL_PUT_ERROR(X509, X509_R_INVALID_POLICY_EXTENSION);
      POLICY_CONSTRAINTS_free(constraints);
      return 0;
    }
    int ok =
        apply_skip_certs(constraints->requireExplicitPolicy, explicit_policy) &&
        apply_skip_certs(constraints->inhibitPolicyMapping, policy_mapping);
    POLICY_CONSTRAINTS_free(constraints);
    if (!ok) {
      return 0;
    }
  }

  ASN1_INTEGER *inhibit_any_policy_ext = reinterpret_cast<ASN1_INTEGER *>(
      X509_get_ext_d2i(x509, NID_inhibit_any_policy, &critical, NULL));
  if (inhibit_any_policy_ext == NULL && critical != -1) {
    return 0;
  }
  int ok = apply_skip_certs(inhibit_any_policy_ext, inhibit_any_policy);
  ASN1_INTEGER_free(inhibit_any_policy_ext);
  return ok;
}

static int has_explicit_policy(STACK_OF(X509_POLICY_LEVEL) *levels,
                               const STACK_OF(ASN1_OBJECT) *user_policies) {
  assert(user_policies == NULL || sk_ASN1_OBJECT_is_sorted(user_policies));

  size_t num_levels = sk_X509_POLICY_LEVEL_num(levels);
  X509_POLICY_LEVEL *level = sk_X509_POLICY_LEVEL_value(levels, num_levels - 1);
  if (x509_policy_level_is_empty(level)) {
    return 0;
  }

  int user_has_any_policy = sk_ASN1_OBJECT_num(user_policies) == 0;
  for (size_t i = 0; i < sk_ASN1_OBJECT_num(user_policies); i++) {
    if (is_any_policy(sk_ASN1_OBJECT_value(user_policies, i))) {
      user_has_any_policy = 1;
      break;
    }
  }

  if (user_has_any_policy) {
    return 1;
  }

  if (level->has_any_policy) {
    return 1;
  }

  for (size_t i = 0; i < sk_X509_POLICY_NODE_num(level->nodes); i++) {
    sk_X509_POLICY_NODE_value(level->nodes, i)->reachable = 1;
  }

  for (size_t i = num_levels - 1; i < num_levels; i--) {
    level = sk_X509_POLICY_LEVEL_value(levels, i);
    for (size_t j = 0; j < sk_X509_POLICY_NODE_num(level->nodes); j++) {
      X509_POLICY_NODE *node = sk_X509_POLICY_NODE_value(level->nodes, j);
      if (!node->reachable) {
        continue;
      }
      if (sk_ASN1_OBJECT_num(node->parent_policies) == 0) {

        if (sk_ASN1_OBJECT_find(user_policies,               NULL,
                                node->policy)) {
          return 1;
        }
      } else if (i > 0) {

        X509_POLICY_LEVEL *prev = sk_X509_POLICY_LEVEL_value(levels, i - 1);
        for (size_t k = 0; k < sk_ASN1_OBJECT_num(node->parent_policies); k++) {
          X509_POLICY_NODE *parent = x509_policy_level_find(
              prev, sk_ASN1_OBJECT_value(node->parent_policies, k));
          if (parent != NULL) {
            parent->reachable = 1;
          }
        }
      }
    }
  }

  return 0;
}

static int asn1_object_cmp(const ASN1_OBJECT *const *a,
                           const ASN1_OBJECT *const *b) {
  return OBJ_cmp(*a, *b);
}

int X509_policy_check(const STACK_OF(X509) *certs,
                      const STACK_OF(ASN1_OBJECT) *user_policies,
                      unsigned long flags, X509 **out_current_cert) {
  *out_current_cert = NULL;
  int ret = X509_V_ERR_OUT_OF_MEM;
  X509_POLICY_LEVEL *level = NULL;
  STACK_OF(X509_POLICY_LEVEL) *levels = NULL;
  STACK_OF(ASN1_OBJECT) *user_policies_sorted = NULL;
  size_t num_certs = sk_X509_num(certs);

  if (num_certs <= 1) {
    return X509_V_OK;
  }

  size_t explicit_policy =
      (flags & X509_V_FLAG_EXPLICIT_POLICY) ? 0 : num_certs + 1;
  size_t inhibit_any_policy =
      (flags & X509_V_FLAG_INHIBIT_ANY) ? 0 : num_certs + 1;
  size_t policy_mapping = (flags & X509_V_FLAG_INHIBIT_MAP) ? 0 : num_certs + 1;

  levels = sk_X509_POLICY_LEVEL_new_null();
  if (levels == NULL) {
    goto err;
  }

  for (size_t i = num_certs - 2; i < num_certs; i--) {
    X509 *cert = sk_X509_value(certs, i);
    if (!x509v3_cache_extensions(cert)) {
      goto err;
    }
    const int is_self_issued = (cert->ex_flags & EXFLAG_SI) != 0;

    if (level == NULL) {
      assert(i == num_certs - 2);
      level = x509_policy_level_new();
      if (level == NULL) {
        goto err;
      }
      level->has_any_policy = 1;
    }

    const int any_policy_allowed =
        inhibit_any_policy > 0 || (i > 0 && is_self_issued);
    if (!process_certificate_policies(cert, level, any_policy_allowed)) {
      ret = X509_V_ERR_INVALID_POLICY_EXTENSION;
      *out_current_cert = cert;
      goto err;
    }

    if (explicit_policy == 0 && x509_policy_level_is_empty(level)) {
      ret = X509_V_ERR_NO_EXPLICIT_POLICY;
      goto err;
    }

    if (!sk_X509_POLICY_LEVEL_push(levels, level)) {
      goto err;
    }
    X509_POLICY_LEVEL *current_level = level;
    level = NULL;

    if (i != 0) {

      level = process_policy_mappings(cert, current_level, policy_mapping > 0);
      if (level == NULL) {
        ret = X509_V_ERR_INVALID_POLICY_EXTENSION;
        *out_current_cert = cert;
        goto err;
      }
    }

    if (i == 0 || !is_self_issued) {
      if (explicit_policy > 0) {
        explicit_policy--;
      }
      if (policy_mapping > 0) {
        policy_mapping--;
      }
      if (inhibit_any_policy > 0) {
        inhibit_any_policy--;
      }
    }
    if (!process_policy_constraints(cert, &explicit_policy, &policy_mapping,
                                    &inhibit_any_policy)) {
      ret = X509_V_ERR_INVALID_POLICY_EXTENSION;
      *out_current_cert = cert;
      goto err;
    }
  }

  if (explicit_policy == 0) {

    if (user_policies != NULL) {
      user_policies_sorted = sk_ASN1_OBJECT_dup(user_policies);
      if (user_policies_sorted == NULL) {
        goto err;
      }
      sk_ASN1_OBJECT_set_cmp_func(user_policies_sorted, asn1_object_cmp);
      sk_ASN1_OBJECT_sort(user_policies_sorted);
    }

    if (!has_explicit_policy(levels, user_policies_sorted)) {
      ret = X509_V_ERR_NO_EXPLICIT_POLICY;
      goto err;
    }
  }

  ret = X509_V_OK;

err:
  x509_policy_level_free(level);

  sk_ASN1_OBJECT_free(user_policies_sorted);
  sk_X509_POLICY_LEVEL_pop_free(levels, x509_policy_level_free);
  return ret;
}
