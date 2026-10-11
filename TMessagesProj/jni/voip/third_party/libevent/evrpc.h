/*
 * Copyright (c) 2006 Niels Provos <provos@citi.umich.edu>
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
#ifndef _EVRPC_H_
#define _EVRPC_H_

#ifdef __cplusplus
extern "C" {
#endif

struct evbuffer;
struct event_base;
struct evrpc_req_generic;

struct evrpc {
	TAILQ_ENTRY(evrpc) next;

	const char* uri;

	void *(*request_new)(void);

	void (*request_free)(void *);

	int (*request_unmarshal)(void *, struct evbuffer *);

	void *(*reply_new)(void);

	void (*reply_free)(void *);

	int (*reply_complete)(void *);

	void (*reply_marshal)(struct evbuffer*, void *);

	void (*cb)(struct evrpc_req_generic *, void *);
	void *cb_arg;

	struct evrpc_base *base;
};

#define EVRPC_STRUCT(rpcname) struct evrpc_req__##rpcname

struct evhttp_request;
struct evrpc_status;

struct evrpc_req_generic {

	void *request;

	void *reply;

	struct evrpc *rpc;

	struct evhttp_request* http_req;

	void (*done)(struct evrpc_req_generic* rpc);
};

#define EVRPC_HEADER(rpcname, reqstruct, rplystruct) \
EVRPC_STRUCT(rpcname) {	\
	struct reqstruct* request; \
	struct rplystruct* reply; \
	struct evrpc* rpc; \
	struct evhttp_request* http_req; \
	void (*done)(struct evrpc_status *, \
	    struct evrpc* rpc, void *request, void *reply);	     \
};								     \
int evrpc_send_request_##rpcname(struct evrpc_pool *, \
    struct reqstruct *, struct rplystruct *, \
    void (*)(struct evrpc_status *, \
	struct reqstruct *, struct rplystruct *, void *cbarg),	\
    void *);

#define EVRPC_GENERATE(rpcname, reqstruct, rplystruct) \
int evrpc_send_request_##rpcname(struct evrpc_pool *pool, \
    struct reqstruct *request, struct rplystruct *reply, \
    void (*cb)(struct evrpc_status *, \
	struct reqstruct *, struct rplystruct *, void *cbarg),	\
    void *cbarg) { \
	struct evrpc_status status;				    \
	struct evrpc_request_wrapper *ctx;			    \
	ctx = (struct evrpc_request_wrapper *) \
	    malloc(sizeof(struct evrpc_request_wrapper));	    \
	if (ctx == NULL)					    \
		goto error;					    \
	ctx->pool = pool;					    \
	ctx->evcon = NULL;					    \
	ctx->name = strdup(#rpcname);				    \
	if (ctx->name == NULL) {				    \
		free(ctx);					    \
		goto error;					    \
	}							    \
	ctx->cb = (void (*)(struct evrpc_status *, \
		void *, void *, void *))cb;			    \
	ctx->cb_arg = cbarg;					    \
	ctx->request = (void *)request;				    \
	ctx->reply = (void *)reply;				    \
	ctx->request_marshal = (void (*)(struct evbuffer *, void *))reqstruct##_marshal; \
	ctx->reply_clear = (void (*)(void *))rplystruct##_clear;    \
	ctx->reply_unmarshal = (int (*)(void *, struct evbuffer *))rplystruct##_unmarshal; \
	return (evrpc_make_request(ctx));			    \
error:								    \
	memset(&status, 0, sizeof(status));			    \
	status.error = EVRPC_STATUS_ERR_UNSTARTED;		    \
	(*(cb))(&status, request, reply, cbarg);		    \
	return (-1);						    \
}

#define EVRPC_REQUEST_HTTP(rpc_req) (rpc_req)->http_req

#define EVRPC_REQUEST_DONE(rpc_req) do { \
  struct evrpc_req_generic *_req = (struct evrpc_req_generic *)(rpc_req); \
  _req->done(_req); \
} while (0)

#define EVRPC_REGISTER_OBJECT(rpc, name, request, reply) \
  do { \
    (rpc)->uri = strdup(#name); \
    if ((rpc)->uri == NULL) {			 \
      fprintf(stderr, "failed to register object\n");	\
      exit(1);						\
    } \
    (rpc)->request_new = (void *(*)(void))request##_new; \
    (rpc)->request_free = (void (*)(void *))request##_free; \
    (rpc)->request_unmarshal = (int (*)(void *, struct evbuffer *))request##_unmarshal; \
    (rpc)->reply_new = (void *(*)(void))reply##_new; \
    (rpc)->reply_free = (void (*)(void *))reply##_free; \
    (rpc)->reply_complete = (int (*)(void *))reply##_complete; \
    (rpc)->reply_marshal = (void (*)(struct evbuffer*, void *))reply##_marshal; \
  } while (0)

struct evrpc_base;
struct evhttp;

struct evrpc_base *evrpc_init(struct evhttp *server);

void evrpc_free(struct evrpc_base *base);

#define EVRPC_REGISTER(base, name, request, reply, callback, cbarg) \
  do { \
    struct evrpc* rpc = (struct evrpc *)calloc(1, sizeof(struct evrpc)); \
    EVRPC_REGISTER_OBJECT(rpc, name, request, reply); \
    evrpc_register_rpc(base, rpc, \
	(void (*)(struct evrpc_req_generic*, void *))callback, cbarg);	\
  } while (0)

int evrpc_register_rpc(struct evrpc_base *, struct evrpc *,
    void (*)(struct evrpc_req_generic*, void *), void *);

#define EVRPC_UNREGISTER(base, name) evrpc_unregister_rpc(base, #name)

int evrpc_unregister_rpc(struct evrpc_base *base, const char *name);

struct evrpc_pool;
struct evhttp_connection;

struct evrpc_status {
#define EVRPC_STATUS_ERR_NONE		0
#define EVRPC_STATUS_ERR_TIMEOUT	1
#define EVRPC_STATUS_ERR_BADPAYLOAD	2
#define EVRPC_STATUS_ERR_UNSTARTED	3
#define EVRPC_STATUS_ERR_HOOKABORTED	4
	int error;

	struct evhttp_request *http_req;
};

struct evrpc_request_wrapper {
	TAILQ_ENTRY(evrpc_request_wrapper) next;

        struct evrpc_pool *pool;

	struct evhttp_connection *evcon;

	struct event ev_timeout;

	char *name;

	void (*cb)(struct evrpc_status*, void *request, void *reply, void *arg);
	void *cb_arg;

	void *request;
	void *reply;

	void (*request_marshal)(struct evbuffer *, void *);

	void (*reply_clear)(void *);

	int (*reply_unmarshal)(void *, struct evbuffer*);
};

#define EVRPC_MAKE_REQUEST(name, pool, request, reply, cb, cbarg)	\
	evrpc_send_request_##name(pool, request, reply, cb, cbarg)

int evrpc_make_request(struct evrpc_request_wrapper *);

struct evrpc_pool *evrpc_pool_new(struct event_base *base);

void evrpc_pool_free(struct evrpc_pool *pool);

void evrpc_pool_add_connection(struct evrpc_pool *,
    struct evhttp_connection *);

void evrpc_pool_set_timeout(struct evrpc_pool *pool, int timeout_in_secs);

enum EVRPC_HOOK_TYPE {
	EVRPC_INPUT,
	EVRPC_OUTPUT
};

#ifndef WIN32

#define INPUT EVRPC_INPUT

#define OUTPUT EVRPC_OUTPUT
#endif

void *evrpc_add_hook(void *vbase,
    enum EVRPC_HOOK_TYPE hook_type,
    int (*cb)(struct evhttp_request *, struct evbuffer *, void *),
    void *cb_arg);

int evrpc_remove_hook(void *vbase,
    enum EVRPC_HOOK_TYPE hook_type,
    void *handle);

#ifdef __cplusplus
}
#endif

#endif
