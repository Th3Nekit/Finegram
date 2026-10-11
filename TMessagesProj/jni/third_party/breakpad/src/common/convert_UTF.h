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

#ifndef COMMON_CONVERT_UTF_H_
#define COMMON_CONVERT_UTF_H_

typedef unsigned long	UTF32;
typedef unsigned short	UTF16;
typedef unsigned char	UTF8;
typedef unsigned char	Boolean;

#define UNI_REPLACEMENT_CHAR (UTF32)0x0000FFFD
#define UNI_MAX_BMP (UTF32)0x0000FFFF
#define UNI_MAX_UTF16 (UTF32)0x0010FFFF
#define UNI_MAX_UTF32 (UTF32)0x7FFFFFFF
#define UNI_MAX_LEGAL_UTF32 (UTF32)0x0010FFFF

typedef enum {
	conversionOK,
	sourceExhausted,
	targetExhausted,
	sourceIllegal
} ConversionResult;

typedef enum {
	strictConversion = 0,
	lenientConversion
} ConversionFlags;

#ifdef __cplusplus
extern "C" {
#endif

ConversionResult ConvertUTF8toUTF16 (const UTF8** sourceStart, const UTF8* sourceEnd,
                                     UTF16** targetStart, UTF16* targetEnd, ConversionFlags flags);

ConversionResult ConvertUTF16toUTF8 (const UTF16** sourceStart, const UTF16* sourceEnd,
                                     UTF8** targetStart, UTF8* targetEnd, ConversionFlags flags);

ConversionResult ConvertUTF8toUTF32 (const UTF8** sourceStart, const UTF8* sourceEnd,
                                     UTF32** targetStart, UTF32* targetEnd, ConversionFlags flags);

ConversionResult ConvertUTF32toUTF8 (const UTF32** sourceStart, const UTF32* sourceEnd,
                                     UTF8** targetStart, UTF8* targetEnd, ConversionFlags flags);

ConversionResult ConvertUTF16toUTF32 (const UTF16** sourceStart, const UTF16* sourceEnd,
                                      UTF32** targetStart, UTF32* targetEnd, ConversionFlags flags);

ConversionResult ConvertUTF32toUTF16 (const UTF32** sourceStart, const UTF32* sourceEnd,
                                      UTF16** targetStart, UTF16* targetEnd, ConversionFlags flags);

Boolean isLegalUTF8Sequence(const UTF8 *source, const UTF8 *sourceEnd);

#ifdef __cplusplus
}
#endif

#endif
