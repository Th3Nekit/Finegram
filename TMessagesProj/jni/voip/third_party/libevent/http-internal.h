/*
 * Copyright 2001 Niels Provos <provos@citi.umich.edu>
 * All rights reserved.
 *
 * This header file contains definitions for dealing with HTTP requests
 * that are internal to libevent.  As user of the library, you should not
 * need to know about these.
 */

#ifndef _HTTP_H_
#define _HTTP_H_

#define HTTP_CONNECT_TIMEOUT	45
#define HTTP_WRITE_TIMEOUT	50
#define HTTP_READ_TIMEOUT	50

#define HTTP_PREFIX		"http://"
#define HTTP_DEFAULTPORT	80

enum message_read_status {
	ALL_DATA_READ = 1,
	MORE_DATA_EXPECTED = 0,
	DATA_CORRUPTED = -1,
	REQUEST_CANCELED = -2
};

enum evhttp_connection_error {
	EVCON_HTTP_TIMEOUT,
	EVCON_HTTP_EOF,
	EVCON_HTTP_INVALID_HEADER
};

struct evbuffer;
struct evhttp_request;

enum evhttp_connection_state {
	EVCON_DISCONNECTED,
	EVCON_CONNECTING,
	EVCON_IDLE,
	EVCON_READING_FIRSTLINE,

	EVCON_READING_HEADERS,
	EVCON_READING_BODY,
	EVCON_READING_TRAILER,
	EVCON_WRITING
};

struct event_base;

struct evhttp_connection {

	TAILQ_ENTRY(evhttp_connection) (next);

	int fd;
	struct event ev;
	struct event close_ev;
	struct evbuffer *input_buffer;
	struct evbuffer *output_buffer;

	char *bind_address;
	u_short bind_port;

	char *address;
	u_short port;

	int flags;
#define EVHTTP_CON_INCOMING	0x0001
#define EVHTTP_CON_OUTGOING	0x0002
#define EVHTTP_CON_CLOSEDETECT  0x0004

	int timeout;
	int retry_cnt;
	int retry_max;

	enum evhttp_connection_state state;

	struct evhttp *http_server;

	TAILQ_HEAD(evcon_requestq, evhttp_request) requests;

						   void (*cb)(struct evhttp_connection *, void *);
	void *cb_arg;

	void (*closecb)(struct evhttp_connection *, void *);
	void *closecb_arg;

	struct event_base *base;
};

struct evhttp_cb {
	TAILQ_ENTRY(evhttp_cb) next;

	char *what;

	void (*cb)(struct evhttp_request *req, void *);
	void *cbarg;
};

TAILQ_HEAD(evconq, evhttp_connection);

struct evhttp_bound_socket {
	TAILQ_ENTRY(evhttp_bound_socket) (next);

	struct event  bind_ev;
};

struct evhttp {
	TAILQ_HEAD(boundq, evhttp_bound_socket) sockets;

	TAILQ_HEAD(httpcbq, evhttp_cb) callbacks;
        struct evconq connections;

        int timeout;

	void (*gencb)(struct evhttp_request *req, void *);
	void *gencbarg;

	struct event_base *base;
};

void evhttp_connection_reset(struct evhttp_connection *);

int evhttp_connection_connect(struct evhttp_connection *);

void evhttp_connection_fail(struct evhttp_connection *,
    enum evhttp_connection_error error);

void evhttp_get_request(struct evhttp *, int, struct sockaddr *, socklen_t);

int evhttp_hostportfile(char *, char **, u_short *, char **);

int evhttp_parse_firstline(struct evhttp_request *, struct evbuffer*);
int evhttp_parse_headers(struct evhttp_request *, struct evbuffer*);

void evhttp_start_read(struct evhttp_connection *);
void evhttp_make_header(struct evhttp_connection *, struct evhttp_request *);

void evhttp_write_buffer(struct evhttp_connection *,
    void (*)(struct evhttp_connection *, void *), void *);

void evhttp_response_code(struct evhttp_request *, int, const char *);
void evhttp_send_page(struct evhttp_request *, struct evbuffer *);

#endif
