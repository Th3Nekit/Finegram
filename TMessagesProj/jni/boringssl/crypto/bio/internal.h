// Copyright 1995-2016 The OpenSSL Project Authors. All Rights Reserved.
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

#ifndef OPENSSL_HEADER_CRYPTO_BIO_INTERNAL_H
#define OPENSSL_HEADER_CRYPTO_BIO_INTERNAL_H

#include <openssl/base.h>

#if !defined(OPENSSL_NO_SOCK)
#if !defined(OPENSSL_WINDOWS)
#if defined(OPENSSL_PNACL)

typedef unsigned short u_short;
#endif
#include <sys/types.h>
#include <sys/socket.h>
#else
#include <winsock2.h>
typedef int socklen_t;
#endif
#endif

#if defined(__cplusplus)
extern "C" {
#endif

#if !defined(OPENSSL_NO_SOCK)

int bio_ip_and_port_to_socket_and_addr(int *out_sock,
                                       struct sockaddr_storage *out_addr,
                                       socklen_t *out_addr_length,
                                       const char *hostname,
                                       const char *port_str);

int bio_socket_nbio(int sock, int on);

void bio_clear_socket_error(void);

int bio_sock_error(int sock);

int bio_socket_should_retry(int return_value);

#endif

int bio_errno_should_retry(int return_value);

#if defined(__cplusplus)
}
#endif

#endif
