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

#ifndef OPENSSL_HEADER_BIO_H
#define OPENSSL_HEADER_BIO_H

#include <openssl/base.h>

#include <stdio.h>

#include <openssl/buffer.h>
#include <openssl/err.h>
#include <openssl/ex_data.h>
#include <openssl/stack.h>
#include <openssl/thread.h>

#if defined(__cplusplus)
extern "C" {
#endif

DEFINE_STACK_OF(BIO)

OPENSSL_EXPORT BIO *BIO_new(const BIO_METHOD *method);

OPENSSL_EXPORT int BIO_free(BIO *bio);

OPENSSL_EXPORT void BIO_vfree(BIO *bio);

OPENSSL_EXPORT int BIO_up_ref(BIO *bio);

OPENSSL_EXPORT int BIO_read(BIO *bio, void *data, int len);

OPENSSL_EXPORT int BIO_gets(BIO *bio, char *buf, int size);

OPENSSL_EXPORT int BIO_write(BIO *bio, const void *data, int len);

OPENSSL_EXPORT int BIO_write_all(BIO *bio, const void *data, size_t len);

OPENSSL_EXPORT int BIO_puts(BIO *bio, const char *buf);

OPENSSL_EXPORT int BIO_flush(BIO *bio);

OPENSSL_EXPORT long BIO_ctrl(BIO *bio, int cmd, long larg, void *parg);

OPENSSL_EXPORT char *BIO_ptr_ctrl(BIO *bp, int cmd, long larg);

OPENSSL_EXPORT long BIO_int_ctrl(BIO *bp, int cmd, long larg, int iarg);

OPENSSL_EXPORT int BIO_reset(BIO *bio);

OPENSSL_EXPORT int BIO_eof(BIO *bio);

OPENSSL_EXPORT void BIO_set_flags(BIO *bio, int flags);

OPENSSL_EXPORT int BIO_test_flags(const BIO *bio, int flags);

OPENSSL_EXPORT int BIO_should_read(const BIO *bio);

OPENSSL_EXPORT int BIO_should_write(const BIO *bio);

OPENSSL_EXPORT int BIO_should_retry(const BIO *bio);

OPENSSL_EXPORT int BIO_should_io_special(const BIO *bio);

#define BIO_RR_CONNECT 0x02

#define BIO_RR_ACCEPT 0x03

OPENSSL_EXPORT int BIO_get_retry_reason(const BIO *bio);

OPENSSL_EXPORT void BIO_set_retry_reason(BIO *bio, int reason);

OPENSSL_EXPORT void BIO_clear_flags(BIO *bio, int flags);

OPENSSL_EXPORT void BIO_set_retry_read(BIO *bio);

OPENSSL_EXPORT void BIO_set_retry_write(BIO *bio);

OPENSSL_EXPORT int BIO_get_retry_flags(BIO *bio);

OPENSSL_EXPORT void BIO_clear_retry_flags(BIO *bio);

OPENSSL_EXPORT int BIO_method_type(const BIO *bio);

#define BIO_CB_FREE 0x01
#define BIO_CB_READ 0x02
#define BIO_CB_WRITE 0x03
#define BIO_CB_PUTS 0x04
#define BIO_CB_GETS 0x05
#define BIO_CB_CTRL 0x06

#define BIO_CB_RETURN 0x80

typedef long (*bio_info_cb)(BIO *bio, int event, const char *parg, int cmd,
                            long larg, long return_value);

OPENSSL_EXPORT long BIO_callback_ctrl(BIO *bio, int cmd, bio_info_cb fp);

OPENSSL_EXPORT size_t BIO_pending(const BIO *bio);

OPENSSL_EXPORT size_t BIO_ctrl_pending(const BIO *bio);

OPENSSL_EXPORT size_t BIO_wpending(const BIO *bio);

OPENSSL_EXPORT int BIO_set_close(BIO *bio, int close_flag);

OPENSSL_EXPORT uint64_t BIO_number_read(const BIO *bio);

OPENSSL_EXPORT uint64_t BIO_number_written(const BIO *bio);

OPENSSL_EXPORT BIO *BIO_push(BIO *bio, BIO *appended_bio);

OPENSSL_EXPORT BIO *BIO_pop(BIO *bio);

OPENSSL_EXPORT BIO *BIO_next(BIO *bio);

OPENSSL_EXPORT void BIO_free_all(BIO *bio);

OPENSSL_EXPORT BIO *BIO_find_type(BIO *bio, int type);

OPENSSL_EXPORT void BIO_copy_next_retry(BIO *bio);

OPENSSL_EXPORT int BIO_printf(BIO *bio, const char *format, ...)
    OPENSSL_PRINTF_FORMAT_FUNC(2, 3);

OPENSSL_EXPORT int BIO_indent(BIO *bio, unsigned indent, unsigned max_indent);

OPENSSL_EXPORT int BIO_hexdump(BIO *bio, const uint8_t *data, size_t len,
                               unsigned indent);

OPENSSL_EXPORT void ERR_print_errors(BIO *bio);

OPENSSL_EXPORT int BIO_read_asn1(BIO *bio, uint8_t **out, size_t *out_len,
                                 size_t max_len);

#define BIO_NOCLOSE 0
#define BIO_CLOSE 1

OPENSSL_EXPORT const BIO_METHOD *BIO_s_mem(void);

OPENSSL_EXPORT BIO *BIO_new_mem_buf(const void *buf, ossl_ssize_t len);

OPENSSL_EXPORT int BIO_mem_contents(const BIO *bio,
                                    const uint8_t **out_contents,
                                    size_t *out_len);

OPENSSL_EXPORT long BIO_get_mem_data(BIO *bio, char **contents);

OPENSSL_EXPORT int BIO_get_mem_ptr(BIO *bio, BUF_MEM **out);

OPENSSL_EXPORT int BIO_set_mem_buf(BIO *bio, BUF_MEM *b, int take_ownership);

OPENSSL_EXPORT int BIO_set_mem_eof_return(BIO *bio, int eof_value);

#if !defined(OPENSSL_NO_POSIX_IO)

OPENSSL_EXPORT const BIO_METHOD *BIO_s_fd(void);

OPENSSL_EXPORT BIO *BIO_new_fd(int fd, int close_flag);
#endif

OPENSSL_EXPORT int BIO_set_fd(BIO *bio, int fd, int close_flag);

OPENSSL_EXPORT int BIO_get_fd(BIO *bio, int *out_fd);

OPENSSL_EXPORT const BIO_METHOD *BIO_s_file(void);

OPENSSL_EXPORT BIO *BIO_new_file(const char *filename, const char *mode);

#define BIO_FP_TEXT 0x10

OPENSSL_EXPORT BIO *BIO_new_fp(FILE *file, int flags);

OPENSSL_EXPORT int BIO_get_fp(BIO *bio, FILE **out_file);

OPENSSL_EXPORT int BIO_set_fp(BIO *bio, FILE *file, int flags);

OPENSSL_EXPORT int BIO_read_filename(BIO *bio, const char *filename);

OPENSSL_EXPORT int BIO_write_filename(BIO *bio, const char *filename);

OPENSSL_EXPORT int BIO_append_filename(BIO *bio, const char *filename);

OPENSSL_EXPORT int BIO_rw_filename(BIO *bio, const char *filename);

OPENSSL_EXPORT long BIO_tell(BIO *bio);

OPENSSL_EXPORT long BIO_seek(BIO *bio, long offset);

#if !defined(OPENSSL_NO_SOCK)
OPENSSL_EXPORT const BIO_METHOD *BIO_s_socket(void);

OPENSSL_EXPORT BIO *BIO_new_socket(int fd, int close_flag);
#endif

#if !defined(OPENSSL_NO_SOCK)
OPENSSL_EXPORT const BIO_METHOD *BIO_s_connect(void);

OPENSSL_EXPORT BIO *BIO_new_connect(const char *host_and_optional_port);

OPENSSL_EXPORT int BIO_set_conn_hostname(BIO *bio,
                                         const char *host_and_optional_port);

OPENSSL_EXPORT int BIO_set_conn_port(BIO *bio, const char *port_str);

OPENSSL_EXPORT int BIO_set_conn_int_port(BIO *bio, const int *port);

OPENSSL_EXPORT int BIO_set_nbio(BIO *bio, int on);

OPENSSL_EXPORT int BIO_do_connect(BIO *bio);
#endif

#define BIO_CTRL_DGRAM_QUERY_MTU 40  // as kernel for current MTU

#define BIO_CTRL_DGRAM_SET_MTU 42

#define BIO_CTRL_DGRAM_MTU_EXCEEDED 43

#define BIO_CTRL_DGRAM_GET_PEER           46

#define BIO_CTRL_DGRAM_GET_FALLBACK_MTU   47

OPENSSL_EXPORT int BIO_new_bio_pair(BIO **out1, size_t writebuf1, BIO **out2,
                                    size_t writebuf2);

OPENSSL_EXPORT size_t BIO_ctrl_get_read_request(BIO *bio);

OPENSSL_EXPORT size_t BIO_ctrl_get_write_guarantee(BIO *bio);

OPENSSL_EXPORT int BIO_shutdown_wr(BIO *bio);

OPENSSL_EXPORT int BIO_get_new_index(void);

OPENSSL_EXPORT BIO_METHOD *BIO_meth_new(int type, const char *name);

OPENSSL_EXPORT void BIO_meth_free(BIO_METHOD *method);

OPENSSL_EXPORT int BIO_meth_set_create(BIO_METHOD *method,
                                       int (*create_func)(BIO *));

OPENSSL_EXPORT int BIO_meth_set_destroy(BIO_METHOD *method,
                                        int (*destroy_func)(BIO *));

OPENSSL_EXPORT int BIO_meth_set_write(BIO_METHOD *method,
                                      int (*write_func)(BIO *, const char *,
                                                        int));

OPENSSL_EXPORT int BIO_meth_set_read(BIO_METHOD *method,
                                     int (*read_func)(BIO *, char *, int));

OPENSSL_EXPORT int BIO_meth_set_gets(BIO_METHOD *method,
                                     int (*gets_func)(BIO *, char *, int));

OPENSSL_EXPORT int BIO_meth_set_ctrl(BIO_METHOD *method,
                                     long (*ctrl_func)(BIO *, int, long,
                                                       void *));

OPENSSL_EXPORT void BIO_set_data(BIO *bio, void *ptr);

OPENSSL_EXPORT void *BIO_get_data(BIO *bio);

OPENSSL_EXPORT void BIO_set_init(BIO *bio, int init);

OPENSSL_EXPORT int BIO_get_init(BIO *bio);

#define BIO_CTRL_RESET 1

#define BIO_CTRL_EOF 2

#define BIO_CTRL_INFO 3

#define BIO_CTRL_GET_CLOSE 8

#define BIO_CTRL_SET_CLOSE 9

#define BIO_CTRL_PENDING 10

#define BIO_CTRL_FLUSH 11

#define BIO_CTRL_WPENDING 13

#define BIO_CTRL_SET_CALLBACK 14

#define BIO_CTRL_GET_CALLBACK 15

#define BIO_CTRL_SET 4
#define BIO_CTRL_GET 5
#define BIO_CTRL_PUSH 6
#define BIO_CTRL_POP 7
#define BIO_CTRL_DUP 12
#define BIO_CTRL_SET_FILENAME 30

OPENSSL_EXPORT int BIO_get_ex_new_index(long argl, void *argp,
                                        CRYPTO_EX_unused *unused,
                                        CRYPTO_EX_dup *dup_unused,
                                        CRYPTO_EX_free *free_func);
OPENSSL_EXPORT int BIO_set_ex_data(BIO *bio, int idx, void *arg);
OPENSSL_EXPORT void *BIO_get_ex_data(const BIO *bio, int idx);

#define BIO_set_app_data(bio, arg) (BIO_set_ex_data(bio, 0, (char *)(arg)))
#define BIO_get_app_data(bio) (BIO_get_ex_data(bio, 0))

OPENSSL_EXPORT const BIO_METHOD *BIO_f_base64(void);

OPENSSL_EXPORT void BIO_set_retry_special(BIO *bio);

OPENSSL_EXPORT int BIO_set_write_buffer_size(BIO *bio, int buffer_size);

OPENSSL_EXPORT void BIO_set_shutdown(BIO *bio, int shutdown);

OPENSSL_EXPORT int BIO_get_shutdown(BIO *bio);

OPENSSL_EXPORT int BIO_meth_set_puts(BIO_METHOD *method,
                                     int (*puts)(BIO *, const char *));

#define BIO_FLAGS_READ 0x01
#define BIO_FLAGS_WRITE 0x02
#define BIO_FLAGS_IO_SPECIAL 0x04
#define BIO_FLAGS_RWS (BIO_FLAGS_READ | BIO_FLAGS_WRITE | BIO_FLAGS_IO_SPECIAL)
#define BIO_FLAGS_SHOULD_RETRY 0x08
#define BIO_FLAGS_BASE64_NO_NL 0x100

#define BIO_FLAGS_MEM_RDONLY 0x200

#define BIO_TYPE_DESCRIPTOR 0x0100  // socket, fd, connect or accept
#define BIO_TYPE_FILTER 0x0200
#define BIO_TYPE_SOURCE_SINK 0x0400

#define BIO_TYPE_NONE 0
#define BIO_TYPE_MEM (1 | BIO_TYPE_SOURCE_SINK)
#define BIO_TYPE_FILE (2 | BIO_TYPE_SOURCE_SINK)
#define BIO_TYPE_FD (4 | BIO_TYPE_SOURCE_SINK | BIO_TYPE_DESCRIPTOR)
#define BIO_TYPE_SOCKET (5 | BIO_TYPE_SOURCE_SINK | BIO_TYPE_DESCRIPTOR)
#define BIO_TYPE_NULL (6 | BIO_TYPE_SOURCE_SINK)
#define BIO_TYPE_SSL (7 | BIO_TYPE_FILTER)
#define BIO_TYPE_MD (8 | BIO_TYPE_FILTER)
#define BIO_TYPE_BUFFER (9 | BIO_TYPE_FILTER)
#define BIO_TYPE_CIPHER (10 | BIO_TYPE_FILTER)
#define BIO_TYPE_BASE64 (11 | BIO_TYPE_FILTER)
#define BIO_TYPE_CONNECT (12 | BIO_TYPE_SOURCE_SINK | BIO_TYPE_DESCRIPTOR)
#define BIO_TYPE_ACCEPT (13 | BIO_TYPE_SOURCE_SINK | BIO_TYPE_DESCRIPTOR)
#define BIO_TYPE_PROXY_CLIENT (14 | BIO_TYPE_FILTER)
#define BIO_TYPE_PROXY_SERVER (15 | BIO_TYPE_FILTER)
#define BIO_TYPE_NBIO_TEST (16 | BIO_TYPE_FILTER)
#define BIO_TYPE_NULL_FILTER (17 | BIO_TYPE_FILTER)
#define BIO_TYPE_BER (18 | BIO_TYPE_FILTER)       // BER -> bin filter
#define BIO_TYPE_BIO (19 | BIO_TYPE_SOURCE_SINK)  // (half a) BIO pair
#define BIO_TYPE_LINEBUFFER (20 | BIO_TYPE_FILTER)
#define BIO_TYPE_DGRAM (21 | BIO_TYPE_SOURCE_SINK | BIO_TYPE_DESCRIPTOR)
#define BIO_TYPE_ASN1 (22 | BIO_TYPE_FILTER)
#define BIO_TYPE_COMP (23 | BIO_TYPE_FILTER)

#define BIO_TYPE_START 128

struct bio_method_st {
  int type;
  const char *name;
  int (*bwrite)(BIO *, const char *, int);
  int (*bread)(BIO *, char *, int);

  int (*bputs)(BIO *, const char *);
  int (*bgets)(BIO *, char *, int);
  long (*ctrl)(BIO *, int, long, void *);
  int (*create)(BIO *);
  int (*destroy)(BIO *);
  long (*callback_ctrl)(BIO *, int, bio_info_cb);
};

struct bio_st {
  const BIO_METHOD *method;
  CRYPTO_EX_DATA ex_data;

  int init;

  int shutdown;
  int flags;
  int retry_reason;

  int num;
  CRYPTO_refcount_t references;
  void *ptr;

  BIO *next_bio;
  uint64_t num_read, num_write;
};

#define BIO_C_SET_CONNECT 100
#define BIO_C_DO_STATE_MACHINE 101
#define BIO_C_SET_NBIO 102
#define BIO_C_SET_PROXY_PARAM 103
#define BIO_C_SET_FD 104
#define BIO_C_GET_FD 105
#define BIO_C_SET_FILE_PTR 106
#define BIO_C_GET_FILE_PTR 107
#define BIO_C_SET_FILENAME 108
#define BIO_C_SET_SSL 109
#define BIO_C_SET_MD 111
#define BIO_C_GET_MD 112
#define BIO_C_GET_CIPHER_STATUS 113
#define BIO_C_SET_BUF_MEM 114
#define BIO_C_GET_BUF_MEM_PTR 115
#define BIO_C_GET_BUFF_NUM_LINES 116
#define BIO_C_SET_BUFF_SIZE 117
#define BIO_C_SET_ACCEPT 118
#define BIO_C_SSL_MODE 119
#define BIO_C_GET_MD_CTX 120
#define BIO_C_GET_PROXY_PARAM 121
#define BIO_C_SET_BUFF_READ_DATA 122  // data to read first
#define BIO_C_GET_ACCEPT 124
#define BIO_C_FILE_SEEK 128
#define BIO_C_GET_CIPHER_CTX 129
#define BIO_C_SET_BUF_MEM_EOF_RETURN 130  // return end of input value
#define BIO_C_SET_BIND_MODE 131
#define BIO_C_GET_BIND_MODE 132
#define BIO_C_FILE_TELL 133
#define BIO_C_GET_SOCKS 134
#define BIO_C_SET_SOCKS 135

#define BIO_C_SET_WRITE_BUF_SIZE 136  // for BIO_s_bio
#define BIO_C_GET_WRITE_BUF_SIZE 137
#define BIO_C_GET_WRITE_GUARANTEE 140
#define BIO_C_GET_READ_REQUEST 141
#define BIO_C_SHUTDOWN_WR 142
#define BIO_C_NREAD0 143
#define BIO_C_NREAD 144
#define BIO_C_NWRITE0 145
#define BIO_C_NWRITE 146
#define BIO_C_RESET_READ_REQUEST 147
#define BIO_C_SET_MD_CTX 148

#define BIO_C_SET_PREFIX 149
#define BIO_C_GET_PREFIX 150
#define BIO_C_SET_SUFFIX 151
#define BIO_C_GET_SUFFIX 152

#define BIO_C_SET_EX_ARG 153
#define BIO_C_GET_EX_ARG 154

#if defined(__cplusplus)
}

extern "C++" {

BSSL_NAMESPACE_BEGIN

BORINGSSL_MAKE_DELETER(BIO, BIO_free)
BORINGSSL_MAKE_UP_REF(BIO, BIO_up_ref)
BORINGSSL_MAKE_DELETER(BIO_METHOD, BIO_meth_free)

BSSL_NAMESPACE_END

}

#endif

#define BIO_R_BAD_FOPEN_MODE 100
#define BIO_R_BROKEN_PIPE 101
#define BIO_R_CONNECT_ERROR 102
#define BIO_R_ERROR_SETTING_NBIO 103
#define BIO_R_INVALID_ARGUMENT 104
#define BIO_R_IN_USE 105
#define BIO_R_KEEPALIVE 106
#define BIO_R_NBIO_CONNECT_ERROR 107
#define BIO_R_NO_HOSTNAME_SPECIFIED 108
#define BIO_R_NO_PORT_SPECIFIED 109
#define BIO_R_NO_SUCH_FILE 110
#define BIO_R_NULL_PARAMETER 111
#define BIO_R_SYS_LIB 112
#define BIO_R_UNABLE_TO_CREATE_SOCKET 113
#define BIO_R_UNINITIALIZED 114
#define BIO_R_UNSUPPORTED_METHOD 115
#define BIO_R_WRITE_TO_READ_ONLY_BIO 116

#endif
