/*
 * Copyright (c) 2007 Mans Rullgard
 *
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef AVUTIL_AVSTRING_H
#define AVUTIL_AVSTRING_H

#include <stddef.h>
#include <stdint.h>
#include "attributes.h"

int av_strstart(const char *str, const char *pfx, const char **ptr);

int av_stristart(const char *str, const char *pfx, const char **ptr);

char *av_stristr(const char *haystack, const char *needle);

char *av_strnstr(const char *haystack, const char *needle, size_t hay_length);

size_t av_strlcpy(char *dst, const char *src, size_t size);

size_t av_strlcat(char *dst, const char *src, size_t size);

size_t av_strlcatf(char *dst, size_t size, const char *fmt, ...) av_printf_format(3, 4);

static inline size_t av_strnlen(const char *s, size_t len)
{
    size_t i;
    for (i = 0; i < len && s[i]; i++)
        ;
    return i;
}

char *av_asprintf(const char *fmt, ...) av_printf_format(1, 2);

char *av_get_token(const char **buf, const char *term);

char *av_strtok(char *s, const char *delim, char **saveptr);

static inline av_const int av_isdigit(int c)
{
    return c >= '0' && c <= '9';
}

static inline av_const int av_isgraph(int c)
{
    return c > 32 && c < 127;
}

static inline av_const int av_isspace(int c)
{
    return c == ' ' || c == '\f' || c == '\n' || c == '\r' || c == '\t' ||
           c == '\v';
}

static inline av_const int av_toupper(int c)
{
    if (c >= 'a' && c <= 'z')
        c ^= 0x20;
    return c;
}

static inline av_const int av_tolower(int c)
{
    if (c >= 'A' && c <= 'Z')
        c ^= 0x20;
    return c;
}

static inline av_const int av_isxdigit(int c)
{
    c = av_tolower(c);
    return av_isdigit(c) || (c >= 'a' && c <= 'f');
}

int av_strcasecmp(const char *a, const char *b);

int av_strncasecmp(const char *a, const char *b, size_t n);

char *av_strireplace(const char *str, const char *from, const char *to);

const char *av_basename(const char *path);

const char *av_dirname(char *path);

int av_match_name(const char *name, const char *names);

char *av_append_path_component(const char *path, const char *component);

enum AVEscapeMode {
    AV_ESCAPE_MODE_AUTO,
    AV_ESCAPE_MODE_BACKSLASH,
    AV_ESCAPE_MODE_QUOTE,
    AV_ESCAPE_MODE_XML,
};

#define AV_ESCAPE_FLAG_WHITESPACE (1 << 0)

#define AV_ESCAPE_FLAG_STRICT (1 << 1)

#define AV_ESCAPE_FLAG_XML_SINGLE_QUOTES (1 << 2)

#define AV_ESCAPE_FLAG_XML_DOUBLE_QUOTES (1 << 3)

av_warn_unused_result
int av_escape(char **dst, const char *src, const char *special_chars,
              enum AVEscapeMode mode, int flags);

#define AV_UTF8_FLAG_ACCEPT_INVALID_BIG_CODES          1 ///< accept codepoints over 0x10FFFF
#define AV_UTF8_FLAG_ACCEPT_NON_CHARACTERS             2 ///< accept non-characters - 0xFFFE and 0xFFFF
#define AV_UTF8_FLAG_ACCEPT_SURROGATES                 4 ///< accept UTF-16 surrogates codes
#define AV_UTF8_FLAG_EXCLUDE_XML_INVALID_CONTROL_CODES 8 ///< exclude control codes not accepted by XML

#define AV_UTF8_FLAG_ACCEPT_ALL \
    AV_UTF8_FLAG_ACCEPT_INVALID_BIG_CODES|AV_UTF8_FLAG_ACCEPT_NON_CHARACTERS|AV_UTF8_FLAG_ACCEPT_SURROGATES

av_warn_unused_result
int av_utf8_decode(int32_t *codep, const uint8_t **bufp, const uint8_t *buf_end,
                   unsigned int flags);

int av_match_list(const char *name, const char *list, char separator);

int av_sscanf(const char *string, const char *format, ...) av_scanf_format(2, 3);

#endif
