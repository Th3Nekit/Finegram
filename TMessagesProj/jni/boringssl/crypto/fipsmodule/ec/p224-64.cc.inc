// Copyright 2015 The BoringSSL Authors
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

// A 64-bit implementation of the NIST P-224 elliptic curve point multiplication
//
// Inspired by Daniel J. Bernstein's public domain nistp224 implementation
// and Adam Langley's public domain 64-bit C implementation of curve25519.

#include <openssl/base.h>

#include <openssl/bn.h>
#include <openssl/ec.h>
#include <openssl/err.h>
#include <openssl/mem.h>

#include <assert.h>
#include <string.h>

#include "internal.h"
#include "../delocate.h"
#include "../../internal.h"

#if defined(BORINGSSL_HAS_UINT128) && !defined(OPENSSL_SMALL)

typedef uint64_t p224_limb;
typedef uint128_t p224_widelimb;

typedef p224_limb p224_felem[4];
typedef p224_widelimb p224_widefelem[7];

static const p224_felem g_p224_pre_comp[2][16][3] = {
    {{{0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}},
     {{0x3280d6115c1d21, 0xc1d356c2112234, 0x7f321390b94a03, 0xb70e0cbd6bb4bf},
      {0xd5819985007e34, 0x75a05a07476444, 0xfb4c22dfe6cd43, 0xbd376388b5f723},
      {1, 0, 0, 0}},
     {{0xfd9675666ebbe9, 0xbca7664d40ce5e, 0x2242df8d8a2a43, 0x1f49bbb0f99bc5},
      {0x29e0b892dc9c43, 0xece8608436e662, 0xdc858f185310d0, 0x9812dd4eb8d321},
      {1, 0, 0, 0}},
     {{0x6d3e678d5d8eb8, 0x559eed1cb362f1, 0x16e9a3bbce8a3f, 0xeedcccd8c2a748},
      {0xf19f90ed50266d, 0xabf2b4bf65f9df, 0x313865468fafec, 0x5cb379ba910a17},
      {1, 0, 0, 0}},
     {{0x0641966cab26e3, 0x91fb2991fab0a0, 0xefec27a4e13a0b, 0x0499aa8a5f8ebe},
      {0x7510407766af5d, 0x84d929610d5450, 0x81d77aae82f706, 0x6916f6d4338c5b},
      {1, 0, 0, 0}},
     {{0xea95ac3b1f15c6, 0x086000905e82d4, 0xdd323ae4d1c8b1, 0x932b56be7685a3},
      {0x9ef93dea25dbbf, 0x41665960f390f0, 0xfdec76dbe2a8a7, 0x523e80f019062a},
      {1, 0, 0, 0}},
     {{0x822fdd26732c73, 0xa01c83531b5d0f, 0x363f37347c1ba4, 0xc391b45c84725c},
      {0xbbd5e1b2d6ad24, 0xddfbcde19dfaec, 0xc393da7e222a7f, 0x1efb7890ede244},
      {1, 0, 0, 0}},
     {{0x4c9e90ca217da1, 0xd11beca79159bb, 0xff8d33c2c98b7c, 0x2610b39409f849},
      {0x44d1352ac64da0, 0xcdbb7b2c46b4fb, 0x966c079b753c89, 0xfe67e4e820b112},
      {1, 0, 0, 0}},
     {{0xe28cae2df5312d, 0xc71b61d16f5c6e, 0x79b7619a3e7c4c, 0x05c73240899b47},
      {0x9f7f6382c73e3a, 0x18615165c56bda, 0x641fab2116fd56, 0x72855882b08394},
      {1, 0, 0, 0}},
     {{0x0469182f161c09, 0x74a98ca8d00fb5, 0xb89da93489a3e0, 0x41c98768fb0c1d},
      {0xe5ea05fb32da81, 0x3dce9ffbca6855, 0x1cfe2d3fbf59e6, 0x0e5e03408738a7},
      {1, 0, 0, 0}},
     {{0xdab22b2333e87f, 0x4430137a5dd2f6, 0xe03ab9f738beb8, 0xcb0c5d0dc34f24},
      {0x764a7df0c8fda5, 0x185ba5c3fa2044, 0x9281d688bcbe50, 0xc40331df893881},
      {1, 0, 0, 0}},
     {{0xb89530796f0f60, 0xade92bd26909a3, 0x1a0c83fb4884da, 0x1765bf22a5a984},
      {0x772a9ee75db09e, 0x23bc6c67cec16f, 0x4c1edba8b14e2f, 0xe2a215d9611369},
      {1, 0, 0, 0}},
     {{0x571e509fb5efb3, 0xade88696410552, 0xc8ae85fada74fe, 0x6c7e4be83bbde3},
      {0xff9f51160f4652, 0xb47ce2495a6539, 0xa2946c53b582f4, 0x286d2db3ee9a60},
      {1, 0, 0, 0}},
     {{0x40bbd5081a44af, 0x0995183b13926c, 0xbcefba6f47f6d0, 0x215619e9cc0057},
      {0x8bc94d3b0df45e, 0xf11c54a3694f6f, 0x8631b93cdfe8b5, 0xe7e3f4b0982db9},
      {1, 0, 0, 0}},
     {{0xb17048ab3e1c7b, 0xac38f36ff8a1d8, 0x1c29819435d2c6, 0xc813132f4c07e9},
      {0x2891425503b11f, 0x08781030579fea, 0xf5426ba5cc9674, 0x1e28ebf18562bc},
      {1, 0, 0, 0}},
     {{0x9f31997cc864eb, 0x06cd91d28b5e4c, 0xff17036691a973, 0xf1aef351497c58},
      {0xdd1f2d600564ff, 0xdead073b1402db, 0x74a684435bd693, 0xeea7471f962558},
      {1, 0, 0, 0}}},
    {{{0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}},
     {{0x9665266dddf554, 0x9613d78b60ef2d, 0xce27a34cdba417, 0xd35ab74d6afc31},
      {0x85ccdd22deb15e, 0x2137e5783a6aab, 0xa141cffd8c93c6, 0x355a1830e90f2d},
      {1, 0, 0, 0}},
     {{0x1a494eadaade65, 0xd6da4da77fe53c, 0xe7992996abec86, 0x65c3553c6090e3},
      {0xfa610b1fb09346, 0xf1c6540b8a4aaf, 0xc51a13ccd3cbab, 0x02995b1b18c28a},
      {1, 0, 0, 0}},
     {{0x7874568e7295ef, 0x86b419fbe38d04, 0xdc0690a7550d9a, 0xd3966a44beac33},
      {0x2b7280ec29132f, 0xbeaa3b6a032df3, 0xdc7dd88ae41200, 0xd25e2513e3a100},
      {1, 0, 0, 0}},
     {{0x924857eb2efafd, 0xac2bce41223190, 0x8edaa1445553fc, 0x825800fd3562d5},
      {0x8d79148ea96621, 0x23a01c3dd9ed8d, 0xaf8b219f9416b5, 0xd8db0cc277daea},
      {1, 0, 0, 0}},
     {{0x76a9c3b1a700f0, 0xe9acd29bc7e691, 0x69212d1a6b0327, 0x6322e97fe154be},
      {0x469fc5465d62aa, 0x8d41ed18883b05, 0x1f8eae66c52b88, 0xe4fcbe9325be51},
      {1, 0, 0, 0}},
     {{0x825fdf583cac16, 0x020b857c7b023a, 0x683c17744b0165, 0x14ffd0a2daf2f1},
      {0x323b36184218f9, 0x4944ec4e3b47d4, 0xc15b3080841acf, 0x0bced4b01a28bb},
      {1, 0, 0, 0}},
     {{0x92ac22230df5c4, 0x52f33b4063eda8, 0xcb3f19870c0c93, 0x40064f2ba65233},
      {0xfe16f0924f8992, 0x012da25af5b517, 0x1a57bb24f723a6, 0x06f8bc76760def},
      {1, 0, 0, 0}},
     {{0x4a7084f7817cb9, 0xbcab0738ee9a78, 0x3ec11e11d9c326, 0xdc0fe90e0f1aae},
      {0xcf639ea5f98390, 0x5c350aa22ffb74, 0x9afae98a4047b7, 0x956ec2d617fc45},
      {1, 0, 0, 0}},
     {{0x4306d648c1be6a, 0x9247cd8bc9a462, 0xf5595e377d2f2e, 0xbd1c3caff1a52e},
      {0x045e14472409d0, 0x29f3e17078f773, 0x745a602b2d4f7d, 0x191837685cdfbb},
      {1, 0, 0, 0}},
     {{0x5b6ee254a8cb79, 0x4953433f5e7026, 0xe21faeb1d1def4, 0xc4c225785c09de},
      {0x307ce7bba1e518, 0x31b125b1036db8, 0x47e91868839e8f, 0xc765866e33b9f3},
      {1, 0, 0, 0}},
     {{0x3bfece24f96906, 0x4794da641e5093, 0xde5df64f95db26, 0x297ecd89714b05},
      {0x701bd3ebb2c3aa, 0x7073b4f53cb1d5, 0x13c5665658af16, 0x9895089d66fe58},
      {1, 0, 0, 0}},
     {{0x0fef05f78c4790, 0x2d773633b05d2e, 0x94229c3a951c94, 0xbbbd70df4911bb},
      {0xb2c6963d2c1168, 0x105f47a72b0d73, 0x9fdf6111614080, 0x7b7e94b39e67b0},
      {1, 0, 0, 0}},
     {{0xad1a7d6efbe2b3, 0xf012482c0da69d, 0x6b3bdf12438345, 0x40d7558d7aa4d9},
      {0x8a09fffb5c6d3d, 0x9a356e5d9ffd38, 0x5973f15f4f9b1c, 0xdcd5f59f63c3ea},
      {1, 0, 0, 0}},
     {{0xacf39f4c5ca7ab, 0x4c8071cc5fd737, 0xc64e3602cd1184, 0x0acd4644c9abba},
      {0x6c011a36d8bf6e, 0xfecd87ba24e32a, 0x19f6f56574fad8, 0x050b204ced9405},
      {1, 0, 0, 0}},
     {{0xed4f1cae7d9a96, 0x5ceef7ad94c40a, 0x778e4a3bf3ef9b, 0x7405783dc3b55e},
      {0x32477c61b6e8c6, 0xb46a97570f018b, 0x91176d0a7e95d1, 0x3df90fbc4c7d0e},
      {1, 0, 0, 0}}}};

static void p224_generic_to_felem(p224_felem out, const EC_FELEM *in) {

  out[0] = in->words[0] & 0x00ffffffffffffff;
  out[1] = ((in->words[0] >> 56) | (in->words[1] << 8)) & 0x00ffffffffffffff;
  out[2] = ((in->words[1] >> 48) | (in->words[2] << 16)) & 0x00ffffffffffffff;
  out[3] = ((in->words[2] >> 40) | (in->words[3] << 24)) & 0x00ffffffffffffff;
}

static void p224_felem_to_generic(EC_FELEM *out, const p224_felem in) {

  static const int64_t two56 = ((p224_limb)1) << 56;

  int64_t tmp[4], a;
  tmp[0] = in[0];
  tmp[1] = in[1];
  tmp[2] = in[2];
  tmp[3] = in[3];

  a = (in[3] >> 56);
  tmp[0] -= a;
  tmp[1] += a << 40;
  tmp[3] &= 0x00ffffffffffffff;

  a = ((in[3] & in[2] & (in[1] | 0x000000ffffffffff)) + 1) |
      (((int64_t)(in[0] + (in[1] & 0x000000ffffffffff)) - 1) >> 63);
  a &= 0x00ffffffffffffff;

  a = (a - 1) >> 63;

  tmp[3] &= a ^ 0xffffffffffffffff;
  tmp[2] &= a ^ 0xffffffffffffffff;
  tmp[1] &= (a ^ 0xffffffffffffffff) | 0x000000ffffffffff;
  tmp[0] -= 1 & a;

  a = tmp[0] >> 63;
  tmp[0] += two56 & a;
  tmp[1] -= 1 & a;

  tmp[2] += tmp[1] >> 56;
  tmp[1] &= 0x00ffffffffffffff;

  tmp[3] += tmp[2] >> 56;
  tmp[2] &= 0x00ffffffffffffff;

  p224_felem tmp2;
  tmp2[0] = tmp[0];
  tmp2[1] = tmp[1];
  tmp2[2] = tmp[2];
  tmp2[3] = tmp[3];

  out->words[0] = tmp2[0] | (tmp2[1] << 56);
  out->words[1] = (tmp2[1] >> 8) | (tmp2[2] << 48);
  out->words[2] = (tmp2[2] >> 16) | (tmp2[3] << 40);
  out->words[3] = tmp2[3] >> 24;
}

static void p224_felem_assign(p224_felem out, const p224_felem in) {
  out[0] = in[0];
  out[1] = in[1];
  out[2] = in[2];
  out[3] = in[3];
}

static void p224_felem_sum(p224_felem out, const p224_felem in) {
  out[0] += in[0];
  out[1] += in[1];
  out[2] += in[2];
  out[3] += in[3];
}

static void p224_felem_diff(p224_felem out, const p224_felem in) {
  static const p224_limb two58p2 =
      (((p224_limb)1) << 58) + (((p224_limb)1) << 2);
  static const p224_limb two58m2 =
      (((p224_limb)1) << 58) - (((p224_limb)1) << 2);
  static const p224_limb two58m42m2 =
      (((p224_limb)1) << 58) - (((p224_limb)1) << 42) - (((p224_limb)1) << 2);

  out[0] += two58p2;
  out[1] += two58m42m2;
  out[2] += two58m2;
  out[3] += two58m2;

  out[0] -= in[0];
  out[1] -= in[1];
  out[2] -= in[2];
  out[3] -= in[3];
}

static void p224_widefelem_diff(p224_widefelem out, const p224_widefelem in) {
  static const p224_widelimb two120 = ((p224_widelimb)1) << 120;
  static const p224_widelimb two120m64 =
      (((p224_widelimb)1) << 120) - (((p224_widelimb)1) << 64);
  static const p224_widelimb two120m104m64 = (((p224_widelimb)1) << 120) -
                                             (((p224_widelimb)1) << 104) -
                                             (((p224_widelimb)1) << 64);

  out[0] += two120;
  out[1] += two120m64;
  out[2] += two120m64;
  out[3] += two120;
  out[4] += two120m104m64;
  out[5] += two120m64;
  out[6] += two120m64;

  out[0] -= in[0];
  out[1] -= in[1];
  out[2] -= in[2];
  out[3] -= in[3];
  out[4] -= in[4];
  out[5] -= in[5];
  out[6] -= in[6];
}

static void p224_felem_diff_128_64(p224_widefelem out, const p224_felem in) {
  static const p224_widelimb two64p8 =
      (((p224_widelimb)1) << 64) + (((p224_widelimb)1) << 8);
  static const p224_widelimb two64m8 =
      (((p224_widelimb)1) << 64) - (((p224_widelimb)1) << 8);
  static const p224_widelimb two64m48m8 = (((p224_widelimb)1) << 64) -
                                          (((p224_widelimb)1) << 48) -
                                          (((p224_widelimb)1) << 8);

  out[0] += two64p8;
  out[1] += two64m48m8;
  out[2] += two64m8;
  out[3] += two64m8;

  out[0] -= in[0];
  out[1] -= in[1];
  out[2] -= in[2];
  out[3] -= in[3];
}

static void p224_felem_scalar(p224_felem out, const p224_limb scalar) {
  out[0] *= scalar;
  out[1] *= scalar;
  out[2] *= scalar;
  out[3] *= scalar;
}

static void p224_widefelem_scalar(p224_widefelem out,
                                  const p224_widelimb scalar) {
  out[0] *= scalar;
  out[1] *= scalar;
  out[2] *= scalar;
  out[3] *= scalar;
  out[4] *= scalar;
  out[5] *= scalar;
  out[6] *= scalar;
}

static void p224_felem_square(p224_widefelem out, const p224_felem in) {
  p224_limb tmp0, tmp1, tmp2;
  tmp0 = 2 * in[0];
  tmp1 = 2 * in[1];
  tmp2 = 2 * in[2];
  out[0] = ((p224_widelimb)in[0]) * in[0];
  out[1] = ((p224_widelimb)in[0]) * tmp1;
  out[2] = ((p224_widelimb)in[0]) * tmp2 + ((p224_widelimb)in[1]) * in[1];
  out[3] = ((p224_widelimb)in[3]) * tmp0 + ((p224_widelimb)in[1]) * tmp2;
  out[4] = ((p224_widelimb)in[3]) * tmp1 + ((p224_widelimb)in[2]) * in[2];
  out[5] = ((p224_widelimb)in[3]) * tmp2;
  out[6] = ((p224_widelimb)in[3]) * in[3];
}

static void p224_felem_mul(p224_widefelem out, const p224_felem in1,
                           const p224_felem in2) {
  out[0] = ((p224_widelimb)in1[0]) * in2[0];
  out[1] = ((p224_widelimb)in1[0]) * in2[1] + ((p224_widelimb)in1[1]) * in2[0];
  out[2] = ((p224_widelimb)in1[0]) * in2[2] + ((p224_widelimb)in1[1]) * in2[1] +
           ((p224_widelimb)in1[2]) * in2[0];
  out[3] = ((p224_widelimb)in1[0]) * in2[3] + ((p224_widelimb)in1[1]) * in2[2] +
           ((p224_widelimb)in1[2]) * in2[1] + ((p224_widelimb)in1[3]) * in2[0];
  out[4] = ((p224_widelimb)in1[1]) * in2[3] + ((p224_widelimb)in1[2]) * in2[2] +
           ((p224_widelimb)in1[3]) * in2[1];
  out[5] = ((p224_widelimb)in1[2]) * in2[3] + ((p224_widelimb)in1[3]) * in2[2];
  out[6] = ((p224_widelimb)in1[3]) * in2[3];
}

static void p224_felem_reduce(p224_felem out, const p224_widefelem in) {
  static const p224_widelimb two127p15 =
      (((p224_widelimb)1) << 127) + (((p224_widelimb)1) << 15);
  static const p224_widelimb two127m71 =
      (((p224_widelimb)1) << 127) - (((p224_widelimb)1) << 71);
  static const p224_widelimb two127m71m55 = (((p224_widelimb)1) << 127) -
                                            (((p224_widelimb)1) << 71) -
                                            (((p224_widelimb)1) << 55);
  p224_widelimb output[5];

  output[0] = in[0] + two127p15;
  output[1] = in[1] + two127m71m55;
  output[2] = in[2] + two127m71;
  output[3] = in[3];
  output[4] = in[4];

  output[4] += in[6] >> 16;
  output[3] += (in[6] & 0xffff) << 40;
  output[2] -= in[6];

  output[3] += in[5] >> 16;
  output[2] += (in[5] & 0xffff) << 40;
  output[1] -= in[5];

  output[2] += output[4] >> 16;
  output[1] += (output[4] & 0xffff) << 40;
  output[0] -= output[4];

  output[3] += output[2] >> 56;
  output[2] &= 0x00ffffffffffffff;

  output[4] = output[3] >> 56;
  output[3] &= 0x00ffffffffffffff;

  output[2] += output[4] >> 16;

  output[1] += (output[4] & 0xffff) << 40;
  output[0] -= output[4];

  output[1] += output[0] >> 56;
  out[0] = output[0] & 0x00ffffffffffffff;

  output[2] += output[1] >> 56;

  out[1] = output[1] & 0x00ffffffffffffff;
  output[3] += output[2] >> 56;

  out[2] = output[2] & 0x00ffffffffffffff;

  out[3] = output[3];
}

static void p224_felem_neg(p224_felem out, const p224_felem in) {
  p224_widefelem tmp = {0};
  p224_felem_diff_128_64(tmp, in);
  p224_felem_reduce(out, tmp);
}

static p224_limb p224_felem_is_zero(const p224_felem in) {
  p224_limb zero = in[0] | in[1] | in[2] | in[3];
  zero = (((int64_t)(zero)-1) >> 63) & 1;

  p224_limb two224m96p1 = (in[0] ^ 1) | (in[1] ^ 0x00ffff0000000000) |
                     (in[2] ^ 0x00ffffffffffffff) |
                     (in[3] ^ 0x00ffffffffffffff);
  two224m96p1 = (((int64_t)(two224m96p1)-1) >> 63) & 1;
  p224_limb two225m97p2 = (in[0] ^ 2) | (in[1] ^ 0x00fffe0000000000) |
                     (in[2] ^ 0x00ffffffffffffff) |
                     (in[3] ^ 0x01ffffffffffffff);
  two225m97p2 = (((int64_t)(two225m97p2)-1) >> 63) & 1;
  return (zero | two224m96p1 | two225m97p2);
}

static void p224_felem_inv(p224_felem out, const p224_felem in) {
  p224_felem ftmp, ftmp2, ftmp3, ftmp4;
  p224_widefelem tmp;

  p224_felem_square(tmp, in);
  p224_felem_reduce(ftmp, tmp);
  p224_felem_mul(tmp, in, ftmp);
  p224_felem_reduce(ftmp, tmp);
  p224_felem_square(tmp, ftmp);
  p224_felem_reduce(ftmp, tmp);
  p224_felem_mul(tmp, in, ftmp);
  p224_felem_reduce(ftmp, tmp);
  p224_felem_square(tmp, ftmp);
  p224_felem_reduce(ftmp2, tmp);
  p224_felem_square(tmp, ftmp2);
  p224_felem_reduce(ftmp2, tmp);
  p224_felem_square(tmp, ftmp2);
  p224_felem_reduce(ftmp2, tmp);
  p224_felem_mul(tmp, ftmp2, ftmp);
  p224_felem_reduce(ftmp, tmp);
  p224_felem_square(tmp, ftmp);
  p224_felem_reduce(ftmp2, tmp);
  for (size_t i = 0; i < 5; ++i) {
    p224_felem_square(tmp, ftmp2);
    p224_felem_reduce(ftmp2, tmp);
  }
  p224_felem_mul(tmp, ftmp2, ftmp);
  p224_felem_reduce(ftmp2, tmp);
  p224_felem_square(tmp, ftmp2);
  p224_felem_reduce(ftmp3, tmp);
  for (size_t i = 0; i < 11; ++i) {
    p224_felem_square(tmp, ftmp3);
    p224_felem_reduce(ftmp3, tmp);
  }
  p224_felem_mul(tmp, ftmp3, ftmp2);
  p224_felem_reduce(ftmp2, tmp);
  p224_felem_square(tmp, ftmp2);
  p224_felem_reduce(ftmp3, tmp);
  for (size_t i = 0; i < 23; ++i) {
    p224_felem_square(tmp, ftmp3);
    p224_felem_reduce(ftmp3, tmp);
  }
  p224_felem_mul(tmp, ftmp3, ftmp2);
  p224_felem_reduce(ftmp3, tmp);
  p224_felem_square(tmp, ftmp3);
  p224_felem_reduce(ftmp4, tmp);
  for (size_t i = 0; i < 47; ++i) {
    p224_felem_square(tmp, ftmp4);
    p224_felem_reduce(ftmp4, tmp);
  }
  p224_felem_mul(tmp, ftmp3, ftmp4);
  p224_felem_reduce(ftmp3, tmp);
  p224_felem_square(tmp, ftmp3);
  p224_felem_reduce(ftmp4, tmp);
  for (size_t i = 0; i < 23; ++i) {
    p224_felem_square(tmp, ftmp4);
    p224_felem_reduce(ftmp4, tmp);
  }
  p224_felem_mul(tmp, ftmp2, ftmp4);
  p224_felem_reduce(ftmp2, tmp);
  for (size_t i = 0; i < 6; ++i) {
    p224_felem_square(tmp, ftmp2);
    p224_felem_reduce(ftmp2, tmp);
  }
  p224_felem_mul(tmp, ftmp2, ftmp);
  p224_felem_reduce(ftmp, tmp);
  p224_felem_square(tmp, ftmp);
  p224_felem_reduce(ftmp, tmp);
  p224_felem_mul(tmp, ftmp, in);
  p224_felem_reduce(ftmp, tmp);
  for (size_t i = 0; i < 97; ++i) {
    p224_felem_square(tmp, ftmp);
    p224_felem_reduce(ftmp, tmp);
  }
  p224_felem_mul(tmp, ftmp, ftmp3);
  p224_felem_reduce(out, tmp);
}

static void p224_copy_conditional(p224_felem out, const p224_felem in,
                                  p224_limb icopy) {

  const p224_limb copy = -icopy;
  for (size_t i = 0; i < 4; ++i) {
    const p224_limb tmp = copy & (in[i] ^ out[i]);
    out[i] ^= tmp;
  }
}

static void p224_point_double(p224_felem x_out, p224_felem y_out,
                              p224_felem z_out, const p224_felem x_in,
                              const p224_felem y_in, const p224_felem z_in) {
  p224_widefelem tmp, tmp2;
  p224_felem delta, gamma, beta, alpha, ftmp, ftmp2;

  p224_felem_assign(ftmp, x_in);
  p224_felem_assign(ftmp2, x_in);

  p224_felem_square(tmp, z_in);
  p224_felem_reduce(delta, tmp);

  p224_felem_square(tmp, y_in);
  p224_felem_reduce(gamma, tmp);

  p224_felem_mul(tmp, x_in, gamma);
  p224_felem_reduce(beta, tmp);

  p224_felem_diff(ftmp, delta);

  p224_felem_sum(ftmp2, delta);

  p224_felem_scalar(ftmp2, 3);

  p224_felem_mul(tmp, ftmp, ftmp2);

  p224_felem_reduce(alpha, tmp);

  p224_felem_square(tmp, alpha);

  p224_felem_assign(ftmp, beta);
  p224_felem_scalar(ftmp, 8);

  p224_felem_diff_128_64(tmp, ftmp);

  p224_felem_reduce(x_out, tmp);

  p224_felem_sum(delta, gamma);

  p224_felem_assign(ftmp, y_in);
  p224_felem_sum(ftmp, z_in);

  p224_felem_square(tmp, ftmp);

  p224_felem_diff_128_64(tmp, delta);

  p224_felem_reduce(z_out, tmp);

  p224_felem_scalar(beta, 4);

  p224_felem_diff(beta, x_out);

  p224_felem_mul(tmp, alpha, beta);

  p224_felem_square(tmp2, gamma);

  p224_widefelem_scalar(tmp2, 8);

  p224_widefelem_diff(tmp, tmp2);

  p224_felem_reduce(y_out, tmp);
}

static void p224_point_add(p224_felem x3, p224_felem y3, p224_felem z3,
                           const p224_felem x1, const p224_felem y1,
                           const p224_felem z1, const int mixed,
                           const p224_felem x2, const p224_felem y2,
                           const p224_felem z2) {
  p224_felem ftmp, ftmp2, ftmp3, ftmp4, ftmp5, x_out, y_out, z_out;
  p224_widefelem tmp, tmp2;
  p224_limb z1_is_zero, z2_is_zero, x_equal, y_equal;

  if (!mixed) {

    p224_felem_square(tmp, z2);
    p224_felem_reduce(ftmp2, tmp);

    p224_felem_mul(tmp, ftmp2, z2);
    p224_felem_reduce(ftmp4, tmp);

    p224_felem_mul(tmp2, ftmp4, y1);
    p224_felem_reduce(ftmp4, tmp2);

    p224_felem_mul(tmp2, ftmp2, x1);
    p224_felem_reduce(ftmp2, tmp2);
  } else {

    p224_felem_assign(ftmp4, y1);

    p224_felem_assign(ftmp2, x1);
  }

  p224_felem_square(tmp, z1);
  p224_felem_reduce(ftmp, tmp);

  p224_felem_mul(tmp, ftmp, z1);
  p224_felem_reduce(ftmp3, tmp);

  p224_felem_mul(tmp, ftmp3, y2);

  p224_felem_diff_128_64(tmp, ftmp4);

  p224_felem_reduce(ftmp3, tmp);

  p224_felem_mul(tmp, ftmp, x2);

  p224_felem_diff_128_64(tmp, ftmp2);

  p224_felem_reduce(ftmp, tmp);

  x_equal = p224_felem_is_zero(ftmp);
  y_equal = p224_felem_is_zero(ftmp3);
  z1_is_zero = p224_felem_is_zero(z1);
  z2_is_zero = p224_felem_is_zero(z2);

  p224_limb is_nontrivial_double =
      x_equal & y_equal & (1 - z1_is_zero) & (1 - z2_is_zero);
  if (constant_time_declassify_w(is_nontrivial_double)) {
    p224_point_double(x3, y3, z3, x1, y1, z1);
    return;
  }

  if (!mixed) {
    p224_felem_mul(tmp, z1, z2);
    p224_felem_reduce(ftmp5, tmp);
  } else {

    p224_felem_assign(ftmp5, z1);
  }

  p224_felem_mul(tmp, ftmp, ftmp5);
  p224_felem_reduce(z_out, tmp);

  p224_felem_assign(ftmp5, ftmp);
  p224_felem_square(tmp, ftmp);
  p224_felem_reduce(ftmp, tmp);

  p224_felem_mul(tmp, ftmp, ftmp5);
  p224_felem_reduce(ftmp5, tmp);

  p224_felem_mul(tmp, ftmp2, ftmp);
  p224_felem_reduce(ftmp2, tmp);

  p224_felem_mul(tmp, ftmp4, ftmp5);

  p224_felem_square(tmp2, ftmp3);

  p224_felem_diff_128_64(tmp2, ftmp5);

  p224_felem_assign(ftmp5, ftmp2);
  p224_felem_scalar(ftmp5, 2);

  p224_felem_diff_128_64(tmp2, ftmp5);

  p224_felem_reduce(x_out, tmp2);

  p224_felem_diff(ftmp2, x_out);

  p224_felem_mul(tmp2, ftmp3, ftmp2);

  p224_widefelem_diff(tmp2, tmp);

  p224_felem_reduce(y_out, tmp2);

  p224_copy_conditional(x_out, x2, z1_is_zero);
  p224_copy_conditional(x_out, x1, z2_is_zero);
  p224_copy_conditional(y_out, y2, z1_is_zero);
  p224_copy_conditional(y_out, y1, z2_is_zero);
  p224_copy_conditional(z_out, z2, z1_is_zero);
  p224_copy_conditional(z_out, z1, z2_is_zero);
  p224_felem_assign(x3, x_out);
  p224_felem_assign(y3, y_out);
  p224_felem_assign(z3, z_out);
}

static void p224_select_point(const uint64_t idx, size_t size,
                              const p224_felem pre_comp[        ][3],
                              p224_felem out[3]) {
  p224_limb *outlimbs = &out[0][0];
  OPENSSL_memset(outlimbs, 0, 3 * sizeof(p224_felem));

  for (size_t i = 0; i < size; i++) {
    const p224_limb *inlimbs = &pre_comp[i][0][0];
    static_assert(sizeof(uint64_t) <= sizeof(crypto_word_t),
                  "crypto_word_t too small");
    static_assert(sizeof(size_t) <= sizeof(crypto_word_t),
                  "crypto_word_t too small");

    uint64_t mask = value_barrier_w(constant_time_eq_w(i, idx));
    for (size_t j = 0; j < 4 * 3; j++) {
      outlimbs[j] |= inlimbs[j] & mask;
    }
  }
}

static crypto_word_t p224_get_bit(const EC_SCALAR *in, size_t i) {
  if (i >= 224) {
    return 0;
  }
  static_assert(sizeof(in->words[0]) == 8, "BN_ULONG is not 64-bit");
  return (in->words[i >> 6] >> (i & 63)) & 1;
}

static int ec_GFp_nistp224_point_get_affine_coordinates(
    const EC_GROUP *group, const EC_JACOBIAN *point, EC_FELEM *x,
    EC_FELEM *y) {
  if (constant_time_declassify_int(
          ec_GFp_simple_is_at_infinity(group, point))) {
    OPENSSL_PUT_ERROR(EC, EC_R_POINT_AT_INFINITY);
    return 0;
  }

  p224_felem z1, z2;
  p224_widefelem tmp;
  p224_generic_to_felem(z1, &point->Z);
  p224_felem_inv(z2, z1);
  p224_felem_square(tmp, z2);
  p224_felem_reduce(z1, tmp);

  if (x != NULL) {
    p224_felem x_in, x_out;
    p224_generic_to_felem(x_in, &point->X);
    p224_felem_mul(tmp, x_in, z1);
    p224_felem_reduce(x_out, tmp);
    p224_felem_to_generic(x, x_out);
  }

  if (y != NULL) {
    p224_felem y_in, y_out;
    p224_generic_to_felem(y_in, &point->Y);
    p224_felem_mul(tmp, z1, z2);
    p224_felem_reduce(z1, tmp);
    p224_felem_mul(tmp, y_in, z1);
    p224_felem_reduce(y_out, tmp);
    p224_felem_to_generic(y, y_out);
  }

  return 1;
}

static void ec_GFp_nistp224_add(const EC_GROUP *group, EC_JACOBIAN *r,
                                const EC_JACOBIAN *a, const EC_JACOBIAN *b) {
  p224_felem x1, y1, z1, x2, y2, z2;
  p224_generic_to_felem(x1, &a->X);
  p224_generic_to_felem(y1, &a->Y);
  p224_generic_to_felem(z1, &a->Z);
  p224_generic_to_felem(x2, &b->X);
  p224_generic_to_felem(y2, &b->Y);
  p224_generic_to_felem(z2, &b->Z);
  p224_point_add(x1, y1, z1, x1, y1, z1, 0                    , x2, y2, z2);

  p224_felem_to_generic(&r->X, x1);
  p224_felem_to_generic(&r->Y, y1);
  p224_felem_to_generic(&r->Z, z1);
}

static void ec_GFp_nistp224_dbl(const EC_GROUP *group, EC_JACOBIAN *r,
                                const EC_JACOBIAN *a) {
  p224_felem x, y, z;
  p224_generic_to_felem(x, &a->X);
  p224_generic_to_felem(y, &a->Y);
  p224_generic_to_felem(z, &a->Z);
  p224_point_double(x, y, z, x, y, z);

  p224_felem_to_generic(&r->X, x);
  p224_felem_to_generic(&r->Y, y);
  p224_felem_to_generic(&r->Z, z);
}

static void ec_GFp_nistp224_make_precomp(p224_felem out[17][3],
                                         const EC_JACOBIAN *p) {
  OPENSSL_memset(out[0], 0, sizeof(p224_felem) * 3);

  p224_generic_to_felem(out[1][0], &p->X);
  p224_generic_to_felem(out[1][1], &p->Y);
  p224_generic_to_felem(out[1][2], &p->Z);

  for (size_t j = 2; j <= 16; ++j) {
    if (j & 1) {
      p224_point_add(out[j][0], out[j][1], out[j][2], out[1][0], out[1][1],
                     out[1][2], 0, out[j - 1][0], out[j - 1][1], out[j - 1][2]);
    } else {
      p224_point_double(out[j][0], out[j][1], out[j][2], out[j / 2][0],
                        out[j / 2][1], out[j / 2][2]);
    }
  }
}

static void ec_GFp_nistp224_point_mul(const EC_GROUP *group, EC_JACOBIAN *r,
                                      const EC_JACOBIAN *p,
                                      const EC_SCALAR *scalar) {
  p224_felem p_pre_comp[17][3];
  ec_GFp_nistp224_make_precomp(p_pre_comp, p);

  p224_felem nq[3], tmp[4];
  OPENSSL_memset(nq, 0, 3 * sizeof(p224_felem));

  int skip = 1;
  for (size_t i = 220; i < 221; i--) {
    if (!skip) {
      p224_point_double(nq[0], nq[1], nq[2], nq[0], nq[1], nq[2]);
    }

    if (i % 5 == 0) {
      crypto_word_t bits = p224_get_bit(scalar, i + 4) << 5;
      bits |= p224_get_bit(scalar, i + 3) << 4;
      bits |= p224_get_bit(scalar, i + 2) << 3;
      bits |= p224_get_bit(scalar, i + 1) << 2;
      bits |= p224_get_bit(scalar, i) << 1;
      bits |= p224_get_bit(scalar, i - 1);
      crypto_word_t sign, digit;
      ec_GFp_nistp_recode_scalar_bits(&sign, &digit, bits);

      p224_select_point(digit, 17, (const p224_felem(*)[3])p_pre_comp, tmp);
      p224_felem_neg(tmp[3], tmp[1]);
      p224_copy_conditional(tmp[1], tmp[3], sign);

      if (!skip) {
        p224_point_add(nq[0], nq[1], nq[2], nq[0], nq[1], nq[2], 0            ,
                       tmp[0], tmp[1], tmp[2]);
      } else {
        OPENSSL_memcpy(nq, tmp, 3 * sizeof(p224_felem));
        skip = 0;
      }
    }
  }

  p224_felem_to_generic(&r->X, nq[0]);
  p224_felem_to_generic(&r->Y, nq[1]);
  p224_felem_to_generic(&r->Z, nq[2]);
}

static void ec_GFp_nistp224_point_mul_base(const EC_GROUP *group,
                                           EC_JACOBIAN *r,
                                           const EC_SCALAR *scalar) {

  p224_felem nq[3], tmp[3];
  OPENSSL_memset(nq, 0, 3 * sizeof(p224_felem));

  int skip = 1;
  for (size_t i = 27; i < 28; i--) {

    if (!skip) {
      p224_point_double(nq[0], nq[1], nq[2], nq[0], nq[1], nq[2]);
    }

    crypto_word_t bits = p224_get_bit(scalar, i + 196) << 3;
    bits |= p224_get_bit(scalar, i + 140) << 2;
    bits |= p224_get_bit(scalar, i + 84) << 1;
    bits |= p224_get_bit(scalar, i + 28);

    p224_select_point(bits, 16, g_p224_pre_comp[1], tmp);

    if (!skip) {
      p224_point_add(nq[0], nq[1], nq[2], nq[0], nq[1], nq[2], 1            ,
                     tmp[0], tmp[1], tmp[2]);
    } else {
      OPENSSL_memcpy(nq, tmp, 3 * sizeof(p224_felem));
      skip = 0;
    }

    bits = p224_get_bit(scalar, i + 168) << 3;
    bits |= p224_get_bit(scalar, i + 112) << 2;
    bits |= p224_get_bit(scalar, i + 56) << 1;
    bits |= p224_get_bit(scalar, i);

    p224_select_point(bits, 16, g_p224_pre_comp[0], tmp);
    p224_point_add(nq[0], nq[1], nq[2], nq[0], nq[1], nq[2], 1            ,
                   tmp[0], tmp[1], tmp[2]);
  }

  p224_felem_to_generic(&r->X, nq[0]);
  p224_felem_to_generic(&r->Y, nq[1]);
  p224_felem_to_generic(&r->Z, nq[2]);
}

static void ec_GFp_nistp224_point_mul_public(const EC_GROUP *group,
                                             EC_JACOBIAN *r,
                                             const EC_SCALAR *g_scalar,
                                             const EC_JACOBIAN *p,
                                             const EC_SCALAR *p_scalar) {

  p224_felem p_pre_comp[17][3];
  ec_GFp_nistp224_make_precomp(p_pre_comp, p);

  p224_felem nq[3], tmp[3];
  OPENSSL_memset(nq, 0, 3 * sizeof(p224_felem));

  int skip = 1;
  for (size_t i = 220; i < 221; i--) {
    if (!skip) {
      p224_point_double(nq[0], nq[1], nq[2], nq[0], nq[1], nq[2]);
    }

    if (i <= 27) {

      crypto_word_t bits = p224_get_bit(g_scalar, i + 196) << 3;
      bits |= p224_get_bit(g_scalar, i + 140) << 2;
      bits |= p224_get_bit(g_scalar, i + 84) << 1;
      bits |= p224_get_bit(g_scalar, i + 28);

      size_t index = (size_t)bits;
      p224_point_add(nq[0], nq[1], nq[2], nq[0], nq[1], nq[2], 1            ,
                     g_p224_pre_comp[1][index][0], g_p224_pre_comp[1][index][1],
                     g_p224_pre_comp[1][index][2]);
      assert(!skip);

      bits = p224_get_bit(g_scalar, i + 168) << 3;
      bits |= p224_get_bit(g_scalar, i + 112) << 2;
      bits |= p224_get_bit(g_scalar, i + 56) << 1;
      bits |= p224_get_bit(g_scalar, i);
      index = (size_t)bits;
      p224_point_add(nq[0], nq[1], nq[2], nq[0], nq[1], nq[2], 1            ,
                     g_p224_pre_comp[0][index][0], g_p224_pre_comp[0][index][1],
                     g_p224_pre_comp[0][index][2]);
    }

    if (i % 5 == 0) {
      crypto_word_t bits = p224_get_bit(p_scalar, i + 4) << 5;
      bits |= p224_get_bit(p_scalar, i + 3) << 4;
      bits |= p224_get_bit(p_scalar, i + 2) << 3;
      bits |= p224_get_bit(p_scalar, i + 1) << 2;
      bits |= p224_get_bit(p_scalar, i) << 1;
      bits |= p224_get_bit(p_scalar, i - 1);
      crypto_word_t sign, digit;
      ec_GFp_nistp_recode_scalar_bits(&sign, &digit, bits);

      OPENSSL_memcpy(tmp, p_pre_comp[digit], 3 * sizeof(p224_felem));
      if (sign) {
        p224_felem_neg(tmp[1], tmp[1]);
      }

      if (!skip) {
        p224_point_add(nq[0], nq[1], nq[2], nq[0], nq[1], nq[2], 0            ,
                       tmp[0], tmp[1], tmp[2]);
      } else {
        OPENSSL_memcpy(nq, tmp, 3 * sizeof(p224_felem));
        skip = 0;
      }
    }
  }

  p224_felem_to_generic(&r->X, nq[0]);
  p224_felem_to_generic(&r->Y, nq[1]);
  p224_felem_to_generic(&r->Z, nq[2]);
}

static void ec_GFp_nistp224_felem_mul(const EC_GROUP *group, EC_FELEM *r,
                                      const EC_FELEM *a, const EC_FELEM *b) {
  p224_felem felem1, felem2;
  p224_widefelem wide;
  p224_generic_to_felem(felem1, a);
  p224_generic_to_felem(felem2, b);
  p224_felem_mul(wide, felem1, felem2);
  p224_felem_reduce(felem1, wide);
  p224_felem_to_generic(r, felem1);
}

static void ec_GFp_nistp224_felem_sqr(const EC_GROUP *group, EC_FELEM *r,
                                      const EC_FELEM *a) {
  p224_felem felem;
  p224_generic_to_felem(felem, a);
  p224_widefelem wide;
  p224_felem_square(wide, felem);
  p224_felem_reduce(felem, wide);
  p224_felem_to_generic(r, felem);
}

DEFINE_METHOD_FUNCTION(EC_METHOD, EC_GFp_nistp224_method) {
  out->point_get_affine_coordinates =
      ec_GFp_nistp224_point_get_affine_coordinates;
  out->add = ec_GFp_nistp224_add;
  out->dbl = ec_GFp_nistp224_dbl;
  out->mul = ec_GFp_nistp224_point_mul;
  out->mul_base = ec_GFp_nistp224_point_mul_base;
  out->mul_public = ec_GFp_nistp224_point_mul_public;
  out->felem_mul = ec_GFp_nistp224_felem_mul;
  out->felem_sqr = ec_GFp_nistp224_felem_sqr;
  out->felem_to_bytes = ec_GFp_simple_felem_to_bytes;
  out->felem_from_bytes = ec_GFp_simple_felem_from_bytes;
  out->scalar_inv0_montgomery = ec_simple_scalar_inv0_montgomery;
  out->scalar_to_montgomery_inv_vartime =
      ec_simple_scalar_to_montgomery_inv_vartime;
  out->cmp_x_coordinate = ec_GFp_simple_cmp_x_coordinate;
}

#endif
