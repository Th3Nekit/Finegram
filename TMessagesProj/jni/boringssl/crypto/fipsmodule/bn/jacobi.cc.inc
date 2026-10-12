// Copyright 2000-2016 The OpenSSL Project Authors. All Rights Reserved.
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

#include <openssl/bn.h>

#include <openssl/err.h>

#include "internal.h"

#define BN_lsw(n) (((n)->width == 0) ? (BN_ULONG) 0 : (n)->d[0])

int bn_jacobi(const BIGNUM *a, const BIGNUM *b, BN_CTX *ctx) {

  static const int tab[8] = {0, 1, 0, -1, 0, -1, 0, 1};

  if (!BN_is_odd(b)) {
    OPENSSL_PUT_ERROR(BN, BN_R_CALLED_WITH_EVEN_MODULUS);
    return -2;
  }

  if (BN_is_negative(b)) {
    OPENSSL_PUT_ERROR(BN, BN_R_NEGATIVE_NUMBER);
    return -2;
  }

  int ret = -2;
  BN_CTX_start(ctx);
  BIGNUM *A = BN_CTX_get(ctx);
  BIGNUM *B = BN_CTX_get(ctx);
  if (B == NULL) {
    goto end;
  }

  if (!BN_copy(A, a) ||
      !BN_copy(B, b)) {
    goto end;
  }

  ret = 1;

  while (1) {

    if (BN_is_zero(A)) {
      ret = BN_is_one(B) ? ret : 0;
      goto end;
    }

    int i = 0;
    while (!BN_is_bit_set(A, i)) {
      i++;
    }
    if (!BN_rshift(A, A, i)) {
      ret = -2;
      goto end;
    }
    if (i & 1) {

      ret = ret * tab[BN_lsw(B) & 7];
    }

    if ((A->neg ? ~BN_lsw(A) : BN_lsw(A)) & BN_lsw(B) & 2) {
      ret = -ret;
    }

    if (!BN_nnmod(B, B, A, ctx)) {
      ret = -2;
      goto end;
    }
    BIGNUM *tmp = A;
    A = B;
    B = tmp;
    tmp->neg = 0;
  }

end:
  BN_CTX_end(ctx);
  return ret;
}
