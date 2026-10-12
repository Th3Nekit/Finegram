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

/*
 * The original DNS code is due to Adam Langley with heavy
 * modifications by Nick Mathewson.  Adam put his DNS software in the
 * public domain.  You can find his original copyright below.  Please,
 * aware that the code as part of libevent is governed by the 3-clause
 * BSD license above.
 *
 * This software is Public Domain. To view a copy of the public domain dedication,
 * visit http://creativecommons.org/licenses/publicdomain/ or send a letter to
 * Creative Commons, 559 Nathan Abbott Way, Stanford, California 94305, USA.
 *
 * I ask and expect, but do not require, that all derivative works contain an
 * attribution similar to:
 * 	Parts developed by Adam Langley <agl@imperialviolet.org>
 *
 * You may wish to replace the word "Parts" with something else depending on
 * the amount of original code.
 *
 * (Derivative works does not include programs which link against, run or include
 * the source verbatim in their source distributions)
 */

#ifndef EVENTDNS_H
#define EVENTDNS_H

#ifdef __cplusplus
extern "C" {
#endif

#include "evutil.h"

#define DNS_ERR_NONE 0

#define DNS_ERR_FORMAT 1

#define DNS_ERR_SERVERFAILED 2

#define DNS_ERR_NOTEXIST 3

#define DNS_ERR_NOTIMPL 4

#define DNS_ERR_REFUSED 5

#define DNS_ERR_TRUNCATED 65

#define DNS_ERR_UNKNOWN 66

#define DNS_ERR_TIMEOUT 67

#define DNS_ERR_SHUTDOWN 68

#define DNS_IPv4_A 1
#define DNS_PTR 2
#define DNS_IPv6_AAAA 3

#define DNS_QUERY_NO_SEARCH 1

#define DNS_OPTION_SEARCH 1
#define DNS_OPTION_NAMESERVERS 2
#define DNS_OPTION_MISC 4
#define DNS_OPTIONS_ALL 7

typedef void (*evdns_callback_type) (int result, char type, int count, int ttl, void *addresses, void *arg);

int evdns_init(void);

void evdns_shutdown(int fail_requests);

const char *evdns_err_to_string(int err);

int evdns_nameserver_add(unsigned long int address);

int evdns_count_nameservers(void);

int evdns_clear_nameservers_and_suspend(void);

int evdns_resume(void);

int evdns_nameserver_ip_add(const char *ip_as_string);

int evdns_resolve_ipv4(const char *name, int flags, evdns_callback_type callback, void *ptr);

int evdns_resolve_ipv6(const char *name, int flags, evdns_callback_type callback, void *ptr);

struct in_addr;
struct in6_addr;

int evdns_resolve_reverse(const struct in_addr *in, int flags, evdns_callback_type callback, void *ptr);

int evdns_resolve_reverse_ipv6(const struct in6_addr *in, int flags, evdns_callback_type callback, void *ptr);

int evdns_set_option(const char *option, const char *val, int flags);

int evdns_resolv_conf_parse(int flags, const char *const filename);

#ifdef WIN32
int evdns_config_windows_nameservers(void);
#endif

void evdns_search_clear(void);

void evdns_search_add(const char *domain);

void evdns_search_ndots_set(const int ndots);

typedef void (*evdns_debug_log_fn_type)(int is_warning, const char *msg);

void evdns_set_log_fn(evdns_debug_log_fn_type fn);

void evdns_set_transaction_id_fn(ev_uint16_t (*fn)(void));

#define DNS_NO_SEARCH 1

struct evdns_server_request {
	int flags;
	int nquestions;
	struct evdns_server_question **questions;
};
struct evdns_server_question {
	int type;
#ifdef __cplusplus
	int dns_question_class;
#else

	int class;
#define dns_question_class class
#endif
	char name[1];
};
typedef void (*evdns_request_callback_fn_type)(struct evdns_server_request *, void *);
#define EVDNS_ANSWER_SECTION 0
#define EVDNS_AUTHORITY_SECTION 1
#define EVDNS_ADDITIONAL_SECTION 2

#define EVDNS_TYPE_A	   1
#define EVDNS_TYPE_NS	   2
#define EVDNS_TYPE_CNAME   5
#define EVDNS_TYPE_SOA	   6
#define EVDNS_TYPE_PTR	  12
#define EVDNS_TYPE_MX	  15
#define EVDNS_TYPE_TXT	  16
#define EVDNS_TYPE_AAAA	  28

#define EVDNS_QTYPE_AXFR 252
#define EVDNS_QTYPE_ALL	 255

#define EVDNS_CLASS_INET   1

struct evdns_server_port *evdns_add_server_port(int socket, int is_tcp, evdns_request_callback_fn_type callback, void *user_data);
void evdns_close_server_port(struct evdns_server_port *port);

int evdns_server_request_add_reply(struct evdns_server_request *req, int section, const char *name, int type, int dns_class, int ttl, int datalen, int is_name, const char *data);
int evdns_server_request_add_a_reply(struct evdns_server_request *req, const char *name, int n, void *addrs, int ttl);
int evdns_server_request_add_aaaa_reply(struct evdns_server_request *req, const char *name, int n, void *addrs, int ttl);
int evdns_server_request_add_ptr_reply(struct evdns_server_request *req, struct in_addr *in, const char *inaddr_name, const char *hostname, int ttl);
int evdns_server_request_add_cname_reply(struct evdns_server_request *req, const char *name, const char *cname, int ttl);

int evdns_server_request_respond(struct evdns_server_request *req, int err);
int evdns_server_request_drop(struct evdns_server_request *req);
struct sockaddr;
int evdns_server_request_get_requesting_addr(struct evdns_server_request *_req, struct sockaddr *sa, int addr_len);

#ifdef __cplusplus
}
#endif

#endif
