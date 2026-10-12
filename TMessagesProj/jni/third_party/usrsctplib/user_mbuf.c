/*-
 * Copyright (c) 1982, 1986, 1988, 1993
 *      The Regents of the University of California.
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

/*
 *  __Userspace__ version of /usr/src/sys/kern/kern_mbuf.c
 *  We are initializing two zones for Mbufs and Clusters.
 *
 */

#include <stdio.h>
#include <string.h>

#if !defined(SCTP_SIMPLE_ALLOCATOR)
#include "umem.h"
#endif
#include "user_mbuf.h"
#include "user_environment.h"
#include "user_atomic.h"
#include "netinet/sctp_pcb.h"

#define KIPC_MAX_LINKHDR        4
#define KIPC_MAX_PROTOHDR	5
int max_linkhdr = KIPC_MAX_LINKHDR;
int max_protohdr = KIPC_MAX_PROTOHDR;

sctp_zone_t	zone_mbuf;
sctp_zone_t	zone_clust;
sctp_zone_t	zone_ext_refcnt;

struct clust_args clust_mb_args;

static int	mb_ctor_mbuf(void *, void *, int);
static int      mb_ctor_clust(void *, void *, int);
static void	mb_dtor_mbuf(void *,  void *);
static void	mb_dtor_clust(void *, void *);

static int mbuf_constructor_dup(struct mbuf *m, int pkthdr, short type)
{
	int flags = pkthdr;

	m->m_next = NULL;
	m->m_nextpkt = NULL;
	m->m_len = 0;
	m->m_flags = flags;
	m->m_type = type;
	if (flags & M_PKTHDR) {
		m->m_data = m->m_pktdat;
		m->m_pkthdr.rcvif = NULL;
		m->m_pkthdr.len = 0;
		m->m_pkthdr.header = NULL;
		m->m_pkthdr.csum_flags = 0;
		m->m_pkthdr.csum_data = 0;
		m->m_pkthdr.tso_segsz = 0;
		m->m_pkthdr.ether_vtag = 0;
		SLIST_INIT(&m->m_pkthdr.tags);
	} else
		m->m_data = m->m_dat;

	return (0);
}

struct mbuf *
m_get(int how, short type)
{
	struct mbuf *mret;
#if defined(SCTP_SIMPLE_ALLOCATOR)
	struct mb_args mbuf_mb_args;

	mbuf_mb_args.flags = 0;
	mbuf_mb_args.type = type;
#endif

	mret = SCTP_ZONE_GET(zone_mbuf, struct mbuf);
#if defined(SCTP_SIMPLE_ALLOCATOR)
	mb_ctor_mbuf(mret, &mbuf_mb_args, 0);
#endif

	if (mret) {
#if USING_MBUF_CONSTRUCTOR
		if (! (mret->m_type == type) ) {
			mbuf_constructor_dup(mret, 0, type);
		}
#else
		mbuf_constructor_dup(mret, 0, type);
#endif

	}
	return mret;
}

struct mbuf *
m_gethdr(int how, short type)
{
	struct mbuf *mret;
#if defined(SCTP_SIMPLE_ALLOCATOR)
	struct mb_args mbuf_mb_args;

	mbuf_mb_args.flags = M_PKTHDR;
	mbuf_mb_args.type = type;
#endif
	mret = SCTP_ZONE_GET(zone_mbuf, struct mbuf);
#if defined(SCTP_SIMPLE_ALLOCATOR)
	mb_ctor_mbuf(mret, &mbuf_mb_args, 0);
#endif

	if (mret) {
#if USING_MBUF_CONSTRUCTOR
		if (! ((mret->m_flags & M_PKTHDR) && (mret->m_type == type)) ) {
			mbuf_constructor_dup(mret, M_PKTHDR, type);
		}
#else
		mbuf_constructor_dup(mret, M_PKTHDR, type);
#endif
	}
	return mret;
}

struct mbuf *
m_free(struct mbuf *m)
{

	struct mbuf *n = m->m_next;

	if (m->m_flags & M_EXT)
		mb_free_ext(m);
	else if ((m->m_flags & M_NOFREE) == 0) {
#if defined(SCTP_SIMPLE_ALLOCATOR)
		mb_dtor_mbuf(m, NULL);
#endif
		SCTP_ZONE_FREE(zone_mbuf, m);
	}

	return (n);
}

static void
clust_constructor_dup(caddr_t m_clust, struct mbuf* m)
{
	u_int *refcnt;
	int type, size;

	if (m == NULL) {
		return;
	}

	type = EXT_CLUSTER;
	size = MCLBYTES;

	refcnt = SCTP_ZONE_GET(zone_ext_refcnt, u_int);

#if !defined(SCTP_SIMPLE_ALLOCATOR)
	if (refcnt == NULL) {
		umem_reap();
		refcnt = SCTP_ZONE_GET(zone_ext_refcnt, u_int);

	}
#endif
	*refcnt = 1;
	m->m_ext.ext_buf = (caddr_t)m_clust;
	m->m_data = m->m_ext.ext_buf;
	m->m_flags |= M_EXT;
	m->m_ext.ext_free = NULL;
	m->m_ext.ext_args = NULL;
	m->m_ext.ext_size = size;
	m->m_ext.ext_type = type;
	m->m_ext.ref_cnt = refcnt;
	return;
}

void
m_clget(struct mbuf *m, int how)
{
	caddr_t mclust_ret;
#if defined(SCTP_SIMPLE_ALLOCATOR)
	struct clust_args clust_mb_args_l;
#endif
	if (m->m_flags & M_EXT) {
		SCTPDBG(SCTP_DEBUG_USR, "%s: %p mbuf already has cluster\n", __func__, (void *)m);
	}
	m->m_ext.ext_buf = (char *)NULL;
#if defined(SCTP_SIMPLE_ALLOCATOR)
	clust_mb_args_l.parent_mbuf = m;
#endif
	mclust_ret = SCTP_ZONE_GET(zone_clust, char);
#if defined(SCTP_SIMPLE_ALLOCATOR)
	mb_ctor_clust(mclust_ret, &clust_mb_args_l, 0);
#endif

	if (mclust_ret == NULL) {
#if !defined(SCTP_SIMPLE_ALLOCATOR)

		umem_reap();
		mclust_ret = SCTP_ZONE_GET(zone_clust, char);
#endif

		SCTPDBG(SCTP_DEBUG_USR, "Memory allocation failure in %s\n", __func__);

	}

#if USING_MBUF_CONSTRUCTOR
	if ((m->m_ext.ext_buf == NULL)) {
		clust_constructor_dup(mclust_ret, m);
	}
#else
	clust_constructor_dup(mclust_ret, m);
#endif
}

struct mbuf *
m_getm2(struct mbuf *m, int len, int how, short type, int flags, int allonebuf)
{
	struct mbuf *mb, *nm = NULL, *mtail = NULL;
	int size, mbuf_threshold, space_needed = len;

	KASSERT(len >= 0, ("%s: len is < 0", __func__));

	flags &= (M_PKTHDR | M_EOR);

	if ((flags & M_PKTHDR) && m != NULL) {
		flags &= ~M_PKTHDR;
	}

	if (allonebuf == 0)
		mbuf_threshold = SCTP_BASE_SYSCTL(sctp_mbuf_threshold_count);
	else
		mbuf_threshold = 1;

	while (len > 0) {
		if ((!allonebuf && len >= MCLBYTES) || (len > (int)(((mbuf_threshold - 1) * MLEN) + MHLEN))) {
			mb = m_gethdr(how, type);
			MCLGET(mb, how);
			size = MCLBYTES;

		} else if (flags & M_PKTHDR) {
			mb = m_gethdr(how, type);
			if (len < MHLEN) {
				size = len;
			} else {
				size = MHLEN;
			}
		} else {
			mb = m_get(how, type);
			if (len < MLEN) {
				size = len;
			} else {
				size = MLEN;
			}
		}

		if (mb == NULL) {
			if (nm != NULL)
				m_freem(nm);
			return (NULL);
		}

		if (allonebuf != 0 && size < space_needed) {
			m_freem(mb);
			return (NULL);
		}

		len -= size;
		if (mtail != NULL)
			mtail->m_next = mb;
		else
			nm = mb;
		mtail = mb;
		flags &= ~M_PKTHDR;
	}
	if (flags & M_EOR) {
		mtail->m_flags |= M_EOR;
	}

	if (m != NULL) {
		for (mtail = m; mtail->m_next != NULL; mtail = mtail->m_next);
		mtail->m_next = nm;
		mtail->m_flags &= ~M_EOR;
	} else {
		m = nm;
	}

	return (m);
}

struct mbuf *
m_uiotombuf(struct uio *uio, int how, int len, int align, int flags)
{
	struct mbuf *m, *mb;
	int error, length;
	ssize_t total;
	int progress = 0;

	if (len > 0)
		total = min(uio->uio_resid, len);
	else
		total = uio->uio_resid;

	if (align >= MHLEN)
		return (NULL);

	m = m_getm2(NULL, (int)max(total + align, 1), how, MT_DATA, flags, 0);
	if (m == NULL)
		return (NULL);
	m->m_data += align;

	for (mb = m; mb != NULL; mb = mb->m_next) {
		length = (int)min(M_TRAILINGSPACE(mb), total - progress);
		error = uiomove(mtod(mb, void *), length, uio);
		if (error) {
			m_freem(m);
			return (NULL);
		}

		mb->m_len = length;
		progress += length;
		if (flags & M_PKTHDR)
			m->m_pkthdr.len += length;
	}
	KASSERT(progress == total, ("%s: progress != total", __func__));

	return (m);
}

u_int
m_length(struct mbuf *m0, struct mbuf **last)
{
	struct mbuf *m;
	u_int len;

	len = 0;
	for (m = m0; m != NULL; m = m->m_next) {
		len += m->m_len;
		if (m->m_next == NULL)
			break;
	}
	if (last != NULL)
	*last = m;
	return (len);
}

struct mbuf *
m_last(struct mbuf *m)
{
	while (m->m_next) {
		m = m->m_next;
	}
	return (m);
}

static __inline void
m_tag_unlink(struct mbuf *m, struct m_tag *t)
{

	SLIST_REMOVE(&m->m_pkthdr.tags, t, m_tag, m_tag_link);
}

static __inline void
m_tag_free(struct m_tag *t)
{

	(*t->m_tag_free)(t);
}

static __inline void
m_tag_setup(struct m_tag *t, uint32_t cookie, int type, int len)
{

	t->m_tag_id = type;
	t->m_tag_len = len;
	t->m_tag_cookie = cookie;
}

void
mbuf_initialize(void *dummy)
{

#if defined(SCTP_SIMPLE_ALLOCATOR)
	SCTP_ZONE_INIT(zone_mbuf, MBUF_MEM_NAME, MSIZE, 0);
#else
	zone_mbuf = umem_cache_create(MBUF_MEM_NAME, MSIZE, 0,
	                              mb_ctor_mbuf, mb_dtor_mbuf, NULL,
	                              NULL,
	                              NULL, 0);
#endif

	SCTP_ZONE_INIT(zone_ext_refcnt, MBUF_EXTREFCNT_MEM_NAME, sizeof(u_int), 0);

#if defined(SCTP_SIMPLE_ALLOCATOR)
	SCTP_ZONE_INIT(zone_clust, MBUF_CLUSTER_MEM_NAME, MCLBYTES, 0);
#else
	zone_clust = umem_cache_create(MBUF_CLUSTER_MEM_NAME, MCLBYTES, 0,
								   mb_ctor_clust, mb_dtor_clust, NULL,
								   &clust_mb_args,
								   NULL, 0);
#endif

}

static int
mb_ctor_mbuf(void *mem, void *arg, int flgs)
{
#if USING_MBUF_CONSTRUCTOR
	struct mbuf *m;
	struct mb_args *args;

	int flags;
	short type;

	m = (struct mbuf *)mem;
	args = (struct mb_args *)arg;
	flags = args->flags;
	type = args->type;

	m->m_next = NULL;
	m->m_nextpkt = NULL;
	m->m_len = 0;
	m->m_flags = flags;
	m->m_type = type;
	if (flags & M_PKTHDR) {
		m->m_data = m->m_pktdat;
		m->m_pkthdr.rcvif = NULL;
		m->m_pkthdr.len = 0;
		m->m_pkthdr.header = NULL;
		m->m_pkthdr.csum_flags = 0;
		m->m_pkthdr.csum_data = 0;
		m->m_pkthdr.tso_segsz = 0;
		m->m_pkthdr.ether_vtag = 0;
		SLIST_INIT(&m->m_pkthdr.tags);
	} else
		m->m_data = m->m_dat;
#endif
	return (0);
}

static void
mb_dtor_mbuf(void *mem, void *arg)
{
	struct mbuf *m;

	m = (struct mbuf *)mem;
	if ((m->m_flags & M_PKTHDR) != 0) {
		m_tag_delete_chain(m, NULL);
	}
}

static int
mb_ctor_clust(void *mem, void *arg, int flgs)
{

#if USING_MBUF_CONSTRUCTOR
	struct mbuf *m;
	struct clust_args * cla;
	u_int *refcnt;
	int type, size;
	sctp_zone_t zone;

	type = EXT_CLUSTER;
	zone = zone_clust;
	size = MCLBYTES;

	cla = (struct clust_args *)arg;
	m = cla->parent_mbuf;

	refcnt = SCTP_ZONE_GET(zone_ext_refcnt, u_int);

	*refcnt = 1;

	if (m != NULL) {
		m->m_ext.ext_buf = (caddr_t)mem;
		m->m_data = m->m_ext.ext_buf;
		m->m_flags |= M_EXT;
		m->m_ext.ext_free = NULL;
		m->m_ext.ext_args = NULL;
		m->m_ext.ext_size = size;
		m->m_ext.ext_type = type;
		m->m_ext.ref_cnt = refcnt;
	}
#endif
	return (0);
}

static void
mb_dtor_clust(void *mem, void *arg)
{

}

void
m_tag_delete(struct mbuf *m, struct m_tag *t)
{
	KASSERT(m && t, ("m_tag_delete: null argument, m %p t %p", (void *)m, (void *)t));
	m_tag_unlink(m, t);
	m_tag_free(t);
}

void
m_tag_delete_chain(struct mbuf *m, struct m_tag *t)
{

	struct m_tag *p, *q;

	KASSERT(m, ("m_tag_delete_chain: null mbuf"));
	if (t != NULL)
		p = t;
	else
		p = SLIST_FIRST(&m->m_pkthdr.tags);
	if (p == NULL)
		return;
	while ((q = SLIST_NEXT(p, m_tag_link)) != NULL)
		m_tag_delete(m, q);
	m_tag_delete(m, p);
}

#if 0
static void
sctp_print_mbuf_chain(struct mbuf *m)
{
	SCTP_DEBUG_USR(SCTP_DEBUG_USR, "Printing mbuf chain %p.\n", (void *)m);
	for(; m; m=m->m_next) {
		SCTP_DEBUG_USR(SCTP_DEBUG_USR, "%p: m_len = %ld, m_type = %x, m_next = %p.\n", (void *)m, m->m_len, m->m_type, (void *)m->m_next);
		if (m->m_flags & M_EXT)
			SCTP_DEBUG_USR(SCTP_DEBUG_USR, "%p: extend_size = %d, extend_buffer = %p, ref_cnt = %d.\n", (void *)m, m->m_ext.ext_size, (void *)m->m_ext.ext_buf, *(m->m_ext.ref_cnt));
	}
}
#endif

void
m_freem(struct mbuf *mb)
{
	while (mb != NULL)
		mb = m_free(mb);
}

void
mb_free_ext(struct mbuf *m)
{

	int skipmbuf;

	KASSERT((m->m_flags & M_EXT) == M_EXT, ("%s: M_EXT not set", __func__));
	KASSERT(m->m_ext.ref_cnt != NULL, ("%s: ref_cnt not set", __func__));

	skipmbuf = (m->m_flags & M_NOFREE);

#ifdef IPHONE
	if (atomic_fetchadd_int(m->m_ext.ref_cnt, -1) == 0)
#else
	if (SCTP_DECREMENT_AND_CHECK_REFCOUNT(m->m_ext.ref_cnt))
#endif
	{
		if (m->m_ext.ext_type == EXT_CLUSTER){
#if defined(SCTP_SIMPLE_ALLOCATOR)
			mb_dtor_clust(m->m_ext.ext_buf, &clust_mb_args);
#endif
			SCTP_ZONE_FREE(zone_clust, m->m_ext.ext_buf);
			SCTP_ZONE_FREE(zone_ext_refcnt, (u_int*)m->m_ext.ref_cnt);
			m->m_ext.ref_cnt = NULL;
		}
	}

	if (skipmbuf)
		return;

	m->m_ext.ext_buf = NULL;
	m->m_ext.ext_free = NULL;
	m->m_ext.ext_args = NULL;
	m->m_ext.ref_cnt = NULL;
	m->m_ext.ext_size = 0;
	m->m_ext.ext_type = 0;
	m->m_flags &= ~M_EXT;
#if defined(SCTP_SIMPLE_ALLOCATOR)
	mb_dtor_mbuf(m, NULL);
#endif
	SCTP_ZONE_FREE(zone_mbuf, m);

}

void
m_move_pkthdr(struct mbuf *to, struct mbuf *from)
{

	to->m_flags = (from->m_flags & M_COPYFLAGS) | (to->m_flags & M_EXT);
	if ((to->m_flags & M_EXT) == 0)
		to->m_data = to->m_pktdat;
	to->m_pkthdr = from->m_pkthdr;
	SLIST_INIT(&from->m_pkthdr.tags);
	from->m_flags &= ~M_PKTHDR;
}

struct mbuf *
m_pullup(struct mbuf *n, int len)
{
	struct mbuf *m;
	int count;
	int space;

	if ((n->m_flags & M_EXT) == 0 &&
	    n->m_data + len < &n->m_dat[MLEN] && n->m_next) {
		if (n->m_len >= len)
			return (n);
		m = n;
		n = n->m_next;
		len -= m->m_len;
	} else {
		if (len > MHLEN)
			goto bad;
		MGET(m, M_NOWAIT, n->m_type);
		if (m == NULL)
			goto bad;
		m->m_len = 0;
		if (n->m_flags & M_PKTHDR)
			M_MOVE_PKTHDR(m, n);
	}
	space = (int)(&m->m_dat[MLEN] - (m->m_data + m->m_len));
	do {
		count = min(min(max(len, max_protohdr), space), n->m_len);
		memcpy(mtod(m, caddr_t) + m->m_len,mtod(n, caddr_t), (u_int)count);
		len -= count;
		m->m_len += count;
		n->m_len -= count;
		space -= count;
		if (n->m_len)
			n->m_data += count;
		else
			n = m_free(n);
	} while (len > 0 && n);
	if (len > 0) {
		(void) m_free(m);
		goto bad;
	}
	m->m_next = n;
	return (m);
bad:
	m_freem(n);
	return (NULL);
}

static struct mbuf *
m_dup1(struct mbuf *m, int off, int len, int wait)
{
	struct mbuf *n = NULL;
	int copyhdr;

	if (len > MCLBYTES)
		return NULL;
	if (off == 0 && (m->m_flags & M_PKTHDR) != 0)
		copyhdr = 1;
	else
		copyhdr = 0;
	if (len >= MINCLSIZE) {
		if (copyhdr == 1) {
			m_clget(n, wait);
			m_dup_pkthdr(n, m, wait);
		} else
			m_clget(n, wait);
	} else {
		if (copyhdr == 1)
			n = m_gethdr(wait, m->m_type);
		else
			n = m_get(wait, m->m_type);
	}
	if (!n)
		return NULL;

	if (copyhdr && !m_dup_pkthdr(n, m, wait)) {
		m_free(n);
		return NULL;
	}
	m_copydata(m, off, len, mtod(n, caddr_t));
	n->m_len = len;
	return n;
}

struct mbuf *
m_pulldown(struct mbuf *m, int off, int len, int *offp)
{
	struct mbuf *n, *o;
	int hlen, tlen, olen;
	int writable;

	KASSERT(m, ("m == NULL in m_pulldown()"));
	if (len > MCLBYTES) {
		m_freem(m);
		return NULL;
	}

#ifdef PULLDOWN_DEBUG
	{
		struct mbuf *t;
		SCTP_DEBUG_USR(SCTP_DEBUG_USR, "before:");
		for (t = m; t; t = t->m_next)
			SCTP_DEBUG_USR(SCTP_DEBUG_USR, " %d", t->m_len);
		SCTP_DEBUG_USR(SCTP_DEBUG_USR, "\n");
	}
#endif
	n = m;
	while (n != NULL && off > 0) {
		if (n->m_len > off)
			break;
		off -= n->m_len;
		n = n->m_next;
	}

	while (n != NULL && n->m_len == 0)
		n = n->m_next;
	if (!n) {
		m_freem(m);
		return NULL;
	}

	writable = 0;
	if ((n->m_flags & M_EXT) == 0 ||
	    (n->m_ext.ext_type == EXT_CLUSTER && M_WRITABLE(n)))
		writable = 1;

	if ((off == 0 || offp) && len <= n->m_len - off && writable)
		goto ok;

	if (len <= n->m_len - off) {
		o = m_dup1(n, off, n->m_len - off, M_NOWAIT);
		if (o == NULL) {
			m_freem(m);
		return NULL;
		}
		n->m_len = off;
		o->m_next = n->m_next;
		n->m_next = o;
		n = n->m_next;
		off = 0;
		goto ok;
	}

	hlen = n->m_len - off;
	tlen = len - hlen;

	olen = 0;
	for (o = n->m_next; o != NULL; o = o->m_next)
		olen += o->m_len;
	if (hlen + olen < len) {
		m_freem(m);
		return NULL;
	}

	if ((off == 0 || offp) && (M_TRAILINGSPACE(n) >= tlen) && writable) {
		m_copydata(n->m_next, 0, tlen, mtod(n, caddr_t) + n->m_len);
		n->m_len += tlen;
		m_adj(n->m_next, tlen);
		goto ok;
	}

	if ((off == 0 || offp) && (M_LEADINGSPACE(n->m_next) >= hlen) && writable) {
		n->m_next->m_data -= hlen;
		n->m_next->m_len += hlen;
		memcpy( mtod(n->m_next, caddr_t), mtod(n, caddr_t) + off,hlen);
		n->m_len -= hlen;
		n = n->m_next;
		off = 0;
		goto ok;
	}

	if (len > MLEN)
		m_clget(o, M_NOWAIT);

	else
		o = m_get(M_NOWAIT, m->m_type);
	if (!o) {
		m_freem(m);
		return NULL;
	}

	o->m_len = hlen;
	memcpy(mtod(o, caddr_t), mtod(n, caddr_t) + off, hlen);
	n->m_len -= hlen;

	m_copydata(n->m_next, 0, tlen, mtod(o, caddr_t) + o->m_len);
	o->m_len += tlen;
	m_adj(n->m_next, tlen);
	o->m_next = n->m_next;
	n->m_next = o;
	n = o;
	off = 0;
ok:
#ifdef PULLDOWN_DEBUG
	{
		struct mbuf *t;
		SCTP_DEBUG_USR(SCTP_DEBUG_USR, "after:");
		for (t = m; t; t = t->m_next)
			SCTP_DEBUG_USR(SCTP_DEBUG_USR, "%c%d", t == n ? '*' : ' ', t->m_len);
		SCTP_DEBUG_USR(SCTP_DEBUG_USR, " (off=%d)\n", off);
	}
#endif
	if (offp)
		*offp = off;
	return n;
}

static void
mb_dupcl(struct mbuf *n, struct mbuf *m)
{
	KASSERT((m->m_flags & M_EXT) == M_EXT, ("%s: M_EXT not set", __func__));
	KASSERT(m->m_ext.ref_cnt != NULL, ("%s: ref_cnt not set", __func__));
	KASSERT((n->m_flags & M_EXT) == 0, ("%s: M_EXT set", __func__));

	if (*(m->m_ext.ref_cnt) == 1)
		*(m->m_ext.ref_cnt) += 1;
	else
		atomic_add_int(m->m_ext.ref_cnt, 1);
	n->m_ext.ext_buf = m->m_ext.ext_buf;
	n->m_ext.ext_free = m->m_ext.ext_free;
	n->m_ext.ext_args = m->m_ext.ext_args;
	n->m_ext.ext_size = m->m_ext.ext_size;
	n->m_ext.ref_cnt = m->m_ext.ref_cnt;
	n->m_ext.ext_type = m->m_ext.ext_type;
	n->m_flags |= M_EXT;
}

struct mbuf *
m_copym(struct mbuf *m, int off0, int len, int wait)
{
	struct mbuf *n, **np;
	int off = off0;
	struct mbuf *top;
	int copyhdr = 0;

	KASSERT(off >= 0, ("m_copym, negative off %d", off));
	KASSERT(len >= 0, ("m_copym, negative len %d", len));
	KASSERT(m != NULL, ("m_copym, m is NULL"));

#if !defined(INVARIANTS)
	if (m == NULL) {
		return (NULL);
	}
#endif
	if (off == 0 && m->m_flags & M_PKTHDR)
		copyhdr = 1;
	while (off > 0) {
		KASSERT(m != NULL, ("m_copym, offset > size of mbuf chain"));
		if (off < m->m_len)
			break;
		off -= m->m_len;
		m = m->m_next;
	}
	np = &top;
	top = 0;
	while (len > 0) {
		if (m == NULL) {
			KASSERT(len == M_COPYALL, ("m_copym, length > size of mbuf chain"));
			break;
		}
		if (copyhdr)
			MGETHDR(n, wait, m->m_type);
		else
			MGET(n, wait, m->m_type);
		*np = n;
		if (n == NULL)
			goto nospace;
		if (copyhdr) {
			if (!m_dup_pkthdr(n, m, wait))
				goto nospace;
			if (len == M_COPYALL)
				n->m_pkthdr.len -= off0;
			else
				n->m_pkthdr.len = len;
			copyhdr = 0;
		}
		n->m_len = min(len, m->m_len - off);
		if (m->m_flags & M_EXT) {
			n->m_data = m->m_data + off;
			mb_dupcl(n, m);
		} else
			memcpy(mtod(n, caddr_t), mtod(m, caddr_t) + off, (u_int)n->m_len);
		if (len != M_COPYALL)
			len -= n->m_len;
		off = 0;
		m = m->m_next;
		np = &n->m_next;
	}

	return (top);
nospace:
	m_freem(top);
	return (NULL);
}

int
m_tag_copy_chain(struct mbuf *to, struct mbuf *from, int how)
{
	struct m_tag *p, *t, *tprev = NULL;

	KASSERT(to && from, ("m_tag_copy_chain: null argument, to %p from %p", (void *)to, (void *)from));
	m_tag_delete_chain(to, NULL);
	SLIST_FOREACH(p, &from->m_pkthdr.tags, m_tag_link) {
		t = m_tag_copy(p, how);
		if (t == NULL) {
			m_tag_delete_chain(to, NULL);
			return 0;
		}
		if (tprev == NULL)
			SLIST_INSERT_HEAD(&to->m_pkthdr.tags, t, m_tag_link);
		else
			SLIST_INSERT_AFTER(tprev, t, m_tag_link);
		tprev = t;
	}
	return 1;
}

int
m_dup_pkthdr(struct mbuf *to, struct mbuf *from, int how)
{

	KASSERT(to, ("m_dup_pkthdr: to is NULL"));
	KASSERT(from, ("m_dup_pkthdr: from is NULL"));
	to->m_flags = (from->m_flags & M_COPYFLAGS) | (to->m_flags & M_EXT);
	if ((to->m_flags & M_EXT) == 0)
		to->m_data = to->m_pktdat;
	to->m_pkthdr = from->m_pkthdr;
	SLIST_INIT(&to->m_pkthdr.tags);
	return (m_tag_copy_chain(to, from, MBTOM(how)));
}

struct m_tag *
m_tag_copy(struct m_tag *t, int how)
{
	struct m_tag *p;

	KASSERT(t, ("m_tag_copy: null tag"));
	p = m_tag_alloc(t->m_tag_cookie, t->m_tag_id, t->m_tag_len, how);
	if (p == NULL)
		return (NULL);
	memcpy(p + 1, t + 1, t->m_tag_len);
	return p;
}

struct m_tag *
m_tag_alloc(uint32_t cookie, int type, int len, int wait)
{
	struct m_tag *t;

	if (len < 0)
		return NULL;
	t = malloc(len + sizeof(struct m_tag));
	if (t == NULL)
		return NULL;
	m_tag_setup(t, cookie, type, len);
	t->m_tag_free = m_tag_free_default;
	return t;
}

void
m_tag_free_default(struct m_tag *t)
{
  free(t);
}

void
m_copyback(struct mbuf *m0, int off, int len, caddr_t cp)
{
	int mlen;
	struct mbuf *m = m0, *n;
	int totlen = 0;

	if (m0 == NULL)
		return;
	while (off > (mlen = m->m_len)) {
		off -= mlen;
		totlen += mlen;
		if (m->m_next == NULL) {
			n = m_get(M_NOWAIT, m->m_type);
			if (n == NULL)
				goto out;
			memset(mtod(n, caddr_t), 0, MLEN);
			n->m_len = min(MLEN, len + off);
			m->m_next = n;
		}
		m = m->m_next;
	}
	while (len > 0) {
		mlen = min (m->m_len - off, len);
		memcpy(off + mtod(m, caddr_t), cp, (u_int)mlen);
		cp += mlen;
		len -= mlen;
		mlen += off;
		off = 0;
		totlen += mlen;
		if (len == 0)
			break;
		if (m->m_next == NULL) {
			n = m_get(M_NOWAIT, m->m_type);
			if (n == NULL)
				break;
			n->m_len = min(MLEN, len);
			m->m_next = n;
		}
		m = m->m_next;
	}
out:	if (((m = m0)->m_flags & M_PKTHDR) && (m->m_pkthdr.len < totlen))
		m->m_pkthdr.len = totlen;
}

struct mbuf *
m_prepend(struct mbuf *m, int len, int how)
{
	struct mbuf *mn;

	if (m->m_flags & M_PKTHDR)
		MGETHDR(mn, how, m->m_type);
	else
		MGET(mn, how, m->m_type);
	if (mn == NULL) {
		m_freem(m);
		return (NULL);
	}
	if (m->m_flags & M_PKTHDR)
		M_MOVE_PKTHDR(mn, m);
	mn->m_next = m;
	m = mn;
	if (m->m_flags & M_PKTHDR) {
		if (len < MHLEN)
			MH_ALIGN(m, len);
	} else {
		if (len < MLEN)
			M_ALIGN(m, len);
	}
	m->m_len = len;
	return (m);
}

void
m_copydata(const struct mbuf *m, int off, int len, caddr_t cp)
{
	u_int count;

	KASSERT(off >= 0, ("m_copydata, negative off %d", off));
	KASSERT(len >= 0, ("m_copydata, negative len %d", len));
	while (off > 0) {
		KASSERT(m != NULL, ("m_copydata, offset > size of mbuf chain"));
		if (off < m->m_len)
			break;
		off -= m->m_len;
		m = m->m_next;
	}
	while (len > 0) {
		KASSERT(m != NULL, ("m_copydata, length > size of mbuf chain"));
		count = min(m->m_len - off, len);
		memcpy(cp, mtod(m, caddr_t) + off, count);
		len -= count;
		cp += count;
		off = 0;
		m = m->m_next;
	}
}

void
m_cat(struct mbuf *m, struct mbuf *n)
{
	while (m->m_next)
		m = m->m_next;
	while (n) {
		if (m->m_flags & M_EXT ||
		    m->m_data + m->m_len + n->m_len >= &m->m_dat[MLEN]) {

			m->m_next = n;
			return;
		}

		memcpy(mtod(m, caddr_t) + m->m_len, mtod(n, caddr_t), (u_int)n->m_len);
		m->m_len += n->m_len;
		n = m_free(n);
	}
}

void
m_adj(struct mbuf *mp, int req_len)
{
	int len = req_len;
	struct mbuf *m;
	int count;

	if ((m = mp) == NULL)
		return;
	if (len >= 0) {

		while (m != NULL && len > 0) {
			if (m->m_len <= len) {
				len -= m->m_len;
				m->m_len = 0;
				m = m->m_next;
			} else {
				m->m_len -= len;
				m->m_data += len;
				len = 0;
			}
		}
		m = mp;
		if (mp->m_flags & M_PKTHDR)
			m->m_pkthdr.len -= (req_len - len);
	} else {

		len = -len;
		count = 0;
		for (;;) {
			count += m->m_len;
			if (m->m_next == (struct mbuf *)0)
				break;
			m = m->m_next;
		}
		if (m->m_len >= len) {
			m->m_len -= len;
			if (mp->m_flags & M_PKTHDR)
				mp->m_pkthdr.len -= len;
			return;
		}
		count -= len;
		if (count < 0)
			count = 0;

		m = mp;
		if (m->m_flags & M_PKTHDR)
			m->m_pkthdr.len = count;
		for (; m; m = m->m_next) {
			if (m->m_len >= count) {
				m->m_len = count;
				if (m->m_next != NULL) {
					m_freem(m->m_next);
					m->m_next = NULL;
				}
				break;
			}
			count -= m->m_len;
		}
	}
}

struct mbuf *
m_split(struct mbuf *m0, int len0, int wait)
{
	struct mbuf *m, *n;
	u_int len = len0, remain;

	for (m = m0; m && (int)len > m->m_len; m = m->m_next)
		len -= m->m_len;
	if (m == NULL)
		return (NULL);
	remain = m->m_len - len;
	if (m0->m_flags & M_PKTHDR) {
		MGETHDR(n, wait, m0->m_type);
		if (n == NULL)
			return (NULL);
		n->m_pkthdr.rcvif = m0->m_pkthdr.rcvif;
		n->m_pkthdr.len = m0->m_pkthdr.len - len0;
		m0->m_pkthdr.len = len0;
		if (m->m_flags & M_EXT)
			goto extpacket;
		if (remain > MHLEN) {

			MH_ALIGN(n, 0);
			n->m_next = m_split(m, len, wait);
			if (n->m_next == NULL) {
				(void) m_free(n);
				return (NULL);
			} else {
				n->m_len = 0;
				return (n);
			}
		} else
			MH_ALIGN(n, remain);
	} else if (remain == 0) {
		n = m->m_next;
		m->m_next = NULL;
		return (n);
	} else {
		MGET(n, wait, m->m_type);
		if (n == NULL)
			return (NULL);
		M_ALIGN(n, remain);
	}
extpacket:
	if (m->m_flags & M_EXT) {
		n->m_data = m->m_data + len;
		mb_dupcl(n, m);
	} else {
		memcpy(mtod(n, caddr_t), mtod(m, caddr_t) + len, remain);
	}
	n->m_len = remain;
	m->m_len = len;
	n->m_next = m->m_next;
	m->m_next = NULL;
	return (n);
}

int
pack_send_buffer(caddr_t buffer, struct mbuf* mb){

	int count_to_copy;
	int total_count_copied = 0;
	int offset = 0;

	do {
		count_to_copy = mb->m_len;
		memcpy(buffer+offset, mtod(mb, caddr_t), count_to_copy);
		offset += count_to_copy;
		total_count_copied += count_to_copy;
		mb = mb->m_next;
	} while(mb);

	return (total_count_copied);
}
