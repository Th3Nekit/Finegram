/*
  compile with cc -DTESTING_FFTPACK fftpack.c in order to build the
  test application.

  This is an f2c translation of the full fftpack sources as found on
  http://www.netlib.org/fftpack/ The translated code has been
  slightlty edited to remove the ugliest artefacts of the translation
  (a hundred of wild GOTOs were wiped during that operation).

  The original fftpack file was written by Paul N. Swarztrauber
  (Version 4, 1985), in fortran 77.

   FFTPACK license:

   http://www.cisl.ucar.edu/css/software/fftpack5/ftpk.html

   Copyright (c) 2004 the University Corporation for Atmospheric
   Research ("UCAR"). All rights reserved. Developed by NCAR's
   Computational and Information Systems Laboratory, UCAR,
   www.cisl.ucar.edu.

   Redistribution and use of the Software in source and binary forms,
   with or without modification, is permitted provided that the
   following conditions are met:

   - Neither the names of NCAR's Computational and Information Systems
   Laboratory, the University Corporation for Atmospheric Research,
   nor the names of its sponsors or contributors may be used to
   endorse or promote products derived from this Software without
   specific prior written permission.  

   - Redistributions of source code must retain the above copyright
   notices, this list of conditions, and the disclaimer below.

   - Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions, and the disclaimer below in the
   documentation and/or other materials provided with the
   distribution.

   THIS SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
   EXPRESS OR IMPLIED, INCLUDING, BUT NOT LIMITED TO THE WARRANTIES OF
   MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
   NONINFRINGEMENT. IN NO EVENT SHALL THE CONTRIBUTORS OR COPYRIGHT
   HOLDERS BE LIABLE FOR ANY CLAIM, INDIRECT, INCIDENTAL, SPECIAL,
   EXEMPLARY, OR CONSEQUENTIAL DAMAGES OR OTHER LIABILITY, WHETHER IN AN
   ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
   CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS WITH THE
   SOFTWARE.

   ChangeLog:
   2011/10/02: this is my first release of this file.
*/

#include "fftpack.h"
#include <math.h>

typedef fftpack_real real;
typedef fftpack_int  integer;

typedef struct f77complex {
  real r, i;
} f77complex;

#ifdef TESTING_FFTPACK
static real c_abs(f77complex *c) { return sqrt(c->r*c->r + c->i*c->i); }
static double dmax(double a, double b) { return a < b ? b : a; }
#endif

static void passfb(integer *nac, integer ido, integer ip, integer l1, integer idl1,
                   real *cc, real *c1, real *c2, real *ch, real *ch2, const real *wa, real fsign)
{

  integer ch_offset, cc_offset,
    c1_offset, c2_offset, ch2_offset;

  integer i, j, k, l, jc, lc, ik, idj, idl, inc, idp;
  real wai, war;
  integer ipp2, idij, idlj, idot, ipph;

#define c1_ref(a_1,a_2,a_3) c1[((a_3)*l1 + (a_2))*ido + a_1]
#define c2_ref(a_1,a_2) c2[(a_2)*idl1 + a_1]
#define cc_ref(a_1,a_2,a_3) cc[((a_3)*ip + (a_2))*ido + a_1]
#define ch_ref(a_1,a_2,a_3) ch[((a_3)*l1 + (a_2))*ido + a_1]
#define ch2_ref(a_1,a_2) ch2[(a_2)*idl1 + a_1]

  ch_offset = 1 + ido * (1 + l1);
  ch -= ch_offset;
  c1_offset = 1 + ido * (1 + l1);
  c1 -= c1_offset;
  cc_offset = 1 + ido * (1 + ip);
  cc -= cc_offset;
  ch2_offset = 1 + idl1;
  ch2 -= ch2_offset;
  c2_offset = 1 + idl1;
  c2 -= c2_offset;
  --wa;

  idot = ido / 2;
  ipp2 = ip + 2;
  ipph = (ip + 1) / 2;
  idp = ip * ido;

  if (ido >= l1) {
    for (j = 2; j <= ipph; ++j) {
      jc = ipp2 - j;
      for (k = 1; k <= l1; ++k) {
        for (i = 1; i <= ido; ++i) {
          ch_ref(i, k, j) = cc_ref(i, j, k) + cc_ref(i, jc, k);
          ch_ref(i, k, jc) = cc_ref(i, j, k) - cc_ref(i, jc, k);
        }
      }
    }
    for (k = 1; k <= l1; ++k) {
      for (i = 1; i <= ido; ++i) {
        ch_ref(i, k, 1) = cc_ref(i, 1, k);
      }
    }
  } else {
    for (j = 2; j <= ipph; ++j) {
      jc = ipp2 - j;
      for (i = 1; i <= ido; ++i) {
        for (k = 1; k <= l1; ++k) {
          ch_ref(i, k, j) = cc_ref(i, j, k) + cc_ref(i, jc, k);
          ch_ref(i, k, jc) = cc_ref(i, j, k) - cc_ref(i, jc, k);
        }
      }
    }
    for (i = 1; i <= ido; ++i) {
      for (k = 1; k <= l1; ++k) {
        ch_ref(i, k, 1) = cc_ref(i, 1, k);
      }
    }
  }
  idl = 2 - ido;
  inc = 0;
  for (l = 2; l <= ipph; ++l) {
    lc = ipp2 - l;
    idl += ido;
    for (ik = 1; ik <= idl1; ++ik) {
      c2_ref(ik, l) = ch2_ref(ik, 1) + wa[idl - 1] * ch2_ref(ik, 2);
      c2_ref(ik, lc) = fsign*wa[idl] * ch2_ref(ik, ip);
    }
    idlj = idl;
    inc += ido;
    for (j = 3; j <= ipph; ++j) {
      jc = ipp2 - j;
      idlj += inc;
      if (idlj > idp) {
        idlj -= idp;
      }
      war = wa[idlj - 1];
      wai = wa[idlj];
      for (ik = 1; ik <= idl1; ++ik) {
        c2_ref(ik, l) = c2_ref(ik, l) + war * ch2_ref(ik, j);
        c2_ref(ik, lc) = c2_ref(ik, lc) + fsign*wai * ch2_ref(ik, jc);
      }
    }
  }
  for (j = 2; j <= ipph; ++j) {
    for (ik = 1; ik <= idl1; ++ik) {
      ch2_ref(ik, 1) = ch2_ref(ik, 1) + ch2_ref(ik, j);
    }
  }
  for (j = 2; j <= ipph; ++j) {
    jc = ipp2 - j;
    for (ik = 2; ik <= idl1; ik += 2) {
      ch2_ref(ik - 1, j) = c2_ref(ik - 1, j) - c2_ref(ik, jc);
      ch2_ref(ik - 1, jc) = c2_ref(ik - 1, j) + c2_ref(ik, jc);
      ch2_ref(ik, j) = c2_ref(ik, j) + c2_ref(ik - 1, jc);
      ch2_ref(ik, jc) = c2_ref(ik, j) - c2_ref(ik - 1, jc);
    }
  }
  *nac = 1;
  if (ido == 2) {
    return;
  }
  *nac = 0;
  for (ik = 1; ik <= idl1; ++ik) {
    c2_ref(ik, 1) = ch2_ref(ik, 1);
  }
  for (j = 2; j <= ip; ++j) {
    for (k = 1; k <= l1; ++k) {
      c1_ref(1, k, j) = ch_ref(1, k, j);
      c1_ref(2, k, j) = ch_ref(2, k, j);
    }
  }
  if (idot <= l1) {
    idij = 0;
    for (j = 2; j <= ip; ++j) {
      idij += 2;
      for (i = 4; i <= ido; i += 2) {
        idij += 2;
        for (k = 1; k <= l1; ++k) {
          c1_ref(i - 1, k, j) = wa[idij - 1] * ch_ref(i - 1, k, j) - fsign*wa[idij] * ch_ref(i, k, j);
          c1_ref(i, k, j) = wa[idij - 1] * ch_ref(i, k, j) + fsign*wa[idij] * ch_ref(i - 1, k, j);
        }
      }
    }
    return;
  }
  idj = 2 - ido;
  for (j = 2; j <= ip; ++j) {
    idj += ido;
    for (k = 1; k <= l1; ++k) {
      idij = idj;
      for (i = 4; i <= ido; i += 2) {
        idij += 2;
        c1_ref(i - 1, k, j) = wa[idij - 1] * ch_ref(i - 1, k, j) - fsign*wa[idij] * ch_ref(i, k, j);
        c1_ref(i, k, j) = wa[idij - 1] * ch_ref(i, k, j) + fsign*wa[idij] * ch_ref(i - 1, k, j);
      }
    }
  }
}

#undef ch2_ref
#undef ch_ref
#undef cc_ref
#undef c2_ref
#undef c1_ref

static void passb2(integer ido, integer l1, const real *cc, real *ch, const real *wa1)
{

  integer cc_offset, ch_offset;

  integer i, k;
  real ti2, tr2;

#define cc_ref(a_1,a_2,a_3) cc[((a_3)*2 + (a_2))*ido + a_1]
#define ch_ref(a_1,a_2,a_3) ch[((a_3)*l1 + (a_2))*ido + a_1]

  ch_offset = 1 + ido * (1 + l1);
  ch -= ch_offset;
  cc_offset = 1 + ido * 3;
  cc -= cc_offset;
  --wa1;

  if (ido <= 2) {
    for (k = 1; k <= l1; ++k) {
      ch_ref(1, k, 1) = cc_ref(1, 1, k) + cc_ref(1, 2, k);
      ch_ref(1, k, 2) = cc_ref(1, 1, k) - cc_ref(1, 2, k);
      ch_ref(2, k, 1) = cc_ref(2, 1, k) + cc_ref(2, 2, k);
      ch_ref(2, k, 2) = cc_ref(2, 1, k) - cc_ref(2, 2, k);
    }
    return;
  }
  for (k = 1; k <= l1; ++k) {
    for (i = 2; i <= ido; i += 2) {
      ch_ref(i - 1, k, 1) = cc_ref(i - 1, 1, k) + cc_ref(i - 1, 2, k);
      tr2 = cc_ref(i - 1, 1, k) - cc_ref(i - 1, 2, k);
      ch_ref(i, k, 1) = cc_ref(i, 1, k) + cc_ref(i, 2, k);
      ti2 = cc_ref(i, 1, k) - cc_ref(i, 2, k);
      ch_ref(i, k, 2) = wa1[i - 1] * ti2 + wa1[i] * tr2;
      ch_ref(i - 1, k, 2) = wa1[i - 1] * tr2 - wa1[i] * ti2;
    }
  }
}

#undef ch_ref
#undef cc_ref

static void passb3(integer ido, integer l1, const real *cc, real *ch, const real *wa1, const real *wa2)
{
  static const real taur = -.5f;
  static const real taui = .866025403784439f;

  integer cc_offset, ch_offset;

  integer i, k;
  real ci2, ci3, di2, di3, cr2, cr3, dr2, dr3, ti2, tr2;

#define cc_ref(a_1,a_2,a_3) cc[((a_3)*3 + (a_2))*ido + a_1]
#define ch_ref(a_1,a_2,a_3) ch[((a_3)*l1 + (a_2))*ido + a_1]

  ch_offset = 1 + ido * (1 + l1);
  ch -= ch_offset;
  cc_offset = 1 + (ido << 2);
  cc -= cc_offset;
  --wa1;
  --wa2;

  if (ido == 2) {
    for (k = 1; k <= l1; ++k) {
      tr2 = cc_ref(1, 2, k) + cc_ref(1, 3, k);
      cr2 = cc_ref(1, 1, k) + taur * tr2;
      ch_ref(1, k, 1) = cc_ref(1, 1, k) + tr2;
      ti2 = cc_ref(2, 2, k) + cc_ref(2, 3, k);
      ci2 = cc_ref(2, 1, k) + taur * ti2;
      ch_ref(2, k, 1) = cc_ref(2, 1, k) + ti2;
      cr3 = taui * (cc_ref(1, 2, k) - cc_ref(1, 3, k));
      ci3 = taui * (cc_ref(2, 2, k) - cc_ref(2, 3, k));
      ch_ref(1, k, 2) = cr2 - ci3;
      ch_ref(1, k, 3) = cr2 + ci3;
      ch_ref(2, k, 2) = ci2 + cr3;
      ch_ref(2, k, 3) = ci2 - cr3;
    }
  } else {
    for (k = 1; k <= l1; ++k) {
      for (i = 2; i <= ido; i += 2) {
        tr2 = cc_ref(i - 1, 2, k) + cc_ref(i - 1, 3, k);
        cr2 = cc_ref(i - 1, 1, k) + taur * tr2;
        ch_ref(i - 1, k, 1) = cc_ref(i - 1, 1, k) + tr2;
        ti2 = cc_ref(i, 2, k) + cc_ref(i, 3, k);
        ci2 = cc_ref(i, 1, k) + taur * ti2;
        ch_ref(i, k, 1) = cc_ref(i, 1, k) + ti2;
        cr3 = taui * (cc_ref(i - 1, 2, k) - cc_ref(i - 1, 3, k));
        ci3 = taui * (cc_ref(i, 2, k) - cc_ref(i, 3, k));
        dr2 = cr2 - ci3;
        dr3 = cr2 + ci3;
        di2 = ci2 + cr3;
        di3 = ci2 - cr3;
        ch_ref(i, k, 2) = wa1[i - 1] * di2 + wa1[i] * dr2;
        ch_ref(i - 1, k, 2) = wa1[i - 1] * dr2 - wa1[i] * di2;
        ch_ref(i, k, 3) = wa2[i - 1] * di3 + wa2[i] * dr3;
        ch_ref(i - 1, k, 3) = wa2[i - 1] * dr3 - wa2[i] * di3;
      }
    }
  }
}

#undef ch_ref
#undef cc_ref

static void passb4(integer ido, integer l1, const real *cc, real *ch,
                   const real *wa1, const real *wa2, const real *wa3)
{

  integer cc_offset, ch_offset;

  integer i, k;
  real ci2, ci3, ci4, cr2, cr3, cr4, ti1, ti2, ti3, ti4, tr1, tr2, tr3, tr4;

#define cc_ref(a_1,a_2,a_3) cc[((a_3)*4 + (a_2))*ido + a_1]
#define ch_ref(a_1,a_2,a_3) ch[((a_3)*l1 + (a_2))*ido + a_1]

  ch_offset = 1 + ido * (1 + l1);
  ch -= ch_offset;
  cc_offset = 1 + ido * 5;
  cc -= cc_offset;
  --wa1;
  --wa2;
  --wa3;

  if (ido == 2) {
    for (k = 1; k <= l1; ++k) {
      ti1 = cc_ref(2, 1, k) - cc_ref(2, 3, k);
      ti2 = cc_ref(2, 1, k) + cc_ref(2, 3, k);
      tr4 = cc_ref(2, 4, k) - cc_ref(2, 2, k);
      ti3 = cc_ref(2, 2, k) + cc_ref(2, 4, k);
      tr1 = cc_ref(1, 1, k) - cc_ref(1, 3, k);
      tr2 = cc_ref(1, 1, k) + cc_ref(1, 3, k);
      ti4 = cc_ref(1, 2, k) - cc_ref(1, 4, k);
      tr3 = cc_ref(1, 2, k) + cc_ref(1, 4, k);
      ch_ref(1, k, 1) = tr2 + tr3;
      ch_ref(1, k, 3) = tr2 - tr3;
      ch_ref(2, k, 1) = ti2 + ti3;
      ch_ref(2, k, 3) = ti2 - ti3;
      ch_ref(1, k, 2) = tr1 + tr4;
      ch_ref(1, k, 4) = tr1 - tr4;
      ch_ref(2, k, 2) = ti1 + ti4;
      ch_ref(2, k, 4) = ti1 - ti4;
    }
  } else {
    for (k = 1; k <= l1; ++k) {
      for (i = 2; i <= ido; i += 2) {
        ti1 = cc_ref(i, 1, k) - cc_ref(i, 3, k);
        ti2 = cc_ref(i, 1, k) + cc_ref(i, 3, k);
        ti3 = cc_ref(i, 2, k) + cc_ref(i, 4, k);
        tr4 = cc_ref(i, 4, k) - cc_ref(i, 2, k);
        tr1 = cc_ref(i - 1, 1, k) - cc_ref(i - 1, 3, k);
        tr2 = cc_ref(i - 1, 1, k) + cc_ref(i - 1, 3, k);
        ti4 = cc_ref(i - 1, 2, k) - cc_ref(i - 1, 4, k);
        tr3 = cc_ref(i - 1, 2, k) + cc_ref(i - 1, 4, k);
        ch_ref(i - 1, k, 1) = tr2 + tr3;
        cr3 = tr2 - tr3;
        ch_ref(i, k, 1) = ti2 + ti3;
        ci3 = ti2 - ti3;
        cr2 = tr1 + tr4;
        cr4 = tr1 - tr4;
        ci2 = ti1 + ti4;
        ci4 = ti1 - ti4;
        ch_ref(i - 1, k, 2) = wa1[i - 1] * cr2 - wa1[i] * ci2;
        ch_ref(i, k, 2) = wa1[i - 1] * ci2 + wa1[i] * cr2;
        ch_ref(i - 1, k, 3) = wa2[i - 1] * cr3 - wa2[i] * ci3;
        ch_ref(i, k, 3) = wa2[i - 1] * ci3 + wa2[i] * cr3;
        ch_ref(i - 1, k, 4) = wa3[i - 1] * cr4 - wa3[i] * ci4;
        ch_ref(i, k, 4) = wa3[i - 1] * ci4 + wa3[i] * cr4;
      }
    }
  }
}

#undef ch_ref
#undef cc_ref

static void passfb5(integer ido, integer l1, const real *cc, real *ch,
                    const real *wa1, const real *wa2, const real *wa3, const real *wa4, real fsign)
{
  const real tr11 = .309016994374947f;
  const real ti11 = .951056516295154f*fsign;
  const real tr12 = -.809016994374947f;
  const real ti12 = .587785252292473f*fsign;

  integer cc_offset, ch_offset;

  integer i, k;
  real ci2, ci3, ci4, ci5, di3, di4, di5, di2, cr2, cr3, cr5, cr4, ti2, ti3,
    ti4, ti5, dr3, dr4, dr5, dr2, tr2, tr3, tr4, tr5;

#define cc_ref(a_1,a_2,a_3) cc[((a_3)*5 + (a_2))*ido + a_1]
#define ch_ref(a_1,a_2,a_3) ch[((a_3)*l1 + (a_2))*ido + a_1]

  ch_offset = 1 + ido * (1 + l1);
  ch -= ch_offset;
  cc_offset = 1 + ido * 6;
  cc -= cc_offset;
  --wa1;
  --wa2;
  --wa3;
  --wa4;

  if (ido == 2) {
    for (k = 1; k <= l1; ++k) {
      ti5 = cc_ref(2, 2, k) - cc_ref(2, 5, k);
      ti2 = cc_ref(2, 2, k) + cc_ref(2, 5, k);
      ti4 = cc_ref(2, 3, k) - cc_ref(2, 4, k);
      ti3 = cc_ref(2, 3, k) + cc_ref(2, 4, k);
      tr5 = cc_ref(1, 2, k) - cc_ref(1, 5, k);
      tr2 = cc_ref(1, 2, k) + cc_ref(1, 5, k);
      tr4 = cc_ref(1, 3, k) - cc_ref(1, 4, k);
      tr3 = cc_ref(1, 3, k) + cc_ref(1, 4, k);
      ch_ref(1, k, 1) = cc_ref(1, 1, k) + tr2 + tr3;
      ch_ref(2, k, 1) = cc_ref(2, 1, k) + ti2 + ti3;
      cr2 = cc_ref(1, 1, k) + tr11 * tr2 + tr12 * tr3;
      ci2 = cc_ref(2, 1, k) + tr11 * ti2 + tr12 * ti3;
      cr3 = cc_ref(1, 1, k) + tr12 * tr2 + tr11 * tr3;
      ci3 = cc_ref(2, 1, k) + tr12 * ti2 + tr11 * ti3;
      cr5 = ti11 * tr5 + ti12 * tr4;
      ci5 = ti11 * ti5 + ti12 * ti4;
      cr4 = ti12 * tr5 - ti11 * tr4;
      ci4 = ti12 * ti5 - ti11 * ti4;
      ch_ref(1, k, 2) = cr2 - ci5;
      ch_ref(1, k, 5) = cr2 + ci5;
      ch_ref(2, k, 2) = ci2 + cr5;
      ch_ref(2, k, 3) = ci3 + cr4;
      ch_ref(1, k, 3) = cr3 - ci4;
      ch_ref(1, k, 4) = cr3 + ci4;
      ch_ref(2, k, 4) = ci3 - cr4;
      ch_ref(2, k, 5) = ci2 - cr5;
    }
  } else {
    for (k = 1; k <= l1; ++k) {
      for (i = 2; i <= ido; i += 2) {
        ti5 = cc_ref(i, 2, k) - cc_ref(i, 5, k);
        ti2 = cc_ref(i, 2, k) + cc_ref(i, 5, k);
        ti4 = cc_ref(i, 3, k) - cc_ref(i, 4, k);
        ti3 = cc_ref(i, 3, k) + cc_ref(i, 4, k);
        tr5 = cc_ref(i - 1, 2, k) - cc_ref(i - 1, 5, k);
        tr2 = cc_ref(i - 1, 2, k) + cc_ref(i - 1, 5, k);
        tr4 = cc_ref(i - 1, 3, k) - cc_ref(i - 1, 4, k);
        tr3 = cc_ref(i - 1, 3, k) + cc_ref(i - 1, 4, k);
        ch_ref(i - 1, k, 1) = cc_ref(i - 1, 1, k) + tr2 + tr3;
        ch_ref(i, k, 1) = cc_ref(i, 1, k) + ti2 + ti3;
        cr2 = cc_ref(i - 1, 1, k) + tr11 * tr2 + tr12 * tr3;
        ci2 = cc_ref(i, 1, k) + tr11 * ti2 + tr12 * ti3;
        cr3 = cc_ref(i - 1, 1, k) + tr12 * tr2 + tr11 * tr3;
        ci3 = cc_ref(i, 1, k) + tr12 * ti2 + tr11 * ti3;
        cr5 = ti11 * tr5 + ti12 * tr4;
        ci5 = ti11 * ti5 + ti12 * ti4;
        cr4 = ti12 * tr5 - ti11 * tr4;
        ci4 = ti12 * ti5 - ti11 * ti4;
        dr3 = cr3 - ci4;
        dr4 = cr3 + ci4;
        di3 = ci3 + cr4;
        di4 = ci3 - cr4;
        dr5 = cr2 + ci5;
        dr2 = cr2 - ci5;
        di5 = ci2 - cr5;
        di2 = ci2 + cr5;
        ch_ref(i - 1, k, 2) = wa1[i - 1] * dr2 - fsign*wa1[i] * di2;
        ch_ref(i, k, 2) = wa1[i - 1] * di2 + fsign*wa1[i] * dr2;
        ch_ref(i - 1, k, 3) = wa2[i - 1] * dr3 - fsign*wa2[i] * di3;
        ch_ref(i, k, 3) = wa2[i - 1] * di3 + fsign*wa2[i] * dr3;
        ch_ref(i - 1, k, 4) = wa3[i - 1] * dr4 - fsign*wa3[i] * di4;
        ch_ref(i, k, 4) = wa3[i - 1] * di4 + fsign*wa3[i] * dr4;
        ch_ref(i - 1, k, 5) = wa4[i - 1] * dr5 - fsign*wa4[i] * di5;
        ch_ref(i, k, 5) = wa4[i - 1] * di5 + fsign*wa4[i] * dr5;
      }
    }
  }
}

#undef ch_ref
#undef cc_ref

static void passf2(integer ido, integer l1, const real *cc, real *ch, const real *wa1)
{

  integer cc_offset, ch_offset;

  integer i, k;
  real ti2, tr2;

#define cc_ref(a_1,a_2,a_3) cc[((a_3)*2 + (a_2))*ido + a_1]
#define ch_ref(a_1,a_2,a_3) ch[((a_3)*l1 + (a_2))*ido + a_1]

  ch_offset = 1 + ido * (1 + l1);
  ch -= ch_offset;
  cc_offset = 1 + ido * 3;
  cc -= cc_offset;
  --wa1;

  if (ido == 2) {
    for (k = 1; k <= l1; ++k) {
      ch_ref(1, k, 1) = cc_ref(1, 1, k) + cc_ref(1, 2, k);
      ch_ref(1, k, 2) = cc_ref(1, 1, k) - cc_ref(1, 2, k);
      ch_ref(2, k, 1) = cc_ref(2, 1, k) + cc_ref(2, 2, k);
      ch_ref(2, k, 2) = cc_ref(2, 1, k) - cc_ref(2, 2, k);
    }
  } else {
    for (k = 1; k <= l1; ++k) {
      for (i = 2; i <= ido; i += 2) {
        ch_ref(i - 1, k, 1) = cc_ref(i - 1, 1, k) + cc_ref(i - 1, 2,
                                                           k);
        tr2 = cc_ref(i - 1, 1, k) - cc_ref(i - 1, 2, k);
        ch_ref(i, k, 1) = cc_ref(i, 1, k) + cc_ref(i, 2, k);
        ti2 = cc_ref(i, 1, k) - cc_ref(i, 2, k);
        ch_ref(i, k, 2) = wa1[i - 1] * ti2 - wa1[i] * tr2;
        ch_ref(i - 1, k, 2) = wa1[i - 1] * tr2 + wa1[i] * ti2;
      }
    }
  }
}

#undef ch_ref
#undef cc_ref

static void passf3(integer ido, integer l1, const real *cc, real *ch,
                   const real *wa1, const real *wa2)
{
  static const real taur = -.5f;
  static const real taui = -.866025403784439f;

  integer cc_offset, ch_offset;

  integer i, k;
  real ci2, ci3, di2, di3, cr2, cr3, dr2, dr3, ti2, tr2;

#define cc_ref(a_1,a_2,a_3) cc[((a_3)*3 + (a_2))*ido + a_1]
#define ch_ref(a_1,a_2,a_3) ch[((a_3)*l1 + (a_2))*ido + a_1]

  ch_offset = 1 + ido * (1 + l1);
  ch -= ch_offset;
  cc_offset = 1 + (ido << 2);
  cc -= cc_offset;
  --wa1;
  --wa2;

  if (ido == 2) {
    for (k = 1; k <= l1; ++k) {
      tr2 = cc_ref(1, 2, k) + cc_ref(1, 3, k);
      cr2 = cc_ref(1, 1, k) + taur * tr2;
      ch_ref(1, k, 1) = cc_ref(1, 1, k) + tr2;
      ti2 = cc_ref(2, 2, k) + cc_ref(2, 3, k);
      ci2 = cc_ref(2, 1, k) + taur * ti2;
      ch_ref(2, k, 1) = cc_ref(2, 1, k) + ti2;
      cr3 = taui * (cc_ref(1, 2, k) - cc_ref(1, 3, k));
      ci3 = taui * (cc_ref(2, 2, k) - cc_ref(2, 3, k));
      ch_ref(1, k, 2) = cr2 - ci3;
      ch_ref(1, k, 3) = cr2 + ci3;
      ch_ref(2, k, 2) = ci2 + cr3;
      ch_ref(2, k, 3) = ci2 - cr3;
    }
  } else {
    for (k = 1; k <= l1; ++k) {
      for (i = 2; i <= ido; i += 2) {
        tr2 = cc_ref(i - 1, 2, k) + cc_ref(i - 1, 3, k);
        cr2 = cc_ref(i - 1, 1, k) + taur * tr2;
        ch_ref(i - 1, k, 1) = cc_ref(i - 1, 1, k) + tr2;
        ti2 = cc_ref(i, 2, k) + cc_ref(i, 3, k);
        ci2 = cc_ref(i, 1, k) + taur * ti2;
        ch_ref(i, k, 1) = cc_ref(i, 1, k) + ti2;
        cr3 = taui * (cc_ref(i - 1, 2, k) - cc_ref(i - 1, 3, k));
        ci3 = taui * (cc_ref(i, 2, k) - cc_ref(i, 3, k));
        dr2 = cr2 - ci3;
        dr3 = cr2 + ci3;
        di2 = ci2 + cr3;
        di3 = ci2 - cr3;
        ch_ref(i, k, 2) = wa1[i - 1] * di2 - wa1[i] * dr2;
        ch_ref(i - 1, k, 2) = wa1[i - 1] * dr2 + wa1[i] * di2;
        ch_ref(i, k, 3) = wa2[i - 1] * di3 - wa2[i] * dr3;
        ch_ref(i - 1, k, 3) = wa2[i - 1] * dr3 + wa2[i] * di3;
      }
    }
  }
}

#undef ch_ref
#undef cc_ref

static void passf4(integer ido, integer l1, const real *cc, real *ch,
                   const real *wa1, const real *wa2, const real *wa3)
{

  integer cc_offset, ch_offset;

  integer i, k;
  real ci2, ci3, ci4, cr2, cr3, cr4, ti1, ti2, ti3, ti4, tr1, tr2, tr3, tr4;

#define cc_ref(a_1,a_2,a_3) cc[((a_3)*4 + (a_2))*ido + a_1]
#define ch_ref(a_1,a_2,a_3) ch[((a_3)*l1 + (a_2))*ido + a_1]

  ch_offset = 1 + ido * (1 + l1);
  ch -= ch_offset;
  cc_offset = 1 + ido * 5;
  cc -= cc_offset;
  --wa1;
  --wa2;
  --wa3;

  if (ido == 2) {
    for (k = 1; k <= l1; ++k) {
      ti1 = cc_ref(2, 1, k) - cc_ref(2, 3, k);
      ti2 = cc_ref(2, 1, k) + cc_ref(2, 3, k);
      tr4 = cc_ref(2, 2, k) - cc_ref(2, 4, k);
      ti3 = cc_ref(2, 2, k) + cc_ref(2, 4, k);
      tr1 = cc_ref(1, 1, k) - cc_ref(1, 3, k);
      tr2 = cc_ref(1, 1, k) + cc_ref(1, 3, k);
      ti4 = cc_ref(1, 4, k) - cc_ref(1, 2, k);
      tr3 = cc_ref(1, 2, k) + cc_ref(1, 4, k);
      ch_ref(1, k, 1) = tr2 + tr3;
      ch_ref(1, k, 3) = tr2 - tr3;
      ch_ref(2, k, 1) = ti2 + ti3;
      ch_ref(2, k, 3) = ti2 - ti3;
      ch_ref(1, k, 2) = tr1 + tr4;
      ch_ref(1, k, 4) = tr1 - tr4;
      ch_ref(2, k, 2) = ti1 + ti4;
      ch_ref(2, k, 4) = ti1 - ti4;
    }
  } else {
    for (k = 1; k <= l1; ++k) {
      for (i = 2; i <= ido; i += 2) {
        ti1 = cc_ref(i, 1, k) - cc_ref(i, 3, k);
        ti2 = cc_ref(i, 1, k) + cc_ref(i, 3, k);
        ti3 = cc_ref(i, 2, k) + cc_ref(i, 4, k);
        tr4 = cc_ref(i, 2, k) - cc_ref(i, 4, k);
        tr1 = cc_ref(i - 1, 1, k) - cc_ref(i - 1, 3, k);
        tr2 = cc_ref(i - 1, 1, k) + cc_ref(i - 1, 3, k);
        ti4 = cc_ref(i - 1, 4, k) - cc_ref(i - 1, 2, k);
        tr3 = cc_ref(i - 1, 2, k) + cc_ref(i - 1, 4, k);
        ch_ref(i - 1, k, 1) = tr2 + tr3;
        cr3 = tr2 - tr3;
        ch_ref(i, k, 1) = ti2 + ti3;
        ci3 = ti2 - ti3;
        cr2 = tr1 + tr4;
        cr4 = tr1 - tr4;
        ci2 = ti1 + ti4;
        ci4 = ti1 - ti4;
        ch_ref(i - 1, k, 2) = wa1[i - 1] * cr2 + wa1[i] * ci2;
        ch_ref(i, k, 2) = wa1[i - 1] * ci2 - wa1[i] * cr2;
        ch_ref(i - 1, k, 3) = wa2[i - 1] * cr3 + wa2[i] * ci3;
        ch_ref(i, k, 3) = wa2[i - 1] * ci3 - wa2[i] * cr3;
        ch_ref(i - 1, k, 4) = wa3[i - 1] * cr4 + wa3[i] * ci4;
        ch_ref(i, k, 4) = wa3[i - 1] * ci4 - wa3[i] * cr4;
      }
    }
  }
}

#undef ch_ref
#undef cc_ref

static void radb2(integer ido, integer l1, const real *cc, real *ch, const real *wa1)
{

  integer cc_offset, ch_offset;

  integer i, k, ic;
  real ti2, tr2;
  integer idp2;

#define cc_ref(a_1,a_2,a_3) cc[((a_3)*2 + (a_2))*ido + a_1]
#define ch_ref(a_1,a_2,a_3) ch[((a_3)*l1 + (a_2))*ido + a_1]

  ch_offset = 1 + ido * (1 + l1);
  ch -= ch_offset;
  cc_offset = 1 + ido * 3;
  cc -= cc_offset;
  --wa1;

  for (k = 1; k <= l1; ++k) {
    ch_ref(1, k, 1) = cc_ref(1, 1, k) + cc_ref(ido, 2, k);
    ch_ref(1, k, 2) = cc_ref(1, 1, k) - cc_ref(ido, 2, k);
  }
  if (ido < 2) return;
  else if (ido != 2) {
    idp2 = ido + 2;
    for (k = 1; k <= l1; ++k) {
      for (i = 3; i <= ido; i += 2) {
        ic = idp2 - i;
        ch_ref(i - 1, k, 1) = cc_ref(i - 1, 1, k) + cc_ref(ic - 1, 2,
                                                           k);
        tr2 = cc_ref(i - 1, 1, k) - cc_ref(ic - 1, 2, k);
        ch_ref(i, k, 1) = cc_ref(i, 1, k) - cc_ref(ic, 2, k);
        ti2 = cc_ref(i, 1, k) + cc_ref(ic, 2, k);
        ch_ref(i - 1, k, 2) = wa1[i - 2] * tr2 - wa1[i - 1] * ti2;
        ch_ref(i, k, 2) = wa1[i - 2] * ti2 + wa1[i - 1] * tr2;
      }
    }
    if (ido % 2 == 1) return;
  }
  for (k = 1; k <= l1; ++k) {
    ch_ref(ido, k, 1) = cc_ref(ido, 1, k) + cc_ref(ido, 1, k);
    ch_ref(ido, k, 2) = -(cc_ref(1, 2, k) + cc_ref(1, 2, k));
  }
}

#undef ch_ref
#undef cc_ref

static void radb3(integer ido, integer l1, const real *cc, real *ch,
                  const real *wa1, const real *wa2)
{

  static const real taur = -.5f;
  static const real taui = .866025403784439f;

  integer cc_offset, ch_offset;

  integer i, k, ic;
  real ci2, ci3, di2, di3, cr2, cr3, dr2, dr3, ti2, tr2;
  integer idp2;

#define cc_ref(a_1,a_2,a_3) cc[((a_3)*3 + (a_2))*ido + a_1]
#define ch_ref(a_1,a_2,a_3) ch[((a_3)*l1 + (a_2))*ido + a_1]

  ch_offset = 1 + ido * (1 + l1);
  ch -= ch_offset;
  cc_offset = 1 + (ido << 2);
  cc -= cc_offset;
  --wa1;
  --wa2;

  for (k = 1; k <= l1; ++k) {
    tr2 = cc_ref(ido, 2, k) + cc_ref(ido, 2, k);
    cr2 = cc_ref(1, 1, k) + taur * tr2;
    ch_ref(1, k, 1) = cc_ref(1, 1, k) + tr2;
    ci3 = taui * (cc_ref(1, 3, k) + cc_ref(1, 3, k));
    ch_ref(1, k, 2) = cr2 - ci3;
    ch_ref(1, k, 3) = cr2 + ci3;
  }
  if (ido == 1) {
    return;
  }
  idp2 = ido + 2;
  for (k = 1; k <= l1; ++k) {
    for (i = 3; i <= ido; i += 2) {
      ic = idp2 - i;
      tr2 = cc_ref(i - 1, 3, k) + cc_ref(ic - 1, 2, k);
      cr2 = cc_ref(i - 1, 1, k) + taur * tr2;
      ch_ref(i - 1, k, 1) = cc_ref(i - 1, 1, k) + tr2;
      ti2 = cc_ref(i, 3, k) - cc_ref(ic, 2, k);
      ci2 = cc_ref(i, 1, k) + taur * ti2;
      ch_ref(i, k, 1) = cc_ref(i, 1, k) + ti2;
      cr3 = taui * (cc_ref(i - 1, 3, k) - cc_ref(ic - 1, 2, k));
      ci3 = taui * (cc_ref(i, 3, k) + cc_ref(ic, 2, k));
      dr2 = cr2 - ci3;
      dr3 = cr2 + ci3;
      di2 = ci2 + cr3;
      di3 = ci2 - cr3;
      ch_ref(i - 1, k, 2) = wa1[i - 2] * dr2 - wa1[i - 1] * di2;
      ch_ref(i, k, 2) = wa1[i - 2] * di2 + wa1[i - 1] * dr2;
      ch_ref(i - 1, k, 3) = wa2[i - 2] * dr3 - wa2[i - 1] * di3;
      ch_ref(i, k, 3) = wa2[i - 2] * di3 + wa2[i - 1] * dr3;
    }
  }
}

#undef ch_ref
#undef cc_ref

static void radb4(integer ido, integer l1, const real *cc, real *ch,
                  const real *wa1, const real *wa2, const real *wa3)
{

  static const real sqrt2 = 1.414213562373095f;

  integer cc_offset, ch_offset;

  integer i, k, ic;
  real ci2, ci3, ci4, cr2, cr3, cr4, ti1, ti2, ti3, ti4, tr1, tr2, tr3, tr4;
  integer idp2;

#define cc_ref(a_1,a_2,a_3) cc[((a_3)*4 + (a_2))*ido + a_1]
#define ch_ref(a_1,a_2,a_3) ch[((a_3)*l1 + (a_2))*ido + a_1]

  ch_offset = 1 + ido * (1 + l1);
  ch -= ch_offset;
  cc_offset = 1 + ido * 5;
  cc -= cc_offset;
  --wa1;
  --wa2;
  --wa3;

  for (k = 1; k <= l1; ++k) {
    tr1 = cc_ref(1, 1, k) - cc_ref(ido, 4, k);
    tr2 = cc_ref(1, 1, k) + cc_ref(ido, 4, k);
    tr3 = cc_ref(ido, 2, k) + cc_ref(ido, 2, k);
    tr4 = cc_ref(1, 3, k) + cc_ref(1, 3, k);
    ch_ref(1, k, 1) = tr2 + tr3;
    ch_ref(1, k, 2) = tr1 - tr4;
    ch_ref(1, k, 3) = tr2 - tr3;
    ch_ref(1, k, 4) = tr1 + tr4;
  }
  if (ido < 2) return;
  if (ido != 2) {
    idp2 = ido + 2;
    for (k = 1; k <= l1; ++k) {
      for (i = 3; i <= ido; i += 2) {
        ic = idp2 - i;
        ti1 = cc_ref(i, 1, k) + cc_ref(ic, 4, k);
        ti2 = cc_ref(i, 1, k) - cc_ref(ic, 4, k);
        ti3 = cc_ref(i, 3, k) - cc_ref(ic, 2, k);
        tr4 = cc_ref(i, 3, k) + cc_ref(ic, 2, k);
        tr1 = cc_ref(i - 1, 1, k) - cc_ref(ic - 1, 4, k);
        tr2 = cc_ref(i - 1, 1, k) + cc_ref(ic - 1, 4, k);
        ti4 = cc_ref(i - 1, 3, k) - cc_ref(ic - 1, 2, k);
        tr3 = cc_ref(i - 1, 3, k) + cc_ref(ic - 1, 2, k);
        ch_ref(i - 1, k, 1) = tr2 + tr3;
        cr3 = tr2 - tr3;
        ch_ref(i, k, 1) = ti2 + ti3;
        ci3 = ti2 - ti3;
        cr2 = tr1 - tr4;
        cr4 = tr1 + tr4;
        ci2 = ti1 + ti4;
        ci4 = ti1 - ti4;
        ch_ref(i - 1, k, 2) = wa1[i - 2] * cr2 - wa1[i - 1] * ci2;
        ch_ref(i, k, 2) = wa1[i - 2] * ci2 + wa1[i - 1] * cr2;
        ch_ref(i - 1, k, 3) = wa2[i - 2] * cr3 - wa2[i - 1] * ci3;
        ch_ref(i, k, 3) = wa2[i - 2] * ci3 + wa2[i - 1] * cr3;
        ch_ref(i - 1, k, 4) = wa3[i - 2] * cr4 - wa3[i - 1] * ci4;
        ch_ref(i, k, 4) = wa3[i - 2] * ci4 + wa3[i - 1] * cr4;
      }
    }
    if (ido % 2 == 1) return;
  }
  for (k = 1; k <= l1; ++k) {
    ti1 = cc_ref(1, 2, k) + cc_ref(1, 4, k);
    ti2 = cc_ref(1, 4, k) - cc_ref(1, 2, k);
    tr1 = cc_ref(ido, 1, k) - cc_ref(ido, 3, k);
    tr2 = cc_ref(ido, 1, k) + cc_ref(ido, 3, k);
    ch_ref(ido, k, 1) = tr2 + tr2;
    ch_ref(ido, k, 2) = sqrt2 * (tr1 - ti1);
    ch_ref(ido, k, 3) = ti2 + ti2;
    ch_ref(ido, k, 4) = -sqrt2 * (tr1 + ti1);
  }
}

#undef ch_ref
#undef cc_ref

static void radb5(integer ido, integer l1, const real *cc, real *ch,
                  const real *wa1, const real *wa2, const real *wa3, const real *wa4)
{

  static const real tr11 = .309016994374947f;
  static const real ti11 = .951056516295154f;
  static const real tr12 = -.809016994374947f;
  static const real ti12 = .587785252292473f;

  integer cc_offset, ch_offset;

  integer i, k, ic;
  real ci2, ci3, ci4, ci5, di3, di4, di5, di2, cr2, cr3, cr5, cr4, ti2, ti3,
    ti4, ti5, dr3, dr4, dr5, dr2, tr2, tr3, tr4, tr5;
  integer idp2;

#define cc_ref(a_1,a_2,a_3) cc[((a_3)*5 + (a_2))*ido + a_1]
#define ch_ref(a_1,a_2,a_3) ch[((a_3)*l1 + (a_2))*ido + a_1]

  ch_offset = 1 + ido * (1 + l1);
  ch -= ch_offset;
  cc_offset = 1 + ido * 6;
  cc -= cc_offset;
  --wa1;
  --wa2;
  --wa3;
  --wa4;

  for (k = 1; k <= l1; ++k) {
    ti5 = cc_ref(1, 3, k) + cc_ref(1, 3, k);
    ti4 = cc_ref(1, 5, k) + cc_ref(1, 5, k);
    tr2 = cc_ref(ido, 2, k) + cc_ref(ido, 2, k);
    tr3 = cc_ref(ido, 4, k) + cc_ref(ido, 4, k);
    ch_ref(1, k, 1) = cc_ref(1, 1, k) + tr2 + tr3;
    cr2 = cc_ref(1, 1, k) + tr11 * tr2 + tr12 * tr3;
    cr3 = cc_ref(1, 1, k) + tr12 * tr2 + tr11 * tr3;
    ci5 = ti11 * ti5 + ti12 * ti4;
    ci4 = ti12 * ti5 - ti11 * ti4;
    ch_ref(1, k, 2) = cr2 - ci5;
    ch_ref(1, k, 3) = cr3 - ci4;
    ch_ref(1, k, 4) = cr3 + ci4;
    ch_ref(1, k, 5) = cr2 + ci5;
  }
  if (ido == 1) {
    return;
  }
  idp2 = ido + 2;
  for (k = 1; k <= l1; ++k) {
    for (i = 3; i <= ido; i += 2) {
      ic = idp2 - i;
      ti5 = cc_ref(i, 3, k) + cc_ref(ic, 2, k);
      ti2 = cc_ref(i, 3, k) - cc_ref(ic, 2, k);
      ti4 = cc_ref(i, 5, k) + cc_ref(ic, 4, k);
      ti3 = cc_ref(i, 5, k) - cc_ref(ic, 4, k);
      tr5 = cc_ref(i - 1, 3, k) - cc_ref(ic - 1, 2, k);
      tr2 = cc_ref(i - 1, 3, k) + cc_ref(ic - 1, 2, k);
      tr4 = cc_ref(i - 1, 5, k) - cc_ref(ic - 1, 4, k);
      tr3 = cc_ref(i - 1, 5, k) + cc_ref(ic - 1, 4, k);
      ch_ref(i - 1, k, 1) = cc_ref(i - 1, 1, k) + tr2 + tr3;
      ch_ref(i, k, 1) = cc_ref(i, 1, k) + ti2 + ti3;
      cr2 = cc_ref(i - 1, 1, k) + tr11 * tr2 + tr12 * tr3;
      ci2 = cc_ref(i, 1, k) + tr11 * ti2 + tr12 * ti3;
      cr3 = cc_ref(i - 1, 1, k) + tr12 * tr2 + tr11 * tr3;
      ci3 = cc_ref(i, 1, k) + tr12 * ti2 + tr11 * ti3;
      cr5 = ti11 * tr5 + ti12 * tr4;
      ci5 = ti11 * ti5 + ti12 * ti4;
      cr4 = ti12 * tr5 - ti11 * tr4;
      ci4 = ti12 * ti5 - ti11 * ti4;
      dr3 = cr3 - ci4;
      dr4 = cr3 + ci4;
      di3 = ci3 + cr4;
      di4 = ci3 - cr4;
      dr5 = cr2 + ci5;
      dr2 = cr2 - ci5;
      di5 = ci2 - cr5;
      di2 = ci2 + cr5;
      ch_ref(i - 1, k, 2) = wa1[i - 2] * dr2 - wa1[i - 1] * di2;
      ch_ref(i, k, 2) = wa1[i - 2] * di2 + wa1[i - 1] * dr2;
      ch_ref(i - 1, k, 3) = wa2[i - 2] * dr3 - wa2[i - 1] * di3;
      ch_ref(i, k, 3) = wa2[i - 2] * di3 + wa2[i - 1] * dr3;
      ch_ref(i - 1, k, 4) = wa3[i - 2] * dr4 - wa3[i - 1] * di4;
      ch_ref(i, k, 4) = wa3[i - 2] * di4 + wa3[i - 1] * dr4;
      ch_ref(i - 1, k, 5) = wa4[i - 2] * dr5 - wa4[i - 1] * di5;
      ch_ref(i, k, 5) = wa4[i - 2] * di5 + wa4[i - 1] * dr5;
    }
  }
}

#undef ch_ref
#undef cc_ref

static void radbg(integer ido, integer ip, integer l1, integer idl1,
                  const real *cc, real *c1, real *c2, real *ch, real *ch2, const real *wa)
{

  integer ch_offset, cc_offset,
    c1_offset, c2_offset, ch2_offset;

  integer i, j, k, l, j2, ic, jc, lc, ik, is;
  real dc2, ai1, ai2, ar1, ar2, ds2;
  integer nbd;
  real dcp, arg, dsp, ar1h, ar2h;
  integer idp2, ipp2, idij, ipph;

#define c1_ref(a_1,a_2,a_3) c1[((a_3)*l1 + (a_2))*ido + a_1]
#define c2_ref(a_1,a_2) c2[(a_2)*idl1 + a_1]
#define cc_ref(a_1,a_2,a_3) cc[((a_3)*ip + (a_2))*ido + a_1]
#define ch_ref(a_1,a_2,a_3) ch[((a_3)*l1 + (a_2))*ido + a_1]
#define ch2_ref(a_1,a_2) ch2[(a_2)*idl1 + a_1]

  ch_offset = 1 + ido * (1 + l1);
  ch -= ch_offset;
  c1_offset = 1 + ido * (1 + l1);
  c1 -= c1_offset;
  cc_offset = 1 + ido * (1 + ip);
  cc -= cc_offset;
  ch2_offset = 1 + idl1;
  ch2 -= ch2_offset;
  c2_offset = 1 + idl1;
  c2 -= c2_offset;
  --wa;

  arg = (2*M_PI) / (real) (ip);
  dcp = cos(arg);
  dsp = sin(arg);
  idp2 = ido + 2;
  nbd = (ido - 1) / 2;
  ipp2 = ip + 2;
  ipph = (ip + 1) / 2;
  if (ido >= l1) {
    for (k = 1; k <= l1; ++k) {
      for (i = 1; i <= ido; ++i) {
        ch_ref(i, k, 1) = cc_ref(i, 1, k);
      }
    }
  } else {
    for (i = 1; i <= ido; ++i) {
      for (k = 1; k <= l1; ++k) {
        ch_ref(i, k, 1) = cc_ref(i, 1, k);
      }
    }
  }
  for (j = 2; j <= ipph; ++j) {
    jc = ipp2 - j;
    j2 = j + j;
    for (k = 1; k <= l1; ++k) {
      ch_ref(1, k, j) = cc_ref(ido, j2 - 2, k) + cc_ref(ido, j2 - 2, k);
      ch_ref(1, k, jc) = cc_ref(1, j2 - 1, k) + cc_ref(1, j2 - 1, k);
    }
  }
  if (ido != 1) {
    if (nbd >= l1) {
      for (j = 2; j <= ipph; ++j) {
        jc = ipp2 - j;
        for (k = 1; k <= l1; ++k) {
          for (i = 3; i <= ido; i += 2) {
            ic = idp2 - i;
            ch_ref(i - 1, k, j) = cc_ref(i - 1, (j << 1) - 1, k) + cc_ref(ic - 1, (j << 1) - 2, k);
            ch_ref(i - 1, k, jc) = cc_ref(i - 1, (j << 1) - 1, k) - cc_ref(ic - 1, (j << 1) - 2, k);
            ch_ref(i, k, j) = cc_ref(i, (j << 1) - 1, k) - cc_ref(ic, (j << 1) - 2, k);
            ch_ref(i, k, jc) = cc_ref(i, (j << 1) - 1, k) + cc_ref(ic, (j << 1) - 2, k);
          }
        }
      }
    } else {
      for (j = 2; j <= ipph; ++j) {
        jc = ipp2 - j;
        for (i = 3; i <= ido; i += 2) {
          ic = idp2 - i;
          for (k = 1; k <= l1; ++k) {
            ch_ref(i - 1, k, j) = cc_ref(i - 1, (j << 1) - 1, k) + cc_ref(ic - 1, (j << 1) - 2, k);
            ch_ref(i - 1, k, jc) = cc_ref(i - 1, (j << 1) - 1, k) - cc_ref(ic - 1, (j << 1) - 2, k);
            ch_ref(i, k, j) = cc_ref(i, (j << 1) - 1, k) - cc_ref(ic, (j << 1) - 2, k);
            ch_ref(i, k, jc) = cc_ref(i, (j << 1) - 1, k) + cc_ref(ic, (j << 1) - 2, k);
          }
        }
      }
    }
  }
  ar1 = 1.f;
  ai1 = 0.f;
  for (l = 2; l <= ipph; ++l) {
    lc = ipp2 - l;
    ar1h = dcp * ar1 - dsp * ai1;
    ai1 = dcp * ai1 + dsp * ar1;
    ar1 = ar1h;
    for (ik = 1; ik <= idl1; ++ik) {
      c2_ref(ik, l) = ch2_ref(ik, 1) + ar1 * ch2_ref(ik, 2);
      c2_ref(ik, lc) = ai1 * ch2_ref(ik, ip);
    }
    dc2 = ar1;
    ds2 = ai1;
    ar2 = ar1;
    ai2 = ai1;
    for (j = 3; j <= ipph; ++j) {
      jc = ipp2 - j;
      ar2h = dc2 * ar2 - ds2 * ai2;
      ai2 = dc2 * ai2 + ds2 * ar2;
      ar2 = ar2h;
      for (ik = 1; ik <= idl1; ++ik) {
        c2_ref(ik, l) = c2_ref(ik, l) + ar2 * ch2_ref(ik, j);
        c2_ref(ik, lc) = c2_ref(ik, lc) + ai2 * ch2_ref(ik, jc);
      }
    }
  }
  for (j = 2; j <= ipph; ++j) {
    for (ik = 1; ik <= idl1; ++ik) {
      ch2_ref(ik, 1) = ch2_ref(ik, 1) + ch2_ref(ik, j);
    }
  }
  for (j = 2; j <= ipph; ++j) {
    jc = ipp2 - j;
    for (k = 1; k <= l1; ++k) {
      ch_ref(1, k, j) = c1_ref(1, k, j) - c1_ref(1, k, jc);
      ch_ref(1, k, jc) = c1_ref(1, k, j) + c1_ref(1, k, jc);
    }
  }
  if (ido != 1) {
    if (nbd >= l1) {
      for (j = 2; j <= ipph; ++j) {
        jc = ipp2 - j;
        for (k = 1; k <= l1; ++k) {
          for (i = 3; i <= ido; i += 2) {
            ch_ref(i - 1, k, j) = c1_ref(i - 1, k, j) - c1_ref(i, k, jc);
            ch_ref(i - 1, k, jc) = c1_ref(i - 1, k, j) + c1_ref(i, k, jc);
            ch_ref(i, k, j) = c1_ref(i, k, j) + c1_ref(i - 1, k, jc);
            ch_ref(i, k, jc) = c1_ref(i, k, j) - c1_ref(i - 1, k, jc);
          }
        }
      }
    } else {
      for (j = 2; j <= ipph; ++j) {
        jc = ipp2 - j;
        for (i = 3; i <= ido; i += 2) {
          for (k = 1; k <= l1; ++k) {
            ch_ref(i - 1, k, j) = c1_ref(i - 1, k, j) - c1_ref(i, k, jc);
            ch_ref(i - 1, k, jc) = c1_ref(i - 1, k, j) + c1_ref(i, k, jc);
            ch_ref(i, k, j) = c1_ref(i, k, j) + c1_ref(i - 1, k, jc);
            ch_ref(i, k, jc) = c1_ref(i, k, j) - c1_ref(i - 1, k, jc);
          }
        }
      }
    }
  }
  if (ido == 1) {
    return;
  }
  for (ik = 1; ik <= idl1; ++ik) {
    c2_ref(ik, 1) = ch2_ref(ik, 1);
  }
  for (j = 2; j <= ip; ++j) {
    for (k = 1; k <= l1; ++k) {
      c1_ref(1, k, j) = ch_ref(1, k, j);
    }
  }
  if (nbd <= l1) {
    is = -(ido);
    for (j = 2; j <= ip; ++j) {
      is += ido;
      idij = is;
      for (i = 3; i <= ido; i += 2) {
        idij += 2;
        for (k = 1; k <= l1; ++k) {
          c1_ref(i - 1, k, j) = wa[idij - 1] * ch_ref(i - 1, k, j)
            - wa[idij] * ch_ref(i, k, j);
          c1_ref(i, k, j) = wa[idij - 1] * ch_ref(i, k, j) + wa[idij] * ch_ref(i - 1, k, j);
        }
      }
    }
  } else {
    is = -(ido);
    for (j = 2; j <= ip; ++j) {
      is += ido;
      for (k = 1; k <= l1; ++k) {
        idij = is;
        for (i = 3; i <= ido; i += 2) {
          idij += 2;
          c1_ref(i - 1, k, j) = wa[idij - 1] * ch_ref(i - 1, k, j)
            - wa[idij] * ch_ref(i, k, j);
          c1_ref(i, k, j) = wa[idij - 1] * ch_ref(i, k, j) + wa[idij] * ch_ref(i - 1, k, j);
        }
      }
    }
  }
}

#undef ch2_ref
#undef ch_ref
#undef cc_ref
#undef c2_ref
#undef c1_ref

static void radf2(integer ido, integer l1, const real *cc, real *ch,
                  const real *wa1)
{

  integer ch_offset, cc_offset;

  integer i, k, ic;
  real ti2, tr2;
  integer idp2;

#define cc_ref(a_1,a_2,a_3) cc[((a_3)*l1 + (a_2))*ido + a_1]
#define ch_ref(a_1,a_2,a_3) ch[((a_3)*2 + (a_2))*ido + a_1]

  ch_offset = 1 + ido * 3;
  ch -= ch_offset;
  cc_offset = 1 + ido * (1 + l1);
  cc -= cc_offset;
  --wa1;

  for (k = 1; k <= l1; ++k) {
    ch_ref(1, 1, k) = cc_ref(1, k, 1) + cc_ref(1, k, 2);
    ch_ref(ido, 2, k) = cc_ref(1, k, 1) - cc_ref(1, k, 2);
  }
  if (ido < 2) return;
  if (ido != 2) {
    idp2 = ido + 2;
    for (k = 1; k <= l1; ++k) {
      for (i = 3; i <= ido; i += 2) {
        ic = idp2 - i;
        tr2 = wa1[i - 2] * cc_ref(i - 1, k, 2) + wa1[i - 1] *
          cc_ref(i, k, 2);
        ti2 = wa1[i - 2] * cc_ref(i, k, 2) - wa1[i - 1] * cc_ref(
                                                                 i - 1, k, 2);
        ch_ref(i, 1, k) = cc_ref(i, k, 1) + ti2;
        ch_ref(ic, 2, k) = ti2 - cc_ref(i, k, 1);
        ch_ref(i - 1, 1, k) = cc_ref(i - 1, k, 1) + tr2;
        ch_ref(ic - 1, 2, k) = cc_ref(i - 1, k, 1) - tr2;
      }
    }
    if (ido % 2 == 1) {
      return;
    }
  }
  for (k = 1; k <= l1; ++k) {
    ch_ref(1, 2, k) = -cc_ref(ido, k, 2);
    ch_ref(ido, 1, k) = cc_ref(ido, k, 1);
  }
}

#undef ch_ref
#undef cc_ref

static void radf3(integer ido, integer l1, const real *cc, real *ch,
                  const real *wa1, const real *wa2)
{
  static const real taur = -.5f;
  static const real taui = .866025403784439f;

  integer ch_offset, cc_offset;

  integer i, k, ic;
  real ci2, di2, di3, cr2, dr2, dr3, ti2, ti3, tr2, tr3;
  integer idp2;

#define cc_ref(a_1,a_2,a_3) cc[((a_3)*l1 + (a_2))*ido + a_1]
#define ch_ref(a_1,a_2,a_3) ch[((a_3)*3 + (a_2))*ido + a_1]

  ch_offset = 1 + (ido << 2);
  ch -= ch_offset;
  cc_offset = 1 + ido * (1 + l1);
  cc -= cc_offset;
  --wa1;
  --wa2;

  for (k = 1; k <= l1; ++k) {
    cr2 = cc_ref(1, k, 2) + cc_ref(1, k, 3);
    ch_ref(1, 1, k) = cc_ref(1, k, 1) + cr2;
    ch_ref(1, 3, k) = taui * (cc_ref(1, k, 3) - cc_ref(1, k, 2));
    ch_ref(ido, 2, k) = cc_ref(1, k, 1) + taur * cr2;
  }
  if (ido == 1) {
    return;
  }
  idp2 = ido + 2;
  for (k = 1; k <= l1; ++k) {
    for (i = 3; i <= ido; i += 2) {
      ic = idp2 - i;
      dr2 = wa1[i - 2] * cc_ref(i - 1, k, 2) + wa1[i - 1] *
        cc_ref(i, k, 2);
      di2 = wa1[i - 2] * cc_ref(i, k, 2) - wa1[i - 1] * cc_ref(
                                                               i - 1, k, 2);
      dr3 = wa2[i - 2] * cc_ref(i - 1, k, 3) + wa2[i - 1] *
        cc_ref(i, k, 3);
      di3 = wa2[i - 2] * cc_ref(i, k, 3) - wa2[i - 1] * cc_ref(
                                                               i - 1, k, 3);
      cr2 = dr2 + dr3;
      ci2 = di2 + di3;
      ch_ref(i - 1, 1, k) = cc_ref(i - 1, k, 1) + cr2;
      ch_ref(i, 1, k) = cc_ref(i, k, 1) + ci2;
      tr2 = cc_ref(i - 1, k, 1) + taur * cr2;
      ti2 = cc_ref(i, k, 1) + taur * ci2;
      tr3 = taui * (di2 - di3);
      ti3 = taui * (dr3 - dr2);
      ch_ref(i - 1, 3, k) = tr2 + tr3;
      ch_ref(ic - 1, 2, k) = tr2 - tr3;
      ch_ref(i, 3, k) = ti2 + ti3;
      ch_ref(ic, 2, k) = ti3 - ti2;
    }
  }
}

#undef ch_ref
#undef cc_ref

static void radf4(integer ido, integer l1, const real *cc, real *ch,
                  const real *wa1, const real *wa2, const real *wa3)
{

  static const real hsqt2 = .7071067811865475f;

  integer cc_offset, ch_offset;

  integer i, k, ic;
  real ci2, ci3, ci4, cr2, cr3, cr4, ti1, ti2, ti3, ti4, tr1, tr2, tr3, tr4;
  integer idp2;

#define cc_ref(a_1,a_2,a_3) cc[((a_3)*l1 + (a_2))*ido + a_1]
#define ch_ref(a_1,a_2,a_3) ch[((a_3)*4 + (a_2))*ido + a_1]

  ch_offset = 1 + ido * 5;
  ch -= ch_offset;
  cc_offset = 1 + ido * (1 + l1);
  cc -= cc_offset;
  --wa1;
  --wa2;
  --wa3;

  for (k = 1; k <= l1; ++k) {
    tr1 = cc_ref(1, k, 2) + cc_ref(1, k, 4);
    tr2 = cc_ref(1, k, 1) + cc_ref(1, k, 3);
    ch_ref(1, 1, k) = tr1 + tr2;
    ch_ref(ido, 4, k) = tr2 - tr1;
    ch_ref(ido, 2, k) = cc_ref(1, k, 1) - cc_ref(1, k, 3);
    ch_ref(1, 3, k) = cc_ref(1, k, 4) - cc_ref(1, k, 2);
  }
  if (ido < 2) return;
  if (ido != 2) {
    idp2 = ido + 2;
    for (k = 1; k <= l1; ++k) {
      for (i = 3; i <= ido; i += 2) {
        ic = idp2 - i;
        cr2 = wa1[i - 2] * cc_ref(i - 1, k, 2) + wa1[i - 1] *
          cc_ref(i, k, 2);
        ci2 = wa1[i - 2] * cc_ref(i, k, 2) - wa1[i - 1] * cc_ref(
                                                                 i - 1, k, 2);
        cr3 = wa2[i - 2] * cc_ref(i - 1, k, 3) + wa2[i - 1] *
          cc_ref(i, k, 3);
        ci3 = wa2[i - 2] * cc_ref(i, k, 3) - wa2[i - 1] * cc_ref(
                                                                 i - 1, k, 3);
        cr4 = wa3[i - 2] * cc_ref(i - 1, k, 4) + wa3[i - 1] *
          cc_ref(i, k, 4);
        ci4 = wa3[i - 2] * cc_ref(i, k, 4) - wa3[i - 1] * cc_ref(
                                                                 i - 1, k, 4);
        tr1 = cr2 + cr4;
        tr4 = cr4 - cr2;
        ti1 = ci2 + ci4;
        ti4 = ci2 - ci4;
        ti2 = cc_ref(i, k, 1) + ci3;
        ti3 = cc_ref(i, k, 1) - ci3;
        tr2 = cc_ref(i - 1, k, 1) + cr3;
        tr3 = cc_ref(i - 1, k, 1) - cr3;
        ch_ref(i - 1, 1, k) = tr1 + tr2;
        ch_ref(ic - 1, 4, k) = tr2 - tr1;
        ch_ref(i, 1, k) = ti1 + ti2;
        ch_ref(ic, 4, k) = ti1 - ti2;
        ch_ref(i - 1, 3, k) = ti4 + tr3;
        ch_ref(ic - 1, 2, k) = tr3 - ti4;
        ch_ref(i, 3, k) = tr4 + ti3;
        ch_ref(ic, 2, k) = tr4 - ti3;
      }
    }
    if (ido % 2 == 1) {
      return;
    }
  }
  for (k = 1; k <= l1; ++k) {
    ti1 = -hsqt2 * (cc_ref(ido, k, 2) + cc_ref(ido, k, 4));
    tr1 = hsqt2 * (cc_ref(ido, k, 2) - cc_ref(ido, k, 4));
    ch_ref(ido, 1, k) = tr1 + cc_ref(ido, k, 1);
    ch_ref(ido, 3, k) = cc_ref(ido, k, 1) - tr1;
    ch_ref(1, 2, k) = ti1 - cc_ref(ido, k, 3);
    ch_ref(1, 4, k) = ti1 + cc_ref(ido, k, 3);
  }
}

#undef ch_ref
#undef cc_ref

static void radf5(integer ido, integer l1, const real *cc, real *ch,
                  const real *wa1, const real *wa2, const real *wa3, const real *wa4)
{

  static const real tr11 = .309016994374947f;
  static const real ti11 = .951056516295154f;
  static const real tr12 = -.809016994374947f;
  static const real ti12 = .587785252292473f;

  integer cc_offset, ch_offset;

  integer i, k, ic;
  real ci2, di2, ci4, ci5, di3, di4, di5, ci3, cr2, cr3, dr2, dr3, dr4, dr5,
    cr5, cr4, ti2, ti3, ti5, ti4, tr2, tr3, tr4, tr5;
  integer idp2;

#define cc_ref(a_1,a_2,a_3) cc[((a_3)*l1 + (a_2))*ido + a_1]
#define ch_ref(a_1,a_2,a_3) ch[((a_3)*5 + (a_2))*ido + a_1]

  ch_offset = 1 + ido * 6;
  ch -= ch_offset;
  cc_offset = 1 + ido * (1 + l1);
  cc -= cc_offset;
  --wa1;
  --wa2;
  --wa3;
  --wa4;

  for (k = 1; k <= l1; ++k) {
    cr2 = cc_ref(1, k, 5) + cc_ref(1, k, 2);
    ci5 = cc_ref(1, k, 5) - cc_ref(1, k, 2);
    cr3 = cc_ref(1, k, 4) + cc_ref(1, k, 3);
    ci4 = cc_ref(1, k, 4) - cc_ref(1, k, 3);
    ch_ref(1, 1, k) = cc_ref(1, k, 1) + cr2 + cr3;
    ch_ref(ido, 2, k) = cc_ref(1, k, 1) + tr11 * cr2 + tr12 * cr3;
    ch_ref(1, 3, k) = ti11 * ci5 + ti12 * ci4;
    ch_ref(ido, 4, k) = cc_ref(1, k, 1) + tr12 * cr2 + tr11 * cr3;
    ch_ref(1, 5, k) = ti12 * ci5 - ti11 * ci4;
  }
  if (ido == 1) {
    return;
  }
  idp2 = ido + 2;
  for (k = 1; k <= l1; ++k) {
    for (i = 3; i <= ido; i += 2) {
      ic = idp2 - i;
      dr2 = wa1[i - 2] * cc_ref(i - 1, k, 2) + wa1[i - 1] * cc_ref(i, k, 2);
      di2 = wa1[i - 2] * cc_ref(i, k, 2) - wa1[i - 1] * cc_ref(i - 1, k, 2);
      dr3 = wa2[i - 2] * cc_ref(i - 1, k, 3) + wa2[i - 1] * cc_ref(i, k, 3);
      di3 = wa2[i - 2] * cc_ref(i, k, 3) - wa2[i - 1] * cc_ref(i - 1, k, 3);
      dr4 = wa3[i - 2] * cc_ref(i - 1, k, 4) + wa3[i - 1] * cc_ref(i, k, 4);
      di4 = wa3[i - 2] * cc_ref(i, k, 4) - wa3[i - 1] * cc_ref(i - 1, k, 4);
      dr5 = wa4[i - 2] * cc_ref(i - 1, k, 5) + wa4[i - 1] * cc_ref(i, k, 5);
      di5 = wa4[i - 2] * cc_ref(i, k, 5) - wa4[i - 1] * cc_ref(i - 1, k, 5);
      cr2 = dr2 + dr5;
      ci5 = dr5 - dr2;
      cr5 = di2 - di5;
      ci2 = di2 + di5;
      cr3 = dr3 + dr4;
      ci4 = dr4 - dr3;
      cr4 = di3 - di4;
      ci3 = di3 + di4;
      ch_ref(i - 1, 1, k) = cc_ref(i - 1, k, 1) + cr2 + cr3;
      ch_ref(i, 1, k) = cc_ref(i, k, 1) + ci2 + ci3;
      tr2 = cc_ref(i - 1, k, 1) + tr11 * cr2 + tr12 * cr3;
      ti2 = cc_ref(i, k, 1) + tr11 * ci2 + tr12 * ci3;
      tr3 = cc_ref(i - 1, k, 1) + tr12 * cr2 + tr11 * cr3;
      ti3 = cc_ref(i, k, 1) + tr12 * ci2 + tr11 * ci3;
      tr5 = ti11 * cr5 + ti12 * cr4;
      ti5 = ti11 * ci5 + ti12 * ci4;
      tr4 = ti12 * cr5 - ti11 * cr4;
      ti4 = ti12 * ci5 - ti11 * ci4;
      ch_ref(i - 1, 3, k) = tr2 + tr5;
      ch_ref(ic - 1, 2, k) = tr2 - tr5;
      ch_ref(i, 3, k) = ti2 + ti5;
      ch_ref(ic, 2, k) = ti5 - ti2;
      ch_ref(i - 1, 5, k) = tr3 + tr4;
      ch_ref(ic - 1, 4, k) = tr3 - tr4;
      ch_ref(i, 5, k) = ti3 + ti4;
      ch_ref(ic, 4, k) = ti4 - ti3;
    }
  }
}

#undef ch_ref
#undef cc_ref

static void radfg(integer ido, integer ip, integer l1, integer idl1,
                  real *cc, real *c1, real *c2, real *ch, real *ch2, const real *wa)
{

  integer ch_offset, cc_offset,
    c1_offset, c2_offset, ch2_offset;

  integer i, j, k, l, j2, ic, jc, lc, ik, is;
  real dc2, ai1, ai2, ar1, ar2, ds2;
  integer nbd;
  real dcp, arg, dsp, ar1h, ar2h;
  integer idp2, ipp2, idij, ipph;

#define c1_ref(a_1,a_2,a_3) c1[((a_3)*l1 + (a_2))*ido + a_1]
#define c2_ref(a_1,a_2) c2[(a_2)*idl1 + a_1]
#define cc_ref(a_1,a_2,a_3) cc[((a_3)*ip + (a_2))*ido + a_1]
#define ch_ref(a_1,a_2,a_3) ch[((a_3)*l1 + (a_2))*ido + a_1]
#define ch2_ref(a_1,a_2) ch2[(a_2)*idl1 + a_1]

  ch_offset = 1 + ido * (1 + l1);
  ch -= ch_offset;
  c1_offset = 1 + ido * (1 + l1);
  c1 -= c1_offset;
  cc_offset = 1 + ido * (1 + ip);
  cc -= cc_offset;
  ch2_offset = 1 + idl1;
  ch2 -= ch2_offset;
  c2_offset = 1 + idl1;
  c2 -= c2_offset;
  --wa;

  arg = (2*M_PI) / (real) (ip);
  dcp = cos(arg);
  dsp = sin(arg);
  ipph = (ip + 1) / 2;
  ipp2 = ip + 2;
  idp2 = ido + 2;
  nbd = (ido - 1) / 2;
  if (ido == 1) {
    for (ik = 1; ik <= idl1; ++ik) {
      c2_ref(ik, 1) = ch2_ref(ik, 1);
    }
  } else {
    for (ik = 1; ik <= idl1; ++ik) {
      ch2_ref(ik, 1) = c2_ref(ik, 1);
    }
    for (j = 2; j <= ip; ++j) {
      for (k = 1; k <= l1; ++k) {
        ch_ref(1, k, j) = c1_ref(1, k, j);
      }
    }
    if (nbd <= l1) {
      is = -(ido);
      for (j = 2; j <= ip; ++j) {
        is += ido;
        idij = is;
        for (i = 3; i <= ido; i += 2) {
          idij += 2;
          for (k = 1; k <= l1; ++k) {
            ch_ref(i - 1, k, j) = wa[idij - 1] * c1_ref(i - 1, k, j)
              + wa[idij] * c1_ref(i, k, j);
            ch_ref(i, k, j) = wa[idij - 1] * c1_ref(i, k, j) - wa[
                                                                  idij] * c1_ref(i - 1, k, j);
          }
        }
      }
    } else {
      is = -(ido);
      for (j = 2; j <= ip; ++j) {
        is += ido;
        for (k = 1; k <= l1; ++k) {
          idij = is;
          for (i = 3; i <= ido; i += 2) {
            idij += 2;
            ch_ref(i - 1, k, j) = wa[idij - 1] * c1_ref(i - 1, k, j)
              + wa[idij] * c1_ref(i, k, j);
            ch_ref(i, k, j) = wa[idij - 1] * c1_ref(i, k, j) - wa[
                                                                  idij] * c1_ref(i - 1, k, j);
          }
        }
      }
    }
    if (nbd >= l1) {
      for (j = 2; j <= ipph; ++j) {
        jc = ipp2 - j;
        for (k = 1; k <= l1; ++k) {
          for (i = 3; i <= ido; i += 2) {
            c1_ref(i - 1, k, j) = ch_ref(i - 1, k, j) + ch_ref(i -
                                                               1, k, jc);
            c1_ref(i - 1, k, jc) = ch_ref(i, k, j) - ch_ref(i, k,
                                                            jc);
            c1_ref(i, k, j) = ch_ref(i, k, j) + ch_ref(i, k, jc);
            c1_ref(i, k, jc) = ch_ref(i - 1, k, jc) - ch_ref(i - 1,
                                                             k, j);
          }
        }
      }
    } else {
      for (j = 2; j <= ipph; ++j) {
        jc = ipp2 - j;
        for (i = 3; i <= ido; i += 2) {
          for (k = 1; k <= l1; ++k) {
            c1_ref(i - 1, k, j) = ch_ref(i - 1, k, j) + ch_ref(i -
                                                               1, k, jc);
            c1_ref(i - 1, k, jc) = ch_ref(i, k, j) - ch_ref(i, k,
                                                            jc);
            c1_ref(i, k, j) = ch_ref(i, k, j) + ch_ref(i, k, jc);
            c1_ref(i, k, jc) = ch_ref(i - 1, k, jc) - ch_ref(i - 1,
                                                             k, j);
          }
        }
      }
    }
  }
  for (j = 2; j <= ipph; ++j) {
    jc = ipp2 - j;
    for (k = 1; k <= l1; ++k) {
      c1_ref(1, k, j) = ch_ref(1, k, j) + ch_ref(1, k, jc);
      c1_ref(1, k, jc) = ch_ref(1, k, jc) - ch_ref(1, k, j);
    }
  }

  ar1 = 1.f;
  ai1 = 0.f;
  for (l = 2; l <= ipph; ++l) {
    lc = ipp2 - l;
    ar1h = dcp * ar1 - dsp * ai1;
    ai1 = dcp * ai1 + dsp * ar1;
    ar1 = ar1h;
    for (ik = 1; ik <= idl1; ++ik) {
      ch2_ref(ik, l) = c2_ref(ik, 1) + ar1 * c2_ref(ik, 2);
      ch2_ref(ik, lc) = ai1 * c2_ref(ik, ip);
    }
    dc2 = ar1;
    ds2 = ai1;
    ar2 = ar1;
    ai2 = ai1;
    for (j = 3; j <= ipph; ++j) {
      jc = ipp2 - j;
      ar2h = dc2 * ar2 - ds2 * ai2;
      ai2 = dc2 * ai2 + ds2 * ar2;
      ar2 = ar2h;
      for (ik = 1; ik <= idl1; ++ik) {
        ch2_ref(ik, l) = ch2_ref(ik, l) + ar2 * c2_ref(ik, j);
        ch2_ref(ik, lc) = ch2_ref(ik, lc) + ai2 * c2_ref(ik, jc);
      }
    }
  }
  for (j = 2; j <= ipph; ++j) {
    for (ik = 1; ik <= idl1; ++ik) {
      ch2_ref(ik, 1) = ch2_ref(ik, 1) + c2_ref(ik, j);
    }
  }

  if (ido >= l1) {
    for (k = 1; k <= l1; ++k) {
      for (i = 1; i <= ido; ++i) {
        cc_ref(i, 1, k) = ch_ref(i, k, 1);
      }
    }
  } else {
    for (i = 1; i <= ido; ++i) {
      for (k = 1; k <= l1; ++k) {
        cc_ref(i, 1, k) = ch_ref(i, k, 1);
      }
    }
  }
  for (j = 2; j <= ipph; ++j) {
    jc = ipp2 - j;
    j2 = j + j;
    for (k = 1; k <= l1; ++k) {
      cc_ref(ido, j2 - 2, k) = ch_ref(1, k, j);
      cc_ref(1, j2 - 1, k) = ch_ref(1, k, jc);
    }
  }
  if (ido == 1) {
    return;
  }
  if (nbd >= l1) {
    for (j = 2; j <= ipph; ++j) {
      jc = ipp2 - j;
      j2 = j + j;
      for (k = 1; k <= l1; ++k) {
        for (i = 3; i <= ido; i += 2) {
          ic = idp2 - i;
          cc_ref(i - 1, j2 - 1, k) = ch_ref(i - 1, k, j) + ch_ref(
                                                                  i - 1, k, jc);
          cc_ref(ic - 1, j2 - 2, k) = ch_ref(i - 1, k, j) - ch_ref(
                                                                   i - 1, k, jc);
          cc_ref(i, j2 - 1, k) = ch_ref(i, k, j) + ch_ref(i, k,
                                                          jc);
          cc_ref(ic, j2 - 2, k) = ch_ref(i, k, jc) - ch_ref(i, k, j)
            ;
        }
      }
    }
  } else {
    for (j = 2; j <= ipph; ++j) {
      jc = ipp2 - j;
      j2 = j + j;
      for (i = 3; i <= ido; i += 2) {
        ic = idp2 - i;
        for (k = 1; k <= l1; ++k) {
          cc_ref(i - 1, j2 - 1, k) = ch_ref(i - 1, k, j) + ch_ref(
                                                                  i - 1, k, jc);
          cc_ref(ic - 1, j2 - 2, k) = ch_ref(i - 1, k, j) - ch_ref(
                                                                   i - 1, k, jc);
          cc_ref(i, j2 - 1, k) = ch_ref(i, k, j) + ch_ref(i, k,
                                                          jc);
          cc_ref(ic, j2 - 2, k) = ch_ref(i, k, jc) - ch_ref(i, k, j)
            ;
        }
      }
    }
  }
}

#undef ch2_ref
#undef ch_ref
#undef cc_ref
#undef c2_ref
#undef c1_ref

static void cfftb1(integer n, real *c, real *ch, const real *wa, integer *ifac)
{
  integer i, k1, l1, l2, na, nf, ip, iw, ix2, ix3, ix4, nac, ido,
    idl1, idot;

  nf = ifac[1];
  na = 0;
  l1 = 1;
  iw = 0;
  for (k1 = 1; k1 <= nf; ++k1) {
    ip = ifac[k1 + 1];
    l2 = ip * l1;
    ido = n / l2;
    idot = ido + ido;
    idl1 = idot * l1;
    switch (ip) {
      case 4:
        ix2 = iw + idot;
        ix3 = ix2 + idot;
        passb4(idot, l1, na?ch:c, na?c:ch, &wa[iw], &wa[ix2], &wa[ix3]);
        na = 1 - na;
        break;
      case 2:
        passb2(idot, l1, na?ch:c, na?c:ch, &wa[iw]);
        na = 1 - na;
        break;
      case 3:
        ix2 = iw + idot;
        passb3(idot, l1, na?ch:c, na?c:ch, &wa[iw], &wa[ix2]);
        na = 1 - na;
        break;
      case 5:
        ix2 = iw + idot;
        ix3 = ix2 + idot;
        ix4 = ix3 + idot;
        passfb5(idot, l1, na?ch:c, na?c:ch, &wa[iw], &wa[ix2], &wa[ix3], &wa[ix4], +1);
        na = 1 - na;
        break;
      default:
        if (na == 0) {
          passfb(&nac, idot, ip, l1, idl1, c, c, c, ch, ch, &wa[iw], +1);
        } else {
          passfb(&nac, idot, ip, l1, idl1, ch, ch, ch, c, c, &wa[iw], +1);
        }
        if (nac != 0) {
          na = 1 - na;
        }
        break;
    }
    l1 = l2;
    iw += (ip - 1) * idot;
  }
  if (na == 0) {
    return;
  }
  for (i = 0; i < 2*n; ++i) {
    c[i] = ch[i];
  }
}

void cfftb(integer n, real *c, real *wsave)
{
  integer iw1, iw2;

  --wsave;
  --c;

  if (n == 1) {
    return;
  }
  iw1 = 2*n + 1;
  iw2 = iw1 + 2*n;
  cfftb1(n, &c[1], &wsave[1], &wsave[iw1], (int*)&wsave[iw2]);
}

static void cfftf1(integer n, real *c, real *ch, const real *wa, integer *ifac)
{

  integer i, k1, l1, l2, na, nf, ip, iw, ix2, ix3, ix4, nac, ido,
    idl1, idot;

  nf = ifac[1];
  na = 0;
  l1 = 1;
  iw = 0;
  for (k1 = 1; k1 <= nf; ++k1) {
    ip = ifac[k1 + 1];
    l2 = ip * l1;
    ido = n / l2;
    idot = ido + ido;
    idl1 = idot * l1;
    switch (ip) {
      case 4:
        ix2 = iw + idot;
        ix3 = ix2 + idot;
        passf4(idot, l1, na?ch:c, na?c:ch, &wa[iw], &wa[ix2], &wa[ix3]);
        na = 1 - na;
        break;
      case 2:
        passf2(idot, l1, na?ch:c, na?c:ch, &wa[iw]);
        na = 1 - na;
        break;
      case 3:
        ix2 = iw + idot;
        passf3(idot, l1, na?ch:c, na?c:ch, &wa[iw], &wa[ix2]);
        na = 1 - na;
        break;
      case 5:
        ix2 = iw + idot;
        ix3 = ix2 + idot;
        ix4 = ix3 + idot;
        passfb5(idot, l1, na?ch:c, na?c:ch, &wa[iw], &wa[ix2], &wa[ix3], &wa[ix4], -1);
        na = 1 - na;
        break;
      default:
        if (na == 0) {
          passfb(&nac, idot, ip, l1, idl1, c, c, c, ch, ch, &wa[iw], -1);
        } else {
          passfb(&nac, idot, ip, l1, idl1, ch, ch, ch, c, c, &wa[iw], -1);
        }
        if (nac != 0) {
          na = 1 - na;
        }
        break;
    }
    l1 = l2;
    iw += (ip - 1)*idot;
  }
  if (na == 0) {
    return;
  }
  for (i = 0; i < 2*n; ++i) {
    c[i] = ch[i];
  }
}

void cfftf(integer n, real *c, real *wsave)
{
  integer iw1, iw2;

  --wsave;
  --c;

  if (n == 1) {
    return;
  }
  iw1 = 2*n + 1;
  iw2 = iw1 + 2*n;
  cfftf1(n, &c[1], &wsave[1], &wsave[iw1], (int*)&wsave[iw2]);
}

static int decompose(integer n, integer *ifac, integer ntryh[4]) {
  integer ntry=0, nl = n, nf = 0, nq, nr, i, j = 0;
  do {
    if (j < 4) {
      ntry = ntryh[j];
    } else {
      ntry += 2;
    }
    ++j;
  L104:
    nq = nl / ntry;
    nr = nl - ntry * nq;
    if (nr != 0) continue;
    ++nf;
    ifac[nf + 2] = ntry;
    nl = nq;
    if (ntry == 2 && nf != 1) {
      for (i = 2; i <= nf; ++i) {
        integer ib = nf - i + 2;
        ifac[ib + 2] = ifac[ib + 1];
      }
      ifac[3] = 2;
    }
    if (nl != 1) {
      goto L104;
    }
  } while (nl != 1);
  ifac[1] = n;
  ifac[2] = nf;
  return nf;
}

static void cffti1(integer n, real *wa, integer *ifac)
{
  static integer ntryh[4] = { 3,4,2,5 };

  integer i, j, i1, k1, l1, l2;
  real fi;
  integer ld, ii, nf, ip;
  real arg;
  integer ido, ipm;
  real argh;
  integer idot;
  real argld;

  --ifac;
  --wa;

  nf = decompose(n, ifac, ntryh);

  argh = (2*M_PI) / (real) (n);
  i = 2;
  l1 = 1;
  for (k1 = 1; k1 <= nf; ++k1) {
    ip = ifac[k1 + 2];
    ld = 0;
    l2 = l1 * ip;
    ido = n / l2;
    idot = ido + ido + 2;
    ipm = ip - 1;
    for (j = 1; j <= ipm; ++j) {
      i1 = i;
      wa[i - 1] = 1.f;
      wa[i] = 0.f;
      ld += l1;
      fi = 0.f;
      argld = (real) ld * argh;
      for (ii = 4; ii <= idot; ii += 2) {
        i += 2;
        fi += 1.f;
        arg = fi * argld;
        wa[i - 1] = cos(arg);
        wa[i] = sin(arg);
      }
      if (ip > 5) {
        wa[i1 - 1] = wa[i - 1];
        wa[i1] = wa[i];
      };
    }
    l1 = l2;
  }
}

void cffti(integer n, real *wsave)
{
  integer iw1, iw2;

  --wsave;

  if (n == 1) {
    return;
  }
  iw1 = 2*n + 1;
  iw2 = iw1 + 2*n;
  cffti1(n, &wsave[iw1], (int*)&wsave[iw2]);
  return;
}

static void rfftb1(integer n, real *c, real *ch, const real *wa, integer *ifac)
{

  integer i, k1, l1, l2, na, nf, ip, iw, ix2, ix3, ix4, ido, idl1;

  nf = ifac[1];
  na = 0;
  l1 = 1;
  iw = 0;
  for (k1 = 1; k1 <= nf; ++k1) {
    ip = ifac[k1 + 1];
    l2 = ip * l1;
    ido = n / l2;
    idl1 = ido * l1;
    switch (ip) {
      case 4:
        ix2 = iw + ido;
        ix3 = ix2 + ido;
        radb4(ido, l1, na?ch:c, na?c:ch, &wa[iw], &wa[ix2], &wa[ix3]);
        na = 1 - na;
        break;
      case 2:
        radb2(ido, l1, na?ch:c, na?c:ch, &wa[iw]);
        na = 1 - na;
        break;
      case 3:
        ix2 = iw + ido;
        radb3(ido, l1, na?ch:c, na?c:ch, &wa[iw], &wa[ix2]);
        na = 1 - na;
        break;
      case 5:
        ix2 = iw + ido;
        ix3 = ix2 + ido;
        ix4 = ix3 + ido;
        radb5(ido, l1, na?ch:c, na?c:ch, &wa[iw], &wa[ix2], &wa[ix3], &wa[ix4]);
        na = 1 - na;
        break;
      default:
        if (na == 0) {
          radbg(ido, ip, l1, idl1, c, c, c, ch, ch, &wa[iw]);
        } else {
          radbg(ido, ip, l1, idl1, ch, ch, ch, c, c, &wa[iw]);
        }
        if (ido == 1) {
          na = 1 - na;
        }
        break;
    }
    l1 = l2;
    iw += (ip - 1) * ido;
  }
  if (na == 0) {
    return;
  }
  for (i = 0; i < n; ++i) {
    c[i] = ch[i];
  }
}

static void rfftf1(integer n, real *c, real *ch, const real *wa, integer *ifac)
{

  integer i, k1, l1, l2, na, kh, nf, ip, iw, ix2, ix3, ix4, ido, idl1;

  nf = ifac[1];
  na = 1;
  l2 = n;
  iw = n-1;
  for (k1 = 1; k1 <= nf; ++k1) {
    kh = nf - k1;
    ip = ifac[kh + 2];
    l1 = l2 / ip;
    ido = n / l2;
    idl1 = ido * l1;
    iw -= (ip - 1) * ido;
    na = 1 - na;
    switch (ip) {
      case 4:
        ix2 = iw + ido;
        ix3 = ix2 + ido;
        radf4(ido, l1, na ? ch : c, na ? c : ch, &wa[iw], &wa[ix2], &wa[ix3]);
        break;
      case 2:
        radf2(ido, l1, na ? ch : c, na ? c : ch, &wa[iw]);
        break;
      case 3:
        ix2 = iw + ido;
        radf3(ido, l1, na ? ch : c, na ? c : ch, &wa[iw], &wa[ix2]);
        break;
      case 5:
        ix2 = iw + ido;
        ix3 = ix2 + ido;
        ix4 = ix3 + ido;
        radf5(ido, l1, na ? ch : c, na ? c : ch, &wa[iw], &wa[ix2], &wa[ix3], &wa[ix4]);
        break;
      default:
        if (ido == 1) {
          na = 1 - na;
        }
        if (na == 0) {
          radfg(ido, ip, l1, idl1, c, c, c, ch, ch, &wa[iw]);
          na = 1;
        } else {
          radfg(ido, ip, l1, idl1, ch, ch, ch, c, c, &wa[iw]);
          na = 0;
        }
        break;
    }
    l2 = l1;
  }
  if (na == 1) {
    return;
  }
  for (i = 0; i < n; ++i) {
    c[i] = ch[i];
  }
}

void rfftb(integer n, real *r, real *wsave)
{

  --wsave;
  --r;

  if (n == 1) {
    return;
  }
  rfftb1(n, &r[1], &wsave[1], &wsave[n + 1], (int*)&wsave[(n << 1) + 1]);
}

static void rffti1(integer n, real *wa, integer *ifac)
{
  static integer ntryh[4] = { 4,2,3,5 };

  integer i, j, k1, l1, l2;
  real fi;
  integer ld, ii, nf, ip, is;
  real arg;
  integer ido, ipm;
  integer nfm1;
  real argh;
  real argld;

  --ifac;
  --wa;

  nf = decompose(n, ifac, ntryh);

  argh = (2*M_PI) / (real) (n);
  is = 0;
  nfm1 = nf - 1;
  l1 = 1;
  if (nfm1 == 0) {
    return;
  }
  for (k1 = 1; k1 <= nfm1; ++k1) {
    ip = ifac[k1 + 2];
    ld = 0;
    l2 = l1 * ip;
    ido = n / l2;
    ipm = ip - 1;
    for (j = 1; j <= ipm; ++j) {
      ld += l1;
      i = is;
      argld = (real) ld * argh;
      fi = 0.f;
      for (ii = 3; ii <= ido; ii += 2) {
        i += 2;
        fi += 1.f;
        arg = fi * argld;
        wa[i - 1] = cos(arg);
        wa[i] = sin(arg);
      }
      is += ido;
    }
    l1 = l2;
  }
}

void rfftf(integer n, real *r, real *wsave)
{

  --wsave;
  --r;

  if (n == 1) {
    return;
  }
  rfftf1(n, &r[1], &wsave[1], &wsave[n + 1], (int*)&wsave[(n << 1) + 1]);
}

void rffti(integer n, real *wsave)
{

  --wsave;

  if (n == 1) {
    return;
  }
  rffti1(n, &wsave[n + 1], (int*)&wsave[(n << 1) + 1]);
  return;
}

static void cosqb1(integer n, real *x, real *w, real *xh)
{

  integer i, k, kc, np2, ns2;
  real xim1;
  integer modn;

  --xh;
  --w;
  --x;

  ns2 = (n + 1) / 2;
  np2 = n + 2;
  for (i = 3; i <= n; i += 2) {
    xim1 = x[i - 1] + x[i];
    x[i] -= x[i - 1];
    x[i - 1] = xim1;
  }
  x[1] += x[1];
  modn = n % 2;
  if (modn == 0) {
    x[n] += x[n];
  }
  rfftb(n, &x[1], &xh[1]);
  for (k = 2; k <= ns2; ++k) {
    kc = np2 - k;
    xh[k] = w[k - 1] * x[kc] + w[kc - 1] * x[k];
    xh[kc] = w[k - 1] * x[k] - w[kc - 1] * x[kc];
  }
  if (modn == 0) {
    x[ns2 + 1] = w[ns2] * (x[ns2 + 1] + x[ns2 + 1]);
  }
  for (k = 2; k <= ns2; ++k) {
    kc = np2 - k;
    x[k] = xh[k] + xh[kc];
    x[kc] = xh[k] - xh[kc];
  }
  x[1] += x[1];
}

void cosqb(integer n, real *x, real *wsave)
{
  static const real tsqrt2 = 2.82842712474619f;

  real x1;

  --wsave;
  --x;

  if (n < 2) {
    x[1] *= 4.f;
  } else if (n == 2) {
    x1 = (x[1] + x[2]) * 4.f;
    x[2] = tsqrt2 * (x[1] - x[2]);
    x[1] = x1;
  } else {
    cosqb1(n, &x[1], &wsave[1], &wsave[n + 1]);
  }
}

static void cosqf1(integer n, real *x, real *w, real *xh)
{

  integer i, k, kc, np2, ns2;
  real xim1;
  integer modn;

  --xh;
  --w;
  --x;

  ns2 = (n + 1) / 2;
  np2 = n + 2;
  for (k = 2; k <= ns2; ++k) {
    kc = np2 - k;
    xh[k] = x[k] + x[kc];
    xh[kc] = x[k] - x[kc];
  }
  modn = n % 2;
  if (modn == 0) {
    xh[ns2 + 1] = x[ns2 + 1] + x[ns2 + 1];
  }
  for (k = 2; k <= ns2; ++k) {
    kc = np2 - k;
    x[k] = w[k - 1] * xh[kc] + w[kc - 1] * xh[k];
    x[kc] = w[k - 1] * xh[k] - w[kc - 1] * xh[kc];
  }
  if (modn == 0) {
    x[ns2 + 1] = w[ns2] * xh[ns2 + 1];
  }
  rfftf(n, &x[1], &xh[1]);
  for (i = 3; i <= n; i += 2) {
    xim1 = x[i - 1] - x[i];
    x[i] = x[i - 1] + x[i];
    x[i - 1] = xim1;
  }
}

void cosqf(integer n, real *x, real *wsave)
{
  static const real sqrt2 = 1.4142135623731f;

  real tsqx;

  --wsave;
  --x;

  if (n == 2) {
    tsqx = sqrt2 * x[2];
    x[2] = x[1] - tsqx;
    x[1] += tsqx;
  } else if (n > 2) {
    cosqf1(n, &x[1], &wsave[1], &wsave[n + 1]);
  }
}

void cosqi(integer n, real *wsave)
{

  integer k;
  real fk, dt;

  --wsave;

  dt = M_PI/2 / (real) (n);
  fk = 0.f;
  for (k = 1; k <= n; ++k) {
    fk += 1.f;
    wsave[k] = cos(fk * dt);
  }
  rffti(n, &wsave[n + 1]);
}

void cost(integer n, real *x, real *wsave)
{

  integer i, k;
  real c1, t1, t2;
  integer kc;
  real xi;
  integer nm1, np1;
  real x1h;
  integer ns2;
  real tx2, x1p3, xim2;
  integer modn;

  --wsave;
  --x;

  nm1 = n - 1;
  np1 = n + 1;
  ns2 = n / 2;
  if (n < 2) {
  } else if (n == 2) {
    x1h = x[1] + x[2];
    x[2] = x[1] - x[2];
    x[1] = x1h;
  } else if (n == 3) {
    x1p3 = x[1] + x[3];
    tx2 = x[2] + x[2];
    x[2] = x[1] - x[3];
    x[1] = x1p3 + tx2;
    x[3] = x1p3 - tx2;
  } else {
    c1 = x[1] - x[n];
    x[1] += x[n];
    for (k = 2; k <= ns2; ++k) {
      kc = np1 - k;
      t1 = x[k] + x[kc];
      t2 = x[k] - x[kc];
      c1 += wsave[kc] * t2;
      t2 = wsave[k] * t2;
      x[k] = t1 - t2;
      x[kc] = t1 + t2;
    }
    modn = n % 2;
    if (modn != 0) {
      x[ns2 + 1] += x[ns2 + 1];
    }
    rfftf(nm1, &x[1], &wsave[n + 1]);
    xim2 = x[2];
    x[2] = c1;
    for (i = 4; i <= n; i += 2) {
      xi = x[i];
      x[i] = x[i - 2] - x[i - 1];
      x[i - 1] = xim2;
      xim2 = xi;
    }
    if (modn != 0) {
      x[n] = xim2;
    }
  }
}

void costi(integer n, real *wsave)
{

  integer k, kc;
  real fk, dt;
  integer nm1, np1, ns2;

  --wsave;

  if (n <= 3) {
    return;
  }
  nm1 = n - 1;
  np1 = n + 1;
  ns2 = n / 2;
  dt = M_PI / (real) nm1;
  fk = 0.f;
  for (k = 2; k <= ns2; ++k) {
    kc = np1 - k;
    fk += 1.f;
    wsave[k] = sin(fk * dt) * 2.f;
    wsave[kc] = cos(fk * dt) * 2.f;
  }
  rffti(nm1, &wsave[n + 1]);
}

void sinqb(integer n, real *x, real *wsave)
{

  integer k, kc, ns2;
  real xhold;

  --wsave;
  --x;

  if (n <= 1) {
    x[1] *= 4.f;
    return;
  }
  ns2 = n / 2;
  for (k = 2; k <= n; k += 2) {
    x[k] = -x[k];
  }
  cosqb(n, &x[1], &wsave[1]);
  for (k = 1; k <= ns2; ++k) {
    kc = n - k;
    xhold = x[k];
    x[k] = x[kc + 1];
    x[kc + 1] = xhold;
  }
}

void sinqf(integer n, real *x, real *wsave)
{

  integer k, kc, ns2;
  real xhold;

  --wsave;
  --x;

  if (n == 1) {
    return;
  }
  ns2 = n / 2;
  for (k = 1; k <= ns2; ++k) {
    kc = n - k;
    xhold = x[k];
    x[k] = x[kc + 1];
    x[kc + 1] = xhold;
  }
  cosqf(n, &x[1], &wsave[1]);
  for (k = 2; k <= n; k += 2) {
    x[k] = -x[k];
  }
}

void sinqi(integer n, real *wsave)
{

  --wsave;

  cosqi(n, &wsave[1]);
}

static void sint1(integer n, real *war, real *was, real *xh, real *
                  x, integer *ifac)
{

  static const real sqrt3 = 1.73205080756888f;

  integer i, k;
  real t1, t2;
  integer kc, np1, ns2, modn;
  real xhold;

  --ifac;
  --x;
  --xh;
  --was;
  --war;

  for (i = 1; i <= n; ++i) {
    xh[i] = war[i];
    war[i] = x[i];
  }

  if (n < 2) {
    xh[1] += xh[1];
  } else if (n == 2) {
    xhold = sqrt3 * (xh[1] + xh[2]);
    xh[2] = sqrt3 * (xh[1] - xh[2]);
    xh[1] = xhold;
  } else {
    np1 = n + 1;
    ns2 = n / 2;
    x[1] = 0.f;
    for (k = 1; k <= ns2; ++k) {
      kc = np1 - k;
      t1 = xh[k] - xh[kc];
      t2 = was[k] * (xh[k] + xh[kc]);
      x[k + 1] = t1 + t2;
      x[kc + 1] = t2 - t1;
    }
    modn = n % 2;
    if (modn != 0) {
      x[ns2 + 2] = xh[ns2 + 1] * 4.f;
    }
    rfftf1(np1, &x[1], &xh[1], &war[1], &ifac[1]);
    xh[1] = x[1] * .5f;
    for (i = 3; i <= n; i += 2) {
      xh[i - 1] = -x[i];
      xh[i] = xh[i - 2] + x[i - 1];
    }
    if (modn == 0) {
      xh[n] = -x[n + 1];
    }
  }
  for (i = 1; i <= n; ++i) {
    x[i] = war[i];
    war[i] = xh[i];
  }
}

void sinti(integer n, real *wsave)
{

  integer k;
  real dt;
  integer np1, ns2;

  --wsave;

  if (n <= 1) {
    return;
  }
  ns2 = n / 2;
  np1 = n + 1;
  dt = M_PI / (real) np1;
  for (k = 1; k <= ns2; ++k) {
    wsave[k] = sin(k * dt) * 2.f;
  }
  rffti(np1, &wsave[ns2 + 1]);
}

void sint(integer n, real *x, real *wsave)
{
  integer np1, iw1, iw2, iw3;

  --wsave;
  --x;

  np1 = n + 1;
  iw1 = n / 2 + 1;
  iw2 = iw1 + np1;
  iw3 = iw2 + np1;
  sint1(n, &x[1], &wsave[1], &wsave[iw1], &wsave[iw2], (int*)&wsave[iw3]);
}

#ifdef TESTING_FFTPACK
#include <stdio.h>

int main(void)
{
  static integer nd[] = { 120,91,54,49,32,28,24,8,4,3,2 };

  real r1, r2, r3;
  f77complex q1, q2, q3;

  integer i, j, k, n;
  real w[2000], x[200], y[200], cf, fn, dt;
  f77complex cx[200], cy[200];
  real xh[200];
  integer nz, nm1, np1, ns2;
  real arg, tfn;
  real sum, arg1, arg2;
  real sum1, sum2, dcfb;
  integer modn;
  real rftb, rftf;
  real sqrt2;
  real rftfb;
  real costt, sintt, dcfftb, dcfftf, cosqfb, costfb;
  real sinqfb;
  real sintfb;
  real cosqbt, cosqft, sinqbt, sinqft;

  sqrt2 = sqrt(2.f);
  int all_ok = 1;
  for (nz = 1; nz <= (int)(sizeof nd/sizeof nd[0]); ++nz) {
    n = nd[nz - 1];
    modn = n % 2;
    fn = (real) n;
    tfn = fn + fn;
    np1 = n + 1;
    nm1 = n - 1;
    for (j = 1; j <= np1; ++j) {
      x[j - 1] = sin((real) j * sqrt2);
      y[j - 1] = x[j - 1];
      xh[j - 1] = x[j - 1];
    }

    rffti(n, w);
    dt = (2*M_PI) / fn;
    ns2 = (n + 1) / 2;
    if (ns2 < 2) {
      goto L104;
    }
    for (k = 2; k <= ns2; ++k) {
      sum1 = 0.f;
      sum2 = 0.f;
      arg = (real) (k - 1) * dt;
      for (i = 1; i <= n; ++i) {
        arg1 = (real) (i - 1) * arg;
        sum1 += x[i - 1] * cos(arg1);
        sum2 += x[i - 1] * sin(arg1);
      }
      y[(k << 1) - 3] = sum1;
      y[(k << 1) - 2] = -sum2;
    }
  L104:
    sum1 = 0.f;
    sum2 = 0.f;
    for (i = 1; i <= nm1; i += 2) {
      sum1 += x[i - 1];
      sum2 += x[i];
    }
    if (modn == 1) {
      sum1 += x[n - 1];
    }
    y[0] = sum1 + sum2;
    if (modn == 0) {
      y[n - 1] = sum1 - sum2;
    }
    rfftf(n, x, w);
    rftf = 0.f;
    for (i = 1; i <= n; ++i) {

      r2 = rftf, r3 = (r1 = x[i - 1] - y[i - 1], fabs(r1));
      rftf = dmax(r2,r3);
      x[i - 1] = xh[i - 1];
    }
    rftf /= fn;
    for (i = 1; i <= n; ++i) {
      sum = x[0] * .5f;
      arg = (real) (i - 1) * dt;
      if (ns2 < 2) {
        goto L108;
      }
      for (k = 2; k <= ns2; ++k) {
        arg1 = (real) (k - 1) * arg;
        sum = sum + x[(k << 1) - 3] * cos(arg1) - x[(k << 1) - 2] *
          sin(arg1);
      }
    L108:
      if (modn == 0) {
        sum += (real)pow(-1, i-1) * .5f * x[n - 1];
      }
      y[i - 1] = sum + sum;
    }
    rfftb(n, x, w);
    rftb = 0.f;
    for (i = 1; i <= n; ++i) {

      r2 = rftb, r3 = (r1 = x[i - 1] - y[i - 1], fabs(r1));
      rftb = dmax(r2,r3);
      x[i - 1] = xh[i - 1];
      y[i - 1] = xh[i - 1];
    }
    rfftb(n, y, w);
    rfftf(n, y, w);
    cf = 1.f / fn;
    rftfb = 0.f;
    for (i = 1; i <= n; ++i) {

      r2 = rftfb, r3 = (r1 = cf * y[i - 1] - x[i - 1], fabs(
                                                            r1));
      rftfb = dmax(r2,r3);
    }

    dt = M_PI / fn;
    for (i = 1; i <= nm1; ++i) {
      x[i - 1] = xh[i - 1];
    }
    for (i = 1; i <= nm1; ++i) {
      y[i - 1] = 0.f;
      arg1 = (real) i * dt;
      for (k = 1; k <= nm1; ++k) {
        y[i - 1] += x[k - 1] * sin((real) k * arg1);
      }
      y[i - 1] += y[i - 1];
    }
    sinti(nm1, w);
    sint(nm1, x, w);
    cf = .5f / fn;
    sintt = 0.f;
    for (i = 1; i <= nm1; ++i) {

      r2 = sintt, r3 = (r1 = x[i - 1] - y[i - 1], fabs(r1));
      sintt = dmax(r2,r3);
      x[i - 1] = xh[i - 1];
      y[i - 1] = x[i - 1];
    }
    sintt = cf * sintt;
    sint(nm1, x, w);
    sint(nm1, x, w);
    sintfb = 0.f;
    for (i = 1; i <= nm1; ++i) {

      r2 = sintfb, r3 = (r1 = cf * x[i - 1] - y[i - 1], fabs(
                                                             r1));
      sintfb = dmax(r2,r3);
    }

    for (i = 1; i <= np1; ++i) {
      x[i - 1] = xh[i - 1];
    }
    for (i = 1; i <= np1; ++i) {
      y[i - 1] = (x[0] + (real) pow(-1, i+1) * x[n]) * .5f;
      arg = (real) (i - 1) * dt;
      for (k = 2; k <= n; ++k) {
        y[i - 1] += x[k - 1] * cos((real) (k - 1) * arg);
      }
      y[i - 1] += y[i - 1];
    }
    costi(np1, w);
    cost(np1, x, w);
    costt = 0.f;
    for (i = 1; i <= np1; ++i) {

      r2 = costt, r3 = (r1 = x[i - 1] - y[i - 1], fabs(r1));
      costt = dmax(r2,r3);
      x[i - 1] = xh[i - 1];
      y[i - 1] = xh[i - 1];
    }
    costt = cf * costt;
    cost(np1, x, w);
    cost(np1, x, w);
    costfb = 0.f;
    for (i = 1; i <= np1; ++i) {

      r2 = costfb, r3 = (r1 = cf * x[i - 1] - y[i - 1], fabs(
                                                             r1));
      costfb = dmax(r2,r3);
    }

    cf = .25f / fn;
    for (i = 1; i <= n; ++i) {
      y[i - 1] = xh[i - 1];
    }
    dt = M_PI / (fn + fn);
    for (i = 1; i <= n; ++i) {
      x[i - 1] = 0.f;
      arg = dt * (real) i;
      for (k = 1; k <= n; ++k) {
        x[i - 1] += y[k - 1] * sin((real) (k + k - 1) * arg);
      }
      x[i - 1] *= 4.f;
    }
    sinqi(n, w);
    sinqb(n, y, w);
    sinqbt = 0.f;
    for (i = 1; i <= n; ++i) {

      r2 = sinqbt, r3 = (r1 = y[i - 1] - x[i - 1], fabs(r1))
        ;
      sinqbt = dmax(r2,r3);
      x[i - 1] = xh[i - 1];
    }
    sinqbt = cf * sinqbt;
    for (i = 1; i <= n; ++i) {
      arg = (real) (i + i - 1) * dt;
      y[i - 1] = (real) pow(-1, i+1) * .5f * x[n - 1];
      for (k = 1; k <= nm1; ++k) {
        y[i - 1] += x[k - 1] * sin((real) k * arg);
      }
      y[i - 1] += y[i - 1];
    }
    sinqf(n, x, w);
    sinqft = 0.f;
    for (i = 1; i <= n; ++i) {

      r2 = sinqft, r3 = (r1 = x[i - 1] - y[i - 1], fabs(r1))
        ;
      sinqft = dmax(r2,r3);
      y[i - 1] = xh[i - 1];
      x[i - 1] = xh[i - 1];
    }
    sinqf(n, y, w);
    sinqb(n, y, w);
    sinqfb = 0.f;
    for (i = 1; i <= n; ++i) {

      r2 = sinqfb, r3 = (r1 = cf * y[i - 1] - x[i - 1], fabs(
                                                             r1));
      sinqfb = dmax(r2,r3);
    }

    for (i = 1; i <= n; ++i) {
      y[i - 1] = xh[i - 1];
    }
    for (i = 1; i <= n; ++i) {
      x[i - 1] = 0.f;
      arg = (real) (i - 1) * dt;
      for (k = 1; k <= n; ++k) {
        x[i - 1] += y[k - 1] * cos((real) (k + k - 1) * arg);
      }
      x[i - 1] *= 4.f;
    }
    cosqi(n, w);
    cosqb(n, y, w);
    cosqbt = 0.f;
    for (i = 1; i <= n; ++i) {

      r2 = cosqbt, r3 = (r1 = x[i - 1] - y[i - 1], fabs(r1))
        ;
      cosqbt = dmax(r2,r3);
      x[i - 1] = xh[i - 1];
    }
    cosqbt = cf * cosqbt;
    for (i = 1; i <= n; ++i) {
      y[i - 1] = x[0] * .5f;
      arg = (real) (i + i - 1) * dt;
      for (k = 2; k <= n; ++k) {
        y[i - 1] += x[k - 1] * cos((real) (k - 1) * arg);
      }
      y[i - 1] += y[i - 1];
    }
    cosqf(n, x, w);
    cosqft = 0.f;
    for (i = 1; i <= n; ++i) {

      r2 = cosqft, r3 = (r1 = y[i - 1] - x[i - 1], fabs(r1))
        ;
      cosqft = dmax(r2,r3);
      x[i - 1] = xh[i - 1];
      y[i - 1] = xh[i - 1];
    }
    cosqft = cf * cosqft;
    cosqb(n, x, w);
    cosqf(n, x, w);
    cosqfb = 0.f;
    for (i = 1; i <= n; ++i) {

      r2 = cosqfb, r3 = (r1 = cf * x[i - 1] - y[i - 1], fabs(r1));
      cosqfb = dmax(r2,r3);
    }

    for (i = 1; i <= n; ++i) {
      r1 = cos(sqrt2 * (real) i);
      r2 = sin(sqrt2 * (real) (i * i));
      q1.r = r1, q1.i = r2;
      cx[i-1].r = q1.r, cx[i-1].i = q1.i;
    }
    dt = (2*M_PI) / fn;
    for (i = 1; i <= n; ++i) {
      arg1 = -((real) (i - 1)) * dt;
      cy[i-1].r = 0.f, cy[i-1].i = 0.f;
      for (k = 1; k <= n; ++k) {
        arg2 = (real) (k - 1) * arg1;
        r1 = cos(arg2);
        r2 = sin(arg2);
        q3.r = r1, q3.i = r2;
        q2.r = q3.r * cx[k-1].r - q3.i * cx[k-1].i, q2.i =
          q3.r * cx[k-1].i + q3.i * cx[k-1].r;
        q1.r = cy[i-1].r + q2.r, q1.i = cy[i-1].i + q2.i;
        cy[i-1].r = q1.r, cy[i-1].i = q1.i;
      }
    }
    cffti(n, w);
    cfftf(n, (real*)cx, w);
    dcfftf = 0.f;
    for (i = 1; i <= n; ++i) {

      q1.r = cx[i-1].r - cy[i-1].r, q1.i = cx[i-1].i - cy[i-1]
        .i;
      r1 = dcfftf, r2 = c_abs(&q1);
      dcfftf = dmax(r1,r2);
      q1.r = cx[i-1].r / fn, q1.i = cx[i-1].i / fn;
      cx[i-1].r = q1.r, cx[i-1].i = q1.i;
    }
    dcfftf /= fn;
    for (i = 1; i <= n; ++i) {
      arg1 = (real) (i - 1) * dt;
      cy[i-1].r = 0.f, cy[i-1].i = 0.f;
      for (k = 1; k <= n; ++k) {
        arg2 = (real) (k - 1) * arg1;
        r1 = cos(arg2);
        r2 = sin(arg2);
        q3.r = r1, q3.i = r2;
        q2.r = q3.r * cx[k-1].r - q3.i * cx[k-1].i, q2.i =
          q3.r * cx[k-1].i + q3.i * cx[k-1].r;
        q1.r = cy[i-1].r + q2.r, q1.i = cy[i-1].i + q2.i;
        cy[i-1].r = q1.r, cy[i-1].i = q1.i;
      }
    }
    cfftb(n, (real*)cx, w);
    dcfftb = 0.f;
    for (i = 1; i <= n; ++i) {

      q1.r = cx[i-1].r - cy[i-1].r, q1.i = cx[i-1].i - cy[i-1].i;
      r1 = dcfftb, r2 = c_abs(&q1);
      dcfftb = dmax(r1,r2);
      cx[i-1].r = cy[i-1].r, cx[i-1].i = cy[i-1].i;
    }
    cf = 1.f / fn;
    cfftf(n, (real*)cx, w);
    cfftb(n, (real*)cx, w);
    dcfb = 0.f;
    for (i = 1; i <= n; ++i) {

      q2.r = cf * cx[i-1].r, q2.i = cf * cx[i-1].i;
      q1.r = q2.r - cy[i-1].r, q1.i = q2.i - cy[i-1].i;
      r1 = dcfb, r2 = c_abs(&q1);
      dcfb = dmax(r1,r2);
    }
    printf("%d\tRFFTF  %10.3g\tRFFTB  %10.ge\tRFFTFB %10.3g", n, rftf, rftb, rftfb);
    printf(  "\tSINT   %10.3g\tSINTFB %10.ge\tCOST   %10.3g\n", sintt, sintfb, costt);
    printf(  "\tCOSTFB %10.3g\tSINQF  %10.ge\tSINQB  %10.3g", costfb, sinqft, sinqbt);
    printf(  "\tSINQFB %10.3g\tCOSQF  %10.ge\tCOSQB  %10.3g\n", sinqfb, cosqft, cosqbt);
    printf(  "\tCOSQFB %10.3g\t", cosqfb);
    printf(  "\tCFFTF  %10.ge\tCFFTB  %10.3g\n", dcfftf, dcfftb);
    printf(  "\tCFFTFB %10.3g\n", dcfb);

#define CHECK(x) if (x > 1e-3) { printf(#x " failed: %g\n", x); all_ok = 0; }
    CHECK(rftf); CHECK(rftb); CHECK(rftfb); CHECK(sintt); CHECK(sintfb); CHECK(costt);
    CHECK(costfb); CHECK(sinqft); CHECK(sinqbt); CHECK(sinqfb); CHECK(cosqft); CHECK(cosqbt);
    CHECK(cosqfb); CHECK(dcfftf); CHECK(dcfftb);
  }

  if (all_ok) printf("Everything looks fine.\n");
  else printf("ERRORS WERE DETECTED.\n");

  return all_ok ? 0 : 1;
}
#endif
