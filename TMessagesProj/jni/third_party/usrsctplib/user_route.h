/*-
 * Copyright (c) 1980, 1986, 1993
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

#ifndef _USER_ROUTE_H_
#define _USER_ROUTE_H_

struct sctp_route {
	struct	sctp_rtentry *ro_rt;
	struct	sockaddr ro_dst;
};

struct sctp_rt_metrics_lite {
	uint32_t rmx_mtu;
#if 0
	u_long	rmx_expire;
	u_long	rmx_pksent;
#endif
};

struct sctp_rtentry {
#if 0
	struct	radix_node rt_nodes[2];

#define	rt_key(r)	(*((struct sockaddr **)(&(r)->rt_nodes->rn_key)))
#define	rt_mask(r)	(*((struct sockaddr **)(&(r)->rt_nodes->rn_mask)))
	struct	sockaddr *rt_gateway;
	u_long	rt_flags;
#endif
	struct	ifnet *rt_ifp;
	struct	ifaddr *rt_ifa;
	struct	sctp_rt_metrics_lite rt_rmx;
	long	rt_refcnt;
#if 0
	struct	sockaddr *rt_genmask;
	caddr_t	rt_llinfo;
	struct	rtentry *rt_gwroute;
	struct	rtentry *rt_parent;
#endif
	struct	mtx rt_mtx;
};

#define	RT_LOCK_INIT(_rt)	mtx_init(&(_rt)->rt_mtx, "rtentry", NULL, MTX_DEF | MTX_DUPOK)
#define	RT_LOCK(_rt)		mtx_lock(&(_rt)->rt_mtx)
#define	RT_UNLOCK(_rt)		mtx_unlock(&(_rt)->rt_mtx)
#define	RT_LOCK_DESTROY(_rt)	mtx_destroy(&(_rt)->rt_mtx)
#define	RT_LOCK_ASSERT(_rt)	mtx_assert(&(_rt)->rt_mtx, MA_OWNED)

#define	RT_ADDREF(_rt)	do {					\
	RT_LOCK_ASSERT(_rt);					\
	KASSERT((_rt)->rt_refcnt >= 0,				\
		("negative refcnt %ld", (_rt)->rt_refcnt));	\
	(_rt)->rt_refcnt++;					\
} while (0)
#define	RT_REMREF(_rt)	do {					\
	RT_LOCK_ASSERT(_rt);					\
	KASSERT((_rt)->rt_refcnt > 0,				\
		("bogus refcnt %ld", (_rt)->rt_refcnt));	\
	(_rt)->rt_refcnt--;					\
} while (0)
#define	RTFREE_LOCKED(_rt) do {					\
		if ((_rt)->rt_refcnt <= 1) {			\
			rtfree(_rt);				\
		} else {					\
			RT_REMREF(_rt);				\
			RT_UNLOCK(_rt);				\
		}						\
		                                		\
		_rt = NULL;					\
	} while (0)
#define	RTFREE(_rt) do {					\
		RT_LOCK(_rt);					\
		RTFREE_LOCKED(_rt);				\
} while (0)
#endif
