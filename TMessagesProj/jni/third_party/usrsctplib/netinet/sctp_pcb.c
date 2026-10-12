/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 2001-2008, by Cisco Systems, Inc. All rights reserved.
 * Copyright (c) 2008-2012, by Randall Stewart. All rights reserved.
 * Copyright (c) 2008-2012, by Michael Tuexen. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * a) Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * b) Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the distribution.
 *
 * c) Neither the name of Cisco Systems, Inc. nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#if defined(__FreeBSD__) && !defined(__Userspace__)
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");
#endif

#include <netinet/sctp_os.h>
#if defined(__FreeBSD__) && !defined(__Userspace__)
#include <sys/proc.h>
#endif
#include <netinet/sctp_var.h>
#include <netinet/sctp_sysctl.h>
#include <netinet/sctp_pcb.h>
#include <netinet/sctputil.h>
#include <netinet/sctp.h>
#include <netinet/sctp_header.h>
#include <netinet/sctp_asconf.h>
#include <netinet/sctp_output.h>
#include <netinet/sctp_timer.h>
#include <netinet/sctp_bsd_addr.h>
#if defined(INET) || defined(INET6)
#if !defined(_WIN32)
#include <netinet/udp.h>
#endif
#endif
#ifdef INET6
#if defined(__Userspace__)
#include "user_ip6_var.h"
#else
#include <netinet6/ip6_var.h>
#endif
#endif
#if defined(__FreeBSD__) && !defined(__Userspace__)
#include <sys/sched.h>
#include <sys/smp.h>
#include <sys/unistd.h>
#endif
#if defined(__Userspace__)
#include <user_socketvar.h>
#include <user_atomic.h>
#if !defined(_WIN32)
#include <netdb.h>
#endif
#endif

#if !defined(__FreeBSD__) || defined(__Userspace__)
struct sctp_base_info system_base_info;

#endif

#ifdef INET6
int
SCTP6_ARE_ADDR_EQUAL(struct sockaddr_in6 *a, struct sockaddr_in6 *b)
{
#ifdef SCTP_EMBEDDED_V6_SCOPE
#if defined(__APPLE__) && !defined(__Userspace__)
	struct in6_addr tmp_a, tmp_b;

	tmp_a = a->sin6_addr;
#if defined(APPLE_LEOPARD) || defined(APPLE_SNOWLEOPARD)
	if (in6_embedscope(&tmp_a, a, NULL, NULL) != 0) {
#else
	if (in6_embedscope(&tmp_a, a, NULL, NULL, NULL) != 0) {
#endif
		return (0);
	}
	tmp_b = b->sin6_addr;
#if defined(APPLE_LEOPARD) || defined(APPLE_SNOWLEOPARD)
	if (in6_embedscope(&tmp_b, b, NULL, NULL) != 0) {
#else
	if (in6_embedscope(&tmp_b, b, NULL, NULL, NULL) != 0) {
#endif
		return (0);
	}
	return (IN6_ARE_ADDR_EQUAL(&tmp_a, &tmp_b));
#elif defined(SCTP_KAME)
	struct sockaddr_in6 tmp_a, tmp_b;

	memcpy(&tmp_a, a, sizeof(struct sockaddr_in6));
	if (sa6_embedscope(&tmp_a, MODULE_GLOBAL(ip6_use_defzone)) != 0) {
		return (0);
	}
	memcpy(&tmp_b, b, sizeof(struct sockaddr_in6));
	if (sa6_embedscope(&tmp_b, MODULE_GLOBAL(ip6_use_defzone)) != 0) {
		return (0);
	}
	return (IN6_ARE_ADDR_EQUAL(&tmp_a.sin6_addr, &tmp_b.sin6_addr));
#else
	struct in6_addr tmp_a, tmp_b;

	tmp_a = a->sin6_addr;
	if (in6_embedscope(&tmp_a, a) != 0) {
		return (0);
	}
	tmp_b = b->sin6_addr;
	if (in6_embedscope(&tmp_b, b) != 0) {
		return (0);
	}
	return (IN6_ARE_ADDR_EQUAL(&tmp_a, &tmp_b));
#endif
#else
	return (IN6_ARE_ADDR_EQUAL(&(a->sin6_addr), &(b->sin6_addr)));
#endif
}
#endif

void
sctp_fill_pcbinfo(struct sctp_pcbinfo *spcb)
{

	SCTP_INP_INFO_RLOCK();
	spcb->ep_count = SCTP_BASE_INFO(ipi_count_ep);
	spcb->asoc_count = SCTP_BASE_INFO(ipi_count_asoc);
	spcb->laddr_count = SCTP_BASE_INFO(ipi_count_laddr);
	spcb->raddr_count = SCTP_BASE_INFO(ipi_count_raddr);
	spcb->chk_count = SCTP_BASE_INFO(ipi_count_chunk);
	spcb->readq_count = SCTP_BASE_INFO(ipi_count_readq);
	spcb->stream_oque = SCTP_BASE_INFO(ipi_count_strmoq);
	spcb->free_chunks = SCTP_BASE_INFO(ipi_free_chunks);
	SCTP_INP_INFO_RUNLOCK();
}

struct sctp_vrf *
sctp_allocate_vrf(int vrf_id)
{
	struct sctp_vrf *vrf = NULL;
	struct sctp_vrflist *bucket;

	vrf = sctp_find_vrf(vrf_id);
	if (vrf) {

		return (vrf);
	}
	SCTP_MALLOC(vrf, struct sctp_vrf *, sizeof(struct sctp_vrf),
		    SCTP_M_VRF);
	if (vrf == NULL) {

#ifdef INVARIANTS
		panic("No memory for VRF:%d", vrf_id);
#endif
		return (NULL);
	}

	memset(vrf, 0, sizeof(struct sctp_vrf));
	vrf->vrf_id = vrf_id;
	LIST_INIT(&vrf->ifnlist);
	vrf->total_ifa_count = 0;
	vrf->refcount = 0;

	SCTP_INIT_VRF_TABLEID(vrf);

	vrf->vrf_addr_hash = SCTP_HASH_INIT(SCTP_VRF_ADDR_HASH_SIZE,
					    &vrf->vrf_addr_hashmark);
	if (vrf->vrf_addr_hash == NULL) {

#ifdef INVARIANTS
		panic("No memory for VRF:%d", vrf_id);
#endif
		SCTP_FREE(vrf, SCTP_M_VRF);
		return (NULL);
	}

	bucket = &SCTP_BASE_INFO(sctp_vrfhash)[(vrf_id & SCTP_BASE_INFO(hashvrfmark))];
	LIST_INSERT_HEAD(bucket, vrf, next_vrf);
	atomic_add_int(&SCTP_BASE_INFO(ipi_count_vrfs), 1);
	return (vrf);
}

struct sctp_ifn *
sctp_find_ifn(void *ifn, uint32_t ifn_index)
{
	struct sctp_ifn *sctp_ifnp;
	struct sctp_ifnlist *hash_ifn_head;

	SCTP_IPI_ADDR_LOCK_ASSERT();
	hash_ifn_head = &SCTP_BASE_INFO(vrf_ifn_hash)[(ifn_index & SCTP_BASE_INFO(vrf_ifn_hashmark))];
	LIST_FOREACH(sctp_ifnp, hash_ifn_head, next_bucket) {
		if (sctp_ifnp->ifn_index == ifn_index) {
			return (sctp_ifnp);
		}
		if (sctp_ifnp->ifn_p && ifn && (sctp_ifnp->ifn_p == ifn)) {
			return (sctp_ifnp);
		}
	}
	return (NULL);
}

struct sctp_vrf *
sctp_find_vrf(uint32_t vrf_id)
{
	struct sctp_vrflist *bucket;
	struct sctp_vrf *liste;

	bucket = &SCTP_BASE_INFO(sctp_vrfhash)[(vrf_id & SCTP_BASE_INFO(hashvrfmark))];
	LIST_FOREACH(liste, bucket, next_vrf) {
		if (vrf_id == liste->vrf_id) {
			return (liste);
		}
	}
	return (NULL);
}

void
sctp_free_vrf(struct sctp_vrf *vrf)
{
	if (SCTP_DECREMENT_AND_CHECK_REFCOUNT(&vrf->refcount)) {
                if (vrf->vrf_addr_hash) {
                    SCTP_HASH_FREE(vrf->vrf_addr_hash, vrf->vrf_addr_hashmark);
                    vrf->vrf_addr_hash = NULL;
                }

		LIST_REMOVE(vrf, next_vrf);
		SCTP_FREE(vrf, SCTP_M_VRF);
		atomic_subtract_int(&SCTP_BASE_INFO(ipi_count_vrfs), 1);
	}
}

void
sctp_free_ifn(struct sctp_ifn *sctp_ifnp)
{
	if (SCTP_DECREMENT_AND_CHECK_REFCOUNT(&sctp_ifnp->refcount)) {

		if (sctp_ifnp->vrf) {
			sctp_free_vrf(sctp_ifnp->vrf);
		}
		SCTP_FREE(sctp_ifnp, SCTP_M_IFN);
		atomic_subtract_int(&SCTP_BASE_INFO(ipi_count_ifns), 1);
	}
}

void
sctp_update_ifn_mtu(uint32_t ifn_index, uint32_t mtu)
{
	struct sctp_ifn *sctp_ifnp;

	sctp_ifnp = sctp_find_ifn((void *)NULL, ifn_index);
	if (sctp_ifnp != NULL) {
		sctp_ifnp->ifn_mtu = mtu;
	}
}

void
sctp_free_ifa(struct sctp_ifa *sctp_ifap)
{
	if (SCTP_DECREMENT_AND_CHECK_REFCOUNT(&sctp_ifap->refcount)) {

		if (sctp_ifap->ifn_p) {
			sctp_free_ifn(sctp_ifap->ifn_p);
		}
		SCTP_FREE(sctp_ifap, SCTP_M_IFA);
		atomic_subtract_int(&SCTP_BASE_INFO(ipi_count_ifas), 1);
	}
}

static void
sctp_delete_ifn(struct sctp_ifn *sctp_ifnp, int hold_addr_lock)
{
	struct sctp_ifn *found;

	found = sctp_find_ifn(sctp_ifnp->ifn_p, sctp_ifnp->ifn_index);
	if (found == NULL) {

		return;
	}
	if (hold_addr_lock == 0) {
		SCTP_IPI_ADDR_WLOCK();
	} else {
		SCTP_IPI_ADDR_WLOCK_ASSERT();
	}
	LIST_REMOVE(sctp_ifnp, next_bucket);
	LIST_REMOVE(sctp_ifnp, next_ifn);
	if (hold_addr_lock == 0) {
		SCTP_IPI_ADDR_WUNLOCK();
	}

	sctp_free_ifn(sctp_ifnp);
}

void
sctp_mark_ifa_addr_down(uint32_t vrf_id, struct sockaddr *addr,
			const char *if_name, uint32_t ifn_index)
{
	struct sctp_vrf *vrf;
	struct sctp_ifa *sctp_ifap;

	SCTP_IPI_ADDR_RLOCK();
	vrf = sctp_find_vrf(vrf_id);
	if (vrf == NULL) {
		SCTPDBG(SCTP_DEBUG_PCB4, "Can't find vrf_id 0x%x\n", vrf_id);
		goto out;
	}
	sctp_ifap = sctp_find_ifa_by_addr(addr, vrf->vrf_id, SCTP_ADDR_LOCKED);
	if (sctp_ifap == NULL) {
		SCTPDBG(SCTP_DEBUG_PCB4, "Can't find sctp_ifap for address\n");
		goto out;
	}
	if (sctp_ifap->ifn_p == NULL) {
		SCTPDBG(SCTP_DEBUG_PCB4, "IFA has no IFN - can't mark unusable\n");
		goto out;
	}
	if (if_name) {
		if (strncmp(if_name, sctp_ifap->ifn_p->ifn_name, SCTP_IFNAMSIZ) != 0) {
			SCTPDBG(SCTP_DEBUG_PCB4, "IFN %s of IFA not the same as %s\n",
				sctp_ifap->ifn_p->ifn_name, if_name);
			goto out;
		}
	} else {
		if (sctp_ifap->ifn_p->ifn_index != ifn_index) {
			SCTPDBG(SCTP_DEBUG_PCB4, "IFA owned by ifn_index:%d down command for ifn_index:%d - ignored\n",
				sctp_ifap->ifn_p->ifn_index, ifn_index);
			goto out;
		}
	}

	sctp_ifap->localifa_flags &= (~SCTP_ADDR_VALID);
	sctp_ifap->localifa_flags |= SCTP_ADDR_IFA_UNUSEABLE;
 out:
	SCTP_IPI_ADDR_RUNLOCK();
}

void
sctp_mark_ifa_addr_up(uint32_t vrf_id, struct sockaddr *addr,
		      const char *if_name, uint32_t ifn_index)
{
	struct sctp_vrf *vrf;
	struct sctp_ifa *sctp_ifap;

	SCTP_IPI_ADDR_RLOCK();
	vrf = sctp_find_vrf(vrf_id);
	if (vrf == NULL) {
		SCTPDBG(SCTP_DEBUG_PCB4, "Can't find vrf_id 0x%x\n", vrf_id);
		goto out;
	}
	sctp_ifap = sctp_find_ifa_by_addr(addr, vrf->vrf_id, SCTP_ADDR_LOCKED);
	if (sctp_ifap == NULL) {
		SCTPDBG(SCTP_DEBUG_PCB4, "Can't find sctp_ifap for address\n");
		goto out;
	}
	if (sctp_ifap->ifn_p == NULL) {
		SCTPDBG(SCTP_DEBUG_PCB4, "IFA has no IFN - can't mark unusable\n");
		goto out;
	}
	if (if_name) {
		if (strncmp(if_name, sctp_ifap->ifn_p->ifn_name, SCTP_IFNAMSIZ) != 0) {
			SCTPDBG(SCTP_DEBUG_PCB4, "IFN %s of IFA not the same as %s\n",
				sctp_ifap->ifn_p->ifn_name, if_name);
			goto out;
		}
	} else {
		if (sctp_ifap->ifn_p->ifn_index != ifn_index) {
			SCTPDBG(SCTP_DEBUG_PCB4, "IFA owned by ifn_index:%d down command for ifn_index:%d - ignored\n",
				sctp_ifap->ifn_p->ifn_index, ifn_index);
			goto out;
		}
	}

	sctp_ifap->localifa_flags &= (~SCTP_ADDR_IFA_UNUSEABLE);
	sctp_ifap->localifa_flags |= SCTP_ADDR_VALID;
 out:
	SCTP_IPI_ADDR_RUNLOCK();
}

static void
sctp_add_ifa_to_ifn(struct sctp_ifn *sctp_ifnp, struct sctp_ifa *sctp_ifap)
{
	int ifa_af;

	LIST_INSERT_HEAD(&sctp_ifnp->ifalist, sctp_ifap, next_ifa);
	sctp_ifap->ifn_p = sctp_ifnp;
	atomic_add_int(&sctp_ifap->ifn_p->refcount, 1);

	sctp_ifnp->ifa_count++;
	ifa_af = sctp_ifap->address.sa.sa_family;
	switch (ifa_af) {
#ifdef INET
	case AF_INET:
		sctp_ifnp->num_v4++;
		break;
#endif
#ifdef INET6
	case AF_INET6:
		sctp_ifnp->num_v6++;
		break;
#endif
	default:
		break;
	}
	if (sctp_ifnp->ifa_count == 1) {

		sctp_ifnp->registered_af = ifa_af;
	}
}

static void
sctp_remove_ifa_from_ifn(struct sctp_ifa *sctp_ifap)
{
	LIST_REMOVE(sctp_ifap, next_ifa);
	if (sctp_ifap->ifn_p) {

		sctp_ifap->ifn_p->ifa_count--;
		switch (sctp_ifap->address.sa.sa_family) {
#ifdef INET
		case AF_INET:
			sctp_ifap->ifn_p->num_v4--;
			break;
#endif
#ifdef INET6
		case AF_INET6:
			sctp_ifap->ifn_p->num_v6--;
			break;
#endif
		default:
			break;
		}

		if (LIST_EMPTY(&sctp_ifap->ifn_p->ifalist)) {

			sctp_delete_ifn(sctp_ifap->ifn_p, SCTP_ADDR_LOCKED);
		} else {

			if ((sctp_ifap->ifn_p->num_v6 == 0) &&
			    (sctp_ifap->ifn_p->registered_af == AF_INET6)) {
				sctp_ifap->ifn_p->registered_af = AF_INET;
			} else if ((sctp_ifap->ifn_p->num_v4 == 0) &&
				   (sctp_ifap->ifn_p->registered_af == AF_INET)) {
				sctp_ifap->ifn_p->registered_af = AF_INET6;
			}

			sctp_free_ifn(sctp_ifap->ifn_p);
		}
		sctp_ifap->ifn_p = NULL;
	}
}

struct sctp_ifa *
sctp_add_addr_to_vrf(uint32_t vrf_id, void *ifn, uint32_t ifn_index,
		     uint32_t ifn_type, const char *if_name, void *ifa,
		     struct sockaddr *addr, uint32_t ifa_flags,
		     int dynamic_add)
{
	struct sctp_vrf *vrf;
	struct sctp_ifn *sctp_ifnp, *new_sctp_ifnp;
	struct sctp_ifa *sctp_ifap, *new_sctp_ifap;
	struct sctp_ifalist *hash_addr_head;
	struct sctp_ifnlist *hash_ifn_head;
	uint32_t hash_of_addr;
	int new_ifn_af = 0;

#ifdef SCTP_DEBUG
	SCTPDBG(SCTP_DEBUG_PCB4, "vrf_id 0x%x: adding address: ", vrf_id);
	SCTPDBG_ADDR(SCTP_DEBUG_PCB4, addr);
#endif
	SCTP_MALLOC(new_sctp_ifnp, struct sctp_ifn *,
	            sizeof(struct sctp_ifn), SCTP_M_IFN);
	if (new_sctp_ifnp == NULL) {
#ifdef INVARIANTS
		panic("No memory for IFN");
#endif
		return (NULL);
	}
	SCTP_MALLOC(new_sctp_ifap, struct sctp_ifa *, sizeof(struct sctp_ifa), SCTP_M_IFA);
	if (new_sctp_ifap == NULL) {
#ifdef INVARIANTS
		panic("No memory for IFA");
#endif
		SCTP_FREE(new_sctp_ifnp, SCTP_M_IFN);
		return (NULL);
	}

	SCTP_IPI_ADDR_WLOCK();
	sctp_ifnp = sctp_find_ifn(ifn, ifn_index);
	if (sctp_ifnp) {
		vrf = sctp_ifnp->vrf;
	} else {
		vrf = sctp_find_vrf(vrf_id);
		if (vrf == NULL) {
			vrf = sctp_allocate_vrf(vrf_id);
			if (vrf == NULL) {
				SCTP_IPI_ADDR_WUNLOCK();
				SCTP_FREE(new_sctp_ifnp, SCTP_M_IFN);
				SCTP_FREE(new_sctp_ifap, SCTP_M_IFA);
				return (NULL);
			}
		}
	}
	if (sctp_ifnp == NULL) {

		sctp_ifnp = new_sctp_ifnp;
		new_sctp_ifnp = NULL;
		memset(sctp_ifnp, 0, sizeof(struct sctp_ifn));
		sctp_ifnp->ifn_index = ifn_index;
		sctp_ifnp->ifn_p = ifn;
		sctp_ifnp->ifn_type = ifn_type;
		sctp_ifnp->refcount = 0;
		sctp_ifnp->vrf = vrf;
		atomic_add_int(&vrf->refcount, 1);
		sctp_ifnp->ifn_mtu = SCTP_GATHER_MTU_FROM_IFN_INFO(ifn, ifn_index, addr->sa_family);
		if (if_name != NULL) {
			SCTP_SNPRINTF(sctp_ifnp->ifn_name, SCTP_IFNAMSIZ, "%s", if_name);
		} else {
			SCTP_SNPRINTF(sctp_ifnp->ifn_name, SCTP_IFNAMSIZ, "%s", "unknown");
		}
		hash_ifn_head = &SCTP_BASE_INFO(vrf_ifn_hash)[(ifn_index & SCTP_BASE_INFO(vrf_ifn_hashmark))];
		LIST_INIT(&sctp_ifnp->ifalist);
		LIST_INSERT_HEAD(hash_ifn_head, sctp_ifnp, next_bucket);
		LIST_INSERT_HEAD(&vrf->ifnlist, sctp_ifnp, next_ifn);
		atomic_add_int(&SCTP_BASE_INFO(ipi_count_ifns), 1);
		new_ifn_af = 1;
	}
	sctp_ifap = sctp_find_ifa_by_addr(addr, vrf->vrf_id, SCTP_ADDR_LOCKED);
	if (sctp_ifap) {

		if ((sctp_ifap->ifn_p) &&
		    (sctp_ifap->ifn_p->ifn_index == ifn_index)) {
			SCTPDBG(SCTP_DEBUG_PCB4, "Using existing ifn %s (0x%x) for ifa %p\n",
				sctp_ifap->ifn_p->ifn_name, ifn_index,
				(void *)sctp_ifap);
			if (new_ifn_af) {

				sctp_delete_ifn(sctp_ifnp, SCTP_ADDR_LOCKED);
			}
			if (sctp_ifap->localifa_flags & SCTP_BEING_DELETED) {

				SCTPDBG(SCTP_DEBUG_PCB4, "Clearing deleted ifa flag\n");
				sctp_ifap->localifa_flags = SCTP_ADDR_VALID;
				sctp_ifap->ifn_p = sctp_ifnp;
				atomic_add_int(&sctp_ifap->ifn_p->refcount, 1);
			}
		exit_stage_left:
			SCTP_IPI_ADDR_WUNLOCK();
			if (new_sctp_ifnp != NULL) {
				SCTP_FREE(new_sctp_ifnp, SCTP_M_IFN);
			}
			SCTP_FREE(new_sctp_ifap, SCTP_M_IFA);
			return (sctp_ifap);
		} else {
			if (sctp_ifap->ifn_p) {

				SCTPDBG(SCTP_DEBUG_PCB4, "Moving ifa %p from %s (0x%x) to %s (0x%x)\n",
					(void *)sctp_ifap, sctp_ifap->ifn_p->ifn_name,
					sctp_ifap->ifn_p->ifn_index, if_name,
					ifn_index);

				sctp_remove_ifa_from_ifn(sctp_ifap);

				sctp_add_ifa_to_ifn(sctp_ifnp, sctp_ifap);
				goto exit_stage_left;
			} else {

				sctp_ifap->localifa_flags = SCTP_ADDR_VALID;
				SCTPDBG(SCTP_DEBUG_PCB4, "Repairing ifn %p for ifa %p\n",
					(void *)sctp_ifnp, (void *)sctp_ifap);
				sctp_add_ifa_to_ifn(sctp_ifnp, sctp_ifap);
			}
			goto exit_stage_left;
		}
	}
	sctp_ifap = new_sctp_ifap;
	memset(sctp_ifap, 0, sizeof(struct sctp_ifa));
	sctp_ifap->ifn_p = sctp_ifnp;
	atomic_add_int(&sctp_ifnp->refcount, 1);
	sctp_ifap->vrf_id = vrf_id;
	sctp_ifap->ifa = ifa;
#ifdef HAVE_SA_LEN
	memcpy(&sctp_ifap->address, addr, addr->sa_len);
#else
	switch (addr->sa_family) {
#ifdef INET
	case AF_INET:
		memcpy(&sctp_ifap->address, addr, sizeof(struct sockaddr_in));
		break;
#endif
#ifdef INET6
	case AF_INET6:
		memcpy(&sctp_ifap->address, addr, sizeof(struct sockaddr_in6));
		break;
#endif
#if defined(__Userspace__)
	case AF_CONN:
		memcpy(&sctp_ifap->address, addr, sizeof(struct sockaddr_conn));
		break;
#endif
	default:

		break;
	}
#endif
	sctp_ifap->localifa_flags = SCTP_ADDR_VALID | SCTP_ADDR_DEFER_USE;
	sctp_ifap->flags = ifa_flags;

	switch (sctp_ifap->address.sa.sa_family) {
#ifdef INET
	case AF_INET:
	{
		struct sockaddr_in *sin;

		sin = &sctp_ifap->address.sin;
		if (SCTP_IFN_IS_IFT_LOOP(sctp_ifap->ifn_p) ||
		    (IN4_ISLOOPBACK_ADDRESS(&sin->sin_addr))) {
			sctp_ifap->src_is_loop = 1;
		}
		if ((IN4_ISPRIVATE_ADDRESS(&sin->sin_addr))) {
			sctp_ifap->src_is_priv = 1;
		}
		sctp_ifnp->num_v4++;
		if (new_ifn_af)
		    new_ifn_af = AF_INET;
		break;
	}
#endif
#ifdef INET6
	case AF_INET6:
	{

		struct sockaddr_in6 *sin6;

		sin6 = &sctp_ifap->address.sin6;
		if (SCTP_IFN_IS_IFT_LOOP(sctp_ifap->ifn_p) ||
		    (IN6_IS_ADDR_LOOPBACK(&sin6->sin6_addr))) {
			sctp_ifap->src_is_loop = 1;
		}
		if (IN6_IS_ADDR_LINKLOCAL(&sin6->sin6_addr)) {
			sctp_ifap->src_is_priv = 1;
		}
		sctp_ifnp->num_v6++;
		if (new_ifn_af)
			new_ifn_af = AF_INET6;
		break;
	}
#endif
#if defined(__Userspace__)
	case AF_CONN:
		if (new_ifn_af)
			new_ifn_af = AF_CONN;
		break;
#endif
	default:
		new_ifn_af = 0;
		break;
	}
	hash_of_addr = sctp_get_ifa_hash_val(&sctp_ifap->address.sa);

	if ((sctp_ifap->src_is_priv == 0) &&
	    (sctp_ifap->src_is_loop == 0)) {
		sctp_ifap->src_is_glob = 1;
	}
	hash_addr_head = &vrf->vrf_addr_hash[(hash_of_addr & vrf->vrf_addr_hashmark)];
	LIST_INSERT_HEAD(hash_addr_head, sctp_ifap, next_bucket);
	sctp_ifap->refcount = 1;
	LIST_INSERT_HEAD(&sctp_ifnp->ifalist, sctp_ifap, next_ifa);
	sctp_ifnp->ifa_count++;
	vrf->total_ifa_count++;
	atomic_add_int(&SCTP_BASE_INFO(ipi_count_ifas), 1);
	if (new_ifn_af) {
		sctp_ifnp->registered_af = new_ifn_af;
	}
	SCTP_IPI_ADDR_WUNLOCK();
	if (new_sctp_ifnp != NULL) {
		SCTP_FREE(new_sctp_ifnp, SCTP_M_IFN);
	}

	if (dynamic_add) {

		struct sctp_laddr *wi;

		atomic_add_int(&sctp_ifap->refcount, 1);
		wi = SCTP_ZONE_GET(SCTP_BASE_INFO(ipi_zone_laddr), struct sctp_laddr);
		if (wi == NULL) {

			SCTPDBG(SCTP_DEBUG_PCB4, "Lost an address change?\n");

			sctp_del_addr_from_vrf(vrf_id, addr, ifn_index,
					       if_name);
			return (NULL);
		}
		SCTP_INCR_LADDR_COUNT();
		memset(wi, 0, sizeof(*wi));
		(void)SCTP_GETTIME_TIMEVAL(&wi->start_time);
		wi->ifa = sctp_ifap;
		wi->action = SCTP_ADD_IP_ADDRESS;

		SCTP_WQ_ADDR_LOCK();
		LIST_INSERT_HEAD(&SCTP_BASE_INFO(addr_wq), wi, sctp_nxt_addr);
		sctp_timer_start(SCTP_TIMER_TYPE_ADDR_WQ,
				 (struct sctp_inpcb *)NULL,
				 (struct sctp_tcb *)NULL,
				 (struct sctp_nets *)NULL);
		SCTP_WQ_ADDR_UNLOCK();
	} else {

		sctp_ifap->localifa_flags &= ~SCTP_ADDR_DEFER_USE;
	}
	return (sctp_ifap);
}

void
sctp_del_addr_from_vrf(uint32_t vrf_id, struct sockaddr *addr,
		       uint32_t ifn_index, const char *if_name)
{
	struct sctp_vrf *vrf;
	struct sctp_ifa *sctp_ifap = NULL;

	SCTP_IPI_ADDR_WLOCK();
	vrf = sctp_find_vrf(vrf_id);
	if (vrf == NULL) {
		SCTPDBG(SCTP_DEBUG_PCB4, "Can't find vrf_id 0x%x\n", vrf_id);
		goto out_now;
	}

#ifdef SCTP_DEBUG
	SCTPDBG(SCTP_DEBUG_PCB4, "vrf_id 0x%x: deleting address:", vrf_id);
	SCTPDBG_ADDR(SCTP_DEBUG_PCB4, addr);
#endif
	sctp_ifap = sctp_find_ifa_by_addr(addr, vrf->vrf_id, SCTP_ADDR_LOCKED);
	if (sctp_ifap) {

		if (sctp_ifap->ifn_p) {
			int valid = 0;

			if (if_name) {
				if (strncmp(if_name, sctp_ifap->ifn_p->ifn_name, SCTP_IFNAMSIZ) == 0) {

					valid = 1;
				}
			}
			if (!valid) {

				if (ifn_index == sctp_ifap->ifn_p->ifn_index) {
					valid = 1;
				}
			}
			if (!valid) {
				SCTPDBG(SCTP_DEBUG_PCB4, "ifn:%d ifname:%s does not match addresses\n",
					ifn_index, ((if_name == NULL) ? "NULL" : if_name));
				SCTPDBG(SCTP_DEBUG_PCB4, "ifn:%d ifname:%s - ignoring delete\n",
					sctp_ifap->ifn_p->ifn_index, sctp_ifap->ifn_p->ifn_name);
				SCTP_IPI_ADDR_WUNLOCK();
				return;
			}
		}
		SCTPDBG(SCTP_DEBUG_PCB4, "Deleting ifa %p\n", (void *)sctp_ifap);
		sctp_ifap->localifa_flags &= SCTP_ADDR_VALID;

		vrf->total_ifa_count--;
		LIST_REMOVE(sctp_ifap, next_bucket);
		sctp_remove_ifa_from_ifn(sctp_ifap);
	}
#ifdef SCTP_DEBUG
	else {
		SCTPDBG(SCTP_DEBUG_PCB4, "Del Addr-ifn:%d Could not find address:",
			ifn_index);
		SCTPDBG_ADDR(SCTP_DEBUG_PCB1, addr);
	}
#endif

 out_now:
	SCTP_IPI_ADDR_WUNLOCK();
	if (sctp_ifap) {
		struct sctp_laddr *wi;

		wi = SCTP_ZONE_GET(SCTP_BASE_INFO(ipi_zone_laddr), struct sctp_laddr);
		if (wi == NULL) {

			SCTPDBG(SCTP_DEBUG_PCB4, "Lost an address change?\n");

			sctp_free_ifa(sctp_ifap);
			return;
		}
		SCTP_INCR_LADDR_COUNT();
		memset(wi, 0, sizeof(*wi));
		(void)SCTP_GETTIME_TIMEVAL(&wi->start_time);
		wi->ifa = sctp_ifap;
		wi->action = SCTP_DEL_IP_ADDRESS;
		SCTP_WQ_ADDR_LOCK();

		LIST_INSERT_HEAD(&SCTP_BASE_INFO(addr_wq), wi, sctp_nxt_addr);
		sctp_timer_start(SCTP_TIMER_TYPE_ADDR_WQ,
				 (struct sctp_inpcb *)NULL,
				 (struct sctp_tcb *)NULL,
				 (struct sctp_nets *)NULL);
		SCTP_WQ_ADDR_UNLOCK();
	}
	return;
}

static int
sctp_does_stcb_own_this_addr(struct sctp_tcb *stcb, struct sockaddr *to)
{
	int loopback_scope;
#if defined(INET)
	int ipv4_local_scope, ipv4_addr_legal;
#endif
#if defined(INET6)
	int local_scope, site_scope, ipv6_addr_legal;
#endif
#if defined(__Userspace__)
	int conn_addr_legal;
#endif
	struct sctp_vrf *vrf;
	struct sctp_ifn *sctp_ifn;
	struct sctp_ifa *sctp_ifa;

	loopback_scope = stcb->asoc.scope.loopback_scope;
#if defined(INET)
	ipv4_local_scope = stcb->asoc.scope.ipv4_local_scope;
	ipv4_addr_legal = stcb->asoc.scope.ipv4_addr_legal;
#endif
#if defined(INET6)
	local_scope = stcb->asoc.scope.local_scope;
	site_scope = stcb->asoc.scope.site_scope;
	ipv6_addr_legal = stcb->asoc.scope.ipv6_addr_legal;
#endif
#if defined(__Userspace__)
	conn_addr_legal = stcb->asoc.scope.conn_addr_legal;
#endif

	SCTP_IPI_ADDR_RLOCK();
	vrf = sctp_find_vrf(stcb->asoc.vrf_id);
	if (vrf == NULL) {

		SCTP_IPI_ADDR_RUNLOCK();
		return (0);
	}

	if (stcb->sctp_ep->sctp_flags & SCTP_PCB_FLAGS_BOUNDALL) {
		LIST_FOREACH(sctp_ifn, &vrf->ifnlist, next_ifn) {
			if ((loopback_scope == 0) &&
			    SCTP_IFN_IS_IFT_LOOP(sctp_ifn)) {
				continue;
			}
			LIST_FOREACH(sctp_ifa, &sctp_ifn->ifalist, next_ifa) {
				if (sctp_is_addr_restricted(stcb, sctp_ifa) &&
				    (!sctp_is_addr_pending(stcb, sctp_ifa))) {

					continue;
				}
				if (sctp_ifa->address.sa.sa_family != to->sa_family) {
					continue;
				}
				switch (sctp_ifa->address.sa.sa_family) {
#ifdef INET
				case AF_INET:
					if (ipv4_addr_legal) {
						struct sockaddr_in *sin, *rsin;

						sin = &sctp_ifa->address.sin;
						rsin = (struct sockaddr_in *)to;
						if ((ipv4_local_scope == 0) &&
						    IN4_ISPRIVATE_ADDRESS(&sin->sin_addr)) {
							continue;
						}
#if defined(__FreeBSD__) && !defined(__Userspace__)
						if (prison_check_ip4(stcb->sctp_ep->ip_inp.inp.inp_cred,
						                     &sin->sin_addr) != 0) {
							continue;
						}
#endif
						if (sin->sin_addr.s_addr == rsin->sin_addr.s_addr) {
							SCTP_IPI_ADDR_RUNLOCK();
							return (1);
						}
					}
					break;
#endif
#ifdef INET6
				case AF_INET6:
					if (ipv6_addr_legal) {
						struct sockaddr_in6 *sin6, *rsin6;
#if defined(SCTP_EMBEDDED_V6_SCOPE) && !defined(SCTP_KAME)
						struct sockaddr_in6 lsa6;
#endif
						sin6 = &sctp_ifa->address.sin6;
						rsin6 = (struct sockaddr_in6 *)to;
#if defined(__FreeBSD__) && !defined(__Userspace__)
						if (prison_check_ip6(stcb->sctp_ep->ip_inp.inp.inp_cred,
						                     &sin6->sin6_addr) != 0) {
							continue;
						}
#endif
						if (IN6_IS_ADDR_LINKLOCAL(&sin6->sin6_addr)) {
							if (local_scope == 0)
								continue;
#if defined(SCTP_EMBEDDED_V6_SCOPE)
							if (sin6->sin6_scope_id == 0) {
#ifdef SCTP_KAME
								if (sa6_recoverscope(sin6) != 0)
									continue;
#else
								lsa6 = *sin6;
								if (in6_recoverscope(&lsa6,
								                     &lsa6.sin6_addr,
								                     NULL))
									continue;
								sin6 = &lsa6;
#endif
							}
#endif
						}
						if ((site_scope == 0) &&
						    (IN6_IS_ADDR_SITELOCAL(&sin6->sin6_addr))) {
							continue;
						}
						if (SCTP6_ARE_ADDR_EQUAL(sin6, rsin6)) {
							SCTP_IPI_ADDR_RUNLOCK();
							return (1);
						}
					}
					break;
#endif
#if defined(__Userspace__)
				case AF_CONN:
					if (conn_addr_legal) {
						struct sockaddr_conn *sconn, *rsconn;

						sconn = &sctp_ifa->address.sconn;
						rsconn = (struct sockaddr_conn *)to;
						if (sconn->sconn_addr == rsconn->sconn_addr) {
							SCTP_IPI_ADDR_RUNLOCK();
							return (1);
						}
					}
					break;
#endif
				default:

					break;
				}
			}
		}
	} else {
		struct sctp_laddr *laddr;

		LIST_FOREACH(laddr, &stcb->sctp_ep->sctp_addr_list, sctp_nxt_addr) {
			if (laddr->ifa->localifa_flags & SCTP_BEING_DELETED) {
				SCTPDBG(SCTP_DEBUG_PCB1, "ifa being deleted\n");
				continue;
			}
			if (sctp_is_addr_restricted(stcb, laddr->ifa) &&
			    (!sctp_is_addr_pending(stcb, laddr->ifa))) {

				continue;
			}
			if (laddr->ifa->address.sa.sa_family != to->sa_family) {
				continue;
			}
			switch (to->sa_family) {
#ifdef INET
			case AF_INET:
			{
				struct sockaddr_in *sin, *rsin;

				sin = &laddr->ifa->address.sin;
				rsin = (struct sockaddr_in *)to;
				if (sin->sin_addr.s_addr == rsin->sin_addr.s_addr) {
					SCTP_IPI_ADDR_RUNLOCK();
					return (1);
				}
				break;
			}
#endif
#ifdef INET6
			case AF_INET6:
			{
				struct sockaddr_in6 *sin6, *rsin6;

				sin6 = &laddr->ifa->address.sin6;
				rsin6 = (struct sockaddr_in6 *)to;
				if (SCTP6_ARE_ADDR_EQUAL(sin6, rsin6)) {
					SCTP_IPI_ADDR_RUNLOCK();
					return (1);
				}
				break;
			}

#endif
#if defined(__Userspace__)
			case AF_CONN:
			{
				struct sockaddr_conn *sconn, *rsconn;

				sconn = &laddr->ifa->address.sconn;
				rsconn = (struct sockaddr_conn *)to;
				if (sconn->sconn_addr == rsconn->sconn_addr) {
					SCTP_IPI_ADDR_RUNLOCK();
					return (1);
				}
				break;
			}
#endif
			default:

				break;
			}
		}
	}
	SCTP_IPI_ADDR_RUNLOCK();
	return (0);
}

static struct sctp_tcb *
sctp_tcb_special_locate(struct sctp_inpcb **inp_p, struct sockaddr *from,
    struct sockaddr *to, struct sctp_nets **netp, uint32_t vrf_id)
{

	uint16_t lport, rport;
	struct sctppcbhead *ephead;
	struct sctp_inpcb *inp;
	struct sctp_laddr *laddr;
	struct sctp_tcb *stcb;
	struct sctp_nets *net;
#ifdef SCTP_MVRF
	int fnd, i;
#endif

	if ((to == NULL) || (from == NULL)) {
		return (NULL);
	}

	switch (to->sa_family) {
#ifdef INET
	case AF_INET:
		if (from->sa_family == AF_INET) {
			lport = ((struct sockaddr_in *)to)->sin_port;
			rport = ((struct sockaddr_in *)from)->sin_port;
		} else {
			return (NULL);
		}
		break;
#endif
#ifdef INET6
	case AF_INET6:
		if (from->sa_family == AF_INET6) {
			lport = ((struct sockaddr_in6 *)to)->sin6_port;
			rport = ((struct sockaddr_in6 *)from)->sin6_port;
		} else {
			return (NULL);
		}
		break;
#endif
#if defined(__Userspace__)
	case AF_CONN:
		if (from->sa_family == AF_CONN) {
			lport = ((struct sockaddr_conn *)to)->sconn_port;
			rport = ((struct sockaddr_conn *)from)->sconn_port;
		} else {
			return (NULL);
		}
		break;
#endif
	default:
		return (NULL);
	}
	ephead = &SCTP_BASE_INFO(sctp_tcpephash)[SCTP_PCBHASH_ALLADDR((lport | rport), SCTP_BASE_INFO(hashtcpmark))];

	LIST_FOREACH(inp, ephead, sctp_hash) {
		SCTP_INP_RLOCK(inp);
		if (inp->sctp_flags & SCTP_PCB_FLAGS_SOCKET_ALLGONE) {
			SCTP_INP_RUNLOCK(inp);
			continue;
		}
		if (lport != inp->sctp_lport) {
			SCTP_INP_RUNLOCK(inp);
			continue;
		}
#if defined(__FreeBSD__) && !defined(__Userspace__)
		switch (to->sa_family) {
#ifdef INET
		case AF_INET:
		{
			struct sockaddr_in *sin;

			sin = (struct sockaddr_in *)to;
			if (prison_check_ip4(inp->ip_inp.inp.inp_cred,
			                     &sin->sin_addr) != 0) {
				SCTP_INP_RUNLOCK(inp);
				continue;
			}
			break;
		}
#endif
#ifdef INET6
		case AF_INET6:
		{
			struct sockaddr_in6 *sin6;

			sin6 = (struct sockaddr_in6 *)to;
			if (prison_check_ip6(inp->ip_inp.inp.inp_cred,
			                     &sin6->sin6_addr) != 0) {
				SCTP_INP_RUNLOCK(inp);
				continue;
			}
			break;
		}
#endif
		default:
			SCTP_INP_RUNLOCK(inp);
			continue;
		}
#endif
#ifdef SCTP_MVRF
		fnd = 0;
		for (i = 0; i < inp->num_vrfs; i++) {
			if (inp->m_vrf_ids[i] == vrf_id) {
				fnd = 1;
				break;
			}
		}
		if (fnd == 0) {
			SCTP_INP_RUNLOCK(inp);
			continue;
		}
#else
		if (inp->def_vrf_id != vrf_id) {
			SCTP_INP_RUNLOCK(inp);
			continue;
		}
#endif

		if ((inp->sctp_flags & SCTP_PCB_FLAGS_BOUNDALL) == 0) {

			int match = 0;

			LIST_FOREACH(laddr, &inp->sctp_addr_list, sctp_nxt_addr) {
				if (laddr->ifa == NULL) {
					SCTPDBG(SCTP_DEBUG_PCB1, "%s: NULL ifa\n", __func__);
					continue;
				}
				if (laddr->ifa->localifa_flags & SCTP_BEING_DELETED) {
					SCTPDBG(SCTP_DEBUG_PCB1, "ifa being deleted\n");
					continue;
				}
				if (laddr->ifa->address.sa.sa_family ==
				    to->sa_family) {

#ifdef INET
					if (from->sa_family == AF_INET) {
						struct sockaddr_in *intf_addr, *sin;

						intf_addr = &laddr->ifa->address.sin;
						sin = (struct sockaddr_in *)to;
						if (sin->sin_addr.s_addr ==
						    intf_addr->sin_addr.s_addr) {
							match = 1;
							break;
						}
					}
#endif
#ifdef INET6
					if (from->sa_family == AF_INET6) {
						struct sockaddr_in6 *intf_addr6;
						struct sockaddr_in6 *sin6;

						sin6 = (struct sockaddr_in6 *)
						    to;
						intf_addr6 = &laddr->ifa->address.sin6;

						if (SCTP6_ARE_ADDR_EQUAL(sin6,
						    intf_addr6)) {
							match = 1;
							break;
						}
					}
#endif
#if defined(__Userspace__)
					if (from->sa_family == AF_CONN) {
						struct sockaddr_conn *intf_addr, *sconn;

						intf_addr = &laddr->ifa->address.sconn;
						sconn = (struct sockaddr_conn *)to;
						if (sconn->sconn_addr ==
						    intf_addr->sconn_addr) {
							match = 1;
							break;
						}
					}
#endif
				}
			}
			if (match == 0) {

				SCTP_INP_RUNLOCK(inp);
				continue;
			}
		}

		stcb = LIST_FIRST(&inp->sctp_asoc_list);
		if (stcb == NULL) {
			SCTP_INP_RUNLOCK(inp);
			continue;
		}
		SCTP_TCB_LOCK(stcb);
		if (!sctp_does_stcb_own_this_addr(stcb, to)) {
			SCTP_TCB_UNLOCK(stcb);
			SCTP_INP_RUNLOCK(inp);
			continue;
		}
		if (stcb->rport != rport) {

			SCTP_TCB_UNLOCK(stcb);
			SCTP_INP_RUNLOCK(inp);
			continue;
		}
		if (stcb->asoc.state & SCTP_STATE_ABOUT_TO_BE_FREED) {
			SCTP_TCB_UNLOCK(stcb);
			SCTP_INP_RUNLOCK(inp);
			continue;
		}
		if (!sctp_does_stcb_own_this_addr(stcb, to)) {
			SCTP_TCB_UNLOCK(stcb);
			SCTP_INP_RUNLOCK(inp);
			continue;
		}

		TAILQ_FOREACH(net, &stcb->asoc.nets, sctp_next) {
			if (net->ro._l_addr.sa.sa_family != from->sa_family) {

				continue;
			}
			switch (from->sa_family) {
#ifdef INET
			case AF_INET:
			{
				struct sockaddr_in *sin, *rsin;

				sin = (struct sockaddr_in *)&net->ro._l_addr;
				rsin = (struct sockaddr_in *)from;
				if (sin->sin_addr.s_addr ==
				    rsin->sin_addr.s_addr) {

					if (netp != NULL) {
						*netp = net;
					}

					*inp_p = inp;
					SCTP_INP_RUNLOCK(inp);
					return (stcb);
				}
				break;
			}
#endif
#ifdef INET6
			case AF_INET6:
			{
				struct sockaddr_in6 *sin6, *rsin6;

				sin6 = (struct sockaddr_in6 *)&net->ro._l_addr;
				rsin6 = (struct sockaddr_in6 *)from;
				if (SCTP6_ARE_ADDR_EQUAL(sin6,
				    rsin6)) {

					if (netp != NULL) {
						*netp = net;
					}

					*inp_p = inp;
					SCTP_INP_RUNLOCK(inp);
					return (stcb);
				}
				break;
			}
#endif
#if defined(__Userspace__)
			case AF_CONN:
			{
				struct sockaddr_conn *sconn, *rsconn;

				sconn = (struct sockaddr_conn *)&net->ro._l_addr;
				rsconn = (struct sockaddr_conn *)from;
				if (sconn->sconn_addr == rsconn->sconn_addr) {

					if (netp != NULL) {
						*netp = net;
					}

					*inp_p = inp;
					SCTP_INP_RUNLOCK(inp);
					return (stcb);
				}
				break;
			}
#endif
			default:

				break;
			}
		}
		SCTP_TCB_UNLOCK(stcb);
		SCTP_INP_RUNLOCK(inp);
	}
	return (NULL);
}

struct sctp_tcb *
sctp_findassociation_ep_addr(struct sctp_inpcb **inp_p, struct sockaddr *remote,
    struct sctp_nets **netp, struct sockaddr *local, struct sctp_tcb *locked_tcb)
{
	struct sctpasochead *head;
	struct sctp_inpcb *inp;
	struct sctp_tcb *stcb = NULL;
	struct sctp_nets *net;
	uint16_t rport;

	inp = *inp_p;
	switch (remote->sa_family) {
#ifdef INET
	case AF_INET:
		rport = (((struct sockaddr_in *)remote)->sin_port);
		break;
#endif
#ifdef INET6
	case AF_INET6:
		rport = (((struct sockaddr_in6 *)remote)->sin6_port);
		break;
#endif
#if defined(__Userspace__)
	case AF_CONN:
		rport = (((struct sockaddr_conn *)remote)->sconn_port);
		break;
#endif
	default:
		return (NULL);
	}
	if (locked_tcb) {

		atomic_add_int(&locked_tcb->asoc.refcnt, 1);
		SCTP_TCB_UNLOCK(locked_tcb);
	}
	SCTP_INP_INFO_RLOCK();
	if ((inp->sctp_flags & SCTP_PCB_FLAGS_TCPTYPE) ||
	    (inp->sctp_flags & SCTP_PCB_FLAGS_IN_TCPPOOL)) {

		if ((inp->sctp_socket) && SCTP_IS_LISTENING(inp)) {

#ifndef SCTP_MVRF
			stcb = sctp_tcb_special_locate(inp_p, remote, local,
			    netp, inp->def_vrf_id);
			if ((stcb != NULL) && (locked_tcb == NULL)) {

				SCTP_INP_DECR_REF(inp);
			}
			if ((locked_tcb != NULL) && (locked_tcb != stcb)) {
				SCTP_INP_RLOCK(locked_tcb->sctp_ep);
				SCTP_TCB_LOCK(locked_tcb);
				atomic_subtract_int(&locked_tcb->asoc.refcnt, 1);
				SCTP_INP_RUNLOCK(locked_tcb->sctp_ep);
			}
#else

			int i;

			for (i = 0; i < inp->num_vrfs; i++) {
				stcb = sctp_tcb_special_locate(inp_p, remote, local,
				                               netp, inp->m_vrf_ids[i]);
				if ((stcb != NULL) && (locked_tcb == NULL)) {

					SCTP_INP_DECR_REF(inp);
					break;
				}
				if ((locked_tcb != NULL) && (locked_tcb != stcb)) {
					SCTP_INP_RLOCK(locked_tcb->sctp_ep);
					SCTP_TCB_LOCK(locked_tcb);
					atomic_subtract_int(&locked_tcb->asoc.refcnt, 1);
					SCTP_INP_RUNLOCK(locked_tcb->sctp_ep);
					break;
				}
			}
#endif
			SCTP_INP_INFO_RUNLOCK();
			return (stcb);
		} else {
			SCTP_INP_WLOCK(inp);
			if (inp->sctp_flags & SCTP_PCB_FLAGS_SOCKET_ALLGONE) {
				goto null_return;
			}
			stcb = LIST_FIRST(&inp->sctp_asoc_list);
			if (stcb == NULL) {
				goto null_return;
			}
			SCTP_TCB_LOCK(stcb);

			if (stcb->rport != rport) {

				SCTP_TCB_UNLOCK(stcb);
				goto null_return;
			}
			if (stcb->asoc.state & SCTP_STATE_ABOUT_TO_BE_FREED) {
				SCTP_TCB_UNLOCK(stcb);
				goto null_return;
			}
			if (local && !sctp_does_stcb_own_this_addr(stcb, local)) {
				SCTP_TCB_UNLOCK(stcb);
				goto null_return;
			}

			TAILQ_FOREACH(net, &stcb->asoc.nets, sctp_next) {
#ifdef INVARIANTS
				if (net == (TAILQ_NEXT(net, sctp_next))) {
					panic("Corrupt net list");
				}
#endif
				if (net->ro._l_addr.sa.sa_family !=
				    remote->sa_family) {

					continue;
				}
				switch (remote->sa_family) {
#ifdef INET
				case AF_INET:
				{
					struct sockaddr_in *sin, *rsin;

					sin = (struct sockaddr_in *)
					    &net->ro._l_addr;
					rsin = (struct sockaddr_in *)remote;
					if (sin->sin_addr.s_addr ==
					    rsin->sin_addr.s_addr) {

						if (netp != NULL) {
							*netp = net;
						}
						if (locked_tcb == NULL) {
							SCTP_INP_DECR_REF(inp);
						} else if (locked_tcb != stcb) {
							SCTP_TCB_LOCK(locked_tcb);
						}
						if (locked_tcb) {
							atomic_subtract_int(&locked_tcb->asoc.refcnt, 1);
						}

						SCTP_INP_WUNLOCK(inp);
						SCTP_INP_INFO_RUNLOCK();
						return (stcb);
					}
					break;
				}
#endif
#ifdef INET6
				case AF_INET6:
				{
					struct sockaddr_in6 *sin6, *rsin6;

					sin6 = (struct sockaddr_in6 *)&net->ro._l_addr;
					rsin6 = (struct sockaddr_in6 *)remote;
					if (SCTP6_ARE_ADDR_EQUAL(sin6,
					    rsin6)) {

						if (netp != NULL) {
							*netp = net;
						}
						if (locked_tcb == NULL) {
							SCTP_INP_DECR_REF(inp);
						} else if (locked_tcb != stcb) {
							SCTP_TCB_LOCK(locked_tcb);
						}
						if (locked_tcb) {
							atomic_subtract_int(&locked_tcb->asoc.refcnt, 1);
						}
						SCTP_INP_WUNLOCK(inp);
						SCTP_INP_INFO_RUNLOCK();
						return (stcb);
					}
					break;
				}
#endif
#if defined(__Userspace__)
				case AF_CONN:
				{
					struct sockaddr_conn *sconn, *rsconn;

					sconn = (struct sockaddr_conn *)&net->ro._l_addr;
					rsconn = (struct sockaddr_conn *)remote;
					if (sconn->sconn_addr == rsconn->sconn_addr) {

						if (netp != NULL) {
							*netp = net;
						}
						if (locked_tcb == NULL) {
							SCTP_INP_DECR_REF(inp);
						} else if (locked_tcb != stcb) {
							SCTP_TCB_LOCK(locked_tcb);
						}
						if (locked_tcb) {
							atomic_subtract_int(&locked_tcb->asoc.refcnt, 1);
						}
						SCTP_INP_WUNLOCK(inp);
						SCTP_INP_INFO_RUNLOCK();
						return (stcb);
					}
					break;
				}
#endif
				default:

					break;
				}
			}
			SCTP_TCB_UNLOCK(stcb);
		}
	} else {
		SCTP_INP_WLOCK(inp);
		if (inp->sctp_flags & SCTP_PCB_FLAGS_SOCKET_ALLGONE) {
			goto null_return;
		}
		head = &inp->sctp_tcbhash[SCTP_PCBHASH_ALLADDR(rport,
		                                               inp->sctp_hashmark)];
		LIST_FOREACH(stcb, head, sctp_tcbhash) {
			if (stcb->rport != rport) {

				continue;
			}
			SCTP_TCB_LOCK(stcb);
			if (stcb->asoc.state & SCTP_STATE_ABOUT_TO_BE_FREED) {
				SCTP_TCB_UNLOCK(stcb);
				continue;
			}
			if (local && !sctp_does_stcb_own_this_addr(stcb, local)) {
				SCTP_TCB_UNLOCK(stcb);
				continue;
			}

			TAILQ_FOREACH(net, &stcb->asoc.nets, sctp_next) {
#ifdef INVARIANTS
				if (net == (TAILQ_NEXT(net, sctp_next))) {
					panic("Corrupt net list");
				}
#endif
				if (net->ro._l_addr.sa.sa_family !=
				    remote->sa_family) {

					continue;
				}
				switch (remote->sa_family) {
#ifdef INET
				case AF_INET:
				{
					struct sockaddr_in *sin, *rsin;

					sin = (struct sockaddr_in *)
					    &net->ro._l_addr;
					rsin = (struct sockaddr_in *)remote;
					if (sin->sin_addr.s_addr ==
					    rsin->sin_addr.s_addr) {

						if (netp != NULL) {
							*netp = net;
						}
						if (locked_tcb == NULL) {
							SCTP_INP_DECR_REF(inp);
						} else if (locked_tcb != stcb) {
							SCTP_TCB_LOCK(locked_tcb);
						}
						if (locked_tcb) {
							atomic_subtract_int(&locked_tcb->asoc.refcnt, 1);
						}
						SCTP_INP_WUNLOCK(inp);
						SCTP_INP_INFO_RUNLOCK();
						return (stcb);
					}
					break;
				}
#endif
#ifdef INET6
				case AF_INET6:
				{
					struct sockaddr_in6 *sin6, *rsin6;

					sin6 = (struct sockaddr_in6 *)
					    &net->ro._l_addr;
					rsin6 = (struct sockaddr_in6 *)remote;
					if (SCTP6_ARE_ADDR_EQUAL(sin6,
					    rsin6)) {

						if (netp != NULL) {
							*netp = net;
						}
						if (locked_tcb == NULL) {
							SCTP_INP_DECR_REF(inp);
						} else if (locked_tcb != stcb) {
							SCTP_TCB_LOCK(locked_tcb);
						}
						if (locked_tcb) {
							atomic_subtract_int(&locked_tcb->asoc.refcnt, 1);
						}
						SCTP_INP_WUNLOCK(inp);
						SCTP_INP_INFO_RUNLOCK();
						return (stcb);
					}
					break;
				}
#endif
#if defined(__Userspace__)
				case AF_CONN:
				{
					struct sockaddr_conn *sconn, *rsconn;

					sconn = (struct sockaddr_conn *)&net->ro._l_addr;
					rsconn = (struct sockaddr_conn *)remote;
					if (sconn->sconn_addr == rsconn->sconn_addr) {

						if (netp != NULL) {
							*netp = net;
						}
						if (locked_tcb == NULL) {
							SCTP_INP_DECR_REF(inp);
						} else if (locked_tcb != stcb) {
							SCTP_TCB_LOCK(locked_tcb);
						}
						if (locked_tcb) {
							atomic_subtract_int(&locked_tcb->asoc.refcnt, 1);
						}
						SCTP_INP_WUNLOCK(inp);
						SCTP_INP_INFO_RUNLOCK();
						return (stcb);
					}
					break;
				}
#endif
				default:

					break;
				}
			}
			SCTP_TCB_UNLOCK(stcb);
		}
	}
null_return:

	if (locked_tcb) {
		SCTP_TCB_LOCK(locked_tcb);
		atomic_subtract_int(&locked_tcb->asoc.refcnt, 1);
	}
	SCTP_INP_WUNLOCK(inp);
	SCTP_INP_INFO_RUNLOCK();

	return (NULL);
}

struct sctp_tcb *
sctp_findasoc_ep_asocid_locked(struct sctp_inpcb *inp, sctp_assoc_t asoc_id, int want_lock)
{

	struct sctpasochead *head;
	struct sctp_tcb *stcb;
	uint32_t id;

	if (inp->sctp_flags & SCTP_PCB_FLAGS_SOCKET_ALLGONE) {
		SCTP_PRINTF("TSNH ep_associd0\n");
		return (NULL);
	}
	id = (uint32_t)asoc_id;
	head = &inp->sctp_asocidhash[SCTP_PCBHASH_ASOC(id, inp->hashasocidmark)];
	if (head == NULL) {

		SCTP_PRINTF("TSNH ep_associd1\n");
		return (NULL);
	}
	LIST_FOREACH(stcb, head, sctp_tcbasocidhash) {
		if (stcb->asoc.assoc_id == id) {
			if (inp != stcb->sctp_ep) {

				SCTP_PRINTF("TSNH ep_associd2\n");
				continue;
			}
			if (stcb->asoc.state & SCTP_STATE_ABOUT_TO_BE_FREED) {
				continue;
			}
			if (want_lock) {
				SCTP_TCB_LOCK(stcb);
			}
			return (stcb);
		}
	}
	return (NULL);
}

struct sctp_tcb *
sctp_findassociation_ep_asocid(struct sctp_inpcb *inp, sctp_assoc_t asoc_id, int want_lock)
{
	struct sctp_tcb *stcb;

	SCTP_INP_RLOCK(inp);
	stcb = sctp_findasoc_ep_asocid_locked(inp, asoc_id, want_lock);
	SCTP_INP_RUNLOCK(inp);
	return (stcb);
}

static struct sctp_inpcb *
sctp_endpoint_probe(struct sockaddr *nam, struct sctppcbhead *head,
		    uint16_t lport, uint32_t vrf_id)
{
	struct sctp_inpcb *inp;
	struct sctp_laddr *laddr;
#ifdef INET
	struct sockaddr_in *sin;
#endif
#ifdef INET6
	struct sockaddr_in6 *sin6;
	struct sockaddr_in6 *intf_addr6;
#endif
#if defined(__Userspace__)
	struct sockaddr_conn *sconn;
#endif
#ifdef SCTP_MVRF
	int i;
#endif
	int  fnd;

#ifdef INET
	sin = NULL;
#endif
#ifdef INET6
	sin6 = NULL;
#endif
#if defined(__Userspace__)
	sconn = NULL;
#endif
	switch (nam->sa_family) {
#ifdef INET
	case AF_INET:
		sin = (struct sockaddr_in *)nam;
		break;
#endif
#ifdef INET6
	case AF_INET6:
		sin6 = (struct sockaddr_in6 *)nam;
		break;
#endif
#if defined(__Userspace__)
	case AF_CONN:
		sconn = (struct sockaddr_conn *)nam;
		break;
#endif
	default:

		return (NULL);
	}

	if (head == NULL)
		return (NULL);

	LIST_FOREACH(inp, head, sctp_hash) {
		SCTP_INP_RLOCK(inp);
		if (inp->sctp_flags & SCTP_PCB_FLAGS_SOCKET_ALLGONE) {
			SCTP_INP_RUNLOCK(inp);
			continue;
		}
		if ((inp->sctp_flags & SCTP_PCB_FLAGS_BOUNDALL) &&
		    (inp->sctp_lport == lport)) {

			switch (nam->sa_family) {
#ifdef INET
			case AF_INET:
				if ((inp->sctp_flags & SCTP_PCB_FLAGS_BOUND_V6) &&
				    SCTP_IPV6_V6ONLY(inp)) {

					SCTP_INP_RUNLOCK(inp);
					continue;
				}
#if defined(__FreeBSD__) && !defined(__Userspace__)
				if (prison_check_ip4(inp->ip_inp.inp.inp_cred,
				                     &sin->sin_addr) != 0) {
					SCTP_INP_RUNLOCK(inp);
					continue;
				}
#endif
				break;
#endif
#ifdef INET6
			case AF_INET6:

				if ((inp->sctp_flags & SCTP_PCB_FLAGS_BOUND_V6) == 0) {
					SCTP_INP_RUNLOCK(inp);
					continue;
				}
#if defined(__FreeBSD__) && !defined(__Userspace__)
				if (prison_check_ip6(inp->ip_inp.inp.inp_cred,
				                     &sin6->sin6_addr) != 0) {
					SCTP_INP_RUNLOCK(inp);
					continue;
				}
#endif
				break;
#endif
			default:
				break;
			}

			fnd = 0;
#ifdef SCTP_MVRF
			for (i = 0; i < inp->num_vrfs; i++) {
				if (inp->m_vrf_ids[i] == vrf_id) {
					fnd = 1;
					break;
				}
			}
#else
			if (inp->def_vrf_id == vrf_id)
				fnd = 1;
#endif

			SCTP_INP_RUNLOCK(inp);
			if (!fnd)
				continue;
			return (inp);
		}
		SCTP_INP_RUNLOCK(inp);
	}
	switch (nam->sa_family) {
#ifdef INET
	case AF_INET:
		if (sin->sin_addr.s_addr == INADDR_ANY) {

			return (NULL);
		}
		break;
#endif
#ifdef INET6
	case AF_INET6:
		if (IN6_IS_ADDR_UNSPECIFIED(&sin6->sin6_addr)) {

			return (NULL);
		}
		break;
#endif
#if defined(__Userspace__)
	case AF_CONN:
		if (sconn->sconn_addr == NULL) {
			return (NULL);
		}
		break;
#endif
	default:
		break;
	}

	LIST_FOREACH(inp, head, sctp_hash) {
		SCTP_INP_RLOCK(inp);
		if (inp->sctp_flags & SCTP_PCB_FLAGS_SOCKET_ALLGONE) {
			SCTP_INP_RUNLOCK(inp);
			continue;
		}
		if ((inp->sctp_flags & SCTP_PCB_FLAGS_BOUNDALL)) {
			SCTP_INP_RUNLOCK(inp);
			continue;
		}

		if (inp->sctp_lport != lport) {
			SCTP_INP_RUNLOCK(inp);
			continue;
		}

		fnd = 0;
#ifdef SCTP_MVRF
		for (i = 0; i < inp->num_vrfs; i++) {
			if (inp->m_vrf_ids[i] == vrf_id) {
				fnd = 1;
				break;
			}
		}
#else
		if (inp->def_vrf_id == vrf_id)
			fnd = 1;

#endif
		if (!fnd) {
			SCTP_INP_RUNLOCK(inp);
			continue;
		}
		LIST_FOREACH(laddr, &inp->sctp_addr_list, sctp_nxt_addr) {
			if (laddr->ifa == NULL) {
				SCTPDBG(SCTP_DEBUG_PCB1, "%s: NULL ifa\n",
					__func__);
				continue;
			}
			SCTPDBG(SCTP_DEBUG_PCB1, "Ok laddr->ifa:%p is possible, ",
				(void *)laddr->ifa);
			if (laddr->ifa->localifa_flags & SCTP_BEING_DELETED) {
				SCTPDBG(SCTP_DEBUG_PCB1, "Huh IFA being deleted\n");
				continue;
			}
			if (laddr->ifa->address.sa.sa_family == nam->sa_family) {

				switch (nam->sa_family) {
#ifdef INET
				case AF_INET:
#if defined(__APPLE__) && !defined(__Userspace__)
					if (sin == NULL) {

						break;
					}
#endif
					if (sin->sin_addr.s_addr ==
					    laddr->ifa->address.sin.sin_addr.s_addr) {
						SCTP_INP_RUNLOCK(inp);
						return (inp);
					}
					break;
#endif
#ifdef INET6
				case AF_INET6:
					intf_addr6 = &laddr->ifa->address.sin6;
					if (SCTP6_ARE_ADDR_EQUAL(sin6,
					    intf_addr6)) {
						SCTP_INP_RUNLOCK(inp);
						return (inp);
					}
					break;
#endif
#if defined(__Userspace__)
				case AF_CONN:
					if (sconn->sconn_addr == laddr->ifa->address.sconn.sconn_addr) {
						SCTP_INP_RUNLOCK(inp);
						return (inp);
					}
					break;
#endif
				}
			}
		}
		SCTP_INP_RUNLOCK(inp);
	}
	return (NULL);
}

static struct sctp_inpcb *
sctp_isport_inuse(struct sctp_inpcb *inp, uint16_t lport, uint32_t vrf_id)
{
	struct sctppcbhead *head;
	struct sctp_inpcb *t_inp;
#ifdef SCTP_MVRF
	int i;
#endif
	int fnd;

	head = &SCTP_BASE_INFO(sctp_ephash)[SCTP_PCBHASH_ALLADDR(lport,
	    SCTP_BASE_INFO(hashmark))];
	LIST_FOREACH(t_inp, head, sctp_hash) {
		if (t_inp->sctp_lport != lport) {
			continue;
		}

		fnd = 0;
#ifdef SCTP_MVRF
		for (i = 0; i < inp->num_vrfs; i++) {
			if (t_inp->m_vrf_ids[i] == vrf_id) {
				fnd = 1;
				break;
			}
		}
#else
		if (t_inp->def_vrf_id == vrf_id)
			fnd = 1;
#endif
		if (!fnd)
			continue;

		if ((t_inp->sctp_flags & SCTP_PCB_FLAGS_BOUND_V6) &&
		    SCTP_IPV6_V6ONLY(t_inp)) {
			if (inp->sctp_flags & SCTP_PCB_FLAGS_BOUND_V6) {

				return (t_inp);
			} else {

				continue;
			}
		} else if (t_inp->sctp_flags & SCTP_PCB_FLAGS_BOUND_V6) {

			return (t_inp);
		} else {

			if ((inp->sctp_flags & SCTP_PCB_FLAGS_BOUND_V6) &&
			    SCTP_IPV6_V6ONLY(inp)) {

				continue;
			}

		}
		return (t_inp);
	}
	return (NULL);
}

int
sctp_swap_inpcb_for_listen(struct sctp_inpcb *inp)
{

	struct sctppcbhead *head;
	struct sctp_inpcb *tinp, *ninp;

	SCTP_INP_INFO_WLOCK_ASSERT();
	SCTP_INP_WLOCK_ASSERT(inp);

	if (sctp_is_feature_off(inp, SCTP_PCB_FLAGS_PORTREUSE)) {

		return (-1);
	}
	if ((inp->sctp_flags & SCTP_PCB_FLAGS_IN_TCPPOOL) == 0) {
		return (0);
	}
	SCTP_INP_WUNLOCK(inp);
	head = &SCTP_BASE_INFO(sctp_ephash)[SCTP_PCBHASH_ALLADDR(inp->sctp_lport,
	                                    SCTP_BASE_INFO(hashmark))];

	LIST_FOREACH_SAFE(tinp, head, sctp_hash, ninp) {
		if (tinp->sctp_lport != inp->sctp_lport) {
			continue;
		}
		if (tinp->sctp_flags & SCTP_PCB_FLAGS_SOCKET_ALLGONE) {
			continue;
		}
		if (tinp->sctp_flags & SCTP_PCB_FLAGS_SOCKET_GONE) {
			continue;
		}
		if (SCTP_IS_LISTENING(tinp)) {
			continue;
		}
		SCTP_INP_WLOCK(tinp);
		LIST_REMOVE(tinp, sctp_hash);
		head = &SCTP_BASE_INFO(sctp_tcpephash)[SCTP_PCBHASH_ALLADDR(tinp->sctp_lport, SCTP_BASE_INFO(hashtcpmark))];
		tinp->sctp_flags |= SCTP_PCB_FLAGS_IN_TCPPOOL;
		LIST_INSERT_HEAD(head, tinp, sctp_hash);
		SCTP_INP_WUNLOCK(tinp);
	}
	SCTP_INP_WLOCK(inp);

	LIST_REMOVE(inp, sctp_hash);
	inp->sctp_flags &= ~SCTP_PCB_FLAGS_IN_TCPPOOL;
	head = &SCTP_BASE_INFO(sctp_ephash)[SCTP_PCBHASH_ALLADDR(inp->sctp_lport, SCTP_BASE_INFO(hashmark))];
	LIST_INSERT_HEAD(head, inp, sctp_hash);
	return (0);
}

struct sctp_inpcb *
sctp_pcb_findep(struct sockaddr *nam, int find_tcp_pool, int have_lock,
		uint32_t vrf_id)
{

	struct sctp_inpcb *inp;
	struct sctppcbhead *head;
	int lport;
	unsigned int i;
#ifdef INET
	struct sockaddr_in *sin;
#endif
#ifdef INET6
	struct sockaddr_in6 *sin6;
#endif
#if defined(__Userspace__)
	struct sockaddr_conn *sconn;
#endif

	switch (nam->sa_family) {
#ifdef INET
	case AF_INET:
		sin = (struct sockaddr_in *)nam;
		lport = sin->sin_port;
		break;
#endif
#ifdef INET6
	case AF_INET6:
		sin6 = (struct sockaddr_in6 *)nam;
		lport = sin6->sin6_port;
		break;
#endif
#if defined(__Userspace__)
	case AF_CONN:
		sconn = (struct sockaddr_conn *)nam;
		lport = sconn->sconn_port;
		break;
#endif
	default:
		return (NULL);
	}

	if (have_lock == 0) {
		SCTP_INP_INFO_RLOCK();
	}
	head = &SCTP_BASE_INFO(sctp_ephash)[SCTP_PCBHASH_ALLADDR(lport,
	    SCTP_BASE_INFO(hashmark))];
	inp = sctp_endpoint_probe(nam, head, lport, vrf_id);

	if (inp == NULL && find_tcp_pool) {
		for (i = 0; i < SCTP_BASE_INFO(hashtcpmark) + 1; i++) {
			head = &SCTP_BASE_INFO(sctp_tcpephash)[i];
			inp = sctp_endpoint_probe(nam, head, lport, vrf_id);
			if (inp) {
				break;
			}
		}
	}
	if (inp) {
		SCTP_INP_INCR_REF(inp);
	}
	if (have_lock == 0) {
		SCTP_INP_INFO_RUNLOCK();
	}
	return (inp);
}

struct sctp_tcb *
sctp_findassociation_addr_sa(struct sockaddr *from, struct sockaddr *to,
    struct sctp_inpcb **inp_p, struct sctp_nets **netp, int find_tcp_pool,
    uint32_t vrf_id)
{
	struct sctp_inpcb *inp = NULL;
	struct sctp_tcb *stcb;

	SCTP_INP_INFO_RLOCK();
	if (find_tcp_pool) {
		if (inp_p != NULL) {
			stcb = sctp_tcb_special_locate(inp_p, from, to, netp,
			                               vrf_id);
		} else {
			stcb = sctp_tcb_special_locate(&inp, from, to, netp,
			                               vrf_id);
		}
		if (stcb != NULL) {
			SCTP_INP_INFO_RUNLOCK();
			return (stcb);
		}
	}
	inp = sctp_pcb_findep(to, 0, 1, vrf_id);
	if (inp_p != NULL) {
		*inp_p = inp;
	}
	SCTP_INP_INFO_RUNLOCK();
	if (inp == NULL) {
		return (NULL);
	}

	if (inp_p != NULL) {
		stcb = sctp_findassociation_ep_addr(inp_p, from, netp, to,
		                                    NULL);
	} else {
		stcb = sctp_findassociation_ep_addr(&inp, from, netp, to,
		                                    NULL);
	}
	return (stcb);
}

static struct sctp_tcb *
sctp_findassociation_special_addr(struct mbuf *m, int offset,
    struct sctphdr *sh, struct sctp_inpcb **inp_p, struct sctp_nets **netp,
    struct sockaddr *dst)
{
	struct sctp_paramhdr *phdr, param_buf;
#if defined(INET) || defined(INET6)
	struct sctp_tcb *stcb;
	uint16_t ptype;
#endif
	uint16_t plen;
#ifdef INET
	struct sockaddr_in sin4;
#endif
#ifdef INET6
	struct sockaddr_in6 sin6;
#endif

#ifdef INET
	memset(&sin4, 0, sizeof(sin4));
#ifdef HAVE_SIN_LEN
	sin4.sin_len = sizeof(sin4);
#endif
	sin4.sin_family = AF_INET;
	sin4.sin_port = sh->src_port;
#endif
#ifdef INET6
	memset(&sin6, 0, sizeof(sin6));
#ifdef HAVE_SIN6_LEN
	sin6.sin6_len = sizeof(sin6);
#endif
	sin6.sin6_family = AF_INET6;
	sin6.sin6_port = sh->src_port;
#endif

	offset += sizeof(struct sctp_init_chunk);

	phdr = sctp_get_next_param(m, offset, &param_buf, sizeof(param_buf));
	while (phdr != NULL) {

#if defined(INET) || defined(INET6)
		ptype = ntohs(phdr->param_type);
#endif
		plen = ntohs(phdr->param_length);
		if (plen == 0) {
			break;
		}
#ifdef INET
		if (ptype == SCTP_IPV4_ADDRESS &&
		    plen == sizeof(struct sctp_ipv4addr_param)) {

			struct sctp_ipv4addr_param ip4_param, *p4;

			phdr = sctp_get_next_param(m, offset,
			    (struct sctp_paramhdr *)&ip4_param, sizeof(ip4_param));
			if (phdr == NULL) {
				return (NULL);
			}
			p4 = (struct sctp_ipv4addr_param *)phdr;
			memcpy(&sin4.sin_addr, &p4->addr, sizeof(p4->addr));

			stcb = sctp_findassociation_ep_addr(inp_p,
			    (struct sockaddr *)&sin4, netp, dst, NULL);
			if (stcb != NULL) {
				return (stcb);
			}
		}
#endif
#ifdef INET6
		if (ptype == SCTP_IPV6_ADDRESS &&
		    plen == sizeof(struct sctp_ipv6addr_param)) {

			struct sctp_ipv6addr_param ip6_param, *p6;

			phdr = sctp_get_next_param(m, offset,
			    (struct sctp_paramhdr *)&ip6_param, sizeof(ip6_param));
			if (phdr == NULL) {
				return (NULL);
			}
			p6 = (struct sctp_ipv6addr_param *)phdr;
			memcpy(&sin6.sin6_addr, &p6->addr, sizeof(p6->addr));

			stcb = sctp_findassociation_ep_addr(inp_p,
			    (struct sockaddr *)&sin6, netp, dst, NULL);
			if (stcb != NULL) {
				return (stcb);
			}
		}
#endif
		offset += SCTP_SIZE32(plen);
		phdr = sctp_get_next_param(m, offset, &param_buf,
					   sizeof(param_buf));
	}
	return (NULL);
}

static struct sctp_tcb *
sctp_findassoc_by_vtag(struct sockaddr *from, struct sockaddr *to, uint32_t vtag,
		       struct sctp_inpcb **inp_p, struct sctp_nets **netp, uint16_t rport,
		       uint16_t lport, int skip_src_check, uint32_t vrf_id, uint32_t remote_tag)
{

	struct sctpasochead *head;
	struct sctp_nets *net;
	struct sctp_tcb *stcb;
#ifdef SCTP_MVRF
	unsigned int i;
#endif

	SCTP_INP_INFO_RLOCK();
	head = &SCTP_BASE_INFO(sctp_asochash)[SCTP_PCBHASH_ASOC(vtag,
	                                                        SCTP_BASE_INFO(hashasocmark))];
	LIST_FOREACH(stcb, head, sctp_asocs) {
		SCTP_INP_RLOCK(stcb->sctp_ep);
		if (stcb->sctp_ep->sctp_flags & SCTP_PCB_FLAGS_SOCKET_ALLGONE) {
			SCTP_INP_RUNLOCK(stcb->sctp_ep);
			continue;
		}
#ifdef SCTP_MVRF
		for (i = 0; i < stcb->sctp_ep->num_vrfs; i++) {
			if (stcb->sctp_ep->m_vrf_ids[i] == vrf_id) {
				break;
			}
		}
		if (i == stcb->sctp_ep->num_vrfs) {
			SCTP_INP_RUNLOCK(inp);
			continue;
		}
#else
		if (stcb->sctp_ep->def_vrf_id != vrf_id) {
			SCTP_INP_RUNLOCK(stcb->sctp_ep);
			continue;
		}
#endif
		SCTP_TCB_LOCK(stcb);
		SCTP_INP_RUNLOCK(stcb->sctp_ep);
		if (stcb->asoc.my_vtag == vtag) {

			if (stcb->rport != rport) {
				SCTP_TCB_UNLOCK(stcb);
				continue;
			}
			if (stcb->sctp_ep->sctp_lport != lport) {
				SCTP_TCB_UNLOCK(stcb);
				continue;
			}
			if (stcb->asoc.state & SCTP_STATE_ABOUT_TO_BE_FREED) {
				SCTP_TCB_UNLOCK(stcb);
				continue;
			}

			if (sctp_does_stcb_own_this_addr(stcb, to) == 0) {

				SCTP_TCB_UNLOCK(stcb);
				continue;
			}
			if (remote_tag) {

				if (stcb->asoc.peer_vtag == remote_tag) {

					goto conclusive;
				}
			}
			if (skip_src_check) {
			conclusive:
			        if (from) {
					*netp = sctp_findnet(stcb, from);
				} else {
					*netp = NULL;
				}
				if (inp_p)
					*inp_p = stcb->sctp_ep;
				SCTP_INP_INFO_RUNLOCK();
				return (stcb);
			}
			net = sctp_findnet(stcb, from);
			if (net) {

				*netp = net;
				SCTP_STAT_INCR(sctps_vtagexpress);
				*inp_p = stcb->sctp_ep;
				SCTP_INP_INFO_RUNLOCK();
				return (stcb);
			} else {

				SCTP_STAT_INCR(sctps_vtagbogus);
			}
		}
		SCTP_TCB_UNLOCK(stcb);
	}
	SCTP_INP_INFO_RUNLOCK();
	return (NULL);
}

struct sctp_tcb *
sctp_findassociation_addr(struct mbuf *m, int offset,
    struct sockaddr *src, struct sockaddr *dst,
    struct sctphdr *sh, struct sctp_chunkhdr *ch,
    struct sctp_inpcb **inp_p, struct sctp_nets **netp, uint32_t vrf_id)
{
	struct sctp_tcb *stcb;
	struct sctp_inpcb *inp;

	if (sh->v_tag) {

		stcb = sctp_findassoc_by_vtag(src, dst, ntohl(sh->v_tag),
		                              inp_p, netp, sh->src_port, sh->dest_port, 0, vrf_id, 0);
		if (stcb) {
			return (stcb);
		}
	}

	if (inp_p) {
		stcb = sctp_findassociation_addr_sa(src, dst, inp_p, netp,
		                                    1, vrf_id);
		inp = *inp_p;
	} else {
		stcb = sctp_findassociation_addr_sa(src, dst, &inp, netp,
		                                    1, vrf_id);
	}
	SCTPDBG(SCTP_DEBUG_PCB1, "stcb:%p inp:%p\n", (void *)stcb, (void *)inp);
	if (stcb == NULL && inp) {

		if ((ch->chunk_type == SCTP_INITIATION) ||
		    (ch->chunk_type == SCTP_INITIATION_ACK)) {

			if (inp->sctp_flags & SCTP_PCB_FLAGS_IN_TCPPOOL) {
				if (inp_p) {
					*inp_p = NULL;
				}
				return (NULL);
			}
			stcb = sctp_findassociation_special_addr(m,
			    offset, sh, &inp, netp, dst);
			if (inp_p != NULL) {
				*inp_p = inp;
			}
		}
	}
	SCTPDBG(SCTP_DEBUG_PCB1, "stcb is %p\n", (void *)stcb);
	return (stcb);
}

struct sctp_tcb *
sctp_findassociation_ep_asconf(struct mbuf *m, int offset,
			       struct sockaddr *dst, struct sctphdr *sh,
                               struct sctp_inpcb **inp_p, struct sctp_nets **netp, uint32_t vrf_id)
{
	struct sctp_tcb *stcb;
	union sctp_sockstore remote_store;
	struct sctp_paramhdr param_buf, *phdr;
	int ptype;
	int zero_address = 0;
#ifdef INET
	struct sockaddr_in *sin;
#endif
#ifdef INET6
	struct sockaddr_in6 *sin6;
#endif

	memset(&remote_store, 0, sizeof(remote_store));
	phdr = sctp_get_next_param(m, offset + sizeof(struct sctp_asconf_chunk),
				   &param_buf, sizeof(struct sctp_paramhdr));
	if (phdr == NULL) {
		SCTPDBG(SCTP_DEBUG_INPUT3, "%s: failed to get asconf lookup addr\n",
			__func__);
		return NULL;
	}
	ptype = (int)((uint32_t) ntohs(phdr->param_type));

	switch (ptype) {
#ifdef INET6
	case SCTP_IPV6_ADDRESS:
	{

		struct sctp_ipv6addr_param *p6, p6_buf;

		if (ntohs(phdr->param_length) != sizeof(struct sctp_ipv6addr_param)) {
			return NULL;
		}
		p6 = (struct sctp_ipv6addr_param *)sctp_get_next_param(m,
								       offset + sizeof(struct sctp_asconf_chunk),
								       &p6_buf.ph, sizeof(p6_buf));
		if (p6 == NULL) {
			SCTPDBG(SCTP_DEBUG_INPUT3, "%s: failed to get asconf v6 lookup addr\n",
				__func__);
			return (NULL);
		}
		sin6 = &remote_store.sin6;
		sin6->sin6_family = AF_INET6;
#ifdef HAVE_SIN6_LEN
		sin6->sin6_len = sizeof(*sin6);
#endif
		sin6->sin6_port = sh->src_port;
		memcpy(&sin6->sin6_addr, &p6->addr, sizeof(struct in6_addr));
		if (IN6_IS_ADDR_UNSPECIFIED(&sin6->sin6_addr))
			zero_address = 1;
		break;
	}
#endif
#ifdef INET
	case SCTP_IPV4_ADDRESS:
	{

		struct sctp_ipv4addr_param *p4, p4_buf;

		if (ntohs(phdr->param_length) != sizeof(struct sctp_ipv4addr_param)) {
			return NULL;
		}
		p4 = (struct sctp_ipv4addr_param *)sctp_get_next_param(m,
								       offset + sizeof(struct sctp_asconf_chunk),
								       &p4_buf.ph, sizeof(p4_buf));
		if (p4 == NULL) {
			SCTPDBG(SCTP_DEBUG_INPUT3, "%s: failed to get asconf v4 lookup addr\n",
				__func__);
			return (NULL);
		}
		sin = &remote_store.sin;
		sin->sin_family = AF_INET;
#ifdef HAVE_SIN_LEN
		sin->sin_len = sizeof(*sin);
#endif
		sin->sin_port = sh->src_port;
		memcpy(&sin->sin_addr, &p4->addr, sizeof(struct in_addr));
		if (sin->sin_addr.s_addr == INADDR_ANY)
			zero_address = 1;
		break;
	}
#endif
	default:

		return NULL;
	}

	if (zero_address) {
	        stcb = sctp_findassoc_by_vtag(NULL, dst, ntohl(sh->v_tag), inp_p,
					      netp, sh->src_port, sh->dest_port, 1, vrf_id, 0);
		if (stcb != NULL) {
			SCTP_INP_DECR_REF(*inp_p);
		}
	} else {
		stcb = sctp_findassociation_ep_addr(inp_p,
		    &remote_store.sa, netp,
		    dst, NULL);
	}
	return (stcb);
}

int
sctp_inpcb_alloc(struct socket *so, uint32_t vrf_id)
{

	int i, error;
	struct sctp_inpcb *inp;
	struct sctp_pcb *m;
	struct timeval time;
	sctp_sharedkey_t *null_key;

	error = 0;

	SCTP_INP_INFO_WLOCK();
	inp = SCTP_ZONE_GET(SCTP_BASE_INFO(ipi_zone_ep), struct sctp_inpcb);
	if (inp == NULL) {
		SCTP_PRINTF("Out of SCTP-INPCB structures - no resources\n");
		SCTP_INP_INFO_WUNLOCK();
		SCTP_LTRACE_ERR_RET(inp, NULL, NULL, SCTP_FROM_SCTP_PCB, ENOBUFS);
		return (ENOBUFS);
	}

	memset(inp, 0, sizeof(*inp));

#if defined(__APPLE__) && !defined(__Userspace__)
	inp->ip_inp.inp.inp_state = INPCB_STATE_INUSE;
#endif

	inp->sctp_socket = so;
	inp->ip_inp.inp.inp_socket = so;
#if defined(__FreeBSD__) && !defined(__Userspace__)
	inp->ip_inp.inp.inp_cred = crhold(so->so_cred);
#endif
#ifdef INET6
#if !defined(__Userspace__) && !defined(_WIN32)
	if (INP_SOCKAF(so) == AF_INET6) {
		if (MODULE_GLOBAL(ip6_auto_flowlabel)) {
			inp->ip_inp.inp.inp_flags |= IN6P_AUTOFLOWLABEL;
		}
		if (MODULE_GLOBAL(ip6_v6only)) {
			inp->ip_inp.inp.inp_flags |= IN6P_IPV6_V6ONLY;
		}
	}
#endif
#endif
	inp->sctp_associd_counter = 1;
	inp->partial_delivery_point = SCTP_SB_LIMIT_RCV(so) >> SCTP_PARTIAL_DELIVERY_SHIFT;
	inp->sctp_frag_point = SCTP_DEFAULT_MAXSEGMENT;
	inp->max_cwnd = 0;
	inp->sctp_cmt_on_off = SCTP_BASE_SYSCTL(sctp_cmt_on_off);
	inp->ecn_supported = (uint8_t)SCTP_BASE_SYSCTL(sctp_ecn_enable);
	inp->prsctp_supported = (uint8_t)SCTP_BASE_SYSCTL(sctp_pr_enable);
	inp->auth_supported = (uint8_t)SCTP_BASE_SYSCTL(sctp_auth_enable);
	inp->asconf_supported = (uint8_t)SCTP_BASE_SYSCTL(sctp_asconf_enable);
	inp->reconfig_supported = (uint8_t)SCTP_BASE_SYSCTL(sctp_reconfig_enable);
	inp->nrsack_supported = (uint8_t)SCTP_BASE_SYSCTL(sctp_nrsack_enable);
	inp->pktdrop_supported = (uint8_t)SCTP_BASE_SYSCTL(sctp_pktdrop_enable);
	inp->idata_supported = 0;

#if defined(__FreeBSD__) && !defined(__Userspace__)
	inp->fibnum = so->so_fibnum;
#else
	inp->fibnum = 0;
#endif
#if defined(__Userspace__)
	inp->ulp_info = NULL;
	inp->recv_callback = NULL;
	inp->send_callback = NULL;
	inp->send_sb_threshold = 0;
#endif

	inp->sctp_asocidhash = SCTP_HASH_INIT(SCTP_STACK_VTAG_HASH_SIZE, &inp->hashasocidmark);
	if (inp->sctp_asocidhash == NULL) {
#if defined(__FreeBSD__) && !defined(__Userspace__)
		crfree(inp->ip_inp.inp.inp_cred);
#endif
		SCTP_ZONE_FREE(SCTP_BASE_INFO(ipi_zone_ep), inp);
		SCTP_INP_INFO_WUNLOCK();
		return (ENOBUFS);
	}
	SCTP_INCR_EP_COUNT();
	inp->ip_inp.inp.inp_ip_ttl = MODULE_GLOBAL(ip_defttl);
	SCTP_INP_INFO_WUNLOCK();

	so->so_pcb = (caddr_t)inp;

	if (SCTP_SO_TYPE(so) == SOCK_SEQPACKET) {

		inp->sctp_flags = (SCTP_PCB_FLAGS_UDPTYPE |
		    SCTP_PCB_FLAGS_UNBOUND);

	} else if (SCTP_SO_TYPE(so) == SOCK_STREAM) {

		inp->sctp_flags = (SCTP_PCB_FLAGS_TCPTYPE |
		    SCTP_PCB_FLAGS_UNBOUND);

		SOCK_LOCK(so);
		SCTP_CLEAR_SO_NBIO(so);
		SOCK_UNLOCK(so);
	} else {

		SCTP_LTRACE_ERR_RET(inp, NULL, NULL, SCTP_FROM_SCTP_PCB, EOPNOTSUPP);
		so->so_pcb = NULL;
#if defined(__FreeBSD__) && !defined(__Userspace__)
		crfree(inp->ip_inp.inp.inp_cred);
#endif
		SCTP_ZONE_FREE(SCTP_BASE_INFO(ipi_zone_ep), inp);
		return (EOPNOTSUPP);
	}
	if (SCTP_BASE_SYSCTL(sctp_default_frag_interleave) == SCTP_FRAG_LEVEL_1) {
		sctp_feature_on(inp, SCTP_PCB_FLAGS_FRAG_INTERLEAVE);
		sctp_feature_off(inp, SCTP_PCB_FLAGS_INTERLEAVE_STRMS);
	} else if (SCTP_BASE_SYSCTL(sctp_default_frag_interleave) == SCTP_FRAG_LEVEL_2) {
		sctp_feature_on(inp, SCTP_PCB_FLAGS_FRAG_INTERLEAVE);
		sctp_feature_on(inp, SCTP_PCB_FLAGS_INTERLEAVE_STRMS);
	} else if (SCTP_BASE_SYSCTL(sctp_default_frag_interleave) == SCTP_FRAG_LEVEL_0) {
		sctp_feature_off(inp, SCTP_PCB_FLAGS_FRAG_INTERLEAVE);
		sctp_feature_off(inp, SCTP_PCB_FLAGS_INTERLEAVE_STRMS);
	}
	inp->sctp_tcbhash = SCTP_HASH_INIT(SCTP_BASE_SYSCTL(sctp_pcbtblsize),
					   &inp->sctp_hashmark);
	if (inp->sctp_tcbhash == NULL) {
		SCTP_PRINTF("Out of SCTP-INPCB->hashinit - no resources\n");
		SCTP_LTRACE_ERR_RET(inp, NULL, NULL, SCTP_FROM_SCTP_PCB, ENOBUFS);
		so->so_pcb = NULL;
#if defined(__FreeBSD__) && !defined(__Userspace__)
		crfree(inp->ip_inp.inp.inp_cred);
#endif
		SCTP_ZONE_FREE(SCTP_BASE_INFO(ipi_zone_ep), inp);
		return (ENOBUFS);
	}
#ifdef SCTP_MVRF
	inp->vrf_size = SCTP_DEFAULT_VRF_SIZE;
	SCTP_MALLOC(inp->m_vrf_ids, uint32_t *,
		    (sizeof(uint32_t) * inp->vrf_size), SCTP_M_MVRF);
	if (inp->m_vrf_ids == NULL) {
		SCTP_LTRACE_ERR_RET(inp, NULL, NULL, SCTP_FROM_SCTP_PCB, ENOBUFS);
		so->so_pcb = NULL;
		SCTP_HASH_FREE(inp->sctp_tcbhash, inp->sctp_hashmark);
#if defined(__FreeBSD__) && !defined(__Userspace__)
		crfree(inp->ip_inp.inp.inp_cred);
#endif
		SCTP_ZONE_FREE(SCTP_BASE_INFO(ipi_zone_ep), inp);
		return (ENOBUFS);
	}
	inp->m_vrf_ids[0] = vrf_id;
	inp->num_vrfs = 1;
#endif
	inp->def_vrf_id = vrf_id;

#if defined(__APPLE__) && !defined(__Userspace__)
#if defined(APPLE_LEOPARD) || defined(APPLE_SNOWLEOPARD)
	inp->ip_inp.inp.inpcb_mtx = lck_mtx_alloc_init(SCTP_BASE_INFO(sctbinfo).mtx_grp, SCTP_BASE_INFO(sctbinfo).mtx_attr);
	if (inp->ip_inp.inp.inpcb_mtx == NULL) {
		SCTP_PRINTF("in_pcballoc: can't alloc mutex! so=%p\n", (void *)so);
#ifdef SCTP_MVRF
		SCTP_FREE(inp->m_vrf_ids, SCTP_M_MVRF);
#endif
		SCTP_HASH_FREE(inp->sctp_tcbhash, inp->sctp_hashmark);
		so->so_pcb = NULL;
		SCTP_ZONE_FREE(SCTP_BASE_INFO(ipi_zone_ep), inp);
		SCTP_UNLOCK_EXC(SCTP_BASE_INFO(sctbinfo).ipi_lock);
		SCTP_LTRACE_ERR_RET(inp, NULL, NULL, SCTP_FROM_SCTP_PCB, ENOMEM);
		return (ENOMEM);
	}
#elif defined(APPLE_LION) || defined(APPLE_MOUNTAINLION)
	lck_mtx_init(&inp->ip_inp.inp.inpcb_mtx, SCTP_BASE_INFO(sctbinfo).mtx_grp, SCTP_BASE_INFO(sctbinfo).mtx_attr);
#else
	lck_mtx_init(&inp->ip_inp.inp.inpcb_mtx, SCTP_BASE_INFO(sctbinfo).ipi_lock_grp, SCTP_BASE_INFO(sctbinfo).ipi_lock_attr);
#endif
#endif
	SCTP_INP_INFO_WLOCK();
	SCTP_INP_LOCK_INIT(inp);
#if defined(__FreeBSD__) && !defined(__Userspace__)
	INP_LOCK_INIT(&inp->ip_inp.inp, "inp", "sctpinp");
#endif
	SCTP_INP_READ_INIT(inp);
	SCTP_ASOC_CREATE_LOCK_INIT(inp);

	SCTP_INP_WLOCK(inp);

	LIST_INSERT_HEAD(&SCTP_BASE_INFO(listhead), inp, sctp_list);
#if defined(__APPLE__) && !defined(__Userspace__)
	inp->ip_inp.inp.inp_pcbinfo = &SCTP_BASE_INFO(sctbinfo);
#if defined(APPLE_LEOPARD) || defined(APPLE_SNOWLEOPARD) || defined(APPLE_LION) || defined(APPLE_MOUNTAINLION)
	LIST_INSERT_HEAD(SCTP_BASE_INFO(sctbinfo).listhead, &inp->ip_inp.inp, inp_list);
#else
	LIST_INSERT_HEAD(SCTP_BASE_INFO(sctbinfo).ipi_listhead, &inp->ip_inp.inp, inp_list);
#endif
#endif
	SCTP_INP_INFO_WUNLOCK();

	TAILQ_INIT(&inp->read_queue);
	LIST_INIT(&inp->sctp_addr_list);

	LIST_INIT(&inp->sctp_asoc_list);

#ifdef SCTP_TRACK_FREED_ASOCS

	LIST_INIT(&inp->sctp_asoc_free_list);
#endif

	SCTP_OS_TIMER_INIT(&inp->sctp_ep.signature_change.timer);
	inp->sctp_ep.signature_change.type = SCTP_TIMER_TYPE_NEWCOOKIE;

	m = &inp->sctp_ep;

	m->sctp_timeoutticks[SCTP_TIMER_SEND] = sctp_secs_to_ticks(SCTP_SEND_SEC);
	m->sctp_timeoutticks[SCTP_TIMER_INIT] = sctp_secs_to_ticks(SCTP_INIT_SEC);
	m->sctp_timeoutticks[SCTP_TIMER_RECV] = sctp_msecs_to_ticks(SCTP_BASE_SYSCTL(sctp_delayed_sack_time_default));
	m->sctp_timeoutticks[SCTP_TIMER_HEARTBEAT] = sctp_msecs_to_ticks(SCTP_BASE_SYSCTL(sctp_heartbeat_interval_default));
	m->sctp_timeoutticks[SCTP_TIMER_PMTU] = sctp_secs_to_ticks(SCTP_BASE_SYSCTL(sctp_pmtu_raise_time_default));
	m->sctp_timeoutticks[SCTP_TIMER_MAXSHUTDOWN] = sctp_secs_to_ticks(SCTP_BASE_SYSCTL(sctp_shutdown_guard_time_default));
	m->sctp_timeoutticks[SCTP_TIMER_SIGNATURE] = sctp_secs_to_ticks(SCTP_BASE_SYSCTL(sctp_secret_lifetime_default));

	m->sctp_maxrto = SCTP_BASE_SYSCTL(sctp_rto_max_default);
	m->sctp_minrto = SCTP_BASE_SYSCTL(sctp_rto_min_default);
	m->initial_rto = SCTP_BASE_SYSCTL(sctp_rto_initial_default);
	m->initial_init_rto_max = SCTP_BASE_SYSCTL(sctp_init_rto_max_default);
	m->sctp_sack_freq = SCTP_BASE_SYSCTL(sctp_sack_freq_default);
	m->max_init_times = SCTP_BASE_SYSCTL(sctp_init_rtx_max_default);
	m->max_send_times = SCTP_BASE_SYSCTL(sctp_assoc_rtx_max_default);
	m->def_net_failure = SCTP_BASE_SYSCTL(sctp_path_rtx_max_default);
	m->def_net_pf_threshold = SCTP_BASE_SYSCTL(sctp_path_pf_threshold);
	m->sctp_sws_sender = SCTP_SWS_SENDER_DEF;
	m->sctp_sws_receiver = SCTP_SWS_RECEIVER_DEF;
	m->max_burst = SCTP_BASE_SYSCTL(sctp_max_burst_default);
	m->fr_max_burst = SCTP_BASE_SYSCTL(sctp_fr_max_burst_default);

	m->sctp_default_cc_module = SCTP_BASE_SYSCTL(sctp_default_cc_module);
	m->sctp_default_ss_module = SCTP_BASE_SYSCTL(sctp_default_ss_module);
	m->max_open_streams_intome = SCTP_BASE_SYSCTL(sctp_nr_incoming_streams_default);

	m->pre_open_stream_count = SCTP_BASE_SYSCTL(sctp_nr_outgoing_streams_default);

	m->default_mtu = 0;

	m->adaptation_layer_indicator = 0;
	m->adaptation_layer_indicator_provided = 0;

	m->random_counter = 1;
	m->store_at = SCTP_SIGNATURE_SIZE;
	SCTP_READ_RANDOM(m->random_numbers, sizeof(m->random_numbers));
	sctp_fill_random_store(m);

	m->size_of_a_cookie = (sizeof(struct sctp_init_msg) * 2) +
	    sizeof(struct sctp_state_cookie);
	m->size_of_a_cookie += SCTP_SIGNATURE_SIZE;

	(void)SCTP_GETTIME_TIMEVAL(&time);
	m->time_of_secret_change = (unsigned int)time.tv_sec;

	for (i = 0; i < SCTP_NUMBER_OF_SECRETS; i++) {
		m->secret_key[0][i] = sctp_select_initial_TSN(m);
	}
	sctp_timer_start(SCTP_TIMER_TYPE_NEWCOOKIE, inp, NULL, NULL);

	m->def_cookie_life = sctp_msecs_to_ticks(SCTP_BASE_SYSCTL(sctp_valid_cookie_life_default));

	m->local_hmacs = sctp_default_supported_hmaclist();
	m->local_auth_chunks = sctp_alloc_chunklist();
	if (inp->asconf_supported) {
		sctp_auth_add_chunk(SCTP_ASCONF, m->local_auth_chunks);
		sctp_auth_add_chunk(SCTP_ASCONF_ACK, m->local_auth_chunks);
	}
	m->default_dscp = 0;
#ifdef INET6
	m->default_flowlabel = 0;
#endif
	m->port = 0;
	LIST_INIT(&m->shared_keys);

	null_key = sctp_alloc_sharedkey();
	sctp_insert_sharedkey(&m->shared_keys, null_key);
	SCTP_INP_WUNLOCK(inp);
#ifdef SCTP_LOG_CLOSING
	sctp_log_closing(inp, NULL, 12);
#endif
	return (error);
}

void
sctp_move_pcb_and_assoc(struct sctp_inpcb *old_inp, struct sctp_inpcb *new_inp,
    struct sctp_tcb *stcb)
{
	struct sctp_nets *net;
	uint16_t lport, rport;
	struct sctppcbhead *head;
	struct sctp_laddr *laddr, *oladdr;

	atomic_add_int(&stcb->asoc.refcnt, 1);
	SCTP_TCB_UNLOCK(stcb);
	SCTP_INP_INFO_WLOCK();
	SCTP_INP_WLOCK(old_inp);
	SCTP_INP_WLOCK(new_inp);
	SCTP_TCB_LOCK(stcb);
	atomic_subtract_int(&stcb->asoc.refcnt, 1);

	new_inp->sctp_ep.time_of_secret_change =
	    old_inp->sctp_ep.time_of_secret_change;
	memcpy(new_inp->sctp_ep.secret_key, old_inp->sctp_ep.secret_key,
	    sizeof(old_inp->sctp_ep.secret_key));
	new_inp->sctp_ep.current_secret_number =
	    old_inp->sctp_ep.current_secret_number;
	new_inp->sctp_ep.last_secret_number =
	    old_inp->sctp_ep.last_secret_number;
	new_inp->sctp_ep.size_of_a_cookie = old_inp->sctp_ep.size_of_a_cookie;

	stcb->sctp_socket = new_inp->sctp_socket;
	stcb->sctp_ep = new_inp;

	lport = new_inp->sctp_lport = old_inp->sctp_lport;
	rport = stcb->rport;

	LIST_REMOVE(stcb, sctp_tcbhash);
	LIST_REMOVE(stcb, sctp_tcblist);
	if (stcb->asoc.in_asocid_hash) {
		LIST_REMOVE(stcb, sctp_tcbasocidhash);
	}

	head = &SCTP_BASE_INFO(sctp_tcpephash)[SCTP_PCBHASH_ALLADDR((lport | rport), SCTP_BASE_INFO(hashtcpmark))];

	LIST_INSERT_HEAD(head, new_inp, sctp_hash);

	new_inp->sctp_flags &= ~SCTP_PCB_FLAGS_UNBOUND;

	LIST_INSERT_HEAD(&new_inp->sctp_asoc_list, stcb, sctp_tcblist);

	if (stcb->asoc.in_asocid_hash) {
		struct sctpasochead *lhd;
		lhd = &new_inp->sctp_asocidhash[SCTP_PCBHASH_ASOC(stcb->asoc.assoc_id,
			new_inp->hashasocidmark)];
		LIST_INSERT_HEAD(lhd, stcb, sctp_tcbasocidhash);
	}

	TAILQ_FOREACH(net, &stcb->asoc.nets, sctp_next) {
		sctp_timer_start(SCTP_TIMER_TYPE_PATHMTURAISE, new_inp,
		    stcb, net);
	}

	SCTP_INP_INFO_WUNLOCK();
	if (new_inp->sctp_tcbhash != NULL) {
		SCTP_HASH_FREE(new_inp->sctp_tcbhash, new_inp->sctp_hashmark);
		new_inp->sctp_tcbhash = NULL;
	}
	if ((new_inp->sctp_flags & SCTP_PCB_FLAGS_BOUNDALL) == 0) {

		LIST_FOREACH(oladdr, &old_inp->sctp_addr_list, sctp_nxt_addr) {
			laddr = SCTP_ZONE_GET(SCTP_BASE_INFO(ipi_zone_laddr), struct sctp_laddr);
			if (laddr == NULL) {

				SCTPDBG(SCTP_DEBUG_PCB1, "Association hosed in TCP model, out of laddr memory\n");
				continue;
			}
			SCTP_INCR_LADDR_COUNT();
			memset(laddr, 0, sizeof(*laddr));
			(void)SCTP_GETTIME_TIMEVAL(&laddr->start_time);
			laddr->ifa = oladdr->ifa;
			atomic_add_int(&laddr->ifa->refcount, 1);
			LIST_INSERT_HEAD(&new_inp->sctp_addr_list, laddr,
			    sctp_nxt_addr);
			new_inp->laddr_count++;
			if (oladdr == stcb->asoc.last_used_address) {
				stcb->asoc.last_used_address = laddr;
			}
		}
	}

	if (stcb->asoc.dack_timer.ep == old_inp) {
		SCTP_INP_DECR_REF(old_inp);
		stcb->asoc.dack_timer.ep = new_inp;
		SCTP_INP_INCR_REF(new_inp);
	}
	if (stcb->asoc.asconf_timer.ep == old_inp) {
		SCTP_INP_DECR_REF(old_inp);
		stcb->asoc.asconf_timer.ep = new_inp;
		SCTP_INP_INCR_REF(new_inp);
	}
	if (stcb->asoc.strreset_timer.ep == old_inp) {
		SCTP_INP_DECR_REF(old_inp);
		stcb->asoc.strreset_timer.ep = new_inp;
		SCTP_INP_INCR_REF(new_inp);
	}
	if (stcb->asoc.shut_guard_timer.ep == old_inp) {
		SCTP_INP_DECR_REF(old_inp);
		stcb->asoc.shut_guard_timer.ep = new_inp;
		SCTP_INP_INCR_REF(new_inp);
	}
	if (stcb->asoc.autoclose_timer.ep == old_inp) {
		SCTP_INP_DECR_REF(old_inp);
		stcb->asoc.autoclose_timer.ep = new_inp;
		SCTP_INP_INCR_REF(new_inp);
	}
	if (stcb->asoc.delete_prim_timer.ep == old_inp) {
		SCTP_INP_DECR_REF(old_inp);
		stcb->asoc.delete_prim_timer.ep = new_inp;
		SCTP_INP_INCR_REF(new_inp);
	}

	TAILQ_FOREACH(net, &stcb->asoc.nets, sctp_next) {
		if (net->pmtu_timer.ep == old_inp) {
			SCTP_INP_DECR_REF(old_inp);
			net->pmtu_timer.ep = new_inp;
			SCTP_INP_INCR_REF(new_inp);
		}
		if (net->hb_timer.ep == old_inp) {
			SCTP_INP_DECR_REF(old_inp);
			net->hb_timer.ep = new_inp;
			SCTP_INP_INCR_REF(new_inp);
		}
		if (net->rxt_timer.ep == old_inp) {
			SCTP_INP_DECR_REF(old_inp);
			net->rxt_timer.ep = new_inp;
			SCTP_INP_INCR_REF(new_inp);
		}
	}
	SCTP_INP_WUNLOCK(new_inp);
	SCTP_INP_WUNLOCK(old_inp);
}

static int
sctp_insert_laddr(struct sctpladdr *list, struct sctp_ifa *ifa, uint32_t act)
{
	struct sctp_laddr *laddr;

	laddr = SCTP_ZONE_GET(SCTP_BASE_INFO(ipi_zone_laddr), struct sctp_laddr);
	if (laddr == NULL) {

		SCTP_LTRACE_ERR_RET(NULL, NULL, NULL, SCTP_FROM_SCTP_PCB, EINVAL);
		return (EINVAL);
	}
	SCTP_INCR_LADDR_COUNT();
	memset(laddr, 0, sizeof(*laddr));
	(void)SCTP_GETTIME_TIMEVAL(&laddr->start_time);
	laddr->ifa = ifa;
	laddr->action = act;
	atomic_add_int(&ifa->refcount, 1);

	LIST_INSERT_HEAD(list, laddr, sctp_nxt_addr);

	return (0);
}

static void
sctp_remove_laddr(struct sctp_laddr *laddr)
{

	LIST_REMOVE(laddr, sctp_nxt_addr);
	sctp_free_ifa(laddr->ifa);
	SCTP_ZONE_FREE(SCTP_BASE_INFO(ipi_zone_laddr), laddr);
	SCTP_DECR_LADDR_COUNT();
}
#if !(defined(__FreeBSD__) || defined(__APPLE__) || defined(__Userspace__))

extern void in6_sin6_2_sin(struct sockaddr_in *, struct sockaddr_in6 *sin6);
#endif

int
#if defined(__FreeBSD__) && !defined(__Userspace__)
sctp_inpcb_bind_locked(struct sctp_inpcb *inp, struct sockaddr *addr,
                       struct sctp_ifa *sctp_ifap, struct thread *td)
#elif defined(_WIN32) && !defined(__Userspace__)
sctp_inpcb_bind_locked(struct sctp_inpcb *inp, struct sockaddr *addr,
                       struct sctp_ifa *sctp_ifap, PKTHREAD p)
#else
sctp_inpcb_bind_locked(struct sctp_inpcb *inp, struct sockaddr *addr,
                       struct sctp_ifa *sctp_ifap, struct proc *p)
#endif
{

	struct sctppcbhead *head;
	struct sctp_inpcb *inp_tmp;
#if (defined(__FreeBSD__) || defined(__APPLE__)) && !defined(__Userspace__)
	struct inpcb *ip_inp;
#endif
	int port_reuse_active = 0;
	int bindall;
#ifdef SCTP_MVRF
	int i;
#endif
	uint16_t lport;
	int error;
	uint32_t vrf_id;

#if defined(__FreeBSD__) && !defined(__Userspace__)
	KASSERT(td != NULL, ("%s: null thread", __func__));

#endif
	error = 0;
	lport = 0;
	bindall = 1;
#if (defined(__FreeBSD__) || defined(__APPLE__)) && !defined(__Userspace__)
	ip_inp = &inp->ip_inp.inp;
#endif

	SCTP_INP_INFO_WLOCK_ASSERT();
	SCTP_INP_WLOCK_ASSERT(inp);

#ifdef SCTP_DEBUG
	if (addr) {
		SCTPDBG(SCTP_DEBUG_PCB1, "Bind called port: %d\n",
			ntohs(((struct sockaddr_in *)addr)->sin_port));
		SCTPDBG(SCTP_DEBUG_PCB1, "Addr: ");
		SCTPDBG_ADDR(SCTP_DEBUG_PCB1, addr);
	}
#endif
	if ((inp->sctp_flags & SCTP_PCB_FLAGS_UNBOUND) == 0) {
		error = EINVAL;

		SCTP_LTRACE_ERR_RET(inp, NULL, NULL, SCTP_FROM_SCTP_PCB, error);
		goto out;
	}
	if (addr != NULL) {
		switch (addr->sa_family) {
#ifdef INET
		case AF_INET:
		{
			struct sockaddr_in *sin;

			if (SCTP_IPV6_V6ONLY(inp)) {
				error = EINVAL;
				SCTP_LTRACE_ERR_RET(inp, NULL, NULL, SCTP_FROM_SCTP_PCB, error);
				goto out;
			}
#ifdef HAVE_SA_LEN
			if (addr->sa_len != sizeof(*sin)) {
				error = EINVAL;
				SCTP_LTRACE_ERR_RET(inp, NULL, NULL, SCTP_FROM_SCTP_PCB, error);
				goto out;
			}
#endif

			sin = (struct sockaddr_in *)addr;
			lport = sin->sin_port;
#if defined(__FreeBSD__) && !defined(__Userspace__)

			if ((error = prison_local_ip4(td->td_ucred, &sin->sin_addr)) != 0) {
				SCTP_LTRACE_ERR_RET(inp, NULL, NULL, SCTP_FROM_SCTP_PCB, error);
				goto out;
			}
#endif
			if (sin->sin_addr.s_addr != INADDR_ANY) {
				bindall = 0;
			}
			break;
		}
#endif
#ifdef INET6
		case AF_INET6:
		{

			struct sockaddr_in6 *sin6;

			sin6 = (struct sockaddr_in6 *)addr;
#ifdef HAVE_SA_LEN
			if (addr->sa_len != sizeof(*sin6)) {
				error = EINVAL;
				SCTP_LTRACE_ERR_RET(inp, NULL, NULL, SCTP_FROM_SCTP_PCB, error);
				goto out;
			}
#endif
			lport = sin6->sin6_port;
#if defined(__FreeBSD__) && !defined(__Userspace__)

			if ((error = prison_local_ip6(td->td_ucred, &sin6->sin6_addr,
			    (SCTP_IPV6_V6ONLY(inp) != 0))) != 0) {
				SCTP_LTRACE_ERR_RET(inp, NULL, NULL, SCTP_FROM_SCTP_PCB, error);
				goto out;
			}
#endif
			if (!IN6_IS_ADDR_UNSPECIFIED(&sin6->sin6_addr)) {
				bindall = 0;
#ifdef SCTP_EMBEDDED_V6_SCOPE

#if defined(SCTP_KAME)
				if (sa6_embedscope(sin6, MODULE_GLOBAL(ip6_use_defzone)) != 0) {
					error = EINVAL;
					SCTP_LTRACE_ERR_RET(inp, NULL, NULL, SCTP_FROM_SCTP_PCB, error);
					goto out;
				}
#elif defined(__APPLE__) && !defined(__Userspace__)
#if defined(APPLE_LEOPARD) || defined(APPLE_SNOWLEOPARD)
				if (in6_embedscope(&sin6->sin6_addr, sin6, ip_inp, NULL) != 0) {
#else
				if (in6_embedscope(&sin6->sin6_addr, sin6, ip_inp, NULL, NULL) != 0) {
#endif
					error = EINVAL;
					SCTP_LTRACE_ERR_RET(inp, NULL, NULL, SCTP_FROM_SCTP_PCB, error);
					goto out;
				}
#elif defined(__FreeBSD__) && !defined(__Userspace__)
				error = scope6_check_id(sin6, MODULE_GLOBAL(ip6_use_defzone));
				if (error != 0) {
					error = EINVAL;
					SCTP_LTRACE_ERR_RET(inp, NULL, NULL, SCTP_FROM_SCTP_PCB, error);
					goto out;
				}
#else
				if (in6_embedscope(&sin6->sin6_addr, sin6) != 0) {
					error = EINVAL;
					SCTP_LTRACE_ERR_RET(inp, NULL, NULL, SCTP_FROM_SCTP_PCB, error);
					goto out;
				}
#endif
#endif
			}
#ifndef SCOPEDROUTING

			sin6->sin6_scope_id = 0;
#endif
			break;
		}
#endif
#if defined(__Userspace__)
		case AF_CONN:
		{
			struct sockaddr_conn *sconn;

#ifdef HAVE_SA_LEN
			if (addr->sa_len != sizeof(struct sockaddr_conn)) {
				error = EINVAL;
				SCTP_LTRACE_ERR_RET(inp, NULL, NULL, SCTP_FROM_SCTP_PCB, error);
				goto out;
			}
#endif
			sconn = (struct sockaddr_conn *)addr;
			lport = sconn->sconn_port;
			if (sconn->sconn_addr != NULL) {
				bindall = 0;
			}
			break;
		}
#endif
		default:
			error = EAFNOSUPPORT;
			SCTP_LTRACE_ERR_RET(inp, NULL, NULL, SCTP_FROM_SCTP_PCB, error);
			goto out;
		}
	}

	vrf_id = inp->def_vrf_id;

	if (lport) {

#if !(defined(_WIN32) && !defined(__Userspace__))
		if (ntohs(lport) < IPPORT_RESERVED &&
#if defined(__FreeBSD__) && !defined(__Userspace__)
		    (error = priv_check(td, PRIV_NETINET_RESERVEDPORT)) != 0) {
#elif defined(__APPLE__) && !defined(__Userspace__)
		    (error = suser(p->p_ucred, &p->p_acflag)) != 0) {
#elif defined(__Userspace__)

		    0) {
#else
		    (error = suser(p, 0)) != 0) {
#endif
			goto out;
		}
#endif
		SCTP_INP_INCR_REF(inp);
		SCTP_INP_WUNLOCK(inp);
		if (bindall) {
#ifdef SCTP_MVRF
			for (i = 0; i < inp->num_vrfs; i++) {
				vrf_id = inp->m_vrf_ids[i];
#else
				vrf_id = inp->def_vrf_id;
#endif
				inp_tmp = sctp_pcb_findep(addr, 0, 1, vrf_id);
				if (inp_tmp != NULL) {

					SCTP_INP_DECR_REF(inp_tmp);

					if ((sctp_is_feature_on(inp, SCTP_PCB_FLAGS_PORTREUSE)) &&
					    (sctp_is_feature_on(inp_tmp, SCTP_PCB_FLAGS_PORTREUSE))) {

						port_reuse_active = 1;
						goto continue_anyway;
					}
					SCTP_INP_WLOCK(inp);
					SCTP_INP_DECR_REF(inp);
					error = EADDRINUSE;
					SCTP_LTRACE_ERR_RET(inp, NULL, NULL, SCTP_FROM_SCTP_PCB, error);
					goto out;
				}
#ifdef SCTP_MVRF
			}
#endif
		} else {
			inp_tmp = sctp_pcb_findep(addr, 0, 1, vrf_id);
			if (inp_tmp != NULL) {

				SCTP_INP_DECR_REF(inp_tmp);

				if ((sctp_is_feature_on(inp, SCTP_PCB_FLAGS_PORTREUSE)) &&
				    (sctp_is_feature_on(inp_tmp, SCTP_PCB_FLAGS_PORTREUSE))) {

					port_reuse_active = 1;
					goto continue_anyway;
				}
				SCTP_INP_WLOCK(inp);
				SCTP_INP_DECR_REF(inp);
				error = EADDRINUSE;
				SCTP_LTRACE_ERR_RET(inp, NULL, NULL, SCTP_FROM_SCTP_PCB, error);
				goto out;
			}
		}
	continue_anyway:
		SCTP_INP_WLOCK(inp);
		SCTP_INP_DECR_REF(inp);
		if (bindall) {

			if ((port_reuse_active == 0) &&
			    (inp_tmp = sctp_isport_inuse(inp, lport, vrf_id))) {

				if ((sctp_is_feature_on(inp, SCTP_PCB_FLAGS_PORTREUSE)) &&
				    (sctp_is_feature_on(inp_tmp, SCTP_PCB_FLAGS_PORTREUSE))) {
					port_reuse_active = 1;
				} else {
					error = EADDRINUSE;
					SCTP_LTRACE_ERR_RET(inp, NULL, NULL, SCTP_FROM_SCTP_PCB, error);
					goto out;
				}
			}
		}
	} else {
		uint16_t first, last, candidate;
		uint16_t count;

#if defined(__Userspace__)
		first = MODULE_GLOBAL(ipport_firstauto);
		last = MODULE_GLOBAL(ipport_lastauto);
#elif defined(_WIN32)
		first = 1;
		last = 0xffff;
#elif defined(__FreeBSD__) || defined(__APPLE__)
		if (ip_inp->inp_flags & INP_HIGHPORT) {
			first = MODULE_GLOBAL(ipport_hifirstauto);
			last = MODULE_GLOBAL(ipport_hilastauto);
		} else if (ip_inp->inp_flags & INP_LOWPORT) {
#if defined(__FreeBSD__)
			if ((error = priv_check(td, PRIV_NETINET_RESERVEDPORT)) != 0) {
#else
			if ((error = suser(p->p_ucred, &p->p_acflag)) != 0) {
#endif
				SCTP_LTRACE_ERR_RET(inp, NULL, NULL, SCTP_FROM_SCTP_PCB, error);
				goto out;
			}
			first = MODULE_GLOBAL(ipport_lowfirstauto);
			last = MODULE_GLOBAL(ipport_lowlastauto);
		} else {
			first = MODULE_GLOBAL(ipport_firstauto);
			last = MODULE_GLOBAL(ipport_lastauto);
		}
#endif
		if (first > last) {
			uint16_t temp;

			temp = first;
			first = last;
			last = temp;
		}
		count = last - first + 1;
		candidate = first + sctp_select_initial_TSN(&inp->sctp_ep) % (count);

		for (;;) {
#ifdef SCTP_MVRF
			for (i = 0; i < inp->num_vrfs; i++) {
				if (sctp_isport_inuse(inp, htons(candidate), inp->m_vrf_ids[i]) != NULL) {
					break;
				}
			}
			if (i == inp->num_vrfs) {
				lport = htons(candidate);
				break;
			}
#else
			if (sctp_isport_inuse(inp, htons(candidate), inp->def_vrf_id) == NULL) {
				lport = htons(candidate);
				break;
			}
#endif
			if (--count == 0) {
				error = EADDRINUSE;
				SCTP_LTRACE_ERR_RET(inp, NULL, NULL, SCTP_FROM_SCTP_PCB, error);
				goto out;
			}
			if (candidate == last)
				candidate = first;
			else
				candidate = candidate + 1;
		}
	}
	if (inp->sctp_flags & (SCTP_PCB_FLAGS_SOCKET_GONE |
			       SCTP_PCB_FLAGS_SOCKET_ALLGONE)) {

		error = EINVAL;
		SCTP_LTRACE_ERR_RET(inp, NULL, NULL, SCTP_FROM_SCTP_PCB, error);
		goto out;
	}

	if (bindall) {

		inp->sctp_flags |= SCTP_PCB_FLAGS_BOUNDALL;

		if (SCTP_BASE_SYSCTL(sctp_auto_asconf) == 0) {
			sctp_feature_off(inp, SCTP_PCB_FLAGS_DO_ASCONF);
			sctp_feature_off(inp, SCTP_PCB_FLAGS_AUTO_ASCONF);
		} else {
			sctp_feature_on(inp, SCTP_PCB_FLAGS_DO_ASCONF);
			sctp_feature_on(inp, SCTP_PCB_FLAGS_AUTO_ASCONF);
		}
		if (SCTP_BASE_SYSCTL(sctp_multiple_asconfs) == 0) {
			sctp_feature_off(inp, SCTP_PCB_FLAGS_MULTIPLE_ASCONFS);
		} else {
			sctp_feature_on(inp, SCTP_PCB_FLAGS_MULTIPLE_ASCONFS);
		}

		if (SCTP_BASE_SYSCTL(sctp_mobility_base) == 0) {
			sctp_mobility_feature_off(inp, SCTP_MOBILITY_BASE);
			sctp_mobility_feature_off(inp, SCTP_MOBILITY_PRIM_DELETED);
		} else {
			sctp_mobility_feature_on(inp, SCTP_MOBILITY_BASE);
			sctp_mobility_feature_off(inp, SCTP_MOBILITY_PRIM_DELETED);
		}

		if (SCTP_BASE_SYSCTL(sctp_mobility_fasthandoff) == 0) {
			sctp_mobility_feature_off(inp, SCTP_MOBILITY_FASTHANDOFF);
			sctp_mobility_feature_off(inp, SCTP_MOBILITY_PRIM_DELETED);
		} else {
			sctp_mobility_feature_on(inp, SCTP_MOBILITY_FASTHANDOFF);
			sctp_mobility_feature_off(inp, SCTP_MOBILITY_PRIM_DELETED);
		}
	} else {

		struct sctp_ifa *ifa;
		union sctp_sockstore store;

		memset(&store, 0, sizeof(store));
		switch (addr->sa_family) {
#ifdef INET
		case AF_INET:
			memcpy(&store.sin, addr, sizeof(struct sockaddr_in));
			store.sin.sin_port = 0;
			break;
#endif
#ifdef INET6
		case AF_INET6:
			memcpy(&store.sin6, addr, sizeof(struct sockaddr_in6));
			store.sin6.sin6_port = 0;
			break;
#endif
#if defined(__Userspace__)
		case AF_CONN:
			memcpy(&store.sconn, addr, sizeof(struct sockaddr_conn));
			store.sconn.sconn_port = 0;
			break;
#endif
		default:
			break;
		}

		if (sctp_ifap != NULL) {
			ifa = sctp_ifap;
		} else {

			ifa = sctp_find_ifa_by_addr(&store.sa,
						    vrf_id, SCTP_ADDR_NOT_LOCKED);
		}
		if (ifa == NULL) {
			error = EADDRNOTAVAIL;

			SCTP_LTRACE_ERR_RET(inp, NULL, NULL, SCTP_FROM_SCTP_PCB, error);
			goto out;
		}
#ifdef INET6
		if (addr->sa_family == AF_INET6) {

			if (ifa->localifa_flags & SCTP_ADDR_IFA_UNUSEABLE) {

				error = EINVAL;
				SCTP_LTRACE_ERR_RET(inp, NULL, NULL, SCTP_FROM_SCTP_PCB, error);
				goto out;
			}
		}
#endif

		inp->sctp_flags &= ~SCTP_PCB_FLAGS_BOUNDALL;

		sctp_feature_on(inp, SCTP_PCB_FLAGS_DO_ASCONF);

		sctp_feature_off(inp, SCTP_PCB_FLAGS_AUTO_ASCONF);

		error = sctp_insert_laddr(&inp->sctp_addr_list, ifa, 0);
		if (error != 0)
			goto out;
		inp->laddr_count++;
	}

	if (port_reuse_active) {

		head = &SCTP_BASE_INFO(sctp_tcpephash)[SCTP_PCBHASH_ALLADDR(lport, SCTP_BASE_INFO(hashtcpmark))];
		inp->sctp_flags |= SCTP_PCB_FLAGS_IN_TCPPOOL;
	} else {
		head = &SCTP_BASE_INFO(sctp_ephash)[SCTP_PCBHASH_ALLADDR(lport, SCTP_BASE_INFO(hashmark))];
	}

	LIST_INSERT_HEAD(head, inp, sctp_hash);
	SCTPDBG(SCTP_DEBUG_PCB1, "Main hash to bind at head:%p, bound port:%d - in tcp_pool=%d\n",
		(void *)head, ntohs(lport), port_reuse_active);

	inp->sctp_lport = lport;

	KASSERT((inp->sctp_flags & SCTP_PCB_FLAGS_UNBOUND) != 0,
	    ("%s: inp %p is already bound", __func__, inp));
	inp->sctp_flags &= ~SCTP_PCB_FLAGS_UNBOUND;
out:
	return (error);
}

int
#if defined(__FreeBSD__) && !defined(__Userspace__)
sctp_inpcb_bind(struct socket *so, struct sockaddr *addr,
                struct sctp_ifa *sctp_ifap, struct thread *td)
#elif defined(_WIN32) && !defined(__Userspace__)
sctp_inpcb_bind(struct socket *so, struct sockaddr *addr,
                struct sctp_ifa *sctp_ifap, PKTHREAD p)
#else
sctp_inpcb_bind(struct socket *so, struct sockaddr *addr,
                struct sctp_ifa *sctp_ifap, struct proc *p)
#endif
{
	struct sctp_inpcb *inp;
	int error;

	inp = so->so_pcb;
	SCTP_INP_INFO_WLOCK();
	SCTP_INP_WLOCK(inp);
#if defined(__FreeBSD__) && !defined(__Userspace__)
	error = sctp_inpcb_bind_locked(inp, addr, sctp_ifap, td);
#else
	error = sctp_inpcb_bind_locked(inp, addr, sctp_ifap, p);
#endif
	SCTP_INP_WUNLOCK(inp);
	SCTP_INP_INFO_WUNLOCK();
	return (error);
}

static void
sctp_iterator_inp_being_freed(struct sctp_inpcb *inp)
{
	struct sctp_iterator *it, *nit;

	it = sctp_it_ctl.cur_it;
#if defined(__FreeBSD__) && !defined(__Userspace__)
	if (it && (it->vn != curvnet)) {

		return;
	}
#endif
	if (it && (it->inp == inp)) {

		if (it->iterator_flags & SCTP_ITERATOR_DO_SINGLE_INP) {
			sctp_it_ctl.iterator_flags |= SCTP_ITERATOR_STOP_CUR_IT;
		} else {
			sctp_it_ctl.iterator_flags |= SCTP_ITERATOR_STOP_CUR_INP;
		}
	}

	SCTP_IPI_ITERATOR_WQ_LOCK();
	TAILQ_FOREACH_SAFE(it, &sctp_it_ctl.iteratorhead, sctp_nxt_itr, nit) {
#if defined(__FreeBSD__) && !defined(__Userspace__)
		if (it->vn != curvnet) {
			continue;
		}
#endif
		if (it->inp == inp) {

			if (it->iterator_flags & SCTP_ITERATOR_DO_SINGLE_INP) {

				TAILQ_REMOVE(&sctp_it_ctl.iteratorhead,
				    it, sctp_nxt_itr);
				if (it->function_atend != NULL) {
					(*it->function_atend) (it->pointer, it->val);
				}
				SCTP_FREE(it, SCTP_M_ITER);
			} else {
				it->inp = LIST_NEXT(it->inp, sctp_list);
				if (it->inp) {
					SCTP_INP_INCR_REF(it->inp);
				}
			}

			SCTP_INP_DECR_REF(inp);
		}
	}
	SCTP_IPI_ITERATOR_WQ_UNLOCK();
}

void
sctp_inpcb_free(struct sctp_inpcb *inp, int immediate, int from)
{

	struct sctp_tcb *asoc, *nasoc;
	struct sctp_laddr *laddr, *nladdr;
	struct inpcb *ip_pcb;
	struct socket *so;
	int being_refed = 0;
	struct sctp_queued_to_read *sq, *nsq;
#if !defined(__Userspace__)
#if !defined(__FreeBSD__)
	sctp_rtentry_t *rt;
#endif
#endif
	int cnt;
	sctp_sharedkey_t *shared_key, *nshared_key;

#if defined(__APPLE__) && !defined(__Userspace__)
	sctp_lock_assert(SCTP_INP_SO(inp));
#endif
#ifdef SCTP_LOG_CLOSING
	sctp_log_closing(inp, NULL, 0);
#endif
	SCTP_ITERATOR_LOCK();

	sctp_iterator_inp_being_freed(inp);
	SCTP_ITERATOR_UNLOCK();

	SCTP_ASOC_CREATE_LOCK(inp);
	SCTP_INP_INFO_WLOCK();
	SCTP_INP_WLOCK(inp);
	so = inp->sctp_socket;
	KASSERT((inp->sctp_flags & SCTP_PCB_FLAGS_SOCKET_GONE) != 0,
	    ("%s: inp %p still has socket", __func__, inp));
	KASSERT((inp->sctp_flags & SCTP_PCB_FLAGS_SOCKET_ALLGONE) == 0,
	    ("%s: double free of inp %p", __func__, inp));
	if (from == SCTP_CALLED_AFTER_CMPSET_OFCLOSE) {
		inp->sctp_flags &= ~SCTP_PCB_FLAGS_CLOSE_IP;

		inp->sctp_flags |= SCTP_PCB_FLAGS_DONT_WAKE;
		inp->sctp_flags &= ~SCTP_PCB_FLAGS_WAKEINPUT;
		inp->sctp_flags &= ~SCTP_PCB_FLAGS_WAKEOUTPUT;
	}

	sctp_timer_stop(SCTP_TIMER_TYPE_NEWCOOKIE, inp, NULL, NULL,
	                SCTP_FROM_SCTP_PCB + SCTP_LOC_1);

	if (inp->control) {
		sctp_m_freem(inp->control);
		inp->control = NULL;
	}
	if (inp->pkt) {
		sctp_m_freem(inp->pkt);
		inp->pkt = NULL;
	}
	ip_pcb = &inp->ip_inp.inp;

	if (immediate == SCTP_FREE_SHOULD_USE_GRACEFUL_CLOSE) {
		int cnt_in_sd;

		cnt_in_sd = 0;
		LIST_FOREACH_SAFE(asoc, &inp->sctp_asoc_list, sctp_tcblist, nasoc) {
			SCTP_TCB_LOCK(asoc);
			if (asoc->asoc.state & SCTP_STATE_ABOUT_TO_BE_FREED) {
				asoc->sctp_socket = NULL;

				cnt_in_sd++;
				if (asoc->asoc.state & SCTP_STATE_IN_ACCEPT_QUEUE) {

					SCTP_CLEAR_SUBSTATE(asoc, SCTP_STATE_IN_ACCEPT_QUEUE);
					sctp_timer_start(SCTP_TIMER_TYPE_ASOCKILL, inp, asoc, NULL);
				}
				SCTP_TCB_UNLOCK(asoc);
				continue;
			}
			if (((SCTP_GET_STATE(asoc) == SCTP_STATE_COOKIE_WAIT) ||
			     (SCTP_GET_STATE(asoc) == SCTP_STATE_COOKIE_ECHOED)) &&
			    (asoc->asoc.total_output_queue_size == 0)) {

				if (sctp_free_assoc(inp, asoc, SCTP_PCBFREE_NOFORCE,
						   SCTP_FROM_SCTP_PCB + SCTP_LOC_2) == 0) {
					cnt_in_sd++;
				}
				continue;
			}

			asoc->sctp_socket = NULL;
			SCTP_ADD_SUBSTATE(asoc, SCTP_STATE_CLOSED_SOCKET);
			if ((asoc->asoc.size_on_reasm_queue > 0) ||
			    (asoc->asoc.control_pdapi) ||
			    (asoc->asoc.size_on_all_streams > 0) ||
			    (so && (so->so_rcv.sb_cc > 0))) {

				struct mbuf *op_err;

				op_err = sctp_generate_cause(SCTP_CAUSE_USER_INITIATED_ABT, "");
				asoc->sctp_ep->last_abort_code = SCTP_FROM_SCTP_PCB + SCTP_LOC_3;
				sctp_send_abort_tcb(asoc, op_err, SCTP_SO_LOCKED);
				SCTP_STAT_INCR_COUNTER32(sctps_aborted);
				if ((SCTP_GET_STATE(asoc) == SCTP_STATE_OPEN) ||
				    (SCTP_GET_STATE(asoc) == SCTP_STATE_SHUTDOWN_RECEIVED)) {
					SCTP_STAT_DECR_GAUGE32(sctps_currestab);
				}
				if (sctp_free_assoc(inp, asoc,
						    SCTP_PCBFREE_NOFORCE, SCTP_FROM_SCTP_PCB + SCTP_LOC_4) == 0) {
					cnt_in_sd++;
				}
				continue;
			} else if (TAILQ_EMPTY(&asoc->asoc.send_queue) &&
			           TAILQ_EMPTY(&asoc->asoc.sent_queue) &&
			           (asoc->asoc.stream_queue_cnt == 0)) {
				if ((*asoc->asoc.ss_functions.sctp_ss_is_user_msgs_incomplete)(asoc, &asoc->asoc)) {
					goto abort_anyway;
				}
				if ((SCTP_GET_STATE(asoc) != SCTP_STATE_SHUTDOWN_SENT) &&
				    (SCTP_GET_STATE(asoc) != SCTP_STATE_SHUTDOWN_ACK_SENT)) {
					struct sctp_nets *netp;

					if ((SCTP_GET_STATE(asoc) == SCTP_STATE_OPEN) ||
					    (SCTP_GET_STATE(asoc) == SCTP_STATE_SHUTDOWN_RECEIVED)) {
						SCTP_STAT_DECR_GAUGE32(sctps_currestab);
					}
					SCTP_SET_STATE(asoc, SCTP_STATE_SHUTDOWN_SENT);
					sctp_stop_timers_for_shutdown(asoc);
					if (asoc->asoc.alternate) {
						netp = asoc->asoc.alternate;
					} else {
						netp = asoc->asoc.primary_destination;
					}
					sctp_send_shutdown(asoc, netp);
					sctp_timer_start(SCTP_TIMER_TYPE_SHUTDOWN, asoc->sctp_ep, asoc,
					    netp);
					sctp_timer_start(SCTP_TIMER_TYPE_SHUTDOWNGUARD, asoc->sctp_ep, asoc, NULL);
					sctp_chunk_output(inp, asoc, SCTP_OUTPUT_FROM_SHUT_TMR, SCTP_SO_LOCKED);
				}
			} else {

				SCTP_ADD_SUBSTATE(asoc, SCTP_STATE_SHUTDOWN_PENDING);
				sctp_timer_start(SCTP_TIMER_TYPE_SHUTDOWNGUARD, asoc->sctp_ep, asoc, NULL);
				if ((*asoc->asoc.ss_functions.sctp_ss_is_user_msgs_incomplete)(asoc, &asoc->asoc)) {
					SCTP_ADD_SUBSTATE(asoc, SCTP_STATE_PARTIAL_MSG_LEFT);
				}
				if (TAILQ_EMPTY(&asoc->asoc.send_queue) &&
				    TAILQ_EMPTY(&asoc->asoc.sent_queue) &&
				    (asoc->asoc.state & SCTP_STATE_PARTIAL_MSG_LEFT)) {
					struct mbuf *op_err;
				abort_anyway:
					op_err = sctp_generate_cause(SCTP_CAUSE_USER_INITIATED_ABT, "");
					asoc->sctp_ep->last_abort_code = SCTP_FROM_SCTP_PCB + SCTP_LOC_5;
					sctp_send_abort_tcb(asoc, op_err, SCTP_SO_LOCKED);
					SCTP_STAT_INCR_COUNTER32(sctps_aborted);
					if ((SCTP_GET_STATE(asoc) == SCTP_STATE_OPEN) ||
					    (SCTP_GET_STATE(asoc) == SCTP_STATE_SHUTDOWN_RECEIVED)) {
						SCTP_STAT_DECR_GAUGE32(sctps_currestab);
					}
					if (sctp_free_assoc(inp, asoc,
							    SCTP_PCBFREE_NOFORCE,
							    SCTP_FROM_SCTP_PCB + SCTP_LOC_6) == 0) {
						cnt_in_sd++;
					}
					continue;
				} else {
					sctp_chunk_output(inp, asoc, SCTP_OUTPUT_FROM_CLOSING, SCTP_SO_LOCKED);
				}
			}
			cnt_in_sd++;
			SCTP_TCB_UNLOCK(asoc);
		}

		if (cnt_in_sd) {
#ifdef SCTP_LOG_CLOSING
			sctp_log_closing(inp, NULL, 2);
#endif
			inp->sctp_socket = NULL;
			SCTP_INP_WUNLOCK(inp);
			SCTP_ASOC_CREATE_UNLOCK(inp);
			SCTP_INP_INFO_WUNLOCK();
			return;
		}
	}
	inp->sctp_socket = NULL;
	if ((inp->sctp_flags & SCTP_PCB_FLAGS_UNBOUND) !=
	    SCTP_PCB_FLAGS_UNBOUND) {

		LIST_REMOVE(inp, sctp_hash);
		inp->sctp_flags |= SCTP_PCB_FLAGS_UNBOUND;
	}

	cnt = 0;
	LIST_FOREACH_SAFE(asoc, &inp->sctp_asoc_list, sctp_tcblist, nasoc) {
		SCTP_TCB_LOCK(asoc);
		if (immediate != SCTP_FREE_SHOULD_USE_GRACEFUL_CLOSE) {

			asoc->sctp_socket = NULL;
			SCTP_ADD_SUBSTATE(asoc, SCTP_STATE_CLOSED_SOCKET);
		}
		if (asoc->asoc.state & SCTP_STATE_ABOUT_TO_BE_FREED) {
			if (asoc->asoc.state & SCTP_STATE_IN_ACCEPT_QUEUE) {
				SCTP_CLEAR_SUBSTATE(asoc, SCTP_STATE_IN_ACCEPT_QUEUE);
				sctp_timer_start(SCTP_TIMER_TYPE_ASOCKILL, inp, asoc, NULL);
			}
			cnt++;
			SCTP_TCB_UNLOCK(asoc);
			continue;
		}

		if ((SCTP_GET_STATE(asoc) != SCTP_STATE_COOKIE_WAIT) &&
		    ((asoc->asoc.state & SCTP_STATE_ABOUT_TO_BE_FREED) == 0)) {
			struct mbuf *op_err;

			op_err = sctp_generate_cause(SCTP_CAUSE_USER_INITIATED_ABT, "");
			asoc->sctp_ep->last_abort_code = SCTP_FROM_SCTP_PCB + SCTP_LOC_7;
			sctp_send_abort_tcb(asoc, op_err, SCTP_SO_LOCKED);
			SCTP_STAT_INCR_COUNTER32(sctps_aborted);
		} else if (asoc->asoc.state & SCTP_STATE_ABOUT_TO_BE_FREED) {
			cnt++;
			SCTP_TCB_UNLOCK(asoc);
			continue;
		}
		if ((SCTP_GET_STATE(asoc) == SCTP_STATE_OPEN) ||
		    (SCTP_GET_STATE(asoc) == SCTP_STATE_SHUTDOWN_RECEIVED)) {
			SCTP_STAT_DECR_GAUGE32(sctps_currestab);
		}
		if (sctp_free_assoc(inp, asoc, SCTP_PCBFREE_FORCE,
		                    SCTP_FROM_SCTP_PCB + SCTP_LOC_8) == 0) {
			cnt++;
		}
	}
	if (cnt) {

#ifdef SCTP_LOG_CLOSING
		sctp_log_closing(inp, NULL, 3);
#endif
		SCTP_INP_WUNLOCK(inp);
		SCTP_ASOC_CREATE_UNLOCK(inp);
		SCTP_INP_INFO_WUNLOCK();
		return;
	}
	if (SCTP_INP_LOCK_CONTENDED(inp))
		being_refed++;
	if (SCTP_INP_READ_CONTENDED(inp))
		being_refed++;
	if (SCTP_ASOC_CREATE_LOCK_CONTENDED(inp))
		being_refed++;

	if ((inp->refcount) ||
	    (being_refed) ||
	    (inp->sctp_flags & SCTP_PCB_FLAGS_CLOSE_IP)) {
#ifdef SCTP_LOG_CLOSING
		sctp_log_closing(inp, NULL, 4);
#endif
		sctp_timer_start(SCTP_TIMER_TYPE_INPKILL, inp, NULL, NULL);
		SCTP_INP_WUNLOCK(inp);
		SCTP_ASOC_CREATE_UNLOCK(inp);
		SCTP_INP_INFO_WUNLOCK();
		return;
	}
	inp->sctp_ep.signature_change.type = 0;
	inp->sctp_flags |= SCTP_PCB_FLAGS_SOCKET_ALLGONE;

	LIST_REMOVE(inp, sctp_list);
	SCTP_INP_WUNLOCK(inp);
	SCTP_ASOC_CREATE_UNLOCK(inp);
	SCTP_INP_INFO_WUNLOCK();

#ifdef SCTP_LOG_CLOSING
	sctp_log_closing(inp, NULL, 5);
#endif
#if !(defined(_WIN32) || defined(__Userspace__))
#if !(defined(__FreeBSD__)  && !defined(__Userspace__))
	rt = ip_pcb->inp_route.ro_rt;
#endif
#endif
	if ((inp->sctp_asocidhash) != NULL) {
		SCTP_HASH_FREE(inp->sctp_asocidhash, inp->hashasocidmark);
		inp->sctp_asocidhash = NULL;
	}

	TAILQ_FOREACH_SAFE(sq, &inp->read_queue, next, nsq) {

		if (sq->length)
			SCTP_STAT_INCR(sctps_left_abandon);

		TAILQ_REMOVE(&inp->read_queue, sq, next);
		sctp_free_remote_addr(sq->whoFrom);
		if (so)
			so->so_rcv.sb_cc -= sq->length;
		if (sq->data) {
			sctp_m_freem(sq->data);
			sq->data = NULL;
		}

		sctp_free_a_readq(NULL, sq);
	}

	if (ip_pcb->inp_options) {
		(void)sctp_m_free(ip_pcb->inp_options);
		ip_pcb->inp_options = 0;
	}
#if !(defined(_WIN32) || defined(__Userspace__))
#if !defined(__FreeBSD__)
	if (rt) {
		RTFREE(rt);
		ip_pcb->inp_route.ro_rt = 0;
	}
#endif
#endif
#ifdef INET6
#if !(defined(_WIN32) || defined(__Userspace__))
#if (defined(__FreeBSD__) || defined(__APPLE__) && !defined(__Userspace__))
	if (ip_pcb->inp_vflag & INP_IPV6) {
#else
	if (inp->inp_vflag & INP_IPV6) {
#endif
		ip6_freepcbopts(ip_pcb->in6p_outputopts);
	}
#endif
#endif
	ip_pcb->inp_vflag = 0;

	if (inp->sctp_ep.local_auth_chunks != NULL)
		sctp_free_chunklist(inp->sctp_ep.local_auth_chunks);
	if (inp->sctp_ep.local_hmacs != NULL)
		sctp_free_hmaclist(inp->sctp_ep.local_hmacs);

	LIST_FOREACH_SAFE(shared_key, &inp->sctp_ep.shared_keys, next, nshared_key) {
		LIST_REMOVE(shared_key, next);
		sctp_free_sharedkey(shared_key);

	}

#if defined(__APPLE__) && !defined(__Userspace__)
	inp->ip_inp.inp.inp_state = INPCB_STATE_DEAD;
	if (in_pcb_checkstate(&inp->ip_inp.inp, WNT_STOPUSING, 1) != WNT_STOPUSING) {
#ifdef INVARIANTS
		panic("sctp_inpcb_free inp = %p couldn't set to STOPUSING\n", (void *)inp);
#else
		SCTP_PRINTF("sctp_inpcb_free inp = %p couldn't set to STOPUSING\n", (void *)inp);
#endif
	}
	inp->ip_inp.inp.inp_socket->so_flags |= SOF_PCBCLEARING;
#endif

	LIST_FOREACH_SAFE(laddr, &inp->sctp_addr_list, sctp_nxt_addr, nladdr) {
		sctp_remove_laddr(laddr);
	}

#ifdef SCTP_TRACK_FREED_ASOCS

	LIST_FOREACH_SAFE(asoc, &inp->sctp_asoc_free_list, sctp_tcblist, nasoc) {
		LIST_REMOVE(asoc, sctp_tcblist);
		SCTP_ZONE_FREE(SCTP_BASE_INFO(ipi_zone_asoc), asoc);
		SCTP_DECR_ASOC_COUNT();
	}

#endif
#ifdef SCTP_MVRF
	SCTP_FREE(inp->m_vrf_ids, SCTP_M_MVRF);
#endif

	if (inp->sctp_tcbhash != NULL) {
		SCTP_HASH_FREE(inp->sctp_tcbhash, inp->sctp_hashmark);
		inp->sctp_tcbhash = NULL;
	}

#if defined(__FreeBSD__) && !defined(__Userspace__)
	crfree(inp->ip_inp.inp.inp_cred);
	INP_LOCK_DESTROY(&inp->ip_inp.inp);
#endif
	SCTP_INP_LOCK_DESTROY(inp);
	SCTP_INP_READ_DESTROY(inp);
	SCTP_ASOC_CREATE_LOCK_DESTROY(inp);
#if !(defined(__APPLE__) && !defined(__Userspace__))
	SCTP_ZONE_FREE(SCTP_BASE_INFO(ipi_zone_ep), inp);
	SCTP_DECR_EP_COUNT();
#else

#endif
}

struct sctp_nets *
sctp_findnet(struct sctp_tcb *stcb, struct sockaddr *addr)
{
	struct sctp_nets *net;

	TAILQ_FOREACH(net, &stcb->asoc.nets, sctp_next) {
		if (sctp_cmpaddr(addr, (struct sockaddr *)&net->ro._l_addr))
			return (net);
	}
	return (NULL);
}

int
sctp_is_address_on_local_host(struct sockaddr *addr, uint32_t vrf_id)
{
	struct sctp_ifa *sctp_ifa;
	sctp_ifa = sctp_find_ifa_by_addr(addr, vrf_id, SCTP_ADDR_NOT_LOCKED);
	if (sctp_ifa) {
		return (1);
	} else {
		return (0);
	}
}

int
sctp_add_remote_addr(struct sctp_tcb *stcb, struct sockaddr *newaddr,
    struct sctp_nets **netp, uint16_t port, int set_scope, int from)
{

	struct sctp_nets *net, *netfirst;
	int addr_inscope;

	SCTPDBG(SCTP_DEBUG_PCB1, "Adding an address (from:%d) to the peer: ",
		from);
	SCTPDBG_ADDR(SCTP_DEBUG_PCB1, newaddr);

	netfirst = sctp_findnet(stcb, newaddr);
	if (netfirst) {

		if (netfirst->dest_state & SCTP_ADDR_UNCONFIRMED) {
			netfirst->dest_state = (SCTP_ADDR_REACHABLE |
			    SCTP_ADDR_UNCONFIRMED);
		} else {
			netfirst->dest_state = SCTP_ADDR_REACHABLE;
		}

		return (0);
	}
	addr_inscope = 1;
	switch (newaddr->sa_family) {
#ifdef INET
	case AF_INET:
	{
		struct sockaddr_in *sin;

		sin = (struct sockaddr_in *)newaddr;
		if (sin->sin_addr.s_addr == 0) {

			return (-1);
		}

		memset(&sin->sin_zero, 0, sizeof(sin->sin_zero));

#ifdef HAVE_SIN_LEN
		sin->sin_len = sizeof(struct sockaddr_in);
#endif
		if (set_scope) {
			if (IN4_ISPRIVATE_ADDRESS(&sin->sin_addr)) {
				stcb->asoc.scope.ipv4_local_scope = 1;
			}
		} else {

			if ((IN4_ISPRIVATE_ADDRESS(&sin->sin_addr)) &&
			    (stcb->asoc.scope.ipv4_local_scope == 0)) {
				addr_inscope = 0;
			}
		}
		break;
	}
#endif
#ifdef INET6
	case AF_INET6:
	{
		struct sockaddr_in6 *sin6;

		sin6 = (struct sockaddr_in6 *)newaddr;
		if (IN6_IS_ADDR_UNSPECIFIED(&sin6->sin6_addr)) {

			return (-1);
		}

#ifdef HAVE_SIN6_LEN
		sin6->sin6_len = sizeof(struct sockaddr_in6);
#endif
		if (set_scope) {
			if (sctp_is_address_on_local_host(newaddr, stcb->asoc.vrf_id)) {
				stcb->asoc.scope.loopback_scope = 1;
				stcb->asoc.scope.local_scope = 0;
				stcb->asoc.scope.ipv4_local_scope = 1;
				stcb->asoc.scope.site_scope = 1;
			} else if (IN6_IS_ADDR_LINKLOCAL(&sin6->sin6_addr)) {

				stcb->asoc.scope.ipv4_local_scope = 1;
				stcb->asoc.scope.site_scope = 1;
			} else if (IN6_IS_ADDR_SITELOCAL(&sin6->sin6_addr)) {

				stcb->asoc.scope.site_scope = 1;
			}
		} else {

			if (IN6_IS_ADDR_LOOPBACK(&sin6->sin6_addr) &&
			    (stcb->asoc.scope.loopback_scope == 0)) {
				addr_inscope = 0;
			} else if (IN6_IS_ADDR_LINKLOCAL(&sin6->sin6_addr) &&
			    (stcb->asoc.scope.local_scope == 0)) {
				addr_inscope = 0;
			} else if (IN6_IS_ADDR_SITELOCAL(&sin6->sin6_addr) &&
			    (stcb->asoc.scope.site_scope == 0)) {
				addr_inscope = 0;
			}
		}
		break;
	}
#endif
#if defined(__Userspace__)
	case AF_CONN:
	{
		struct sockaddr_conn *sconn;

		sconn = (struct sockaddr_conn *)newaddr;
		if (sconn->sconn_addr == NULL) {

			return (-1);
		}
#ifdef HAVE_SCONN_LEN
		sconn->sconn_len = sizeof(struct sockaddr_conn);
#endif
		break;
	}
#endif
	default:

		return (-1);
	}
	net = SCTP_ZONE_GET(SCTP_BASE_INFO(ipi_zone_net), struct sctp_nets);
	if (net == NULL) {
		return (-1);
	}
	SCTP_INCR_RADDR_COUNT();
	memset(net, 0, sizeof(struct sctp_nets));
	(void)SCTP_GETTIME_TIMEVAL(&net->start_time);
#ifdef HAVE_SA_LEN
	memcpy(&net->ro._l_addr, newaddr, newaddr->sa_len);
#endif
	switch (newaddr->sa_family) {
#ifdef INET
	case AF_INET:
#ifndef HAVE_SA_LEN
		memcpy(&net->ro._l_addr, newaddr, sizeof(struct sockaddr_in));
#endif
		((struct sockaddr_in *)&net->ro._l_addr)->sin_port = stcb->rport;
		break;
#endif
#ifdef INET6
	case AF_INET6:
#ifndef HAVE_SA_LEN
		memcpy(&net->ro._l_addr, newaddr, sizeof(struct sockaddr_in6));
#endif
		((struct sockaddr_in6 *)&net->ro._l_addr)->sin6_port = stcb->rport;
		break;
#endif
#if defined(__Userspace__)
	case AF_CONN:
#ifndef HAVE_SA_LEN
		memcpy(&net->ro._l_addr, newaddr, sizeof(struct sockaddr_conn));
#endif
		((struct sockaddr_conn *)&net->ro._l_addr)->sconn_port = stcb->rport;
		break;
#endif
	default:
		break;
	}
	net->addr_is_local = sctp_is_address_on_local_host(newaddr, stcb->asoc.vrf_id);
	if (net->addr_is_local && ((set_scope || (from == SCTP_ADDR_IS_CONFIRMED)))) {
		stcb->asoc.scope.loopback_scope = 1;
		stcb->asoc.scope.ipv4_local_scope = 1;
		stcb->asoc.scope.local_scope = 0;
		stcb->asoc.scope.site_scope = 1;
		addr_inscope = 1;
	}
	net->failure_threshold = stcb->asoc.def_net_failure;
	net->pf_threshold = stcb->asoc.def_net_pf_threshold;
	if (addr_inscope == 0) {
		net->dest_state = (SCTP_ADDR_REACHABLE |
		    SCTP_ADDR_OUT_OF_SCOPE);
	} else {
		if (from == SCTP_ADDR_IS_CONFIRMED)

			net->dest_state = SCTP_ADDR_REACHABLE;
		else
			net->dest_state = SCTP_ADDR_REACHABLE |
			    SCTP_ADDR_UNCONFIRMED;
	}

	net->rto_needed = 1;
	net->RTO = 0;
	net->RTO_measured = 0;
	stcb->asoc.numnets++;
	net->ref_count = 1;
	net->cwr_window_tsn = net->last_cwr_tsn = stcb->asoc.sending_seq - 1;
	net->port = port;
	net->dscp = stcb->asoc.default_dscp;
#ifdef INET6
	net->flowlabel = stcb->asoc.default_flowlabel;
#endif
	if (sctp_stcb_is_feature_on(stcb->sctp_ep, stcb, SCTP_PCB_FLAGS_DONOT_HEARTBEAT)) {
		net->dest_state |= SCTP_ADDR_NOHB;
	} else {
		net->dest_state &= ~SCTP_ADDR_NOHB;
	}
	if (sctp_stcb_is_feature_on(stcb->sctp_ep, stcb, SCTP_PCB_FLAGS_DO_NOT_PMTUD)) {
		net->dest_state |= SCTP_ADDR_NO_PMTUD;
	} else {
		net->dest_state &= ~SCTP_ADDR_NO_PMTUD;
	}
	net->heart_beat_delay = stcb->asoc.heart_beat_delay;

	SCTP_OS_TIMER_INIT(&net->rxt_timer.timer);
	SCTP_OS_TIMER_INIT(&net->pmtu_timer.timer);
	SCTP_OS_TIMER_INIT(&net->hb_timer.timer);

#ifdef INET6
#ifdef SCTP_EMBEDDED_V6_SCOPE

	if (newaddr->sa_family == AF_INET6) {
		struct sockaddr_in6 *sin6;

		sin6 = (struct sockaddr_in6 *)&net->ro._l_addr;
#if defined(__APPLE__) && !defined(__Userspace__)
#if defined(APPLE_LEOPARD) || defined(APPLE_SNOWLEOPARD)
		(void)in6_embedscope(&sin6->sin6_addr, sin6, &stcb->sctp_ep->ip_inp.inp, NULL);
#else
		(void)in6_embedscope(&sin6->sin6_addr, sin6, &stcb->sctp_ep->ip_inp.inp, NULL, NULL);
#endif
#elif defined(SCTP_KAME)
		(void)sa6_embedscope(sin6, MODULE_GLOBAL(ip6_use_defzone));
#else
		(void)in6_embedscope(&sin6->sin6_addr, sin6);
#endif
#ifndef SCOPEDROUTING
		sin6->sin6_scope_id = 0;
#endif
	}
#endif
#endif
	SCTP_RTALLOC((sctp_route_t *)&net->ro,
	             stcb->asoc.vrf_id,
	             stcb->sctp_ep->fibnum);

	net->src_addr_selected = 0;
#if !defined(__Userspace__)
	if (SCTP_ROUTE_HAS_VALID_IFN(&net->ro)) {

		net->ro._s_addr = sctp_source_address_selection(stcb->sctp_ep,
		                                                stcb,
		                                                (sctp_route_t *)&net->ro,
		                                                net,
		                                                0,
		                                                stcb->asoc.vrf_id);
		if (stcb->asoc.default_mtu > 0) {
			net->mtu = stcb->asoc.default_mtu;
			switch (net->ro._l_addr.sa.sa_family) {
#ifdef INET
			case AF_INET:
				net->mtu += SCTP_MIN_V4_OVERHEAD;
				break;
#endif
#ifdef INET6
			case AF_INET6:
				net->mtu += SCTP_MIN_OVERHEAD;
				break;
#endif
#if defined(__Userspace__)
			case AF_CONN:
				net->mtu += sizeof(struct sctphdr);
				break;
#endif
			default:
				break;
			}
#if defined(INET) || defined(INET6)
			if (net->port) {
				net->mtu += (uint32_t)sizeof(struct udphdr);
			}
#endif
		} else if (net->ro._s_addr != NULL) {
			uint32_t imtu, rmtu, hcmtu;

			net->src_addr_selected = 1;

			if (net->ro._s_addr->ifn_p != NULL) {
				imtu = SCTP_GATHER_MTU_FROM_INTFC(net->ro._s_addr->ifn_p);
			} else {
				imtu = 0;
			}
#if defined(__FreeBSD__) && !defined(__Userspace__)
			rmtu = SCTP_GATHER_MTU_FROM_ROUTE(net->ro._s_addr, &net->ro._l_addr.sa, net->ro.ro_nh);
			hcmtu = sctp_hc_get_mtu(&net->ro._l_addr, stcb->sctp_ep->fibnum);
#else
			rmtu = SCTP_GATHER_MTU_FROM_ROUTE(net->ro._s_addr, &net->ro._l_addr.sa, net->ro.ro_rt);
			hcmtu = 0;
#endif
			net->mtu = sctp_min_mtu(hcmtu, rmtu, imtu);
#if defined(__FreeBSD__) && !defined(__Userspace__)
#else
			if (rmtu == 0) {

				SCTP_SET_MTU_OF_ROUTE(&net->ro._l_addr.sa,
				                      net->ro.ro_rt, net->mtu);
			}
#endif
		}
	}
#endif
	if (net->mtu == 0) {
		if (stcb->asoc.default_mtu > 0) {
			net->mtu = stcb->asoc.default_mtu;
			switch (net->ro._l_addr.sa.sa_family) {
#ifdef INET
			case AF_INET:
				net->mtu += SCTP_MIN_V4_OVERHEAD;
				break;
#endif
#ifdef INET6
			case AF_INET6:
				net->mtu += SCTP_MIN_OVERHEAD;
				break;
#endif
#if defined(__Userspace__)
			case AF_CONN:
				net->mtu += sizeof(struct sctphdr);
				break;
#endif
			default:
				break;
			}
#if defined(INET) || defined(INET6)
			if (net->port) {
				net->mtu += (uint32_t)sizeof(struct udphdr);
			}
#endif
		} else {
			switch (newaddr->sa_family) {
#ifdef INET
			case AF_INET:
				net->mtu = SCTP_DEFAULT_MTU;
				break;
#endif
#ifdef INET6
			case AF_INET6:
				net->mtu = 1280;
				break;
#endif
#if defined(__Userspace__)
			case AF_CONN:
				net->mtu = 1280;
				break;
#endif
			default:
				break;
			}
		}
	}
#if defined(INET) || defined(INET6)
	if (net->port) {
		net->mtu -= (uint32_t)sizeof(struct udphdr);
	}
#endif
	if (from == SCTP_ALLOC_ASOC) {
		stcb->asoc.smallest_mtu = net->mtu;
	}
	if (stcb->asoc.smallest_mtu > net->mtu) {
		sctp_pathmtu_adjustment(stcb, net->mtu);
	}
#ifdef INET6
#ifdef SCTP_EMBEDDED_V6_SCOPE
	if (newaddr->sa_family == AF_INET6) {
		struct sockaddr_in6 *sin6;

		sin6 = (struct sockaddr_in6 *)&net->ro._l_addr;
#ifdef SCTP_KAME
		(void)sa6_recoverscope(sin6);
#else
		(void)in6_recoverscope(sin6, &sin6->sin6_addr, NULL);
#endif
	}
#endif
#endif

	if (stcb->asoc.cc_functions.sctp_set_initial_cc_param != NULL)
		(*stcb->asoc.cc_functions.sctp_set_initial_cc_param)(stcb, net);

	net->find_pseudo_cumack = 1;
	net->find_rtx_pseudo_cumack = 1;
#if defined(__FreeBSD__) && !defined(__Userspace__)

	net->flowid = stcb->asoc.my_vtag ^
	              ntohs(stcb->rport) ^
	              ntohs(stcb->sctp_ep->sctp_lport);
	net->flowtype = M_HASHTYPE_OPAQUE_HASH;
#endif
	if (netp) {
		*netp = net;
	}
	netfirst = TAILQ_FIRST(&stcb->asoc.nets);
#if defined(__FreeBSD__) && !defined(__Userspace__)
	if (net->ro.ro_nh == NULL) {
#else
	if (net->ro.ro_rt == NULL) {
#endif

		TAILQ_INSERT_TAIL(&stcb->asoc.nets, net, sctp_next);
	} else if (netfirst == NULL) {

		TAILQ_INSERT_HEAD(&stcb->asoc.nets, net, sctp_next);
#if defined(__FreeBSD__) && !defined(__Userspace__)
	} else if (netfirst->ro.ro_nh == NULL) {
#else
	} else if (netfirst->ro.ro_rt == NULL) {
#endif

		TAILQ_INSERT_HEAD(&stcb->asoc.nets, net, sctp_next);
#if defined(__FreeBSD__) && !defined(__Userspace__)
	} else if (net->ro.ro_nh->nh_ifp != netfirst->ro.ro_nh->nh_ifp) {
#else
	} else if (net->ro.ro_rt->rt_ifp != netfirst->ro.ro_rt->rt_ifp) {
#endif

		TAILQ_INSERT_HEAD(&stcb->asoc.nets, net, sctp_next);
	} else {

		struct sctp_nets *netlook;

		do {
			netlook = TAILQ_NEXT(netfirst, sctp_next);
			if (netlook == NULL) {

				TAILQ_INSERT_TAIL(&stcb->asoc.nets, net, sctp_next);
				break;
#if defined(__FreeBSD__) && !defined(__Userspace__)
			} else if (netlook->ro.ro_nh == NULL) {
#else
			} else if (netlook->ro.ro_rt == NULL) {
#endif

				TAILQ_INSERT_BEFORE(netfirst, net, sctp_next);
				break;
#if defined(__FreeBSD__) && !defined(__Userspace__)
			} else if (netlook->ro.ro_nh->nh_ifp != net->ro.ro_nh->nh_ifp) {
#else
			} else if (netlook->ro.ro_rt->rt_ifp != net->ro.ro_rt->rt_ifp) {
#endif
				TAILQ_INSERT_AFTER(&stcb->asoc.nets, netlook,
				                   net, sctp_next);
				break;
			}

			netfirst = netlook;
		} while (netlook != NULL);
	}

	if (stcb->asoc.primary_destination == 0) {
		stcb->asoc.primary_destination = net;
#if defined(__FreeBSD__) && !defined(__Userspace__)
	} else if ((stcb->asoc.primary_destination->ro.ro_nh == NULL) &&
	           (net->ro.ro_nh) &&
#else
	} else if ((stcb->asoc.primary_destination->ro.ro_rt == NULL) &&
	           (net->ro.ro_rt) &&
#endif
	           ((net->dest_state & SCTP_ADDR_UNCONFIRMED) == 0)) {

		stcb->asoc.primary_destination = net;
	}

	net = TAILQ_FIRST(&stcb->asoc.nets);
	if ((net != stcb->asoc.primary_destination) &&
	    (stcb->asoc.primary_destination)) {

		TAILQ_REMOVE(&stcb->asoc.nets,
			     stcb->asoc.primary_destination, sctp_next);
		TAILQ_INSERT_HEAD(&stcb->asoc.nets,
				  stcb->asoc.primary_destination, sctp_next);
	}
	return (0);
}

static uint32_t
sctp_aloc_a_assoc_id(struct sctp_inpcb *inp, struct sctp_tcb *stcb)
{
	uint32_t id;
	struct sctpasochead *head;
	struct sctp_tcb *lstcb;

 try_again:
	if (inp->sctp_flags & SCTP_PCB_FLAGS_SOCKET_ALLGONE) {

		return (0);
	}

	if (inp->sctp_associd_counter <= SCTP_ALL_ASSOC) {
		inp->sctp_associd_counter = SCTP_ALL_ASSOC + 1;
	}
	id = inp->sctp_associd_counter;
	inp->sctp_associd_counter++;
	lstcb = sctp_findasoc_ep_asocid_locked(inp, (sctp_assoc_t)id, 0);
	if (lstcb) {
		goto try_again;
	}
	head = &inp->sctp_asocidhash[SCTP_PCBHASH_ASOC(id, inp->hashasocidmark)];
	LIST_INSERT_HEAD(head, stcb, sctp_tcbasocidhash);
	stcb->asoc.in_asocid_hash = 1;
	return (id);
}

static struct sctp_tcb *
sctp_aloc_assoc_locked(struct sctp_inpcb *inp, struct sockaddr *firstaddr,
                       int *error, uint32_t override_tag, uint32_t initial_tsn,
                       uint32_t vrf_id, uint16_t o_streams, uint16_t port,
#if defined(__FreeBSD__) && !defined(__Userspace__)
                       struct thread *p,
#elif defined(_WIN32) && !defined(__Userspace__)
                       PKTHREAD p,
#else
#if defined(__Userspace__)

#endif
                       struct proc *p,
#endif
                       int initialize_auth_params)
{

	struct sctp_tcb *stcb;
	struct sctp_association *asoc;
	struct sctpasochead *head;
	uint16_t rport;
	int err;

	SCTP_INP_INFO_WLOCK_ASSERT();
	SCTP_INP_WLOCK_ASSERT(inp);

	if (SCTP_BASE_INFO(ipi_count_asoc) >= SCTP_MAX_NUM_OF_ASOC) {

		SCTP_LTRACE_ERR_RET(inp, NULL, NULL, SCTP_FROM_SCTP_PCB, ENOBUFS);
		*error = ENOBUFS;
		return (NULL);
	}
	if (firstaddr == NULL) {
		SCTP_LTRACE_ERR_RET(inp, NULL, NULL, SCTP_FROM_SCTP_PCB, EINVAL);
		*error = EINVAL;
		return (NULL);
	}
	if (inp->sctp_flags & (SCTP_PCB_FLAGS_SOCKET_GONE | SCTP_PCB_FLAGS_SOCKET_ALLGONE)) {
		SCTP_LTRACE_ERR_RET(inp, NULL, NULL, SCTP_FROM_SCTP_PCB, EINVAL);
		*error = EINVAL;
		return (NULL);
	}
	if ((inp->sctp_flags & SCTP_PCB_FLAGS_IN_TCPPOOL) &&
	    ((sctp_is_feature_off(inp, SCTP_PCB_FLAGS_PORTREUSE)) ||
	     (inp->sctp_flags & SCTP_PCB_FLAGS_CONNECTED))) {

		SCTP_LTRACE_ERR_RET(inp, NULL, NULL, SCTP_FROM_SCTP_PCB, EINVAL);
		*error = EINVAL;
		return (NULL);
	}
	if ((inp->sctp_flags & SCTP_PCB_FLAGS_IN_TCPPOOL) ||
	    (inp->sctp_flags & SCTP_PCB_FLAGS_TCPTYPE)) {
		if ((inp->sctp_flags & SCTP_PCB_FLAGS_WAS_CONNECTED) ||
		    (inp->sctp_flags & SCTP_PCB_FLAGS_WAS_ABORTED)) {
			SCTP_LTRACE_ERR_RET(inp, NULL, NULL, SCTP_FROM_SCTP_PCB, EINVAL);
			*error = EINVAL;
			return (NULL);
		}
	}
	SCTPDBG(SCTP_DEBUG_PCB3, "Allocate an association for peer:");
#ifdef SCTP_DEBUG
	if (firstaddr) {
		SCTPDBG_ADDR(SCTP_DEBUG_PCB3, firstaddr);
		switch (firstaddr->sa_family) {
#ifdef INET
		case AF_INET:
			SCTPDBG(SCTP_DEBUG_PCB3, "Port:%d\n",
			        ntohs(((struct sockaddr_in *)firstaddr)->sin_port));
			break;
#endif
#ifdef INET6
		case AF_INET6:
			SCTPDBG(SCTP_DEBUG_PCB3, "Port:%d\n",
			        ntohs(((struct sockaddr_in6 *)firstaddr)->sin6_port));
			break;
#endif
#if defined(__Userspace__)
		case AF_CONN:
			SCTPDBG(SCTP_DEBUG_PCB3, "Port:%d\n",
			        ntohs(((struct sockaddr_conn *)firstaddr)->sconn_port));
			break;
#endif
		default:
			break;
		}
	} else {
		SCTPDBG(SCTP_DEBUG_PCB3,"None\n");
	}
#endif
	switch (firstaddr->sa_family) {
#ifdef INET
	case AF_INET:
	{
		struct sockaddr_in *sin;

		sin = (struct sockaddr_in *)firstaddr;
		if ((ntohs(sin->sin_port) == 0) ||
		    (sin->sin_addr.s_addr == INADDR_ANY) ||
		    (sin->sin_addr.s_addr == INADDR_BROADCAST) ||
		    IN_MULTICAST(ntohl(sin->sin_addr.s_addr)) ||
#if defined(__Userspace__)
		    (((inp->sctp_flags & SCTP_PCB_FLAGS_BOUND_CONN) != 0) ||
		     (((inp->sctp_flags & SCTP_PCB_FLAGS_BOUND_V6) != 0) &&
		      (SCTP_IPV6_V6ONLY(inp) != 0)))) {
#else
		    (((inp->sctp_flags & SCTP_PCB_FLAGS_BOUND_V6) != 0) &&
		     (SCTP_IPV6_V6ONLY(inp) != 0))) {
#endif

			SCTP_LTRACE_ERR_RET(inp, NULL, NULL, SCTP_FROM_SCTP_PCB, EINVAL);
			*error = EINVAL;
			return (NULL);
		}
		rport = sin->sin_port;
		break;
	}
#endif
#ifdef INET6
	case AF_INET6:
	{
		struct sockaddr_in6 *sin6;

		sin6 = (struct sockaddr_in6 *)firstaddr;
		if ((ntohs(sin6->sin6_port) == 0) ||
		    IN6_IS_ADDR_UNSPECIFIED(&sin6->sin6_addr) ||
		    IN6_IS_ADDR_MULTICAST(&sin6->sin6_addr) ||
		    ((inp->sctp_flags & SCTP_PCB_FLAGS_BOUND_V6) == 0)) {

			SCTP_LTRACE_ERR_RET(inp, NULL, NULL, SCTP_FROM_SCTP_PCB, EINVAL);
			*error = EINVAL;
			return (NULL);
		}
		rport = sin6->sin6_port;
		break;
	}
#endif
#if defined(__Userspace__)
	case AF_CONN:
	{
		struct sockaddr_conn *sconn;

		sconn = (struct sockaddr_conn *)firstaddr;
		if ((ntohs(sconn->sconn_port) == 0) ||
		    (sconn->sconn_addr == NULL) ||
		    ((inp->sctp_flags & SCTP_PCB_FLAGS_BOUND_CONN) == 0)) {

			SCTP_LTRACE_ERR_RET(inp, NULL, NULL, SCTP_FROM_SCTP_PCB, EINVAL);
			*error = EINVAL;
			return (NULL);
		}
		rport = sconn->sconn_port;
		break;
	}
#endif
	default:

		SCTP_LTRACE_ERR_RET(inp, NULL, NULL, SCTP_FROM_SCTP_PCB, EINVAL);
		*error = EINVAL;
		return (NULL);
	}
	if (inp->sctp_flags & SCTP_PCB_FLAGS_UNBOUND) {

		if ((err = sctp_inpcb_bind_locked(inp, NULL, NULL, p))) {

			*error = err;
			return (NULL);
		}
	}
	stcb = SCTP_ZONE_GET(SCTP_BASE_INFO(ipi_zone_asoc), struct sctp_tcb);
	if (stcb == NULL) {

		SCTP_LTRACE_ERR_RET(inp, NULL, NULL, SCTP_FROM_SCTP_PCB, ENOMEM);
		*error = ENOMEM;
		return (NULL);
	}
	SCTP_INCR_ASOC_COUNT();

	memset(stcb, 0, sizeof(*stcb));
	asoc = &stcb->asoc;

	SCTP_TCB_LOCK_INIT(stcb);
	SCTP_TCB_SEND_LOCK_INIT(stcb);
	stcb->rport = rport;

	stcb->sctp_ep = inp;
	stcb->sctp_socket = inp->sctp_socket;
	if ((err = sctp_init_asoc(inp, stcb, override_tag, initial_tsn, vrf_id, o_streams))) {

		SCTP_TCB_LOCK_DESTROY(stcb);
		SCTP_TCB_SEND_LOCK_DESTROY(stcb);
		SCTP_ZONE_FREE(SCTP_BASE_INFO(ipi_zone_asoc), stcb);
		SCTP_DECR_ASOC_COUNT();
		*error = err;
		return (NULL);
	}
	SCTP_TCB_LOCK(stcb);

	asoc->assoc_id = sctp_aloc_a_assoc_id(inp, stcb);

	head = &SCTP_BASE_INFO(sctp_asochash)[SCTP_PCBHASH_ASOC(stcb->asoc.my_vtag, SCTP_BASE_INFO(hashasocmark))];

	LIST_INSERT_HEAD(head, stcb, sctp_asocs);

	if (sctp_add_remote_addr(stcb, firstaddr, NULL, port, SCTP_DO_SETSCOPE, SCTP_ALLOC_ASOC)) {

		if (asoc->strmout) {
			SCTP_FREE(asoc->strmout, SCTP_M_STRMO);
			asoc->strmout = NULL;
		}
		if (asoc->mapping_array) {
			SCTP_FREE(asoc->mapping_array, SCTP_M_MAP);
			asoc->mapping_array = NULL;
		}
		if (asoc->nr_mapping_array) {
			SCTP_FREE(asoc->nr_mapping_array, SCTP_M_MAP);
			asoc->nr_mapping_array = NULL;
		}
		SCTP_DECR_ASOC_COUNT();
		SCTP_TCB_UNLOCK(stcb);
		SCTP_TCB_LOCK_DESTROY(stcb);
		SCTP_TCB_SEND_LOCK_DESTROY(stcb);
		LIST_REMOVE(stcb, sctp_asocs);
		LIST_REMOVE(stcb, sctp_tcbasocidhash);
		SCTP_ZONE_FREE(SCTP_BASE_INFO(ipi_zone_asoc), stcb);
		SCTP_INP_WUNLOCK(inp);
		SCTP_LTRACE_ERR_RET(inp, NULL, NULL, SCTP_FROM_SCTP_PCB, ENOBUFS);
		*error = ENOBUFS;
		return (NULL);
	}

	SCTP_OS_TIMER_INIT(&asoc->dack_timer.timer);
	SCTP_OS_TIMER_INIT(&asoc->strreset_timer.timer);
	SCTP_OS_TIMER_INIT(&asoc->asconf_timer.timer);
	SCTP_OS_TIMER_INIT(&asoc->shut_guard_timer.timer);
	SCTP_OS_TIMER_INIT(&asoc->autoclose_timer.timer);
	SCTP_OS_TIMER_INIT(&asoc->delete_prim_timer.timer);

	LIST_INSERT_HEAD(&inp->sctp_asoc_list, stcb, sctp_tcblist);

	if (inp->sctp_tcbhash != NULL) {
		head = &inp->sctp_tcbhash[SCTP_PCBHASH_ALLADDR(stcb->rport,
		    inp->sctp_hashmark)];
		LIST_INSERT_HEAD(head, stcb, sctp_tcbhash);
	}
	if (initialize_auth_params == SCTP_INITIALIZE_AUTH_PARAMS) {
		sctp_initialize_auth_params(inp, stcb);
	}
	SCTPDBG(SCTP_DEBUG_PCB1, "Association %p now allocated\n", (void *)stcb);
	return (stcb);
}

struct sctp_tcb *
sctp_aloc_assoc(struct sctp_inpcb *inp, struct sockaddr *firstaddr,
                int *error, uint32_t override_tag, uint32_t initial_tsn,
                uint32_t vrf_id, uint16_t o_streams, uint16_t port,
#if defined(__FreeBSD__) && !defined(__Userspace__)
                struct thread *p,
#elif defined(_WIN32) && !defined(__Userspace__)
                PKTHREAD p,
#else
                struct proc *p,
#endif
                int initialize_auth_params)
{
	struct sctp_tcb *stcb;

	SCTP_INP_INFO_WLOCK();
	SCTP_INP_WLOCK(inp);
	stcb = sctp_aloc_assoc_locked(inp, firstaddr, error, override_tag,
	    initial_tsn, vrf_id, o_streams, port, p, initialize_auth_params);
	SCTP_INP_INFO_WUNLOCK();
	SCTP_INP_WUNLOCK(inp);
	return (stcb);
}

struct sctp_tcb *
sctp_aloc_assoc_connected(struct sctp_inpcb *inp, struct sockaddr *firstaddr,
                          int *error, uint32_t override_tag, uint32_t initial_tsn,
                          uint32_t vrf_id, uint16_t o_streams, uint16_t port,
#if defined(__FreeBSD__) && !defined(__Userspace__)
                          struct thread *p,
#elif defined(_WIN32) && !defined(__Userspace__)
                          PKTHREAD p,
#else
                          struct proc *p,
#endif
                          int initialize_auth_params)
{
	struct sctp_tcb *stcb;

	SCTP_INP_INFO_WLOCK();
	SCTP_INP_WLOCK(inp);
	if ((inp->sctp_flags & SCTP_PCB_FLAGS_TCPTYPE) &&
	    SCTP_IS_LISTENING(inp)) {
		SCTP_INP_INFO_WUNLOCK();
		SCTP_INP_WUNLOCK(inp);
		*error = EINVAL;
		return (NULL);
	}
	stcb = sctp_aloc_assoc_locked(inp, firstaddr, error, override_tag,
	    initial_tsn, vrf_id, o_streams, port, p, initialize_auth_params);
	SCTP_INP_INFO_WUNLOCK();
	if (stcb != NULL && (inp->sctp_flags & SCTP_PCB_FLAGS_TCPTYPE)) {
		inp->sctp_flags |= SCTP_PCB_FLAGS_CONNECTED;
		soisconnecting(inp->sctp_socket);
	}
	SCTP_INP_WUNLOCK(inp);
	return (stcb);
}

void
sctp_remove_net(struct sctp_tcb *stcb, struct sctp_nets *net)
{
	struct sctp_inpcb *inp;
	struct sctp_association *asoc;

	inp = stcb->sctp_ep;
	asoc = &stcb->asoc;
	asoc->numnets--;
	TAILQ_REMOVE(&asoc->nets, net, sctp_next);
	if (net == asoc->primary_destination) {

		struct sctp_nets *lnet;

		lnet = TAILQ_FIRST(&asoc->nets);

		if (sctp_is_mobility_feature_on(stcb->sctp_ep,
		                                SCTP_MOBILITY_BASE) ||
		    sctp_is_mobility_feature_on(stcb->sctp_ep,
		                                SCTP_MOBILITY_FASTHANDOFF)) {
			SCTPDBG(SCTP_DEBUG_ASCONF1, "remove_net: primary dst is deleting\n");
			if (asoc->deleted_primary != NULL) {
				SCTPDBG(SCTP_DEBUG_ASCONF1, "remove_net: deleted primary may be already stored\n");
				goto out;
			}
			asoc->deleted_primary = net;
			atomic_add_int(&net->ref_count, 1);
			memset(&net->lastsa, 0, sizeof(net->lastsa));
			memset(&net->lastsv, 0, sizeof(net->lastsv));
			sctp_mobility_feature_on(stcb->sctp_ep,
						 SCTP_MOBILITY_PRIM_DELETED);
			sctp_timer_start(SCTP_TIMER_TYPE_PRIM_DELETED,
					 stcb->sctp_ep, stcb, NULL);
		}
out:

		asoc->primary_destination = sctp_find_alternate_net(stcb, lnet, 0);
	}
	if (net == asoc->last_data_chunk_from) {

		asoc->last_data_chunk_from = TAILQ_FIRST(&asoc->nets);
	}
	if (net == asoc->last_control_chunk_from) {

		asoc->last_control_chunk_from = NULL;
	}
	if (net == asoc->last_net_cmt_send_started) {

		asoc->last_net_cmt_send_started = NULL;
	}
	if (net == stcb->asoc.alternate) {
		sctp_free_remote_addr(stcb->asoc.alternate);
		stcb->asoc.alternate = NULL;
	}
	sctp_timer_stop(SCTP_TIMER_TYPE_PATHMTURAISE, inp, stcb, net,
	                SCTP_FROM_SCTP_PCB + SCTP_LOC_9);
	sctp_timer_stop(SCTP_TIMER_TYPE_HEARTBEAT, inp, stcb, net,
	                SCTP_FROM_SCTP_PCB + SCTP_LOC_10);
	net->dest_state |= SCTP_ADDR_BEING_DELETED;
	sctp_free_remote_addr(net);
}

int
sctp_del_remote_addr(struct sctp_tcb *stcb, struct sockaddr *remaddr)
{

	struct sctp_association *asoc;
	struct sctp_nets *net, *nnet;

	asoc = &stcb->asoc;

	TAILQ_FOREACH_SAFE(net, &asoc->nets, sctp_next, nnet) {
		if (net->ro._l_addr.sa.sa_family != remaddr->sa_family) {
			continue;
		}
		if (sctp_cmpaddr((struct sockaddr *)&net->ro._l_addr,
		    remaddr)) {

			if (asoc->numnets < 2) {

				return (-1);
			} else {
				sctp_remove_net(stcb, net);
				return (0);
			}
		}
	}

	return (-2);
}

static bool
sctp_is_in_timewait(uint32_t tag, uint16_t lport, uint16_t rport, uint32_t now)
{
	struct sctpvtaghead *chain;
	struct sctp_tagblock *twait_block;
	int i;

	SCTP_INP_INFO_LOCK_ASSERT();
	chain = &SCTP_BASE_INFO(vtag_timewait)[(tag % SCTP_STACK_VTAG_HASH_SIZE)];
	LIST_FOREACH(twait_block, chain, sctp_nxt_tagblock) {
		for (i = 0; i < SCTP_NUMBER_IN_VTAG_BLOCK; i++) {
			if ((twait_block->vtag_block[i].tv_sec_at_expire >= now) &&
			    (twait_block->vtag_block[i].v_tag == tag) &&
			    (twait_block->vtag_block[i].lport == lport) &&
			    (twait_block->vtag_block[i].rport == rport)) {
				return (true);
			}
		}
	}
	return (false);
}

static void
sctp_set_vtag_block(struct sctp_timewait *vtag_block, uint32_t time,
    uint32_t tag, uint16_t lport, uint16_t rport)
{
	vtag_block->tv_sec_at_expire = time;
	vtag_block->v_tag = tag;
	vtag_block->lport = lport;
	vtag_block->rport = rport;
}

static void
sctp_add_vtag_to_timewait(uint32_t tag, uint16_t lport, uint16_t rport)
{
	struct sctpvtaghead *chain;
	struct sctp_tagblock *twait_block;
	struct timeval now;
	uint32_t time;
	int i;
	bool set;

	SCTP_INP_INFO_WLOCK_ASSERT();
	(void)SCTP_GETTIME_TIMEVAL(&now);
	time = (uint32_t)now.tv_sec + SCTP_BASE_SYSCTL(sctp_vtag_time_wait);
	chain = &SCTP_BASE_INFO(vtag_timewait)[(tag % SCTP_STACK_VTAG_HASH_SIZE)];
	set = false;
	LIST_FOREACH(twait_block, chain, sctp_nxt_tagblock) {

		for (i = 0; i < SCTP_NUMBER_IN_VTAG_BLOCK; i++) {
			if ((twait_block->vtag_block[i].v_tag == 0) && !set) {
				sctp_set_vtag_block(twait_block->vtag_block + i, time, tag, lport, rport);
				set = true;
				continue;
			}
			if ((twait_block->vtag_block[i].v_tag != 0) &&
			    (twait_block->vtag_block[i].tv_sec_at_expire < (uint32_t)now.tv_sec)) {
				if (set) {

					sctp_set_vtag_block(twait_block->vtag_block + i, 0, 0, 0, 0);
				} else {

					sctp_set_vtag_block(twait_block->vtag_block + i, time, tag, lport, rport);
					set = true;
				}
			}
		}
		if (set) {

			break;
		}
	}

	if (!set) {
		SCTP_MALLOC(twait_block, struct sctp_tagblock *,
		    sizeof(struct sctp_tagblock), SCTP_M_TIMW);
		if (twait_block == NULL) {
			return;
		}
		memset(twait_block, 0, sizeof(struct sctp_tagblock));
		LIST_INSERT_HEAD(chain, twait_block, sctp_nxt_tagblock);
		sctp_set_vtag_block(twait_block->vtag_block, time, tag, lport, rport);
	}
}

void
sctp_clean_up_stream(struct sctp_tcb *stcb, struct sctp_readhead *rh)
{
	struct sctp_tmit_chunk *chk, *nchk;
	struct sctp_queued_to_read *control, *ncontrol;

	TAILQ_FOREACH_SAFE(control, rh, next_instrm, ncontrol) {
		TAILQ_REMOVE(rh, control, next_instrm);
		control->on_strm_q = 0;
		if (control->on_read_q == 0) {
			sctp_free_remote_addr(control->whoFrom);
			if (control->data) {
				sctp_m_freem(control->data);
				control->data = NULL;
			}
		}

		TAILQ_FOREACH_SAFE(chk, &control->reasm, sctp_next, nchk) {
			TAILQ_REMOVE(&control->reasm, chk, sctp_next);
			if (chk->data) {
				sctp_m_freem(chk->data);
				chk->data = NULL;
			}
			if (chk->holds_key_ref)
				sctp_auth_key_release(stcb, chk->auth_keyid, SCTP_SO_LOCKED);
			sctp_free_remote_addr(chk->whoTo);
			SCTP_ZONE_FREE(SCTP_BASE_INFO(ipi_zone_chunk), chk);
			SCTP_DECR_CHK_COUNT();

		}

		if (control->on_read_q == 0) {
			sctp_free_a_readq(stcb, control);
		}
	}
}

int
sctp_free_assoc(struct sctp_inpcb *inp, struct sctp_tcb *stcb, int from_inpcbfree, int from_location)
{
	int i;
	struct sctp_association *asoc;
	struct sctp_nets *net, *nnet;
	struct sctp_laddr *laddr, *naddr;
	struct sctp_tmit_chunk *chk, *nchk;
	struct sctp_asconf_addr *aparam, *naparam;
	struct sctp_asconf_ack *aack, *naack;
	struct sctp_stream_reset_list *strrst, *nstrrst;
	struct sctp_queued_to_read *sq, *nsq;
	struct sctp_stream_queue_pending *sp, *nsp;
	sctp_sharedkey_t *shared_key, *nshared_key;
	struct socket *so;

#if defined(__APPLE__) && !defined(__Userspace__)
	sctp_lock_assert(SCTP_INP_SO(inp));
#endif

#ifdef SCTP_LOG_CLOSING
	sctp_log_closing(inp, stcb, 6);
#endif
	if (stcb->asoc.state == 0) {
#ifdef SCTP_LOG_CLOSING
		sctp_log_closing(inp, NULL, 7);
#endif

		return (1);
	}
	SCTP_TCB_SEND_LOCK(stcb);
	if (stcb->asoc.alternate) {
		sctp_free_remote_addr(stcb->asoc.alternate);
		stcb->asoc.alternate = NULL;
	}
#if !(defined(__APPLE__) && !defined(__Userspace__))

	if (stcb->freed_from_where == 0) {

		stcb->freed_from_where = from_location;
	}

#endif

	asoc = &stcb->asoc;
	if ((inp->sctp_flags & SCTP_PCB_FLAGS_SOCKET_ALLGONE) ||
	    (inp->sctp_flags & SCTP_PCB_FLAGS_SOCKET_GONE))

		so = NULL;
	else
		so = inp->sctp_socket;

	if ((stcb->asoc.state & SCTP_STATE_ABOUT_TO_BE_FREED) &&
	    (from_inpcbfree == SCTP_NORMAL_PROC)) {

		if (stcb->asoc.refcnt) {

			sctp_timer_start(SCTP_TIMER_TYPE_ASOCKILL, inp, stcb, NULL);

			SCTP_TCB_SEND_UNLOCK(stcb);
			SCTP_TCB_UNLOCK(stcb);
#ifdef SCTP_LOG_CLOSING
			sctp_log_closing(inp, stcb, 8);
#endif
			return (0);
		}
	}

	sctp_stop_association_timers(stcb, false);

	if ((stcb->asoc.state & SCTP_STATE_ABOUT_TO_BE_FREED) == 0) {
		SCTP_ADD_SUBSTATE(stcb, SCTP_STATE_ABOUT_TO_BE_FREED);
		SCTP_INP_READ_LOCK(inp);
		TAILQ_FOREACH(sq, &inp->read_queue, next) {
			if (sq->stcb == stcb) {
				sq->do_not_ref_stcb = 1;
				sq->sinfo_cumtsn = stcb->asoc.cumulative_tsn;

				if (sq->end_added == 0) {

					sq->pdapi_aborted = 1;
					sq->held_length = 0;
					if (sctp_stcb_is_feature_on(inp, stcb, SCTP_PCB_FLAGS_PDAPIEVNT) && (so != NULL)) {

						uint32_t strseq;
						stcb->asoc.control_pdapi = sq;
						strseq = (sq->sinfo_stream << 16) | (sq->mid & 0x0000ffff);
						sctp_ulp_notify(SCTP_NOTIFY_PARTIAL_DELVIERY_INDICATION,
						                stcb,
						                SCTP_PARTIAL_DELIVERY_ABORTED,
						                (void *)&strseq,
						                SCTP_SO_LOCKED);
						stcb->asoc.control_pdapi = NULL;
					}
				}

				sq->end_added = 1;
			}
		}
		SCTP_INP_READ_UNLOCK(inp);
		if (stcb->block_entry) {
			SCTP_LTRACE_ERR_RET(inp, stcb, NULL, SCTP_FROM_SCTP_PCB, ECONNRESET);
			stcb->block_entry->error = ECONNRESET;
			stcb->block_entry = NULL;
		}
	}
	if ((stcb->asoc.refcnt) || (stcb->asoc.state & SCTP_STATE_IN_ACCEPT_QUEUE)) {

		if ((stcb->asoc.refcnt)  ||
		    (inp->sctp_flags & SCTP_PCB_FLAGS_SOCKET_ALLGONE) ||
		    (inp->sctp_flags & SCTP_PCB_FLAGS_SOCKET_GONE)) {
			SCTP_CLEAR_SUBSTATE(stcb, SCTP_STATE_IN_ACCEPT_QUEUE);
			sctp_timer_start(SCTP_TIMER_TYPE_ASOCKILL, inp, stcb, NULL);
		}
		if ((inp->sctp_flags & SCTP_PCB_FLAGS_SOCKET_ALLGONE) ||
		    (inp->sctp_flags & SCTP_PCB_FLAGS_SOCKET_GONE))

			so = NULL;
		if (so) {

			sctp_sorwakeup(inp, so);
			sctp_sowwakeup(inp, so);
		}
		SCTP_TCB_SEND_UNLOCK(stcb);
		SCTP_TCB_UNLOCK(stcb);

#ifdef SCTP_LOG_CLOSING
		sctp_log_closing(inp, stcb, 9);
#endif

		return (0);
	}
#ifdef SCTP_LOG_CLOSING
	sctp_log_closing(inp, stcb, 10);
#endif

	if (from_inpcbfree == SCTP_NORMAL_PROC) {
		atomic_add_int(&stcb->asoc.refcnt, 1);

		SCTP_TCB_SEND_UNLOCK(stcb);
		SCTP_TCB_UNLOCK(stcb);
		SCTP_INP_INFO_WLOCK();
		SCTP_INP_WLOCK(inp);
		SCTP_TCB_LOCK(stcb);
		SCTP_TCB_SEND_LOCK(stcb);
	}

	if ((inp->sctp_flags & SCTP_PCB_FLAGS_SOCKET_ALLGONE) ||
	    (inp->sctp_flags & SCTP_PCB_FLAGS_SOCKET_GONE))

		so = NULL;

	if ((inp->sctp_flags & SCTP_PCB_FLAGS_TCPTYPE) ||
	    (inp->sctp_flags & SCTP_PCB_FLAGS_IN_TCPPOOL)) {

		if (inp->sctp_flags & SCTP_PCB_FLAGS_CONNECTED) {
			inp->sctp_flags &= ~SCTP_PCB_FLAGS_CONNECTED;
			inp->sctp_flags |= SCTP_PCB_FLAGS_WAS_CONNECTED;
			if (so) {
				SOCKBUF_LOCK(&so->so_rcv);
				so->so_state &= ~(SS_ISCONNECTING |
				    SS_ISDISCONNECTING |
				    SS_ISCONFIRMING |
				    SS_ISCONNECTED);
				so->so_state |= SS_ISDISCONNECTED;
#if defined(__APPLE__) && !defined(__Userspace__)
				socantrcvmore(so);
#else
				socantrcvmore_locked(so);
#endif
				socantsendmore(so);
				sctp_sowwakeup(inp, so);
				sctp_sorwakeup(inp, so);
				SCTP_SOWAKEUP(so);
			}
		}
	}

	if (from_inpcbfree == SCTP_NORMAL_PROC) {
		atomic_add_int(&stcb->asoc.refcnt, -1);
	}
	if (stcb->asoc.refcnt) {
		SCTP_CLEAR_SUBSTATE(stcb, SCTP_STATE_IN_ACCEPT_QUEUE);
		sctp_timer_start(SCTP_TIMER_TYPE_ASOCKILL, inp, stcb, NULL);
		if (from_inpcbfree == SCTP_NORMAL_PROC) {
			SCTP_INP_INFO_WUNLOCK();
			SCTP_INP_WUNLOCK(inp);
		}
		SCTP_TCB_SEND_UNLOCK(stcb);
		SCTP_TCB_UNLOCK(stcb);
		return (0);
	}
	asoc->state = 0;
	if (inp->sctp_tcbhash) {
		LIST_REMOVE(stcb, sctp_tcbhash);
	}
	if (stcb->asoc.in_asocid_hash) {
		LIST_REMOVE(stcb, sctp_tcbasocidhash);
	}
	if (inp->sctp_socket == NULL) {
		stcb->sctp_socket = NULL;
	}

	LIST_REMOVE(stcb, sctp_tcblist);
	if (from_inpcbfree == SCTP_NORMAL_PROC) {
		SCTP_INP_INCR_REF(inp);
		SCTP_INP_WUNLOCK(inp);
	}

	LIST_REMOVE(stcb, sctp_asocs);
	sctp_add_vtag_to_timewait(asoc->my_vtag, inp->sctp_lport, stcb->rport);

	sctp_stop_association_timers(stcb, true);

	for (i = 0; i < asoc->streamoutcnt; i++) {
		struct sctp_stream_out *outs;

		outs = &asoc->strmout[i];

		TAILQ_FOREACH_SAFE(sp, &outs->outqueue, next, nsp) {
			atomic_subtract_int(&asoc->stream_queue_cnt, 1);
			TAILQ_REMOVE(&outs->outqueue, sp, next);
			stcb->asoc.ss_functions.sctp_ss_remove_from_stream(stcb, asoc, outs, sp, 1);
			sctp_free_spbufspace(stcb, asoc, sp);
			if (sp->data) {
				if (so) {

					sctp_ulp_notify(SCTP_NOTIFY_SPECIAL_SP_FAIL, stcb,
					                0, (void *)sp, SCTP_SO_LOCKED);
				}
				if (sp->data) {
					sctp_m_freem(sp->data);
					sp->data = NULL;
					sp->tail_mbuf = NULL;
					sp->length = 0;
				}
			}
			if (sp->net) {
				sctp_free_remote_addr(sp->net);
				sp->net = NULL;
			}
			sctp_free_a_strmoq(stcb, sp, SCTP_SO_LOCKED);
		}
	}

	TAILQ_FOREACH_SAFE(strrst, &asoc->resetHead, next_resp, nstrrst) {
		TAILQ_REMOVE(&asoc->resetHead, strrst, next_resp);
		SCTP_FREE(strrst, SCTP_M_STRESET);
	}
	TAILQ_FOREACH_SAFE(sq, &asoc->pending_reply_queue, next, nsq) {
		TAILQ_REMOVE(&asoc->pending_reply_queue, sq, next);
		if (sq->data) {
			sctp_m_freem(sq->data);
			sq->data = NULL;
		}
		sctp_free_remote_addr(sq->whoFrom);
		sq->whoFrom = NULL;
		sq->stcb = NULL;

		sctp_free_a_readq(stcb, sq);

	}
	TAILQ_FOREACH_SAFE(chk, &asoc->free_chunks, sctp_next, nchk) {
		TAILQ_REMOVE(&asoc->free_chunks, chk, sctp_next);
		if (chk->data) {
			sctp_m_freem(chk->data);
			chk->data = NULL;
		}
		if (chk->holds_key_ref)
			sctp_auth_key_release(stcb, chk->auth_keyid, SCTP_SO_LOCKED);
		SCTP_ZONE_FREE(SCTP_BASE_INFO(ipi_zone_chunk), chk);
		SCTP_DECR_CHK_COUNT();
		atomic_subtract_int(&SCTP_BASE_INFO(ipi_free_chunks), 1);
		asoc->free_chunk_cnt--;

	}

	TAILQ_FOREACH_SAFE(chk, &asoc->send_queue, sctp_next, nchk) {
		if (asoc->strmout[chk->rec.data.sid].chunks_on_queues > 0) {
			asoc->strmout[chk->rec.data.sid].chunks_on_queues--;
#ifdef INVARIANTS
		} else {
			panic("No chunks on the queues for sid %u.", chk->rec.data.sid);
#endif
		}
		TAILQ_REMOVE(&asoc->send_queue, chk, sctp_next);
		if (chk->data) {
			if (so) {

				sctp_ulp_notify(SCTP_NOTIFY_UNSENT_DG_FAIL, stcb,
				                0, chk, SCTP_SO_LOCKED);
			}
			if (chk->data) {
				sctp_m_freem(chk->data);
				chk->data = NULL;
			}
		}
		if (chk->holds_key_ref)
			sctp_auth_key_release(stcb, chk->auth_keyid, SCTP_SO_LOCKED);
		if (chk->whoTo) {
			sctp_free_remote_addr(chk->whoTo);
			chk->whoTo = NULL;
		}
		SCTP_ZONE_FREE(SCTP_BASE_INFO(ipi_zone_chunk), chk);
		SCTP_DECR_CHK_COUNT();

	}

	TAILQ_FOREACH_SAFE(chk, &asoc->sent_queue, sctp_next, nchk) {
		if (chk->sent != SCTP_DATAGRAM_NR_ACKED) {
			if (asoc->strmout[chk->rec.data.sid].chunks_on_queues > 0) {
				asoc->strmout[chk->rec.data.sid].chunks_on_queues--;
#ifdef INVARIANTS
			} else {
				panic("No chunks on the queues for sid %u.", chk->rec.data.sid);
#endif
			}
		}
		TAILQ_REMOVE(&asoc->sent_queue, chk, sctp_next);
		if (chk->data) {
			if (so) {

				sctp_ulp_notify(SCTP_NOTIFY_SENT_DG_FAIL, stcb,
				                0, chk, SCTP_SO_LOCKED);
			}
			if (chk->data) {
				sctp_m_freem(chk->data);
				chk->data = NULL;
			}
		}
		if (chk->holds_key_ref)
			sctp_auth_key_release(stcb, chk->auth_keyid, SCTP_SO_LOCKED);
		sctp_free_remote_addr(chk->whoTo);
		SCTP_ZONE_FREE(SCTP_BASE_INFO(ipi_zone_chunk), chk);
		SCTP_DECR_CHK_COUNT();

	}
#ifdef INVARIANTS
	for (i = 0; i < stcb->asoc.streamoutcnt; i++) {
		if (stcb->asoc.strmout[i].chunks_on_queues > 0) {
			panic("%u chunks left for stream %u.", stcb->asoc.strmout[i].chunks_on_queues, i);
		}
	}
#endif

	TAILQ_FOREACH_SAFE(chk, &asoc->control_send_queue, sctp_next, nchk) {
		TAILQ_REMOVE(&asoc->control_send_queue, chk, sctp_next);
		if (chk->data) {
			sctp_m_freem(chk->data);
			chk->data = NULL;
		}
		if (chk->holds_key_ref)
			sctp_auth_key_release(stcb, chk->auth_keyid, SCTP_SO_LOCKED);
		sctp_free_remote_addr(chk->whoTo);
		SCTP_ZONE_FREE(SCTP_BASE_INFO(ipi_zone_chunk), chk);
		SCTP_DECR_CHK_COUNT();

	}

	TAILQ_FOREACH_SAFE(chk, &asoc->asconf_send_queue, sctp_next, nchk) {
		TAILQ_REMOVE(&asoc->asconf_send_queue, chk, sctp_next);
		if (chk->data) {
			sctp_m_freem(chk->data);
			chk->data = NULL;
		}
		if (chk->holds_key_ref)
			sctp_auth_key_release(stcb, chk->auth_keyid, SCTP_SO_LOCKED);
		sctp_free_remote_addr(chk->whoTo);
		SCTP_ZONE_FREE(SCTP_BASE_INFO(ipi_zone_chunk), chk);
		SCTP_DECR_CHK_COUNT();

	}
	if (asoc->mapping_array) {
		SCTP_FREE(asoc->mapping_array, SCTP_M_MAP);
		asoc->mapping_array = NULL;
	}
	if (asoc->nr_mapping_array) {
		SCTP_FREE(asoc->nr_mapping_array, SCTP_M_MAP);
		asoc->nr_mapping_array = NULL;
	}

	if (asoc->strmout) {
		SCTP_FREE(asoc->strmout, SCTP_M_STRMO);
		asoc->strmout = NULL;
	}
	asoc->strm_realoutsize = asoc->streamoutcnt = 0;
	if (asoc->strmin) {
		for (i = 0; i < asoc->streamincnt; i++) {
			sctp_clean_up_stream(stcb, &asoc->strmin[i].inqueue);
			sctp_clean_up_stream(stcb, &asoc->strmin[i].uno_inqueue);
		}
		SCTP_FREE(asoc->strmin, SCTP_M_STRMI);
		asoc->strmin = NULL;
	}
	asoc->streamincnt = 0;
	TAILQ_FOREACH_SAFE(net, &asoc->nets, sctp_next, nnet) {
#ifdef INVARIANTS
		if (SCTP_BASE_INFO(ipi_count_raddr) == 0) {
			panic("no net's left alloc'ed, or list points to itself");
		}
#endif
		TAILQ_REMOVE(&asoc->nets, net, sctp_next);
		sctp_free_remote_addr(net);
	}
	LIST_FOREACH_SAFE(laddr, &asoc->sctp_restricted_addrs, sctp_nxt_addr, naddr) {

		sctp_remove_laddr(laddr);
	}

	TAILQ_FOREACH_SAFE(aparam, &asoc->asconf_queue, next, naparam) {

		TAILQ_REMOVE(&asoc->asconf_queue, aparam, next);
		SCTP_FREE(aparam,SCTP_M_ASC_ADDR);
	}
	TAILQ_FOREACH_SAFE(aack, &asoc->asconf_ack_sent, next, naack) {

		TAILQ_REMOVE(&asoc->asconf_ack_sent, aack, next);
		if (aack->data != NULL) {
			sctp_m_freem(aack->data);
		}
		SCTP_ZONE_FREE(SCTP_BASE_INFO(ipi_zone_asconf_ack), aack);
	}

	if (asoc->local_hmacs)
		sctp_free_hmaclist(asoc->local_hmacs);
	if (asoc->peer_hmacs)
		sctp_free_hmaclist(asoc->peer_hmacs);

	if (asoc->local_auth_chunks)
		sctp_free_chunklist(asoc->local_auth_chunks);
	if (asoc->peer_auth_chunks)
		sctp_free_chunklist(asoc->peer_auth_chunks);

	sctp_free_authinfo(&asoc->authinfo);

	LIST_FOREACH_SAFE(shared_key, &asoc->shared_keys, next, nshared_key) {
		LIST_REMOVE(shared_key, next);
		sctp_free_sharedkey(shared_key);

	}

	SCTP_TCB_SEND_UNLOCK(stcb);
	SCTP_TCB_UNLOCK(stcb);
	SCTP_TCB_LOCK_DESTROY(stcb);
	SCTP_TCB_SEND_LOCK_DESTROY(stcb);
	if (from_inpcbfree == SCTP_NORMAL_PROC) {
		SCTP_INP_INFO_WUNLOCK();
		SCTP_INP_RLOCK(inp);
	}
#if defined(__APPLE__) && !defined(__Userspace__)

	stcb->freed_from_where = from_location;
#endif
#ifdef SCTP_TRACK_FREED_ASOCS
	if (inp->sctp_flags & SCTP_PCB_FLAGS_SOCKET_GONE) {

		SCTP_ZONE_FREE(SCTP_BASE_INFO(ipi_zone_asoc), stcb);
		SCTP_DECR_ASOC_COUNT();
	} else {
		LIST_INSERT_HEAD(&inp->sctp_asoc_free_list, stcb, sctp_tcblist);
	}
#else
	SCTP_ZONE_FREE(SCTP_BASE_INFO(ipi_zone_asoc), stcb);
	SCTP_DECR_ASOC_COUNT();
#endif
	if (from_inpcbfree == SCTP_NORMAL_PROC) {
		if (inp->sctp_flags & SCTP_PCB_FLAGS_SOCKET_GONE) {

			SCTP_INP_RUNLOCK(inp);

			sctp_inpcb_free(inp,
					SCTP_FREE_SHOULD_USE_GRACEFUL_CLOSE,
					SCTP_CALLED_DIRECTLY_NOCMPSET);
			SCTP_INP_DECR_REF(inp);
		} else {

			SCTP_INP_DECR_REF(inp);
			SCTP_INP_RUNLOCK(inp);
		}
	}

#ifdef SCTP_LOG_CLOSING
	sctp_log_closing(inp, NULL, 11);
#endif
	return (1);
}

int
sctp_destination_is_reachable(struct sctp_tcb *stcb, struct sockaddr *destaddr)
{
	struct sctp_inpcb *inp;
	int answer;

	inp = stcb->sctp_ep;
	if (inp->sctp_flags & SCTP_PCB_FLAGS_BOUNDALL) {

		return (1);
	}

	switch (destaddr->sa_family) {
#ifdef INET6
	case AF_INET6:
		answer = inp->ip_inp.inp.inp_vflag & INP_IPV6;
		break;
#endif
#ifdef INET
	case AF_INET:
		answer = inp->ip_inp.inp.inp_vflag & INP_IPV4;
		break;
#endif
#if defined(__Userspace__)
	case AF_CONN:
		answer = inp->ip_inp.inp.inp_vflag & INP_CONN;
		break;
#endif
	default:

		answer = 0;
		break;
	}
	return (answer);
}

static void
sctp_update_ep_vflag(struct sctp_inpcb *inp)
{
	struct sctp_laddr *laddr;

	inp->ip_inp.inp.inp_vflag = 0;

	LIST_FOREACH(laddr, &inp->sctp_addr_list, sctp_nxt_addr) {
		if (laddr->ifa == NULL) {
			SCTPDBG(SCTP_DEBUG_PCB1, "%s: NULL ifa\n",
				__func__);
			continue;
		}

		if (laddr->ifa->localifa_flags & SCTP_BEING_DELETED) {
			continue;
		}
		switch (laddr->ifa->address.sa.sa_family) {
#ifdef INET6
		case AF_INET6:
			inp->ip_inp.inp.inp_vflag |= INP_IPV6;
			break;
#endif
#ifdef INET
		case AF_INET:
			inp->ip_inp.inp.inp_vflag |= INP_IPV4;
			break;
#endif
#if defined(__Userspace__)
		case AF_CONN:
			inp->ip_inp.inp.inp_vflag |= INP_CONN;
			break;
#endif
		default:
			break;
		}
	}
}

void
sctp_add_local_addr_ep(struct sctp_inpcb *inp, struct sctp_ifa *ifa, uint32_t action)
{
	struct sctp_laddr *laddr;
	struct sctp_tcb *stcb;
	int fnd, error = 0;

	fnd = 0;

	if (inp->sctp_flags & SCTP_PCB_FLAGS_BOUNDALL) {

		return;
	}
#ifdef INET6
	if (ifa->address.sa.sa_family == AF_INET6) {
		if (ifa->localifa_flags & SCTP_ADDR_IFA_UNUSEABLE) {

			return;
		}
	}
#endif

	LIST_FOREACH(laddr, &inp->sctp_addr_list, sctp_nxt_addr) {
		if (laddr->ifa == ifa) {
			fnd = 1;
			break;
		}
	}

	if (fnd == 0) {

		error = sctp_insert_laddr(&inp->sctp_addr_list, ifa, action);
		if (error != 0)
			return;
		inp->laddr_count++;

		switch (ifa->address.sa.sa_family) {
#ifdef INET6
		case AF_INET6:
			inp->ip_inp.inp.inp_vflag |= INP_IPV6;
			break;
#endif
#ifdef INET
		case AF_INET:
			inp->ip_inp.inp.inp_vflag |= INP_IPV4;
			break;
#endif
#if defined(__Userspace__)
		case AF_CONN:
			inp->ip_inp.inp.inp_vflag |= INP_CONN;
			break;
#endif
		default:
			break;
		}
		LIST_FOREACH(stcb, &inp->sctp_asoc_list, sctp_tcblist) {
			sctp_add_local_addr_restricted(stcb, ifa);
		}
	}
	return;
}

static void
sctp_select_primary_destination(struct sctp_tcb *stcb)
{
	struct sctp_nets *net;

	TAILQ_FOREACH(net, &stcb->asoc.nets, sctp_next) {

		if (net->dest_state & SCTP_ADDR_UNCONFIRMED)
			continue;
		if (sctp_destination_is_reachable(stcb,
		    (struct sockaddr *)&net->ro._l_addr)) {

			stcb->asoc.primary_destination = net;
		}
	}

}

void
sctp_del_local_addr_ep(struct sctp_inpcb *inp, struct sctp_ifa *ifa)
{
	struct sctp_laddr *laddr;
	int fnd;

	fnd = 0;
	if (inp->sctp_flags & SCTP_PCB_FLAGS_BOUNDALL) {

		return;
	}
	LIST_FOREACH(laddr, &inp->sctp_addr_list, sctp_nxt_addr) {
		if (laddr->ifa == ifa) {
			fnd = 1;
			break;
		}
	}
	if (fnd && (inp->laddr_count < 2)) {

		return;
	}
	if (fnd) {

		struct sctp_tcb *stcb;

		if (inp->next_addr_touse == laddr)

			inp->next_addr_touse = NULL;

		LIST_FOREACH(stcb, &inp->sctp_asoc_list, sctp_tcblist) {
			struct sctp_nets *net;

			SCTP_TCB_LOCK(stcb);
			if (stcb->asoc.last_used_address == laddr)

				stcb->asoc.last_used_address = NULL;

			TAILQ_FOREACH(net, &stcb->asoc.nets, sctp_next) {
				if (net->ro._s_addr == laddr->ifa) {

#if defined(__FreeBSD__) && !defined(__Userspace__)
					RO_NHFREE(&net->ro);
#else
					sctp_rtentry_t *rt;

					rt = net->ro.ro_rt;
					if (rt != NULL) {
						RTFREE(rt);
						net->ro.ro_rt = NULL;
					}
#endif
					sctp_free_ifa(net->ro._s_addr);
					net->ro._s_addr = NULL;
					net->src_addr_selected = 0;
				}
			}
			SCTP_TCB_UNLOCK(stcb);
		}

		sctp_remove_laddr(laddr);
		inp->laddr_count--;

		sctp_update_ep_vflag(inp);
	}
	return;
}

void
sctp_add_local_addr_restricted(struct sctp_tcb *stcb, struct sctp_ifa *ifa)
{
	struct sctp_laddr *laddr;
	struct sctpladdr *list;

	list = &stcb->asoc.sctp_restricted_addrs;

#ifdef INET6
	if (ifa->address.sa.sa_family == AF_INET6) {
		if (ifa->localifa_flags & SCTP_ADDR_IFA_UNUSEABLE) {

			return;
		}
	}
#endif

	LIST_FOREACH(laddr, list, sctp_nxt_addr) {
		if (laddr->ifa == ifa) {
			return;
		}
	}

	(void)sctp_insert_laddr(list, ifa, 0);
	return;
}

void
sctp_del_local_addr_restricted(struct sctp_tcb *stcb, struct sctp_ifa *ifa)
{
	struct sctp_inpcb *inp;
	struct sctp_laddr *laddr;

	inp = stcb->sctp_ep;

	if (((inp->sctp_flags & SCTP_PCB_FLAGS_BOUNDALL) == 0) &&
	    sctp_is_feature_off(inp, SCTP_PCB_FLAGS_DO_ASCONF)) {
		if (stcb->sctp_ep->laddr_count < 2) {

			return;
		}
	}
	LIST_FOREACH(laddr, &stcb->asoc.sctp_restricted_addrs, sctp_nxt_addr) {

		if (laddr->ifa == NULL)
			continue;
		if (laddr->ifa == ifa) {
			sctp_remove_laddr(laddr);
			return;
		}
	}

	return;
}

#if defined(__FreeBSD__) && !defined(__Userspace__)

static int sctp_max_number_of_assoc = SCTP_MAX_NUM_OF_ASOC;
static int sctp_scale_up_for_address = SCTP_SCALE_FOR_ADDR;
#endif

#if defined(__FreeBSD__) && !defined(__Userspace__)
#if defined(SCTP_MCORE_INPUT) && defined(SMP)
struct sctp_mcore_ctrl *sctp_mcore_workers = NULL;
int *sctp_cpuarry = NULL;

void
sctp_queue_to_mcore(struct mbuf *m, int off, int cpu_to_use)
{

	struct sctp_mcore_queue *qent;
	struct sctp_mcore_ctrl *wkq;
	int need_wake = 0;

	if (sctp_mcore_workers == NULL) {

		sctp_input_with_port(m, off, 0);
		return;
	}
	SCTP_MALLOC(qent, struct sctp_mcore_queue *,
		    (sizeof(struct sctp_mcore_queue)),
		    SCTP_M_MCORE);
	if (qent == NULL) {

		sctp_input_with_port(m, off, 0);
		return;
	}
	qent->vn = curvnet;
	qent->m = m;
	qent->off = off;
	qent->v6 = 0;
	wkq = &sctp_mcore_workers[cpu_to_use];
	SCTP_MCORE_QLOCK(wkq);

	TAILQ_INSERT_TAIL(&wkq->que, qent, next);
	if (wkq->running == 0) {
		need_wake = 1;
	}
	SCTP_MCORE_QUNLOCK(wkq);
	if (need_wake) {
		wakeup(&wkq->running);
	}
}

static void
sctp_mcore_thread(void *arg)
{

	struct sctp_mcore_ctrl *wkq;
	struct sctp_mcore_queue *qent;

	wkq = (struct sctp_mcore_ctrl *)arg;
	struct mbuf *m;
	int off, v6;

	SCTP_MCORE_LOCK(wkq);
	wkq->running = 0;
	msleep(&wkq->running,
	       &wkq->core_mtx,
	       0, "wait for pkt", 0);
	SCTP_MCORE_UNLOCK(wkq);

	thread_lock(curthread);
	sched_bind(curthread, wkq->cpuid);
	thread_unlock(curthread);

	SCTP_MCORE_LOCK(wkq);

	for (;;) {
		SCTP_MCORE_QLOCK(wkq);
	skip_sleep:
		wkq->running = 1;
		qent = TAILQ_FIRST(&wkq->que);
		if (qent) {
			TAILQ_REMOVE(&wkq->que, qent, next);
			SCTP_MCORE_QUNLOCK(wkq);
			CURVNET_SET(qent->vn);
			m = qent->m;
			off = qent->off;
			v6 = qent->v6;
			SCTP_FREE(qent, SCTP_M_MCORE);
			if (v6 == 0) {
				sctp_input_with_port(m, off, 0);
			} else {
				SCTP_PRINTF("V6 not yet supported\n");
				sctp_m_freem(m);
			}
			CURVNET_RESTORE();
			SCTP_MCORE_QLOCK(wkq);
		}
		wkq->running = 0;
		if (!TAILQ_EMPTY(&wkq->que)) {
			goto skip_sleep;
		}
		SCTP_MCORE_QUNLOCK(wkq);
		msleep(&wkq->running,
		       &wkq->core_mtx,
		       0, "wait for pkt", 0);
	}
}

static void
sctp_startup_mcore_threads(void)
{
	int i, cpu;

	if (mp_ncpus == 1)
		return;

	if (sctp_mcore_workers != NULL) {

		return;
	}
	SCTP_MALLOC(sctp_mcore_workers, struct sctp_mcore_ctrl *,
		    ((mp_maxid+1) * sizeof(struct sctp_mcore_ctrl)),
		    SCTP_M_MCORE);
	if (sctp_mcore_workers == NULL) {

		return;
	}
	memset(sctp_mcore_workers, 0 , ((mp_maxid+1) *
					sizeof(struct sctp_mcore_ctrl)));

	for (i = 0; i<=mp_maxid; i++) {
		TAILQ_INIT(&sctp_mcore_workers[i].que);
		SCTP_MCORE_LOCK_INIT(&sctp_mcore_workers[i]);
		SCTP_MCORE_QLOCK_INIT(&sctp_mcore_workers[i]);
		sctp_mcore_workers[i].cpuid = i;
	}
	if (sctp_cpuarry == NULL) {
		SCTP_MALLOC(sctp_cpuarry, int *,
			    (mp_ncpus * sizeof(int)),
			    SCTP_M_MCORE);
		i = 0;
		CPU_FOREACH(cpu) {
			sctp_cpuarry[i] = cpu;
			i++;
		}
	}

	CPU_FOREACH(cpu) {
		(void)kproc_create(sctp_mcore_thread,
				   (void *)&sctp_mcore_workers[cpu],
				   &sctp_mcore_workers[cpu].thread_proc,
				   0,
				   SCTP_KTHREAD_PAGES,
				   SCTP_MCORE_NAME);
	}
}
#endif
#endif

#if defined(__FreeBSD__) && !defined(__Userspace__)
#if defined(SCTP_NOT_YET)
static struct mbuf *
sctp_netisr_hdlr(struct mbuf *m, uintptr_t source)
{
	struct ip *ip;
	struct sctphdr *sh;
	int offset;
	uint32_t flowid, tag;

	ip = mtod(m, struct ip *);
	offset = (ip->ip_hl << 2) + sizeof(struct sctphdr);
	if (SCTP_BUF_LEN(m) < offset) {
		if ((m = m_pullup(m, offset)) == NULL) {
			SCTP_STAT_INCR(sctps_hdrops);
			return (NULL);
		}
		ip = mtod(m, struct ip *);
	}
	sh = (struct sctphdr *)((caddr_t)ip + (ip->ip_hl << 2));
	tag = htonl(sh->v_tag);
	flowid = tag ^ ntohs(sh->dest_port) ^ ntohs(sh->src_port);
	m->m_pkthdr.flowid = flowid;

	m->m_flags |= M_FLOWID;
	return (m);
}

#endif
#endif
void
#if defined(__Userspace__)
sctp_pcb_init(int start_threads)
#else
sctp_pcb_init(void)
#endif
{

	int i;
	struct timeval tv;

	if (SCTP_BASE_VAR(sctp_pcb_initialized) != 0) {

		return;
	}
	SCTP_BASE_VAR(sctp_pcb_initialized) = 1;

#if defined(SCTP_PROCESS_LEVEL_LOCKS)
#if !defined(_WIN32)
	pthread_mutexattr_init(&SCTP_BASE_VAR(mtx_attr));
#ifdef INVARIANTS
	pthread_mutexattr_settype(&SCTP_BASE_VAR(mtx_attr), PTHREAD_MUTEX_ERRORCHECK);
#endif
#endif
#endif
#if defined(SCTP_LOCAL_TRACE_BUF)
#if defined(_WIN32) && !defined(__Userspace__)
	if (SCTP_BASE_SYSCTL(sctp_log) != NULL) {
		memset(SCTP_BASE_SYSCTL(sctp_log), 0, sizeof(struct sctp_log));
	}
#else
	memset(&SCTP_BASE_SYSCTL(sctp_log), 0, sizeof(struct sctp_log));
#endif
#endif
#if defined(__FreeBSD__) && !defined(__Userspace__)
#if defined(SMP) && defined(SCTP_USE_PERCPU_STAT)
	SCTP_MALLOC(SCTP_BASE_STATS, struct sctpstat *,
		    ((mp_maxid+1) * sizeof(struct sctpstat)),
		    SCTP_M_MCORE);
#endif
#endif
	(void)SCTP_GETTIME_TIMEVAL(&tv);
#if defined(__FreeBSD__) && !defined(__Userspace__)
#if defined(SMP) && defined(SCTP_USE_PERCPU_STAT)
	memset(SCTP_BASE_STATS, 0, sizeof(struct sctpstat) * (mp_maxid+1));
	SCTP_BASE_STATS[PCPU_GET(cpuid)].sctps_discontinuitytime.tv_sec = (uint32_t)tv.tv_sec;
	SCTP_BASE_STATS[PCPU_GET(cpuid)].sctps_discontinuitytime.tv_usec = (uint32_t)tv.tv_usec;
#else
	memset(&SCTP_BASE_STATS, 0, sizeof(struct sctpstat));
	SCTP_BASE_STAT(sctps_discontinuitytime).tv_sec = (uint32_t)tv.tv_sec;
	SCTP_BASE_STAT(sctps_discontinuitytime).tv_usec = (uint32_t)tv.tv_usec;
#endif
#else
	memset(&SCTP_BASE_STATS, 0, sizeof(struct sctpstat));
	SCTP_BASE_STAT(sctps_discontinuitytime).tv_sec = (uint32_t)tv.tv_sec;
	SCTP_BASE_STAT(sctps_discontinuitytime).tv_usec = (uint32_t)tv.tv_usec;
#endif

	LIST_INIT(&SCTP_BASE_INFO(listhead));
#if defined(__APPLE__) && !defined(__Userspace__)
	LIST_INIT(&SCTP_BASE_INFO(inplisthead));
#if defined(APPLE_LEOPARD) || defined(APPLE_SNOWLEOPARD) || defined(APPLE_LION) || defined(APPLE_MOUNTAINLION)
	SCTP_BASE_INFO(sctbinfo).listhead = &SCTP_BASE_INFO(inplisthead);
	SCTP_BASE_INFO(sctbinfo).mtx_grp_attr = lck_grp_attr_alloc_init();
	lck_grp_attr_setdefault(SCTP_BASE_INFO(sctbinfo).mtx_grp_attr);
	SCTP_BASE_INFO(sctbinfo).mtx_grp = lck_grp_alloc_init("sctppcb", SCTP_BASE_INFO(sctbinfo).mtx_grp_attr);
	SCTP_BASE_INFO(sctbinfo).mtx_attr = lck_attr_alloc_init();
	lck_attr_setdefault(SCTP_BASE_INFO(sctbinfo).mtx_attr);
#else
	SCTP_BASE_INFO(sctbinfo).ipi_listhead = &SCTP_BASE_INFO(inplisthead);
	SCTP_BASE_INFO(sctbinfo).ipi_lock_grp_attr = lck_grp_attr_alloc_init();
	lck_grp_attr_setdefault(SCTP_BASE_INFO(sctbinfo).ipi_lock_grp_attr);
	SCTP_BASE_INFO(sctbinfo).ipi_lock_grp = lck_grp_alloc_init("sctppcb", SCTP_BASE_INFO(sctbinfo).ipi_lock_grp_attr);
	SCTP_BASE_INFO(sctbinfo).ipi_lock_attr = lck_attr_alloc_init();
	lck_attr_setdefault(SCTP_BASE_INFO(sctbinfo).ipi_lock_attr);
#endif
#if !defined(APPLE_LEOPARD) && !defined(APPLE_SNOWLEOPARD) && !defined(APPLE_LION) && !defined(APPLE_MOUNTAINLION)
	SCTP_BASE_INFO(sctbinfo).ipi_gc = sctp_gc;
	in_pcbinfo_attach(&SCTP_BASE_INFO(sctbinfo));
#endif
#endif

#if defined(__FreeBSD__) && !defined(__Userspace__)
	TUNABLE_INT_FETCH("net.inet.sctp.tcbhashsize", &SCTP_BASE_SYSCTL(sctp_hashtblsize));
	TUNABLE_INT_FETCH("net.inet.sctp.pcbhashsize", &SCTP_BASE_SYSCTL(sctp_pcbtblsize));
	TUNABLE_INT_FETCH("net.inet.sctp.chunkscale", &SCTP_BASE_SYSCTL(sctp_chunkscale));
#endif
	SCTP_BASE_INFO(sctp_asochash) = SCTP_HASH_INIT((SCTP_BASE_SYSCTL(sctp_hashtblsize) * 31),
						       &SCTP_BASE_INFO(hashasocmark));
	SCTP_BASE_INFO(sctp_ephash) = SCTP_HASH_INIT(SCTP_BASE_SYSCTL(sctp_hashtblsize),
						     &SCTP_BASE_INFO(hashmark));
	SCTP_BASE_INFO(sctp_tcpephash) = SCTP_HASH_INIT(SCTP_BASE_SYSCTL(sctp_hashtblsize),
							&SCTP_BASE_INFO(hashtcpmark));
	SCTP_BASE_INFO(hashtblsize) = SCTP_BASE_SYSCTL(sctp_hashtblsize);
	SCTP_BASE_INFO(sctp_vrfhash) = SCTP_HASH_INIT(SCTP_SIZE_OF_VRF_HASH,
						      &SCTP_BASE_INFO(hashvrfmark));

	SCTP_BASE_INFO(vrf_ifn_hash) = SCTP_HASH_INIT(SCTP_VRF_IFN_HASH_SIZE,
						      &SCTP_BASE_INFO(vrf_ifn_hashmark));

	SCTP_ZONE_INIT(SCTP_BASE_INFO(ipi_zone_ep), "sctp_ep",
		       sizeof(struct sctp_inpcb), maxsockets);

	SCTP_ZONE_INIT(SCTP_BASE_INFO(ipi_zone_asoc), "sctp_asoc",
		       sizeof(struct sctp_tcb), sctp_max_number_of_assoc);

	SCTP_ZONE_INIT(SCTP_BASE_INFO(ipi_zone_laddr), "sctp_laddr",
		       sizeof(struct sctp_laddr),
		       (sctp_max_number_of_assoc * sctp_scale_up_for_address));

	SCTP_ZONE_INIT(SCTP_BASE_INFO(ipi_zone_net), "sctp_raddr",
		       sizeof(struct sctp_nets),
		       (sctp_max_number_of_assoc * sctp_scale_up_for_address));

	SCTP_ZONE_INIT(SCTP_BASE_INFO(ipi_zone_chunk), "sctp_chunk",
		       sizeof(struct sctp_tmit_chunk),
		       (sctp_max_number_of_assoc * SCTP_BASE_SYSCTL(sctp_chunkscale)));

	SCTP_ZONE_INIT(SCTP_BASE_INFO(ipi_zone_readq), "sctp_readq",
		       sizeof(struct sctp_queued_to_read),
		       (sctp_max_number_of_assoc * SCTP_BASE_SYSCTL(sctp_chunkscale)));

	SCTP_ZONE_INIT(SCTP_BASE_INFO(ipi_zone_strmoq), "sctp_stream_msg_out",
		       sizeof(struct sctp_stream_queue_pending),
		       (sctp_max_number_of_assoc * SCTP_BASE_SYSCTL(sctp_chunkscale)));

	SCTP_ZONE_INIT(SCTP_BASE_INFO(ipi_zone_asconf), "sctp_asconf",
		       sizeof(struct sctp_asconf),
		       (sctp_max_number_of_assoc * SCTP_BASE_SYSCTL(sctp_chunkscale)));

	SCTP_ZONE_INIT(SCTP_BASE_INFO(ipi_zone_asconf_ack), "sctp_asconf_ack",
		       sizeof(struct sctp_asconf_ack),
		       (sctp_max_number_of_assoc * SCTP_BASE_SYSCTL(sctp_chunkscale)));

	SCTP_INP_INFO_LOCK_INIT();
	SCTP_STATLOG_INIT_LOCK();

	SCTP_IPI_COUNT_INIT();
	SCTP_IPI_ADDR_INIT();
#ifdef SCTP_PACKET_LOGGING
	SCTP_IP_PKTLOG_INIT();
#endif
	LIST_INIT(&SCTP_BASE_INFO(addr_wq));

	SCTP_WQ_ADDR_INIT();

	SCTP_BASE_INFO(ipi_count_ep) = 0;

	SCTP_BASE_INFO(ipi_count_asoc) = 0;

	SCTP_BASE_INFO(ipi_count_laddr) = 0;

	SCTP_BASE_INFO(ipi_count_raddr) = 0;

	SCTP_BASE_INFO(ipi_count_chunk) = 0;

	SCTP_BASE_INFO(ipi_count_readq) = 0;

	SCTP_BASE_INFO(ipi_count_strmoq) = 0;

	SCTP_BASE_INFO(ipi_free_strmoq) = 0;
	SCTP_BASE_INFO(ipi_free_chunks) = 0;

	SCTP_OS_TIMER_INIT(&SCTP_BASE_INFO(addr_wq_timer.timer));

	for (i = 0; i < SCTP_STACK_VTAG_HASH_SIZE; i++) {
		LIST_INIT(&SCTP_BASE_INFO(vtag_timewait)[i]);
	}
#if defined(SCTP_PROCESS_LEVEL_LOCKS)
#if defined(_WIN32)
	InitializeConditionVariable(&sctp_it_ctl.iterator_wakeup);
#else
	(void)pthread_cond_init(&sctp_it_ctl.iterator_wakeup, NULL);
#endif
#endif
	sctp_startup_iterator();

#if defined(__FreeBSD__) && !defined(__Userspace__)
#if defined(SCTP_MCORE_INPUT) && defined(SMP)
	sctp_startup_mcore_threads();
#endif
#endif

	sctp_init_vrf_list(SCTP_DEFAULT_VRF);
#if defined(__FreeBSD__) && !defined(__Userspace__) && defined(SCTP_NOT_YET)
	if (ip_register_flow_handler(sctp_netisr_hdlr, IPPROTO_SCTP)) {
		SCTP_PRINTF("***SCTP- Error can't register netisr handler***\n");
	}
#endif
#if defined(_SCTP_NEEDS_CALLOUT_) || defined(_USER_SCTP_NEEDS_CALLOUT_)

	SCTP_TIMERQ_LOCK_INIT();
	TAILQ_INIT(&SCTP_BASE_INFO(callqueue));
#endif
#if defined(__Userspace__)
	mbuf_initialize(NULL);
	atomic_init();
#if defined(INET) || defined(INET6)
	if (start_threads)
		recv_thread_init();
#endif
#endif
}

void
sctp_pcb_finish(void)
{
	struct sctp_vrflist *vrf_bucket;
	struct sctp_vrf *vrf, *nvrf;
	struct sctp_ifn *ifn, *nifn;
	struct sctp_ifa *ifa, *nifa;
	struct sctpvtaghead *chain;
	struct sctp_tagblock *twait_block, *prev_twait_block;
	struct sctp_laddr *wi, *nwi;
	int i;
	struct sctp_iterator *it, *nit;

	if (SCTP_BASE_VAR(sctp_pcb_initialized) == 0) {
		SCTP_PRINTF("%s: race condition on teardown.\n", __func__);
		return;
	}
	SCTP_BASE_VAR(sctp_pcb_initialized) = 0;
#if !(defined(__FreeBSD__) && !defined(__Userspace__))

	SCTP_IPI_ITERATOR_WQ_LOCK();
	sctp_it_ctl.iterator_flags |= SCTP_ITERATOR_MUST_EXIT;
	sctp_wakeup_iterator();
	SCTP_IPI_ITERATOR_WQ_UNLOCK();
#endif
#if defined(__APPLE__) && !defined(__Userspace__)
#if !defined(APPLE_LEOPARD) && !defined(APPLE_SNOWLEOPARD) && !defined(APPLE_LION) && !defined(APPLE_MOUNTAINLION)
	in_pcbinfo_detach(&SCTP_BASE_INFO(sctbinfo));
#endif
	SCTP_IPI_ITERATOR_WQ_LOCK();
	do {
		msleep(&sctp_it_ctl.iterator_flags,
		       sctp_it_ctl.ipi_iterator_wq_mtx,
		       0, "waiting_for_work", 0);
	} while ((sctp_it_ctl.iterator_flags & SCTP_ITERATOR_EXITED) == 0);
	thread_deallocate(sctp_it_ctl.thread_proc);
	SCTP_IPI_ITERATOR_WQ_UNLOCK();
#endif
#if defined(_WIN32) && !defined(__Userspace__)
	if (sctp_it_ctl.iterator_thread_obj != NULL) {
		NTSTATUS status = STATUS_SUCCESS;

		KeSetEvent(&sctp_it_ctl.iterator_wakeup[1], IO_NO_INCREMENT, FALSE);
		status = KeWaitForSingleObject(sctp_it_ctl.iterator_thread_obj,
					       Executive,
					       KernelMode,
					       FALSE,
					       NULL);
		ObDereferenceObject(sctp_it_ctl.iterator_thread_obj);
	}
#endif
#if defined(__Userspace__)
	if (SCTP_BASE_VAR(iterator_thread_started)) {
#if defined(_WIN32)
		WaitForSingleObject(sctp_it_ctl.thread_proc, INFINITE);
		CloseHandle(sctp_it_ctl.thread_proc);
		sctp_it_ctl.thread_proc = NULL;
#else
		pthread_join(sctp_it_ctl.thread_proc, NULL);
		sctp_it_ctl.thread_proc = 0;
#endif
	}
#endif
#if defined(SCTP_PROCESS_LEVEL_LOCKS)
#if defined(_WIN32)
	DeleteConditionVariable(&sctp_it_ctl.iterator_wakeup);
#else
	pthread_cond_destroy(&sctp_it_ctl.iterator_wakeup);
	pthread_mutexattr_destroy(&SCTP_BASE_VAR(mtx_attr));
#endif
#endif

#if defined(__FreeBSD__) && !defined(__Userspace__)
retry:
#endif
	SCTP_IPI_ITERATOR_WQ_LOCK();
#if defined(__FreeBSD__) && !defined(__Userspace__)

	if (sctp_it_ctl.iterator_running != 0 || sctp_it_ctl.cur_it != NULL) {
		SCTP_IPI_ITERATOR_WQ_UNLOCK();
		SCTP_PRINTF("%s: Iterator running while we held the lock. Retry. "
		            "cur_it=%p\n", __func__, sctp_it_ctl.cur_it);
		DELAY(10);
		goto retry;
	}
#endif
	TAILQ_FOREACH_SAFE(it, &sctp_it_ctl.iteratorhead, sctp_nxt_itr, nit) {
#if defined(__FreeBSD__) && !defined(__Userspace__)
		if (it->vn != curvnet) {
			continue;
		}
#endif
		TAILQ_REMOVE(&sctp_it_ctl.iteratorhead, it, sctp_nxt_itr);
		if (it->function_atend != NULL) {
			(*it->function_atend) (it->pointer, it->val);
		}
		SCTP_FREE(it,SCTP_M_ITER);
	}
	SCTP_IPI_ITERATOR_WQ_UNLOCK();
#if defined(__FreeBSD__) && !defined(__Userspace__)
	SCTP_ITERATOR_LOCK();
	if ((sctp_it_ctl.cur_it) &&
	    (sctp_it_ctl.cur_it->vn == curvnet)) {
		sctp_it_ctl.iterator_flags |= SCTP_ITERATOR_STOP_CUR_IT;
	}
	SCTP_ITERATOR_UNLOCK();
#endif
#if !(defined(__FreeBSD__) && !defined(__Userspace__))
	SCTP_IPI_ITERATOR_WQ_DESTROY();
	SCTP_ITERATOR_LOCK_DESTROY();
#endif
	SCTP_OS_TIMER_STOP_DRAIN(&SCTP_BASE_INFO(addr_wq_timer.timer));
	SCTP_WQ_ADDR_LOCK();
	LIST_FOREACH_SAFE(wi, &SCTP_BASE_INFO(addr_wq), sctp_nxt_addr, nwi) {
		LIST_REMOVE(wi, sctp_nxt_addr);
		SCTP_DECR_LADDR_COUNT();
		if (wi->action == SCTP_DEL_IP_ADDRESS) {
			SCTP_FREE(wi->ifa, SCTP_M_IFA);
		}
		SCTP_ZONE_FREE(SCTP_BASE_INFO(ipi_zone_laddr), wi);
	}
	SCTP_WQ_ADDR_UNLOCK();

	SCTP_IPI_ADDR_WLOCK();
	vrf_bucket = &SCTP_BASE_INFO(sctp_vrfhash)[(SCTP_DEFAULT_VRFID & SCTP_BASE_INFO(hashvrfmark))];
	LIST_FOREACH_SAFE(vrf, vrf_bucket, next_vrf, nvrf) {
		LIST_FOREACH_SAFE(ifn, &vrf->ifnlist, next_ifn, nifn) {
			LIST_FOREACH_SAFE(ifa, &ifn->ifalist, next_ifa, nifa) {

				LIST_REMOVE(ifa, next_bucket);
				LIST_REMOVE(ifa, next_ifa);
				SCTP_FREE(ifa, SCTP_M_IFA);
			}

			LIST_REMOVE(ifn, next_bucket);
			LIST_REMOVE(ifn, next_ifn);
			SCTP_FREE(ifn, SCTP_M_IFN);
		}
		SCTP_HASH_FREE(vrf->vrf_addr_hash, vrf->vrf_addr_hashmark);

		LIST_REMOVE(vrf, next_vrf);
		SCTP_FREE(vrf, SCTP_M_VRF);
	}
	SCTP_IPI_ADDR_WUNLOCK();

	SCTP_HASH_FREE(SCTP_BASE_INFO(sctp_vrfhash), SCTP_BASE_INFO(hashvrfmark));
	SCTP_HASH_FREE(SCTP_BASE_INFO(vrf_ifn_hash), SCTP_BASE_INFO(vrf_ifn_hashmark));

	for (i = 0; i < SCTP_STACK_VTAG_HASH_SIZE; i++) {
		chain = &SCTP_BASE_INFO(vtag_timewait)[i];
		if (!LIST_EMPTY(chain)) {
			prev_twait_block = NULL;
			LIST_FOREACH(twait_block, chain, sctp_nxt_tagblock) {
				if (prev_twait_block) {
					SCTP_FREE(prev_twait_block, SCTP_M_TIMW);
				}
				prev_twait_block = twait_block;
			}
			SCTP_FREE(prev_twait_block, SCTP_M_TIMW);
		}
	}

#if defined(__APPLE__) && !defined(__Userspace__)
	SCTP_TIMERQ_LOCK_DESTROY();
#endif
#ifdef SCTP_PACKET_LOGGING
	SCTP_IP_PKTLOG_DESTROY();
#endif
	SCTP_IPI_ADDR_DESTROY();
#if defined(__APPLE__) && !defined(__Userspace__)
	SCTP_IPI_COUNT_DESTROY();
#endif
	SCTP_STATLOG_DESTROY();
	SCTP_INP_INFO_LOCK_DESTROY();

	SCTP_WQ_ADDR_DESTROY();

#if defined(__APPLE__) && !defined(__Userspace__)
#if defined(APPLE_LEOPARD) || defined(APPLE_SNOWLEOPARD) || defined(APPLE_LION) || defined(APPLE_MOUNTAINLION)
	lck_grp_attr_free(SCTP_BASE_INFO(sctbinfo).mtx_grp_attr);
	lck_grp_free(SCTP_BASE_INFO(sctbinfo).mtx_grp);
	lck_attr_free(SCTP_BASE_INFO(sctbinfo).mtx_attr);
#else
	lck_grp_attr_free(SCTP_BASE_INFO(sctbinfo).ipi_lock_grp_attr);
	lck_grp_free(SCTP_BASE_INFO(sctbinfo).ipi_lock_grp);
	lck_attr_free(SCTP_BASE_INFO(sctbinfo).ipi_lock_attr);
#endif
#endif
#if defined(__Userspace__)
	SCTP_TIMERQ_LOCK_DESTROY();
	SCTP_ZONE_DESTROY(zone_mbuf);
	SCTP_ZONE_DESTROY(zone_clust);
	SCTP_ZONE_DESTROY(zone_ext_refcnt);
#endif

	if (SCTP_BASE_INFO(sctp_asochash) != NULL)
		SCTP_HASH_FREE(SCTP_BASE_INFO(sctp_asochash), SCTP_BASE_INFO(hashasocmark));
	if (SCTP_BASE_INFO(sctp_ephash) != NULL)
		SCTP_HASH_FREE(SCTP_BASE_INFO(sctp_ephash), SCTP_BASE_INFO(hashmark));
	if (SCTP_BASE_INFO(sctp_tcpephash) != NULL)
		SCTP_HASH_FREE(SCTP_BASE_INFO(sctp_tcpephash), SCTP_BASE_INFO(hashtcpmark));

#if defined(_WIN32) || defined(__FreeBSD__) || defined(__Userspace__)
	SCTP_ZONE_DESTROY(SCTP_BASE_INFO(ipi_zone_ep));
	SCTP_ZONE_DESTROY(SCTP_BASE_INFO(ipi_zone_asoc));
	SCTP_ZONE_DESTROY(SCTP_BASE_INFO(ipi_zone_laddr));
	SCTP_ZONE_DESTROY(SCTP_BASE_INFO(ipi_zone_net));
	SCTP_ZONE_DESTROY(SCTP_BASE_INFO(ipi_zone_chunk));
	SCTP_ZONE_DESTROY(SCTP_BASE_INFO(ipi_zone_readq));
	SCTP_ZONE_DESTROY(SCTP_BASE_INFO(ipi_zone_strmoq));
	SCTP_ZONE_DESTROY(SCTP_BASE_INFO(ipi_zone_asconf));
	SCTP_ZONE_DESTROY(SCTP_BASE_INFO(ipi_zone_asconf_ack));
#endif
#if defined(__FreeBSD__) && !defined(__Userspace__)
#if defined(SMP) && defined(SCTP_USE_PERCPU_STAT)
	SCTP_FREE(SCTP_BASE_STATS, SCTP_M_MCORE);
#endif
#endif
}

int
sctp_load_addresses_from_init(struct sctp_tcb *stcb, struct mbuf *m,
                              int offset, int limit,
                              struct sockaddr *src, struct sockaddr *dst,
                              struct sockaddr *altsa, uint16_t port)
{

	struct sctp_inpcb *inp;
	struct sctp_nets *net, *nnet, *net_tmp;
	struct sctp_paramhdr *phdr, param_buf;
	struct sctp_tcb *stcb_tmp;
	uint16_t ptype, plen;
	struct sockaddr *sa;
	uint8_t random_store[SCTP_PARAM_BUFFER_SIZE];
	struct sctp_auth_random *p_random = NULL;
	uint16_t random_len = 0;
	uint8_t hmacs_store[SCTP_PARAM_BUFFER_SIZE];
	struct sctp_auth_hmac_algo *hmacs = NULL;
	uint16_t hmacs_len = 0;
	uint8_t saw_asconf = 0;
	uint8_t saw_asconf_ack = 0;
	uint8_t chunks_store[SCTP_PARAM_BUFFER_SIZE];
	struct sctp_auth_chunk_list *chunks = NULL;
	uint16_t num_chunks = 0;
	sctp_key_t *new_key;
	uint32_t keylen;
	int got_random = 0, got_hmacs = 0, got_chklist = 0;
	uint8_t peer_supports_ecn;
	uint8_t peer_supports_prsctp;
	uint8_t peer_supports_auth;
	uint8_t peer_supports_asconf;
	uint8_t peer_supports_asconf_ack;
	uint8_t peer_supports_reconfig;
	uint8_t peer_supports_nrsack;
	uint8_t peer_supports_pktdrop;
	uint8_t peer_supports_idata;
#ifdef INET
	struct sockaddr_in sin;
#endif
#ifdef INET6
	struct sockaddr_in6 sin6;
#endif

#ifdef INET
	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
#ifdef HAVE_SIN_LEN
	sin.sin_len = sizeof(sin);
#endif
	sin.sin_port = stcb->rport;
#endif
#ifdef INET6
	memset(&sin6, 0, sizeof(sin6));
	sin6.sin6_family = AF_INET6;
#ifdef HAVE_SIN6_LEN
	sin6.sin6_len = sizeof(struct sockaddr_in6);
#endif
	sin6.sin6_port = stcb->rport;
#endif
	if (altsa) {
		sa = altsa;
	} else {
		sa = src;
	}
	peer_supports_idata = 0;
	peer_supports_ecn = 0;
	peer_supports_prsctp = 0;
	peer_supports_auth = 0;
	peer_supports_asconf = 0;
	peer_supports_asconf_ack = 0;
	peer_supports_reconfig = 0;
	peer_supports_nrsack = 0;
	peer_supports_pktdrop = 0;
	TAILQ_FOREACH(net, &stcb->asoc.nets, sctp_next) {

		net->dest_state |= SCTP_ADDR_NOT_IN_ASSOC;
	}

	inp = stcb->sctp_ep;
	atomic_add_int(&stcb->asoc.refcnt, 1);
	stcb_tmp = sctp_findassociation_ep_addr(&inp, sa, &net_tmp, dst, stcb);
	atomic_add_int(&stcb->asoc.refcnt, -1);

	if ((stcb_tmp == NULL && inp == stcb->sctp_ep) || inp == NULL) {

		switch (sa->sa_family) {
#ifdef INET
		case AF_INET:
			if (stcb->asoc.scope.ipv4_addr_legal) {
				if (sctp_add_remote_addr(stcb, sa, NULL, port, SCTP_DONOT_SETSCOPE, SCTP_LOAD_ADDR_2)) {
					return (-1);
				}
			}
			break;
#endif
#ifdef INET6
		case AF_INET6:
			if (stcb->asoc.scope.ipv6_addr_legal) {
				if (sctp_add_remote_addr(stcb, sa, NULL, port, SCTP_DONOT_SETSCOPE, SCTP_LOAD_ADDR_3)) {
					return (-2);
				}
			}
			break;
#endif
#if defined(__Userspace__)
		case AF_CONN:
			if (stcb->asoc.scope.conn_addr_legal) {
				if (sctp_add_remote_addr(stcb, sa, NULL, port, SCTP_DONOT_SETSCOPE, SCTP_LOAD_ADDR_3)) {
					return (-2);
				}
			}
			break;
#endif
		default:
			break;
		}
	} else {
		if (net_tmp != NULL && stcb_tmp == stcb) {
			net_tmp->dest_state &= ~SCTP_ADDR_NOT_IN_ASSOC;
		} else if (stcb_tmp != stcb) {

			if (stcb_tmp)
				SCTP_TCB_UNLOCK(stcb_tmp);
			return (-3);
		}
	}
	if (stcb->asoc.state == 0) {

		return (-4);
	}

	phdr = sctp_get_next_param(m, offset, &param_buf, sizeof(param_buf));
	while (phdr) {
		ptype = ntohs(phdr->param_type);
		plen = ntohs(phdr->param_length);

		if (offset + plen > limit) {
			break;
		}
		if (plen < sizeof(struct sctp_paramhdr)) {
			break;
		}
#ifdef INET
		if (ptype == SCTP_IPV4_ADDRESS) {
			if (stcb->asoc.scope.ipv4_addr_legal) {
				struct sctp_ipv4addr_param *p4, p4_buf;

				phdr = sctp_get_next_param(m, offset,
							   (struct sctp_paramhdr *)&p4_buf,
							   sizeof(p4_buf));
				if (plen != sizeof(struct sctp_ipv4addr_param) ||
				    phdr == NULL) {
					return (-5);
				}
				p4 = (struct sctp_ipv4addr_param *)phdr;
				sin.sin_addr.s_addr = p4->addr;
				if (IN_MULTICAST(ntohl(sin.sin_addr.s_addr))) {

					goto next_param;
				}
				if ((sin.sin_addr.s_addr == INADDR_BROADCAST) ||
				    (sin.sin_addr.s_addr == INADDR_ANY)) {
					goto next_param;
				}
				sa = (struct sockaddr *)&sin;
				inp = stcb->sctp_ep;
				atomic_add_int(&stcb->asoc.refcnt, 1);
				stcb_tmp = sctp_findassociation_ep_addr(&inp, sa, &net,
									dst, stcb);
				atomic_add_int(&stcb->asoc.refcnt, -1);

				if ((stcb_tmp == NULL && inp == stcb->sctp_ep) ||
				    inp == NULL) {

				add_it_now:
					if (stcb->asoc.state == 0) {

						return (-7);
					}
					if (sctp_add_remote_addr(stcb, sa, NULL, port, SCTP_DONOT_SETSCOPE, SCTP_LOAD_ADDR_4)) {
						return (-8);
					}
				} else if (stcb_tmp == stcb) {
					if (stcb->asoc.state == 0) {

						return (-10);
					}
					if (net != NULL) {

						net->dest_state &=
							~SCTP_ADDR_NOT_IN_ASSOC;
					}
				} else {

					if (stcb_tmp) {
						if (SCTP_GET_STATE(stcb_tmp) == SCTP_STATE_COOKIE_WAIT) {
							struct mbuf *op_err;
							char msg[SCTP_DIAG_INFO_LEN];

							SCTP_SNPRINTF(msg, sizeof(msg),
							              "%s:%d at %s", __FILE__, __LINE__, __func__);
							op_err = sctp_generate_cause(SCTP_BASE_SYSCTL(sctp_diag_info_code),
							         msg);
							sctp_abort_an_association(stcb_tmp->sctp_ep,
							                          stcb_tmp, op_err, false,
							                          SCTP_SO_NOT_LOCKED);
							goto add_it_now;
						}
						SCTP_TCB_UNLOCK(stcb_tmp);
					}

					if (stcb->asoc.state == 0) {

						return (-12);
					}
					return (-13);
				}
			}
		} else
#endif
#ifdef INET6
		if (ptype == SCTP_IPV6_ADDRESS) {
			if (stcb->asoc.scope.ipv6_addr_legal) {

				struct sctp_ipv6addr_param *p6, p6_buf;

				phdr = sctp_get_next_param(m, offset,
							   (struct sctp_paramhdr *)&p6_buf,
							   sizeof(p6_buf));
				if (plen != sizeof(struct sctp_ipv6addr_param) ||
				    phdr == NULL) {
					return (-14);
				}
				p6 = (struct sctp_ipv6addr_param *)phdr;
				memcpy((caddr_t)&sin6.sin6_addr, p6->addr,
				       sizeof(p6->addr));
				if (IN6_IS_ADDR_MULTICAST(&sin6.sin6_addr)) {

					goto next_param;
				}
				if (IN6_IS_ADDR_LINKLOCAL(&sin6.sin6_addr)) {

					goto next_param;
				}
				sa = (struct sockaddr *)&sin6;
				inp = stcb->sctp_ep;
				atomic_add_int(&stcb->asoc.refcnt, 1);
				stcb_tmp = sctp_findassociation_ep_addr(&inp, sa, &net,
									dst, stcb);
				atomic_add_int(&stcb->asoc.refcnt, -1);
				if (stcb_tmp == NULL &&
				    (inp == stcb->sctp_ep || inp == NULL)) {

				add_it_now6:
					if (stcb->asoc.state == 0) {

						return (-16);
					}

					if (sctp_add_remote_addr(stcb, sa, NULL, port, SCTP_DONOT_SETSCOPE, SCTP_LOAD_ADDR_5)) {
						return (-17);
					}
				} else if (stcb_tmp == stcb) {

					if (stcb->asoc.state == 0) {

						return (-19);
					}
					if (net != NULL) {

						net->dest_state &=
							~SCTP_ADDR_NOT_IN_ASSOC;
					}
				} else {

					if (stcb_tmp) {
						if (SCTP_GET_STATE(stcb_tmp) == SCTP_STATE_COOKIE_WAIT) {
							struct mbuf *op_err;
							char msg[SCTP_DIAG_INFO_LEN];

							SCTP_SNPRINTF(msg, sizeof(msg),
							              "%s:%d at %s", __FILE__, __LINE__, __func__);
							op_err = sctp_generate_cause(SCTP_BASE_SYSCTL(sctp_diag_info_code),
							         msg);
							sctp_abort_an_association(stcb_tmp->sctp_ep,
							                          stcb_tmp, op_err, false,
							                          SCTP_SO_NOT_LOCKED);
							goto add_it_now6;
						}
						SCTP_TCB_UNLOCK(stcb_tmp);
					}
					if (stcb->asoc.state == 0) {

						return (-21);
					}
					return (-22);
				}
			}
		} else
#endif
		if (ptype == SCTP_ECN_CAPABLE) {
			peer_supports_ecn = 1;
		} else if (ptype == SCTP_ULP_ADAPTATION) {
			if (stcb->asoc.state != SCTP_STATE_OPEN) {
				struct sctp_adaptation_layer_indication ai, *aip;

				phdr = sctp_get_next_param(m, offset,
							   (struct sctp_paramhdr *)&ai, sizeof(ai));
				aip = (struct sctp_adaptation_layer_indication *)phdr;
				if (aip) {
					stcb->asoc.peers_adaptation = ntohl(aip->indication);
					stcb->asoc.adaptation_needed = 1;
				}
			}
		} else if (ptype == SCTP_SET_PRIM_ADDR) {
			struct sctp_asconf_addr_param lstore, *fee;
			int lptype;
			struct sockaddr *lsa = NULL;
#ifdef INET
			struct sctp_asconf_addrv4_param *fii;
#endif

			if (stcb->asoc.asconf_supported == 0) {
				return (-100);
			}
			if (plen > sizeof(lstore)) {
				return (-23);
			}
			if (plen < sizeof(struct sctp_asconf_addrv4_param)) {
				return (-101);
			}
			phdr = sctp_get_next_param(m, offset,
						   (struct sctp_paramhdr *)&lstore,
						   plen);
			if (phdr == NULL) {
				return (-24);
			}
			fee = (struct sctp_asconf_addr_param *)phdr;
			lptype = ntohs(fee->addrp.ph.param_type);
			switch (lptype) {
#ifdef INET
			case SCTP_IPV4_ADDRESS:
				if (plen !=
				    sizeof(struct sctp_asconf_addrv4_param)) {
					SCTP_PRINTF("Sizeof setprim in init/init ack not %d but %d - ignored\n",
						    (int)sizeof(struct sctp_asconf_addrv4_param),
						    plen);
				} else {
					fii = (struct sctp_asconf_addrv4_param *)fee;
					sin.sin_addr.s_addr = fii->addrp.addr;
					lsa = (struct sockaddr *)&sin;
				}
				break;
#endif
#ifdef INET6
			case SCTP_IPV6_ADDRESS:
				if (plen !=
				    sizeof(struct sctp_asconf_addr_param)) {
					SCTP_PRINTF("Sizeof setprim (v6) in init/init ack not %d but %d - ignored\n",
						    (int)sizeof(struct sctp_asconf_addr_param),
						    plen);
				} else {
					memcpy(sin6.sin6_addr.s6_addr,
					       fee->addrp.addr,
					       sizeof(fee->addrp.addr));
					lsa = (struct sockaddr *)&sin6;
				}
				break;
#endif
			default:
				break;
			}
			if (lsa) {
				(void)sctp_set_primary_addr(stcb, sa, NULL);
			}
		} else if (ptype == SCTP_HAS_NAT_SUPPORT) {
			stcb->asoc.peer_supports_nat = 1;
		} else if (ptype == SCTP_PRSCTP_SUPPORTED) {

			peer_supports_prsctp = 1;
		} else if (ptype == SCTP_SUPPORTED_CHUNK_EXT) {

			struct sctp_supported_chunk_types_param *pr_supported;
			uint8_t local_store[SCTP_PARAM_BUFFER_SIZE];
			int num_ent, i;

			if (plen > sizeof(local_store)) {
				return (-35);
			}
			phdr = sctp_get_next_param(m, offset,
						   (struct sctp_paramhdr *)&local_store, plen);
			if (phdr == NULL) {
				return (-25);
			}
			pr_supported = (struct sctp_supported_chunk_types_param *)phdr;
			num_ent = plen - sizeof(struct sctp_paramhdr);
			for (i = 0; i < num_ent; i++) {
				switch (pr_supported->chunk_types[i]) {
				case SCTP_ASCONF:
					peer_supports_asconf = 1;
					break;
				case SCTP_ASCONF_ACK:
					peer_supports_asconf_ack = 1;
					break;
				case SCTP_FORWARD_CUM_TSN:
					peer_supports_prsctp = 1;
					break;
				case SCTP_PACKET_DROPPED:
					peer_supports_pktdrop = 1;
					break;
				case SCTP_NR_SELECTIVE_ACK:
					peer_supports_nrsack = 1;
					break;
				case SCTP_STREAM_RESET:
					peer_supports_reconfig = 1;
					break;
				case SCTP_AUTHENTICATION:
					peer_supports_auth = 1;
					break;
				case SCTP_IDATA:
					peer_supports_idata = 1;
					break;
				default:

					break;
				}
			}
		} else if (ptype == SCTP_RANDOM) {
			if (plen > sizeof(random_store))
				break;
			if (got_random) {

				goto next_param;
			}
			phdr = sctp_get_next_param(m, offset,
						   (struct sctp_paramhdr *)random_store,
						   plen);
			if (phdr == NULL)
				return (-26);
			p_random = (struct sctp_auth_random *)phdr;
			random_len = plen - sizeof(*p_random);

			if (random_len != SCTP_AUTH_RANDOM_SIZE_REQUIRED) {
				SCTPDBG(SCTP_DEBUG_AUTH1, "SCTP: invalid RANDOM len\n");
				return (-27);
			}
			got_random = 1;
		} else if (ptype == SCTP_HMAC_LIST) {
			uint16_t num_hmacs;
			uint16_t i;

			if (plen > sizeof(hmacs_store))
				break;
			if (got_hmacs) {

				goto next_param;
			}
			phdr = sctp_get_next_param(m, offset,
						   (struct sctp_paramhdr *)hmacs_store,
						   plen);
			if (phdr == NULL)
				return (-28);
			hmacs = (struct sctp_auth_hmac_algo *)phdr;
			hmacs_len = plen - sizeof(*hmacs);
			num_hmacs = hmacs_len / sizeof(hmacs->hmac_ids[0]);

			if (sctp_verify_hmac_param(hmacs, num_hmacs)) {
				return (-29);
			}
			if (stcb->asoc.peer_hmacs != NULL)
				sctp_free_hmaclist(stcb->asoc.peer_hmacs);
			stcb->asoc.peer_hmacs = sctp_alloc_hmaclist(num_hmacs);
			if (stcb->asoc.peer_hmacs != NULL) {
				for (i = 0; i < num_hmacs; i++) {
					(void)sctp_auth_add_hmacid(stcb->asoc.peer_hmacs,
								   ntohs(hmacs->hmac_ids[i]));
				}
			}
			got_hmacs = 1;
		} else if (ptype == SCTP_CHUNK_LIST) {
			int i;

			if (plen > sizeof(chunks_store))
				break;
			if (got_chklist) {

				goto next_param;
			}
			phdr = sctp_get_next_param(m, offset,
						   (struct sctp_paramhdr *)chunks_store,
						   plen);
			if (phdr == NULL)
				return (-30);
			chunks = (struct sctp_auth_chunk_list *)phdr;
			num_chunks = plen - sizeof(*chunks);
			if (stcb->asoc.peer_auth_chunks != NULL)
				sctp_clear_chunklist(stcb->asoc.peer_auth_chunks);
			else
				stcb->asoc.peer_auth_chunks = sctp_alloc_chunklist();
			for (i = 0; i < num_chunks; i++) {
				(void)sctp_auth_add_chunk(chunks->chunk_types[i],
							  stcb->asoc.peer_auth_chunks);

				if (chunks->chunk_types[i] == SCTP_ASCONF)
					saw_asconf = 1;
				if (chunks->chunk_types[i] == SCTP_ASCONF_ACK)
					saw_asconf_ack = 1;
			}
			got_chklist = 1;
		} else if ((ptype == SCTP_HEARTBEAT_INFO) ||
			   (ptype == SCTP_STATE_COOKIE) ||
			   (ptype == SCTP_UNRECOG_PARAM) ||
			   (ptype == SCTP_COOKIE_PRESERVE) ||
			   (ptype == SCTP_SUPPORTED_ADDRTYPE) ||
			   (ptype == SCTP_ADD_IP_ADDRESS) ||
			   (ptype == SCTP_DEL_IP_ADDRESS) ||
			   (ptype == SCTP_ERROR_CAUSE_IND) ||
			   (ptype == SCTP_SUCCESS_REPORT)) {

		} else {
			if ((ptype & 0x8000) == 0x0000) {

				break;
			}
		}

	next_param:
		offset += SCTP_SIZE32(plen);
		if (offset >= limit) {
			break;
		}
		phdr = sctp_get_next_param(m, offset, &param_buf,
		                           sizeof(param_buf));
	}

	TAILQ_FOREACH_SAFE(net, &stcb->asoc.nets, sctp_next, nnet) {
		if ((net->dest_state & SCTP_ADDR_NOT_IN_ASSOC) ==
		    SCTP_ADDR_NOT_IN_ASSOC) {

			stcb->asoc.numnets--;
			TAILQ_REMOVE(&stcb->asoc.nets, net, sctp_next);
			if (net == stcb->asoc.alternate) {
				sctp_free_remote_addr(stcb->asoc.alternate);
				stcb->asoc.alternate = NULL;
			}
			if (net == stcb->asoc.primary_destination) {
				stcb->asoc.primary_destination = NULL;
				sctp_select_primary_destination(stcb);
			}
			sctp_free_remote_addr(net);
		}
	}
	if ((stcb->asoc.ecn_supported == 1) &&
	    (peer_supports_ecn == 0)) {
		stcb->asoc.ecn_supported = 0;
	}
	if ((stcb->asoc.prsctp_supported == 1) &&
	    (peer_supports_prsctp == 0)) {
		stcb->asoc.prsctp_supported = 0;
	}
	if ((stcb->asoc.auth_supported == 1) &&
	    ((peer_supports_auth == 0) ||
	     (got_random == 0) || (got_hmacs == 0))) {
		stcb->asoc.auth_supported = 0;
	}
	if ((stcb->asoc.asconf_supported == 1) &&
	    ((peer_supports_asconf == 0) || (peer_supports_asconf_ack == 0) ||
	     (stcb->asoc.auth_supported == 0) ||
	     (saw_asconf == 0) || (saw_asconf_ack == 0))) {
		stcb->asoc.asconf_supported = 0;
	}
	if ((stcb->asoc.reconfig_supported == 1) &&
	    (peer_supports_reconfig == 0)) {
		stcb->asoc.reconfig_supported = 0;
	}
	if ((stcb->asoc.idata_supported == 1) &&
	    (peer_supports_idata == 0)) {
		stcb->asoc.idata_supported = 0;
	}
	if ((stcb->asoc.nrsack_supported == 1) &&
	    (peer_supports_nrsack == 0)) {
		stcb->asoc.nrsack_supported = 0;
	}
	if ((stcb->asoc.pktdrop_supported == 1) &&
	    (peer_supports_pktdrop == 0)){
		stcb->asoc.pktdrop_supported = 0;
	}

	if ((peer_supports_auth == 0) && (got_chklist == 1)) {

		return (-31);
	}
	if ((peer_supports_asconf == 1) && (peer_supports_auth == 0)) {

		return (-32);
	} else if ((peer_supports_asconf == 1) &&
	           (peer_supports_auth == 1) &&
		   ((saw_asconf == 0) || (saw_asconf_ack == 0))) {
		return (-33);
	}

	keylen = sizeof(*p_random) + random_len + sizeof(*hmacs) + hmacs_len;
	if (chunks != NULL) {
		keylen += sizeof(*chunks) + num_chunks;
	}
	new_key = sctp_alloc_key(keylen);
	if (new_key != NULL) {

		if (p_random != NULL) {
			keylen = sizeof(*p_random) + random_len;
			memcpy(new_key->key, p_random, keylen);
		} else {
			keylen = 0;
		}

		if (chunks != NULL) {
			memcpy(new_key->key + keylen, chunks,
			       sizeof(*chunks) + num_chunks);
			keylen += sizeof(*chunks) + num_chunks;
		}

		if (hmacs != NULL) {
			memcpy(new_key->key + keylen, hmacs,
			       sizeof(*hmacs) + hmacs_len);
		}
	} else {

		return (-34);
	}
	if (stcb->asoc.authinfo.peer_random != NULL)
		sctp_free_key(stcb->asoc.authinfo.peer_random);
	stcb->asoc.authinfo.peer_random = new_key;
	sctp_clear_cachedkeys(stcb, stcb->asoc.authinfo.assoc_keyid);
	sctp_clear_cachedkeys(stcb, stcb->asoc.authinfo.recv_keyid);

	return (0);
}

int
sctp_set_primary_addr(struct sctp_tcb *stcb, struct sockaddr *sa,
		      struct sctp_nets *net)
{

	if (net == NULL && sa)
		net = sctp_findnet(stcb, sa);

	if (net == NULL) {

		return (-1);
	} else {

		if (net->dest_state & SCTP_ADDR_UNCONFIRMED) {

			net->dest_state |= SCTP_ADDR_REQ_PRIMARY;
			return (0);
		}
		stcb->asoc.primary_destination = net;
		if (!(net->dest_state & SCTP_ADDR_PF) && (stcb->asoc.alternate)) {
			sctp_free_remote_addr(stcb->asoc.alternate);
			stcb->asoc.alternate = NULL;
		}
		net = TAILQ_FIRST(&stcb->asoc.nets);
		if (net != stcb->asoc.primary_destination) {

			TAILQ_REMOVE(&stcb->asoc.nets, stcb->asoc.primary_destination, sctp_next);
			TAILQ_INSERT_HEAD(&stcb->asoc.nets, stcb->asoc.primary_destination, sctp_next);
		}
		return (0);
	}
}

bool
sctp_is_vtag_good(uint32_t tag, uint16_t lport, uint16_t rport, struct timeval *now)
{
	struct sctpasochead *head;
	struct sctp_tcb *stcb;

	SCTP_INP_INFO_LOCK_ASSERT();

	head = &SCTP_BASE_INFO(sctp_asochash)[SCTP_PCBHASH_ASOC(tag, SCTP_BASE_INFO(hashasocmark))];
	LIST_FOREACH(stcb, head, sctp_asocs) {

		if (stcb->sctp_ep->sctp_flags & SCTP_PCB_FLAGS_SOCKET_ALLGONE) {
			continue;
		}
		if (stcb->asoc.my_vtag == tag) {

			if (stcb->rport != rport) {
				continue;
			}
			if (stcb->sctp_ep->sctp_lport != lport) {
				continue;
			}

			return (false);
		}
	}
	return (!sctp_is_in_timewait(tag, lport, rport, (uint32_t)now->tv_sec));
}

static void
sctp_drain_mbufs(struct sctp_tcb *stcb)
{

	struct sctp_association *asoc;
	struct sctp_tmit_chunk *chk, *nchk;
	uint32_t cumulative_tsn_p1;
	struct sctp_queued_to_read *control, *ncontrol;
	int cnt, strmat;
	uint32_t gap, i;
	int fnd = 0;

	asoc = &stcb->asoc;
	if (asoc->cumulative_tsn == asoc->highest_tsn_inside_map) {

		return;
	}
	SCTP_STAT_INCR(sctps_protocol_drains_done);
	cumulative_tsn_p1 = asoc->cumulative_tsn + 1;
	cnt = 0;

	for (strmat = 0; strmat < asoc->streamincnt; strmat++) {
		TAILQ_FOREACH_SAFE(control, &asoc->strmin[strmat].inqueue, next_instrm, ncontrol) {
#ifdef INVARIANTS
			if (control->on_strm_q != SCTP_ON_ORDERED) {
				panic("Huh control: %p on_q: %d -- not ordered?",
				      control, control->on_strm_q);
			}
#endif
			if (SCTP_TSN_GT(control->sinfo_tsn, cumulative_tsn_p1)) {

				cnt++;
				SCTP_CALC_TSN_TO_GAP(gap, control->sinfo_tsn, asoc->mapping_array_base_tsn);
				KASSERT(control->length > 0, ("control has zero length"));
				if (asoc->size_on_all_streams >= control->length) {
					asoc->size_on_all_streams -= control->length;
				} else {
#ifdef INVARIANTS
					panic("size_on_all_streams = %u smaller than control length %u", asoc->size_on_all_streams, control->length);
#else
					asoc->size_on_all_streams = 0;
#endif
				}
				sctp_ucount_decr(asoc->cnt_on_all_streams);
				SCTP_UNSET_TSN_PRESENT(asoc->mapping_array, gap);
				if (control->on_read_q) {
					TAILQ_REMOVE(&stcb->sctp_ep->read_queue, control, next);
					control->on_read_q = 0;
				}
				TAILQ_REMOVE(&asoc->strmin[strmat].inqueue, control, next_instrm);
				control->on_strm_q = 0;
				if (control->data) {
					sctp_m_freem(control->data);
					control->data = NULL;
				}
				sctp_free_remote_addr(control->whoFrom);

				TAILQ_FOREACH_SAFE(chk, &control->reasm, sctp_next, nchk) {
					cnt++;
					SCTP_CALC_TSN_TO_GAP(gap, chk->rec.data.tsn, asoc->mapping_array_base_tsn);
					KASSERT(chk->send_size > 0, ("chunk has zero length"));
					if (asoc->size_on_reasm_queue >= chk->send_size) {
						asoc->size_on_reasm_queue -= chk->send_size;
					} else {
#ifdef INVARIANTS
						panic("size_on_reasm_queue = %u smaller than chunk length %u", asoc->size_on_reasm_queue, chk->send_size);
#else
						asoc->size_on_reasm_queue = 0;
#endif
					}
					sctp_ucount_decr(asoc->cnt_on_reasm_queue);
					SCTP_UNSET_TSN_PRESENT(asoc->mapping_array, gap);
					TAILQ_REMOVE(&control->reasm, chk, sctp_next);
					if (chk->data) {
						sctp_m_freem(chk->data);
						chk->data = NULL;
					}
					sctp_free_a_chunk(stcb, chk, SCTP_SO_NOT_LOCKED);
				}
				sctp_free_a_readq(stcb, control);
			}
		}
		TAILQ_FOREACH_SAFE(control, &asoc->strmin[strmat].uno_inqueue, next_instrm, ncontrol) {
#ifdef INVARIANTS
			if (control->on_strm_q != SCTP_ON_UNORDERED) {
				panic("Huh control: %p on_q: %d -- not unordered?",
				      control, control->on_strm_q);
			}
#endif
			if (SCTP_TSN_GT(control->sinfo_tsn, cumulative_tsn_p1)) {

				cnt++;
				SCTP_CALC_TSN_TO_GAP(gap, control->sinfo_tsn, asoc->mapping_array_base_tsn);
				KASSERT(control->length > 0, ("control has zero length"));
				if (asoc->size_on_all_streams >= control->length) {
					asoc->size_on_all_streams -= control->length;
				} else {
#ifdef INVARIANTS
					panic("size_on_all_streams = %u smaller than control length %u", asoc->size_on_all_streams, control->length);
#else
					asoc->size_on_all_streams = 0;
#endif
				}
				sctp_ucount_decr(asoc->cnt_on_all_streams);
				SCTP_UNSET_TSN_PRESENT(asoc->mapping_array, gap);
				if (control->on_read_q) {
					TAILQ_REMOVE(&stcb->sctp_ep->read_queue, control, next);
					control->on_read_q = 0;
				}
				TAILQ_REMOVE(&asoc->strmin[strmat].uno_inqueue, control, next_instrm);
				control->on_strm_q = 0;
				if (control->data) {
					sctp_m_freem(control->data);
					control->data = NULL;
				}
				sctp_free_remote_addr(control->whoFrom);

				TAILQ_FOREACH_SAFE(chk, &control->reasm, sctp_next, nchk) {
					cnt++;
					SCTP_CALC_TSN_TO_GAP(gap, chk->rec.data.tsn, asoc->mapping_array_base_tsn);
					KASSERT(chk->send_size > 0, ("chunk has zero length"));
					if (asoc->size_on_reasm_queue >= chk->send_size) {
						asoc->size_on_reasm_queue -= chk->send_size;
					} else {
#ifdef INVARIANTS
						panic("size_on_reasm_queue = %u smaller than chunk length %u", asoc->size_on_reasm_queue, chk->send_size);
#else
						asoc->size_on_reasm_queue = 0;
#endif
					}
					sctp_ucount_decr(asoc->cnt_on_reasm_queue);
					SCTP_UNSET_TSN_PRESENT(asoc->mapping_array, gap);
					TAILQ_REMOVE(&control->reasm, chk, sctp_next);
					if (chk->data) {
						sctp_m_freem(chk->data);
						chk->data = NULL;
					}
					sctp_free_a_chunk(stcb, chk, SCTP_SO_NOT_LOCKED);
				}
				sctp_free_a_readq(stcb, control);
			}
		}
	}
	if (cnt) {

		for (i = asoc->highest_tsn_inside_map; SCTP_TSN_GE(i, asoc->mapping_array_base_tsn); i--) {
			SCTP_CALC_TSN_TO_GAP(gap, i, asoc->mapping_array_base_tsn);
			if (SCTP_IS_TSN_PRESENT(asoc->mapping_array, gap)) {
				asoc->highest_tsn_inside_map = i;
				fnd = 1;
				break;
			}
		}
		if (!fnd) {
			asoc->highest_tsn_inside_map = asoc->mapping_array_base_tsn - 1;
		}

#ifdef SCTP_DEBUG
		SCTPDBG(SCTP_DEBUG_PCB1, "Freed %d chunks from reneg harvest\n", cnt);
#endif

		asoc->last_revoke_count = cnt;
		sctp_timer_stop(SCTP_TIMER_TYPE_RECV, stcb->sctp_ep, stcb, NULL,
		                SCTP_FROM_SCTP_PCB + SCTP_LOC_11);

		sctp_send_sack(stcb, SCTP_SO_NOT_LOCKED);
		sctp_chunk_output(stcb->sctp_ep, stcb, SCTP_OUTPUT_FROM_DRAIN, SCTP_SO_NOT_LOCKED);
	}

}

void
sctp_drain()
{

#if defined(__FreeBSD__) && !defined(__Userspace__)
	VNET_ITERATOR_DECL(vnet_iter);
#else
	struct sctp_inpcb *inp;
	struct sctp_tcb *stcb;

	SCTP_STAT_INCR(sctps_protocol_drain_calls);
	if (SCTP_BASE_SYSCTL(sctp_do_drain) == 0) {
		return;
	}
#endif
#if defined(__FreeBSD__) && !defined(__Userspace__)
	VNET_LIST_RLOCK_NOSLEEP();
	VNET_FOREACH(vnet_iter) {
		CURVNET_SET(vnet_iter);
		struct sctp_inpcb *inp;
		struct sctp_tcb *stcb;
#endif

#if defined(__FreeBSD__) && !defined(__Userspace__)
		SCTP_STAT_INCR(sctps_protocol_drain_calls);
		if (SCTP_BASE_SYSCTL(sctp_do_drain) == 0) {
#ifdef VIMAGE
			continue;
#else
			return;
#endif
		}
#endif
		SCTP_INP_INFO_RLOCK();
		LIST_FOREACH(inp, &SCTP_BASE_INFO(listhead), sctp_list) {

			SCTP_INP_RLOCK(inp);
			LIST_FOREACH(stcb, &inp->sctp_asoc_list, sctp_tcblist) {

				SCTP_TCB_LOCK(stcb);
				sctp_drain_mbufs(stcb);
				SCTP_TCB_UNLOCK(stcb);
			}
			SCTP_INP_RUNLOCK(inp);
		}
		SCTP_INP_INFO_RUNLOCK();
#if defined(__FreeBSD__) && !defined(__Userspace__)
		CURVNET_RESTORE();
	}
	VNET_LIST_RUNLOCK_NOSLEEP();
#endif
}

int
sctp_initiate_iterator(inp_func inpf,
		       asoc_func af,
		       inp_func inpe,
		       uint32_t pcb_state,
		       uint32_t pcb_features,
		       uint32_t asoc_state,
		       void *argp,
		       uint32_t argi,
		       end_func ef,
		       struct sctp_inpcb *s_inp,
		       uint8_t chunk_output_off)
{
	struct sctp_iterator *it = NULL;

	if (af == NULL) {
		return (-1);
	}
	if (SCTP_BASE_VAR(sctp_pcb_initialized) == 0) {
		SCTP_PRINTF("%s: abort on initialize being %d\n", __func__,
		            SCTP_BASE_VAR(sctp_pcb_initialized));
		return (-1);
	}
	SCTP_MALLOC(it, struct sctp_iterator *, sizeof(struct sctp_iterator),
		    SCTP_M_ITER);
	if (it == NULL) {
		SCTP_LTRACE_ERR_RET(NULL, NULL, NULL, SCTP_FROM_SCTP_PCB, ENOMEM);
		return (-1);
	}
	memset(it, 0, sizeof(*it));
	it->function_assoc = af;
	it->function_inp = inpf;
	if (inpf)
		it->done_current_ep = 0;
	else
		it->done_current_ep = 1;
	it->function_atend = ef;
	it->pointer = argp;
	it->val = argi;
	it->pcb_flags = pcb_state;
	it->pcb_features = pcb_features;
	it->asoc_state = asoc_state;
	it->function_inp_end = inpe;
	it->no_chunk_output = chunk_output_off;
#if defined(__FreeBSD__) && !defined(__Userspace__)
	it->vn = curvnet;
#endif
	if (s_inp) {

		it->inp = s_inp;
		SCTP_INP_INCR_REF(it->inp);
		it->iterator_flags = SCTP_ITERATOR_DO_SINGLE_INP;
	} else {
		SCTP_INP_INFO_RLOCK();
		it->inp = LIST_FIRST(&SCTP_BASE_INFO(listhead));
		if (it->inp) {
			SCTP_INP_INCR_REF(it->inp);
		}
		SCTP_INP_INFO_RUNLOCK();
		it->iterator_flags = SCTP_ITERATOR_DO_ALL_INP;
	}
	SCTP_IPI_ITERATOR_WQ_LOCK();
	if (SCTP_BASE_VAR(sctp_pcb_initialized) == 0) {
		SCTP_IPI_ITERATOR_WQ_UNLOCK();
		SCTP_PRINTF("%s: rollback on initialize being %d it=%p\n", __func__,
		            SCTP_BASE_VAR(sctp_pcb_initialized), it);
		SCTP_FREE(it, SCTP_M_ITER);
		return (-1);
	}
	TAILQ_INSERT_TAIL(&sctp_it_ctl.iteratorhead, it, sctp_nxt_itr);
	if (sctp_it_ctl.iterator_running == 0) {
		sctp_wakeup_iterator();
	}
	SCTP_IPI_ITERATOR_WQ_UNLOCK();

	return (0);
}
