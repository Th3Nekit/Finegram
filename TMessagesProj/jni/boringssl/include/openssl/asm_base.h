// Copyright 2023 The BoringSSL Authors
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

#ifndef OPENSSL_HEADER_ASM_BASE_H
#define OPENSSL_HEADER_ASM_BASE_H

#include <openssl/target.h>

#if defined(__ASSEMBLER__)

#if defined(BORINGSSL_PREFIX)
#include <boringssl_prefix_symbols_asm.h>
#endif

#if defined(__ELF__)

.pushsection .note.GNU-stack, "", %progbits
.popsection
#endif

#if defined(__CET__) && defined(OPENSSL_X86_64)

#include <cet.h>
#else
#define _CET_ENDBR
#endif

#if defined(OPENSSL_ARM) || defined(OPENSSL_AARCH64)

#if !defined(__ARM_ARCH)
#error "ARM assembler must define __ARM_ARCH"
#endif

#define __ARM_MAX_ARCH__ 8

#if defined(__ARM_FEATURE_BTI_DEFAULT) && __ARM_FEATURE_BTI_DEFAULT == 1
#define GNU_PROPERTY_AARCH64_BTI (1 << 0)   // Has Branch Target Identification
#define AARCH64_VALID_CALL_TARGET hint #34  // BTI 'c'
#else
#define GNU_PROPERTY_AARCH64_BTI 0  // No Branch Target Identification
#define AARCH64_VALID_CALL_TARGET
#endif

#if defined(__ARM_FEATURE_PAC_DEFAULT) && \
    (__ARM_FEATURE_PAC_DEFAULT & 1) == 1
#define GNU_PROPERTY_AARCH64_POINTER_AUTH \
  (1 << 1)                                       // Has Pointer Authentication
#define AARCH64_SIGN_LINK_REGISTER hint #25      // PACIASP
#define AARCH64_VALIDATE_LINK_REGISTER hint #29  // AUTIASP
#elif defined(__ARM_FEATURE_PAC_DEFAULT) && \
    (__ARM_FEATURE_PAC_DEFAULT & 2) == 2
#define GNU_PROPERTY_AARCH64_POINTER_AUTH \
  (1 << 1)                                       // Has Pointer Authentication
#define AARCH64_SIGN_LINK_REGISTER hint #27      // PACIBSP
#define AARCH64_VALIDATE_LINK_REGISTER hint #31  // AUTIBSP
#else
#define GNU_PROPERTY_AARCH64_POINTER_AUTH 0  // No Pointer Authentication
#if GNU_PROPERTY_AARCH64_BTI != 0
#define AARCH64_SIGN_LINK_REGISTER AARCH64_VALID_CALL_TARGET
#else
#define AARCH64_SIGN_LINK_REGISTER
#endif
#define AARCH64_VALIDATE_LINK_REGISTER
#endif

#if GNU_PROPERTY_AARCH64_POINTER_AUTH != 0 || GNU_PROPERTY_AARCH64_BTI != 0
.pushsection .note.gnu.property, "a";
.balign 8;
.long 4;
.long 0x10;
.long 0x5;
.asciz "GNU";
.long 0xc0000000;
.long 4;
.long (GNU_PROPERTY_AARCH64_POINTER_AUTH | GNU_PROPERTY_AARCH64_BTI);
.long 0;
.popsection;
#endif
#endif

#endif

#endif
