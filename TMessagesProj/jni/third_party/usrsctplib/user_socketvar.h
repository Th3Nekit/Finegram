/*-
 * Copyright (c) 1982, 1986, 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

/* __Userspace__ version of <sys/socketvar.h> goes here.*/

#ifndef _USER_SOCKETVAR_H_
#define _USER_SOCKETVAR_H_

#if defined(__APPLE__)
#include <sys/types.h>
#include <unistd.h>
#endif

#if !defined(__DragonFly__) && !defined(__FreeBSD__) && !defined(__NetBSD__) && !defined(_WIN32) && !defined(__native_client__)
#include <sys/uio.h>
#endif
#define SOCK_MAXADDRLEN 255
#if !defined(MSG_NOTIFICATION)
#define MSG_NOTIFICATION 0x2000
#endif
#define SCTP_SO_LINGER     0x0001
#define SCTP_SO_ACCEPTCONN 0x0002
#define SS_CANTRCVMORE 0x020
#define SS_CANTSENDMORE 0x010

#if defined(__APPLE__) || defined(__DragonFly__) || defined(__FreeBSD__) || defined(__OpenBSD__) || defined(_WIN32) || defined(__native_client__)
#define UIO_MAXIOV 1024
#define ERESTART (-1)
#endif

#if !defined(__APPLE__) && !defined(__NetBSD__) && !defined(__OpenBSD__)
enum	uio_rw { UIO_READ, UIO_WRITE };
#endif

#if !defined(__NetBSD__) && !defined(__OpenBSD__)

enum uio_seg {
	UIO_USERSPACE,
	UIO_SYSSPACE
};
#endif

struct proc {
	int stub;
};

MALLOC_DECLARE(M_ACCF);
MALLOC_DECLARE(M_PCB);
MALLOC_DECLARE(M_SONAME);

struct uio {
	struct	iovec *uio_iov;
	int		uio_iovcnt;
	off_t	uio_offset;
	ssize_t 	uio_resid;
	enum	uio_seg uio_segflg;
	enum	uio_rw uio_rw;
};

#if defined(_WIN32)
#define AF_ROUTE  17
#if !defined(__MINGW32__)
typedef __int32 pid_t;
#endif
typedef unsigned __int32 uid_t;
enum sigType {
	SIGNAL = 0,
	BROADCAST = 1,
	MAX_EVENTS = 2
};
#endif

struct socket {
	int	so_count;
	short	so_type;
	short	so_options;
	short	so_linger;
	short	so_state;
	int	so_qstate;
	void	*so_pcb;
	int	so_dom;

	struct	socket *so_head;
	TAILQ_HEAD(, socket) so_incomp;
	TAILQ_HEAD(, socket) so_comp;
	TAILQ_ENTRY(socket) so_list;
	u_short	so_qlen;
	u_short	so_incqlen;

	u_short	so_qlimit;
	short	so_timeo;
	userland_cond_t timeo_cond;

	u_short	so_error;
	struct	sigio *so_sigio;

	u_long	so_oobmark;
	TAILQ_HEAD(, aiocblist) so_aiojobq;

	struct sockbuf {

		userland_cond_t sb_cond;
		userland_mutex_t sb_mtx;
		short	sb_state;
#define	sb_startzero	sb_mb
		struct	mbuf *sb_mb;
		struct	mbuf *sb_mbtail;
		struct	mbuf *sb_lastrecord;

		struct	mbuf *sb_sndptr;
		u_int	sb_sndptroff;
		u_int	sb_cc;
		u_int	sb_hiwat;
		u_int	sb_mbcnt;
		u_int	sb_mbmax;
		u_int	sb_ctl;
		int	sb_lowat;
		int	sb_timeo;
		short	sb_flags;
	} so_rcv, so_snd;

#define	SB_MAX		(256*1024)
#define SB_RAW          (64*1024*2)

#define	SB_WAIT		0x04
#define	SB_SEL		0x08
#define	SB_ASYNC	0x10
#define	SB_UPCALL	0x20
#define	SB_NOINTR	0x40
#define	SB_AIO		0x80
#define	SB_KNOTE	0x100
#define	SB_AUTOSIZE	0x800

	void	(*so_upcall)(struct socket *, void *, int);
	void	*so_upcallarg;
	struct	ucred *so_cred;
	struct	label *so_label;
	struct	label *so_peerlabel;

	uint32_t so_gencnt;
	void	*so_emuldata;
 	struct so_accf {
		struct	accept_filter *so_accept_filter;
		void	*so_accept_filter_arg;
		char	*so_accept_filter_str;
	} *so_accf;
};

#define SB_EMPTY_FIXUP(sb) do {						\
	if ((sb)->sb_mb == NULL) {					\
		(sb)->sb_mbtail = NULL;					\
		(sb)->sb_lastrecord = NULL;				\
	}								\
} while (             0)

#if defined(_WIN32)
extern userland_mutex_t accept_mtx;
extern userland_cond_t accept_cond;
#define ACCEPT_LOCK_ASSERT()
#define	ACCEPT_LOCK() do { \
	EnterCriticalSection(&accept_mtx); \
} while (0)
#define	ACCEPT_UNLOCK()	do { \
	LeaveCriticalSection(&accept_mtx); \
} while (0)
#define	ACCEPT_UNLOCK_ASSERT()
#else
extern userland_mutex_t accept_mtx;

extern userland_cond_t accept_cond;
#ifdef INVARIANTS
#define	ACCEPT_LOCK()	KASSERT(pthread_mutex_lock(&accept_mtx) == 0, ("%s: accept_mtx already locked", __func__))
#define	ACCEPT_UNLOCK()	KASSERT(pthread_mutex_unlock(&accept_mtx) == 0, ("%s: accept_mtx not locked", __func__))
#else
#define	ACCEPT_LOCK()   (void)pthread_mutex_lock(&accept_mtx)
#define	ACCEPT_UNLOCK() (void)pthread_mutex_unlock(&accept_mtx)
#endif
#define	ACCEPT_LOCK_ASSERT() \
          KASSERT(pthread_mutex_trylock(&accept_mtx) == EBUSY, ("%s: accept_mtx not locked", __func__))
#define	ACCEPT_UNLOCK_ASSERT() do {                                                               \
	  KASSERT(pthread_mutex_trylock(&accept_mtx) == 0, ("%s: accept_mtx  locked", __func__)); \
	  (void)pthread_mutex_unlock(&accept_mtx);                                                \
        } while (0)
#endif

#define	SOCKBUF_MTX(_sb) (&(_sb)->sb_mtx)
#if defined(_WIN32)
#define SOCKBUF_LOCK_INIT(_sb, _name) \
	InitializeCriticalSection(SOCKBUF_MTX(_sb))
#define SOCKBUF_LOCK_DESTROY(_sb) DeleteCriticalSection(SOCKBUF_MTX(_sb))
#define SOCKBUF_COND_INIT(_sb) InitializeConditionVariable((&(_sb)->sb_cond))
#define SOCKBUF_COND_DESTROY(_sb) DeleteConditionVariable((&(_sb)->sb_cond))
#define SOCK_COND_INIT(_so) InitializeConditionVariable((&(_so)->timeo_cond))
#define SOCK_COND_DESTROY(_so) DeleteConditionVariable((&(_so)->timeo_cond))
#define SOCK_COND(_so) (&(_so)->timeo_cond)
#else
#ifdef INVARIANTS
#define SOCKBUF_LOCK_INIT(_sb, _name) do {                                 \
	pthread_mutexattr_t mutex_attr;                                    \
	                                                                   \
	pthread_mutexattr_init(&mutex_attr);                               \
	pthread_mutexattr_settype(&mutex_attr, PTHREAD_MUTEX_ERRORCHECK);  \
	pthread_mutex_init(SOCKBUF_MTX(_sb), &mutex_attr);                 \
	pthread_mutexattr_destroy(&mutex_attr);                            \
} while (0)
#else
#define SOCKBUF_LOCK_INIT(_sb, _name) \
	pthread_mutex_init(SOCKBUF_MTX(_sb), NULL)
#endif
#define SOCKBUF_LOCK_DESTROY(_sb) pthread_mutex_destroy(SOCKBUF_MTX(_sb))
#define SOCKBUF_COND_INIT(_sb) pthread_cond_init((&(_sb)->sb_cond), NULL)
#define SOCKBUF_COND_DESTROY(_sb) pthread_cond_destroy((&(_sb)->sb_cond))
#define SOCK_COND_INIT(_so) pthread_cond_init((&(_so)->timeo_cond), NULL)
#define SOCK_COND_DESTROY(_so) pthread_cond_destroy((&(_so)->timeo_cond))
#define SOCK_COND(_so) (&(_so)->timeo_cond)
#endif

#define	SOCK_MTX(_so)			SOCKBUF_MTX(&(_so)->so_rcv)

#define	SOCK_LOCK_ASSERT(_so)		SOCKBUF_LOCK_ASSERT(&(_so)->so_rcv)

#define	SS_NOFDREF		0x0001
#define	SS_ISCONNECTED		0x0002
#define	SS_ISCONNECTING		0x0004
#define	SS_ISDISCONNECTING	0x0008
#define	SS_NBIO			0x0100
#define	SS_ASYNC		0x0200
#define	SS_ISCONFIRMING		0x0400
#define	SS_ISDISCONNECTED	0x2000

#define	SS_PROTOREF		0x4000

#define	SBS_CANTSENDMORE	0x0010
#define	SBS_CANTRCVMORE		0x0020
#define	SBS_RCVATMARK		0x0040

#define	SQ_INCOMP		0x0800
#define	SQ_COMP			0x1000

#define SCTP_EVENT_READ		0x0001
#define SCTP_EVENT_WRITE	0x0002
#define SCTP_EVENT_ERROR	0x0004

void	soisconnecting(struct socket *so);
void	soisdisconnecting(struct socket *so);
void	soisconnected(struct socket *so);
struct socket * sonewconn(struct socket *head, int connstatus);
void	socantrcvmore(struct socket *so);
void	socantsendmore(struct socket *so);
void	sofree(struct socket *so);

#define	soref(so) do {							\
	SOCK_LOCK_ASSERT(so);						\
	++(so)->so_count;						\
} while (0)

#define	sorele(so) do {							\
	ACCEPT_LOCK_ASSERT();						\
	SOCK_LOCK_ASSERT(so);						\
	KASSERT((so)->so_count > 0, ("sorele"));			\
	if (--(so)->so_count == 0)					\
		sofree(so);						\
	else {								\
		SOCK_UNLOCK(so);					\
		ACCEPT_UNLOCK();					\
	}								\
} while (0)

#define	sbspace(sb) \
    ((long) min((int)((sb)->sb_hiwat - (sb)->sb_cc), \
	 (int)((sb)->sb_mbmax - (sb)->sb_mbcnt)))

#define	sosendallatonce(so) \
    ((so)->so_proto->pr_flags & PR_ATOMIC)

#define	soreadable(so) \
    ((int)((so)->so_rcv.sb_cc) >= (so)->so_rcv.sb_lowat || \
	((so)->so_rcv.sb_state & SBS_CANTRCVMORE) || \
	!TAILQ_EMPTY(&(so)->so_comp) || (so)->so_error)

#if 0
#define PR_CONNREQUIRED 0x04
#define	sowriteable(so) \
    ((sbspace(&(so)->so_snd) >= (so)->so_snd.sb_lowat && \
	(((so)->so_state&SS_ISCONNECTED) || \
	  ((so)->so_proto->pr_flags&PR_CONNREQUIRED)==0)) || \
     ((so)->so_snd.sb_state & SBS_CANTSENDMORE) || \
     (so)->so_error)
#else

#define	sowriteable(so) \
    ((sbspace(&(so)->so_snd) >= (so)->so_snd.sb_lowat && \
      (((so)->so_state&SS_ISCONNECTED))) ||              \
     ((so)->so_snd.sb_state & SBS_CANTSENDMORE) || \
     (so)->so_error)
#endif

extern void solisten_proto(struct socket *so, int backlog);
extern int solisten_proto_check(struct socket *so);
extern int sctp_listen(struct socket *so, int backlog, struct proc *p);
extern void socantrcvmore_locked(struct socket *so);
extern int sctp_bind(struct socket *so, struct sockaddr *addr);
extern int sctp6_bind(struct socket *so, struct sockaddr *addr, void *proc);
extern int sctpconn_bind(struct socket *so, struct sockaddr *addr);
extern int sctp_accept(struct socket *so, struct sockaddr **addr);
extern int sctp_attach(struct socket *so, int proto, uint32_t vrf_id);
extern int sctp6_attach(struct socket *so, int proto, uint32_t vrf_id);
extern int sctp_abort(struct socket *so);
extern int sctp6_abort(struct socket *so);
extern void sctp_close(struct socket *so);
extern int soaccept(struct socket *so, struct sockaddr **nam);
extern int solisten(struct socket *so, int backlog);
extern int  soreserve(struct socket *so, u_long sndcc, u_long rcvcc);
extern void sowakeup(struct socket *so, struct sockbuf *sb);
extern void wakeup(void *ident, struct socket *so);
extern int uiomove(void *cp, int n, struct uio *uio);
extern int sbwait(struct sockbuf *sb);
extern int sodisconnect(struct socket *so);
extern int soconnect(struct socket *so, struct sockaddr *nam);
extern int sctp_disconnect(struct socket *so);
extern int sctp_connect(struct socket *so, struct sockaddr *addr);
extern int sctp6_connect(struct socket *so, struct sockaddr *addr);
extern int sctpconn_connect(struct socket *so, struct sockaddr *addr);
extern void sctp_finish(void);

#define	sb_notify(sb)	(((sb)->sb_flags & (SB_WAIT | SB_SEL | SB_ASYNC | \
    SB_UPCALL | SB_AIO | SB_KNOTE)) != 0)

#define	sorwakeup_locked(so) do {					\
	SOCKBUF_LOCK_ASSERT(&(so)->so_rcv);				\
	if (sb_notify(&(so)->so_rcv))					\
		sowakeup((so), &(so)->so_rcv);	 			\
	else								\
		SOCKBUF_UNLOCK(&(so)->so_rcv);				\
} while (0)

#define	sorwakeup(so) do {						\
	SOCKBUF_LOCK(&(so)->so_rcv);					\
	sorwakeup_locked(so);						\
} while (0)

#define	sowwakeup_locked(so) do {					\
	SOCKBUF_LOCK_ASSERT(&(so)->so_snd);				\
	if (sb_notify(&(so)->so_snd))					\
		sowakeup((so), &(so)->so_snd); 				\
	else								\
		SOCKBUF_UNLOCK(&(so)->so_snd);				\
} while (0)

#define	sowwakeup(so) do {						\
	SOCKBUF_LOCK(&(so)->so_snd);					\
	sowwakeup_locked(so);						\
} while (0)

#endif
