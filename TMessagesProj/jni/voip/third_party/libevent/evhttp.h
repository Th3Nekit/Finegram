/*
 * Copyright (c) 2000-2004 Niels Provos <provos@citi.umich.edu>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#ifndef _EVHTTP_H_
#define _EVHTTP_H_

#include "event.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef WIN32
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <windows.h>
#undef WIN32_LEAN_AND_MEAN
#endif

#define HTTP_OK			200
#define HTTP_NOCONTENT		204
#define HTTP_MOVEPERM		301
#define HTTP_MOVETEMP		302
#define HTTP_NOTMODIFIED	304
#define HTTP_BADREQUEST		400
#define HTTP_NOTFOUND		404
#define HTTP_SERVUNAVAIL	503

struct evhttp;
struct evhttp_request;
struct evkeyvalq;

struct evhttp *evhttp_new(struct event_base *base);

int evhttp_bind_socket(struct evhttp *http, const char *address, u_short port);

int evhttp_accept_socket(struct evhttp *http, int fd);

void evhttp_free(struct evhttp* http);

void evhttp_set_cb(struct evhttp *, const char *,
    void (*)(struct evhttp_request *, void *), void *);

int evhttp_del_cb(struct evhttp *, const char *);

void evhttp_set_gencb(struct evhttp *,
    void (*)(struct evhttp_request *, void *), void *);

void evhttp_set_timeout(struct evhttp *, int timeout_in_secs);

void evhttp_send_error(struct evhttp_request *req, int error,
    const char *reason);

void evhttp_send_reply(struct evhttp_request *req, int code,
    const char *reason, struct evbuffer *databuf);

void evhttp_send_reply_start(struct evhttp_request *, int, const char *);
void evhttp_send_reply_chunk(struct evhttp_request *, struct evbuffer *);
void evhttp_send_reply_end(struct evhttp_request *);

struct evhttp *evhttp_start(const char *address, u_short port);

enum evhttp_cmd_type { EVHTTP_REQ_GET, EVHTTP_REQ_POST, EVHTTP_REQ_HEAD };

enum evhttp_request_kind { EVHTTP_REQUEST, EVHTTP_RESPONSE };

struct evhttp_request {
#if defined(TAILQ_ENTRY)
	TAILQ_ENTRY(evhttp_request) next;
#else
struct {
	struct evhttp_request *tqe_next;
	struct evhttp_request **tqe_prev;
}       next;
#endif

	struct evhttp_connection *evcon;
	int flags;
#define EVHTTP_REQ_OWN_CONNECTION	0x0001
#define EVHTTP_PROXY_REQUEST		0x0002

	struct evkeyvalq *input_headers;
	struct evkeyvalq *output_headers;

	char *remote_host;
	u_short remote_port;

	enum evhttp_request_kind kind;
	enum evhttp_cmd_type type;

	char *uri;

	char major;
	char minor;

	int response_code;
	char *response_code_line;

	struct evbuffer *input_buffer;
	ev_int64_t ntoread;
	int chunked:1,
	    userdone:1;

	struct evbuffer *output_buffer;

	void (*cb)(struct evhttp_request *, void *);
	void *cb_arg;

	void (*chunk_cb)(struct evhttp_request *, void *);
};

struct evhttp_request *evhttp_request_new(
	void (*cb)(struct evhttp_request *, void *), void *arg);

void evhttp_request_set_chunked_cb(struct evhttp_request *,
    void (*cb)(struct evhttp_request *, void *));

void evhttp_request_free(struct evhttp_request *req);

struct evhttp_connection *evhttp_request_get_connection(struct evhttp_request *req);

struct evhttp_connection *evhttp_connection_new(
	const char *address, unsigned short port);

void evhttp_connection_free(struct evhttp_connection *evcon);

void evhttp_connection_set_local_address(struct evhttp_connection *evcon,
    const char *address);

void evhttp_connection_set_local_port(struct evhttp_connection *evcon,
    unsigned short port);

void evhttp_connection_set_timeout(struct evhttp_connection *evcon,
    int timeout_in_secs);

void evhttp_connection_set_retries(struct evhttp_connection *evcon,
    int retry_max);

void evhttp_connection_set_closecb(struct evhttp_connection *evcon,
    void (*)(struct evhttp_connection *, void *), void *);

void evhttp_connection_set_base(struct evhttp_connection *evcon,
    struct event_base *base);

void evhttp_connection_get_peer(struct evhttp_connection *evcon,
    char **address, u_short *port);

int evhttp_make_request(struct evhttp_connection *evcon,
    struct evhttp_request *req,
    enum evhttp_cmd_type type, const char *uri);

const char *evhttp_request_uri(struct evhttp_request *req);

const char *evhttp_find_header(const struct evkeyvalq *, const char *);
int evhttp_remove_header(struct evkeyvalq *, const char *);
int evhttp_add_header(struct evkeyvalq *, const char *, const char *);
void evhttp_clear_headers(struct evkeyvalq *);

char *evhttp_encode_uri(const char *uri);

char *evhttp_decode_uri(const char *uri);

void evhttp_parse_query(const char *uri, struct evkeyvalq *headers);

char *evhttp_htmlescape(const char *html);

#ifdef __cplusplus
}
#endif

#endif
