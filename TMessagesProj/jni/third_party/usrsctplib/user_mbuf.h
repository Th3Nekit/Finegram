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

#ifndef _USER_MBUF_H_
#define _USER_MBUF_H_

#include <stdio.h>
#if !defined(SCTP_SIMPLE_ALLOCATOR)
#include "umem.h"
#endif
#include "user_malloc.h"
#include "netinet/sctp_os_userspace.h"

#define USING_MBUF_CONSTRUCTOR 0

#ifndef MSIZE
#define MSIZE 256

#endif
#ifndef MCLBYTES
#define MCLBYTES 2048
#endif

struct mbuf * m_gethdr(int how, short type);
struct mbuf * m_get(int how, short type);
struct mbuf * m_free(struct mbuf *m);
void m_clget(struct mbuf *m, int how);
struct mbuf * m_getm2(struct mbuf *m, int len, int how, short type, int flags, int allonebuf);
struct mbuf *m_uiotombuf(struct uio *uio, int how, int len, int align, int flags);
u_int m_length(struct mbuf *m0, struct mbuf **last);
struct mbuf *m_last(struct mbuf *m);

void mbuf_initialize(void *);

#define	M_MOVE_PKTHDR(to, from)	m_move_pkthdr((to), (from))
#define	MGET(m, how, type)	((m) = m_get((how), (type)))
#define	MGETHDR(m, how, type)	((m) = m_gethdr((how), (type)))
#define	MCLGET(m, how)		m_clget((m), (how))

#define M_HDR_PAD ((sizeof(intptr_t)==4) ? 2 : 6)

#define	M_COPYALL	1000000000

#if defined(SCTP_SIMPLE_ALLOCATOR)
typedef size_t sctp_zone_t;
#else
typedef umem_cache_t *sctp_zone_t;
#endif

extern sctp_zone_t zone_mbuf;
extern sctp_zone_t zone_clust;
extern sctp_zone_t zone_ext_refcnt;

#define	mtod(m, t)	((t)((m)->m_data))
#define	dtom(x)		((struct mbuf *)((intptr_t)(x) & ~(MSIZE-1)))

struct mb_args {
	int	flags;
	short	type;
};

struct clust_args {
	struct mbuf * parent_mbuf;
};

struct mbuf *    m_split(struct mbuf *, int, int);
void             m_cat(struct mbuf *m, struct mbuf *n);
void		 m_adj(struct mbuf *, int);
void  mb_free_ext(struct mbuf *);
void  m_freem(struct mbuf *);
struct m_tag	*m_tag_alloc(uint32_t, int, int, int);
struct mbuf	*m_copym(struct mbuf *, int, int, int);
void		 m_copyback(struct mbuf *, int, int, caddr_t);
struct mbuf	*m_pullup(struct mbuf *, int);
struct mbuf	*m_pulldown(struct mbuf *, int off, int len, int *offp);
int		 m_dup_pkthdr(struct mbuf *, struct mbuf *, int);
struct m_tag	*m_tag_copy(struct m_tag *, int);
int		 m_tag_copy_chain(struct mbuf *, struct mbuf *, int);
struct mbuf	*m_prepend(struct mbuf *, int, int);
void		 m_copydata(const struct mbuf *, int, int, caddr_t);

#define MBUF_MEM_NAME "mbuf"
#define MBUF_CLUSTER_MEM_NAME "mbuf_cluster"
#define	MBUF_EXTREFCNT_MEM_NAME	"mbuf_ext_refcnt"

#define	MLEN		((int)(MSIZE - sizeof(struct m_hdr)))
#define	MHLEN		((int)(MLEN - sizeof(struct pkthdr)))
#define	MINCLSIZE	((int)(MHLEN + 1))
#define	M_MAXCOMPRESS	(MHLEN / 2)

struct m_hdr {
	struct mbuf	*mh_next;
	struct mbuf	*mh_nextpkt;
	caddr_t		 mh_data;
	int		 mh_len;
	int		 mh_flags;
	short		 mh_type;
	uint8_t          pad[M_HDR_PAD];
};

struct m_tag {
	SLIST_ENTRY(m_tag)	m_tag_link;
	uint16_t		m_tag_id;
	uint16_t		m_tag_len;
	uint32_t		m_tag_cookie;
	void			(*m_tag_free)(struct m_tag *);
};

struct pkthdr {
	struct ifnet	*rcvif;

	void		*header;
	int		 len;

	int		 csum_flags;
	int		 csum_data;
	uint16_t	 tso_segsz;
	uint16_t	 ether_vtag;
	SLIST_HEAD(packet_tags, m_tag) tags;
};

struct m_ext {
	caddr_t		 ext_buf;
	void		(*ext_free)
			    (void *, void *);
	void		*ext_args;
	u_int		 ext_size;
	volatile u_int	*ref_cnt;
	int		 ext_type;
};

struct mbuf {
	struct m_hdr	m_hdr;
	union {
		struct {
			struct pkthdr	MH_pkthdr;
			union {
				struct m_ext	MH_ext;
				char		MH_databuf[MHLEN];
			} MH_dat;
		} MH;
		char	M_databuf[MLEN];
	} M_dat;
};

#define	m_next		m_hdr.mh_next
#define	m_len		m_hdr.mh_len
#define	m_data		m_hdr.mh_data
#define	m_type		m_hdr.mh_type
#define	m_flags		m_hdr.mh_flags
#define	m_nextpkt	m_hdr.mh_nextpkt
#define	m_act		m_nextpkt
#define	m_pkthdr	M_dat.MH.MH_pkthdr
#define	m_ext		M_dat.MH.MH_dat.MH_ext
#define	m_pktdat	M_dat.MH.MH_dat.MH_databuf
#define	m_dat		M_dat.M_databuf

#define	M_EXT		0x0001
#define	M_PKTHDR	0x0002
#define	M_EOR		0x0004
#define	M_RDONLY	0x0008
#define	M_PROTO1	0x0010
#define	M_PROTO2	0x0020
#define	M_PROTO3	0x0040
#define	M_PROTO4	0x0080
#define	M_PROTO5	0x0100
#define	M_FREELIST	0x8000

#define	M_COPYFLAGS	(M_PKTHDR|M_EOR|M_RDONLY|M_PROTO1|M_PROTO1|M_PROTO2|\
			    M_PROTO3|M_PROTO4|M_PROTO5|\
			    M_BCAST|M_MCAST|M_FRAG|M_FIRSTFRAG|M_LASTFRAG|\
			    M_VLANTAG|M_PROMISC)

#define	M_BCAST		0x0200
#define	M_MCAST		0x0400
#define	M_FRAG		0x0800
#define	M_FIRSTFRAG	0x1000
#define	M_LASTFRAG	0x2000
#define	M_VLANTAG	0x10000
#define	M_PROMISC	0x20000
#define	M_NOFREE	0x40000

#define	EXT_CLUSTER	1
#define	EXT_SFBUF	2
#define	EXT_JUMBOP	3
#define	EXT_JUMBO9	4
#define	EXT_JUMBO16	5
#define	EXT_PACKET	6
#define	EXT_MBUF	7
#define	EXT_NET_DRV	100
#define	EXT_MOD_TYPE	200
#define	EXT_DISPOSABLE	300
#define	EXT_EXTREF	400

#define	MT_NOTMBUF	0
#define	MT_DATA		1
#define	MT_HEADER	MT_DATA
#define	MT_SONAME	8
#define	MT_CONTROL	14
#define	MT_OOBDATA	15
#define	MT_NTYPES	16

#define	MBTOM(how)	(how)

void		 m_tag_delete(struct mbuf *, struct m_tag *);
void		 m_tag_delete_chain(struct mbuf *, struct m_tag *);
void		 m_move_pkthdr(struct mbuf *, struct mbuf *);
void		 m_tag_free_default(struct m_tag *);

extern int max_linkhdr;
extern int max_protohdr;

#define	M_WRITABLE(m)	(!((m)->m_flags & M_RDONLY) &&			\
			 (!(((m)->m_flags & M_EXT)) ||			\
			 (*((m)->m_ext.ref_cnt) == 1)) )		\


#define	M_LEADINGSPACE(m)						\
	(((m)->m_flags & M_EXT) ?					\
	    (M_WRITABLE(m) ? (m)->m_data - (m)->m_ext.ext_buf : 0):	\
	    ((m)->m_flags & M_PKTHDR)? (m)->m_data - (m)->m_pktdat :	\
	    (m)->m_data - (m)->m_dat)

#define	M_TRAILINGSPACE(m)						\
	(((m)->m_flags & M_EXT) ?					\
	    (M_WRITABLE(m) ? (m)->m_ext.ext_buf + (m)->m_ext.ext_size	\
		- ((m)->m_data + (m)->m_len) : 0) :			\
	    &(m)->m_dat[MLEN] - ((m)->m_data + (m)->m_len))

#define	M_PREPEND(m, plen, how) do {					\
	struct mbuf **_mmp = &(m);					\
	struct mbuf *_mm = *_mmp;					\
	int _mplen = (plen);						\
	int __mhow = (how);						\
									\
	if (M_LEADINGSPACE(_mm) >= _mplen) {				\
		_mm->m_data -= _mplen;					\
		_mm->m_len += _mplen;					\
	} else								\
		_mm = m_prepend(_mm, _mplen, __mhow);			\
	if (_mm != NULL && _mm->m_flags & M_PKTHDR)			\
		_mm->m_pkthdr.len += _mplen;				\
	*_mmp = _mm;							\
} while (0)

#define	M_ALIGN(m, len) do {						\
        KASSERT(!((m)->m_flags & (M_PKTHDR|M_EXT)),                     \
                ("%s: M_ALIGN not normal mbuf", __func__));             \
        KASSERT((m)->m_data == (m)->m_dat,                              \
                ("%s: M_ALIGN not a virgin mbuf", __func__));           \
	(m)->m_data += (MLEN - (len)) & ~(sizeof(long) - 1);		\
} while (0)

#define	MH_ALIGN(m, len) do {						\
        KASSERT((m)->m_flags & M_PKTHDR && !((m)->m_flags & M_EXT),     \
                ("%s: MH_ALIGN not PKTHDR mbuf", __func__));            \
        KASSERT((m)->m_data == (m)->m_pktdat,                           \
                ("%s: MH_ALIGN not a virgin mbuf", __func__));          \
	(m)->m_data += (MHLEN - (len)) & ~(sizeof(long) - 1);		\
} while (0)

#define M_SIZE(m)						\
		(((m)->m_flags & M_EXT) ? (m)->m_ext.ext_size :	\
		((m)->m_flags & M_PKTHDR) ? MHLEN :		\
		MLEN)

#endif
