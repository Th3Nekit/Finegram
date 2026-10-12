// Copyright 2017 The CRC32C Authors. All rights reserved.

#ifndef CRC32C_CRC32C_CONFIG_H_
#define CRC32C_CRC32C_CONFIG_H_

#define HAVE_BUILTIN_PREFETCH 1

#if HAVE_SSE42 && (defined(_M_X64) || defined(__x86_64__))
#define HAVE_MM_PREFETCH 1
#endif

#if defined(__i386) || defined(__x86_64) || defined(_M_IX86)

#endif

#if defined(__aarch64__)

#endif

#define HAVE_STRONG_GETAUXVAL 1

#define HAVE_WEAK_GETAUXVAL

#endif
