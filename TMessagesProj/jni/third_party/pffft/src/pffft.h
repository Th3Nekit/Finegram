/* Copyright (c) 2013  Julien Pommier ( pommier@modartt.com ) 

   Based on original fortran 77 code from FFTPACKv4 from NETLIB,
   authored by Dr Paul Swarztrauber of NCAR, in 1985.

   As confirmed by the NCAR fftpack software curators, the following
   FFTPACKv5 license applies to FFTPACKv4 sources. My changes are
   released under the same terms.

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
*/

/*
   PFFFT : a Pretty Fast FFT.

   This is basically an adaptation of the single precision fftpack
   (v4) as found on netlib taking advantage of SIMD instruction found
   on cpus such as intel x86 (SSE1), powerpc (Altivec), and arm (NEON).
   
   For architectures where no SIMD instruction is available, the code
   falls back to a scalar version.  

   Restrictions: 

   - 1D transforms only, with 32-bit single precision.

   - supports only transforms for inputs of length N of the form
   N=(2^a)*(3^b)*(5^c), a >= 5, b >=0, c >= 0 (32, 48, 64, 96, 128,
   144, 160, etc are all acceptable lengths). Performance is best for
   128<=N<=8192.

   - all (float*) pointers in the functions below are expected to
   have an "simd-compatible" alignment, that is 16 bytes on x86 and
   powerpc CPUs.
  
   You can allocate such buffers with the functions
   pffft_aligned_malloc / pffft_aligned_free (or with stuff like
   posix_memalign..)

*/

#ifndef PFFFT_H
#define PFFFT_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef PFFFT_SIMD_DISABLE

void validate_pffft_simd();
#endif

typedef struct PFFFT_Setup PFFFT_Setup;

typedef enum { PFFFT_FORWARD, PFFFT_BACKWARD } pffft_direction_t;

typedef enum { PFFFT_REAL, PFFFT_COMPLEX } pffft_transform_t;

PFFFT_Setup* pffft_new_setup(int N, pffft_transform_t transform);
void pffft_destroy_setup(PFFFT_Setup*);

void pffft_transform(PFFFT_Setup* setup,
                     const float* input,
                     float* output,
                     float* work,
                     pffft_direction_t direction);

void pffft_transform_ordered(PFFFT_Setup* setup,
                             const float* input,
                             float* output,
                             float* work,
                             pffft_direction_t direction);

void pffft_zreorder(PFFFT_Setup* setup,
                    const float* input,
                    float* output,
                    pffft_direction_t direction);

void pffft_zconvolve_accumulate(PFFFT_Setup* setup,
                                const float* dft_a,
                                const float* dft_b,
                                float* dft_ab,
                                float scaling);

void* pffft_aligned_malloc(size_t nb_bytes);
void pffft_aligned_free(void*);

int pffft_simd_size();

#ifdef __cplusplus
}
#endif

#endif
