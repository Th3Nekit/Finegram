/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 2001-2007, by Cisco Systems, Inc. All rights reserved.
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
#include <netinet/sctp_header.h>
#include <netinet/sctp_pcb.h>
#include <netinet/sctputil.h>
#include <netinet/sctp_output.h>
#include <netinet/sctp_uio.h>
#include <netinet/sctp_auth.h>
#include <netinet/sctp_timer.h>
#include <netinet/sctp_asconf.h>
#include <netinet/sctp_indata.h>
#include <netinet/sctp_bsd_addr.h>
#include <netinet/sctp_input.h>
#include <netinet/sctp_crc32.h>
#if defined(__FreeBSD__) && !defined(__Userspace__)
#include <netinet/sctp_lock_bsd.h>
#endif

static uint32_t
sctp_add_chk_to_control(struct sctp_queued_to_read *control,
			struct sctp_stream_in *strm,
			struct sctp_tcb *stcb,
			struct sctp_association *asoc,
			struct sctp_tmit_chunk *chk, int hold_rlock);

void
sctp_set_rwnd(struct sctp_tcb *stcb, struct sctp_association *asoc)
{
	asoc->my_rwnd = sctp_calc_rwnd(stcb, asoc);
}

uint32_t
sctp_calc_rwnd(struct sctp_tcb *stcb, struct sctp_association *asoc)
{
	uint32_t calc = 0;

	if (stcb->sctp_socket == NULL) {
		return (calc);
	}

	KASSERT(asoc->cnt_on_reasm_queue > 0 || asoc->size_on_reasm_queue == 0,
	        ("size_on_reasm_queue is %u", asoc->size_on_reasm_queue));
	KASSERT(asoc->cnt_on_all_streams > 0 || asoc->size_on_all_streams == 0,
	        ("size_on_all_streams is %u", asoc->size_on_all_streams));
	if (stcb->asoc.sb_cc == 0 &&
	    asoc->cnt_on_reasm_queue == 0 &&
	    asoc->cnt_on_all_streams == 0) {

		calc = max(SCTP_SB_LIMIT_RCV(stcb->sctp_socket), SCTP_MINIMAL_RWND);
		return (calc);
	}

	calc = (uint32_t) sctp_sbspace(&stcb->asoc, &stcb->sctp_socket->so_rcv);

	calc = sctp_sbspace_sub(calc, (uint32_t)(asoc->size_on_reasm_queue +
	                                         asoc->cnt_on_reasm_queue * MSIZE));
	calc = sctp_sbspace_sub(calc, (uint32_t)(asoc->size_on_all_streams +
	                                         asoc->cnt_on_all_streams * MSIZE));
	if (calc == 0) {

		return (calc);
	}

	calc = sctp_sbspace_sub(calc, stcb->asoc.my_rwnd_control_len);

	if (calc < stcb->asoc.my_rwnd_control_len) {
		calc = 1;
	}
	return (calc);
}

struct sctp_queued_to_read *
sctp_build_readq_entry(struct sctp_tcb *stcb,
    struct sctp_nets *net,
    uint32_t tsn, uint32_t ppid,
    uint32_t context, uint16_t sid,
    uint32_t mid, uint8_t flags,
    struct mbuf *dm)
{
	struct sctp_queued_to_read *read_queue_e = NULL;

	sctp_alloc_a_readq(stcb, read_queue_e);
	if (read_queue_e == NULL) {
		goto failed_build;
	}
	memset(read_queue_e, 0, sizeof(struct sctp_queued_to_read));
	read_queue_e->sinfo_stream = sid;
	read_queue_e->sinfo_flags = (flags << 8);
	read_queue_e->sinfo_ppid = ppid;
	read_queue_e->sinfo_context = context;
	read_queue_e->sinfo_tsn = tsn;
	read_queue_e->sinfo_cumtsn = tsn;
	read_queue_e->sinfo_assoc_id = sctp_get_associd(stcb);
	read_queue_e->mid = mid;
	read_queue_e->top_fsn = read_queue_e->fsn_included = 0xffffffff;
	TAILQ_INIT(&read_queue_e->reasm);
	read_queue_e->whoFrom = net;
	atomic_add_int(&net->ref_count, 1);
	read_queue_e->data = dm;
	read_queue_e->stcb = stcb;
	read_queue_e->port_from = stcb->rport;
	if (stcb->asoc.state & SCTP_STATE_ABOUT_TO_BE_FREED) {
		read_queue_e->do_not_ref_stcb = 1;
	}
failed_build:
	return (read_queue_e);
}

struct mbuf *
sctp_build_ctl_nchunk(struct sctp_inpcb *inp, struct sctp_sndrcvinfo *sinfo)
{
	struct sctp_extrcvinfo *seinfo;
	struct sctp_sndrcvinfo *outinfo;
	struct sctp_rcvinfo *rcvinfo;
	struct sctp_nxtinfo *nxtinfo;
#if defined(_WIN32)
	WSACMSGHDR *cmh;
#else
	struct cmsghdr *cmh;
#endif
	struct mbuf *ret;
	int len;
	int use_extended;
	int provide_nxt;

	if (sctp_is_feature_off(inp, SCTP_PCB_FLAGS_RECVDATAIOEVNT) &&
	    sctp_is_feature_off(inp, SCTP_PCB_FLAGS_RECVRCVINFO) &&
	    sctp_is_feature_off(inp, SCTP_PCB_FLAGS_RECVNXTINFO)) {

		return (NULL);
	}

	len = 0;
	if (sctp_is_feature_on(inp, SCTP_PCB_FLAGS_RECVRCVINFO)) {
		len += CMSG_SPACE(sizeof(struct sctp_rcvinfo));
	}
	seinfo = (struct sctp_extrcvinfo *)sinfo;
	if (sctp_is_feature_on(inp, SCTP_PCB_FLAGS_RECVNXTINFO) &&
	    (seinfo->serinfo_next_flags & SCTP_NEXT_MSG_AVAIL)) {
		provide_nxt = 1;
		len += CMSG_SPACE(sizeof(struct sctp_nxtinfo));
	} else {
		provide_nxt = 0;
	}
	if (sctp_is_feature_on(inp, SCTP_PCB_FLAGS_RECVDATAIOEVNT)) {
		if (sctp_is_feature_on(inp, SCTP_PCB_FLAGS_EXT_RCVINFO)) {
			use_extended = 1;
			len += CMSG_SPACE(sizeof(struct sctp_extrcvinfo));
		} else {
			use_extended = 0;
			len += CMSG_SPACE(sizeof(struct sctp_sndrcvinfo));
		}
	} else {
		use_extended = 0;
	}

	ret = sctp_get_mbuf_for_msg(len, 0, M_NOWAIT, 1, MT_DATA);
	if (ret == NULL) {

		return (ret);
	}
	SCTP_BUF_LEN(ret) = 0;

#if defined(_WIN32)
	cmh = mtod(ret, WSACMSGHDR *);
#else
	cmh = mtod(ret, struct cmsghdr *);
#endif

	memset(cmh, 0, len);
	if (sctp_is_feature_on(inp, SCTP_PCB_FLAGS_RECVRCVINFO)) {
		cmh->cmsg_level = IPPROTO_SCTP;
		cmh->cmsg_len = CMSG_LEN(sizeof(struct sctp_rcvinfo));
		cmh->cmsg_type = SCTP_RCVINFO;
		rcvinfo = (struct sctp_rcvinfo *)CMSG_DATA(cmh);
		rcvinfo->rcv_sid = sinfo->sinfo_stream;
		rcvinfo->rcv_ssn = sinfo->sinfo_ssn;
		rcvinfo->rcv_flags = sinfo->sinfo_flags;
		rcvinfo->rcv_ppid = sinfo->sinfo_ppid;
		rcvinfo->rcv_tsn = sinfo->sinfo_tsn;
		rcvinfo->rcv_cumtsn = sinfo->sinfo_cumtsn;
		rcvinfo->rcv_context = sinfo->sinfo_context;
		rcvinfo->rcv_assoc_id = sinfo->sinfo_assoc_id;
#if defined(_WIN32)
		cmh = (WSACMSGHDR *)((caddr_t)cmh + CMSG_SPACE(sizeof(struct sctp_rcvinfo)));
#else
		cmh = (struct cmsghdr *)((caddr_t)cmh + CMSG_SPACE(sizeof(struct sctp_rcvinfo)));
#endif
		SCTP_BUF_LEN(ret) += CMSG_SPACE(sizeof(struct sctp_rcvinfo));
	}
	if (provide_nxt) {
		cmh->cmsg_level = IPPROTO_SCTP;
		cmh->cmsg_len = CMSG_LEN(sizeof(struct sctp_nxtinfo));
		cmh->cmsg_type = SCTP_NXTINFO;
		nxtinfo = (struct sctp_nxtinfo *)CMSG_DATA(cmh);
		nxtinfo->nxt_sid = seinfo->serinfo_next_stream;
		nxtinfo->nxt_flags = 0;
		if (seinfo->serinfo_next_flags & SCTP_NEXT_MSG_IS_UNORDERED) {
			nxtinfo->nxt_flags |= SCTP_UNORDERED;
		}
		if (seinfo->serinfo_next_flags & SCTP_NEXT_MSG_IS_NOTIFICATION) {
			nxtinfo->nxt_flags |= SCTP_NOTIFICATION;
		}
		if (seinfo->serinfo_next_flags & SCTP_NEXT_MSG_ISCOMPLETE) {
			nxtinfo->nxt_flags |= SCTP_COMPLETE;
		}
		nxtinfo->nxt_ppid = seinfo->serinfo_next_ppid;
		nxtinfo->nxt_length = seinfo->serinfo_next_length;
		nxtinfo->nxt_assoc_id = seinfo->serinfo_next_aid;
#if defined(_WIN32)
		cmh = (WSACMSGHDR *)((caddr_t)cmh + CMSG_SPACE(sizeof(struct sctp_nxtinfo)));
#else
		cmh = (struct cmsghdr *)((caddr_t)cmh + CMSG_SPACE(sizeof(struct sctp_nxtinfo)));
#endif
		SCTP_BUF_LEN(ret) += CMSG_SPACE(sizeof(struct sctp_nxtinfo));
	}
	if (sctp_is_feature_on(inp, SCTP_PCB_FLAGS_RECVDATAIOEVNT)) {
		cmh->cmsg_level = IPPROTO_SCTP;
		outinfo = (struct sctp_sndrcvinfo *)CMSG_DATA(cmh);
		if (use_extended) {
			cmh->cmsg_len = CMSG_LEN(sizeof(struct sctp_extrcvinfo));
			cmh->cmsg_type = SCTP_EXTRCV;
			memcpy(outinfo, sinfo, sizeof(struct sctp_extrcvinfo));
			SCTP_BUF_LEN(ret) += CMSG_SPACE(sizeof(struct sctp_extrcvinfo));
		} else {
			cmh->cmsg_len = CMSG_LEN(sizeof(struct sctp_sndrcvinfo));
			cmh->cmsg_type = SCTP_SNDRCV;
			*outinfo = *sinfo;
			SCTP_BUF_LEN(ret) += CMSG_SPACE(sizeof(struct sctp_sndrcvinfo));
		}
	}
	return (ret);
}

static void
sctp_mark_non_revokable(struct sctp_association *asoc, uint32_t tsn)
{
	uint32_t gap, i;
	int in_r, in_nr;

	if (SCTP_BASE_SYSCTL(sctp_do_drain) == 0) {
		return;
	}
	if (SCTP_TSN_GE(asoc->cumulative_tsn, tsn)) {

		return;
	}
	SCTP_CALC_TSN_TO_GAP(gap, tsn, asoc->mapping_array_base_tsn);
	in_r = SCTP_IS_TSN_PRESENT(asoc->mapping_array, gap);
	in_nr = SCTP_IS_TSN_PRESENT(asoc->nr_mapping_array, gap);
	KASSERT(in_r || in_nr, ("%s: Things are really messed up now", __func__));
	if (!in_nr) {
		SCTP_SET_TSN_PRESENT(asoc->nr_mapping_array, gap);
		if (SCTP_TSN_GT(tsn, asoc->highest_tsn_inside_nr_map)) {
			asoc->highest_tsn_inside_nr_map = tsn;
		}
	}
	if (in_r) {
		SCTP_UNSET_TSN_PRESENT(asoc->mapping_array, gap);
		if (tsn == asoc->highest_tsn_inside_map) {

			for (i = tsn - 1; SCTP_TSN_GE(i, asoc->mapping_array_base_tsn); i--) {
				SCTP_CALC_TSN_TO_GAP(gap, i, asoc->mapping_array_base_tsn);
				if (SCTP_IS_TSN_PRESENT(asoc->mapping_array, gap)) {
					asoc->highest_tsn_inside_map = i;
					break;
				}
			}
			if (!SCTP_TSN_GE(i, asoc->mapping_array_base_tsn)) {
				asoc->highest_tsn_inside_map = asoc->mapping_array_base_tsn - 1;
			}
		}
	}
}

static int
sctp_place_control_in_stream(struct sctp_stream_in *strm,
			     struct sctp_association *asoc,
			     struct sctp_queued_to_read *control)
{
	struct sctp_queued_to_read *at;
	struct sctp_readhead *q;
	uint8_t flags, unordered;

	flags = (control->sinfo_flags >> 8);
	unordered = flags & SCTP_DATA_UNORDERED;
	if (unordered) {
		q = &strm->uno_inqueue;
		if (asoc->idata_supported == 0) {
			if (!TAILQ_EMPTY(q)) {

				return (-1);
			}
			TAILQ_INSERT_TAIL(q, control, next_instrm);
			control->on_strm_q = SCTP_ON_UNORDERED;
			return (0);
		}
	} else {
		q = &strm->inqueue;
	}
	if ((flags & SCTP_DATA_NOT_FRAG) == SCTP_DATA_NOT_FRAG) {
		control->end_added = 1;
		control->first_frag_seen = 1;
		control->last_frag_seen = 1;
	}
	if (TAILQ_EMPTY(q)) {

		TAILQ_INSERT_HEAD(q, control, next_instrm);
		if (unordered) {
			control->on_strm_q = SCTP_ON_UNORDERED;
		} else {
			control->on_strm_q = SCTP_ON_ORDERED;
		}
		return (0);
	} else {
		TAILQ_FOREACH(at, q, next_instrm) {
			if (SCTP_MID_GT(asoc->idata_supported, at->mid, control->mid)) {

				TAILQ_INSERT_BEFORE(at, control, next_instrm);
				if (unordered) {
					control->on_strm_q = SCTP_ON_UNORDERED;
				} else {
					control->on_strm_q = SCTP_ON_ORDERED;
				}
				break;
			} else if (SCTP_MID_EQ(asoc->idata_supported, at->mid, control->mid)) {

				return (-1);
			} else {
				if (TAILQ_NEXT(at, next_instrm) == NULL) {

					if (SCTP_BASE_SYSCTL(sctp_logging_level) & SCTP_STR_LOGGING_ENABLE) {
						sctp_log_strm_del(control, at,
								  SCTP_STR_LOG_FROM_INSERT_TL);
					}
					TAILQ_INSERT_AFTER(q, at, control, next_instrm);
					if (unordered) {
						control->on_strm_q = SCTP_ON_UNORDERED;
					} else {
						control->on_strm_q = SCTP_ON_ORDERED;
					}
					break;
				}
			}
		}
	}
	return (0);
}

static void
sctp_abort_in_reasm(struct sctp_tcb *stcb,
                    struct sctp_queued_to_read *control,
                    struct sctp_tmit_chunk *chk,
                    int *abort_flag, int opspot)
{
	char msg[SCTP_DIAG_INFO_LEN];
	struct mbuf *oper;

	if (stcb->asoc.idata_supported) {
		SCTP_SNPRINTF(msg, sizeof(msg),
		              "Reass %x,CF:%x,TSN=%8.8x,SID=%4.4x,FSN=%8.8x,MID:%8.8x",
		              opspot,
		              control->fsn_included,
		              chk->rec.data.tsn,
		              chk->rec.data.sid,
		              chk->rec.data.fsn, chk->rec.data.mid);
	} else {
		SCTP_SNPRINTF(msg, sizeof(msg),
		              "Reass %x,CI:%x,TSN=%8.8x,SID=%4.4x,FSN=%4.4x,SSN:%4.4x",
		              opspot,
		              control->fsn_included,
		              chk->rec.data.tsn,
		              chk->rec.data.sid,
		              chk->rec.data.fsn,
		              (uint16_t)chk->rec.data.mid);
	}
	oper = sctp_generate_cause(SCTP_CAUSE_PROTOCOL_VIOLATION, msg);
	sctp_m_freem(chk->data);
	chk->data = NULL;
	sctp_free_a_chunk(stcb, chk, SCTP_SO_NOT_LOCKED);
	stcb->sctp_ep->last_abort_code = SCTP_FROM_SCTP_INDATA + SCTP_LOC_1;
	sctp_abort_an_association(stcb->sctp_ep, stcb, oper, false, SCTP_SO_NOT_LOCKED);
	*abort_flag = 1;
}

static void
sctp_clean_up_control(struct sctp_tcb *stcb, struct sctp_queued_to_read *control)
{

	struct sctp_tmit_chunk *chk, *nchk;
	TAILQ_FOREACH_SAFE(chk, &control->reasm, sctp_next, nchk) {
		TAILQ_REMOVE(&control->reasm, chk, sctp_next);
		if (chk->data)
			sctp_m_freem(chk->data);
		chk->data = NULL;
		sctp_free_a_chunk(stcb, chk, SCTP_SO_NOT_LOCKED);
	}
	sctp_free_remote_addr(control->whoFrom);
	if (control->data) {
		sctp_m_freem(control->data);
		control->data = NULL;
	}
	sctp_free_a_readq(stcb, control);
}

static void
sctp_queue_data_to_stream(struct sctp_tcb *stcb,
    struct sctp_association *asoc,
    struct sctp_queued_to_read *control, int *abort_flag, int *need_reasm)
{

	struct sctp_queued_to_read *at;
	int queue_needed;
	uint32_t nxt_todel;
	struct mbuf *op_err;
	struct sctp_stream_in *strm;
	char msg[SCTP_DIAG_INFO_LEN];

	strm = &asoc->strmin[control->sinfo_stream];
	if (SCTP_BASE_SYSCTL(sctp_logging_level) & SCTP_STR_LOGGING_ENABLE) {
		sctp_log_strm_del(control, NULL, SCTP_STR_LOG_FROM_INTO_STRD);
	}
	if (SCTP_MID_GT((asoc->idata_supported), strm->last_mid_delivered, control->mid)) {

		SCTPDBG(SCTP_DEBUG_INDATA1, "Duplicate S-SEQ: %u delivered: %u from peer, Abort association\n",
			strm->last_mid_delivered, control->mid);

		TAILQ_INSERT_HEAD(&strm->inqueue, control, next_instrm);
		if (asoc->idata_supported) {
			SCTP_SNPRINTF(msg, sizeof(msg), "Delivered MID=%8.8x, got TSN=%8.8x, SID=%4.4x, MID=%8.8x",
			              strm->last_mid_delivered, control->sinfo_tsn,
			              control->sinfo_stream, control->mid);
		} else {
			SCTP_SNPRINTF(msg, sizeof(msg), "Delivered SSN=%4.4x, got TSN=%8.8x, SID=%4.4x, SSN=%4.4x",
			              (uint16_t)strm->last_mid_delivered,
			              control->sinfo_tsn,
			              control->sinfo_stream,
			              (uint16_t)control->mid);
		}
		op_err = sctp_generate_cause(SCTP_CAUSE_PROTOCOL_VIOLATION, msg);
		stcb->sctp_ep->last_abort_code = SCTP_FROM_SCTP_INDATA + SCTP_LOC_2;
		sctp_abort_an_association(stcb->sctp_ep, stcb, op_err, false, SCTP_SO_NOT_LOCKED);
		*abort_flag = 1;
		return;
	}
	queue_needed = 1;
	asoc->size_on_all_streams += control->length;
	sctp_ucount_incr(asoc->cnt_on_all_streams);
	nxt_todel = strm->last_mid_delivered + 1;
	if (SCTP_MID_EQ(asoc->idata_supported, nxt_todel, control->mid)) {
#if defined(__APPLE__) && !defined(__Userspace__)
		struct socket *so;

		so = SCTP_INP_SO(stcb->sctp_ep);
		atomic_add_int(&stcb->asoc.refcnt, 1);
		SCTP_TCB_UNLOCK(stcb);
		SCTP_SOCKET_LOCK(so, 1);
		SCTP_TCB_LOCK(stcb);
		atomic_subtract_int(&stcb->asoc.refcnt, 1);
		if (stcb->sctp_ep->sctp_flags & SCTP_PCB_FLAGS_SOCKET_GONE) {
			SCTP_SOCKET_UNLOCK(so, 1);
			return;
		}
#endif

		if (SCTP_BASE_SYSCTL(sctp_logging_level) & SCTP_STR_LOGGING_ENABLE) {
			sctp_log_strm_del(control, NULL, SCTP_STR_LOG_FROM_IMMED_DEL);
		}

		queue_needed = 0;
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
		strm->last_mid_delivered++;
		sctp_mark_non_revokable(asoc, control->sinfo_tsn);
		sctp_add_to_readq(stcb->sctp_ep, stcb,
		                  control,
		                  &stcb->sctp_socket->so_rcv, 1,
		                  SCTP_READ_LOCK_NOT_HELD, SCTP_SO_LOCKED);
		TAILQ_FOREACH_SAFE(control, &strm->inqueue, next_instrm, at) {

			nxt_todel = strm->last_mid_delivered + 1;
			if (SCTP_MID_EQ(asoc->idata_supported, nxt_todel, control->mid) &&
			    (((control->sinfo_flags >> 8) & SCTP_DATA_NOT_FRAG) == SCTP_DATA_NOT_FRAG)) {
				if (control->on_strm_q == SCTP_ON_ORDERED) {
					TAILQ_REMOVE(&strm->inqueue, control, next_instrm);
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
#ifdef INVARIANTS
				} else {
					panic("Huh control: %p is on_strm_q: %d",
					      control, control->on_strm_q);
#endif
				}
				control->on_strm_q = 0;
				strm->last_mid_delivered++;

				if (SCTP_BASE_SYSCTL(sctp_logging_level) & SCTP_STR_LOGGING_ENABLE) {
					sctp_log_strm_del(control, NULL,
							  SCTP_STR_LOG_FROM_IMMED_DEL);
				}
				sctp_mark_non_revokable(asoc, control->sinfo_tsn);
				sctp_add_to_readq(stcb->sctp_ep, stcb,
				                  control,
				                  &stcb->sctp_socket->so_rcv, 1,
				                  SCTP_READ_LOCK_NOT_HELD,
				                  SCTP_SO_LOCKED);
				continue;
			} else if (SCTP_MID_EQ(asoc->idata_supported, nxt_todel, control->mid)) {
				*need_reasm = 1;
			}
			break;
		}
#if defined(__APPLE__) && !defined(__Userspace__)
		SCTP_SOCKET_UNLOCK(so, 1);
#endif
	}
	if (queue_needed) {

		if (sctp_place_control_in_stream(strm, asoc, control)) {
			SCTP_SNPRINTF(msg, sizeof(msg),
			              "Queue to str MID: %u duplicate", control->mid);
			sctp_clean_up_control(stcb, control);
			op_err = sctp_generate_cause(SCTP_CAUSE_PROTOCOL_VIOLATION, msg);
			stcb->sctp_ep->last_abort_code = SCTP_FROM_SCTP_INDATA + SCTP_LOC_3;
			sctp_abort_an_association(stcb->sctp_ep, stcb, op_err, false, SCTP_SO_NOT_LOCKED);
			*abort_flag = 1;
		}
	}
}

static void
sctp_setup_tail_pointer(struct sctp_queued_to_read *control)
{
	struct mbuf *m, *prev = NULL;
	struct sctp_tcb *stcb;

	stcb = control->stcb;
	control->held_length = 0;
	control->length = 0;
	m = control->data;
	while (m) {
		if (SCTP_BUF_LEN(m) == 0) {

			if (prev == NULL) {

				control->data = sctp_m_free(m);
				m = control->data;
			} else {
				SCTP_BUF_NEXT(prev) = sctp_m_free(m);
				m = SCTP_BUF_NEXT(prev);
			}
			if (m == NULL) {
				control->tail_mbuf = prev;
			}
			continue;
		}
		prev = m;
		atomic_add_int(&control->length, SCTP_BUF_LEN(m));
		if (control->on_read_q) {

			sctp_sballoc(stcb, &stcb->sctp_socket->so_rcv, m);
		}
		m = SCTP_BUF_NEXT(m);
	}
	if (prev) {
		control->tail_mbuf = prev;
	}
}

static void
sctp_add_to_tail_pointer(struct sctp_queued_to_read *control, struct mbuf *m, uint32_t *added)
{
	struct mbuf *prev=NULL;
	struct sctp_tcb *stcb;

	stcb = control->stcb;
	if (stcb == NULL) {
#ifdef INVARIANTS
		panic("Control broken");
#else
		return;
#endif
	}
	if (control->tail_mbuf == NULL) {

		sctp_m_freem(control->data);
		control->data = m;
		sctp_setup_tail_pointer(control);
		return;
	}
	control->tail_mbuf->m_next = m;
	while (m) {
		if (SCTP_BUF_LEN(m) == 0) {

			if (prev == NULL) {

				control->tail_mbuf->m_next = sctp_m_free(m);
				m = control->tail_mbuf->m_next;
			} else {
				SCTP_BUF_NEXT(prev) = sctp_m_free(m);
				m = SCTP_BUF_NEXT(prev);
			}
			if (m == NULL) {
				control->tail_mbuf = prev;
			}
			continue;
		}
		prev = m;
		if (control->on_read_q) {

			sctp_sballoc(stcb, &stcb->sctp_socket->so_rcv, m);
		}
		*added += SCTP_BUF_LEN(m);
		atomic_add_int(&control->length, SCTP_BUF_LEN(m));
		m = SCTP_BUF_NEXT(m);
	}
	if (prev) {
		control->tail_mbuf = prev;
	}
}

static void
sctp_build_readq_entry_from_ctl(struct sctp_queued_to_read *nc, struct sctp_queued_to_read *control)
{
	memset(nc, 0, sizeof(struct sctp_queued_to_read));
	nc->sinfo_stream = control->sinfo_stream;
	nc->mid = control->mid;
	TAILQ_INIT(&nc->reasm);
	nc->top_fsn = control->top_fsn;
	nc->mid = control->mid;
	nc->sinfo_flags = control->sinfo_flags;
	nc->sinfo_ppid = control->sinfo_ppid;
	nc->sinfo_context = control->sinfo_context;
	nc->fsn_included = 0xffffffff;
	nc->sinfo_tsn = control->sinfo_tsn;
	nc->sinfo_cumtsn = control->sinfo_cumtsn;
	nc->sinfo_assoc_id = control->sinfo_assoc_id;
	nc->whoFrom = control->whoFrom;
	atomic_add_int(&nc->whoFrom->ref_count, 1);
	nc->stcb = control->stcb;
	nc->port_from = control->port_from;
	nc->do_not_ref_stcb = control->do_not_ref_stcb;
}

static void
sctp_reset_a_control(struct sctp_queued_to_read *control,
                     struct sctp_inpcb *inp, uint32_t tsn)
{
	control->fsn_included = tsn;
	if (control->on_read_q) {

		TAILQ_REMOVE(&inp->read_queue, control, next);
		control->on_read_q = 0;
	}
}

static int
sctp_handle_old_unordered_data(struct sctp_tcb *stcb,
                               struct sctp_association *asoc,
                               struct sctp_stream_in *strm,
                               struct sctp_queued_to_read *control,
                               uint32_t pd_point,
                               int inp_read_lock_held)
{

	struct sctp_tmit_chunk *chk, *lchk, *tchk;
	uint32_t fsn;
	struct sctp_queued_to_read *nc;
	int cnt_added;

	if (control->first_frag_seen == 0) {

		return (1);
	}

	cnt_added = 0;
restart:
	fsn = control->fsn_included + 1;

	TAILQ_FOREACH_SAFE(chk, &control->reasm, sctp_next, lchk) {
		if (chk->rec.data.fsn == fsn) {

			sctp_alloc_a_readq(stcb, nc);
			if (nc == NULL) {
				break;
			}
			memset(nc, 0, sizeof(struct sctp_queued_to_read));
			TAILQ_REMOVE(&control->reasm, chk, sctp_next);
			sctp_add_chk_to_control(control, strm, stcb, asoc, chk, inp_read_lock_held);
			fsn++;
			cnt_added++;
			chk = NULL;
			if (control->end_added) {

				if (!TAILQ_EMPTY(&control->reasm)) {

					sctp_build_readq_entry_from_ctl(nc, control);
					tchk = TAILQ_FIRST(&control->reasm);
					if (tchk->rec.data.rcv_flags & SCTP_DATA_FIRST_FRAG) {
						TAILQ_REMOVE(&control->reasm, tchk, sctp_next);
						if (asoc->size_on_reasm_queue >= tchk->send_size) {
							asoc->size_on_reasm_queue -= tchk->send_size;
						} else {
#ifdef INVARIANTS
						panic("size_on_reasm_queue = %u smaller than chunk length %u", asoc->size_on_reasm_queue, tchk->send_size);
#else
						asoc->size_on_reasm_queue = 0;
#endif
						}
						sctp_ucount_decr(asoc->cnt_on_reasm_queue);
						nc->first_frag_seen = 1;
						nc->fsn_included = tchk->rec.data.fsn;
						nc->data = tchk->data;
						nc->sinfo_ppid = tchk->rec.data.ppid;
						nc->sinfo_tsn = tchk->rec.data.tsn;
						sctp_mark_non_revokable(asoc, tchk->rec.data.tsn);
						tchk->data = NULL;
						sctp_free_a_chunk(stcb, tchk, SCTP_SO_NOT_LOCKED);
						sctp_setup_tail_pointer(nc);
						tchk = TAILQ_FIRST(&control->reasm);
					}

					while (tchk) {
						TAILQ_REMOVE(&control->reasm, tchk, sctp_next);
						TAILQ_INSERT_TAIL(&nc->reasm, tchk, sctp_next);
						tchk = TAILQ_FIRST(&control->reasm);
					}

					TAILQ_INSERT_TAIL(&strm->uno_inqueue, nc, next_instrm);
					nc->on_strm_q = SCTP_ON_UNORDERED;
					if (control->on_strm_q) {
						TAILQ_REMOVE(&strm->uno_inqueue, control, next_instrm);
						control->on_strm_q = 0;
					}
				}
				if (control->pdapi_started) {
					strm->pd_api_started = 0;
					control->pdapi_started = 0;
				}
				if (control->on_strm_q) {
					TAILQ_REMOVE(&strm->uno_inqueue, control, next_instrm);
					control->on_strm_q = 0;
					SCTP_STAT_INCR_COUNTER64(sctps_reasmusrmsgs);
				}
				if (control->on_read_q == 0) {
					sctp_add_to_readq(stcb->sctp_ep, stcb, control,
							  &stcb->sctp_socket->so_rcv, control->end_added,
							  inp_read_lock_held, SCTP_SO_NOT_LOCKED);
#if defined(__Userspace__)
				} else {
					sctp_invoke_recv_callback(stcb->sctp_ep, stcb, control, inp_read_lock_held);
#endif
				}
				sctp_wakeup_the_read_socket(stcb->sctp_ep, stcb, SCTP_SO_NOT_LOCKED);
				if ((nc->first_frag_seen) && !TAILQ_EMPTY(&nc->reasm)) {

					control = nc;
					goto restart;
				} else {
					if (nc->on_strm_q == 0) {
						sctp_free_a_readq(stcb, nc);
					}
				}
				return (1);
			} else {
				sctp_free_a_readq(stcb, nc);
			}
		} else {

			break;
		}
	}
	if (cnt_added && strm->pd_api_started) {
#if defined(__Userspace__)
		sctp_invoke_recv_callback(stcb->sctp_ep, stcb, control, inp_read_lock_held);
#endif
		sctp_wakeup_the_read_socket(stcb->sctp_ep, stcb, SCTP_SO_NOT_LOCKED);
	}
	if ((control->length > pd_point) && (strm->pd_api_started == 0)) {
		strm->pd_api_started = 1;
		control->pdapi_started = 1;
		sctp_add_to_readq(stcb->sctp_ep, stcb, control,
		                  &stcb->sctp_socket->so_rcv, control->end_added,
		                  inp_read_lock_held, SCTP_SO_NOT_LOCKED);
		sctp_wakeup_the_read_socket(stcb->sctp_ep, stcb, SCTP_SO_NOT_LOCKED);
		return (0);
	} else {
		return (1);
	}
}

static void
sctp_inject_old_unordered_data(struct sctp_tcb *stcb,
                               struct sctp_association *asoc,
                               struct sctp_queued_to_read *control,
                               struct sctp_tmit_chunk *chk,
                               int *abort_flag)
{
	struct sctp_tmit_chunk *at;
	int inserted;

	if (chk->rec.data.rcv_flags & SCTP_DATA_FIRST_FRAG) {

		SCTPDBG(SCTP_DEBUG_XXX,
			"chunk is a first fsn: %u becomes fsn_included\n",
			chk->rec.data.fsn);
		at = TAILQ_FIRST(&control->reasm);
		if (at && SCTP_TSN_GT(chk->rec.data.fsn, at->rec.data.fsn)) {

			goto place_chunk;
		}
		if (control->first_frag_seen) {

			struct mbuf *tdata;
			uint32_t tmp;

			if (SCTP_TSN_GT(chk->rec.data.fsn, control->fsn_included)) {

				goto place_chunk;
			}
			if ((chk->rec.data.fsn == control->fsn_included) ||
			    (control->pdapi_started)) {

				sctp_abort_in_reasm(stcb, control, chk,
						    abort_flag,
						    SCTP_FROM_SCTP_INDATA + SCTP_LOC_4);

				return;
			}

			tdata = control->data;
			control->data = chk->data;
			chk->data = tdata;

			chk->send_size = control->length;

			sctp_setup_tail_pointer(control);

			tmp = control->fsn_included;
			control->fsn_included = chk->rec.data.fsn;
			chk->rec.data.fsn = tmp;

			tmp = control->sinfo_tsn;
			control->sinfo_tsn = chk->rec.data.tsn;
			chk->rec.data.tsn = tmp;

			tmp = control->sinfo_ppid;
			control->sinfo_ppid = chk->rec.data.ppid;
			chk->rec.data.ppid = tmp;

			goto place_chunk;
		}
		control->first_frag_seen = 1;
		control->fsn_included = chk->rec.data.fsn;
		control->top_fsn = chk->rec.data.fsn;
		control->sinfo_tsn = chk->rec.data.tsn;
		control->sinfo_ppid = chk->rec.data.ppid;
		control->data = chk->data;
		sctp_mark_non_revokable(asoc, chk->rec.data.tsn);
		chk->data = NULL;
		sctp_free_a_chunk(stcb, chk, SCTP_SO_NOT_LOCKED);
		sctp_setup_tail_pointer(control);
		return;
	}
place_chunk:
	inserted = 0;
	TAILQ_FOREACH(at, &control->reasm, sctp_next) {
		if (SCTP_TSN_GT(at->rec.data.fsn, chk->rec.data.fsn)) {

			asoc->size_on_reasm_queue += chk->send_size;
			sctp_ucount_incr(asoc->cnt_on_reasm_queue);
			inserted = 1;
			TAILQ_INSERT_BEFORE(at, chk, sctp_next);
			break;
		} else if (at->rec.data.fsn == chk->rec.data.fsn) {

			sctp_abort_in_reasm(stcb, control, chk,
			                    abort_flag,
			                    SCTP_FROM_SCTP_INDATA + SCTP_LOC_5);
			return;
		}
	}
	if (inserted == 0) {

		asoc->size_on_reasm_queue += chk->send_size;
		sctp_ucount_incr(asoc->cnt_on_reasm_queue);
		control->top_fsn = chk->rec.data.fsn;
		TAILQ_INSERT_TAIL(&control->reasm, chk, sctp_next);
	}
}

static int
sctp_deliver_reasm_check(struct sctp_tcb *stcb, struct sctp_association *asoc,
                         struct sctp_stream_in *strm, int inp_read_lock_held)
{

	struct sctp_queued_to_read *control, *nctl = NULL;
	uint32_t next_to_del;
	uint32_t pd_point;
	int ret = 0;

	if (stcb->sctp_socket) {
		pd_point = min(SCTP_SB_LIMIT_RCV(stcb->sctp_socket) >> SCTP_PARTIAL_DELIVERY_SHIFT,
			       stcb->sctp_ep->partial_delivery_point);
	} else {
		pd_point = stcb->sctp_ep->partial_delivery_point;
	}
	control = TAILQ_FIRST(&strm->uno_inqueue);

	if ((control != NULL) &&
	    (asoc->idata_supported == 0)) {

		if (sctp_handle_old_unordered_data(stcb, asoc, strm, control, pd_point, inp_read_lock_held)) {
			goto done_un;
		}
	}
	if (strm->pd_api_started) {

		return (0);
	}
	while (control) {
		SCTPDBG(SCTP_DEBUG_XXX, "Looking at control: %p e(%d) ssn: %u top_fsn: %u inc_fsn: %u -uo\n",
			control, control->end_added, control->mid, control->top_fsn, control->fsn_included);
		nctl = TAILQ_NEXT(control, next_instrm);
		if (control->end_added) {

			if (control->on_strm_q) {
#ifdef INVARIANTS
				if (control->on_strm_q != SCTP_ON_UNORDERED) {
					panic("Huh control: %p on_q: %d -- not unordered?",
					      control, control->on_strm_q);
				}
#endif
				SCTP_STAT_INCR_COUNTER64(sctps_reasmusrmsgs);
				TAILQ_REMOVE(&strm->uno_inqueue, control, next_instrm);
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
				control->on_strm_q = 0;
			}
			if (control->on_read_q == 0) {
				sctp_add_to_readq(stcb->sctp_ep, stcb,
						  control,
						  &stcb->sctp_socket->so_rcv, control->end_added,
						  inp_read_lock_held, SCTP_SO_NOT_LOCKED);
			}
		} else {

			if ((control->length >= pd_point) && (strm->pd_api_started == 0)) {
				strm->pd_api_started = 1;
				control->pdapi_started = 1;
				sctp_add_to_readq(stcb->sctp_ep, stcb,
						  control,
						  &stcb->sctp_socket->so_rcv, control->end_added,
						  inp_read_lock_held, SCTP_SO_NOT_LOCKED);

				break;
			}
		}
		control = nctl;
	}
done_un:
	control = TAILQ_FIRST(&strm->inqueue);
	if (strm->pd_api_started) {

		return (0);
	}
	if (control == NULL) {
		return (ret);
	}
	if (SCTP_MID_EQ(asoc->idata_supported, strm->last_mid_delivered, control->mid)) {

		nctl = TAILQ_NEXT(control, next_instrm);
		SCTPDBG(SCTP_DEBUG_XXX,
			"Looking at control: %p e(%d) ssn: %u top_fsn: %u inc_fsn: %u (lastdel: %u)- o\n",
			control, control->end_added, control->mid,
			control->top_fsn, control->fsn_included,
			strm->last_mid_delivered);
		if (control->end_added) {
			if (control->on_strm_q) {
#ifdef INVARIANTS
				if (control->on_strm_q != SCTP_ON_ORDERED) {
					panic("Huh control: %p on_q: %d -- not ordered?",
					      control, control->on_strm_q);
				}
#endif
				SCTP_STAT_INCR_COUNTER64(sctps_reasmusrmsgs);
				TAILQ_REMOVE(&strm->inqueue, control, next_instrm);
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
				control->on_strm_q = 0;
			}
			if (strm->pd_api_started && control->pdapi_started) {
				control->pdapi_started = 0;
				strm->pd_api_started = 0;
			}
			if (control->on_read_q == 0) {
				sctp_add_to_readq(stcb->sctp_ep, stcb,
						  control,
						  &stcb->sctp_socket->so_rcv, control->end_added,
						  inp_read_lock_held, SCTP_SO_NOT_LOCKED);
			}
			control = nctl;
		}
	}
	if (strm->pd_api_started) {

		return (0);
	}
deliver_more:
	next_to_del = strm->last_mid_delivered + 1;
	if (control) {
		SCTPDBG(SCTP_DEBUG_XXX,
			"Looking at control: %p e(%d) ssn: %u top_fsn: %u inc_fsn: %u (nxtdel: %u)- o\n",
			control, control->end_added, control->mid, control->top_fsn, control->fsn_included,
			next_to_del);
		nctl = TAILQ_NEXT(control, next_instrm);
		if (SCTP_MID_EQ(asoc->idata_supported, control->mid, next_to_del) &&
		    (control->first_frag_seen)) {
			int done;

			if (control->end_added) {

				if (control->on_strm_q) {
#ifdef INVARIANTS
					if (control->on_strm_q != SCTP_ON_ORDERED) {
						panic("Huh control: %p on_q: %d -- not ordered?",
						      control, control->on_strm_q);
					}
#endif
					SCTP_STAT_INCR_COUNTER64(sctps_reasmusrmsgs);
					TAILQ_REMOVE(&strm->inqueue, control, next_instrm);
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
					control->on_strm_q = 0;
				}
				ret++;
			}
			if (((control->sinfo_flags >> 8) & SCTP_DATA_NOT_FRAG) == SCTP_DATA_NOT_FRAG) {

				sctp_mark_non_revokable(asoc, control->sinfo_tsn);
			} else if (control->end_added == 0) {

				if ((control->length < pd_point) || (strm->pd_api_started)) {

					goto out;
				}
			}
			done = (control->end_added) && (control->last_frag_seen);
			if (control->on_read_q == 0) {
				if (!done) {
					if (asoc->size_on_all_streams >= control->length) {
						asoc->size_on_all_streams -= control->length;
					} else {
#ifdef INVARIANTS
						panic("size_on_all_streams = %u smaller than control length %u", asoc->size_on_all_streams, control->length);
#else
						asoc->size_on_all_streams = 0;
#endif
					}
					strm->pd_api_started = 1;
					control->pdapi_started = 1;
				}
				sctp_add_to_readq(stcb->sctp_ep, stcb,
						  control,
						  &stcb->sctp_socket->so_rcv, control->end_added,
						  inp_read_lock_held, SCTP_SO_NOT_LOCKED);
			}
			strm->last_mid_delivered = next_to_del;
			if (done) {
				control = nctl;
				goto deliver_more;
			}
		}
	}
out:
	return (ret);
}

uint32_t
sctp_add_chk_to_control(struct sctp_queued_to_read *control,
			struct sctp_stream_in *strm,
			struct sctp_tcb *stcb, struct sctp_association *asoc,
			struct sctp_tmit_chunk *chk, int hold_rlock)
{

	uint32_t added=0;
	int i_locked = 0;

	if (control->on_read_q && (hold_rlock == 0)) {

		SCTP_INP_READ_LOCK(stcb->sctp_ep);
		i_locked = 1;
	}
	if (control->data == NULL) {
		control->data = chk->data;
		sctp_setup_tail_pointer(control);
	} else {
		sctp_add_to_tail_pointer(control, chk->data, &added);
	}
	control->fsn_included = chk->rec.data.fsn;
	asoc->size_on_reasm_queue -= chk->send_size;
	sctp_ucount_decr(asoc->cnt_on_reasm_queue);
	sctp_mark_non_revokable(asoc, chk->rec.data.tsn);
	chk->data = NULL;
	if (chk->rec.data.rcv_flags & SCTP_DATA_FIRST_FRAG) {
		control->first_frag_seen = 1;
		control->sinfo_tsn = chk->rec.data.tsn;
		control->sinfo_ppid = chk->rec.data.ppid;
	}
	if (chk->rec.data.rcv_flags & SCTP_DATA_LAST_FRAG) {

		if ((control->on_strm_q) && (control->on_read_q)) {
			if (control->pdapi_started) {
				control->pdapi_started = 0;
				strm->pd_api_started = 0;
			}
			if (control->on_strm_q == SCTP_ON_UNORDERED) {

				TAILQ_REMOVE(&strm->uno_inqueue, control, next_instrm);
				control->on_strm_q = 0;
			} else if (control->on_strm_q == SCTP_ON_ORDERED) {

				TAILQ_REMOVE(&strm->inqueue, control, next_instrm);

				sctp_ucount_decr(asoc->cnt_on_all_streams);
				control->on_strm_q = 0;
#ifdef INVARIANTS
			} else if (control->on_strm_q) {
				panic("Unknown state on ctrl: %p on_strm_q: %d", control,
				      control->on_strm_q);
#endif
			}
		}
		control->end_added = 1;
		control->last_frag_seen = 1;
	}
	if (i_locked) {
		SCTP_INP_READ_UNLOCK(stcb->sctp_ep);
	}
	sctp_free_a_chunk(stcb, chk, SCTP_SO_NOT_LOCKED);
	return (added);
}

static void
sctp_queue_data_for_reasm(struct sctp_tcb *stcb, struct sctp_association *asoc,
			  struct sctp_queued_to_read *control,
			  struct sctp_tmit_chunk *chk,
			  int created_control,
			  int *abort_flag, uint32_t tsn)
{
	uint32_t next_fsn;
	struct sctp_tmit_chunk *at, *nat;
	struct sctp_stream_in *strm;
	int do_wakeup, unordered;
	uint32_t lenadded;

	strm = &asoc->strmin[control->sinfo_stream];

	if ((control->sinfo_flags >> 8) & SCTP_DATA_UNORDERED) {
		unordered = 1;
	} else {
		unordered = 0;
	}

	if (created_control) {
		if ((unordered == 0) || (asoc->idata_supported)) {
			sctp_ucount_incr(asoc->cnt_on_all_streams);
		}
		if (sctp_place_control_in_stream(strm, asoc, control)) {

			sctp_abort_in_reasm(stcb, control, chk,
					    abort_flag,
					    SCTP_FROM_SCTP_INDATA + SCTP_LOC_6);
			sctp_clean_up_control(stcb, control);
			return;
		}
		if ((tsn == (asoc->cumulative_tsn + 1) && (asoc->idata_supported == 0))) {

			if ((chk->rec.data.rcv_flags & SCTP_DATA_FIRST_FRAG) == 0) {
				sctp_abort_in_reasm(stcb, control, chk,
				                    abort_flag,
				                    SCTP_FROM_SCTP_INDATA + SCTP_LOC_7);
				return;
			}
		}
	}
	if ((asoc->idata_supported == 0) && (unordered == 1)) {
		sctp_inject_old_unordered_data(stcb, asoc, control, chk, abort_flag);
		return;
	}

	if (chk->rec.data.rcv_flags & SCTP_DATA_FIRST_FRAG) {

		SCTPDBG(SCTP_DEBUG_XXX,
			"chunk is a first fsn: %u becomes fsn_included\n",
			chk->rec.data.fsn);
		if (control->first_frag_seen) {

			sctp_abort_in_reasm(stcb, control, chk,
			                    abort_flag,
			                    SCTP_FROM_SCTP_INDATA + SCTP_LOC_8);
			return;
		}
		control->first_frag_seen = 1;
		control->sinfo_ppid = chk->rec.data.ppid;
		control->sinfo_tsn = chk->rec.data.tsn;
		control->fsn_included = chk->rec.data.fsn;
		control->data = chk->data;
		sctp_mark_non_revokable(asoc, chk->rec.data.tsn);
		chk->data = NULL;
		sctp_free_a_chunk(stcb, chk, SCTP_SO_NOT_LOCKED);
		sctp_setup_tail_pointer(control);
		asoc->size_on_all_streams += control->length;
	} else {

		int inserted=0;
		if (control->last_frag_seen == 0) {

			if (SCTP_TSN_GT(chk->rec.data.fsn, control->top_fsn)) {
				SCTPDBG(SCTP_DEBUG_XXX,
					"We have a new top_fsn: %u\n",
					chk->rec.data.fsn);
				control->top_fsn = chk->rec.data.fsn;
			}
			if (chk->rec.data.rcv_flags & SCTP_DATA_LAST_FRAG) {
				SCTPDBG(SCTP_DEBUG_XXX,
					"The last fsn is now in place fsn: %u\n",
					chk->rec.data.fsn);
				control->last_frag_seen = 1;
				if (SCTP_TSN_GT(control->top_fsn, chk->rec.data.fsn)) {
					SCTPDBG(SCTP_DEBUG_XXX,
						"New fsn: %u is not at top_fsn: %u -- abort\n",
						chk->rec.data.fsn,
						control->top_fsn);
					sctp_abort_in_reasm(stcb, control, chk,
							    abort_flag,
							    SCTP_FROM_SCTP_INDATA + SCTP_LOC_9);
					return;
				}
			}
			if (asoc->idata_supported || control->first_frag_seen) {

				if (SCTP_TSN_GE(control->fsn_included, chk->rec.data.fsn)) {

					sctp_abort_in_reasm(stcb, control, chk,
							    abort_flag,
							    SCTP_FROM_SCTP_INDATA + SCTP_LOC_10);
					return;
				}
			}
		} else {
			if (chk->rec.data.rcv_flags & SCTP_DATA_LAST_FRAG) {

				SCTPDBG(SCTP_DEBUG_XXX,
					"Duplicate last fsn: %u (top: %u) -- abort\n",
					chk->rec.data.fsn, control->top_fsn);
				sctp_abort_in_reasm(stcb, control,
						    chk, abort_flag,
						    SCTP_FROM_SCTP_INDATA + SCTP_LOC_11);
				return;
			}
			if (asoc->idata_supported || control->first_frag_seen) {

				if (SCTP_TSN_GE(control->fsn_included, chk->rec.data.fsn)) {

					SCTPDBG(SCTP_DEBUG_XXX,
						"New fsn: %u is already seen in included_fsn: %u -- abort\n",
						chk->rec.data.fsn, control->fsn_included);
					sctp_abort_in_reasm(stcb, control, chk,
							    abort_flag,
							    SCTP_FROM_SCTP_INDATA + SCTP_LOC_12);
					return;
				}
			}

			if (SCTP_TSN_GT(chk->rec.data.fsn, control->top_fsn)) {
				SCTPDBG(SCTP_DEBUG_XXX,
					"New fsn: %u is beyond or at top_fsn: %u -- abort\n",
					chk->rec.data.fsn,
					control->top_fsn);
				sctp_abort_in_reasm(stcb, control, chk,
						    abort_flag,
						    SCTP_FROM_SCTP_INDATA + SCTP_LOC_13);
				return;
			}
		}

		SCTPDBG(SCTP_DEBUG_XXX,
			"chunk is a not first fsn: %u needs to be inserted\n",
			chk->rec.data.fsn);
		TAILQ_FOREACH(at, &control->reasm, sctp_next) {
			if (SCTP_TSN_GT(at->rec.data.fsn, chk->rec.data.fsn)) {
				if (chk->rec.data.rcv_flags & SCTP_DATA_LAST_FRAG) {

					SCTPDBG(SCTP_DEBUG_XXX,
					        "Last fragment not last in list: -- abort\n");
					sctp_abort_in_reasm(stcb, control,
					                    chk, abort_flag,
					                    SCTP_FROM_SCTP_INDATA + SCTP_LOC_14);
					return;
				}

				SCTPDBG(SCTP_DEBUG_XXX,
					"Insert it before fsn: %u\n",
					at->rec.data.fsn);
				asoc->size_on_reasm_queue += chk->send_size;
				sctp_ucount_incr(asoc->cnt_on_reasm_queue);
				TAILQ_INSERT_BEFORE(at, chk, sctp_next);
				inserted = 1;
				break;
			} else if (at->rec.data.fsn == chk->rec.data.fsn) {

				SCTPDBG(SCTP_DEBUG_XXX,
					"Duplicate to fsn: %u -- abort\n",
					at->rec.data.fsn);
				sctp_abort_in_reasm(stcb, control,
						    chk, abort_flag,
						    SCTP_FROM_SCTP_INDATA + SCTP_LOC_15);
				return;
			}
		}
		if (inserted == 0) {

			SCTPDBG(SCTP_DEBUG_XXX, "Inserting at tail of list fsn: %u\n",
				chk->rec.data.fsn);
			asoc->size_on_reasm_queue += chk->send_size;
			sctp_ucount_incr(asoc->cnt_on_reasm_queue);
			TAILQ_INSERT_TAIL(&control->reasm, chk, sctp_next);
		}
	}

	do_wakeup = 0;

	if (control->first_frag_seen) {
		next_fsn = control->fsn_included + 1;
		TAILQ_FOREACH_SAFE(at, &control->reasm, sctp_next, nat) {
			if (at->rec.data.fsn == next_fsn) {

				SCTPDBG(SCTP_DEBUG_XXX,
					"Adding more to control: %p at: %p fsn: %u next_fsn: %u included: %u\n",
					control, at,
					at->rec.data.fsn,
					next_fsn, control->fsn_included);
				TAILQ_REMOVE(&control->reasm, at, sctp_next);
				lenadded = sctp_add_chk_to_control(control, strm, stcb, asoc, at, SCTP_READ_LOCK_NOT_HELD);
				if (control->on_read_q) {
					do_wakeup = 1;
				} else {

					asoc->size_on_all_streams += lenadded;
				}
				next_fsn++;
				if (control->end_added && control->pdapi_started) {
					if (strm->pd_api_started) {
						strm->pd_api_started = 0;
						control->pdapi_started = 0;
					}
					if (control->on_read_q == 0) {
						sctp_add_to_readq(stcb->sctp_ep, stcb,
								  control,
								  &stcb->sctp_socket->so_rcv, control->end_added,
								  SCTP_READ_LOCK_NOT_HELD, SCTP_SO_NOT_LOCKED);
					}
					break;
				}
			} else {
				break;
			}
		}
	}
	if (do_wakeup) {
#if defined(__Userspace__)
		sctp_invoke_recv_callback(stcb->sctp_ep, stcb, control, SCTP_READ_LOCK_NOT_HELD);
#endif

		sctp_wakeup_the_read_socket(stcb->sctp_ep, stcb, SCTP_SO_NOT_LOCKED);
	}
}

static struct sctp_queued_to_read *
sctp_find_reasm_entry(struct sctp_stream_in *strm, uint32_t mid, int ordered, int idata_supported)
{
	struct sctp_queued_to_read *control;

	if (ordered) {
		TAILQ_FOREACH(control, &strm->inqueue, next_instrm) {
			if (SCTP_MID_EQ(idata_supported, control->mid, mid)) {
				break;
			}
		}
	} else {
		if (idata_supported) {
			TAILQ_FOREACH(control, &strm->uno_inqueue, next_instrm) {
				if (SCTP_MID_EQ(idata_supported, control->mid, mid)) {
					break;
				}
			}
		} else {
			control = TAILQ_FIRST(&strm->uno_inqueue);
		}
	}
	return (control);
}

static int
sctp_process_a_data_chunk(struct sctp_tcb *stcb, struct sctp_association *asoc,
			  struct mbuf **m, int offset,  int chk_length,
			  struct sctp_nets *net, uint32_t *high_tsn, int *abort_flag,
			  int *break_flag, int last_chunk, uint8_t chk_type)
{
	struct sctp_tmit_chunk *chk = NULL;
	struct sctp_stream_in *strm;
	uint32_t tsn, fsn, gap, mid;
	struct mbuf *dmbuf;
	int the_len;
	int need_reasm_check = 0;
	uint16_t sid;
	struct mbuf *op_err;
	char msg[SCTP_DIAG_INFO_LEN];
	struct sctp_queued_to_read *control, *ncontrol;
	uint32_t ppid;
	uint8_t chk_flags;
	struct sctp_stream_reset_list *liste;
	int ordered;
	size_t clen;
	int created_control = 0;

	if (chk_type == SCTP_IDATA) {
		struct sctp_idata_chunk *chunk, chunk_buf;

		chunk = (struct sctp_idata_chunk *)sctp_m_getptr(*m, offset,
		                                                 sizeof(struct sctp_idata_chunk), (uint8_t *)&chunk_buf);
		chk_flags = chunk->ch.chunk_flags;
		clen = sizeof(struct sctp_idata_chunk);
		tsn = ntohl(chunk->dp.tsn);
		sid = ntohs(chunk->dp.sid);
		mid = ntohl(chunk->dp.mid);
		if (chk_flags & SCTP_DATA_FIRST_FRAG) {
			fsn = 0;
			ppid = chunk->dp.ppid_fsn.ppid;
		} else {
			fsn = ntohl(chunk->dp.ppid_fsn.fsn);
			ppid = 0xffffffff;
		}
	} else {
		struct sctp_data_chunk *chunk, chunk_buf;

		chunk = (struct sctp_data_chunk *)sctp_m_getptr(*m, offset,
		                                                sizeof(struct sctp_data_chunk), (uint8_t *)&chunk_buf);
		chk_flags = chunk->ch.chunk_flags;
		clen = sizeof(struct sctp_data_chunk);
		tsn = ntohl(chunk->dp.tsn);
		sid = ntohs(chunk->dp.sid);
		mid = (uint32_t)(ntohs(chunk->dp.ssn));
		fsn = tsn;
		ppid = chunk->dp.ppid;
	}
	if ((size_t)chk_length == clen) {

		op_err = sctp_generate_no_user_data_cause(tsn);
		stcb->sctp_ep->last_abort_code = SCTP_FROM_SCTP_INDATA + SCTP_LOC_16;
		sctp_abort_an_association(stcb->sctp_ep, stcb, op_err, false, SCTP_SO_NOT_LOCKED);
		*abort_flag = 1;
		return (0);
	}
	if ((chk_flags & SCTP_DATA_SACK_IMMEDIATELY) == SCTP_DATA_SACK_IMMEDIATELY) {
		asoc->send_sack = 1;
	}
	ordered = ((chk_flags & SCTP_DATA_UNORDERED) == 0);
	if (SCTP_BASE_SYSCTL(sctp_logging_level) & SCTP_MAP_LOGGING_ENABLE) {
		sctp_log_map(tsn, asoc->cumulative_tsn, asoc->highest_tsn_inside_map, SCTP_MAP_TSN_ENTERS);
	}
	if (stcb == NULL) {
		return (0);
	}
	SCTP_LTRACE_CHK(stcb->sctp_ep, stcb, chk_type, tsn);
	if (SCTP_TSN_GE(asoc->cumulative_tsn, tsn)) {

		SCTP_STAT_INCR(sctps_recvdupdata);
		if (asoc->numduptsns < SCTP_MAX_DUP_TSNS) {

			asoc->dup_tsns[asoc->numduptsns] = tsn;
			asoc->numduptsns++;
		}
		asoc->send_sack = 1;
		return (0);
	}

	SCTP_CALC_TSN_TO_GAP(gap, tsn, asoc->mapping_array_base_tsn);
	if (gap >= (SCTP_MAPPING_ARRAY << 3)) {

		return (0);
	}
	if (gap >= (uint32_t) (asoc->mapping_array_size << 3)) {
		SCTP_TCB_LOCK_ASSERT(stcb);
		if (sctp_expand_mapping_array(asoc, gap)) {

			return (0);
		}
	}
	if (SCTP_TSN_GT(tsn, *high_tsn)) {
		*high_tsn = tsn;
	}

	if (SCTP_IS_TSN_PRESENT(asoc->mapping_array, gap) ||
	    SCTP_IS_TSN_PRESENT(asoc->nr_mapping_array, gap)) {
		SCTP_STAT_INCR(sctps_recvdupdata);
		if (asoc->numduptsns < SCTP_MAX_DUP_TSNS) {

			asoc->dup_tsns[asoc->numduptsns] = tsn;
			asoc->numduptsns++;
		}
		asoc->send_sack = 1;
		return (0);
	}

	if (((stcb->sctp_ep->sctp_flags & SCTP_PCB_FLAGS_SOCKET_GONE) ||
	     (stcb->sctp_ep->sctp_flags & SCTP_PCB_FLAGS_SOCKET_ALLGONE) ||
	     (stcb->asoc.state & SCTP_STATE_CLOSED_SOCKET))) {

		op_err = sctp_generate_cause(SCTP_CAUSE_OUT_OF_RESC, "");
		sctp_abort_an_association(stcb->sctp_ep, stcb, op_err, false, SCTP_SO_NOT_LOCKED);
		*abort_flag = 1;
		return (0);
	}

	if (sid >= asoc->streamincnt) {
		struct sctp_error_invalid_stream *cause;

		op_err = sctp_get_mbuf_for_msg(sizeof(struct sctp_error_invalid_stream),
		                               0, M_NOWAIT, 1, MT_DATA);
		if (op_err != NULL) {

			SCTP_BUF_RESV_UF(op_err, sizeof(struct sctp_chunkhdr));
			cause = mtod(op_err, struct sctp_error_invalid_stream *);

			SCTP_BUF_LEN(op_err) = sizeof(struct sctp_error_invalid_stream);
			cause->cause.code = htons(SCTP_CAUSE_INVALID_STREAM);
			cause->cause.length = htons(sizeof(struct sctp_error_invalid_stream));
			cause->stream_id = htons(sid);
			cause->reserved = htons(0);
			sctp_queue_op_err(stcb, op_err);
		}
		SCTP_STAT_INCR(sctps_badsid);
		SCTP_TCB_LOCK_ASSERT(stcb);
		SCTP_SET_TSN_PRESENT(asoc->nr_mapping_array, gap);
		if (SCTP_TSN_GT(tsn, asoc->highest_tsn_inside_nr_map)) {
			asoc->highest_tsn_inside_nr_map = tsn;
		}
		if (tsn == (asoc->cumulative_tsn + 1)) {

			asoc->cumulative_tsn = tsn;
		}
		return (0);
	}

	if ((chk_type == SCTP_IDATA) &&
	    ((chk_flags & SCTP_DATA_FIRST_FRAG) == 0) &&
	    (fsn == 0)) {

		SCTP_SNPRINTF(msg, sizeof(msg), "FSN zero for MID=%8.8x, but flags=%2.2x", mid, chk_flags);
		goto err_out;
	}
	control = sctp_find_reasm_entry(&asoc->strmin[sid], mid, ordered, asoc->idata_supported);
	SCTPDBG(SCTP_DEBUG_XXX, "chunk_flags:0x%x look for control on queues %p\n",
		chk_flags, control);
	if ((chk_flags & SCTP_DATA_NOT_FRAG) != SCTP_DATA_NOT_FRAG) {

		if (control != NULL) {

			if (ordered && (mid != control->mid)) {
				SCTP_SNPRINTF(msg, sizeof(msg), "Reassembly problem (MID=%8.8x)", mid);
			err_out:
				op_err = sctp_generate_cause(SCTP_CAUSE_PROTOCOL_VIOLATION, msg);
				stcb->sctp_ep->last_abort_code = SCTP_FROM_SCTP_INDATA + SCTP_LOC_17;
				sctp_abort_an_association(stcb->sctp_ep, stcb, op_err, false, SCTP_SO_NOT_LOCKED);
				*abort_flag = 1;
				return (0);
			}
			if (ordered && ((control->sinfo_flags >> 8) & SCTP_DATA_UNORDERED)) {

				SCTP_SNPRINTF(msg, sizeof(msg),
				              "All fragments of a user message must be ordered or unordered (TSN=%8.8x)",
				              tsn);
				goto err_out;
			}
			if (!ordered && (((control->sinfo_flags >> 8) & SCTP_DATA_UNORDERED) == 0)) {

				SCTP_SNPRINTF(msg, sizeof(msg),
				             "All fragments of a user message must be ordered or unordered (TSN=%8.8x)",
				             tsn);
				goto err_out;
			}
		}
	} else {

		if (control != NULL) {
			if (ordered || asoc->idata_supported) {
				SCTPDBG(SCTP_DEBUG_XXX, "chunk_flags: 0x%x dup detected on MID: %u\n",
					chk_flags, mid);
				SCTP_SNPRINTF(msg, sizeof(msg), "Duplicate MID=%8.8x detected.", mid);
				goto err_out;
			} else {
				if ((tsn == control->fsn_included + 1) &&
				    (control->end_added == 0)) {
					SCTP_SNPRINTF(msg, sizeof(msg),
					              "Illegal message sequence, missing end for MID: %8.8x",
					              control->fsn_included);
					goto err_out;
				} else {
					control = NULL;
				}
			}
		}
	}

	if (((asoc->cnt_on_all_streams +
	      asoc->cnt_on_reasm_queue +
	      asoc->cnt_msg_on_sb) >= SCTP_BASE_SYSCTL(sctp_max_chunks_on_queue)) ||
	    (((int)asoc->my_rwnd) <= 0)) {

		if (stcb->sctp_socket->so_rcv.sb_cc) {

#if defined(__APPLE__) && !defined(__Userspace__)
			struct socket *so;

			so = SCTP_INP_SO(stcb->sctp_ep);
			atomic_add_int(&stcb->asoc.refcnt, 1);
			SCTP_TCB_UNLOCK(stcb);
			SCTP_SOCKET_LOCK(so, 1);
			SCTP_TCB_LOCK(stcb);
			atomic_subtract_int(&stcb->asoc.refcnt, 1);
			if (stcb->asoc.state & SCTP_STATE_CLOSED_SOCKET) {

				SCTP_SOCKET_UNLOCK(so, 1);
				return (0);
			}
#endif
			sctp_sorwakeup(stcb->sctp_ep, stcb->sctp_socket);
#if defined(__APPLE__) && !defined(__Userspace__)
			SCTP_SOCKET_UNLOCK(so, 1);
#endif
		}

		if (chk_type == SCTP_DATA) {
			if (SCTP_TSN_GT(tsn, asoc->highest_tsn_inside_map) &&
			    SCTP_TSN_GT(tsn, asoc->highest_tsn_inside_nr_map)) {

			dump_packet:
				sctp_set_rwnd(stcb, asoc);
				if ((asoc->cnt_on_all_streams +
				     asoc->cnt_on_reasm_queue +
				     asoc->cnt_msg_on_sb) >= SCTP_BASE_SYSCTL(sctp_max_chunks_on_queue)) {
					SCTP_STAT_INCR(sctps_datadropchklmt);
				} else {
					SCTP_STAT_INCR(sctps_datadroprwnd);
				}
				*break_flag = 1;
				return (0);
			}
		} else {
			if (control == NULL) {
				goto dump_packet;
			}
			if (SCTP_TSN_GT(fsn, control->top_fsn)) {
				goto dump_packet;
			}
		}
	}
#ifdef SCTP_ASOCLOG_OF_TSNS
	SCTP_TCB_LOCK_ASSERT(stcb);
	if (asoc->tsn_in_at >= SCTP_TSN_LOG_SIZE) {
		asoc->tsn_in_at = 0;
		asoc->tsn_in_wrapped = 1;
	}
	asoc->in_tsnlog[asoc->tsn_in_at].tsn = tsn;
	asoc->in_tsnlog[asoc->tsn_in_at].strm = sid;
	asoc->in_tsnlog[asoc->tsn_in_at].seq = mid;
	asoc->in_tsnlog[asoc->tsn_in_at].sz = chk_length;
	asoc->in_tsnlog[asoc->tsn_in_at].flgs = chunk_flags;
	asoc->in_tsnlog[asoc->tsn_in_at].stcb = (void *)stcb;
	asoc->in_tsnlog[asoc->tsn_in_at].in_pos = asoc->tsn_in_at;
	asoc->in_tsnlog[asoc->tsn_in_at].in_out = 1;
	asoc->tsn_in_at++;
#endif

	if ((chk_flags & SCTP_DATA_FIRST_FRAG) &&
	    (TAILQ_EMPTY(&asoc->resetHead)) &&
	    (chk_flags & SCTP_DATA_UNORDERED) == 0 &&
	    SCTP_MID_GE(asoc->idata_supported, asoc->strmin[sid].last_mid_delivered, mid)) {

		SCTPDBG(SCTP_DEBUG_INDATA1, "EVIL/Broken-Dup S-SEQ: %u delivered: %u from peer, Abort!\n",
			mid, asoc->strmin[sid].last_mid_delivered);

		if (asoc->idata_supported) {
			SCTP_SNPRINTF(msg, sizeof(msg), "Delivered MID=%8.8x, got TSN=%8.8x, SID=%4.4x, MID=%8.8x",
			              asoc->strmin[sid].last_mid_delivered,
			              tsn,
			              sid,
			              mid);
		} else {
			SCTP_SNPRINTF(msg, sizeof(msg), "Delivered SSN=%4.4x, got TSN=%8.8x, SID=%4.4x, SSN=%4.4x",
			              (uint16_t)asoc->strmin[sid].last_mid_delivered,
			              tsn,
			              sid,
			              (uint16_t)mid);
		}
		op_err = sctp_generate_cause(SCTP_CAUSE_PROTOCOL_VIOLATION, msg);
		stcb->sctp_ep->last_abort_code = SCTP_FROM_SCTP_INDATA + SCTP_LOC_18;
		sctp_abort_an_association(stcb->sctp_ep, stcb, op_err, false, SCTP_SO_NOT_LOCKED);
		*abort_flag = 1;
		return (0);
	}
	if (chk_type == SCTP_IDATA) {
		the_len = (chk_length - sizeof(struct sctp_idata_chunk));
	} else {
		the_len = (chk_length - sizeof(struct sctp_data_chunk));
	}
	if (last_chunk == 0) {
		if (chk_type == SCTP_IDATA) {
			dmbuf = SCTP_M_COPYM(*m,
					     (offset + sizeof(struct sctp_idata_chunk)),
					     the_len, M_NOWAIT);
		} else {
			dmbuf = SCTP_M_COPYM(*m,
					     (offset + sizeof(struct sctp_data_chunk)),
					     the_len, M_NOWAIT);
		}
#ifdef SCTP_MBUF_LOGGING
		if (SCTP_BASE_SYSCTL(sctp_logging_level) & SCTP_MBUF_LOGGING_ENABLE) {
			sctp_log_mbc(dmbuf, SCTP_MBUF_ICOPY);
		}
#endif
	} else {

		int l_len;
		dmbuf = *m;

		if (chk_type == SCTP_IDATA) {
			m_adj(dmbuf, (offset + sizeof(struct sctp_idata_chunk)));
		} else {
			m_adj(dmbuf, (offset + sizeof(struct sctp_data_chunk)));
		}
		if (SCTP_BUF_NEXT(dmbuf) == NULL) {
			l_len = SCTP_BUF_LEN(dmbuf);
		} else {

			struct mbuf *lat;

			l_len = 0;
			for (lat = dmbuf; lat; lat = SCTP_BUF_NEXT(lat)) {
				l_len += SCTP_BUF_LEN(lat);
			}
		}
		if (l_len > the_len) {

			m_adj(dmbuf, -(l_len - the_len));
		}
	}
	if (dmbuf == NULL) {
		SCTP_STAT_INCR(sctps_nomem);
		return (0);
	}

	if (control == NULL) {
		sctp_alloc_a_readq(stcb, control);
		sctp_build_readq_entry_mac(control, stcb, asoc->context, net, tsn,
					   ppid,
					   sid,
					   chk_flags,
					   NULL, fsn, mid);
		if (control == NULL) {
			SCTP_STAT_INCR(sctps_nomem);
			return (0);
		}
		if ((chk_flags & SCTP_DATA_NOT_FRAG) == SCTP_DATA_NOT_FRAG) {
			struct mbuf *mm;

			control->data = dmbuf;
			control->tail_mbuf = NULL;
			for (mm = control->data; mm; mm = mm->m_next) {
				control->length += SCTP_BUF_LEN(mm);
				if (SCTP_BUF_NEXT(mm) == NULL) {
					control->tail_mbuf = mm;
				}
			}
			control->end_added = 1;
			control->last_frag_seen = 1;
			control->first_frag_seen = 1;
			control->fsn_included = fsn;
			control->top_fsn = fsn;
		}
		created_control = 1;
	}
	SCTPDBG(SCTP_DEBUG_XXX, "chunk_flags: 0x%x ordered: %d MID: %u control: %p\n",
		chk_flags, ordered, mid, control);
	if ((chk_flags & SCTP_DATA_NOT_FRAG) == SCTP_DATA_NOT_FRAG &&
	    TAILQ_EMPTY(&asoc->resetHead) &&
	    ((ordered == 0) ||
	     (SCTP_MID_EQ(asoc->idata_supported, asoc->strmin[sid].last_mid_delivered + 1, mid) &&
	      TAILQ_EMPTY(&asoc->strmin[sid].inqueue)))) {

		SCTP_SET_TSN_PRESENT(asoc->nr_mapping_array, gap);
		if (SCTP_TSN_GT(tsn, asoc->highest_tsn_inside_nr_map)) {
			asoc->highest_tsn_inside_nr_map = tsn;
		}
		SCTPDBG(SCTP_DEBUG_XXX, "Injecting control: %p to be read (MID: %u)\n",
			control, mid);

		sctp_add_to_readq(stcb->sctp_ep, stcb,
		                  control, &stcb->sctp_socket->so_rcv,
		                  1, SCTP_READ_LOCK_NOT_HELD, SCTP_SO_NOT_LOCKED);

		if ((chk_flags & SCTP_DATA_UNORDERED) == 0) {

			asoc->strmin[sid].last_mid_delivered++;
		}
		SCTP_STAT_INCR(sctps_recvexpress);
		if (SCTP_BASE_SYSCTL(sctp_logging_level) & SCTP_STR_LOGGING_ENABLE) {
			sctp_log_strm_del_alt(stcb, tsn, mid, sid,
					      SCTP_STR_LOG_FROM_EXPRS_DEL);
		}
		control = NULL;
		goto finish_express_del;
	}

	if ((chk_flags & SCTP_DATA_NOT_FRAG) != SCTP_DATA_NOT_FRAG) {
		sctp_alloc_a_chunk(stcb, chk);
		if (chk == NULL) {

			SCTP_STAT_INCR(sctps_nomem);
			if (last_chunk == 0) {

				sctp_m_freem(dmbuf);
			}
			return (0);
		}
		chk->rec.data.tsn = tsn;
		chk->no_fr_allowed = 0;
		chk->rec.data.fsn = fsn;
		chk->rec.data.mid = mid;
		chk->rec.data.sid = sid;
		chk->rec.data.ppid = ppid;
		chk->rec.data.context = stcb->asoc.context;
		chk->rec.data.doing_fast_retransmit = 0;
		chk->rec.data.rcv_flags = chk_flags;
		chk->asoc = asoc;
		chk->send_size = the_len;
		chk->whoTo = net;
		SCTPDBG(SCTP_DEBUG_XXX, "Building ck: %p for control: %p to be read (MID: %u)\n",
			chk,
			control, mid);
		atomic_add_int(&net->ref_count, 1);
		chk->data = dmbuf;
	}

	if (SCTP_BASE_SYSCTL(sctp_do_drain) == 0) {
		SCTP_SET_TSN_PRESENT(asoc->nr_mapping_array, gap);
		if (SCTP_TSN_GT(tsn, asoc->highest_tsn_inside_nr_map)) {
			asoc->highest_tsn_inside_nr_map = tsn;
		}
	} else {
		SCTP_SET_TSN_PRESENT(asoc->mapping_array, gap);
		if (SCTP_TSN_GT(tsn, asoc->highest_tsn_inside_map)) {
			asoc->highest_tsn_inside_map = tsn;
		}
	}

	if ((chk_flags & SCTP_DATA_NOT_FRAG) == SCTP_DATA_NOT_FRAG) {

		if (((liste = TAILQ_FIRST(&asoc->resetHead)) != NULL) &&
		    SCTP_TSN_GT(tsn, liste->tsn)) {

			if (TAILQ_EMPTY(&asoc->pending_reply_queue)) {

				TAILQ_INSERT_TAIL(&asoc->pending_reply_queue, control, next);
			} else {
				struct sctp_queued_to_read *lcontrol, *nlcontrol;
				unsigned char inserted = 0;
				TAILQ_FOREACH_SAFE(lcontrol, &asoc->pending_reply_queue, next, nlcontrol) {
					if (SCTP_TSN_GT(control->sinfo_tsn, lcontrol->sinfo_tsn)) {
						continue;
					} else {

						TAILQ_INSERT_BEFORE(lcontrol, control, next);
						inserted = 1;
						break;
					}
				}
				if (inserted == 0) {

					TAILQ_INSERT_TAIL(&asoc->pending_reply_queue, control, next);
				}
			}
			goto finish_express_del;
		}
		if (chk_flags & SCTP_DATA_UNORDERED) {

			SCTPDBG(SCTP_DEBUG_XXX, "Unordered data to be read control: %p MID: %u\n",
				control, mid);
			sctp_mark_non_revokable(asoc, control->sinfo_tsn);
			sctp_add_to_readq(stcb->sctp_ep, stcb,
			                  control,
			                  &stcb->sctp_socket->so_rcv, 1,
			                  SCTP_READ_LOCK_NOT_HELD, SCTP_SO_NOT_LOCKED);

		} else {
			SCTPDBG(SCTP_DEBUG_XXX, "Queue control: %p for reordering MID: %u\n", control,
				mid);
			sctp_queue_data_to_stream(stcb, asoc, control, abort_flag, &need_reasm_check);
			if (*abort_flag) {
				if (last_chunk) {
					*m = NULL;
				}
				return (0);
			}
		}
		goto finish_express_del;
	}

	need_reasm_check = 1;
	SCTPDBG(SCTP_DEBUG_XXX,
		"Queue data to stream for reasm control: %p MID: %u\n",
		control, mid);
	sctp_queue_data_for_reasm(stcb, asoc, control, chk, created_control, abort_flag, tsn);
	if (*abort_flag) {

		if (last_chunk) {
			*m = NULL;
		}
		return (0);
	}
finish_express_del:

	if (tsn == (asoc->cumulative_tsn + 1)) {

		asoc->cumulative_tsn = tsn;
	}
	if (last_chunk) {
		*m = NULL;
	}
	if (ordered) {
		SCTP_STAT_INCR_COUNTER64(sctps_inorderchunks);
	} else {
		SCTP_STAT_INCR_COUNTER64(sctps_inunorderchunks);
	}
	SCTP_STAT_INCR(sctps_recvdata);

	if (SCTP_BASE_SYSCTL(sctp_logging_level) & SCTP_STR_LOGGING_ENABLE) {
		sctp_log_strm_del_alt(stcb, tsn, mid, sid, SCTP_STR_LOG_FROM_MARK_TSN);
	}
	if (SCTP_BASE_SYSCTL(sctp_logging_level) & SCTP_MAP_LOGGING_ENABLE) {
		sctp_log_map(asoc->mapping_array_base_tsn, asoc->cumulative_tsn,
			     asoc->highest_tsn_inside_map, SCTP_MAP_PREPARE_SLIDE);
	}
	if (need_reasm_check) {
		(void)sctp_deliver_reasm_check(stcb, asoc, &asoc->strmin[sid], SCTP_READ_LOCK_NOT_HELD);
		need_reasm_check = 0;
	}

	if (((liste = TAILQ_FIRST(&asoc->resetHead)) != NULL) &&
	    SCTP_TSN_GE(asoc->cumulative_tsn, liste->tsn)) {

		sctp_reset_in_stream(stcb, liste->number_entries, liste->list_of_streams);
		TAILQ_REMOVE(&asoc->resetHead, liste, next_resp);
		sctp_send_deferred_reset_response(stcb, liste, SCTP_STREAM_RESET_RESULT_PERFORMED);
		SCTP_FREE(liste, SCTP_M_STRESET);

		liste = TAILQ_FIRST(&asoc->resetHead);
		if (TAILQ_EMPTY(&asoc->resetHead)) {

			TAILQ_FOREACH_SAFE(control, &asoc->pending_reply_queue, next, ncontrol) {
				TAILQ_REMOVE(&asoc->pending_reply_queue, control, next);
				strm = &asoc->strmin[control->sinfo_stream];
				sctp_queue_data_to_stream(stcb, asoc, control, abort_flag, &need_reasm_check);
				if (*abort_flag) {
					return (0);
				}
				if (need_reasm_check) {
					(void)sctp_deliver_reasm_check(stcb, asoc, strm, SCTP_READ_LOCK_NOT_HELD);
					need_reasm_check = 0;
				}
			}
		} else {
			TAILQ_FOREACH_SAFE(control, &asoc->pending_reply_queue, next, ncontrol) {
				if (SCTP_TSN_GT(control->sinfo_tsn, liste->tsn)) {
					break;
				}

				TAILQ_REMOVE(&asoc->pending_reply_queue, control, next);
				strm = &asoc->strmin[control->sinfo_stream];
				sctp_queue_data_to_stream(stcb, asoc, control, abort_flag, &need_reasm_check);
				if (*abort_flag) {
					return (0);
				}
				if (need_reasm_check) {
					(void)sctp_deliver_reasm_check(stcb, asoc, strm, SCTP_READ_LOCK_NOT_HELD);
					need_reasm_check = 0;
				}
			}
		}
	}
	return (1);
}

static const int8_t sctp_map_lookup_tab[256] = {
  0, 1, 0, 2, 0, 1, 0, 3,
  0, 1, 0, 2, 0, 1, 0, 4,
  0, 1, 0, 2, 0, 1, 0, 3,
  0, 1, 0, 2, 0, 1, 0, 5,
  0, 1, 0, 2, 0, 1, 0, 3,
  0, 1, 0, 2, 0, 1, 0, 4,
  0, 1, 0, 2, 0, 1, 0, 3,
  0, 1, 0, 2, 0, 1, 0, 6,
  0, 1, 0, 2, 0, 1, 0, 3,
  0, 1, 0, 2, 0, 1, 0, 4,
  0, 1, 0, 2, 0, 1, 0, 3,
  0, 1, 0, 2, 0, 1, 0, 5,
  0, 1, 0, 2, 0, 1, 0, 3,
  0, 1, 0, 2, 0, 1, 0, 4,
  0, 1, 0, 2, 0, 1, 0, 3,
  0, 1, 0, 2, 0, 1, 0, 7,
  0, 1, 0, 2, 0, 1, 0, 3,
  0, 1, 0, 2, 0, 1, 0, 4,
  0, 1, 0, 2, 0, 1, 0, 3,
  0, 1, 0, 2, 0, 1, 0, 5,
  0, 1, 0, 2, 0, 1, 0, 3,
  0, 1, 0, 2, 0, 1, 0, 4,
  0, 1, 0, 2, 0, 1, 0, 3,
  0, 1, 0, 2, 0, 1, 0, 6,
  0, 1, 0, 2, 0, 1, 0, 3,
  0, 1, 0, 2, 0, 1, 0, 4,
  0, 1, 0, 2, 0, 1, 0, 3,
  0, 1, 0, 2, 0, 1, 0, 5,
  0, 1, 0, 2, 0, 1, 0, 3,
  0, 1, 0, 2, 0, 1, 0, 4,
  0, 1, 0, 2, 0, 1, 0, 3,
  0, 1, 0, 2, 0, 1, 0, 8
};

void
sctp_slide_mapping_arrays(struct sctp_tcb *stcb)
{

	struct sctp_association *asoc;
	int at;
	uint8_t val;
	int slide_from, slide_end, lgap, distance;
	uint32_t old_cumack, old_base, old_highest, highest_tsn;

	asoc = &stcb->asoc;

	old_cumack = asoc->cumulative_tsn;
	old_base = asoc->mapping_array_base_tsn;
	old_highest = asoc->highest_tsn_inside_map;

	at = 0;
	for (slide_from = 0; slide_from < stcb->asoc.mapping_array_size; slide_from++) {
		val = asoc->nr_mapping_array[slide_from] | asoc->mapping_array[slide_from];
		if (val == 0xff) {
			at += 8;
		} else {

			at += sctp_map_lookup_tab[val];
			break;
		}
	}
	asoc->cumulative_tsn = asoc->mapping_array_base_tsn + (at-1);

	if (SCTP_TSN_GT(asoc->cumulative_tsn, asoc->highest_tsn_inside_map) &&
            SCTP_TSN_GT(asoc->cumulative_tsn, asoc->highest_tsn_inside_nr_map)) {
#ifdef INVARIANTS
		panic("huh, cumack 0x%x greater than high-tsn 0x%x in map",
		      asoc->cumulative_tsn, asoc->highest_tsn_inside_map);
#else
		SCTP_PRINTF("huh, cumack 0x%x greater than high-tsn 0x%x in map - should panic?\n",
			    asoc->cumulative_tsn, asoc->highest_tsn_inside_map);
		sctp_print_mapping_array(asoc);
		if (SCTP_BASE_SYSCTL(sctp_logging_level) & SCTP_MAP_LOGGING_ENABLE) {
			sctp_log_map(0, 6, asoc->highest_tsn_inside_map, SCTP_MAP_SLIDE_RESULT);
		}
		asoc->highest_tsn_inside_map = asoc->cumulative_tsn;
		asoc->highest_tsn_inside_nr_map = asoc->cumulative_tsn;
#endif
	}
	if (SCTP_TSN_GT(asoc->highest_tsn_inside_nr_map, asoc->highest_tsn_inside_map)) {
		highest_tsn = asoc->highest_tsn_inside_nr_map;
	} else {
		highest_tsn = asoc->highest_tsn_inside_map;
	}
	if ((asoc->cumulative_tsn == highest_tsn) && (at >= 8)) {

		int clr;
#ifdef INVARIANTS
		unsigned int i;
#endif

		clr = ((at+7) >> 3);
		if (clr > asoc->mapping_array_size) {
			clr = asoc->mapping_array_size;
		}
		memset(asoc->mapping_array, 0, clr);
		memset(asoc->nr_mapping_array, 0, clr);
#ifdef INVARIANTS
		for (i = 0; i < asoc->mapping_array_size; i++) {
			if ((asoc->mapping_array[i]) || (asoc->nr_mapping_array[i])) {
				SCTP_PRINTF("Error Mapping array's not clean at clear\n");
				sctp_print_mapping_array(asoc);
			}
		}
#endif
		asoc->mapping_array_base_tsn = asoc->cumulative_tsn + 1;
		asoc->highest_tsn_inside_nr_map = asoc->highest_tsn_inside_map = asoc->cumulative_tsn;
	} else if (at >= 8) {

		SCTP_CALC_TSN_TO_GAP(lgap, highest_tsn, asoc->mapping_array_base_tsn);
		slide_end = (lgap >> 3);
		if (slide_end < slide_from) {
			sctp_print_mapping_array(asoc);
#ifdef INVARIANTS
			panic("impossible slide");
#else
			SCTP_PRINTF("impossible slide lgap: %x slide_end: %x slide_from: %x? at: %d\n",
			            lgap, slide_end, slide_from, at);
			return;
#endif
		}
		if (slide_end > asoc->mapping_array_size) {
#ifdef INVARIANTS
			panic("would overrun buffer");
#else
			SCTP_PRINTF("Gak, would have overrun map end: %d slide_end: %d\n",
			            asoc->mapping_array_size, slide_end);
			slide_end = asoc->mapping_array_size;
#endif
		}
		distance = (slide_end - slide_from) + 1;
		if (SCTP_BASE_SYSCTL(sctp_logging_level) & SCTP_MAP_LOGGING_ENABLE) {
			sctp_log_map(old_base, old_cumack, old_highest,
				     SCTP_MAP_PREPARE_SLIDE);
			sctp_log_map((uint32_t) slide_from, (uint32_t) slide_end,
				     (uint32_t) lgap, SCTP_MAP_SLIDE_FROM);
		}
		if (distance + slide_from > asoc->mapping_array_size ||
		    distance < 0) {

			if (SCTP_BASE_SYSCTL(sctp_logging_level) & SCTP_MAP_LOGGING_ENABLE) {
				sctp_log_map((uint32_t) distance, (uint32_t) slide_from,
					     (uint32_t) asoc->mapping_array_size,
					     SCTP_MAP_SLIDE_NONE);
			}
		} else {
			int ii;

			for (ii = 0; ii < distance; ii++) {
				asoc->mapping_array[ii] = asoc->mapping_array[slide_from + ii];
				asoc->nr_mapping_array[ii] = asoc->nr_mapping_array[slide_from + ii];
			}
			for (ii = distance; ii < asoc->mapping_array_size; ii++) {
				asoc->mapping_array[ii] = 0;
				asoc->nr_mapping_array[ii] = 0;
			}
			if (asoc->highest_tsn_inside_map + 1 == asoc->mapping_array_base_tsn) {
				asoc->highest_tsn_inside_map += (slide_from << 3);
			}
			if (asoc->highest_tsn_inside_nr_map + 1 == asoc->mapping_array_base_tsn) {
				asoc->highest_tsn_inside_nr_map += (slide_from << 3);
			}
			asoc->mapping_array_base_tsn += (slide_from << 3);
			if (SCTP_BASE_SYSCTL(sctp_logging_level) & SCTP_MAP_LOGGING_ENABLE) {
				sctp_log_map(asoc->mapping_array_base_tsn,
					     asoc->cumulative_tsn, asoc->highest_tsn_inside_map,
					     SCTP_MAP_SLIDE_RESULT);
			}
		}
	}
}

void
sctp_sack_check(struct sctp_tcb *stcb, int was_a_gap)
{
	struct sctp_association *asoc;
	uint32_t highest_tsn;
	int is_a_gap;

	sctp_slide_mapping_arrays(stcb);
	asoc = &stcb->asoc;
	if (SCTP_TSN_GT(asoc->highest_tsn_inside_nr_map, asoc->highest_tsn_inside_map)) {
		highest_tsn = asoc->highest_tsn_inside_nr_map;
	} else {
		highest_tsn = asoc->highest_tsn_inside_map;
	}

	is_a_gap = SCTP_TSN_GT(highest_tsn, stcb->asoc.cumulative_tsn);

	if (SCTP_GET_STATE(stcb) == SCTP_STATE_SHUTDOWN_SENT) {

		if (SCTP_OS_TIMER_PENDING(&stcb->asoc.dack_timer.timer)) {
			sctp_timer_stop(SCTP_TIMER_TYPE_RECV,
			                stcb->sctp_ep, stcb, NULL,
			                SCTP_FROM_SCTP_INDATA + SCTP_LOC_19);
		}
		sctp_send_shutdown(stcb,
		                   ((stcb->asoc.alternate) ? stcb->asoc.alternate : stcb->asoc.primary_destination));
		if (is_a_gap) {
			sctp_send_sack(stcb, SCTP_SO_NOT_LOCKED);
		}
	} else {

		stcb->asoc.cmt_dac_pkts_rcvd++;

		if ((stcb->asoc.send_sack == 1) ||
		    ((was_a_gap) && (is_a_gap == 0)) ||

		    (stcb->asoc.numduptsns) ||
		    (is_a_gap) ||
		    (stcb->asoc.delayed_ack == 0) ||
		    (stcb->asoc.data_pkts_seen >= stcb->asoc.sack_freq)) {
			if ((stcb->asoc.sctp_cmt_on_off > 0) &&
			    (SCTP_BASE_SYSCTL(sctp_cmt_use_dac)) &&
			    (stcb->asoc.send_sack == 0) &&
			    (stcb->asoc.numduptsns == 0) &&
			    (stcb->asoc.delayed_ack) &&
			    (!SCTP_OS_TIMER_PENDING(&stcb->asoc.dack_timer.timer))) {

				sctp_timer_start(SCTP_TIMER_TYPE_RECV,
				                 stcb->sctp_ep, stcb, NULL);
			} else {

				sctp_timer_stop(SCTP_TIMER_TYPE_RECV, stcb->sctp_ep, stcb, NULL,
				                SCTP_FROM_SCTP_INDATA + SCTP_LOC_20);
				sctp_send_sack(stcb, SCTP_SO_NOT_LOCKED);
			}
		} else {
			if (!SCTP_OS_TIMER_PENDING(&stcb->asoc.dack_timer.timer)) {
				sctp_timer_start(SCTP_TIMER_TYPE_RECV,
				                 stcb->sctp_ep, stcb, NULL);
			}
		}
	}
}

int
sctp_process_data(struct mbuf **mm, int iphlen, int *offset, int length,
                  struct sctp_inpcb *inp, struct sctp_tcb *stcb,
                  struct sctp_nets *net, uint32_t *high_tsn)
{
	struct sctp_chunkhdr *ch, chunk_buf;
	struct sctp_association *asoc;
	int num_chunks = 0;
	int stop_proc = 0;
	int break_flag, last_chunk;
	int abort_flag = 0, was_a_gap;
	struct mbuf *m;
	uint32_t highest_tsn;
	uint16_t chk_length;

	sctp_set_rwnd(stcb, &stcb->asoc);

	m = *mm;
	SCTP_TCB_LOCK_ASSERT(stcb);
	asoc = &stcb->asoc;
	if (SCTP_TSN_GT(asoc->highest_tsn_inside_nr_map, asoc->highest_tsn_inside_map)) {
		highest_tsn = asoc->highest_tsn_inside_nr_map;
	} else {
		highest_tsn = asoc->highest_tsn_inside_map;
	}
	was_a_gap = SCTP_TSN_GT(highest_tsn, stcb->asoc.cumulative_tsn);

	asoc->last_data_chunk_from = net;

	if (SCTP_BUF_LEN(m) < (long)MLEN && SCTP_BUF_NEXT(m) == NULL) {

		m = sctp_get_mbuf_for_msg(SCTP_BUF_LEN(m), 0, M_NOWAIT, 1, MT_DATA);
		if (m) {

			caddr_t *from, *to;

			to = mtod(m, caddr_t *);
			from = mtod((*mm), caddr_t *);
			memcpy(to, from, SCTP_BUF_LEN((*mm)));

			SCTP_BUF_LEN(m) = SCTP_BUF_LEN((*mm));
			sctp_m_freem(*mm);

			*mm = m;
		} else {

			m = *mm;
		}
	}

	ch = (struct sctp_chunkhdr *)sctp_m_getptr(m, *offset,
	                                           sizeof(struct sctp_chunkhdr),
	                                           (uint8_t *)&chunk_buf);
	if (ch == NULL) {
		return (1);
	}

	*high_tsn = asoc->cumulative_tsn;
	break_flag = 0;
	asoc->data_pkts_seen++;
	while (stop_proc == 0) {

		chk_length = ntohs(ch->chunk_length);
		if (length - *offset < chk_length) {

			stop_proc = 1;
			continue;
		}
		if ((asoc->idata_supported == 1) &&
		    (ch->chunk_type == SCTP_DATA)) {
			struct mbuf *op_err;
			char msg[SCTP_DIAG_INFO_LEN];

			SCTP_SNPRINTF(msg, sizeof(msg), "%s", "DATA chunk received when I-DATA was negotiated");
			op_err = sctp_generate_cause(SCTP_CAUSE_PROTOCOL_VIOLATION, msg);
			stcb->sctp_ep->last_abort_code = SCTP_FROM_SCTP_INDATA + SCTP_LOC_21;
			sctp_abort_an_association(inp, stcb, op_err, false, SCTP_SO_NOT_LOCKED);
			return (2);
		}
		if ((asoc->idata_supported == 0) &&
		    (ch->chunk_type == SCTP_IDATA)) {
			struct mbuf *op_err;
			char msg[SCTP_DIAG_INFO_LEN];

			SCTP_SNPRINTF(msg, sizeof(msg), "%s", "I-DATA chunk received when DATA was negotiated");
			op_err = sctp_generate_cause(SCTP_CAUSE_PROTOCOL_VIOLATION, msg);
			stcb->sctp_ep->last_abort_code = SCTP_FROM_SCTP_INDATA + SCTP_LOC_22;
			sctp_abort_an_association(inp, stcb, op_err, false, SCTP_SO_NOT_LOCKED);
			return (2);
		}
		if ((ch->chunk_type == SCTP_DATA) ||
		    (ch->chunk_type == SCTP_IDATA)) {
			uint16_t clen;

			if (ch->chunk_type == SCTP_DATA) {
				clen = sizeof(struct sctp_data_chunk);
			} else {
				clen = sizeof(struct sctp_idata_chunk);
			}
			if (chk_length < clen) {

				struct mbuf *op_err;
				char msg[SCTP_DIAG_INFO_LEN];

				SCTP_SNPRINTF(msg, sizeof(msg), "%s chunk of length %u",
				              ch->chunk_type == SCTP_DATA ? "DATA" : "I-DATA",
				              chk_length);
				op_err = sctp_generate_cause(SCTP_CAUSE_PROTOCOL_VIOLATION, msg);
				stcb->sctp_ep->last_abort_code = SCTP_FROM_SCTP_INDATA + SCTP_LOC_23;
				sctp_abort_an_association(inp, stcb, op_err, false, SCTP_SO_NOT_LOCKED);
				return (2);
			}
#ifdef SCTP_AUDITING_ENABLED
			sctp_audit_log(0xB1, 0);
#endif
			if (SCTP_SIZE32(chk_length) == (length - *offset)) {
				last_chunk = 1;
			} else {
				last_chunk = 0;
			}
			if (sctp_process_a_data_chunk(stcb, asoc, mm, *offset,
						      chk_length, net, high_tsn, &abort_flag, &break_flag,
						      last_chunk, ch->chunk_type)) {
				num_chunks++;
			}
			if (abort_flag)
				return (2);

			if (break_flag) {

				stop_proc = 1;
				continue;
			}
		} else {

			switch (ch->chunk_type) {
			case SCTP_INITIATION:
			case SCTP_INITIATION_ACK:
			case SCTP_SELECTIVE_ACK:
			case SCTP_NR_SELECTIVE_ACK:
			case SCTP_HEARTBEAT_REQUEST:
			case SCTP_HEARTBEAT_ACK:
			case SCTP_ABORT_ASSOCIATION:
			case SCTP_SHUTDOWN:
			case SCTP_SHUTDOWN_ACK:
			case SCTP_OPERATION_ERROR:
			case SCTP_COOKIE_ECHO:
			case SCTP_COOKIE_ACK:
			case SCTP_ECN_ECHO:
			case SCTP_ECN_CWR:
			case SCTP_SHUTDOWN_COMPLETE:
			case SCTP_AUTHENTICATION:
			case SCTP_ASCONF_ACK:
			case SCTP_PACKET_DROPPED:
			case SCTP_STREAM_RESET:
			case SCTP_FORWARD_CUM_TSN:
			case SCTP_ASCONF:
			{

				struct mbuf *op_err;
				char msg[SCTP_DIAG_INFO_LEN];

				SCTP_SNPRINTF(msg, sizeof(msg), "DATA chunk followed by chunk of type %2.2x",
				              ch->chunk_type);
				op_err = sctp_generate_cause(SCTP_CAUSE_PROTOCOL_VIOLATION, msg);
				sctp_abort_an_association(inp, stcb, op_err, false, SCTP_SO_NOT_LOCKED);
				return (2);
			}
			default:

				if (chk_length < sizeof(struct sctp_chunkhdr)) {

					struct mbuf *op_err;
					char msg[SCTP_DIAG_INFO_LEN];

					SCTP_SNPRINTF(msg, sizeof(msg), "Chunk of length %u", chk_length);
					op_err = sctp_generate_cause(SCTP_CAUSE_PROTOCOL_VIOLATION, msg);
					stcb->sctp_ep->last_abort_code = SCTP_FROM_SCTP_INDATA + SCTP_LOC_24;
					sctp_abort_an_association(inp, stcb, op_err, false, SCTP_SO_NOT_LOCKED);
					return (2);
				}
				if (ch->chunk_type & 0x40) {

					struct mbuf *op_err;
					struct sctp_gen_error_cause *cause;

					op_err = sctp_get_mbuf_for_msg(sizeof(struct sctp_gen_error_cause),
					                               0, M_NOWAIT, 1, MT_DATA);
					if (op_err != NULL) {
						cause  = mtod(op_err, struct sctp_gen_error_cause *);
						cause->code = htons(SCTP_CAUSE_UNRECOG_CHUNK);
						cause->length = htons((uint16_t)(chk_length + sizeof(struct sctp_gen_error_cause)));
						SCTP_BUF_LEN(op_err) = sizeof(struct sctp_gen_error_cause);
						SCTP_BUF_NEXT(op_err) = SCTP_M_COPYM(m, *offset, chk_length, M_NOWAIT);
						if (SCTP_BUF_NEXT(op_err) != NULL) {
							sctp_queue_op_err(stcb, op_err);
						} else {
							sctp_m_freem(op_err);
						}
					}
				}
				if ((ch->chunk_type & 0x80) == 0) {

					stop_proc = 1;
				}

				break;
			}
		}
		*offset += SCTP_SIZE32(chk_length);
		if ((*offset >= length) || stop_proc) {

			stop_proc = 1;
			continue;
		}
		ch = (struct sctp_chunkhdr *)sctp_m_getptr(m, *offset,
		                                           sizeof(struct sctp_chunkhdr),
		                                           (uint8_t *)&chunk_buf);
		if (ch == NULL) {
			*offset = length;
			stop_proc = 1;
			continue;
		}
	}
	if (break_flag) {

		sctp_send_packet_dropped(stcb, net, *mm, length, iphlen, 0);
	}
	if (num_chunks) {

		SCTP_STAT_INCR(sctps_recvpktwithdata);
		if (SCTP_BASE_SYSCTL(sctp_logging_level) & SCTP_THRESHOLD_LOGGING) {
			sctp_misc_ints(SCTP_THRESHOLD_CLEAR,
				       stcb->asoc.overall_error_count,
				       0,
				       SCTP_FROM_SCTP_INDATA,
				       __LINE__);
		}
		stcb->asoc.overall_error_count = 0;
		(void)SCTP_GETTIME_TIMEVAL(&stcb->asoc.time_last_rcvd);
	}

	if (SCTP_GET_STATE(stcb) == SCTP_STATE_SHUTDOWN_SENT) {

		stcb->asoc.send_sack = 1;
	}

	sctp_sack_check(stcb, was_a_gap);
	return (0);
}

static int
sctp_process_segment_range(struct sctp_tcb *stcb, struct sctp_tmit_chunk **p_tp1, uint32_t last_tsn,
			   uint16_t frag_strt, uint16_t frag_end, int nr_sacking,
			   int *num_frs,
			   uint32_t *biggest_newly_acked_tsn,
			   uint32_t  *this_sack_lowest_newack,
			   int *rto_ok)
{
	struct sctp_tmit_chunk *tp1;
	unsigned int theTSN;
	int j, wake_him = 0, circled = 0;

	tp1 = *p_tp1;
	if (tp1 == NULL) {
		tp1 = TAILQ_FIRST(&stcb->asoc.sent_queue);
	}
	for (j = frag_strt; j <= frag_end; j++) {
		theTSN = j + last_tsn;
		while (tp1) {
			if (tp1->rec.data.doing_fast_retransmit)
				(*num_frs) += 1;

			if ((tp1->sent < SCTP_DATAGRAM_RESEND) &&
			    (tp1->whoTo->find_pseudo_cumack == 1) &&
			    (tp1->snd_count == 1)) {
				tp1->whoTo->pseudo_cumack = tp1->rec.data.tsn;
				tp1->whoTo->find_pseudo_cumack = 0;
			}
			if ((tp1->sent < SCTP_DATAGRAM_RESEND) &&
			    (tp1->whoTo->find_rtx_pseudo_cumack == 1) &&
			    (tp1->snd_count > 1)) {
				tp1->whoTo->rtx_pseudo_cumack = tp1->rec.data.tsn;
				tp1->whoTo->find_rtx_pseudo_cumack = 0;
			}
			if (tp1->rec.data.tsn == theTSN) {
				if (tp1->sent != SCTP_DATAGRAM_UNSENT) {

					if (tp1->sent < SCTP_DATAGRAM_RESEND) {

						if (SCTP_TSN_GT(tp1->rec.data.tsn,
						                *biggest_newly_acked_tsn)) {
							*biggest_newly_acked_tsn = tp1->rec.data.tsn;
						}

						if (tp1->rec.data.chunk_was_revoked == 0)
							tp1->whoTo->saw_newack = 1;

						if (SCTP_TSN_GT(tp1->rec.data.tsn,
						                tp1->whoTo->this_sack_highest_newack)) {
							tp1->whoTo->this_sack_highest_newack =
								tp1->rec.data.tsn;
						}

						if (*this_sack_lowest_newack == 0) {
							if (SCTP_BASE_SYSCTL(sctp_logging_level) & SCTP_SACK_LOGGING_ENABLE) {
								sctp_log_sack(*this_sack_lowest_newack,
									      last_tsn,
									      tp1->rec.data.tsn,
									      0,
									      0,
									      SCTP_LOG_TSN_ACKED);
							}
							*this_sack_lowest_newack = tp1->rec.data.tsn;
						}

						if (tp1->rec.data.tsn == tp1->whoTo->pseudo_cumack) {
							if (tp1->rec.data.chunk_was_revoked == 0) {
								tp1->whoTo->new_pseudo_cumack = 1;
							}
							tp1->whoTo->find_pseudo_cumack = 1;
						}
						if (SCTP_BASE_SYSCTL(sctp_logging_level) & SCTP_CWND_LOGGING_ENABLE) {
							sctp_log_cwnd(stcb, tp1->whoTo, tp1->rec.data.tsn, SCTP_CWND_LOG_FROM_SACK);
						}
						if (tp1->rec.data.tsn == tp1->whoTo->rtx_pseudo_cumack) {
							if (tp1->rec.data.chunk_was_revoked == 0) {
								tp1->whoTo->new_pseudo_cumack = 1;
							}
							tp1->whoTo->find_rtx_pseudo_cumack = 1;
						}
						if (SCTP_BASE_SYSCTL(sctp_logging_level) & SCTP_SACK_LOGGING_ENABLE) {
							sctp_log_sack(*biggest_newly_acked_tsn,
								      last_tsn,
								      tp1->rec.data.tsn,
								      frag_strt,
								      frag_end,
								      SCTP_LOG_TSN_ACKED);
						}
						if (SCTP_BASE_SYSCTL(sctp_logging_level) & SCTP_FLIGHT_LOGGING_ENABLE) {
							sctp_misc_ints(SCTP_FLIGHT_LOG_DOWN_GAP,
								       tp1->whoTo->flight_size,
								       tp1->book_size,
								       (uint32_t)(uintptr_t)tp1->whoTo,
								       tp1->rec.data.tsn);
						}
						sctp_flight_size_decrease(tp1);
						if (stcb->asoc.cc_functions.sctp_cwnd_update_tsn_acknowledged) {
							(*stcb->asoc.cc_functions.sctp_cwnd_update_tsn_acknowledged)(tp1->whoTo,
														     tp1);
						}
						sctp_total_flight_decrease(stcb, tp1);

						tp1->whoTo->net_ack += tp1->send_size;
						if (tp1->snd_count < 2) {

							tp1->whoTo->net_ack2 += tp1->send_size;

							if (tp1->do_rtt) {
								if (*rto_ok &&
								    sctp_calculate_rto(stcb,
								                       &stcb->asoc,
								                       tp1->whoTo,
								                       &tp1->sent_rcv_time,
								                       SCTP_RTT_FROM_DATA)) {
									*rto_ok = 0;
								}
								if (tp1->whoTo->rto_needed == 0) {
									tp1->whoTo->rto_needed = 1;
								}
								tp1->do_rtt = 0;
							}
						}
					}
					if (tp1->sent <= SCTP_DATAGRAM_RESEND) {
						if (SCTP_TSN_GT(tp1->rec.data.tsn,
						                stcb->asoc.this_sack_highest_gap)) {
							stcb->asoc.this_sack_highest_gap =
								tp1->rec.data.tsn;
						}
						if (tp1->sent == SCTP_DATAGRAM_RESEND) {
							sctp_ucount_decr(stcb->asoc.sent_queue_retran_cnt);
#ifdef SCTP_AUDITING_ENABLED
							sctp_audit_log(0xB2,
								       (stcb->asoc.sent_queue_retran_cnt & 0x000000ff));
#endif
						}
					}

					if ((tp1->sent != SCTP_FORWARD_TSN_SKIP) &&
					    (tp1->sent != SCTP_DATAGRAM_NR_ACKED)) {
						tp1->sent = SCTP_DATAGRAM_MARKED;
					}
					if (tp1->rec.data.chunk_was_revoked) {

						tp1->whoTo->cwnd -= tp1->book_size;
						tp1->rec.data.chunk_was_revoked = 0;
					}

					if (nr_sacking &&
					    (tp1->sent != SCTP_DATAGRAM_NR_ACKED)) {
						if (stcb->asoc.strmout[tp1->rec.data.sid].chunks_on_queues > 0) {
							stcb->asoc.strmout[tp1->rec.data.sid].chunks_on_queues--;
#ifdef INVARIANTS
						} else {
							panic("No chunks on the queues for sid %u.", tp1->rec.data.sid);
#endif
						}
						if ((stcb->asoc.strmout[tp1->rec.data.sid].chunks_on_queues == 0) &&
						    (stcb->asoc.strmout[tp1->rec.data.sid].state == SCTP_STREAM_RESET_PENDING) &&
						    TAILQ_EMPTY(&stcb->asoc.strmout[tp1->rec.data.sid].outqueue)) {
							stcb->asoc.trigger_reset = 1;
						}
						tp1->sent = SCTP_DATAGRAM_NR_ACKED;
						if (tp1->data) {

							sctp_free_bufspace(stcb, &stcb->asoc, tp1, 1);
							sctp_m_freem(tp1->data);
							tp1->data = NULL;
						}
						wake_him++;
					}
				}
				break;
			}
			if (SCTP_TSN_GT(tp1->rec.data.tsn, theTSN)) {
				break;
			}
			tp1 = TAILQ_NEXT(tp1, sctp_next);
			if ((tp1 == NULL) && (circled == 0)) {
				circled++;
				tp1 = TAILQ_FIRST(&stcb->asoc.sent_queue);
			}
		}
		if (tp1 == NULL) {
			circled = 0;
			tp1 = TAILQ_FIRST(&stcb->asoc.sent_queue);
		}

	}
	*p_tp1 = tp1;
	return (wake_him);
}

static int
sctp_handle_segments(struct mbuf *m, int *offset, struct sctp_tcb *stcb, struct sctp_association *asoc,
		uint32_t last_tsn, uint32_t *biggest_tsn_acked,
		uint32_t *biggest_newly_acked_tsn, uint32_t *this_sack_lowest_newack,
		int num_seg, int num_nr_seg, int *rto_ok)
{
	struct sctp_gap_ack_block *frag, block;
	struct sctp_tmit_chunk *tp1;
	int i;
	int num_frs = 0;
	int chunk_freed;
	int non_revocable;
	uint16_t frag_strt, frag_end, prev_frag_end;

	tp1 = TAILQ_FIRST(&asoc->sent_queue);
	prev_frag_end = 0;
	chunk_freed = 0;

	for (i = 0; i < (num_seg + num_nr_seg); i++) {
		if (i == num_seg) {
			prev_frag_end = 0;
			tp1 = TAILQ_FIRST(&asoc->sent_queue);
		}
		frag = (struct sctp_gap_ack_block *)sctp_m_getptr(m, *offset,
		                                                  sizeof(struct sctp_gap_ack_block), (uint8_t *) &block);
		*offset += sizeof(block);
		if (frag == NULL) {
			return (chunk_freed);
		}
		frag_strt = ntohs(frag->start);
		frag_end = ntohs(frag->end);

		if (frag_strt > frag_end) {

			continue;
		}
		if (frag_strt <= prev_frag_end) {

			 tp1 = TAILQ_FIRST(&asoc->sent_queue);
		}
		if (SCTP_TSN_GT((last_tsn + frag_end), *biggest_tsn_acked)) {
			*biggest_tsn_acked = last_tsn + frag_end;
		}
		if (i < num_seg) {
			non_revocable = 0;
		} else {
			non_revocable = 1;
		}
		if (sctp_process_segment_range(stcb, &tp1, last_tsn, frag_strt, frag_end,
		                               non_revocable, &num_frs, biggest_newly_acked_tsn,
		                               this_sack_lowest_newack, rto_ok)) {
			chunk_freed = 1;
		}
		prev_frag_end = frag_end;
	}
	if (SCTP_BASE_SYSCTL(sctp_logging_level) & SCTP_FR_LOGGING_ENABLE) {
		if (num_frs)
			sctp_log_fr(*biggest_tsn_acked,
			            *biggest_newly_acked_tsn,
			            last_tsn, SCTP_FR_LOG_BIGGEST_TSNS);
	}
	return (chunk_freed);
}

static void
sctp_check_for_revoked(struct sctp_tcb *stcb,
		       struct sctp_association *asoc, uint32_t cumack,
		       uint32_t biggest_tsn_acked)
{
	struct sctp_tmit_chunk *tp1;

	TAILQ_FOREACH(tp1, &asoc->sent_queue, sctp_next) {
		if (SCTP_TSN_GT(tp1->rec.data.tsn, cumack)) {

			if (SCTP_TSN_GT(tp1->rec.data.tsn, biggest_tsn_acked)) {
				break;
			}
			if (tp1->sent == SCTP_DATAGRAM_ACKED) {

				tp1->sent = SCTP_DATAGRAM_SENT;
				tp1->rec.data.chunk_was_revoked = 1;

				if (SCTP_BASE_SYSCTL(sctp_logging_level) & SCTP_FLIGHT_LOGGING_ENABLE) {
					sctp_misc_ints(SCTP_FLIGHT_LOG_UP_REVOKE,
						       tp1->whoTo->flight_size,
						       tp1->book_size,
						       (uint32_t)(uintptr_t)tp1->whoTo,
						       tp1->rec.data.tsn);
				}
				sctp_flight_size_increase(tp1);
				sctp_total_flight_increase(stcb, tp1);

				tp1->whoTo->cwnd += tp1->book_size;
				if (SCTP_BASE_SYSCTL(sctp_logging_level) & SCTP_SACK_LOGGING_ENABLE) {
					sctp_log_sack(asoc->last_acked_seq,
						      cumack,
						      tp1->rec.data.tsn,
						      0,
						      0,
						      SCTP_LOG_TSN_REVOKED);
				}
			} else if (tp1->sent == SCTP_DATAGRAM_MARKED) {

				tp1->sent = SCTP_DATAGRAM_ACKED;
			}
		}
		if (tp1->sent == SCTP_DATAGRAM_UNSENT)
			break;
	}
}

static void
sctp_strike_gap_ack_chunks(struct sctp_tcb *stcb, struct sctp_association *asoc,
			   uint32_t biggest_tsn_acked, uint32_t biggest_tsn_newly_acked, uint32_t this_sack_lowest_newack, int accum_moved)
{
	struct sctp_tmit_chunk *tp1;
	int strike_flag = 0;
	struct timeval now;
	int tot_retrans = 0;
	uint32_t sending_seq;
	struct sctp_nets *net;
	int num_dests_sacked = 0;

	tp1 = TAILQ_FIRST(&stcb->asoc.send_queue);
	if (tp1 == NULL) {
		sending_seq = asoc->sending_seq;
	} else {
		sending_seq = tp1->rec.data.tsn;
	}

	if ((asoc->sctp_cmt_on_off > 0) &&
	    SCTP_BASE_SYSCTL(sctp_cmt_use_dac)) {
		TAILQ_FOREACH(net, &asoc->nets, sctp_next) {
			if (net->saw_newack)
				num_dests_sacked++;
		}
	}
	if (stcb->asoc.prsctp_supported) {
		(void)SCTP_GETTIME_TIMEVAL(&now);
	}
	TAILQ_FOREACH(tp1, &asoc->sent_queue, sctp_next) {
		strike_flag = 0;
		if (tp1->no_fr_allowed) {

			continue;
		}
		if (SCTP_BASE_SYSCTL(sctp_logging_level) & SCTP_FR_LOGGING_ENABLE) {
			if (tp1->sent < SCTP_DATAGRAM_RESEND)
				sctp_log_fr(biggest_tsn_newly_acked,
					    tp1->rec.data.tsn,
					    tp1->sent,
					    SCTP_FR_LOG_CHECK_STRIKE);
		}
		if (SCTP_TSN_GT(tp1->rec.data.tsn, biggest_tsn_acked) ||
		    tp1->sent == SCTP_DATAGRAM_UNSENT) {

			break;
		}
		if (stcb->asoc.prsctp_supported) {
			if ((PR_SCTP_TTL_ENABLED(tp1->flags)) && tp1->sent < SCTP_DATAGRAM_ACKED) {

#if !(defined(__FreeBSD__) && !defined(__Userspace__))
				if (timercmp(&now, &tp1->rec.data.timetodrop, >)) {
#else
				if (timevalcmp(&now, &tp1->rec.data.timetodrop, >)) {
#endif

					if (tp1->data != NULL) {
						(void)sctp_release_pr_sctp_chunk(stcb, tp1, 1,
										 SCTP_SO_NOT_LOCKED);
					}
					continue;
				}
			}
		}
		if (SCTP_TSN_GT(tp1->rec.data.tsn, asoc->this_sack_highest_gap) &&
		                !(accum_moved && asoc->fast_retran_loss_recovery)) {

			break;
		}
		if (tp1->sent >= SCTP_DATAGRAM_RESEND) {

			if (tp1->sent == SCTP_FORWARD_TSN_SKIP) {

				tp1->rec.data.fwd_tsn_cnt++;
			}
			continue;
		}

		if (tp1->whoTo && tp1->whoTo->saw_newack == 0) {

			continue;
		} else if (tp1->whoTo &&
		           SCTP_TSN_GT(tp1->rec.data.tsn,
		                       tp1->whoTo->this_sack_highest_newack) &&
		           !(accum_moved && asoc->fast_retran_loss_recovery)) {

			continue;
		}

		if (accum_moved && asoc->fast_retran_loss_recovery) {

			if (SCTP_BASE_SYSCTL(sctp_logging_level) & SCTP_FR_LOGGING_ENABLE) {
				sctp_log_fr(biggest_tsn_newly_acked,
					    tp1->rec.data.tsn,
					    tp1->sent,
					    SCTP_FR_LOG_STRIKE_CHUNK);
			}
			if (tp1->sent < SCTP_DATAGRAM_RESEND) {
				tp1->sent++;
			}
			if ((asoc->sctp_cmt_on_off > 0) &&
			    SCTP_BASE_SYSCTL(sctp_cmt_use_dac)) {

				if ((tp1->sent < SCTP_DATAGRAM_RESEND) && (num_dests_sacked == 1) &&
				    SCTP_TSN_GT(this_sack_lowest_newack, tp1->rec.data.tsn)) {
					if (SCTP_BASE_SYSCTL(sctp_logging_level) & SCTP_FR_LOGGING_ENABLE) {
						sctp_log_fr(16 + num_dests_sacked,
							    tp1->rec.data.tsn,
							    tp1->sent,
							    SCTP_FR_LOG_STRIKE_CHUNK);
					}
					tp1->sent++;
				}
			}
		} else if ((tp1->rec.data.doing_fast_retransmit) &&
		           (asoc->sctp_cmt_on_off == 0)) {

			if (
#ifdef SCTP_FR_TO_ALTERNATE

				(asoc->numnets < 2)
#else
				(1)
#endif
				) {
				if (SCTP_TSN_GE(biggest_tsn_newly_acked,
				                tp1->rec.data.fast_retran_tsn)) {

					if (SCTP_BASE_SYSCTL(sctp_logging_level) & SCTP_FR_LOGGING_ENABLE) {
						sctp_log_fr(biggest_tsn_newly_acked,
							    tp1->rec.data.tsn,
							    tp1->sent,
							    SCTP_FR_LOG_STRIKE_CHUNK);
					}
					if (tp1->sent < SCTP_DATAGRAM_RESEND) {
						tp1->sent++;
					}
					strike_flag = 1;
					if ((asoc->sctp_cmt_on_off > 0) &&
					    SCTP_BASE_SYSCTL(sctp_cmt_use_dac)) {

						if ((tp1->sent < SCTP_DATAGRAM_RESEND) &&
						    (num_dests_sacked == 1) &&
						    SCTP_TSN_GT(this_sack_lowest_newack,
						                tp1->rec.data.tsn)) {
							if (SCTP_BASE_SYSCTL(sctp_logging_level) & SCTP_FR_LOGGING_ENABLE) {
								sctp_log_fr(32 + num_dests_sacked,
									    tp1->rec.data.tsn,
									    tp1->sent,
									    SCTP_FR_LOG_STRIKE_CHUNK);
							}
							if (tp1->sent < SCTP_DATAGRAM_RESEND) {
								tp1->sent++;
							}
						}
					}
				}
			}

		} else if (SCTP_TSN_GT(tp1->rec.data.tsn,
		                       biggest_tsn_newly_acked)) {

			;
		} else {

			if (SCTP_BASE_SYSCTL(sctp_logging_level) & SCTP_FR_LOGGING_ENABLE) {
				sctp_log_fr(biggest_tsn_newly_acked,
					    tp1->rec.data.tsn,
					    tp1->sent,
					    SCTP_FR_LOG_STRIKE_CHUNK);
			}
			if (tp1->sent < SCTP_DATAGRAM_RESEND) {
				tp1->sent++;
			}
			if ((asoc->sctp_cmt_on_off > 0) &&
			    SCTP_BASE_SYSCTL(sctp_cmt_use_dac)) {

				if ((tp1->sent < SCTP_DATAGRAM_RESEND) && (num_dests_sacked == 1) &&
				    SCTP_TSN_GT(this_sack_lowest_newack, tp1->rec.data.tsn)) {
					if (SCTP_BASE_SYSCTL(sctp_logging_level) & SCTP_FR_LOGGING_ENABLE) {
						sctp_log_fr(48 + num_dests_sacked,
							    tp1->rec.data.tsn,
							    tp1->sent,
							    SCTP_FR_LOG_STRIKE_CHUNK);
					}
					tp1->sent++;
				}
			}
		}
		if (tp1->sent == SCTP_DATAGRAM_RESEND) {
			struct sctp_nets *alt;

			if (SCTP_BASE_SYSCTL(sctp_logging_level) & SCTP_FLIGHT_LOGGING_ENABLE) {
				sctp_misc_ints(SCTP_FLIGHT_LOG_DOWN_RSND,
					       (tp1->whoTo ? (tp1->whoTo->flight_size) : 0),
					       tp1->book_size,
					       (uint32_t)(uintptr_t)tp1->whoTo,
					       tp1->rec.data.tsn);
			}
			if (tp1->whoTo) {
				tp1->whoTo->net_ack++;
				sctp_flight_size_decrease(tp1);
				if (stcb->asoc.cc_functions.sctp_cwnd_update_tsn_acknowledged) {
					(*stcb->asoc.cc_functions.sctp_cwnd_update_tsn_acknowledged)(tp1->whoTo,
												     tp1);
				}
			}

			if (SCTP_BASE_SYSCTL(sctp_logging_level) & SCTP_LOG_RWND_ENABLE) {
				sctp_log_rwnd(SCTP_INCREASE_PEER_RWND,
					      asoc->peers_rwnd, tp1->send_size, SCTP_BASE_SYSCTL(sctp_peer_chunk_oh));
			}

			asoc->peers_rwnd += (tp1->send_size + SCTP_BASE_SYSCTL(sctp_peer_chunk_oh));

			sctp_total_flight_decrease(stcb, tp1);

			if ((stcb->asoc.prsctp_supported) &&
			    (PR_SCTP_RTX_ENABLED(tp1->flags))) {

				if (tp1->snd_count > tp1->rec.data.timetodrop.tv_sec) {

					if (tp1->data != NULL) {
						(void)sctp_release_pr_sctp_chunk(stcb, tp1, 1,
										 SCTP_SO_NOT_LOCKED);
					}

					if (tp1->whoTo != NULL) {
						tp1->whoTo->net_ack++;
					}
					continue;
				}
			}

			if (SCTP_BASE_SYSCTL(sctp_logging_level) & SCTP_FR_LOGGING_ENABLE) {
				sctp_log_fr(tp1->rec.data.tsn, tp1->snd_count,
					    0, SCTP_FR_MARKED);
			}
			if (strike_flag) {

				SCTP_STAT_INCR(sctps_sendmultfastretrans);
			}
			sctp_ucount_incr(stcb->asoc.sent_queue_retran_cnt);
			if (asoc->sctp_cmt_on_off > 0) {

				tp1->no_fr_allowed = 1;
				alt = tp1->whoTo;

				if (asoc->sctp_cmt_pf > 0) {

					alt = sctp_find_alternate_net(stcb, alt, 2);
				} else {

					alt = sctp_find_alternate_net(stcb, alt, 1);
				}
				if (alt == NULL) {
					alt = tp1->whoTo;
				}

				if (tp1->whoTo) {
					tp1->whoTo->find_pseudo_cumack = 1;
					tp1->whoTo->find_rtx_pseudo_cumack = 1;
				}
			} else {
#ifdef SCTP_FR_TO_ALTERNATE

				alt = sctp_find_alternate_net(stcb, tp1->whoTo, 0);
#else

				alt = tp1->whoTo;
#endif
			}

			tp1->rec.data.doing_fast_retransmit = 1;
			tot_retrans++;

			if (TAILQ_EMPTY(&asoc->send_queue)) {

				tp1->rec.data.fast_retran_tsn = sending_seq;
			} else {

				struct sctp_tmit_chunk *ttt;

				ttt = TAILQ_FIRST(&asoc->send_queue);
				tp1->rec.data.fast_retran_tsn =
					ttt->rec.data.tsn;
			}

			if (tp1->do_rtt) {

				if ((tp1->whoTo != NULL) &&
				    (tp1->whoTo->rto_needed == 0)) {
					tp1->whoTo->rto_needed = 1;
				}
				tp1->do_rtt = 0;
			}
			if (alt != tp1->whoTo) {

				sctp_free_remote_addr(tp1->whoTo);

				tp1->whoTo = alt;
				atomic_add_int(&alt->ref_count, 1);
			}
		}
	}
}

struct sctp_tmit_chunk *
sctp_try_advance_peer_ack_point(struct sctp_tcb *stcb,
    struct sctp_association *asoc)
{
	struct sctp_tmit_chunk *tp1, *tp2, *a_adv = NULL;
	struct timeval now;
	int now_filled = 0;

	if (asoc->prsctp_supported == 0) {
		return (NULL);
	}
	TAILQ_FOREACH_SAFE(tp1, &asoc->sent_queue, sctp_next, tp2) {
		if (tp1->sent != SCTP_FORWARD_TSN_SKIP &&
		    tp1->sent != SCTP_DATAGRAM_RESEND &&
		    tp1->sent != SCTP_DATAGRAM_NR_ACKED) {

			break;
		}
		if (SCTP_BASE_SYSCTL(sctp_logging_level) & SCTP_LOG_TRY_ADVANCE) {
			if ((tp1->sent == SCTP_FORWARD_TSN_SKIP) ||
			    (tp1->sent == SCTP_DATAGRAM_NR_ACKED)) {
				sctp_misc_ints(SCTP_FWD_TSN_CHECK,
					       asoc->advanced_peer_ack_point,
					       tp1->rec.data.tsn, 0, 0);
			}
		}
		if (!PR_SCTP_ENABLED(tp1->flags)) {

			break;
		}
		if (!now_filled) {
			(void)SCTP_GETTIME_TIMEVAL(&now);
			now_filled = 1;
		}

		if (tp1->sent == SCTP_DATAGRAM_RESEND &&
		    (PR_SCTP_TTL_ENABLED(tp1->flags))) {

#if !(defined(__FreeBSD__)  && !defined(__Userspace__))
			if (timercmp(&now, &tp1->rec.data.timetodrop, >)) {
#else
			if (timevalcmp(&now, &tp1->rec.data.timetodrop, >)) {
#endif

				if (tp1->data) {
					(void)sctp_release_pr_sctp_chunk(stcb, tp1,
					    1, SCTP_SO_NOT_LOCKED);
				}
			} else {

				break;
			}
		}

		if ((tp1->sent == SCTP_FORWARD_TSN_SKIP) ||
		    (tp1->sent == SCTP_DATAGRAM_NR_ACKED)) {

			if (SCTP_TSN_GT(tp1->rec.data.tsn, asoc->advanced_peer_ack_point)) {
				asoc->advanced_peer_ack_point = tp1->rec.data.tsn;
				a_adv = tp1;
			} else if (tp1->rec.data.tsn == asoc->advanced_peer_ack_point) {

				a_adv = tp1;
			}
		} else {

			break;
		}
	}
	return (a_adv);
}

static int
sctp_fs_audit(struct sctp_association *asoc)
{
	struct sctp_tmit_chunk *chk;
	int inflight = 0, resend = 0, inbetween = 0, acked = 0, above = 0;
	int ret;
#ifndef INVARIANTS
	int entry_flight, entry_cnt;
#endif

	ret = 0;
#ifndef INVARIANTS
	entry_flight = asoc->total_flight;
	entry_cnt = asoc->total_flight_count;
#endif
	if (asoc->pr_sctp_cnt >= asoc->sent_queue_cnt)
		return (0);

	TAILQ_FOREACH(chk, &asoc->sent_queue, sctp_next) {
		if (chk->sent < SCTP_DATAGRAM_RESEND) {
			SCTP_PRINTF("Chk TSN: %u size: %d inflight cnt: %d\n",
			            chk->rec.data.tsn,
			            chk->send_size,
			            chk->snd_count);
			inflight++;
		} else if (chk->sent == SCTP_DATAGRAM_RESEND) {
			resend++;
		} else if (chk->sent < SCTP_DATAGRAM_ACKED) {
			inbetween++;
		} else if (chk->sent > SCTP_DATAGRAM_ACKED) {
			above++;
		} else {
			acked++;
		}
	}

	if ((inflight > 0) || (inbetween > 0)) {
#ifdef INVARIANTS
		panic("Flight size-express incorrect? \n");
#else
		SCTP_PRINTF("asoc->total_flight: %d cnt: %d\n",
		            entry_flight, entry_cnt);

		SCTP_PRINTF("Flight size-express incorrect F: %d I: %d R: %d Ab: %d ACK: %d\n",
			    inflight, inbetween, resend, above, acked);
		ret = 1;
#endif
	}
	return (ret);
}

static void
sctp_window_probe_recovery(struct sctp_tcb *stcb,
                           struct sctp_association *asoc,
                           struct sctp_tmit_chunk *tp1)
{
	tp1->window_probe = 0;
	if ((tp1->sent >= SCTP_DATAGRAM_ACKED) || (tp1->data == NULL)) {

		sctp_misc_ints(SCTP_FLIGHT_LOG_DWN_WP_FWD,
			       tp1->whoTo ? tp1->whoTo->flight_size : 0,
			       tp1->book_size,
			       (uint32_t)(uintptr_t)tp1->whoTo,
			       tp1->rec.data.tsn);
		return;
	}

	if (stcb->asoc.cc_functions.sctp_cwnd_update_tsn_acknowledged) {
		(*stcb->asoc.cc_functions.sctp_cwnd_update_tsn_acknowledged)(tp1->whoTo,
									     tp1);
	}
	sctp_flight_size_decrease(tp1);
	sctp_total_flight_decrease(stcb, tp1);

	tp1->sent = SCTP_DATAGRAM_RESEND;
	sctp_ucount_incr(asoc->sent_queue_retran_cnt);

	if (SCTP_BASE_SYSCTL(sctp_logging_level) & SCTP_FLIGHT_LOGGING_ENABLE) {
		sctp_misc_ints(SCTP_FLIGHT_LOG_DOWN_WP,
			       tp1->whoTo->flight_size,
			       tp1->book_size,
			       (uint32_t)(uintptr_t)tp1->whoTo,
			       tp1->rec.data.tsn);
	}
}

void
sctp_express_handle_sack(struct sctp_tcb *stcb, uint32_t cumack,
                         uint32_t rwnd, int *abort_now, int ecne_seen)
{
	struct sctp_nets *net;
	struct sctp_association *asoc;
	struct sctp_tmit_chunk *tp1, *tp2;
	uint32_t old_rwnd;
	int win_probe_recovery = 0;
	int win_probe_recovered = 0;
	int j, done_once = 0;
	int rto_ok = 1;
	uint32_t send_s;

	if (SCTP_BASE_SYSCTL(sctp_logging_level) & SCTP_LOG_SACK_ARRIVALS_ENABLE) {
		sctp_misc_ints(SCTP_SACK_LOG_EXPRESS, cumack,
		               rwnd, stcb->asoc.last_acked_seq, stcb->asoc.peers_rwnd);
	}
	SCTP_TCB_LOCK_ASSERT(stcb);
#ifdef SCTP_ASOCLOG_OF_TSNS
	stcb->asoc.cumack_log[stcb->asoc.cumack_log_at] = cumack;
	stcb->asoc.cumack_log_at++;
	if (stcb->asoc.cumack_log_at > SCTP_TSN_LOG_SIZE) {
		stcb->asoc.cumack_log_at = 0;
	}
#endif
	asoc = &stcb->asoc;
	old_rwnd = asoc->peers_rwnd;
	if (SCTP_TSN_GT(asoc->last_acked_seq, cumack)) {

		return;
	} else if (asoc->last_acked_seq == cumack) {

		asoc->peers_rwnd = sctp_sbspace_sub(rwnd,
						    (uint32_t) (asoc->total_flight + (asoc->total_flight_count * SCTP_BASE_SYSCTL(sctp_peer_chunk_oh))));
		if (asoc->peers_rwnd < stcb->sctp_ep->sctp_ep.sctp_sws_sender) {

			asoc->peers_rwnd = 0;
		}
		if (asoc->peers_rwnd > old_rwnd) {
			goto again;
		}
		return;
	}

	TAILQ_FOREACH(net, &asoc->nets, sctp_next) {
		if (SCTP_TSN_GT(cumack, net->cwr_window_tsn)) {

			net->cwr_window_tsn = cumack;
		}
		net->prev_cwnd = net->cwnd;
		net->net_ack = 0;
		net->net_ack2 = 0;

		net->new_pseudo_cumack = 0;
		net->will_exit_fast_recovery = 0;
		if (stcb->asoc.cc_functions.sctp_cwnd_prepare_net_for_sack) {
			(*stcb->asoc.cc_functions.sctp_cwnd_prepare_net_for_sack)(stcb, net);
		}
	}
	if (!TAILQ_EMPTY(&asoc->sent_queue)) {
		tp1 = TAILQ_LAST(&asoc->sent_queue,
				 sctpchunk_listhead);
		send_s = tp1->rec.data.tsn + 1;
	} else {
		send_s = asoc->sending_seq;
	}
	if (SCTP_TSN_GE(cumack, send_s)) {
		struct mbuf *op_err;
		char msg[SCTP_DIAG_INFO_LEN];

		*abort_now = 1;

		SCTP_SNPRINTF(msg, sizeof(msg),
		              "Cum ack %8.8x greater or equal than TSN %8.8x",
		              cumack, send_s);
		op_err = sctp_generate_cause(SCTP_CAUSE_PROTOCOL_VIOLATION, msg);
		stcb->sctp_ep->last_abort_code = SCTP_FROM_SCTP_INDATA + SCTP_LOC_25;
		sctp_abort_an_association(stcb->sctp_ep, stcb, op_err, false, SCTP_SO_NOT_LOCKED);
		return;
	}
	asoc->this_sack_highest_gap = cumack;
	if (SCTP_BASE_SYSCTL(sctp_logging_level) & SCTP_THRESHOLD_LOGGING) {
		sctp_misc_ints(SCTP_THRESHOLD_CLEAR,
			       stcb->asoc.overall_error_count,
			       0,
			       SCTP_FROM_SCTP_INDATA,
			       __LINE__);
	}
	stcb->asoc.overall_error_count = 0;
	if (SCTP_TSN_GT(cumack, asoc->last_acked_seq)) {

		TAILQ_FOREACH_SAFE(tp1, &asoc->sent_queue, sctp_next, tp2) {
			if (SCTP_TSN_GE(cumack, tp1->rec.data.tsn)) {
				if (tp1->sent == SCTP_DATAGRAM_UNSENT) {
					SCTP_PRINTF("Warning, an unsent is now acked?\n");
				}
				if (tp1->sent < SCTP_DATAGRAM_ACKED) {

					if (tp1->sent < SCTP_DATAGRAM_RESEND) {
						if (SCTP_BASE_SYSCTL(sctp_logging_level) & SCTP_FLIGHT_LOGGING_ENABLE) {
							sctp_misc_ints(SCTP_FLIGHT_LOG_DOWN_CA,
								       tp1->whoTo->flight_size,
								       tp1->book_size,
								       (uint32_t)(uintptr_t)tp1->whoTo,
								       tp1->rec.data.tsn);
						}
						sctp_flight_size_decrease(tp1);
						if (stcb->asoc.cc_functions.sctp_cwnd_update_tsn_acknowledged) {
							(*stcb->asoc.cc_functions.sctp_cwnd_update_tsn_acknowledged)(tp1->whoTo,
														     tp1);
						}

						sctp_total_flight_decrease(stcb, tp1);
					}
					tp1->whoTo->net_ack += tp1->send_size;
					if (tp1->snd_count < 2) {

						tp1->whoTo->net_ack2 +=
							tp1->send_size;

						if (tp1->do_rtt) {
							if (rto_ok &&
							    sctp_calculate_rto(stcb,
									       &stcb->asoc,
									       tp1->whoTo,
									       &tp1->sent_rcv_time,
									       SCTP_RTT_FROM_DATA)) {
								rto_ok = 0;
							}
							if (tp1->whoTo->rto_needed == 0) {
								tp1->whoTo->rto_needed = 1;
							}
							tp1->do_rtt = 0;
						}
					}

					tp1->whoTo->new_pseudo_cumack = 1;
					tp1->whoTo->find_pseudo_cumack = 1;
					tp1->whoTo->find_rtx_pseudo_cumack = 1;
					if (SCTP_BASE_SYSCTL(sctp_logging_level) & SCTP_CWND_LOGGING_ENABLE) {

						sctp_log_cwnd(stcb, tp1->whoTo, tp1->rec.data.tsn, SCTP_CWND_LOG_FROM_SACK);
					}
				}
				if (tp1->sent == SCTP_DATAGRAM_RESEND) {
					sctp_ucount_decr(asoc->sent_queue_retran_cnt);
				}
				if (tp1->rec.data.chunk_was_revoked) {

					tp1->whoTo->cwnd -= tp1->book_size;
					tp1->rec.data.chunk_was_revoked = 0;
				}
				if (tp1->sent != SCTP_DATAGRAM_NR_ACKED) {
					if (asoc->strmout[tp1->rec.data.sid].chunks_on_queues > 0) {
						asoc->strmout[tp1->rec.data.sid].chunks_on_queues--;
#ifdef INVARIANTS
					} else {
						panic("No chunks on the queues for sid %u.", tp1->rec.data.sid);
#endif
					}
				}
				if ((asoc->strmout[tp1->rec.data.sid].chunks_on_queues == 0) &&
				    (asoc->strmout[tp1->rec.data.sid].state == SCTP_STREAM_RESET_PENDING) &&
				    TAILQ_EMPTY(&asoc->strmout[tp1->rec.data.sid].outqueue)) {
					asoc->trigger_reset = 1;
				}
				TAILQ_REMOVE(&asoc->sent_queue, tp1, sctp_next);
				if (tp1->data) {

					sctp_free_bufspace(stcb, asoc, tp1, 1);
					sctp_m_freem(tp1->data);
					tp1->data = NULL;
				}
				if (SCTP_BASE_SYSCTL(sctp_logging_level) & SCTP_SACK_LOGGING_ENABLE) {
					sctp_log_sack(asoc->last_acked_seq,
						      cumack,
						      tp1->rec.data.tsn,
						      0,
						      0,
						      SCTP_LOG_FREE_SENT);
				}
				asoc->sent_queue_cnt--;
				sctp_free_a_chunk(stcb, tp1, SCTP_SO_NOT_LOCKED);
			} else {
				break;
			}
		}
	}
#if defined(__Userspace__)
	if (stcb->sctp_ep->recv_callback) {
		if (stcb->sctp_socket) {
			uint32_t inqueue_bytes, sb_free_now;
			struct sctp_inpcb *inp;

			inp = stcb->sctp_ep;
			inqueue_bytes = stcb->asoc.total_output_queue_size - (stcb->asoc.chunks_on_out_queue * sizeof(struct sctp_data_chunk));
			sb_free_now = SCTP_SB_LIMIT_SND(stcb->sctp_socket) - (inqueue_bytes + stcb->asoc.sb_send_resv);

			if (inp->send_callback &&
			    (((inp->send_sb_threshold > 0) &&
			      (sb_free_now >= inp->send_sb_threshold) &&
			      (stcb->asoc.chunks_on_out_queue <= SCTP_BASE_SYSCTL(sctp_max_chunks_on_queue))) ||
			     (inp->send_sb_threshold == 0))) {
				atomic_add_int(&stcb->asoc.refcnt, 1);
				SCTP_TCB_UNLOCK(stcb);
				inp->send_callback(stcb->sctp_socket, sb_free_now, inp->ulp_info);
				SCTP_TCB_LOCK(stcb);
				atomic_subtract_int(&stcb->asoc.refcnt, 1);
			}
		}
	} else if (stcb->sctp_socket) {
#else

	if (stcb->sctp_socket) {
#endif
#if defined(__APPLE__) && !defined(__Userspace__)
		struct socket *so;

#endif
		SOCKBUF_LOCK(&stcb->sctp_socket->so_snd);
		if (SCTP_BASE_SYSCTL(sctp_logging_level) & SCTP_WAKE_LOGGING_ENABLE) {

			sctp_wakeup_log(stcb, 1, SCTP_WAKESND_FROM_SACK);
		}
#if defined(__APPLE__) && !defined(__Userspace__)
		so = SCTP_INP_SO(stcb->sctp_ep);
		atomic_add_int(&stcb->asoc.refcnt, 1);
		SCTP_TCB_UNLOCK(stcb);
		SCTP_SOCKET_LOCK(so, 1);
		SCTP_TCB_LOCK(stcb);
		atomic_subtract_int(&stcb->asoc.refcnt, 1);
		if (stcb->asoc.state & SCTP_STATE_CLOSED_SOCKET) {

			SCTP_SOCKET_UNLOCK(so, 1);
			return;
		}
#endif
		sctp_sowwakeup_locked(stcb->sctp_ep, stcb->sctp_socket);
#if defined(__APPLE__) && !defined(__Userspace__)
		SCTP_SOCKET_UNLOCK(so, 1);
#endif
	} else {
		if (SCTP_BASE_SYSCTL(sctp_logging_level) & SCTP_WAKE_LOGGING_ENABLE) {
			sctp_wakeup_log(stcb, 1, SCTP_NOWAKE_FROM_SACK);
		}
	}

	if ((asoc->last_acked_seq != cumack) && (ecne_seen == 0)) {
		TAILQ_FOREACH(net, &asoc->nets, sctp_next) {
			if (net->net_ack2 > 0) {

				net->error_count = 0;
				if (!(net->dest_state & SCTP_ADDR_REACHABLE)) {

					net->dest_state |= SCTP_ADDR_REACHABLE;
					sctp_ulp_notify(SCTP_NOTIFY_INTERFACE_UP, stcb,
					                0, (void *)net, SCTP_SO_NOT_LOCKED);
				}
				if (net == stcb->asoc.primary_destination) {
					if (stcb->asoc.alternate) {

						sctp_free_remote_addr(stcb->asoc.alternate);
						stcb->asoc.alternate = NULL;
					}
				}
				if (net->dest_state & SCTP_ADDR_PF) {
					net->dest_state &= ~SCTP_ADDR_PF;
					sctp_timer_stop(SCTP_TIMER_TYPE_HEARTBEAT,
					                stcb->sctp_ep, stcb, net,
					                SCTP_FROM_SCTP_INDATA + SCTP_LOC_26);
					sctp_timer_start(SCTP_TIMER_TYPE_HEARTBEAT, stcb->sctp_ep, stcb, net);
					asoc->cc_functions.sctp_cwnd_update_exit_pf(stcb, net);

					net->net_ack = 0;
				}

				net->RTO = (net->lastsa >> SCTP_RTT_SHIFT) + net->lastsv;
				if (net->RTO < stcb->asoc.minrto) {
					net->RTO = stcb->asoc.minrto;
				}
				if (net->RTO > stcb->asoc.maxrto) {
					net->RTO = stcb->asoc.maxrto;
				}
			}
		}
		asoc->cc_functions.sctp_cwnd_update_after_sack(stcb, asoc, 1, 0, 0);
	}
	asoc->last_acked_seq = cumack;

	if (TAILQ_EMPTY(&asoc->sent_queue)) {

		TAILQ_FOREACH(net, &asoc->nets, sctp_next) {
			net->flight_size = 0;
			net->partial_bytes_acked = 0;
		}
		asoc->total_flight = 0;
		asoc->total_flight_count = 0;
	}

	asoc->peers_rwnd = sctp_sbspace_sub(rwnd,
					    (uint32_t) (asoc->total_flight + (asoc->total_flight_count * SCTP_BASE_SYSCTL(sctp_peer_chunk_oh))));
	if (asoc->peers_rwnd < stcb->sctp_ep->sctp_ep.sctp_sws_sender) {

		asoc->peers_rwnd = 0;
	}
	if (asoc->peers_rwnd > old_rwnd) {
		win_probe_recovery = 1;
	}

again:
	j = 0;
	TAILQ_FOREACH(net, &asoc->nets, sctp_next) {
		if (win_probe_recovery && (net->window_probe)) {
			win_probe_recovered = 1;

			TAILQ_FOREACH(tp1, &asoc->sent_queue, sctp_next) {
				if (tp1->window_probe) {

					sctp_window_probe_recovery(stcb, asoc, tp1);
					break;
				}
			}
		}
		if (net->flight_size) {
			j++;
			sctp_timer_start(SCTP_TIMER_TYPE_SEND, stcb->sctp_ep, stcb, net);
			if (net->window_probe) {
				net->window_probe = 0;
			}
		} else {
			if (net->window_probe) {

				net->window_probe = 0;
				if (!SCTP_OS_TIMER_PENDING(&net->rxt_timer.timer)) {
					sctp_timer_start(SCTP_TIMER_TYPE_SEND, stcb->sctp_ep, stcb, net);
				}
			} else if (SCTP_OS_TIMER_PENDING(&net->rxt_timer.timer)) {
				sctp_timer_stop(SCTP_TIMER_TYPE_SEND, stcb->sctp_ep,
				                stcb, net,
				                SCTP_FROM_SCTP_INDATA + SCTP_LOC_27);
			}
		}
	}
	if ((j == 0) &&
	    (!TAILQ_EMPTY(&asoc->sent_queue)) &&
	    (asoc->sent_queue_retran_cnt == 0) &&
	    (win_probe_recovered == 0) &&
	    (done_once == 0)) {

		if (sctp_fs_audit(asoc)) {
			TAILQ_FOREACH(net, &asoc->nets, sctp_next) {
				net->flight_size = 0;
			}
			asoc->total_flight = 0;
			asoc->total_flight_count = 0;
			asoc->sent_queue_retran_cnt = 0;
			TAILQ_FOREACH(tp1, &asoc->sent_queue, sctp_next) {
				if (tp1->sent < SCTP_DATAGRAM_RESEND) {
					sctp_flight_size_increase(tp1);
					sctp_total_flight_increase(stcb, tp1);
				} else if (tp1->sent == SCTP_DATAGRAM_RESEND) {
					sctp_ucount_incr(asoc->sent_queue_retran_cnt);
				}
			}
		}
		done_once = 1;
		goto again;
	}

	if (TAILQ_EMPTY(&asoc->send_queue) && TAILQ_EMPTY(&asoc->sent_queue)) {

		if ((asoc->stream_queue_cnt == 1) &&
		    ((asoc->state & SCTP_STATE_SHUTDOWN_PENDING) ||
		     (SCTP_GET_STATE(stcb) == SCTP_STATE_SHUTDOWN_RECEIVED)) &&
		    ((*asoc->ss_functions.sctp_ss_is_user_msgs_incomplete)(stcb, asoc))) {
			SCTP_ADD_SUBSTATE(stcb, SCTP_STATE_PARTIAL_MSG_LEFT);
		}
		if (((asoc->state & SCTP_STATE_SHUTDOWN_PENDING) ||
		     (SCTP_GET_STATE(stcb) == SCTP_STATE_SHUTDOWN_RECEIVED)) &&
		    (asoc->stream_queue_cnt == 1) &&
		    (asoc->state & SCTP_STATE_PARTIAL_MSG_LEFT)) {
			struct mbuf *op_err;

			*abort_now = 1;

			op_err = sctp_generate_cause(SCTP_CAUSE_USER_INITIATED_ABT, "");
			stcb->sctp_ep->last_abort_code = SCTP_FROM_SCTP_INDATA + SCTP_LOC_28;
			sctp_abort_an_association(stcb->sctp_ep, stcb, op_err, false, SCTP_SO_NOT_LOCKED);
			return;
		}
		if ((asoc->state & SCTP_STATE_SHUTDOWN_PENDING) &&
		    (asoc->stream_queue_cnt == 0)) {
			struct sctp_nets *netp;

			if ((SCTP_GET_STATE(stcb) == SCTP_STATE_OPEN) ||
			    (SCTP_GET_STATE(stcb) == SCTP_STATE_SHUTDOWN_RECEIVED)) {
				SCTP_STAT_DECR_GAUGE32(sctps_currestab);
			}
			SCTP_SET_STATE(stcb, SCTP_STATE_SHUTDOWN_SENT);
			sctp_stop_timers_for_shutdown(stcb);
			if (asoc->alternate) {
				netp = asoc->alternate;
			} else {
				netp = asoc->primary_destination;
			}
			sctp_send_shutdown(stcb, netp);
			sctp_timer_start(SCTP_TIMER_TYPE_SHUTDOWN,
					 stcb->sctp_ep, stcb, netp);
			sctp_timer_start(SCTP_TIMER_TYPE_SHUTDOWNGUARD,
					 stcb->sctp_ep, stcb, NULL);
		} else if ((SCTP_GET_STATE(stcb) == SCTP_STATE_SHUTDOWN_RECEIVED) &&
			   (asoc->stream_queue_cnt == 0)) {
			struct sctp_nets *netp;

			SCTP_STAT_DECR_GAUGE32(sctps_currestab);
			SCTP_SET_STATE(stcb, SCTP_STATE_SHUTDOWN_ACK_SENT);
			sctp_stop_timers_for_shutdown(stcb);
			if (asoc->alternate) {
				netp = asoc->alternate;
			} else {
				netp = asoc->primary_destination;
			}
			sctp_send_shutdown_ack(stcb, netp);
			sctp_timer_start(SCTP_TIMER_TYPE_SHUTDOWNACK,
					 stcb->sctp_ep, stcb, netp);
		}
	}

	if (SCTP_TSN_GT(cumack, asoc->advanced_peer_ack_point)) {
		asoc->advanced_peer_ack_point = cumack;
	}

	if ((asoc->prsctp_supported) && (asoc->pr_sctp_cnt > 0)) {
		struct sctp_tmit_chunk *lchk;
		uint32_t old_adv_peer_ack_point;

		old_adv_peer_ack_point = asoc->advanced_peer_ack_point;
		lchk = sctp_try_advance_peer_ack_point(stcb, asoc);

		if (SCTP_TSN_GT(asoc->advanced_peer_ack_point, cumack)) {

			if (SCTP_TSN_GT(asoc->advanced_peer_ack_point, old_adv_peer_ack_point)) {
				send_forward_tsn(stcb, asoc);
			} else if (lchk) {

				if (lchk->rec.data.fwd_tsn_cnt >= 3) {
					send_forward_tsn(stcb, asoc);
				}
			}
		}
		for (; lchk != NULL; lchk = TAILQ_NEXT(lchk, sctp_next)) {
			if (lchk->whoTo != NULL) {
				break;
			}
		}
		if (lchk != NULL) {

			sctp_timer_start(SCTP_TIMER_TYPE_SEND,
			                 stcb->sctp_ep, stcb, lchk->whoTo);
		}
	}
	if (SCTP_BASE_SYSCTL(sctp_logging_level) & SCTP_SACK_RWND_LOGGING_ENABLE) {
		sctp_misc_ints(SCTP_SACK_RWND_UPDATE,
			       rwnd,
			       stcb->asoc.peers_rwnd,
			       stcb->asoc.total_flight,
			       stcb->asoc.total_output_queue_size);
	}
}

void
sctp_handle_sack(struct mbuf *m, int offset_seg, int offset_dup,
                 struct sctp_tcb *stcb,
                 uint16_t num_seg, uint16_t num_nr_seg, uint16_t num_dup,
                 int *abort_now, uint8_t flags,
                 uint32_t cum_ack, uint32_t rwnd, int ecne_seen)
{
	struct sctp_association *asoc;
	struct sctp_tmit_chunk *tp1, *tp2;
	uint32_t last_tsn, biggest_tsn_acked, biggest_tsn_newly_acked, this_sack_lowest_newack;
	uint16_t wake_him = 0;
	uint32_t send_s = 0;
	long j;
	int accum_moved = 0;
	int will_exit_fast_recovery = 0;
	uint32_t a_rwnd, old_rwnd;
	int win_probe_recovery = 0;
	int win_probe_recovered = 0;
	struct sctp_nets *net = NULL;
	int done_once;
	int rto_ok = 1;
	uint8_t reneged_all = 0;
	uint8_t cmt_dac_flag;

	SCTP_TCB_LOCK_ASSERT(stcb);

	this_sack_lowest_newack = 0;
	SCTP_STAT_INCR(sctps_slowpath_sack);
	last_tsn = cum_ack;
	cmt_dac_flag = flags & SCTP_SACK_CMT_DAC;
#ifdef SCTP_ASOCLOG_OF_TSNS
	stcb->asoc.cumack_log[stcb->asoc.cumack_log_at] = cum_ack;
	stcb->asoc.cumack_log_at++;
	if (stcb->asoc.cumack_log_at > SCTP_TSN_LOG_SIZE) {
		stcb->asoc.cumack_log_at = 0;
	}
#endif
	a_rwnd = rwnd;

	if (SCTP_BASE_SYSCTL(sctp_logging_level) & SCTP_LOG_SACK_ARRIVALS_ENABLE) {
		sctp_misc_ints(SCTP_SACK_LOG_NORMAL, cum_ack,
		               rwnd, stcb->asoc.last_acked_seq, stcb->asoc.peers_rwnd);
	}

	old_rwnd = stcb->asoc.peers_rwnd;
	if (SCTP_BASE_SYSCTL(sctp_logging_level) & SCTP_THRESHOLD_LOGGING) {
		sctp_misc_ints(SCTP_THRESHOLD_CLEAR,
		               stcb->asoc.overall_error_count,
		               0,
		               SCTP_FROM_SCTP_INDATA,
		               __LINE__);
	}
	stcb->asoc.overall_error_count = 0;
	asoc = &stcb->asoc;
	if (SCTP_BASE_SYSCTL(sctp_logging_level) & SCTP_SACK_LOGGING_ENABLE) {
		sctp_log_sack(asoc->last_acked_seq,
		              cum_ack,
		              0,
		              num_seg,
		              num_dup,
		              SCTP_LOG_NEW_SACK);
	}
	if ((num_dup) && (SCTP_BASE_SYSCTL(sctp_logging_level) & SCTP_FR_LOGGING_ENABLE)) {
		uint16_t i;
		uint32_t *dupdata, dblock;

		for (i = 0; i < num_dup; i++) {
			dupdata = (uint32_t *)sctp_m_getptr(m, offset_dup + i * sizeof(uint32_t),
			                                    sizeof(uint32_t), (uint8_t *)&dblock);
			if (dupdata == NULL) {
				break;
			}
			sctp_log_fr(*dupdata, 0, 0, SCTP_FR_DUPED);
		}
	}

	if (!TAILQ_EMPTY(&asoc->sent_queue)) {
		tp1 = TAILQ_LAST(&asoc->sent_queue,
				 sctpchunk_listhead);
		send_s = tp1->rec.data.tsn + 1;
	} else {
		tp1 = NULL;
		send_s = asoc->sending_seq;
	}
	if (SCTP_TSN_GE(cum_ack, send_s)) {
		struct mbuf *op_err;
		char msg[SCTP_DIAG_INFO_LEN];

		SCTP_PRINTF("NEW cum_ack:%x send_s:%x is smaller or equal\n",
			    cum_ack, send_s);
		if (tp1) {
			SCTP_PRINTF("Got send_s from tsn:%x + 1 of tp1: %p\n",
				    tp1->rec.data.tsn, (void *)tp1);
		}
	hopeless_peer:
		*abort_now = 1;

		SCTP_SNPRINTF(msg, sizeof(msg),
		              "Cum ack %8.8x greater or equal than TSN %8.8x",
		              cum_ack, send_s);
		op_err = sctp_generate_cause(SCTP_CAUSE_PROTOCOL_VIOLATION, msg);
		stcb->sctp_ep->last_abort_code = SCTP_FROM_SCTP_INDATA + SCTP_LOC_29;
		sctp_abort_an_association(stcb->sctp_ep, stcb, op_err, false, SCTP_SO_NOT_LOCKED);
		return;
	}

	if (SCTP_TSN_GT(asoc->last_acked_seq, last_tsn)) {

		return;
	}

	if (TAILQ_EMPTY(&asoc->sent_queue) &&
	    TAILQ_EMPTY(&asoc->send_queue) &&
	    (asoc->stream_queue_cnt == 0)) {

		if (SCTP_BASE_SYSCTL(sctp_logging_level) & SCTP_LOG_RWND_ENABLE) {
			sctp_log_rwnd_set(SCTP_SET_PEER_RWND_VIA_SACK,
			                  asoc->peers_rwnd, 0, 0, a_rwnd);
		}
		asoc->peers_rwnd = a_rwnd;
		if (asoc->sent_queue_retran_cnt) {
			asoc->sent_queue_retran_cnt = 0;
		}
		if (asoc->peers_rwnd < stcb->sctp_ep->sctp_ep.sctp_sws_sender) {

			asoc->peers_rwnd = 0;
		}

		TAILQ_FOREACH(net, &asoc->nets, sctp_next) {
			sctp_timer_stop(SCTP_TIMER_TYPE_SEND, stcb->sctp_ep,
			                stcb, net, SCTP_FROM_SCTP_INDATA + SCTP_LOC_30);
			net->partial_bytes_acked = 0;
			net->flight_size = 0;
		}
		asoc->total_flight = 0;
		asoc->total_flight_count = 0;
		return;
	}

	TAILQ_FOREACH(net, &asoc->nets, sctp_next) {
		if (SCTP_TSN_GT(cum_ack, net->cwr_window_tsn)) {

			net->cwr_window_tsn = cum_ack;
		}
		net->prev_cwnd = net->cwnd;
		net->net_ack = 0;
		net->net_ack2 = 0;

		net->new_pseudo_cumack = 0;
		net->will_exit_fast_recovery = 0;
		if (stcb->asoc.cc_functions.sctp_cwnd_prepare_net_for_sack) {
			(*stcb->asoc.cc_functions.sctp_cwnd_prepare_net_for_sack)(stcb, net);
		}

		net->saw_newack = 0;
		net->this_sack_highest_newack = last_tsn;
	}

	TAILQ_FOREACH(tp1, &asoc->sent_queue, sctp_next) {
		if (SCTP_TSN_GE(last_tsn, tp1->rec.data.tsn)) {
			if (tp1->sent != SCTP_DATAGRAM_UNSENT) {
				accum_moved = 1;
				if (tp1->sent < SCTP_DATAGRAM_ACKED) {

					if ((tp1->whoTo->dest_state &
					     SCTP_ADDR_UNCONFIRMED) &&
					    (tp1->snd_count < 2)) {

						tp1->whoTo->dest_state &=
							~SCTP_ADDR_UNCONFIRMED;
					}
					if (tp1->sent < SCTP_DATAGRAM_RESEND) {
						if (SCTP_BASE_SYSCTL(sctp_logging_level) & SCTP_FLIGHT_LOGGING_ENABLE) {
							sctp_misc_ints(SCTP_FLIGHT_LOG_DOWN_CA,
							               tp1->whoTo->flight_size,
							               tp1->book_size,
							               (uint32_t)(uintptr_t)tp1->whoTo,
							               tp1->rec.data.tsn);
						}
						sctp_flight_size_decrease(tp1);
						sctp_total_flight_decrease(stcb, tp1);
						if (stcb->asoc.cc_functions.sctp_cwnd_update_tsn_acknowledged) {
							(*stcb->asoc.cc_functions.sctp_cwnd_update_tsn_acknowledged)(tp1->whoTo,
														     tp1);
						}
					}
					tp1->whoTo->net_ack += tp1->send_size;

					this_sack_lowest_newack = tp1->rec.data.tsn;
					tp1->whoTo->saw_newack = 1;

					if (tp1->snd_count < 2) {

						tp1->whoTo->net_ack2 +=
							tp1->send_size;

						if (tp1->do_rtt) {
							if (rto_ok &&
							    sctp_calculate_rto(stcb,
									       &stcb->asoc,
									       tp1->whoTo,
									       &tp1->sent_rcv_time,
									       SCTP_RTT_FROM_DATA)) {
								rto_ok = 0;
							}
							if (tp1->whoTo->rto_needed == 0) {
								tp1->whoTo->rto_needed = 1;
							}
							tp1->do_rtt = 0;
						}
					}

					tp1->whoTo->new_pseudo_cumack = 1;
					tp1->whoTo->find_pseudo_cumack = 1;
					tp1->whoTo->find_rtx_pseudo_cumack = 1;
					if (SCTP_BASE_SYSCTL(sctp_logging_level) & SCTP_SACK_LOGGING_ENABLE) {
						sctp_log_sack(asoc->last_acked_seq,
						              cum_ack,
						              tp1->rec.data.tsn,
						              0,
						              0,
						              SCTP_LOG_TSN_ACKED);
					}
					if (SCTP_BASE_SYSCTL(sctp_logging_level) & SCTP_CWND_LOGGING_ENABLE) {
						sctp_log_cwnd(stcb, tp1->whoTo, tp1->rec.data.tsn, SCTP_CWND_LOG_FROM_SACK);
					}
				}
				if (tp1->sent == SCTP_DATAGRAM_RESEND) {
					sctp_ucount_decr(asoc->sent_queue_retran_cnt);
#ifdef SCTP_AUDITING_ENABLED
					sctp_audit_log(0xB3,
					               (asoc->sent_queue_retran_cnt & 0x000000ff));
#endif
				}
				if (tp1->rec.data.chunk_was_revoked) {

					tp1->whoTo->cwnd -= tp1->book_size;
					tp1->rec.data.chunk_was_revoked = 0;
				}
				if (tp1->sent != SCTP_DATAGRAM_NR_ACKED) {
					tp1->sent = SCTP_DATAGRAM_ACKED;
				}
			}
		} else {
			break;
		}
	}
	biggest_tsn_newly_acked = biggest_tsn_acked = last_tsn;

	asoc->this_sack_highest_gap = last_tsn;

	if ((num_seg > 0) || (num_nr_seg > 0)) {

		if (sctp_handle_segments(m, &offset_seg, stcb, asoc, last_tsn, &biggest_tsn_acked,
			&biggest_tsn_newly_acked, &this_sack_lowest_newack,
			num_seg, num_nr_seg, &rto_ok)) {
			wake_him++;
		}

		if (SCTP_TSN_GE(biggest_tsn_acked, send_s)) {

			SCTP_PRINTF("Hopeless peer! biggest_tsn_acked:%x largest seq:%x\n",
				    biggest_tsn_acked, send_s);
			goto hopeless_peer;
		}
	}

	if (asoc->sctp_cmt_on_off > 0) {
		TAILQ_FOREACH(net, &asoc->nets, sctp_next) {
			if (net->new_pseudo_cumack)
				sctp_timer_stop(SCTP_TIMER_TYPE_SEND, stcb->sctp_ep,
				                stcb, net,
				                SCTP_FROM_SCTP_INDATA + SCTP_LOC_31);
		}
	} else {
		if (accum_moved) {
			TAILQ_FOREACH(net, &asoc->nets, sctp_next) {
				sctp_timer_stop(SCTP_TIMER_TYPE_SEND, stcb->sctp_ep,
				                stcb, net, SCTP_FROM_SCTP_INDATA + SCTP_LOC_32);
			}
		}
	}

	asoc->last_acked_seq = cum_ack;

	TAILQ_FOREACH_SAFE(tp1, &asoc->sent_queue, sctp_next, tp2) {
		if (SCTP_TSN_GT(tp1->rec.data.tsn, cum_ack)) {
			break;
		}
		if (tp1->sent != SCTP_DATAGRAM_NR_ACKED) {
			if (asoc->strmout[tp1->rec.data.sid].chunks_on_queues > 0) {
				asoc->strmout[tp1->rec.data.sid].chunks_on_queues--;
#ifdef INVARIANTS
			} else {
				panic("No chunks on the queues for sid %u.", tp1->rec.data.sid);
#endif
			}
		}
		if ((asoc->strmout[tp1->rec.data.sid].chunks_on_queues == 0) &&
		    (asoc->strmout[tp1->rec.data.sid].state == SCTP_STREAM_RESET_PENDING) &&
		    TAILQ_EMPTY(&asoc->strmout[tp1->rec.data.sid].outqueue)) {
			asoc->trigger_reset = 1;
		}
		TAILQ_REMOVE(&asoc->sent_queue, tp1, sctp_next);
		if (PR_SCTP_ENABLED(tp1->flags)) {
			if (asoc->pr_sctp_cnt != 0)
				asoc->pr_sctp_cnt--;
		}
		asoc->sent_queue_cnt--;
		if (tp1->data) {

			sctp_free_bufspace(stcb, asoc, tp1, 1);
			sctp_m_freem(tp1->data);
			tp1->data = NULL;
			if (asoc->prsctp_supported && PR_SCTP_BUF_ENABLED(tp1->flags)) {
				asoc->sent_queue_cnt_removeable--;
			}
		}
		if (SCTP_BASE_SYSCTL(sctp_logging_level) & SCTP_SACK_LOGGING_ENABLE) {
			sctp_log_sack(asoc->last_acked_seq,
			              cum_ack,
			              tp1->rec.data.tsn,
			              0,
			              0,
			              SCTP_LOG_FREE_SENT);
		}
		sctp_free_a_chunk(stcb, tp1, SCTP_SO_NOT_LOCKED);
		wake_him++;
	}
	if (TAILQ_EMPTY(&asoc->sent_queue) && (asoc->total_flight > 0)) {
#ifdef INVARIANTS
		panic("Warning flight size is positive and should be 0");
#else
		SCTP_PRINTF("Warning flight size incorrect should be 0 is %d\n",
		            asoc->total_flight);
#endif
		asoc->total_flight = 0;
	}

#if defined(__Userspace__)
	if (stcb->sctp_ep->recv_callback) {
		if (stcb->sctp_socket) {
			uint32_t inqueue_bytes, sb_free_now;
			struct sctp_inpcb *inp;

			inp = stcb->sctp_ep;
			inqueue_bytes = stcb->asoc.total_output_queue_size - (stcb->asoc.chunks_on_out_queue * sizeof(struct sctp_data_chunk));
			sb_free_now = SCTP_SB_LIMIT_SND(stcb->sctp_socket) - (inqueue_bytes + stcb->asoc.sb_send_resv);

			if (inp->send_callback &&
			   (((inp->send_sb_threshold > 0) && (sb_free_now >= inp->send_sb_threshold)) ||
			    (inp->send_sb_threshold == 0))) {
				atomic_add_int(&stcb->asoc.refcnt, 1);
				SCTP_TCB_UNLOCK(stcb);
				inp->send_callback(stcb->sctp_socket, sb_free_now, inp->ulp_info);
				SCTP_TCB_LOCK(stcb);
				atomic_subtract_int(&stcb->asoc.refcnt, 1);
			}
		}
	} else if ((wake_him) && (stcb->sctp_socket)) {
#else

	if ((wake_him) && (stcb->sctp_socket)) {
#endif
#if defined(__APPLE__) && !defined(__Userspace__)
		struct socket *so;

#endif
		SOCKBUF_LOCK(&stcb->sctp_socket->so_snd);
		if (SCTP_BASE_SYSCTL(sctp_logging_level) & SCTP_WAKE_LOGGING_ENABLE) {
			sctp_wakeup_log(stcb, wake_him, SCTP_WAKESND_FROM_SACK);
		}
#if defined(__APPLE__) && !defined(__Userspace__)
		so = SCTP_INP_SO(stcb->sctp_ep);
		atomic_add_int(&stcb->asoc.refcnt, 1);
		SCTP_TCB_UNLOCK(stcb);
		SCTP_SOCKET_LOCK(so, 1);
		SCTP_TCB_LOCK(stcb);
		atomic_subtract_int(&stcb->asoc.refcnt, 1);
		if (stcb->asoc.state & SCTP_STATE_CLOSED_SOCKET) {

			SCTP_SOCKET_UNLOCK(so, 1);
			return;
		}
#endif
		sctp_sowwakeup_locked(stcb->sctp_ep, stcb->sctp_socket);
#if defined(__APPLE__) && !defined(__Userspace__)
		SCTP_SOCKET_UNLOCK(so, 1);
#endif
	} else {
		if (SCTP_BASE_SYSCTL(sctp_logging_level) & SCTP_WAKE_LOGGING_ENABLE) {
			sctp_wakeup_log(stcb, wake_him, SCTP_NOWAKE_FROM_SACK);
		}
	}

	if (asoc->fast_retran_loss_recovery && accum_moved) {
		if (SCTP_TSN_GE(asoc->last_acked_seq, asoc->fast_recovery_tsn)) {

			will_exit_fast_recovery = 1;
		}
	}

	if (num_seg) {
		sctp_check_for_revoked(stcb, asoc, cum_ack, biggest_tsn_acked);
		asoc->saw_sack_with_frags = 1;
	} else if (asoc->saw_sack_with_frags) {
		int cnt_revoked = 0;

		TAILQ_FOREACH(tp1, &asoc->sent_queue, sctp_next) {
			if (tp1->sent == SCTP_DATAGRAM_ACKED) {
				tp1->sent = SCTP_DATAGRAM_SENT;
				if (SCTP_BASE_SYSCTL(sctp_logging_level) & SCTP_FLIGHT_LOGGING_ENABLE) {
					sctp_misc_ints(SCTP_FLIGHT_LOG_UP_REVOKE,
					               tp1->whoTo->flight_size,
					               tp1->book_size,
					               (uint32_t)(uintptr_t)tp1->whoTo,
					               tp1->rec.data.tsn);
				}
				sctp_flight_size_increase(tp1);
				sctp_total_flight_increase(stcb, tp1);
				tp1->rec.data.chunk_was_revoked = 1;

				tp1->whoTo->cwnd += tp1->book_size;
				cnt_revoked++;
			}
		}
		if (cnt_revoked) {
			reneged_all = 1;
		}
		asoc->saw_sack_with_frags = 0;
	}
	if (num_nr_seg > 0)
		asoc->saw_sack_with_nr_frags = 1;
	else
		asoc->saw_sack_with_nr_frags = 0;

	if (ecne_seen == 0) {
		TAILQ_FOREACH(net, &asoc->nets, sctp_next) {
			if (net->net_ack2 > 0) {

				net->error_count = 0;
				if (!(net->dest_state & SCTP_ADDR_REACHABLE)) {

					net->dest_state |= SCTP_ADDR_REACHABLE;
					sctp_ulp_notify(SCTP_NOTIFY_INTERFACE_UP, stcb,
					                0, (void *)net, SCTP_SO_NOT_LOCKED);
				}

				if (net == stcb->asoc.primary_destination) {
					if (stcb->asoc.alternate) {

						sctp_free_remote_addr(stcb->asoc.alternate);
						stcb->asoc.alternate = NULL;
					}
				}

				if (net->dest_state & SCTP_ADDR_PF) {
					net->dest_state &= ~SCTP_ADDR_PF;
					sctp_timer_stop(SCTP_TIMER_TYPE_HEARTBEAT,
					                stcb->sctp_ep, stcb, net,
					                SCTP_FROM_SCTP_INDATA + SCTP_LOC_33);
					sctp_timer_start(SCTP_TIMER_TYPE_HEARTBEAT, stcb->sctp_ep, stcb, net);
					asoc->cc_functions.sctp_cwnd_update_exit_pf(stcb, net);

					net->net_ack = 0;
				}

				net->RTO = (net->lastsa >> SCTP_RTT_SHIFT) + net->lastsv;
				if (net->RTO < stcb->asoc.minrto) {
					net->RTO = stcb->asoc.minrto;
				}
				if (net->RTO > stcb->asoc.maxrto) {
					net->RTO = stcb->asoc.maxrto;
				}
			}
		}
		asoc->cc_functions.sctp_cwnd_update_after_sack(stcb, asoc, accum_moved, reneged_all, will_exit_fast_recovery);
	}

	if (TAILQ_EMPTY(&asoc->sent_queue)) {

		TAILQ_FOREACH(net, &asoc->nets, sctp_next) {

			sctp_timer_stop(SCTP_TIMER_TYPE_SEND, stcb->sctp_ep,
			                stcb, net,
			                SCTP_FROM_SCTP_INDATA + SCTP_LOC_34);
			net->flight_size = 0;
			net->partial_bytes_acked = 0;
		}
		asoc->total_flight = 0;
		asoc->total_flight_count = 0;
	}

	if (TAILQ_EMPTY(&asoc->send_queue) && TAILQ_EMPTY(&asoc->sent_queue)) {

		if (SCTP_BASE_SYSCTL(sctp_logging_level) & SCTP_LOG_RWND_ENABLE) {
			sctp_log_rwnd_set(SCTP_SET_PEER_RWND_VIA_SACK,
			                  asoc->peers_rwnd, 0, 0, a_rwnd);
		}
		asoc->peers_rwnd = a_rwnd;
		if (asoc->peers_rwnd < stcb->sctp_ep->sctp_ep.sctp_sws_sender) {

			asoc->peers_rwnd = 0;
		}

		if ((asoc->stream_queue_cnt == 1) &&
		    ((asoc->state & SCTP_STATE_SHUTDOWN_PENDING) ||
		     (SCTP_GET_STATE(stcb) == SCTP_STATE_SHUTDOWN_RECEIVED)) &&
		    ((*asoc->ss_functions.sctp_ss_is_user_msgs_incomplete)(stcb, asoc))) {
			SCTP_ADD_SUBSTATE(stcb, SCTP_STATE_PARTIAL_MSG_LEFT);
		}
		if (((asoc->state & SCTP_STATE_SHUTDOWN_PENDING) ||
		     (SCTP_GET_STATE(stcb) == SCTP_STATE_SHUTDOWN_RECEIVED)) &&
		    (asoc->stream_queue_cnt == 1) &&
		    (asoc->state & SCTP_STATE_PARTIAL_MSG_LEFT)) {
			struct mbuf *op_err;

			*abort_now = 1;

			op_err = sctp_generate_cause(SCTP_CAUSE_USER_INITIATED_ABT, "");
			stcb->sctp_ep->last_abort_code = SCTP_FROM_SCTP_INDATA + SCTP_LOC_35;
			sctp_abort_an_association(stcb->sctp_ep, stcb, op_err, false, SCTP_SO_NOT_LOCKED);
			return;
		}
		if ((asoc->state & SCTP_STATE_SHUTDOWN_PENDING) &&
		    (asoc->stream_queue_cnt == 0)) {
			struct sctp_nets *netp;

			if ((SCTP_GET_STATE(stcb) == SCTP_STATE_OPEN) ||
			    (SCTP_GET_STATE(stcb) == SCTP_STATE_SHUTDOWN_RECEIVED)) {
				SCTP_STAT_DECR_GAUGE32(sctps_currestab);
			}
			SCTP_SET_STATE(stcb, SCTP_STATE_SHUTDOWN_SENT);
			sctp_stop_timers_for_shutdown(stcb);
			if (asoc->alternate) {
				netp = asoc->alternate;
			} else {
				netp = asoc->primary_destination;
			}
			sctp_send_shutdown(stcb, netp);
			sctp_timer_start(SCTP_TIMER_TYPE_SHUTDOWN,
					 stcb->sctp_ep, stcb, netp);
			sctp_timer_start(SCTP_TIMER_TYPE_SHUTDOWNGUARD,
					 stcb->sctp_ep, stcb, NULL);
			return;
		} else if ((SCTP_GET_STATE(stcb) == SCTP_STATE_SHUTDOWN_RECEIVED) &&
			   (asoc->stream_queue_cnt == 0)) {
			struct sctp_nets *netp;

			SCTP_STAT_DECR_GAUGE32(sctps_currestab);
			SCTP_SET_STATE(stcb, SCTP_STATE_SHUTDOWN_ACK_SENT);
			sctp_stop_timers_for_shutdown(stcb);
			if (asoc->alternate) {
				netp = asoc->alternate;
			} else {
				netp = asoc->primary_destination;
			}
			sctp_send_shutdown_ack(stcb, netp);
			sctp_timer_start(SCTP_TIMER_TYPE_SHUTDOWNACK,
			                 stcb->sctp_ep, stcb, netp);
			return;
		}
	}

	TAILQ_FOREACH(net, &asoc->nets, sctp_next) {
		net->net_ack = 0;
	}

	if ((asoc->sctp_cmt_on_off > 0) &&
	    SCTP_BASE_SYSCTL(sctp_cmt_use_dac) &&
	    (cmt_dac_flag == 0)) {
		this_sack_lowest_newack = cum_ack;
	}
	if ((num_seg > 0) || (num_nr_seg > 0)) {
		sctp_strike_gap_ack_chunks(stcb, asoc, biggest_tsn_acked,
		                           biggest_tsn_newly_acked, this_sack_lowest_newack, accum_moved);
	}

	asoc->cc_functions.sctp_cwnd_update_after_fr(stcb, asoc);

	if (will_exit_fast_recovery) {

		asoc->fast_retran_loss_recovery = 0;
	}
	if ((asoc->sat_t3_loss_recovery) &&
	    SCTP_TSN_GE(asoc->last_acked_seq, asoc->sat_t3_recovery_tsn)) {

		asoc->sat_t3_loss_recovery = 0;
	}

	TAILQ_FOREACH(net, &asoc->nets, sctp_next) {
		if (net->will_exit_fast_recovery) {

			net->fast_retran_loss_recovery = 0;
		}
	}

	if (SCTP_BASE_SYSCTL(sctp_logging_level) & SCTP_LOG_RWND_ENABLE) {
		sctp_log_rwnd_set(SCTP_SET_PEER_RWND_VIA_SACK,
		                  asoc->peers_rwnd, asoc->total_flight, (asoc->total_flight_count * SCTP_BASE_SYSCTL(sctp_peer_chunk_oh)), a_rwnd);
	}
	asoc->peers_rwnd = sctp_sbspace_sub(a_rwnd,
	                                    (uint32_t) (asoc->total_flight + (asoc->total_flight_count * SCTP_BASE_SYSCTL(sctp_peer_chunk_oh))));
	if (asoc->peers_rwnd < stcb->sctp_ep->sctp_ep.sctp_sws_sender) {

		asoc->peers_rwnd = 0;
	}
	if (asoc->peers_rwnd > old_rwnd) {
		win_probe_recovery = 1;
	}

	done_once = 0;
again:
	j = 0;
	TAILQ_FOREACH(net, &asoc->nets, sctp_next) {
		if (win_probe_recovery && (net->window_probe)) {
			win_probe_recovered = 1;

			TAILQ_FOREACH(tp1, &asoc->sent_queue, sctp_next) {
				if (tp1->window_probe) {
					sctp_window_probe_recovery(stcb, asoc, tp1);
					break;
				}
			}
		}
		if (net->flight_size) {
			j++;
			if (!SCTP_OS_TIMER_PENDING(&net->rxt_timer.timer)) {
				sctp_timer_start(SCTP_TIMER_TYPE_SEND,
				                 stcb->sctp_ep, stcb, net);
			}
			if (net->window_probe) {
				net->window_probe = 0;
			}
		} else {
			if (net->window_probe) {

				if (!SCTP_OS_TIMER_PENDING(&net->rxt_timer.timer)) {
					sctp_timer_start(SCTP_TIMER_TYPE_SEND,
					                 stcb->sctp_ep, stcb, net);
				}
			} else if (SCTP_OS_TIMER_PENDING(&net->rxt_timer.timer)) {
				sctp_timer_stop(SCTP_TIMER_TYPE_SEND, stcb->sctp_ep,
				                stcb, net,
				                SCTP_FROM_SCTP_INDATA + SCTP_LOC_36);
			}
		}
	}
	if ((j == 0) &&
	    (!TAILQ_EMPTY(&asoc->sent_queue)) &&
	    (asoc->sent_queue_retran_cnt == 0) &&
	    (win_probe_recovered == 0) &&
	    (done_once == 0)) {

		if (sctp_fs_audit(asoc)) {
			TAILQ_FOREACH(net, &asoc->nets, sctp_next) {
				net->flight_size = 0;
			}
			asoc->total_flight = 0;
			asoc->total_flight_count = 0;
			asoc->sent_queue_retran_cnt = 0;
			TAILQ_FOREACH(tp1, &asoc->sent_queue, sctp_next) {
				if (tp1->sent < SCTP_DATAGRAM_RESEND) {
					sctp_flight_size_increase(tp1);
					sctp_total_flight_increase(stcb, tp1);
				} else if (tp1->sent == SCTP_DATAGRAM_RESEND) {
					sctp_ucount_incr(asoc->sent_queue_retran_cnt);
				}
			}
		}
		done_once = 1;
		goto again;
	}

	if (SCTP_TSN_GT(cum_ack, asoc->advanced_peer_ack_point)) {
		asoc->advanced_peer_ack_point = cum_ack;
	}

	if ((asoc->prsctp_supported) && (asoc->pr_sctp_cnt > 0)) {
		struct sctp_tmit_chunk *lchk;
		uint32_t old_adv_peer_ack_point;

		old_adv_peer_ack_point = asoc->advanced_peer_ack_point;
		lchk = sctp_try_advance_peer_ack_point(stcb, asoc);

		if (SCTP_TSN_GT(asoc->advanced_peer_ack_point, cum_ack)) {

			if (SCTP_BASE_SYSCTL(sctp_logging_level) & SCTP_LOG_TRY_ADVANCE) {
				sctp_misc_ints(SCTP_FWD_TSN_CHECK,
				               0xee, cum_ack, asoc->advanced_peer_ack_point,
				               old_adv_peer_ack_point);
			}
			if (SCTP_TSN_GT(asoc->advanced_peer_ack_point, old_adv_peer_ack_point)) {
				send_forward_tsn(stcb, asoc);
			} else if (lchk) {

				if (lchk->rec.data.fwd_tsn_cnt >= 3) {
					send_forward_tsn(stcb, asoc);
				}
			}
		}
		for (; lchk != NULL; lchk = TAILQ_NEXT(lchk, sctp_next)) {
			if (lchk->whoTo != NULL) {
				break;
			}
		}
		if (lchk != NULL) {

			sctp_timer_start(SCTP_TIMER_TYPE_SEND,
			                 stcb->sctp_ep, stcb, lchk->whoTo);
		}
	}
	if (SCTP_BASE_SYSCTL(sctp_logging_level) & SCTP_SACK_RWND_LOGGING_ENABLE) {
		sctp_misc_ints(SCTP_SACK_RWND_UPDATE,
		               a_rwnd,
		               stcb->asoc.peers_rwnd,
		               stcb->asoc.total_flight,
		               stcb->asoc.total_output_queue_size);
	}
}

void
sctp_update_acked(struct sctp_tcb *stcb, struct sctp_shutdown_chunk *cp, int *abort_flag)
{

	uint32_t cum_ack, a_rwnd;

	cum_ack = ntohl(cp->cumulative_tsn_ack);

	a_rwnd = stcb->asoc.peers_rwnd + stcb->asoc.total_flight;

	sctp_express_handle_sack(stcb, cum_ack, a_rwnd, abort_flag, 0);
}

static void
sctp_kick_prsctp_reorder_queue(struct sctp_tcb *stcb,
			       struct sctp_stream_in *strmin)
{
	struct sctp_queued_to_read *control, *ncontrol;
	struct sctp_association *asoc;
	uint32_t mid;
	int need_reasm_check = 0;

	asoc = &stcb->asoc;
	mid = strmin->last_mid_delivered;

	TAILQ_FOREACH_SAFE(control, &strmin->inqueue, next_instrm, ncontrol) {
		if (SCTP_MID_GE(asoc->idata_supported, mid, control->mid)) {

			if (((control->sinfo_flags >> 8) & SCTP_DATA_NOT_FRAG)  == SCTP_DATA_NOT_FRAG) {
				if (control->on_strm_q) {
					if (control->on_strm_q == SCTP_ON_ORDERED) {
						TAILQ_REMOVE(&strmin->inqueue, control, next_instrm);
					} else if (control->on_strm_q == SCTP_ON_UNORDERED) {
						TAILQ_REMOVE(&strmin->uno_inqueue, control, next_instrm);
#ifdef INVARIANTS
					} else {
						panic("strmin: %p ctl: %p unknown %d",
						      strmin, control, control->on_strm_q);
#endif
					}
					control->on_strm_q = 0;
				}

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

				if (stcb->sctp_socket) {
					sctp_mark_non_revokable(asoc, control->sinfo_tsn);
					sctp_add_to_readq(stcb->sctp_ep, stcb,
							  control,
							  &stcb->sctp_socket->so_rcv,
							  1, SCTP_READ_LOCK_HELD,
							  SCTP_SO_NOT_LOCKED);
				}
			} else {

				if (control->first_frag_seen) {

					strmin->last_mid_delivered = control->mid - 1;
					need_reasm_check = 1;
					break;
				}
			}
		} else {

			break;
		}
	}
	if (need_reasm_check) {
		int ret;
		ret = sctp_deliver_reasm_check(stcb, &stcb->asoc, strmin, SCTP_READ_LOCK_HELD);
		if (SCTP_MID_GT(asoc->idata_supported, mid, strmin->last_mid_delivered)) {

			strmin->last_mid_delivered = mid;
		}
		if (ret == 0) {

			return;
		}
		need_reasm_check = 0;
	}

	mid = strmin->last_mid_delivered + 1;
	TAILQ_FOREACH_SAFE(control, &strmin->inqueue, next_instrm, ncontrol) {
		if (SCTP_MID_EQ(asoc->idata_supported, mid, control->mid)) {
			if (((control->sinfo_flags >> 8) & SCTP_DATA_NOT_FRAG) == SCTP_DATA_NOT_FRAG) {

				if (control->on_strm_q) {
					if (control->on_strm_q == SCTP_ON_ORDERED) {
						TAILQ_REMOVE(&strmin->inqueue, control, next_instrm);
					} else if (control->on_strm_q == SCTP_ON_UNORDERED) {
						TAILQ_REMOVE(&strmin->uno_inqueue, control, next_instrm);
#ifdef INVARIANTS
					} else {
						panic("strmin: %p ctl: %p unknown %d",
						      strmin, control, control->on_strm_q);
#endif
					}
					control->on_strm_q = 0;
				}

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

				strmin->last_mid_delivered = control->mid;
				if (stcb->sctp_socket) {
					sctp_mark_non_revokable(asoc, control->sinfo_tsn);
					sctp_add_to_readq(stcb->sctp_ep, stcb,
							  control,
							  &stcb->sctp_socket->so_rcv, 1,
							  SCTP_READ_LOCK_HELD, SCTP_SO_NOT_LOCKED);
				}
				mid = strmin->last_mid_delivered + 1;
			} else {

				if (control->first_frag_seen) {

					strmin->last_mid_delivered = control->mid - 1;
					need_reasm_check = 1;
					break;
				}
			}
		} else {
			break;
		}
	}
	if (need_reasm_check) {
		(void)sctp_deliver_reasm_check(stcb, &stcb->asoc, strmin, SCTP_READ_LOCK_HELD);
	}
}

static void
sctp_flush_reassm_for_str_seq(struct sctp_tcb *stcb,
	struct sctp_association *asoc, struct sctp_stream_in *strm,
	struct sctp_queued_to_read *control, int ordered, uint32_t cumtsn)
{
	struct sctp_tmit_chunk *chk, *nchk;

	if (!asoc->idata_supported && !ordered &&
	    control->first_frag_seen &&
	    SCTP_TSN_GT(control->fsn_included, cumtsn)) {
		return;
	}
	TAILQ_FOREACH_SAFE(chk, &control->reasm, sctp_next, nchk) {

		if (!asoc->idata_supported && !ordered) {
			if (SCTP_TSN_GT(chk->rec.data.tsn, cumtsn)) {
				break;
			}
		}
		TAILQ_REMOVE(&control->reasm, chk, sctp_next);
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
		if (chk->data) {
			sctp_m_freem(chk->data);
			chk->data = NULL;
		}
		sctp_free_a_chunk(stcb, chk, SCTP_SO_NOT_LOCKED);
	}
	if (!TAILQ_EMPTY(&control->reasm)) {

		if (control->data) {
			sctp_m_freem(control->data);
			control->data = NULL;
		}
		sctp_reset_a_control(control, stcb->sctp_ep, cumtsn);
		chk = TAILQ_FIRST(&control->reasm);
		if (chk->rec.data.rcv_flags & SCTP_DATA_FIRST_FRAG) {
			TAILQ_REMOVE(&control->reasm, chk, sctp_next);
			sctp_add_chk_to_control(control, strm, stcb, asoc,
						chk, SCTP_READ_LOCK_HELD);
		}
		sctp_deliver_reasm_check(stcb, asoc, strm, SCTP_READ_LOCK_HELD);
		return;
	}
	if (control->on_strm_q == SCTP_ON_ORDERED) {
		TAILQ_REMOVE(&strm->inqueue, control, next_instrm);
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
		control->on_strm_q = 0;
	} else if (control->on_strm_q == SCTP_ON_UNORDERED) {
		TAILQ_REMOVE(&strm->uno_inqueue, control, next_instrm);
		control->on_strm_q = 0;
#ifdef INVARIANTS
	} else if (control->on_strm_q) {
		panic("strm: %p ctl: %p unknown %d",
		    strm, control, control->on_strm_q);
#endif
	}
	control->on_strm_q = 0;
	if (control->on_read_q == 0) {
		sctp_free_remote_addr(control->whoFrom);
		if (control->data) {
			sctp_m_freem(control->data);
			control->data = NULL;
		}
		sctp_free_a_readq(stcb, control);
	}
}

void
sctp_handle_forward_tsn(struct sctp_tcb *stcb,
                        struct sctp_forward_tsn_chunk *fwd,
                        int *abort_flag, struct mbuf *m , int offset)
{

	struct sctp_association *asoc;
	uint32_t new_cum_tsn, gap;
	unsigned int i, fwd_sz, m_size;
	uint32_t str_seq;
	struct sctp_stream_in *strm;
	struct sctp_queued_to_read *control, *ncontrol, *sv;

	asoc = &stcb->asoc;
	if ((fwd_sz = ntohs(fwd->ch.chunk_length)) < sizeof(struct sctp_forward_tsn_chunk)) {
		SCTPDBG(SCTP_DEBUG_INDATA1,
			"Bad size too small/big fwd-tsn\n");
		return;
	}
	m_size = (stcb->asoc.mapping_array_size << 3);

	new_cum_tsn = ntohl(fwd->new_cumulative_tsn);

	if (SCTP_TSN_GE(asoc->cumulative_tsn, new_cum_tsn)) {

		return;
	}

	SCTP_CALC_TSN_TO_GAP(gap, new_cum_tsn, asoc->mapping_array_base_tsn);
	asoc->cumulative_tsn = new_cum_tsn;
	if (gap >= m_size) {
		if ((long)gap > sctp_sbspace(&stcb->asoc, &stcb->sctp_socket->so_rcv)) {
			struct mbuf *op_err;
			char msg[SCTP_DIAG_INFO_LEN];

			*abort_flag = 1;
			SCTP_SNPRINTF(msg, sizeof(msg),
			              "New cum ack %8.8x too high, highest TSN %8.8x",
			              new_cum_tsn, asoc->highest_tsn_inside_map);
			op_err = sctp_generate_cause(SCTP_CAUSE_PROTOCOL_VIOLATION, msg);
			stcb->sctp_ep->last_abort_code = SCTP_FROM_SCTP_INDATA + SCTP_LOC_37;
			sctp_abort_an_association(stcb->sctp_ep, stcb, op_err, false, SCTP_SO_NOT_LOCKED);
			return;
		}
		SCTP_STAT_INCR(sctps_fwdtsn_map_over);

		memset(stcb->asoc.mapping_array, 0, stcb->asoc.mapping_array_size);
		asoc->mapping_array_base_tsn = new_cum_tsn + 1;
		asoc->highest_tsn_inside_map = new_cum_tsn;

		memset(stcb->asoc.nr_mapping_array, 0, stcb->asoc.mapping_array_size);
		asoc->highest_tsn_inside_nr_map = new_cum_tsn;

		if (SCTP_BASE_SYSCTL(sctp_logging_level) & SCTP_MAP_LOGGING_ENABLE) {
			sctp_log_map(0, 3, asoc->highest_tsn_inside_map, SCTP_MAP_SLIDE_RESULT);
		}
	} else {
		SCTP_TCB_LOCK_ASSERT(stcb);
		for (i = 0; i <= gap; i++) {
			if (!SCTP_IS_TSN_PRESENT(asoc->mapping_array, i) &&
			    !SCTP_IS_TSN_PRESENT(asoc->nr_mapping_array, i)) {
				SCTP_SET_TSN_PRESENT(asoc->nr_mapping_array, i);
				if (SCTP_TSN_GT(asoc->mapping_array_base_tsn + i, asoc->highest_tsn_inside_nr_map)) {
					asoc->highest_tsn_inside_nr_map = asoc->mapping_array_base_tsn + i;
				}
			}
		}
	}

	if (asoc->idata_supported == 0) {
		uint16_t sid;

		SCTP_INP_READ_LOCK(stcb->sctp_ep);
		for (sid = 0; sid < asoc->streamincnt; sid++) {
			strm = &asoc->strmin[sid];
			if (!TAILQ_EMPTY(&strm->uno_inqueue)) {
				sctp_flush_reassm_for_str_seq(stcb, asoc, strm, TAILQ_FIRST(&strm->uno_inqueue), 0, new_cum_tsn);
			}
		}
		SCTP_INP_READ_UNLOCK(stcb->sctp_ep);
	}

	fwd_sz -= sizeof(*fwd);
	if (m && fwd_sz) {

		unsigned int num_str;
		uint32_t mid;
		uint16_t sid;
		uint16_t ordered, flags;
		struct sctp_strseq *stseq, strseqbuf;
		struct sctp_strseq_mid *stseq_m, strseqbuf_m;
		offset += sizeof(*fwd);

		SCTP_INP_READ_LOCK(stcb->sctp_ep);
		if (asoc->idata_supported) {
			num_str = fwd_sz / sizeof(struct sctp_strseq_mid);
		} else {
			num_str = fwd_sz / sizeof(struct sctp_strseq);
		}
		for (i = 0; i < num_str; i++) {
			if (asoc->idata_supported) {
				stseq_m = (struct sctp_strseq_mid *)sctp_m_getptr(m, offset,
									    sizeof(struct sctp_strseq_mid),
									    (uint8_t *)&strseqbuf_m);
				offset += sizeof(struct sctp_strseq_mid);
				if (stseq_m == NULL) {
					break;
				}
				sid = ntohs(stseq_m->sid);
				mid = ntohl(stseq_m->mid);
				flags = ntohs(stseq_m->flags);
				if (flags & PR_SCTP_UNORDERED_FLAG) {
					ordered = 0;
				} else {
					ordered = 1;
				}
			} else {
				stseq = (struct sctp_strseq *)sctp_m_getptr(m, offset,
									    sizeof(struct sctp_strseq),
									    (uint8_t *)&strseqbuf);
				offset += sizeof(struct sctp_strseq);
				if (stseq == NULL) {
					break;
				}
				sid = ntohs(stseq->sid);
				mid = (uint32_t)ntohs(stseq->ssn);
				ordered = 1;
			}

			if (sid >= asoc->streamincnt) {

				break;
			}
			if ((asoc->str_of_pdapi == sid) &&
			    (asoc->ssn_of_pdapi == mid)) {

				asoc->fragmented_delivery_inprogress = 0;
			}
			strm = &asoc->strmin[sid];
			if (ordered) {
				TAILQ_FOREACH_SAFE(control, &strm->inqueue, next_instrm, ncontrol) {
					if (SCTP_MID_GE(asoc->idata_supported, mid, control->mid)) {
						sctp_flush_reassm_for_str_seq(stcb, asoc, strm, control, ordered, new_cum_tsn);
					}
				}
			} else {
				if (asoc->idata_supported) {
					TAILQ_FOREACH_SAFE(control, &strm->uno_inqueue, next_instrm, ncontrol) {
						if (SCTP_MID_GE(asoc->idata_supported, mid, control->mid)) {
							sctp_flush_reassm_for_str_seq(stcb, asoc, strm, control, ordered, new_cum_tsn);
						}
					}
				} else {
					if (!TAILQ_EMPTY(&strm->uno_inqueue)) {
						sctp_flush_reassm_for_str_seq(stcb, asoc, strm, TAILQ_FIRST(&strm->uno_inqueue), ordered, new_cum_tsn);
					}
				}
			}
			TAILQ_FOREACH(control, &stcb->sctp_ep->read_queue, next) {
				if ((control->sinfo_stream == sid) &&
				    (SCTP_MID_EQ(asoc->idata_supported, control->mid, mid))) {
					str_seq = (sid << 16) | (0x0000ffff & mid);
					control->pdapi_aborted = 1;
					sv = stcb->asoc.control_pdapi;
					control->end_added = 1;
					if (control->on_strm_q == SCTP_ON_ORDERED) {
						TAILQ_REMOVE(&strm->inqueue, control, next_instrm);
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
					} else if (control->on_strm_q == SCTP_ON_UNORDERED) {
						TAILQ_REMOVE(&strm->uno_inqueue, control, next_instrm);
#ifdef INVARIANTS
					} else if (control->on_strm_q) {
						panic("strm: %p ctl: %p unknown %d",
						      strm, control, control->on_strm_q);
#endif
					}
					control->on_strm_q = 0;
					stcb->asoc.control_pdapi = control;
					sctp_ulp_notify(SCTP_NOTIFY_PARTIAL_DELVIERY_INDICATION,
					                stcb,
					                SCTP_PARTIAL_DELIVERY_ABORTED,
					                (void *)&str_seq,
							SCTP_SO_NOT_LOCKED);
					stcb->asoc.control_pdapi = sv;
					break;
				} else if ((control->sinfo_stream == sid) &&
					   SCTP_MID_GT(asoc->idata_supported, control->mid, mid)) {

					break;
				}
			}
			if (SCTP_MID_GT(asoc->idata_supported, mid, strm->last_mid_delivered)) {

				strm->last_mid_delivered = mid;
			}

			sctp_kick_prsctp_reorder_queue(stcb, strm);
		}
		SCTP_INP_READ_UNLOCK(stcb->sctp_ep);
	}

	sctp_slide_mapping_arrays(stcb);
}
