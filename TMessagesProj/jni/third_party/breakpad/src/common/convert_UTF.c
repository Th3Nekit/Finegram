/*
 * Copyright © 1991-2015 Unicode, Inc. All rights reserved.
 * Distributed under the Terms of Use in 
 * http://www.unicode.org/copyright.html.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of the Unicode data files and any associated documentation
 * (the "Data Files") or Unicode software and any associated documentation
 * (the "Software") to deal in the Data Files or Software
 * without restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, and/or sell copies of
 * the Data Files or Software, and to permit persons to whom the Data Files
 * or Software are furnished to do so, provided that
 * (a) this copyright and permission notice appear with all copies 
 * of the Data Files or Software,
 * (b) this copyright and permission notice appear in associated 
 * documentation, and
 * (c) there is clear notice in each modified Data File or in the Software
 * as well as in the documentation associated with the Data File(s) or
 * Software that the data or software has been modified.
 *
 * THE DATA FILES AND SOFTWARE ARE PROVIDED "AS IS", WITHOUT WARRANTY OF
 * ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
 * WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT OF THIRD PARTY RIGHTS.
 * IN NO EVENT SHALL THE COPYRIGHT HOLDER OR HOLDERS INCLUDED IN THIS
 * NOTICE BE LIABLE FOR ANY CLAIM, OR ANY SPECIAL INDIRECT OR CONSEQUENTIAL
 * DAMAGES, OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THE DATA FILES OR SOFTWARE.
 *
 * Except as contained in this notice, the name of a copyright holder
 * shall not be used in advertising or otherwise to promote the sale,
 * use or other dealings in these Data Files or Software without prior
 * written authorization of the copyright holder.
 */

/* ---------------------------------------------------------------------

Conversions between UTF32, UTF-16, and UTF-8. Source code file.
Author: Mark E. Davis, 1994.
Rev History: Rick McGowan, fixes & updates May 2001.
Sept 2001: fixed const & error conditions per
mods suggested by S. Parent & A. Lillich.
June 2002: Tim Dodd added detection and handling of incomplete
source sequences, enhanced error detection, added casts
to eliminate compiler warnings.
July 2003: slight mods to back out aggressive FFFE detection.
Jan 2004: updated switches in from-UTF8 conversions.
Oct 2004: updated to use UNI_MAX_LEGAL_UTF32 in UTF-32 conversions.

See the header file "ConvertUTF.h" for complete documentation.

------------------------------------------------------------------------ */

#include "convert_UTF.h"
#ifdef CVTUTF_DEBUG
#include <stdio.h>
#endif

static const int halfShift  = 10;

static const UTF32 halfBase = 0x0010000UL;
static const UTF32 halfMask = 0x3FFUL;

#define UNI_SUR_HIGH_START  (UTF32)0xD800
#define UNI_SUR_HIGH_END    (UTF32)0xDBFF
#define UNI_SUR_LOW_START   (UTF32)0xDC00
#define UNI_SUR_LOW_END     (UTF32)0xDFFF

#ifndef false
#define false	   0
#endif
#ifndef true
#define true	    1
#endif

ConversionResult ConvertUTF32toUTF16 (const UTF32** sourceStart, const UTF32* sourceEnd,
                                      UTF16** targetStart, UTF16* targetEnd, ConversionFlags flags) {
  ConversionResult result = conversionOK;
  const UTF32* source = *sourceStart;
  UTF16* target = *targetStart;
  while (source < sourceEnd) {
    UTF32 ch;
    if (target >= targetEnd) {
	    result = targetExhausted; break;
    }
    ch = *source++;
    if (ch <= UNI_MAX_BMP) {

	    if (ch >= UNI_SUR_HIGH_START && ch <= UNI_SUR_LOW_END) {
        if (flags == strictConversion) {
          --source;
          result = sourceIllegal;
          break;
        } else {
          *target++ = UNI_REPLACEMENT_CHAR;
        }
	    } else {
        *target++ = (UTF16)ch;
	    }
    } else if (ch > UNI_MAX_LEGAL_UTF32) {
	    if (flags == strictConversion) {
        result = sourceIllegal;
	    } else {
        *target++ = UNI_REPLACEMENT_CHAR;
	    }
    } else {

	    if (target + 1 >= targetEnd) {
        --source;
        result = targetExhausted; break;
	    }
	    ch -= halfBase;
	    *target++ = (UTF16)((ch >> halfShift) + UNI_SUR_HIGH_START);
	    *target++ = (UTF16)((ch & halfMask) + UNI_SUR_LOW_START);
    }
  }
*sourceStart = source;
*targetStart = target;
return result;
}

ConversionResult ConvertUTF16toUTF32 (const UTF16** sourceStart, const UTF16* sourceEnd,
                                      UTF32** targetStart, UTF32* targetEnd, ConversionFlags flags) {
  ConversionResult result = conversionOK;
  const UTF16* source = *sourceStart;
  UTF32* target = *targetStart;
  UTF32 ch, ch2;
  while (source < sourceEnd) {
    const UTF16* oldSource = source;
    ch = *source++;

    if (ch >= UNI_SUR_HIGH_START && ch <= UNI_SUR_HIGH_END) {

	    if (source < sourceEnd) {
        ch2 = *source;

        if (ch2 >= UNI_SUR_LOW_START && ch2 <= UNI_SUR_LOW_END) {
          ch = ((ch - UNI_SUR_HIGH_START) << halfShift)
          + (ch2 - UNI_SUR_LOW_START) + halfBase;
          ++source;
        } else if (flags == strictConversion) {
          --source;
          result = sourceIllegal;
          break;
        }
	    } else {
        --source;
        result = sourceExhausted;
        break;
	    }
    } else if (flags == strictConversion) {

	    if (ch >= UNI_SUR_LOW_START && ch <= UNI_SUR_LOW_END) {
        --source;
        result = sourceIllegal;
        break;
	    }
    }
    if (target >= targetEnd) {
	    source = oldSource;
	    result = targetExhausted; break;
    }
    *target++ = ch;
  }
  *sourceStart = source;
  *targetStart = target;
#ifdef CVTUTF_DEBUG
  if (result == sourceIllegal) {
    fprintf(stderr, "ConvertUTF16toUTF32 illegal seq 0x%04x,%04x\n", ch, ch2);
    fflush(stderr);
  }
#endif
  return result;
}

static const char trailingBytesForUTF8[256] = {
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
  2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2, 3,3,3,3,3,3,3,3,4,4,4,4,5,5,5,5
};

static const UTF32 offsetsFromUTF8[6] = { 0x00000000UL, 0x00003080UL, 0x000E2080UL,
  0x03C82080UL, 0xFA082080UL, 0x82082080UL };

static const UTF8 firstByteMark[7] = { 0x00, 0x00, 0xC0, 0xE0, 0xF0, 0xF8, 0xFC };

ConversionResult ConvertUTF16toUTF8 (const UTF16** sourceStart, const UTF16* sourceEnd,
                                     UTF8** targetStart, UTF8* targetEnd, ConversionFlags flags) {
  ConversionResult result = conversionOK;
  const UTF16* source = *sourceStart;
  UTF8* target = *targetStart;
  while (source < sourceEnd) {
    UTF32 ch;
    unsigned short bytesToWrite = 0;
    const UTF32 byteMask = 0xBF;
    const UTF32 byteMark = 0x80;
    const UTF16* oldSource = source;
    ch = *source++;

    if (ch >= UNI_SUR_HIGH_START && ch <= UNI_SUR_HIGH_END) {

	    if (source < sourceEnd) {
        UTF32 ch2 = *source;

        if (ch2 >= UNI_SUR_LOW_START && ch2 <= UNI_SUR_LOW_END) {
          ch = ((ch - UNI_SUR_HIGH_START) << halfShift)
          + (ch2 - UNI_SUR_LOW_START) + halfBase;
          ++source;
        } else if (flags == strictConversion) {
          --source;
          result = sourceIllegal;
          break;
        }
	    } else {
        --source;
        result = sourceExhausted;
        break;
	    }
    } else if (flags == strictConversion) {

	    if (ch >= UNI_SUR_LOW_START && ch <= UNI_SUR_LOW_END) {
        --source;
        result = sourceIllegal;
        break;
	    }
    }

    if (ch < (UTF32)0x80) {	     bytesToWrite = 1;
    } else if (ch < (UTF32)0x800) {     bytesToWrite = 2;
    } else if (ch < (UTF32)0x10000) {   bytesToWrite = 3;
    } else if (ch < (UTF32)0x110000) {  bytesToWrite = 4;
    } else {			    bytesToWrite = 3;
      ch = UNI_REPLACEMENT_CHAR;
    }

    target += bytesToWrite;
    if (target > targetEnd) {
	    source = oldSource;
	    target -= bytesToWrite; result = targetExhausted; break;
    }
    switch (bytesToWrite) {
	    case 4: *--target = (UTF8)((ch | byteMark) & byteMask); ch >>= 6;
	    case 3: *--target = (UTF8)((ch | byteMark) & byteMask); ch >>= 6;
	    case 2: *--target = (UTF8)((ch | byteMark) & byteMask); ch >>= 6;
	    case 1: *--target =  (UTF8)(ch | firstByteMark[bytesToWrite]);
    }
    target += bytesToWrite;
  }
*sourceStart = source;
*targetStart = target;
return result;
}

static Boolean isLegalUTF8(const UTF8 *source, int length) {
  UTF8 a;
  const UTF8 *srcptr = source+length;
  switch (length) {
    default: return false;

    case 4: if ((a = (*--srcptr)) < 0x80 || a > 0xBF) return false;
    case 3: if ((a = (*--srcptr)) < 0x80 || a > 0xBF) return false;
    case 2: if ((a = (*--srcptr)) > 0xBF) return false;

      switch (*source) {

        case 0xE0: if (a < 0xA0) return false; break;
        case 0xED: if (a > 0x9F) return false; break;
        case 0xF0: if (a < 0x90) return false; break;
        case 0xF4: if (a > 0x8F) return false; break;
        default:   if (a < 0x80) return false;
      }

      case 1: if (*source >= 0x80 && *source < 0xC2) return false;
  }
  if (*source > 0xF4) return false;
  return true;
}

Boolean isLegalUTF8Sequence(const UTF8 *source, const UTF8 *sourceEnd) {
  int length = trailingBytesForUTF8[*source]+1;
  if (source+length > sourceEnd) {
    return false;
  }
  return isLegalUTF8(source, length);
}

ConversionResult ConvertUTF8toUTF16 (const UTF8** sourceStart, const UTF8* sourceEnd,
                                     UTF16** targetStart, UTF16* targetEnd, ConversionFlags flags) {
  ConversionResult result = conversionOK;
  const UTF8* source = *sourceStart;
  UTF16* target = *targetStart;
  while (source < sourceEnd) {
    UTF32 ch = 0;
    unsigned short extraBytesToRead = trailingBytesForUTF8[*source];
    if (source + extraBytesToRead >= sourceEnd) {
	    result = sourceExhausted; break;
    }

    if (! isLegalUTF8(source, extraBytesToRead+1)) {
	    result = sourceIllegal;
	    break;
    }

    switch (extraBytesToRead) {
	    case 5: ch += *source++; ch <<= 6;
	    case 4: ch += *source++; ch <<= 6;
	    case 3: ch += *source++; ch <<= 6;
	    case 2: ch += *source++; ch <<= 6;
	    case 1: ch += *source++; ch <<= 6;
	    case 0: ch += *source++;
    }
    ch -= offsetsFromUTF8[extraBytesToRead];

    if (target >= targetEnd) {
	    source -= (extraBytesToRead+1);
	    result = targetExhausted; break;
    }
    if (ch <= UNI_MAX_BMP) {

	    if (ch >= UNI_SUR_HIGH_START && ch <= UNI_SUR_LOW_END) {
        if (flags == strictConversion) {
          source -= (extraBytesToRead+1);
          result = sourceIllegal;
          break;
        } else {
          *target++ = UNI_REPLACEMENT_CHAR;
        }
	    } else {
        *target++ = (UTF16)ch;
	    }
    } else if (ch > UNI_MAX_UTF16) {
	    if (flags == strictConversion) {
        result = sourceIllegal;
        source -= (extraBytesToRead+1);
        break;
	    } else {
        *target++ = UNI_REPLACEMENT_CHAR;
	    }
    } else {

	    if (target + 1 >= targetEnd) {
        source -= (extraBytesToRead+1);
        result = targetExhausted; break;
	    }
	    ch -= halfBase;
	    *target++ = (UTF16)((ch >> halfShift) + UNI_SUR_HIGH_START);
	    *target++ = (UTF16)((ch & halfMask) + UNI_SUR_LOW_START);
    }
  }
*sourceStart = source;
*targetStart = target;
return result;
}

ConversionResult ConvertUTF32toUTF8 (const UTF32** sourceStart, const UTF32* sourceEnd,
                                     UTF8** targetStart, UTF8* targetEnd, ConversionFlags flags) {
  ConversionResult result = conversionOK;
  const UTF32* source = *sourceStart;
  UTF8* target = *targetStart;
  while (source < sourceEnd) {
    UTF32 ch;
    unsigned short bytesToWrite = 0;
    const UTF32 byteMask = 0xBF;
    const UTF32 byteMark = 0x80;
    ch = *source++;
    if (flags == strictConversion ) {

	    if (ch >= UNI_SUR_HIGH_START && ch <= UNI_SUR_LOW_END) {
        --source;
        result = sourceIllegal;
        break;
	    }
    }

    if (ch < (UTF32)0x80) {	     bytesToWrite = 1;
    } else if (ch < (UTF32)0x800) {     bytesToWrite = 2;
    } else if (ch < (UTF32)0x10000) {   bytesToWrite = 3;
    } else if (ch <= UNI_MAX_LEGAL_UTF32) {  bytesToWrite = 4;
    } else {			    bytesToWrite = 3;
      ch = UNI_REPLACEMENT_CHAR;
      result = sourceIllegal;
    }

    target += bytesToWrite;
    if (target > targetEnd) {
	    --source;
	    target -= bytesToWrite; result = targetExhausted; break;
    }
    switch (bytesToWrite) {
	    case 4: *--target = (UTF8)((ch | byteMark) & byteMask); ch >>= 6;
	    case 3: *--target = (UTF8)((ch | byteMark) & byteMask); ch >>= 6;
	    case 2: *--target = (UTF8)((ch | byteMark) & byteMask); ch >>= 6;
	    case 1: *--target = (UTF8) (ch | firstByteMark[bytesToWrite]);
    }
    target += bytesToWrite;
  }
*sourceStart = source;
*targetStart = target;
return result;
}

ConversionResult ConvertUTF8toUTF32 (const UTF8** sourceStart, const UTF8* sourceEnd,
                                     UTF32** targetStart, UTF32* targetEnd, ConversionFlags flags) {
  ConversionResult result = conversionOK;
  const UTF8* source = *sourceStart;
  UTF32* target = *targetStart;
  while (source < sourceEnd) {
    UTF32 ch = 0;
    unsigned short extraBytesToRead = trailingBytesForUTF8[*source];
    if (source + extraBytesToRead >= sourceEnd) {
	    result = sourceExhausted; break;
    }

    if (! isLegalUTF8(source, extraBytesToRead+1)) {
	    result = sourceIllegal;
	    break;
    }

    switch (extraBytesToRead) {
	    case 5: ch += *source++; ch <<= 6;
	    case 4: ch += *source++; ch <<= 6;
	    case 3: ch += *source++; ch <<= 6;
	    case 2: ch += *source++; ch <<= 6;
	    case 1: ch += *source++; ch <<= 6;
	    case 0: ch += *source++;
    }
    ch -= offsetsFromUTF8[extraBytesToRead];

    if (target >= targetEnd) {
	    source -= (extraBytesToRead+1);
	    result = targetExhausted; break;
    }
    if (ch <= UNI_MAX_LEGAL_UTF32) {

	    if (ch >= UNI_SUR_HIGH_START && ch <= UNI_SUR_LOW_END) {
        if (flags == strictConversion) {
          source -= (extraBytesToRead+1);
          result = sourceIllegal;
          break;
        } else {
          *target++ = UNI_REPLACEMENT_CHAR;
        }
	    } else {
        *target++ = ch;
	    }
    } else {
	    result = sourceIllegal;
	    *target++ = UNI_REPLACEMENT_CHAR;
    }
  }
  *sourceStart = source;
  *targetStart = target;
  return result;
}
