
#include <sys/types.h>

#include "config.h"

#ifdef DNS_USE_FTIME_FOR_ID
#include <sys/timeb.h>
#endif

#ifndef DNS_USE_CPU_CLOCK_FOR_ID
#ifndef DNS_USE_GETTIMEOFDAY_FOR_ID
#ifndef DNS_USE_OPENSSL_FOR_ID
#ifndef DNS_USE_FTIME_FOR_ID
#error Must configure at least one id generation method.
#error Please see the documentation.
#endif
#endif
#endif
#endif

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#ifdef DNS_USE_CPU_CLOCK_FOR_ID
#ifdef DNS_USE_OPENSSL_FOR_ID
#error Multiple id options selected
#endif
#ifdef DNS_USE_GETTIMEOFDAY_FOR_ID
#error Multiple id options selected
#endif
#include <time.h>
#endif

#ifdef DNS_USE_OPENSSL_FOR_ID
#ifdef DNS_USE_GETTIMEOFDAY_FOR_ID
#error Multiple id options selected
#endif
#include <openssl/rand.h>
#endif

#ifndef _FORTIFY_SOURCE
#define _FORTIFY_SOURCE 3
#endif

#include <string.h>
#include <fcntl.h>
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <limits.h>
#include <sys/stat.h>
#include <ctype.h>
#include <stdio.h>
#include <stdarg.h>

#include "evdns.h"
#include "evutil.h"
#include "log.h"
#ifdef WIN32
#include <winsock2.h>
#include <windows.h>
#include <iphlpapi.h>
#include <io.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif

#ifdef HAVE_NETINET_IN6_H
#include <netinet/in6.h>
#endif

#define EVDNS_LOG_DEBUG 0
#define EVDNS_LOG_WARN 1

#ifndef HOST_NAME_MAX
#define HOST_NAME_MAX 255
#endif

#include <stdio.h>

#undef MIN
#define MIN(a,b) ((a)<(b)?(a):(b))

#ifdef __USE_ISOC99B

typedef ev_uint8_t u_char;
typedef unsigned int uint;
#endif
#include "event.h"

#define u64 ev_uint64_t
#define u32 ev_uint32_t
#define u16 ev_uint16_t
#define u8  ev_uint8_t

#ifdef WIN32
#define open _open
#define read _read
#define close _close
#define strdup _strdup
#endif

#define MAX_ADDRS 32

#define TYPE_A         EVDNS_TYPE_A
#define TYPE_CNAME     5
#define TYPE_PTR       EVDNS_TYPE_PTR
#define TYPE_AAAA      EVDNS_TYPE_AAAA

#define CLASS_INET     EVDNS_CLASS_INET

#ifdef HAVE_SETFD
#define FD_CLOSEONEXEC(x) do { \
	if (fcntl(x, F_SETFD, 1) == -1) \
		event_warn("fcntl(%d, F_SETFD)", x); \
	} while (0)
#else
#define FD_CLOSEONEXEC(x) (void)0
#endif

struct request {
	u8 *request;
	unsigned int request_len;
	int reissue_count;
	int tx_count;
	unsigned int request_type;
	void *user_pointer;
	evdns_callback_type user_callback;
	struct nameserver *ns;

	int search_index;
	struct search_state *search_state;
	char *search_origname;
	int search_flags;

	struct request *next, *prev;

	struct event timeout_event;

	u16 trans_id;
	char request_appended;
	char transmit_me;
};

#ifndef HAVE_STRUCT_IN6_ADDR
struct in6_addr {
	u8 s6_addr[16];
};
#endif

struct reply {
	unsigned int type;
	unsigned int have_answer;
	union {
		struct {
			u32 addrcount;
			u32 addresses[MAX_ADDRS];
		} a;
		struct {
			u32 addrcount;
			struct in6_addr addresses[MAX_ADDRS];
		} aaaa;
		struct {
			char name[HOST_NAME_MAX];
		} ptr;
	} data;
};

struct nameserver {
	int socket;
	u32 address;
	u16 port;
	int failed_times;
	int timedout;
	struct event event;

	struct nameserver *next, *prev;
	struct event timeout_event;

	char state;
	char choked;
	char write_waiting;
};

static struct request *req_head = NULL, *req_waiting_head = NULL;
static struct nameserver *server_head = NULL;

struct evdns_server_port {
	int socket;
	int refcnt;
	char choked;
	char closing;
	evdns_request_callback_fn_type user_callback;
	void *user_data;
	struct event event;

	struct server_request *pending_replies;
};

struct server_reply_item {
	struct server_reply_item *next;
	char *name;
	u16 type : 16;
	u16 class : 16;
	u32 ttl;
	char is_name;
	u16 datalen;
	void *data;
};

struct server_request {

	struct server_request *next_pending;
	struct server_request *prev_pending;

	u16 trans_id;
	struct evdns_server_port *port;
	struct sockaddr_storage addr;
	socklen_t addrlen;

	int n_answer;
	int n_authority;
	int n_additional;

	struct server_reply_item *answer;
	struct server_reply_item *authority;
	struct server_reply_item *additional;

	char *response;
	size_t response_len;

	struct evdns_server_request base;
};

#define OFFSET_OF(st, member) ((off_t) (((char*)&((st*)0)->member)-(char*)0))

#define TO_SERVER_REQUEST(base_ptr)										\
	((struct server_request*)											\
	 (((char*)(base_ptr) - OFFSET_OF(struct server_request, base))))

static int global_good_nameservers = 0;

static int global_requests_inflight = 0;

static int global_requests_waiting = 0;

static int global_max_requests_inflight = 64;

static struct timeval global_timeout = {5, 0};
static int global_max_reissues = 1;
static int global_max_retransmits = 3;

static int global_max_nameserver_timeout = 3;

static const struct timeval global_nameserver_timeouts[] = {{10, 0}, {60, 0}, {300, 0}, {900, 0}, {3600, 0}};
static const int global_nameserver_timeouts_length = sizeof(global_nameserver_timeouts)/sizeof(struct timeval);

static struct nameserver *nameserver_pick(void);
static void evdns_request_insert(struct request *req, struct request **head);
static void nameserver_ready_callback(int fd, short events, void *arg);
static int evdns_transmit(void);
static int evdns_request_transmit(struct request *req);
static void nameserver_send_probe(struct nameserver *const ns);
static void search_request_finished(struct request *const);
static int search_try_next(struct request *const req);
static int search_request_new(int type, const char *const name, int flags, evdns_callback_type user_callback, void *user_arg);
static void evdns_requests_pump_waiting_queue(void);
static u16 transaction_id_pick(void);
static struct request *request_new(int type, const char *name, int flags, evdns_callback_type callback, void *ptr);
static void request_submit(struct request *const req);

static int server_request_free(struct server_request *req);
static void server_request_free_answers(struct server_request *req);
static void server_port_free(struct evdns_server_port *port);
static void server_port_ready_callback(int fd, short events, void *arg);

static int strtoint(const char *const str);

#ifdef WIN32
static int
last_error(int sock)
{
	int optval, optvallen=sizeof(optval);
	int err = WSAGetLastError();
	if (err == WSAEWOULDBLOCK && sock >= 0) {
		if (getsockopt(sock, SOL_SOCKET, SO_ERROR, (void*)&optval,
			       &optvallen))
			return err;
		if (optval)
			return optval;
	}
	return err;

}
static int
error_is_eagain(int err)
{
	return err == EAGAIN || err == WSAEWOULDBLOCK;
}
static int
inet_aton(const char *c, struct in_addr *addr)
{
	ev_uint32_t r;
	if (strcmp(c, "255.255.255.255") == 0) {
		addr->s_addr = 0xffffffffu;
	} else {
		r = inet_addr(c);
		if (r == INADDR_NONE)
			return 0;
		addr->s_addr = r;
	}
	return 1;
}
#else
#define last_error(sock) (errno)
#define error_is_eagain(err) ((err) == EAGAIN)
#endif
#define CLOSE_SOCKET(s) EVUTIL_CLOSESOCKET(s)

#define ISSPACE(c) isspace((int)(unsigned char)(c))
#define ISDIGIT(c) isdigit((int)(unsigned char)(c))

static const char *
debug_ntoa(u32 address)
{
	static char buf[32];
	u32 a = ntohl(address);
	evutil_snprintf(buf, sizeof(buf), "%d.%d.%d.%d",
                      (int)(u8)((a>>24)&0xff),
                      (int)(u8)((a>>16)&0xff),
                      (int)(u8)((a>>8 )&0xff),
  		      (int)(u8)((a    )&0xff));
	return buf;
}

static evdns_debug_log_fn_type evdns_log_fn = NULL;

void
evdns_set_log_fn(evdns_debug_log_fn_type fn)
{
  evdns_log_fn = fn;
}

#ifdef __GNUC__
#define EVDNS_LOG_CHECK  __attribute__ ((format(printf, 2, 3)))
#else
#define EVDNS_LOG_CHECK
#endif

static void _evdns_log(int warn, const char *fmt, ...) EVDNS_LOG_CHECK;
static void
_evdns_log(int warn, const char *fmt, ...)
{
  va_list args;
  static char buf[512];
  if (!evdns_log_fn)
    return;
  va_start(args,fmt);
  evutil_vsnprintf(buf, sizeof(buf), fmt, args);
  buf[sizeof(buf)-1] = '\0';
  evdns_log_fn(warn, buf);
  va_end(args);
}

#define log _evdns_log

static struct request *
request_find_from_trans_id(u16 trans_id) {
	struct request *req = req_head, *const started_at = req_head;

	if (req) {
		do {
			if (req->trans_id == trans_id) return req;
			req = req->next;
		} while (req != started_at);
	}

	return NULL;
}

static void
nameserver_prod_callback(int fd, short events, void *arg) {
	struct nameserver *const ns = (struct nameserver *) arg;
        (void)fd;
        (void)events;

	nameserver_send_probe(ns);
}

static void
nameserver_probe_failed(struct nameserver *const ns) {
	const struct timeval * timeout;
	(void) evtimer_del(&ns->timeout_event);
	if (ns->state == 1) {

		return;
	}

	timeout =
	  &global_nameserver_timeouts[MIN(ns->failed_times,
					  global_nameserver_timeouts_length - 1)];
	ns->failed_times++;

	if (evtimer_add(&ns->timeout_event, (struct timeval *) timeout) < 0) {
          log(EVDNS_LOG_WARN,
              "Error from libevent when adding timer event for %s",
              debug_ntoa(ns->address));

        }
}

static void
nameserver_failed(struct nameserver *const ns, const char *msg) {
	struct request *req, *started_at;

	if (!ns->state) return;

	log(EVDNS_LOG_WARN, "Nameserver %s has failed: %s",
            debug_ntoa(ns->address), msg);
	global_good_nameservers--;
	assert(global_good_nameservers >= 0);
	if (global_good_nameservers == 0) {
		log(EVDNS_LOG_WARN, "All nameservers have failed");
	}

	ns->state = 0;
	ns->failed_times = 1;

	if (evtimer_add(&ns->timeout_event, (struct timeval *) &global_nameserver_timeouts[0]) < 0) {
		log(EVDNS_LOG_WARN,
		    "Error from libevent when adding timer event for %s",
		    debug_ntoa(ns->address));

        }

	if (!global_good_nameservers) return;

	req = req_head;
	started_at = req_head;
	if (req) {
		do {
			if (req->tx_count == 0 && req->ns == ns) {

				req->ns = nameserver_pick();
			}
			req = req->next;
		} while (req != started_at);
	}
}

static void
nameserver_up(struct nameserver *const ns) {
	if (ns->state) return;
	log(EVDNS_LOG_WARN, "Nameserver %s is back up",
	    debug_ntoa(ns->address));
	evtimer_del(&ns->timeout_event);
	ns->state = 1;
	ns->failed_times = 0;
	ns->timedout = 0;
	global_good_nameservers++;
}

static void
request_trans_id_set(struct request *const req, const u16 trans_id) {
	req->trans_id = trans_id;
	*((u16 *) req->request) = htons(trans_id);
}

static void
request_finished(struct request *const req, struct request **head) {
	if (head) {
		if (req->next == req) {

			*head = NULL;
		} else {
			req->next->prev = req->prev;
			req->prev->next = req->next;
			if (*head == req) *head = req->next;
		}
	}

	log(EVDNS_LOG_DEBUG, "Removing timeout for request %lx",
	    (unsigned long) req);
	evtimer_del(&req->timeout_event);

	search_request_finished(req);
	global_requests_inflight--;

	if (!req->request_appended) {

		free(req->request);
	} else {

	}

	free(req);

	evdns_requests_pump_waiting_queue();
}

static int
request_reissue(struct request *req) {
	const struct nameserver *const last_ns = req->ns;

	req->ns = nameserver_pick();
	if (req->ns == last_ns) {

		return 1;
	}

	req->reissue_count++;
	req->tx_count = 0;
	req->transmit_me = 1;

	return 0;
}

static void
evdns_requests_pump_waiting_queue(void) {
	while (global_requests_inflight < global_max_requests_inflight &&
	    global_requests_waiting) {
		struct request *req;

		assert(req_waiting_head);
		if (req_waiting_head->next == req_waiting_head) {

			req = req_waiting_head;
			req_waiting_head = NULL;
		} else {
			req = req_waiting_head;
			req->next->prev = req->prev;
			req->prev->next = req->next;
			req_waiting_head = req->next;
		}

		global_requests_waiting--;
		global_requests_inflight++;

		req->ns = nameserver_pick();
		request_trans_id_set(req, transaction_id_pick());

		evdns_request_insert(req, &req_head);
		evdns_request_transmit(req);
		evdns_transmit();
	}
}

static void
reply_callback(struct request *const req, u32 ttl, u32 err, struct reply *reply) {
	switch (req->request_type) {
	case TYPE_A:
		if (reply)
			req->user_callback(DNS_ERR_NONE, DNS_IPv4_A,
							   reply->data.a.addrcount, ttl,
						 reply->data.a.addresses,
							   req->user_pointer);
		else
			req->user_callback(err, 0, 0, 0, NULL, req->user_pointer);
		return;
	case TYPE_PTR:
		if (reply) {
			char *name = reply->data.ptr.name;
			req->user_callback(DNS_ERR_NONE, DNS_PTR, 1, ttl,
							   &name, req->user_pointer);
		} else {
			req->user_callback(err, 0, 0, 0, NULL,
							   req->user_pointer);
		}
		return;
	case TYPE_AAAA:
		if (reply)
			req->user_callback(DNS_ERR_NONE, DNS_IPv6_AAAA,
							   reply->data.aaaa.addrcount, ttl,
							   reply->data.aaaa.addresses,
							   req->user_pointer);
		else
			req->user_callback(err, 0, 0, 0, NULL, req->user_pointer);
                return;
	}
	assert(0);
}

static void
reply_handle(struct request *const req, u16 flags, u32 ttl, struct reply *reply) {
	int error;
	static const int error_codes[] = {
		DNS_ERR_FORMAT, DNS_ERR_SERVERFAILED, DNS_ERR_NOTEXIST,
		DNS_ERR_NOTIMPL, DNS_ERR_REFUSED
	};

	if (flags & 0x020f || !reply || !reply->have_answer) {

		if (flags & 0x0200) {
			error = DNS_ERR_TRUNCATED;
		} else {
			u16 error_code = (flags & 0x000f) - 1;
			if (error_code > 4) {
				error = DNS_ERR_UNKNOWN;
			} else {
				error = error_codes[error_code];
			}
		}

		switch(error) {
		case DNS_ERR_NOTIMPL:
		case DNS_ERR_REFUSED:

			if (req->reissue_count < global_max_reissues) {
				char msg[64];
				evutil_snprintf(msg, sizeof(msg),
				    "Bad response %d (%s)",
					 error, evdns_err_to_string(error));
				nameserver_failed(req->ns, msg);
				if (!request_reissue(req)) return;
			}
			break;
		case DNS_ERR_SERVERFAILED:

			log(EVDNS_LOG_DEBUG, "Got a SERVERFAILED from nameserver %s; "
				"will allow the request to time out.",
				debug_ntoa(req->ns->address));
			break;
		default:

			nameserver_up(req->ns);
		}

		if (req->search_state && req->request_type != TYPE_PTR) {

			if (!search_try_next(req)) {

				request_finished(req, &req_head);
				return;
			}
		}

		reply_callback(req, 0, error, NULL);
		request_finished(req, &req_head);
	} else {

		reply_callback(req, ttl, 0, reply);
		nameserver_up(req->ns);
		request_finished(req, &req_head);
	}
}

static int
name_parse(u8 *packet, int length, int *idx, char *name_out, int name_out_len) {
	int name_end = -1;
	int j = *idx;
	int ptr_count = 0;
#define GET32(x) do { if (j + 4 > length) goto err; memcpy(&_t32, packet + j, 4); j += 4; x = ntohl(_t32); } while(0)
#define GET16(x) do { if (j + 2 > length) goto err; memcpy(&_t, packet + j, 2); j += 2; x = ntohs(_t); } while(0)
#define GET8(x) do { if (j >= length) goto err; x = packet[j++]; } while(0)

	char *cp = name_out;
	const char *const end = name_out + name_out_len;

	for(;;) {
		u8 label_len;
		if (j >= length) return -1;
		GET8(label_len);
		if (!label_len) break;
		if (label_len & 0xc0) {
			u8 ptr_low;
			GET8(ptr_low);
			if (name_end < 0) name_end = j;
			j = (((int)label_len & 0x3f) << 8) + ptr_low;

			if (j < 0 || j >= length) return -1;

			if (++ptr_count > length) return -1;
			continue;
		}
		if (label_len > 63) return -1;
		if (cp != name_out) {
			if (cp + 1 >= end) return -1;
			*cp++ = '.';
		}
		if (cp + label_len >= end) return -1;
		memcpy(cp, packet + j, label_len);
		cp += label_len;
		j += label_len;
	}
	if (cp >= end) return -1;
	*cp = '\0';
	if (name_end < 0)
		*idx = j;
	else
		*idx = name_end;
	return 0;
 err:
	return -1;
}

static int
reply_parse(u8 *packet, int length) {
	int j = 0, k = 0;
	u16 _t;
	u32 _t32;
	char tmp_name[256], cmp_name[256];

	u16 trans_id, questions, answers, authority, additional, datalength;
        u16 flags = 0;
	u32 ttl, ttl_r = 0xffffffff;
	struct reply reply;
	struct request *req = NULL;
	unsigned int i;

	GET16(trans_id);
	GET16(flags);
	GET16(questions);
	GET16(answers);
	GET16(authority);
	GET16(additional);
	(void) authority;
	(void) additional;

	req = request_find_from_trans_id(trans_id);
	if (!req) return -1;

	memset(&reply, 0, sizeof(reply));

	if (!(flags & 0x8000)) return -1;
	if (flags & 0x020f) {

		goto err;
	}

#define SKIP_NAME \
	do { tmp_name[0] = '\0';				\
		if (name_parse(packet, length, &j, tmp_name, sizeof(tmp_name))<0)\
			goto err;				\
	} while(0)
#define TEST_NAME \
	do { tmp_name[0] = '\0';				\
		cmp_name[0] = '\0';				\
		k = j;						\
		if (name_parse(packet, length, &j, tmp_name, sizeof(tmp_name))<0)\
			goto err;					\
		if (name_parse(req->request, req->request_len, &k, cmp_name, sizeof(cmp_name))<0)	\
			goto err;				\
		if (memcmp(tmp_name, cmp_name, strlen (tmp_name)) != 0)	\
			return (-1);                                  	\
	} while(0)

	reply.type = req->request_type;

	for (i = 0; i < questions; ++i) {

		TEST_NAME;
		j += 4;
		if (j > length) goto err;
	}

	for (i = 0; i < answers; ++i) {
		u16 type, class;

		SKIP_NAME;
		GET16(type);
		GET16(class);
		GET32(ttl);
		GET16(datalength);

		if (type == TYPE_A && class == CLASS_INET) {
			int addrcount, addrtocopy;
			if (req->request_type != TYPE_A) {
				j += datalength; continue;
			}
			if ((datalength & 3) != 0)
			    goto err;
			addrcount = datalength >> 2;
			addrtocopy = MIN(MAX_ADDRS - reply.data.a.addrcount, (unsigned)addrcount);

			ttl_r = MIN(ttl_r, ttl);

			if (j + 4*addrtocopy > length) goto err;
			memcpy(&reply.data.a.addresses[reply.data.a.addrcount],
				   packet + j, 4*addrtocopy);
			j += 4*addrtocopy;
			reply.data.a.addrcount += addrtocopy;
			reply.have_answer = 1;
			if (reply.data.a.addrcount == MAX_ADDRS) break;
		} else if (type == TYPE_PTR && class == CLASS_INET) {
			if (req->request_type != TYPE_PTR) {
				j += datalength; continue;
			}
			if (name_parse(packet, length, &j, reply.data.ptr.name,
						   sizeof(reply.data.ptr.name))<0)
				goto err;
			ttl_r = MIN(ttl_r, ttl);
			reply.have_answer = 1;
			break;
		} else if (type == TYPE_AAAA && class == CLASS_INET) {
			int addrcount, addrtocopy;
			if (req->request_type != TYPE_AAAA) {
				j += datalength; continue;
			}
			if ((datalength & 15) != 0)
				goto err;
			addrcount = datalength >> 4;
			addrtocopy = MIN(MAX_ADDRS - reply.data.aaaa.addrcount, (unsigned)addrcount);
			ttl_r = MIN(ttl_r, ttl);

			if (j + 16*addrtocopy > length) goto err;
			memcpy(&reply.data.aaaa.addresses[reply.data.aaaa.addrcount],
				   packet + j, 16*addrtocopy);
			reply.data.aaaa.addrcount += addrtocopy;
			j += 16*addrtocopy;
			reply.have_answer = 1;
			if (reply.data.aaaa.addrcount == MAX_ADDRS) break;
		} else {

			j += datalength;
		}
	}

	reply_handle(req, flags, ttl_r, &reply);
	return 0;
 err:
	if (req)
		reply_handle(req, flags, 0, NULL);
	return -1;
}

static int
request_parse(u8 *packet, int length, struct evdns_server_port *port, struct sockaddr *addr, socklen_t addrlen)
{
	int j = 0;
	u16 _t;
	char tmp_name[256];

	int i;
	u16 trans_id, flags, questions, answers, authority, additional;
	struct server_request *server_req = NULL;

	GET16(trans_id);
	GET16(flags);
	GET16(questions);
	GET16(answers);
	GET16(authority);
	GET16(additional);

	if (flags & 0x8000) return -1;
	flags &= 0x0110;

	server_req = malloc(sizeof(struct server_request));
	if (server_req == NULL) return -1;
	memset(server_req, 0, sizeof(struct server_request));

	server_req->trans_id = trans_id;
	memcpy(&server_req->addr, addr, addrlen);
	server_req->addrlen = addrlen;

	server_req->base.flags = flags;
	server_req->base.nquestions = 0;
	server_req->base.questions = malloc(sizeof(struct evdns_server_question *) * questions);
	if (server_req->base.questions == NULL)
		goto err;

	for (i = 0; i < questions; ++i) {
		u16 type, class;
		struct evdns_server_question *q;
		int namelen;
		if (name_parse(packet, length, &j, tmp_name, sizeof(tmp_name))<0)
			goto err;
		GET16(type);
		GET16(class);
		namelen = strlen(tmp_name);
		q = malloc(sizeof(struct evdns_server_question) + namelen);
		if (!q)
			goto err;
		q->type = type;
		q->dns_question_class = class;
		memcpy(q->name, tmp_name, namelen+1);
		server_req->base.questions[server_req->base.nquestions++] = q;
	}

	server_req->port = port;
	port->refcnt++;

	if (flags & 0x7800) {
		evdns_server_request_respond(&(server_req->base), DNS_ERR_NOTIMPL);
		return -1;
	}

	port->user_callback(&(server_req->base), port->user_data);

	return 0;
err:
	if (server_req) {
		if (server_req->base.questions) {
			for (i = 0; i < server_req->base.nquestions; ++i)
				free(server_req->base.questions[i]);
			free(server_req->base.questions);
		}
		free(server_req);
	}
	return -1;

#undef SKIP_NAME
#undef GET32
#undef GET16
#undef GET8
}

static u16
default_transaction_id_fn(void)
{
	u16 trans_id;
#ifdef DNS_USE_CPU_CLOCK_FOR_ID
	struct timespec ts;
	static int clkid = -1;
	if (clkid == -1) {
		clkid = CLOCK_REALTIME;
#ifdef CLOCK_MONOTONIC
		if (clock_gettime(CLOCK_MONOTONIC, &ts) != -1)
			clkid = CLOCK_MONOTONIC;
#endif
	}
	if (clock_gettime(clkid, &ts) == -1)
		event_err(1, "clock_gettime");
	trans_id = ts.tv_nsec & 0xffff;
#endif

#ifdef DNS_USE_FTIME_FOR_ID
	struct _timeb tb;
	_ftime(&tb);
	trans_id = tb.millitm & 0xffff;
#endif

#ifdef DNS_USE_GETTIMEOFDAY_FOR_ID
	struct timeval tv;
	evutil_gettimeofday(&tv, NULL);
	trans_id = tv.tv_usec & 0xffff;
#endif

#ifdef DNS_USE_OPENSSL_FOR_ID
	if (RAND_pseudo_bytes((u8 *) &trans_id, 2) == -1) {

		abort();
	}
#endif
	return trans_id;
}

static ev_uint16_t (*trans_id_function)(void) = default_transaction_id_fn;

void
evdns_set_transaction_id_fn(ev_uint16_t (*fn)(void))
{
	if (fn)
		trans_id_function = fn;
	else
		trans_id_function = default_transaction_id_fn;
}

static u16
transaction_id_pick(void) {
	for (;;) {
		u16 trans_id = trans_id_function();

		if (trans_id == 0xffff) continue;

		if (request_find_from_trans_id(trans_id) == NULL)
			return trans_id;
	}
}

static struct nameserver *
nameserver_pick(void) {
	struct nameserver *started_at = server_head, *picked;
	if (!server_head) return NULL;

	if (!global_good_nameservers) {
		server_head = server_head->next;
		return server_head;
	}

	for (;;) {
		if (server_head->state) {

			picked = server_head;
			server_head = server_head->next;
			return picked;
		}

		server_head = server_head->next;
		if (server_head == started_at) {

			assert(global_good_nameservers == 0);
			picked = server_head;
			server_head = server_head->next;
			return picked;
		}
	}
}

static int
address_is_correct(struct nameserver *ns, struct sockaddr *sa, socklen_t slen)
{
	struct sockaddr_in *sin = (struct sockaddr_in*) sa;
	if (sa->sa_family != AF_INET || slen != sizeof(struct sockaddr_in))
		return 0;
	if (sin->sin_addr.s_addr != ns->address)
		return 0;
	return 1;
}

static void
nameserver_read(struct nameserver *ns) {
	u8 packet[1500];
	struct sockaddr_storage ss;
	socklen_t addrlen = sizeof(ss);

	for (;;) {
          	const int r = recvfrom(ns->socket, packet, sizeof(packet), 0,
		    (struct sockaddr*)&ss, &addrlen);
		if (r < 0) {
			int err = last_error(ns->socket);
			if (error_is_eagain(err)) return;
			nameserver_failed(ns, strerror(err));
			return;
		}
		if (!address_is_correct(ns, (struct sockaddr*)&ss, addrlen)) {
			log(EVDNS_LOG_WARN, "Address mismatch on received "
			    "DNS packet.");
			return;
		}
		ns->timedout = 0;
		reply_parse(packet, r);
	}
}

static void
server_port_read(struct evdns_server_port *s) {
	u8 packet[1500];
	struct sockaddr_storage addr;
	socklen_t addrlen;
	int r;

	for (;;) {
		addrlen = sizeof(struct sockaddr_storage);
		r = recvfrom(s->socket, packet, sizeof(packet), 0,
					 (struct sockaddr*) &addr, &addrlen);
		if (r < 0) {
			int err = last_error(s->socket);
			if (error_is_eagain(err)) return;
			log(EVDNS_LOG_WARN, "Error %s (%d) while reading request.",
				strerror(err), err);
			return;
		}
		request_parse(packet, r, s, (struct sockaddr*) &addr, addrlen);
	}
}

static void
server_port_flush(struct evdns_server_port *port)
{
	while (port->pending_replies) {
		struct server_request *req = port->pending_replies;
		int r = sendto(port->socket, req->response, req->response_len, 0,
			   (struct sockaddr*) &req->addr, req->addrlen);
		if (r < 0) {
			int err = last_error(port->socket);
			if (error_is_eagain(err))
				return;
			log(EVDNS_LOG_WARN, "Error %s (%d) while writing response to port; dropping", strerror(err), err);
		}
		if (server_request_free(req)) {

			return;
		}
	}

	(void) event_del(&port->event);
	event_set(&port->event, port->socket, EV_READ | EV_PERSIST,
			  server_port_ready_callback, port);
	if (event_add(&port->event, NULL) < 0) {
		log(EVDNS_LOG_WARN, "Error from libevent when adding event for DNS server.");

	}
}

static void
nameserver_write_waiting(struct nameserver *ns, char waiting) {
	if (ns->write_waiting == waiting) return;

	ns->write_waiting = waiting;
	(void) event_del(&ns->event);
	event_set(&ns->event, ns->socket, EV_READ | (waiting ? EV_WRITE : 0) | EV_PERSIST,
			nameserver_ready_callback, ns);
	if (event_add(&ns->event, NULL) < 0) {
          log(EVDNS_LOG_WARN, "Error from libevent when adding event for %s",
              debug_ntoa(ns->address));

        }
}

static void
nameserver_ready_callback(int fd, short events, void *arg) {
	struct nameserver *ns = (struct nameserver *) arg;
        (void)fd;

	if (events & EV_WRITE) {
		ns->choked = 0;
		if (!evdns_transmit()) {
			nameserver_write_waiting(ns, 0);
		}
	}
	if (events & EV_READ) {
		nameserver_read(ns);
	}
}

static void
server_port_ready_callback(int fd, short events, void *arg) {
	struct evdns_server_port *port = (struct evdns_server_port *) arg;
	(void) fd;

	if (events & EV_WRITE) {
		port->choked = 0;
		server_port_flush(port);
	}
	if (events & EV_READ) {
		server_port_read(port);
	}
}

#define MAX_LABELS 128

struct dnslabel_entry { char *v; off_t pos; };
struct dnslabel_table {
	int n_labels;

	struct dnslabel_entry labels[MAX_LABELS];
};

static void
dnslabel_table_init(struct dnslabel_table *table)
{
	table->n_labels = 0;
}

static void
dnslabel_clear(struct dnslabel_table *table)
{
	int i;
	for (i = 0; i < table->n_labels; ++i)
		free(table->labels[i].v);
	table->n_labels = 0;
}

static int
dnslabel_table_get_pos(const struct dnslabel_table *table, const char *label)
{
	int i;
	for (i = 0; i < table->n_labels; ++i) {
		if (!strcmp(label, table->labels[i].v))
			return table->labels[i].pos;
	}
	return -1;
}

static int
dnslabel_table_add(struct dnslabel_table *table, const char *label, off_t pos)
{
	char *v;
	int p;
	if (table->n_labels == MAX_LABELS)
		return (-1);
	v = strdup(label);
	if (v == NULL)
		return (-1);
	p = table->n_labels++;
	table->labels[p].v = v;
	table->labels[p].pos = pos;

	return (0);
}

static off_t
dnsname_to_labels(u8 *const buf, size_t buf_len, off_t j,
				  const char *name, const int name_len,
				  struct dnslabel_table *table) {
	const char *end = name + name_len;
	int ref = 0;
	u16 _t;

#define APPEND16(x) do {						   \
		if (j + 2 > (off_t)buf_len)				   \
			goto overflow;						   \
		_t = htons(x);							   \
		memcpy(buf + j, &_t, 2);				   \
		j += 2;									   \
	} while (0)
#define APPEND32(x) do {						   \
		if (j + 4 > (off_t)buf_len)				   \
			goto overflow;						   \
		_t32 = htonl(x);						   \
		memcpy(buf + j, &_t32, 4);				   \
		j += 4;									   \
	} while (0)

	if (name_len > 255) return -2;

	for (;;) {
		const char *const start = name;
		if (table && (ref = dnslabel_table_get_pos(table, name)) >= 0) {
			APPEND16(ref | 0xc000);
			return j;
		}
		name = strchr(name, '.');
		if (!name) {
			const unsigned int label_len = end - start;
			if (label_len > 63) return -1;
			if ((size_t)(j+label_len+1) > buf_len) return -2;
			if (table) dnslabel_table_add(table, start, j);
			buf[j++] = label_len;

			memcpy(buf + j, start, end - start);
			j += end - start;
			break;
		} else {

			const unsigned int label_len = name - start;
			if (label_len > 63) return -1;
			if ((size_t)(j+label_len+1) > buf_len) return -2;
			if (table) dnslabel_table_add(table, start, j);
			buf[j++] = label_len;

			memcpy(buf + j, start, name - start);
			j += name - start;

			name++;
		}
	}

	if (!j || buf[j-1]) buf[j++] = 0;
	return j;
 overflow:
	return (-2);
}

static int
evdns_request_len(const int name_len) {
	return 96 +
		name_len + 2 +
		4;
}

static int
evdns_request_data_build(const char *const name, const int name_len,
    const u16 trans_id, const u16 type, const u16 class,
    u8 *const buf, size_t buf_len) {
	off_t j = 0;
	u16 _t;

	APPEND16(trans_id);
	APPEND16(0x0100);
	APPEND16(1);
	APPEND16(0);
	APPEND16(0);
	APPEND16(0);

	j = dnsname_to_labels(buf, buf_len, j, name, name_len, NULL);
	if (j < 0) {
		return (int)j;
	}

	APPEND16(type);
	APPEND16(class);

	return (int)j;
 overflow:
	return (-1);
}

struct evdns_server_port *
evdns_add_server_port(int socket, int is_tcp, evdns_request_callback_fn_type cb, void *user_data)
{
	struct evdns_server_port *port;
	if (!(port = malloc(sizeof(struct evdns_server_port))))
		return NULL;
	memset(port, 0, sizeof(struct evdns_server_port));

	assert(!is_tcp);
	port->socket = socket;
	port->refcnt = 1;
	port->choked = 0;
	port->closing = 0;
	port->user_callback = cb;
	port->user_data = user_data;
	port->pending_replies = NULL;

	event_set(&port->event, port->socket, EV_READ | EV_PERSIST,
			  server_port_ready_callback, port);
	event_add(&port->event, NULL);
	return port;
}

void
evdns_close_server_port(struct evdns_server_port *port)
{
	if (--port->refcnt == 0)
		server_port_free(port);
	port->closing = 1;
}

int
evdns_server_request_add_reply(struct evdns_server_request *_req, int section, const char *name, int type, int class, int ttl, int datalen, int is_name, const char *data)
{
	struct server_request *req = TO_SERVER_REQUEST(_req);
	struct server_reply_item **itemp, *item;
	int *countp;

	if (req->response)
		return (-1);

	switch (section) {
	case EVDNS_ANSWER_SECTION:
		itemp = &req->answer;
		countp = &req->n_answer;
		break;
	case EVDNS_AUTHORITY_SECTION:
		itemp = &req->authority;
		countp = &req->n_authority;
		break;
	case EVDNS_ADDITIONAL_SECTION:
		itemp = &req->additional;
		countp = &req->n_additional;
		break;
	default:
		return (-1);
	}
	while (*itemp) {
		itemp = &((*itemp)->next);
	}
	item = malloc(sizeof(struct server_reply_item));
	if (!item)
		return -1;
	item->next = NULL;
	if (!(item->name = strdup(name))) {
		free(item);
		return -1;
	}
	item->type = type;
	item->dns_question_class = class;
	item->ttl = ttl;
	item->is_name = is_name != 0;
	item->datalen = 0;
	item->data = NULL;
	if (data) {
		if (item->is_name) {
			if (!(item->data = strdup(data))) {
				free(item->name);
				free(item);
				return -1;
			}
			item->datalen = (u16)-1;
		} else {
			if (!(item->data = malloc(datalen))) {
				free(item->name);
				free(item);
				return -1;
			}
			item->datalen = datalen;
			memcpy(item->data, data, datalen);
		}
	}

	*itemp = item;
	++(*countp);
	return 0;
}

int
evdns_server_request_add_a_reply(struct evdns_server_request *req, const char *name, int n, void *addrs, int ttl)
{
	return evdns_server_request_add_reply(
		  req, EVDNS_ANSWER_SECTION, name, TYPE_A, CLASS_INET,
		  ttl, n*4, 0, addrs);
}

int
evdns_server_request_add_aaaa_reply(struct evdns_server_request *req, const char *name, int n, void *addrs, int ttl)
{
	return evdns_server_request_add_reply(
		  req, EVDNS_ANSWER_SECTION, name, TYPE_AAAA, CLASS_INET,
		  ttl, n*16, 0, addrs);
}

int
evdns_server_request_add_ptr_reply(struct evdns_server_request *req, struct in_addr *in, const char *inaddr_name, const char *hostname, int ttl)
{
	u32 a;
	char buf[32];
	assert(in || inaddr_name);
	assert(!(in && inaddr_name));
	if (in) {
		a = ntohl(in->s_addr);
		evutil_snprintf(buf, sizeof(buf), "%d.%d.%d.%d.in-addr.arpa",
				(int)(u8)((a	)&0xff),
				(int)(u8)((a>>8 )&0xff),
				(int)(u8)((a>>16)&0xff),
				(int)(u8)((a>>24)&0xff));
		inaddr_name = buf;
	}
	return evdns_server_request_add_reply(
		  req, EVDNS_ANSWER_SECTION, inaddr_name, TYPE_PTR, CLASS_INET,
		  ttl, -1, 1, hostname);
}

int
evdns_server_request_add_cname_reply(struct evdns_server_request *req, const char *name, const char *cname, int ttl)
{
	return evdns_server_request_add_reply(
		  req, EVDNS_ANSWER_SECTION, name, TYPE_CNAME, CLASS_INET,
		  ttl, -1, 1, cname);
}

static int
evdns_server_request_format_response(struct server_request *req, int err)
{
	unsigned char buf[1500];
	size_t buf_len = sizeof(buf);
	off_t j = 0, r;
	u16 _t;
	u32 _t32;
	int i;
	u16 flags;
	struct dnslabel_table table;

	if (err < 0 || err > 15) return -1;

	flags = req->base.flags;
	flags |= (0x8000 | err);

	dnslabel_table_init(&table);
	APPEND16(req->trans_id);
	APPEND16(flags);
	APPEND16(req->base.nquestions);
	APPEND16(req->n_answer);
	APPEND16(req->n_authority);
	APPEND16(req->n_additional);

	for (i=0; i < req->base.nquestions; ++i) {
		const char *s = req->base.questions[i]->name;
		j = dnsname_to_labels(buf, buf_len, j, s, strlen(s), &table);
		if (j < 0) {
			dnslabel_clear(&table);
			return (int) j;
		}
		APPEND16(req->base.questions[i]->type);
		APPEND16(req->base.questions[i]->dns_question_class);
	}

	for (i=0; i<3; ++i) {
		struct server_reply_item *item;
		if (i==0)
			item = req->answer;
		else if (i==1)
			item = req->authority;
		else
			item = req->additional;
		while (item) {
			r = dnsname_to_labels(buf, buf_len, j, item->name, strlen(item->name), &table);
			if (r < 0)
				goto overflow;
			j = r;

			APPEND16(item->type);
			APPEND16(item->dns_question_class);
			APPEND32(item->ttl);
			if (item->is_name) {
				off_t len_idx = j, name_start;
				j += 2;
				name_start = j;
				r = dnsname_to_labels(buf, buf_len, j, item->data, strlen(item->data), &table);
				if (r < 0)
					goto overflow;
				j = r;
				_t = htons( (short) (j-name_start) );
				memcpy(buf+len_idx, &_t, 2);
			} else {
				APPEND16(item->datalen);
				if (j+item->datalen > (off_t)buf_len)
					goto overflow;
				memcpy(buf+j, item->data, item->datalen);
				j += item->datalen;
			}
			item = item->next;
		}
	}

	if (j > 512) {
overflow:
		j = 512;
		buf[2] |= 0x02;
	}

	req->response_len = j;

	if (!(req->response = malloc(req->response_len))) {
		server_request_free_answers(req);
		dnslabel_clear(&table);
		return (-1);
	}
	memcpy(req->response, buf, req->response_len);
	server_request_free_answers(req);
	dnslabel_clear(&table);
	return (0);
}

int
evdns_server_request_respond(struct evdns_server_request *_req, int err)
{
	struct server_request *req = TO_SERVER_REQUEST(_req);
	struct evdns_server_port *port = req->port;
	int r;
	if (!req->response) {
		if ((r = evdns_server_request_format_response(req, err))<0)
			return r;
	}

	r = sendto(port->socket, req->response, req->response_len, 0,
			   (struct sockaddr*) &req->addr, req->addrlen);
	if (r<0) {
		int sock_err = last_error(port->socket);
		if (! error_is_eagain(sock_err))
			return -1;

		if (port->pending_replies) {
			req->prev_pending = port->pending_replies->prev_pending;
			req->next_pending = port->pending_replies;
			req->prev_pending->next_pending =
				req->next_pending->prev_pending = req;
		} else {
			req->prev_pending = req->next_pending = req;
			port->pending_replies = req;
			port->choked = 1;

			(void) event_del(&port->event);
			event_set(&port->event, port->socket, (port->closing?0:EV_READ) | EV_WRITE | EV_PERSIST, server_port_ready_callback, port);

			if (event_add(&port->event, NULL) < 0) {
				log(EVDNS_LOG_WARN, "Error from libevent when adding event for DNS server");
			}

		}

		return 1;
	}
	if (server_request_free(req))
		return 0;

	if (port->pending_replies)
		server_port_flush(port);

	return 0;
}

static void
server_request_free_answers(struct server_request *req)
{
	struct server_reply_item *victim, *next, **list;
	int i;
	for (i = 0; i < 3; ++i) {
		if (i==0)
			list = &req->answer;
		else if (i==1)
			list = &req->authority;
		else
			list = &req->additional;

		victim = *list;
		while (victim) {
			next = victim->next;
			free(victim->name);
			if (victim->data)
				free(victim->data);
			free(victim);
			victim = next;
		}
		*list = NULL;
	}
}

static int
server_request_free(struct server_request *req)
{
	int i, rc=1;
	if (req->base.questions) {
		for (i = 0; i < req->base.nquestions; ++i)
			free(req->base.questions[i]);
		free(req->base.questions);
	}

	if (req->port) {
		if (req->port->pending_replies == req) {
			if (req->next_pending)
				req->port->pending_replies = req->next_pending;
			else
				req->port->pending_replies = NULL;
		}
		rc = --req->port->refcnt;
	}

	if (req->response) {
		free(req->response);
	}

	server_request_free_answers(req);

	if (req->next_pending && req->next_pending != req) {
		req->next_pending->prev_pending = req->prev_pending;
		req->prev_pending->next_pending = req->next_pending;
	}

	if (rc == 0) {
		server_port_free(req->port);
		free(req);
		return (1);
	}
	free(req);
	return (0);
}

static void
server_port_free(struct evdns_server_port *port)
{
	assert(port);
	assert(!port->refcnt);
	assert(!port->pending_replies);
	if (port->socket > 0) {
		CLOSE_SOCKET(port->socket);
		port->socket = -1;
	}
	(void) event_del(&port->event);

}

int
evdns_server_request_drop(struct evdns_server_request *_req)
{
	struct server_request *req = TO_SERVER_REQUEST(_req);
	server_request_free(req);
	return 0;
}

int
evdns_server_request_get_requesting_addr(struct evdns_server_request *_req, struct sockaddr *sa, int addr_len)
{
	struct server_request *req = TO_SERVER_REQUEST(_req);
	if (addr_len < (int)req->addrlen)
		return -1;
	memcpy(sa, &(req->addr), req->addrlen);
	return req->addrlen;
}

#undef APPEND16
#undef APPEND32

static void
evdns_request_timeout_callback(int fd, short events, void *arg) {
	struct request *const req = (struct request *) arg;
        (void) fd;
        (void) events;

	log(EVDNS_LOG_DEBUG, "Request %lx timed out", (unsigned long) arg);

	req->ns->timedout++;
	if (req->ns->timedout > global_max_nameserver_timeout) {
		req->ns->timedout = 0;
		nameserver_failed(req->ns, "request timed out.");
	}

	(void) evtimer_del(&req->timeout_event);
	if (req->tx_count >= global_max_retransmits) {

		reply_callback(req, 0, DNS_ERR_TIMEOUT, NULL);
		request_finished(req, &req_head);
	} else {

		evdns_request_transmit(req);
	}
}

static int
evdns_request_transmit_to(struct request *req, struct nameserver *server) {
	struct sockaddr_in sin;
	int r;
	memset(&sin, 0, sizeof(sin));
	sin.sin_addr.s_addr = req->ns->address;
	sin.sin_port = req->ns->port;
	sin.sin_family = AF_INET;

	r = sendto(server->socket, req->request, req->request_len, 0,
	    (struct sockaddr*)&sin, sizeof(sin));
	if (r < 0) {
		int err = last_error(server->socket);
		if (error_is_eagain(err)) return 1;
		nameserver_failed(req->ns, strerror(err));
		return 2;
	} else if (r != (int)req->request_len) {
		return 1;
	} else {
		return 0;
	}
}

static int
evdns_request_transmit(struct request *req) {
	int retcode = 0, r;

	req->transmit_me = 1;
	if (req->trans_id == 0xffff) abort();

	if (req->ns->choked) {

		return 1;
	}

	r = evdns_request_transmit_to(req, req->ns);
	switch (r) {
	case 1:

		req->ns->choked = 1;
		nameserver_write_waiting(req->ns, 1);
		return 1;
	case 2:

		retcode = 1;

	default:

		log(EVDNS_LOG_DEBUG,
		    "Setting timeout for request %lx", (unsigned long) req);
		if (evtimer_add(&req->timeout_event, &global_timeout) < 0) {
                  log(EVDNS_LOG_WARN,
		      "Error from libevent when adding timer for request %lx",
                      (unsigned long) req);

                }
		req->tx_count++;
		req->transmit_me = 0;
		return retcode;
	}
}

static void
nameserver_probe_callback(int result, char type, int count, int ttl, void *addresses, void *arg) {
	struct nameserver *const ns = (struct nameserver *) arg;
        (void) type;
        (void) count;
        (void) ttl;
        (void) addresses;

	if (result == DNS_ERR_NONE || result == DNS_ERR_NOTEXIST) {

		nameserver_up(ns);
	} else nameserver_probe_failed(ns);
}

static void
nameserver_send_probe(struct nameserver *const ns) {
	struct request *req;

  	log(EVDNS_LOG_DEBUG, "Sending probe to %s", debug_ntoa(ns->address));

	req = request_new(TYPE_A, "www.google.com", DNS_QUERY_NO_SEARCH, nameserver_probe_callback, ns);
        if (!req) return;

	request_trans_id_set(req, transaction_id_pick());
	req->ns = ns;
	request_submit(req);
}

static int
evdns_transmit(void) {
	char did_try_to_transmit = 0;

	if (req_head) {
		struct request *const started_at = req_head, *req = req_head;

		do {
			if (req->transmit_me) {
				did_try_to_transmit = 1;
				evdns_request_transmit(req);
			}

			req = req->next;
		} while (req != started_at);
	}

	return did_try_to_transmit;
}

int
evdns_count_nameservers(void)
{
	const struct nameserver *server = server_head;
	int n = 0;
	if (!server)
		return 0;
	do {
		++n;
		server = server->next;
	} while (server != server_head);
	return n;
}

int
evdns_clear_nameservers_and_suspend(void)
{
	struct nameserver *server = server_head, *started_at = server_head;
	struct request *req = req_head, *req_started_at = req_head;

	if (!server)
		return 0;
	while (1) {
		struct nameserver *next = server->next;
		(void) event_del(&server->event);
		if (evtimer_initialized(&server->timeout_event))
			(void) evtimer_del(&server->timeout_event);
		if (server->socket >= 0)
			CLOSE_SOCKET(server->socket);
		free(server);
		if (next == started_at)
			break;
		server = next;
	}
	server_head = NULL;
	global_good_nameservers = 0;

	while (req) {
		struct request *next = req->next;
		req->tx_count = req->reissue_count = 0;
		req->ns = NULL;

		(void) evtimer_del(&req->timeout_event);
		req->trans_id = 0;
		req->transmit_me = 0;

		global_requests_waiting++;
		evdns_request_insert(req, &req_waiting_head);

		req_waiting_head = req_waiting_head->prev;

		if (next == req_started_at)
			break;
		req = next;
	}
	req_head = NULL;
	global_requests_inflight = 0;

	return 0;
}

int
evdns_resume(void)
{
	evdns_requests_pump_waiting_queue();
	return 0;
}

static int
_evdns_nameserver_add_impl(unsigned long int address, int port) {

	const struct nameserver *server = server_head, *const started_at = server_head;
	struct nameserver *ns;
	int err = 0;
	if (server) {
		do {
			if (server->address == address) return 3;
			server = server->next;
		} while (server != started_at);
	}

	ns = (struct nameserver *) malloc(sizeof(struct nameserver));
        if (!ns) return -1;

	memset(ns, 0, sizeof(struct nameserver));

	evtimer_set(&ns->timeout_event, nameserver_prod_callback, ns);

	ns->socket = socket(PF_INET, SOCK_DGRAM, 0);
	if (ns->socket < 0) { err = 1; goto out1; }
	FD_CLOSEONEXEC(ns->socket);
	evutil_make_socket_nonblocking(ns->socket);

	ns->address = address;
	ns->port = htons(port);
	ns->state = 1;
	event_set(&ns->event, ns->socket, EV_READ | EV_PERSIST, nameserver_ready_callback, ns);
	if (event_add(&ns->event, NULL) < 0) {
          err = 2;
          goto out2;
        }

	log(EVDNS_LOG_DEBUG, "Added nameserver %s", debug_ntoa(address));

	if (!server_head) {
		ns->next = ns->prev = ns;
		server_head = ns;
	} else {
		ns->next = server_head->next;
		ns->prev = server_head;
		server_head->next = ns;
		if (server_head->prev == server_head) {
			server_head->prev = ns;
		}
	}

	global_good_nameservers++;

	return 0;

out2:
	CLOSE_SOCKET(ns->socket);
out1:
	free(ns);
	log(EVDNS_LOG_WARN, "Unable to add nameserver %s: error %d", debug_ntoa(address), err);
	return err;
}

int
evdns_nameserver_add(unsigned long int address) {
	return _evdns_nameserver_add_impl(address, 53);
}

int
evdns_nameserver_ip_add(const char *ip_as_string) {
	struct in_addr ina;
	int port;
	char buf[20];
	const char *cp;
	cp = strchr(ip_as_string, ':');
	if (! cp) {
		cp = ip_as_string;
		port = 53;
	} else {
		port = strtoint(cp+1);
		if (port < 0 || port > 65535) {
			return 4;
		}
		if ((cp-ip_as_string) >= (int)sizeof(buf)) {
			return 4;
		}
		memcpy(buf, ip_as_string, cp-ip_as_string);
		buf[cp-ip_as_string] = '\0';
		cp = buf;
	}
	if (!inet_aton(cp, &ina)) {
		return 4;
	}
	return _evdns_nameserver_add_impl(ina.s_addr, port);
}

static void
evdns_request_insert(struct request *req, struct request **head) {
	if (!*head) {
		*head = req;
		req->next = req->prev = req;
		return;
	}

	req->prev = (*head)->prev;
	req->prev->next = req;
	req->next = *head;
	(*head)->prev = req;
}

static int
string_num_dots(const char *s) {
	int count = 0;
	while ((s = strchr(s, '.'))) {
		s++;
		count++;
	}
	return count;
}

static struct request *
request_new(int type, const char *name, int flags,
    evdns_callback_type callback, void *user_ptr) {
	const char issuing_now =
	    (global_requests_inflight < global_max_requests_inflight) ? 1 : 0;

	const int name_len = strlen(name);
	const int request_max_len = evdns_request_len(name_len);
	const u16 trans_id = issuing_now ? transaction_id_pick() : 0xffff;

	struct request *const req =
	    (struct request *) malloc(sizeof(struct request) + request_max_len);
	int rlen;
        (void) flags;

        if (!req) return NULL;
	memset(req, 0, sizeof(struct request));

	evtimer_set(&req->timeout_event, evdns_request_timeout_callback, req);

	req->request = ((u8 *) req) + sizeof(struct request);

	req->request_appended = 1;
	rlen = evdns_request_data_build(name, name_len, trans_id,
	    type, CLASS_INET, req->request, request_max_len);
	if (rlen < 0)
		goto err1;
	req->request_len = rlen;
	req->trans_id = trans_id;
	req->tx_count = 0;
	req->request_type = type;
	req->user_pointer = user_ptr;
	req->user_callback = callback;
	req->ns = issuing_now ? nameserver_pick() : NULL;
	req->next = req->prev = NULL;

	return req;
err1:
	free(req);
	return NULL;
}

static void
request_submit(struct request *const req) {
	if (req->ns) {

		evdns_request_insert(req, &req_head);
		global_requests_inflight++;
		evdns_request_transmit(req);
	} else {
		evdns_request_insert(req, &req_waiting_head);
		global_requests_waiting++;
	}
}

int evdns_resolve_ipv4(const char *name, int flags,
    evdns_callback_type callback, void *ptr) {
	log(EVDNS_LOG_DEBUG, "Resolve requested for %s", name);
	if (flags & DNS_QUERY_NO_SEARCH) {
		struct request *const req =
			request_new(TYPE_A, name, flags, callback, ptr);
		if (req == NULL)
			return (1);
		request_submit(req);
		return (0);
	} else {
		return (search_request_new(TYPE_A, name, flags, callback, ptr));
	}
}

int evdns_resolve_ipv6(const char *name, int flags,
					   evdns_callback_type callback, void *ptr) {
	log(EVDNS_LOG_DEBUG, "Resolve requested for %s", name);
	if (flags & DNS_QUERY_NO_SEARCH) {
		struct request *const req =
			request_new(TYPE_AAAA, name, flags, callback, ptr);
		if (req == NULL)
			return (1);
		request_submit(req);
		return (0);
	} else {
		return (search_request_new(TYPE_AAAA, name, flags, callback, ptr));
	}
}

int evdns_resolve_reverse(const struct in_addr *in, int flags, evdns_callback_type callback, void *ptr) {
	char buf[32];
	struct request *req;
	u32 a;
	assert(in);
	a = ntohl(in->s_addr);
	evutil_snprintf(buf, sizeof(buf), "%d.%d.%d.%d.in-addr.arpa",
			(int)(u8)((a	)&0xff),
			(int)(u8)((a>>8 )&0xff),
			(int)(u8)((a>>16)&0xff),
			(int)(u8)((a>>24)&0xff));
	log(EVDNS_LOG_DEBUG, "Resolve requested for %s (reverse)", buf);
	req = request_new(TYPE_PTR, buf, flags, callback, ptr);
	if (!req) return 1;
	request_submit(req);
	return 0;
}

int evdns_resolve_reverse_ipv6(const struct in6_addr *in, int flags, evdns_callback_type callback, void *ptr) {

	char buf[73];
	char *cp;
	struct request *req;
	int i;
	assert(in);
	cp = buf;
	for (i=15; i >= 0; --i) {
		u8 byte = in->s6_addr[i];
		*cp++ = "0123456789abcdef"[byte & 0x0f];
		*cp++ = '.';
		*cp++ = "0123456789abcdef"[byte >> 4];
		*cp++ = '.';
	}
	assert(cp + strlen("ip6.arpa") < buf+sizeof(buf));
	memcpy(cp, "ip6.arpa", strlen("ip6.arpa")+1);
	log(EVDNS_LOG_DEBUG, "Resolve requested for %s (reverse)", buf);
	req = request_new(TYPE_PTR, buf, flags, callback, ptr);
	if (!req) return 1;
	request_submit(req);
	return 0;
}

struct search_domain {
	int len;
	struct search_domain *next;

};

struct search_state {
	int refcount;
	int ndots;
	int num_domains;
	struct search_domain *head;
};

static struct search_state *global_search_state = NULL;

static void
search_state_decref(struct search_state *const state) {
	if (!state) return;
	state->refcount--;
	if (!state->refcount) {
		struct search_domain *next, *dom;
		for (dom = state->head; dom; dom = next) {
			next = dom->next;
			free(dom);
		}
		free(state);
	}
}

static struct search_state *
search_state_new(void) {
	struct search_state *state = (struct search_state *) malloc(sizeof(struct search_state));
        if (!state) return NULL;
	memset(state, 0, sizeof(struct search_state));
	state->refcount = 1;
	state->ndots = 1;

	return state;
}

static void
search_postfix_clear(void) {
	search_state_decref(global_search_state);

	global_search_state = search_state_new();
}

void
evdns_search_clear(void) {
	search_postfix_clear();
}

static void
search_postfix_add(const char *domain) {
	int domain_len;
	struct search_domain *sdomain;
	while (domain[0] == '.') domain++;
	domain_len = strlen(domain);

	if (!global_search_state) global_search_state = search_state_new();
        if (!global_search_state) return;
	global_search_state->num_domains++;

	sdomain = (struct search_domain *) malloc(sizeof(struct search_domain) + domain_len);
        if (!sdomain) return;
	memcpy( ((u8 *) sdomain) + sizeof(struct search_domain), domain, domain_len);
	sdomain->next = global_search_state->head;
	sdomain->len = domain_len;

	global_search_state->head = sdomain;
}

static void
search_reverse(void) {
	struct search_domain *cur, *prev = NULL, *next;
	cur = global_search_state->head;
	while (cur) {
		next = cur->next;
		cur->next = prev;
		prev = cur;
		cur = next;
	}

	global_search_state->head = prev;
}

void
evdns_search_add(const char *domain) {
	search_postfix_add(domain);
}

void
evdns_search_ndots_set(const int ndots) {
	if (!global_search_state) global_search_state = search_state_new();
        if (!global_search_state) return;
	global_search_state->ndots = ndots;
}

static void
search_set_from_hostname(void) {
	char hostname[HOST_NAME_MAX + 1], *domainname;

	search_postfix_clear();
	if (gethostname(hostname, sizeof(hostname))) return;
	domainname = strchr(hostname, '.');
	if (!domainname) return;
	search_postfix_add(domainname);
}

static char *
search_make_new(const struct search_state *const state, int n, const char *const base_name) {
	const int base_len = strlen(base_name);
	const char need_to_append_dot = base_name[base_len - 1] == '.' ? 0 : 1;
	struct search_domain *dom;

	for (dom = state->head; dom; dom = dom->next) {
		if (!n--) {

			const u8 *const postfix = ((u8 *) dom) + sizeof(struct search_domain);
			const int postfix_len = dom->len;
			char *const newname = (char *) malloc(base_len + need_to_append_dot + postfix_len + 1);
                        if (!newname) return NULL;
			memcpy(newname, base_name, base_len);
			if (need_to_append_dot) newname[base_len] = '.';
			memcpy(newname + base_len + need_to_append_dot, postfix, postfix_len);
			newname[base_len + need_to_append_dot + postfix_len] = 0;
			return newname;
		}
	}

	abort();
	return NULL;
}

static int
search_request_new(int type, const char *const name, int flags, evdns_callback_type user_callback, void *user_arg) {
	assert(type == TYPE_A || type == TYPE_AAAA);
	if ( ((flags & DNS_QUERY_NO_SEARCH) == 0) &&
	     global_search_state &&
		 global_search_state->num_domains) {

		struct request *req;
		if (string_num_dots(name) >= global_search_state->ndots) {
			req = request_new(type, name, flags, user_callback, user_arg);
			if (!req) return 1;
			req->search_index = -1;
		} else {
			char *const new_name = search_make_new(global_search_state, 0, name);
                        if (!new_name) return 1;
			req = request_new(type, new_name, flags, user_callback, user_arg);
			free(new_name);
			if (!req) return 1;
			req->search_index = 0;
		}
		req->search_origname = strdup(name);
		req->search_state = global_search_state;
		req->search_flags = flags;
		global_search_state->refcount++;
		request_submit(req);
		return 0;
	} else {
		struct request *const req = request_new(type, name, flags, user_callback, user_arg);
		if (!req) return 1;
		request_submit(req);
		return 0;
	}
}

static int
search_try_next(struct request *const req) {
	if (req->search_state) {

		char *new_name;
		struct request *newreq;
		req->search_index++;
		if (req->search_index >= req->search_state->num_domains) {

			if (string_num_dots(req->search_origname) < req->search_state->ndots) {

				newreq = request_new(req->request_type, req->search_origname, req->search_flags, req->user_callback, req->user_pointer);
				log(EVDNS_LOG_DEBUG, "Search: trying raw query %s", req->search_origname);
				if (newreq) {
					request_submit(newreq);
					return 0;
				}
			}
			return 1;
		}

		new_name = search_make_new(req->search_state, req->search_index, req->search_origname);
                if (!new_name) return 1;
		log(EVDNS_LOG_DEBUG, "Search: now trying %s (%d)", new_name, req->search_index);
		newreq = request_new(req->request_type, new_name, req->search_flags, req->user_callback, req->user_pointer);
		free(new_name);
		if (!newreq) return 1;
		newreq->search_origname = req->search_origname;
		req->search_origname = NULL;
		newreq->search_state = req->search_state;
		newreq->search_flags = req->search_flags;
		newreq->search_index = req->search_index;
		newreq->search_state->refcount++;
		request_submit(newreq);
		return 0;
	}
	return 1;
}

static void
search_request_finished(struct request *const req) {
	if (req->search_state) {
		search_state_decref(req->search_state);
		req->search_state = NULL;
	}
	if (req->search_origname) {
		free(req->search_origname);
		req->search_origname = NULL;
	}
}

static void
evdns_resolv_set_defaults(int flags) {

	if (flags & DNS_OPTION_SEARCH) search_set_from_hostname();
	if (flags & DNS_OPTION_NAMESERVERS) evdns_nameserver_ip_add("127.0.0.1");
}

#ifndef HAVE_STRTOK_R
static char *
strtok_r(char *s, const char *delim, char **state) {
	return strtok(s, delim);
}
#endif

static int
strtoint(const char *const str) {
	char *endptr;
	const int r = strtol(str, &endptr, 10);
	if (*endptr) return -1;
	return r;
}

static int
strtoint_clipped(const char *const str, int min, int max)
{
	int r = strtoint(str);
	if (r == -1)
		return r;
	else if (r<min)
		return min;
	else if (r>max)
		return max;
	else
		return r;
}

int
evdns_set_option(const char *option, const char *val, int flags)
{
	if (!strncmp(option, "ndots:", 6)) {
		const int ndots = strtoint(val);
		if (ndots == -1) return -1;
		if (!(flags & DNS_OPTION_SEARCH)) return 0;
		log(EVDNS_LOG_DEBUG, "Setting ndots to %d", ndots);
		if (!global_search_state) global_search_state = search_state_new();
		if (!global_search_state) return -1;
		global_search_state->ndots = ndots;
	} else if (!strncmp(option, "timeout:", 8)) {
		const int timeout = strtoint(val);
		if (timeout == -1) return -1;
		if (!(flags & DNS_OPTION_MISC)) return 0;
		log(EVDNS_LOG_DEBUG, "Setting timeout to %d", timeout);
		global_timeout.tv_sec = timeout;
	} else if (!strncmp(option, "max-timeouts:", 12)) {
		const int maxtimeout = strtoint_clipped(val, 1, 255);
		if (maxtimeout == -1) return -1;
		if (!(flags & DNS_OPTION_MISC)) return 0;
		log(EVDNS_LOG_DEBUG, "Setting maximum allowed timeouts to %d",
			maxtimeout);
		global_max_nameserver_timeout = maxtimeout;
	} else if (!strncmp(option, "max-inflight:", 13)) {
		const int maxinflight = strtoint_clipped(val, 1, 65000);
		if (maxinflight == -1) return -1;
		if (!(flags & DNS_OPTION_MISC)) return 0;
		log(EVDNS_LOG_DEBUG, "Setting maximum inflight requests to %d",
			maxinflight);
		global_max_requests_inflight = maxinflight;
	} else if (!strncmp(option, "attempts:", 9)) {
		int retries = strtoint(val);
		if (retries == -1) return -1;
		if (retries > 255) retries = 255;
		if (!(flags & DNS_OPTION_MISC)) return 0;
		log(EVDNS_LOG_DEBUG, "Setting retries to %d", retries);
		global_max_retransmits = retries;
	}
	return 0;
}

static void
resolv_conf_parse_line(char *const start, int flags) {
	char *strtok_state;
	static const char *const delims = " \t";
#define NEXT_TOKEN strtok_r(NULL, delims, &strtok_state)

	char *const first_token = strtok_r(start, delims, &strtok_state);
	if (!first_token) return;

	if (!strcmp(first_token, "nameserver") && (flags & DNS_OPTION_NAMESERVERS)) {
		const char *const nameserver = NEXT_TOKEN;
		struct in_addr ina;

		if (nameserver && inet_aton(nameserver, &ina)) {

			evdns_nameserver_add(ina.s_addr);
		}
	} else if (!strcmp(first_token, "domain") && (flags & DNS_OPTION_SEARCH)) {
		const char *const domain = NEXT_TOKEN;
		if (domain) {
			search_postfix_clear();
			search_postfix_add(domain);
		}
	} else if (!strcmp(first_token, "search") && (flags & DNS_OPTION_SEARCH)) {
		const char *domain;
		search_postfix_clear();

		while ((domain = NEXT_TOKEN)) {
			search_postfix_add(domain);
		}
		search_reverse();
	} else if (!strcmp(first_token, "options")) {
		const char *option;
		while ((option = NEXT_TOKEN)) {
			const char *val = strchr(option, ':');
			evdns_set_option(option, val ? val+1 : "", flags);
		}
	}
#undef NEXT_TOKEN
}

int
evdns_resolv_conf_parse(int flags, const char *const filename) {
	struct stat st;
	int fd, n, r;
	u8 *resolv;
	char *start;
	int err = 0;

	log(EVDNS_LOG_DEBUG, "Parsing resolv.conf file %s", filename);

	fd = open(filename, O_RDONLY);
	if (fd < 0) {
		evdns_resolv_set_defaults(flags);
		return 1;
	}

	if (fstat(fd, &st)) { err = 2; goto out1; }
	if (!st.st_size) {
		evdns_resolv_set_defaults(flags);
		err = (flags & DNS_OPTION_NAMESERVERS) ? 6 : 0;
		goto out1;
	}
	if (st.st_size > 65535) { err = 3; goto out1; }

	resolv = (u8 *) malloc((size_t)st.st_size + 1);
	if (!resolv) { err = 4; goto out1; }

	n = 0;
	while ((r = read(fd, resolv+n, (size_t)st.st_size-n)) > 0) {
		n += r;
		if (n == st.st_size)
			break;
		assert(n < st.st_size);
 	}
	if (r < 0) { err = 5; goto out2; }
	resolv[n] = 0;

	start = (char *) resolv;
	for (;;) {
		char *const newline = strchr(start, '\n');
		if (!newline) {
			resolv_conf_parse_line(start, flags);
			break;
		} else {
			*newline = 0;
			resolv_conf_parse_line(start, flags);
			start = newline + 1;
		}
	}

	if (!server_head && (flags & DNS_OPTION_NAMESERVERS)) {

		evdns_nameserver_ip_add("127.0.0.1");
		err = 6;
	}
	if (flags & DNS_OPTION_SEARCH && (!global_search_state || global_search_state->num_domains == 0)) {
		search_set_from_hostname();
	}

out2:
	free(resolv);
out1:
	close(fd);
	return err;
}

#ifdef WIN32

static int
evdns_nameserver_ip_add_line(const char *ips) {
	const char *addr;
	char *buf;
	int r;
	while (*ips) {
		while (ISSPACE(*ips) || *ips == ',' || *ips == '\t')
			++ips;
		addr = ips;
		while (ISDIGIT(*ips) || *ips == '.' || *ips == ':')
			++ips;
		buf = malloc(ips-addr+1);
		if (!buf) return 4;
		memcpy(buf, addr, ips-addr);
		buf[ips-addr] = '\0';
		r = evdns_nameserver_ip_add(buf);
		free(buf);
		if (r) return r;
	}
	return 0;
}

typedef DWORD(WINAPI *GetNetworkParams_fn_t)(FIXED_INFO *, DWORD*);

static int
load_nameservers_with_getnetworkparams(void)
{

	FIXED_INFO *fixed;
	HMODULE handle = 0;
	ULONG size = sizeof(FIXED_INFO);
	void *buf = NULL;
	int status = 0, r, added_any;
	IP_ADDR_STRING *ns;
	GetNetworkParams_fn_t fn;

	if (!(handle = LoadLibraryA("iphlpapi.dll"))) {
		log(EVDNS_LOG_WARN, "Could not open iphlpapi.dll");
		status = -1;
		goto done;
	}
	if (!(fn = (GetNetworkParams_fn_t) GetProcAddress(handle, "GetNetworkParams"))) {
		log(EVDNS_LOG_WARN, "Could not get address of function.");
		status = -1;
		goto done;
	}

	buf = malloc(size);
	if (!buf) { status = 4; goto done; }
	fixed = buf;
	r = fn(fixed, &size);
	if (r != ERROR_SUCCESS && r != ERROR_BUFFER_OVERFLOW) {
		status = -1;
		goto done;
	}
	if (r != ERROR_SUCCESS) {
		free(buf);
		buf = malloc(size);
		if (!buf) { status = 4; goto done; }
		fixed = buf;
		r = fn(fixed, &size);
		if (r != ERROR_SUCCESS) {
			log(EVDNS_LOG_DEBUG, "fn() failed.");
			status = -1;
			goto done;
		}
	}

	assert(fixed);
	added_any = 0;
	ns = &(fixed->DnsServerList);
	while (ns) {
		r = evdns_nameserver_ip_add_line(ns->IpAddress.String);
		if (r) {
			log(EVDNS_LOG_DEBUG,"Could not add nameserver %s to list,error: %d",
				(ns->IpAddress.String),(int)GetLastError());
			status = r;
			goto done;
		} else {
			log(EVDNS_LOG_DEBUG,"Succesfully added %s as nameserver",ns->IpAddress.String);
		}

		added_any++;
		ns = ns->Next;
	}

	if (!added_any) {
		log(EVDNS_LOG_DEBUG, "No nameservers added.");
		status = -1;
	}

 done:
	if (buf)
		free(buf);
	if (handle)
		FreeLibrary(handle);
	return status;
}

static int
config_nameserver_from_reg_key(HKEY key, const char *subkey)
{
	char *buf;
	DWORD bufsz = 0, type = 0;
	int status = 0;

	if (RegQueryValueExA(key, subkey, 0, &type, NULL, &bufsz)
	    != ERROR_MORE_DATA)
		return -1;
	if (!(buf = malloc(bufsz)))
		return -1;

	if (RegQueryValueExA(key, subkey, 0, &type, (LPBYTE)buf, &bufsz)
	    == ERROR_SUCCESS && bufsz > 1) {
		status = evdns_nameserver_ip_add_line(buf);
	}

	free(buf);
	return status;
}

#define SERVICES_KEY "System\\CurrentControlSet\\Services\\"
#define WIN_NS_9X_KEY  SERVICES_KEY "VxD\\MSTCP"
#define WIN_NS_NT_KEY  SERVICES_KEY "Tcpip\\Parameters"

static int
load_nameservers_from_registry(void)
{
	int found = 0;
	int r;
#define TRY(k, name) \
	if (!found && config_nameserver_from_reg_key(k,name) == 0) {	\
		log(EVDNS_LOG_DEBUG,"Found nameservers in %s/%s",#k,name); \
		found = 1;						\
	} else if (!found) {						\
		log(EVDNS_LOG_DEBUG,"Didn't find nameservers in %s/%s", \
		    #k,#name);						\
	}

	if (((int)GetVersion()) > 0) {
		HKEY nt_key = 0, interfaces_key = 0;

		if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, WIN_NS_NT_KEY, 0,
				 KEY_READ, &nt_key) != ERROR_SUCCESS) {
			log(EVDNS_LOG_DEBUG,"Couldn't open nt key, %d",(int)GetLastError());
			return -1;
		}
		r = RegOpenKeyExA(nt_key, "Interfaces", 0,
			     KEY_QUERY_VALUE|KEY_ENUMERATE_SUB_KEYS,
			     &interfaces_key);
		if (r != ERROR_SUCCESS) {
			log(EVDNS_LOG_DEBUG,"Couldn't open interfaces key, %d",(int)GetLastError());
			return -1;
		}
		TRY(nt_key, "NameServer");
		TRY(nt_key, "DhcpNameServer");
		TRY(interfaces_key, "NameServer");
		TRY(interfaces_key, "DhcpNameServer");
		RegCloseKey(interfaces_key);
		RegCloseKey(nt_key);
	} else {
		HKEY win_key = 0;
		if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, WIN_NS_9X_KEY, 0,
				 KEY_READ, &win_key) != ERROR_SUCCESS) {
			log(EVDNS_LOG_DEBUG, "Couldn't open registry key, %d", (int)GetLastError());
			return -1;
		}
		TRY(win_key, "NameServer");
		RegCloseKey(win_key);
	}

	if (found == 0) {
		log(EVDNS_LOG_WARN,"Didn't find any nameservers.");
	}

	return found ? 0 : -1;
#undef TRY
}

int
evdns_config_windows_nameservers(void)
{
	if (load_nameservers_with_getnetworkparams() == 0)
		return 0;
	return load_nameservers_from_registry();
}
#endif

int
evdns_init(void)
{
	int res = 0;
#ifdef WIN32
	res = evdns_config_windows_nameservers();
#else
	res = evdns_resolv_conf_parse(DNS_OPTIONS_ALL, "/etc/resolv.conf");
#endif

	return (res);
}

const char *
evdns_err_to_string(int err)
{
    switch (err) {
	case DNS_ERR_NONE: return "no error";
	case DNS_ERR_FORMAT: return "misformatted query";
	case DNS_ERR_SERVERFAILED: return "server failed";
	case DNS_ERR_NOTEXIST: return "name does not exist";
	case DNS_ERR_NOTIMPL: return "query not implemented";
	case DNS_ERR_REFUSED: return "refused";

	case DNS_ERR_TRUNCATED: return "reply truncated or ill-formed";
	case DNS_ERR_UNKNOWN: return "unknown";
	case DNS_ERR_TIMEOUT: return "request timed out";
	case DNS_ERR_SHUTDOWN: return "dns subsystem shut down";
	default: return "[Unknown error code]";
    }
}

void
evdns_shutdown(int fail_requests)
{
	struct nameserver *server, *server_next;
	struct search_domain *dom, *dom_next;

	while (req_head) {
		if (fail_requests)
			reply_callback(req_head, 0, DNS_ERR_SHUTDOWN, NULL);
		request_finished(req_head, &req_head);
	}
	while (req_waiting_head) {
		if (fail_requests)
			reply_callback(req_waiting_head, 0, DNS_ERR_SHUTDOWN, NULL);
		request_finished(req_waiting_head, &req_waiting_head);
	}
	global_requests_inflight = global_requests_waiting = 0;

	for (server = server_head; server; server = server_next) {
		server_next = server->next;
		if (server->socket >= 0)
			CLOSE_SOCKET(server->socket);
		(void) event_del(&server->event);
		if (server->state == 0)
                        (void) event_del(&server->timeout_event);
		free(server);
		if (server_next == server_head)
			break;
	}
	server_head = NULL;
	global_good_nameservers = 0;

	if (global_search_state) {
		for (dom = global_search_state->head; dom; dom = dom_next) {
			dom_next = dom->next;
			free(dom);
		}
		free(global_search_state);
		global_search_state = NULL;
	}
	evdns_log_fn = NULL;
}

#ifdef EVDNS_MAIN
void
main_callback(int result, char type, int count, int ttl,
			  void *addrs, void *orig) {
	char *n = (char*)orig;
	int i;
	for (i = 0; i < count; ++i) {
		if (type == DNS_IPv4_A) {
			printf("%s: %s\n", n, debug_ntoa(((u32*)addrs)[i]));
		} else if (type == DNS_PTR) {
			printf("%s: %s\n", n, ((char**)addrs)[i]);
		}
	}
	if (!count) {
		printf("%s: No answer (%d)\n", n, result);
	}
	fflush(stdout);
}
void
evdns_server_callback(struct evdns_server_request *req, void *data)
{
	int i, r;
	(void)data;

	for (i = 0; i < req->nquestions; ++i) {
		u32 ans = htonl(0xc0a80b0bUL);
		if (req->questions[i]->type == EVDNS_TYPE_A &&
			req->questions[i]->dns_question_class == EVDNS_CLASS_INET) {
			printf(" -- replying for %s (A)\n", req->questions[i]->name);
			r = evdns_server_request_add_a_reply(req, req->questions[i]->name,
										  1, &ans, 10);
			if (r<0)
				printf("eeep, didn't work.\n");
		} else if (req->questions[i]->type == EVDNS_TYPE_PTR &&
				   req->questions[i]->dns_question_class == EVDNS_CLASS_INET) {
			printf(" -- replying for %s (PTR)\n", req->questions[i]->name);
			r = evdns_server_request_add_ptr_reply(req, NULL, req->questions[i]->name,
											"foo.bar.example.com", 10);
		} else {
			printf(" -- skipping %s [%d %d]\n", req->questions[i]->name,
				   req->questions[i]->type, req->questions[i]->dns_question_class);
		}
	}

	r = evdns_request_respond(req, 0);
	if (r<0)
		printf("eeek, couldn't send reply.\n");
}

void
logfn(int is_warn, const char *msg) {
  (void) is_warn;
  fprintf(stderr, "%s\n", msg);
}
int
main(int c, char **v) {
	int idx;
	int reverse = 0, verbose = 1, servertest = 0;
	if (c<2) {
		fprintf(stderr, "syntax: %s [-x] [-v] hostname\n", v[0]);
		fprintf(stderr, "syntax: %s [-servertest]\n", v[0]);
		return 1;
	}
	idx = 1;
	while (idx < c && v[idx][0] == '-') {
		if (!strcmp(v[idx], "-x"))
			reverse = 1;
		else if (!strcmp(v[idx], "-v"))
			verbose = 1;
		else if (!strcmp(v[idx], "-servertest"))
			servertest = 1;
		else
			fprintf(stderr, "Unknown option %s\n", v[idx]);
		++idx;
	}
	event_init();
	if (verbose)
		evdns_set_log_fn(logfn);
	evdns_resolv_conf_parse(DNS_OPTION_NAMESERVERS, "/etc/resolv.conf");
	if (servertest) {
		int sock;
		struct sockaddr_in my_addr;
		sock = socket(PF_INET, SOCK_DGRAM, 0);
                evutil_make_socket_nonblocking(sock);
		my_addr.sin_family = AF_INET;
		my_addr.sin_port = htons(10053);
		my_addr.sin_addr.s_addr = INADDR_ANY;
		if (bind(sock, (struct sockaddr*)&my_addr, sizeof(my_addr))<0) {
			perror("bind");
			exit(1);
		}
		evdns_add_server_port(sock, 0, evdns_server_callback, NULL);
	}
	for (; idx < c; ++idx) {
		if (reverse) {
			struct in_addr addr;
			if (!inet_aton(v[idx], &addr)) {
				fprintf(stderr, "Skipping non-IP %s\n", v[idx]);
				continue;
			}
			fprintf(stderr, "resolving %s...\n",v[idx]);
			evdns_resolve_reverse(&addr, 0, main_callback, v[idx]);
		} else {
			fprintf(stderr, "resolving (fwd) %s...\n",v[idx]);
			evdns_resolve_ipv4(v[idx], 0, main_callback, v[idx]);
		}
	}
	fflush(stdout);
	event_dispatch();
	return 0;
}
#endif
