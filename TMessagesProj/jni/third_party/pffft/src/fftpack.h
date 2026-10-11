/*
  Interface for the f2c translation of fftpack as found on http://www.netlib.org/fftpack/
  
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

#ifndef FFTPACK_H
#define FFTPACK_H

#ifdef __cplusplus
extern "C" {
#endif

#ifndef FFTPACK_DOUBLE_PRECISION
  typedef float fftpack_real;
  typedef int   fftpack_int;
#else
  typedef double fftpack_real;
  typedef int    fftpack_int;
#endif

  void cffti(fftpack_int n, fftpack_real *wsave);

  void cfftf(fftpack_int n, fftpack_real *c, fftpack_real *wsave);

  void cfftb(fftpack_int n, fftpack_real *c, fftpack_real *wsave);

  void rffti(fftpack_int n, fftpack_real *wsave);
  void rfftf(fftpack_int n, fftpack_real *r, fftpack_real *wsave);
  void rfftb(fftpack_int n, fftpack_real *r, fftpack_real *wsave);

  void cosqi(fftpack_int n, fftpack_real *wsave);
  void cosqf(fftpack_int n, fftpack_real *x, fftpack_real *wsave);
  void cosqb(fftpack_int n, fftpack_real *x, fftpack_real *wsave);

  void costi(fftpack_int n, fftpack_real *wsave);
  void cost(fftpack_int n, fftpack_real *x, fftpack_real *wsave);

  void sinqi(fftpack_int n, fftpack_real *wsave);
  void sinqb(fftpack_int n, fftpack_real *x, fftpack_real *wsave);
  void sinqf(fftpack_int n, fftpack_real *x, fftpack_real *wsave);

  void sinti(fftpack_int n, fftpack_real *wsave);
  void sint(fftpack_int n, fftpack_real *x, fftpack_real *wsave);

#ifdef __cplusplus
}
#endif

#endif
