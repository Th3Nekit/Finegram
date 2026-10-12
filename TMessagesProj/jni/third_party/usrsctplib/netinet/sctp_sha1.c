/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 2001-2007, by Cisco Systems, Inc. All rights reserved.
 * Copyright (c) 2008-2012, by Randall Stewart. All rights reserved.
 * Copyright (c) 2008-2013, by Michael Tuexen. All rights reserved.
 * Copyright (c) 2013,      by Lally Singh. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * a) Redistributions of source code must retain the above copyright notice,
 *   this list of conditions and the following disclaimer.
 *
 * b) Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *   the documentation and/or other materials provided with the distribution.
 *
 * c) Neither the name of Cisco Systems, Inc. nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <netinet/sctp_sha1.h>

#if defined(SCTP_USE_NSS_SHA1)

#define SHA_DIGEST_LENGTH (20)

void
sctp_sha1_init(struct sctp_sha1_context *ctx)
{
	ctx->pk11_ctx = PK11_CreateDigestContext(SEC_OID_SHA1);
	PK11_DigestBegin(ctx->pk11_ctx);
}

void
sctp_sha1_update(struct sctp_sha1_context *ctx, const unsigned char *ptr, unsigned int siz)
{
	PK11_DigestOp(ctx->pk11_ctx, ptr, siz);
}

void
sctp_sha1_final(unsigned char *digest, struct sctp_sha1_context *ctx)
{
	unsigned int output_len = 0;

	PK11_DigestFinal(ctx->pk11_ctx, digest, &output_len, SHA_DIGEST_LENGTH);
	PK11_DestroyContext(ctx->pk11_ctx, PR_TRUE);
}

#elif defined(SCTP_USE_OPENSSL_SHA1)

void
sctp_sha1_init(struct sctp_sha1_context *ctx)
{
	SHA1_Init(&ctx->sha_ctx);
}

void
sctp_sha1_update(struct sctp_sha1_context *ctx, const unsigned char *ptr, unsigned int siz)
{
	SHA1_Update(&ctx->sha_ctx, ptr, (unsigned long)siz);
}

void
sctp_sha1_final(unsigned char *digest, struct sctp_sha1_context *ctx)
{
	SHA1_Final(digest, &ctx->sha_ctx);
}
#elif defined(SCTP_USE_MBEDTLS_SHA1)
void
sctp_sha1_init(struct sctp_sha1_context *ctx)
{
	mbedtls_sha1_init(&ctx->sha1_ctx);
	mbedtls_sha1_starts_ret(&ctx->sha1_ctx);
}

void
sctp_sha1_update(struct sctp_sha1_context *ctx, const unsigned char *ptr, unsigned int siz)
{
	mbedtls_sha1_update_ret(&ctx->sha1_ctx, ptr, siz);
}

void
sctp_sha1_final(unsigned char *digest, struct sctp_sha1_context *ctx)
{
	mbedtls_sha1_finish_ret(&ctx->sha1_ctx, digest);
}
#else

#include <string.h>
#if defined(_WIN32) && defined(__Userspace__)
#include <winsock2.h>
#elif !(defined(_WIN32) && !defined(__Userspace__))
#include <arpa/inet.h>
#endif

#define F1(B,C,D) (((B & C) | ((~B) & D)))
#define F2(B,C,D) (B ^ C ^ D)
#define F3(B,C,D) ((B & C) | (B & D) | (C & D))
#define F4(B,C,D) (B ^ C ^ D)

#define CSHIFT(A,B) ((B << A) | (B >> (32-A)))

#define K1 0x5a827999
#define K2 0x6ed9eba1
#define K3 0x8f1bbcdc
#define K4 0xca62c1d6

#define H0INIT 0x67452301
#define H1INIT 0xefcdab89
#define H2INIT 0x98badcfe
#define H3INIT 0x10325476
#define H4INIT 0xc3d2e1f0

void
sctp_sha1_init(struct sctp_sha1_context *ctx)
{

	ctx->A = 0;
	ctx->B = 0;
	ctx->C = 0;
	ctx->D = 0;
	ctx->E = 0;
	ctx->H0 = H0INIT;
	ctx->H1 = H1INIT;
	ctx->H2 = H2INIT;
	ctx->H3 = H3INIT;
	ctx->H4 = H4INIT;
	ctx->TEMP = 0;
	memset(ctx->words, 0, sizeof(ctx->words));
	ctx->how_many_in_block = 0;
	ctx->running_total = 0;
}

static void
sctp_sha1_process_a_block(struct sctp_sha1_context *ctx, unsigned int *block)
{
	int i;

	for (i = 0; i < 16; i++) {
		ctx->words[i] = ntohl(block[i]);
	}

	for (i = 16; i < 80; i++) {
		ctx->words[i] = CSHIFT(1, ((ctx->words[(i - 3)]) ^
		    (ctx->words[(i - 8)]) ^
		    (ctx->words[(i - 14)]) ^
		    (ctx->words[(i - 16)])));
	}

	ctx->A = ctx->H0;
	ctx->B = ctx->H1;
	ctx->C = ctx->H2;
	ctx->D = ctx->H3;
	ctx->E = ctx->H4;

	for (i = 0; i < 80; i++) {
		if (i < 20) {
			ctx->TEMP = ((CSHIFT(5, ctx->A)) +
			    (F1(ctx->B, ctx->C, ctx->D)) +
			    (ctx->E) +
			    ctx->words[i] +
			    K1);
		} else if (i < 40) {
			ctx->TEMP = ((CSHIFT(5, ctx->A)) +
			    (F2(ctx->B, ctx->C, ctx->D)) +
			    (ctx->E) +
			    (ctx->words[i]) +
			    K2);
		} else if (i < 60) {
			ctx->TEMP = ((CSHIFT(5, ctx->A)) +
			    (F3(ctx->B, ctx->C, ctx->D)) +
			    (ctx->E) +
			    (ctx->words[i]) +
			    K3);
		} else {
			ctx->TEMP = ((CSHIFT(5, ctx->A)) +
			    (F4(ctx->B, ctx->C, ctx->D)) +
			    (ctx->E) +
			    (ctx->words[i]) +
			    K4);
		}
		ctx->E = ctx->D;
		ctx->D = ctx->C;
		ctx->C = CSHIFT(30, ctx->B);
		ctx->B = ctx->A;
		ctx->A = ctx->TEMP;
	}

	ctx->H0 = (ctx->H0) + (ctx->A);
	ctx->H1 = (ctx->H1) + (ctx->B);
	ctx->H2 = (ctx->H2) + (ctx->C);
	ctx->H3 = (ctx->H3) + (ctx->D);
	ctx->H4 = (ctx->H4) + (ctx->E);
}

void
sctp_sha1_update(struct sctp_sha1_context *ctx, const unsigned char *ptr, unsigned int siz)
{
	unsigned int number_left, left_to_fill;

	number_left = siz;
	while (number_left > 0) {
		left_to_fill = sizeof(ctx->sha_block) - ctx->how_many_in_block;
		if (left_to_fill > number_left) {

			memcpy(&ctx->sha_block[ctx->how_many_in_block],
			    ptr, number_left);
			ctx->how_many_in_block += number_left;
			ctx->running_total += number_left;
			break;
		} else {

			memcpy(&ctx->sha_block[ctx->how_many_in_block],
			    ptr, left_to_fill);
			sctp_sha1_process_a_block(ctx,
			    (unsigned int *)ctx->sha_block);
			number_left -= left_to_fill;
			ctx->running_total += left_to_fill;
			ctx->how_many_in_block = 0;
			ptr = (const unsigned char *)(ptr + left_to_fill);
		}
	}
}

void
sctp_sha1_final(unsigned char *digest, struct sctp_sha1_context *ctx)
{

	int left_to_fill;
	unsigned int i, *ptr;

	if (ctx->how_many_in_block > 55) {

		left_to_fill = sizeof(ctx->sha_block) - ctx->how_many_in_block;
		if (left_to_fill == 0) {

			sctp_sha1_process_a_block(ctx,
			    (unsigned int *)ctx->sha_block);

			ctx->sha_block[0] = '\x80';
			for (i = 1; i < sizeof(ctx->sha_block); i++) {
				ctx->sha_block[i] = 0x0;
			}
		} else if (left_to_fill == 1) {
			ctx->sha_block[ctx->how_many_in_block] = '\x80';
			sctp_sha1_process_a_block(ctx,
			    (unsigned int *)ctx->sha_block);

			memset(ctx->sha_block, 0, sizeof(ctx->sha_block));
		} else {
			ctx->sha_block[ctx->how_many_in_block] = '\x80';
			for (i = (ctx->how_many_in_block + 1);
			    i < sizeof(ctx->sha_block);
			    i++) {
				ctx->sha_block[i] = 0x0;
			}
			sctp_sha1_process_a_block(ctx,
			    (unsigned int *)ctx->sha_block);

			memset(ctx->sha_block, 0, sizeof(ctx->sha_block));
		}

		ctx->running_total *= 8;
		ptr = (unsigned int *)&ctx->sha_block[60];
		*ptr = htonl(ctx->running_total);
		sctp_sha1_process_a_block(ctx, (unsigned int *)ctx->sha_block);
	} else {

		ctx->sha_block[ctx->how_many_in_block] = '\x80';
		for (i = (ctx->how_many_in_block + 1);
		    i < sizeof(ctx->sha_block);
		    i++) {
			ctx->sha_block[i] = 0x0;
		}

		ctx->running_total *= 8;
		ptr = (unsigned int *)&ctx->sha_block[60];
		*ptr = htonl(ctx->running_total);
		sctp_sha1_process_a_block(ctx, (unsigned int *)ctx->sha_block);
	}

	digest[3] = (ctx->H0 & 0xff);
	digest[2] = ((ctx->H0 >> 8) & 0xff);
	digest[1] = ((ctx->H0 >> 16) & 0xff);
	digest[0] = ((ctx->H0 >> 24) & 0xff);

	digest[7] = (ctx->H1 & 0xff);
	digest[6] = ((ctx->H1 >> 8) & 0xff);
	digest[5] = ((ctx->H1 >> 16) & 0xff);
	digest[4] = ((ctx->H1 >> 24) & 0xff);

	digest[11] = (ctx->H2 & 0xff);
	digest[10] = ((ctx->H2 >> 8) & 0xff);
	digest[9] = ((ctx->H2 >> 16) & 0xff);
	digest[8] = ((ctx->H2 >> 24) & 0xff);

	digest[15] = (ctx->H3 & 0xff);
	digest[14] = ((ctx->H3 >> 8) & 0xff);
	digest[13] = ((ctx->H3 >> 16) & 0xff);
	digest[12] = ((ctx->H3 >> 24) & 0xff);

	digest[19] = (ctx->H4 & 0xff);
	digest[18] = ((ctx->H4 >> 8) & 0xff);
	digest[17] = ((ctx->H4 >> 16) & 0xff);
	digest[16] = ((ctx->H4 >> 24) & 0xff);
}

#endif
