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

#define _IP_VHL
#include <netinet/sctp_os.h>
#include <netinet/sctp_pcb.h>
#ifdef INET6
#if defined(__FreeBSD__) && defined(__Userspace__)
#include <netinet6/sctp6_var.h>
#endif
#endif
#include <netinet/sctp_var.h>
#include <netinet/sctp_sysctl.h>
#include <netinet/sctp_timer.h>
#include <netinet/sctputil.h>
#include <netinet/sctp_output.h>
#include <netinet/sctp_header.h>
#include <netinet/sctp_indata.h>
#include <netinet/sctp_asconf.h>
#include <netinet/sctp_input.h>
#include <netinet/sctp.h>
#include <netinet/sctp_uio.h>
#if defined(INET) || defined(INET6)
#if !(defined(_WIN32) && defined(__Userspace__))
#include <netinet/udp.h>
#endif
#endif

void
sctp_audit_retranmission_queue(struct sctp_association *asoc)
{
	struct sctp_tmit_chunk *chk;

	SCTPDBG(SCTP_DEBUG_TIMER4, "Audit invoked on send queue cnt:%d onqueue:%d\n",
			asoc->sent_queue_retran_cnt,
			asoc->sent_queue_cnt);
	asoc->sent_queue_retran_cnt = 0;
	asoc->sent_queue_cnt = 0;
	TAILQ_FOREACH(chk, &asoc->sent_queue, sctp_next) {
		if (chk->sent == SCTP_DATAGRAM_RESEND) {
			sctp_ucount_incr(asoc->sent_queue_retran_cnt);
		}
		asoc->sent_queue_cnt++;
	}
	TAILQ_FOREACH(chk, &asoc->control_send_queue, sctp_next) {
		if (chk->sent == SCTP_DATAGRAM_RESEND) {
			sctp_ucount_incr(asoc->sent_queue_retran_cnt);
		}
	}
	TAILQ_FOREACH(chk, &asoc->asconf_send_queue, sctp_next) {
		if (chk->sent == SCTP_DATAGRAM_RESEND) {
			sctp_ucount_incr(asoc->sent_queue_retran_cnt);
		}
	}
	SCTPDBG(SCTP_DEBUG_TIMER4, "Audit completes retran:%d onqueue:%d\n",
		asoc->sent_queue_retran_cnt,
		asoc->sent_queue_cnt);
}

static int
sctp_threshold_management(struct sctp_inpcb *inp, struct sctp_tcb *stcb,
    struct sctp_nets *net, uint16_t threshold)
{
	if (net) {
		net->error_count++;
		SCTPDBG(SCTP_DEBUG_TIMER4, "Error count for %p now %d thresh:%d\n",
			(void *)net, net->error_count,
			net->failure_threshold);
		if (net->error_count > net->failure_threshold) {

			if (net->dest_state & SCTP_ADDR_REACHABLE) {
				net->dest_state &= ~SCTP_ADDR_REACHABLE;
				net->dest_state &= ~SCTP_ADDR_REQ_PRIMARY;
				net->dest_state &= ~SCTP_ADDR_PF;
				sctp_ulp_notify(SCTP_NOTIFY_INTERFACE_DOWN,
				    stcb, 0,
				    (void *)net, SCTP_SO_NOT_LOCKED);
			}
		} else if ((net->pf_threshold < net->failure_threshold) &&
		           (net->error_count > net->pf_threshold)) {
			if (!(net->dest_state & SCTP_ADDR_PF)) {
				net->dest_state |= SCTP_ADDR_PF;
				net->last_active = sctp_get_tick_count();
				sctp_send_hb(stcb, net, SCTP_SO_NOT_LOCKED);
				sctp_timer_stop(SCTP_TIMER_TYPE_HEARTBEAT,
				                inp, stcb, net,
				                SCTP_FROM_SCTP_TIMER + SCTP_LOC_1);
				sctp_timer_start(SCTP_TIMER_TYPE_HEARTBEAT, inp, stcb, net);
			}
		}
	}
	if (stcb == NULL)
		return (0);

	if (net) {
		if ((net->dest_state & SCTP_ADDR_UNCONFIRMED) == 0) {
			if (SCTP_BASE_SYSCTL(sctp_logging_level) & SCTP_THRESHOLD_LOGGING) {
				sctp_misc_ints(SCTP_THRESHOLD_INCR,
					       stcb->asoc.overall_error_count,
					       (stcb->asoc.overall_error_count+1),
					       SCTP_FROM_SCTP_TIMER,
					       __LINE__);
			}
			stcb->asoc.overall_error_count++;
		}
	} else {
		if (SCTP_BASE_SYSCTL(sctp_logging_level) & SCTP_THRESHOLD_LOGGING) {
			sctp_misc_ints(SCTP_THRESHOLD_INCR,
				       stcb->asoc.overall_error_count,
				       (stcb->asoc.overall_error_count+1),
				       SCTP_FROM_SCTP_TIMER,
				       __LINE__);
		}
		stcb->asoc.overall_error_count++;
	}
	SCTPDBG(SCTP_DEBUG_TIMER4, "Overall error count for %p now %d thresh:%u state:%x\n",
		(void *)&stcb->asoc, stcb->asoc.overall_error_count,
		(uint32_t)threshold,
		((net == NULL) ? (uint32_t) 0 : (uint32_t) net->dest_state));

	if (stcb->asoc.overall_error_count > threshold) {

		struct mbuf *op_err;

		op_err = sctp_generate_cause(SCTP_BASE_SYSCTL(sctp_diag_info_code),
		                             "Association error counter exceeded");
		inp->last_abort_code = SCTP_FROM_SCTP_TIMER + SCTP_LOC_2;
		sctp_abort_an_association(inp, stcb, op_err, true, SCTP_SO_NOT_LOCKED);
		return (1);
	}
	return (0);
}

struct sctp_nets *
sctp_find_alternate_net(struct sctp_tcb *stcb,
    struct sctp_nets *net,
    int mode)
{

	struct sctp_nets *alt, *mnet, *min_errors_net = NULL , *max_cwnd_net = NULL;
	bool looped;

	int min_errors = -1;
	uint32_t max_cwnd = 0;

	if (stcb->asoc.numnets == 1) {

		return (TAILQ_FIRST(&stcb->asoc.nets));
	}

	if (mode == 2) {
		TAILQ_FOREACH(mnet, &stcb->asoc.nets, sctp_next) {

			if (((mnet->dest_state & SCTP_ADDR_REACHABLE) != SCTP_ADDR_REACHABLE) ||
			    (mnet->dest_state & SCTP_ADDR_UNCONFIRMED)) {
				continue;
			}

			if (mnet->dest_state & SCTP_ADDR_PF) {

				if (mnet == net) {
					if (min_errors == -1) {
						min_errors = mnet->error_count + 1;
						min_errors_net = mnet;
					} else if (mnet->error_count + 1 < min_errors) {
						min_errors = mnet->error_count + 1;
						min_errors_net = mnet;
					} else if (mnet->error_count + 1 == min_errors
								&& mnet->last_active > min_errors_net->last_active) {
						min_errors_net = mnet;
						min_errors = mnet->error_count + 1;
					}
					continue;
				} else {
					if (min_errors == -1) {
						min_errors = mnet->error_count;
						min_errors_net = mnet;
					} else if (mnet->error_count < min_errors) {
						min_errors = mnet->error_count;
						min_errors_net = mnet;
					} else if (mnet->error_count == min_errors
								&& mnet->last_active > min_errors_net->last_active) {
						min_errors_net = mnet;
						min_errors = mnet->error_count;
					}
					continue;
				}
			}

			if (max_cwnd < mnet->cwnd) {
				max_cwnd_net = mnet;
				max_cwnd = mnet->cwnd;
			} else if (max_cwnd == mnet->cwnd) {
				uint32_t rndval;
				uint8_t this_random;

				if (stcb->asoc.hb_random_idx > 3) {
					rndval = sctp_select_initial_TSN(&stcb->sctp_ep->sctp_ep);
					memcpy(stcb->asoc.hb_random_values, &rndval, sizeof(stcb->asoc.hb_random_values));
					this_random = stcb->asoc.hb_random_values[0];
					stcb->asoc.hb_random_idx++;
					stcb->asoc.hb_ect_randombit = 0;
				} else {
					this_random = stcb->asoc.hb_random_values[stcb->asoc.hb_random_idx];
					stcb->asoc.hb_random_idx++;
					stcb->asoc.hb_ect_randombit = 0;
				}
				if (this_random % 2 == 1) {
					max_cwnd_net = mnet;
					max_cwnd = mnet->cwnd;
				}
			}
		}
		if (max_cwnd_net == NULL) {
			if (min_errors_net == NULL) {
				return (net);
			}
			return (min_errors_net);
		} else {
			return (max_cwnd_net);
		}
	}
	else if (mode == 1) {
		TAILQ_FOREACH(mnet, &stcb->asoc.nets, sctp_next) {
			if (((mnet->dest_state & SCTP_ADDR_REACHABLE) != SCTP_ADDR_REACHABLE) ||
			    (mnet->dest_state & SCTP_ADDR_UNCONFIRMED)) {

				continue;
			}
			if (max_cwnd < mnet->cwnd) {
				max_cwnd_net = mnet;
				max_cwnd = mnet->cwnd;
			} else if (max_cwnd == mnet->cwnd) {
				uint32_t rndval;
				uint8_t this_random;

				if (stcb->asoc.hb_random_idx > 3) {
					rndval = sctp_select_initial_TSN(&stcb->sctp_ep->sctp_ep);
					memcpy(stcb->asoc.hb_random_values, &rndval,
					    sizeof(stcb->asoc.hb_random_values));
					this_random = stcb->asoc.hb_random_values[0];
					stcb->asoc.hb_random_idx = 0;
					stcb->asoc.hb_ect_randombit = 0;
				} else {
					this_random = stcb->asoc.hb_random_values[stcb->asoc.hb_random_idx];
					stcb->asoc.hb_random_idx++;
					stcb->asoc.hb_ect_randombit = 0;
				}
				if (this_random % 2) {
					max_cwnd_net = mnet;
					max_cwnd = mnet->cwnd;
				}
			}
		}
		if (max_cwnd_net) {
			return (max_cwnd_net);
		}
	}

	if ((net != NULL) && ((net->dest_state & SCTP_ADDR_BEING_DELETED) == 0)) {
		alt = TAILQ_NEXT(net, sctp_next);
	} else {
		alt = TAILQ_FIRST(&stcb->asoc.nets);
	}
	looped = false;
	for (;;) {
		if (alt == NULL) {
			if (!looped) {
				alt = TAILQ_FIRST(&stcb->asoc.nets);
				looped = true;
			}

			if (alt == NULL) {
				break;
			}
		}
#if defined(__FreeBSD__) && !defined(__Userspace__)
		if (alt->ro.ro_nh == NULL) {
#else
		if (alt->ro.ro_rt == NULL) {
#endif
			if (alt->ro._s_addr) {
				sctp_free_ifa(alt->ro._s_addr);
				alt->ro._s_addr = NULL;
			}
			alt->src_addr_selected = 0;
		}
		if (((alt->dest_state & SCTP_ADDR_REACHABLE) == SCTP_ADDR_REACHABLE) &&
#if defined(__FreeBSD__) && !defined(__Userspace__)
		    (alt->ro.ro_nh != NULL) &&
#else
		    (alt->ro.ro_rt != NULL) &&
#endif
		    (!(alt->dest_state & SCTP_ADDR_UNCONFIRMED)) &&
		    (alt != net)) {

			break;
		}
		alt = TAILQ_NEXT(alt, sctp_next);
	}

	if (alt == NULL) {

		if ((net != NULL) && ((net->dest_state & SCTP_ADDR_BEING_DELETED) == 0)) {
			alt = TAILQ_NEXT(net, sctp_next);
		} else {
			alt = TAILQ_FIRST(&stcb->asoc.nets);
		}
		looped = false;
		for (;;) {
			if (alt == NULL) {
				if (!looped) {
					alt = TAILQ_FIRST(&stcb->asoc.nets);
					looped = true;
				}

				if (alt == NULL) {
					break;
				}
			}
			if ((!(alt->dest_state & SCTP_ADDR_UNCONFIRMED)) &&
			    (alt != net)) {

				break;
			}
			alt = TAILQ_NEXT(alt, sctp_next);
		}
	}
	if (alt == NULL) {

		if ((net != NULL) && ((net->dest_state & SCTP_ADDR_BEING_DELETED) == 0)) {
			alt = net;
		}
		if (alt == NULL) {
			alt = TAILQ_FIRST(&stcb->asoc.nets);
		}
	}
	return (alt);
}

static void
sctp_backoff_on_timeout(struct sctp_tcb *stcb,
    struct sctp_nets *net,
    int win_probe,
    int num_marked, int num_abandoned)
{
	if (net->RTO == 0) {
		if (net->RTO_measured) {
			net->RTO = stcb->asoc.minrto;
		} else {
			net->RTO = stcb->asoc.initial_rto;
		}
	}
	net->RTO <<= 1;
	if (net->RTO > stcb->asoc.maxrto) {
		net->RTO = stcb->asoc.maxrto;
	}
	if ((win_probe == 0) && (num_marked || num_abandoned)) {

		stcb->asoc.cc_functions.sctp_cwnd_update_after_timeout(stcb, net);
	}
}

#ifndef INVARIANTS
static void
sctp_recover_sent_list(struct sctp_tcb *stcb)
{
	struct sctp_tmit_chunk *chk, *nchk;
	struct sctp_association *asoc;

	asoc = &stcb->asoc;
	TAILQ_FOREACH_SAFE(chk, &asoc->sent_queue, sctp_next, nchk) {
		if (SCTP_TSN_GE(asoc->last_acked_seq, chk->rec.data.tsn)) {
			SCTP_PRINTF("Found chk:%p tsn:%x <= last_acked_seq:%x\n",
			            (void *)chk, chk->rec.data.tsn, asoc->last_acked_seq);
			if (chk->sent != SCTP_DATAGRAM_NR_ACKED) {
				if (asoc->strmout[chk->rec.data.sid].chunks_on_queues > 0) {
					asoc->strmout[chk->rec.data.sid].chunks_on_queues--;
				}
			}
			if ((asoc->strmout[chk->rec.data.sid].chunks_on_queues == 0) &&
			    (asoc->strmout[chk->rec.data.sid].state == SCTP_STREAM_RESET_PENDING) &&
			    TAILQ_EMPTY(&asoc->strmout[chk->rec.data.sid].outqueue)) {
				asoc->trigger_reset = 1;
			}
			TAILQ_REMOVE(&asoc->sent_queue, chk, sctp_next);
			if (PR_SCTP_ENABLED(chk->flags)) {
				if (asoc->pr_sctp_cnt != 0)
					asoc->pr_sctp_cnt--;
			}
			if (chk->data) {

				sctp_free_bufspace(stcb, asoc, chk, 1);
				sctp_m_freem(chk->data);
				chk->data = NULL;
				if (asoc->prsctp_supported && PR_SCTP_BUF_ENABLED(chk->flags)) {
					asoc->sent_queue_cnt_removeable--;
				}
			}
			asoc->sent_queue_cnt--;
			sctp_free_a_chunk(stcb, chk, SCTP_SO_NOT_LOCKED);
		}
	}
	SCTP_PRINTF("after recover order is as follows\n");
	TAILQ_FOREACH(chk, &asoc->sent_queue, sctp_next) {
		SCTP_PRINTF("chk:%p TSN:%x\n", (void *)chk, chk->rec.data.tsn);
	}
}
#endif

static int
sctp_mark_all_for_resend(struct sctp_tcb *stcb,
    struct sctp_nets *net,
    struct sctp_nets *alt,
    int window_probe,
    int *num_marked,
    int *num_abandoned)
{

	struct sctp_tmit_chunk *chk, *nchk;
	struct sctp_nets *lnets;
	struct timeval now, min_wait, tv;
	int cur_rto;
	int cnt_abandoned;
	int audit_tf, num_mk, fir;
	unsigned int cnt_mk;
	uint32_t orig_flight, orig_tf;
	uint32_t tsnlast, tsnfirst;
	int recovery_cnt = 0;

	audit_tf = 0;
	fir = 0;

	(void)SCTP_GETTIME_TIMEVAL(&now);

	cur_rto = (net->lastsa >> SCTP_RTT_SHIFT) + net->lastsv;
	cur_rto *= 1000;
	if (SCTP_BASE_SYSCTL(sctp_logging_level) & SCTP_FR_LOGGING_ENABLE) {
		sctp_log_fr(cur_rto,
			    stcb->asoc.peers_rwnd,
			    window_probe,
			    SCTP_FR_T3_MARK_TIME);
		sctp_log_fr(net->flight_size, 0, 0, SCTP_FR_CWND_REPORT);
		sctp_log_fr(net->flight_size, net->cwnd, stcb->asoc.total_flight, SCTP_FR_CWND_REPORT);
	}
	tv.tv_sec = cur_rto / 1000000;
	tv.tv_usec = cur_rto % 1000000;
#if !(defined(__FreeBSD__) && !defined(__Userspace__))
	timersub(&now, &tv, &min_wait);
#else
	min_wait = now;
	timevalsub(&min_wait, &tv);
#endif
	if (min_wait.tv_sec < 0 || min_wait.tv_usec < 0) {

		min_wait.tv_sec = min_wait.tv_usec = 0;
	}
	if (SCTP_BASE_SYSCTL(sctp_logging_level) & SCTP_FR_LOGGING_ENABLE) {
		sctp_log_fr(cur_rto, (uint32_t)now.tv_sec, now.tv_usec, SCTP_FR_T3_MARK_TIME);
		sctp_log_fr(0, (uint32_t)min_wait.tv_sec, min_wait.tv_usec, SCTP_FR_T3_MARK_TIME);
	}

	orig_flight = net->flight_size;
	orig_tf = stcb->asoc.total_flight;

	net->fast_retran_ip = 0;

	cnt_abandoned = 0;
	num_mk = cnt_mk = 0;
	tsnfirst = tsnlast = 0;
#ifndef INVARIANTS
 start_again:
#endif
	TAILQ_FOREACH_SAFE(chk, &stcb->asoc.sent_queue, sctp_next, nchk) {
		if (SCTP_TSN_GE(stcb->asoc.last_acked_seq, chk->rec.data.tsn)) {

			SCTP_PRINTF("Our list is out of order? last_acked:%x chk:%x\n",
			            (unsigned int)stcb->asoc.last_acked_seq, (unsigned int)chk->rec.data.tsn);
			recovery_cnt++;
#ifdef INVARIANTS
			panic("last acked >= chk on sent-Q");
#else
			SCTP_PRINTF("Recover attempts a restart cnt:%d\n", recovery_cnt);
			sctp_recover_sent_list(stcb);
			if (recovery_cnt < 10) {
				goto start_again;
			} else {
				SCTP_PRINTF("Recovery fails %d times??\n", recovery_cnt);
			}
#endif
		}
		if ((chk->whoTo == net) && (chk->sent < SCTP_DATAGRAM_ACKED)) {

			if (SCTP_BASE_SYSCTL(sctp_logging_level) & SCTP_FR_LOGGING_ENABLE) {
				sctp_log_fr(chk->rec.data.tsn,
					    (uint32_t)chk->sent_rcv_time.tv_sec,
					    chk->sent_rcv_time.tv_usec,
					    SCTP_FR_T3_MARK_TIME);
			}
			if ((chk->sent_rcv_time.tv_sec > min_wait.tv_sec) && (window_probe == 0)) {

				if (SCTP_BASE_SYSCTL(sctp_logging_level) & SCTP_FR_LOGGING_ENABLE) {
					sctp_log_fr(0,
						    (uint32_t)chk->sent_rcv_time.tv_sec,
						    chk->sent_rcv_time.tv_usec,
						    SCTP_FR_T3_STOPPED);
				}
				continue;
			} else if ((chk->sent_rcv_time.tv_sec == min_wait.tv_sec) &&
				   (window_probe == 0)) {

				if (chk->sent_rcv_time.tv_usec >= min_wait.tv_usec) {

					continue;
				}
			}
			if (stcb->asoc.prsctp_supported && PR_SCTP_TTL_ENABLED(chk->flags)) {

#if !(defined(__FreeBSD__) && !defined(__Userspace__))
				if (timercmp(&now, &chk->rec.data.timetodrop, >)) {
#else
				if (timevalcmp(&now, &chk->rec.data.timetodrop, >)) {
#endif

					if (chk->data) {
						(void)sctp_release_pr_sctp_chunk(stcb,
										 chk,
										 1,
										 SCTP_SO_NOT_LOCKED);
						cnt_abandoned++;
					}
					continue;
				}
			}
			if (stcb->asoc.prsctp_supported && PR_SCTP_RTX_ENABLED(chk->flags)) {

				if (chk->snd_count > chk->rec.data.timetodrop.tv_sec) {
					if (chk->data) {
						(void)sctp_release_pr_sctp_chunk(stcb,
										 chk,
										 1,
										 SCTP_SO_NOT_LOCKED);
						cnt_abandoned++;
					}
					continue;
				}
			}
			if (chk->sent < SCTP_DATAGRAM_RESEND) {
				sctp_ucount_incr(stcb->asoc.sent_queue_retran_cnt);
				num_mk++;
				if (fir == 0) {
					fir = 1;
					tsnfirst = chk->rec.data.tsn;
				}
				tsnlast = chk->rec.data.tsn;
				if (SCTP_BASE_SYSCTL(sctp_logging_level) & SCTP_FR_LOGGING_ENABLE) {
					sctp_log_fr(chk->rec.data.tsn, chk->snd_count,
						    0, SCTP_FR_T3_MARKED);
				}

				if (chk->rec.data.chunk_was_revoked) {

					chk->whoTo->cwnd -= chk->book_size;
					chk->rec.data.chunk_was_revoked = 0;
				}
				net->marked_retrans++;
				stcb->asoc.marked_retrans++;
				if (SCTP_BASE_SYSCTL(sctp_logging_level) & SCTP_FLIGHT_LOGGING_ENABLE) {
					sctp_misc_ints(SCTP_FLIGHT_LOG_DOWN_RSND_TO,
						       chk->whoTo->flight_size,
						       chk->book_size,
						       (uint32_t)(uintptr_t)chk->whoTo,
						       chk->rec.data.tsn);
				}
				sctp_flight_size_decrease(chk);
				sctp_total_flight_decrease(stcb, chk);
				stcb->asoc.peers_rwnd += chk->send_size;
				stcb->asoc.peers_rwnd += SCTP_BASE_SYSCTL(sctp_peer_chunk_oh);
			}
			chk->sent = SCTP_DATAGRAM_RESEND;
			chk->flags |= CHUNK_FLAGS_FRAGMENT_OK;
			SCTP_STAT_INCR(sctps_markedretrans);

			chk->rec.data.doing_fast_retransmit = 0;

			if (chk->do_rtt) {
				if (chk->whoTo->rto_needed == 0) {
					chk->whoTo->rto_needed = 1;
				}
			}
			chk->do_rtt = 0;
			if (alt != net) {
				sctp_free_remote_addr(chk->whoTo);
				chk->no_fr_allowed = 1;
				chk->whoTo = alt;
				atomic_add_int(&alt->ref_count, 1);
			} else {
				chk->no_fr_allowed = 0;
				if (TAILQ_EMPTY(&stcb->asoc.send_queue)) {
					chk->rec.data.fast_retran_tsn = stcb->asoc.sending_seq;
				} else {
					chk->rec.data.fast_retran_tsn = (TAILQ_FIRST(&stcb->asoc.send_queue))->rec.data.tsn;
				}
			}

			if (stcb->asoc.sctp_cmt_on_off > 0) {
				chk->no_fr_allowed = 1;
			}
#ifdef THIS_SHOULD_NOT_BE_DONE
		} else if (chk->sent == SCTP_DATAGRAM_ACKED) {

			could_be_sent = chk;
#endif
		}
		if (chk->sent == SCTP_DATAGRAM_RESEND) {
			cnt_mk++;
		}
	}
	if ((orig_flight - net->flight_size) != (orig_tf - stcb->asoc.total_flight)) {

		audit_tf = 1;
	}

	if (SCTP_BASE_SYSCTL(sctp_logging_level) & SCTP_FR_LOGGING_ENABLE) {
		sctp_log_fr(tsnfirst, tsnlast, num_mk, SCTP_FR_T3_TIMEOUT);
	}
#ifdef SCTP_DEBUG
	if (num_mk) {
		SCTPDBG(SCTP_DEBUG_TIMER1, "LAST TSN marked was %x\n",
			tsnlast);
		SCTPDBG(SCTP_DEBUG_TIMER1, "Num marked for retransmission was %d peer-rwd:%u\n",
			num_mk,
			stcb->asoc.peers_rwnd);
	}
#endif
	*num_marked = num_mk;
	*num_abandoned = cnt_abandoned;

	TAILQ_FOREACH(chk, &stcb->asoc.control_send_queue, sctp_next) {
		if (chk->sent == SCTP_DATAGRAM_RESEND) {
			cnt_mk++;
		}
		if ((chk->whoTo == net) &&
		    (chk->rec.chunk_id.id == SCTP_ECN_ECHO)) {
			sctp_free_remote_addr(chk->whoTo);
			chk->whoTo = alt;
			if (chk->sent != SCTP_DATAGRAM_RESEND) {
				chk->sent = SCTP_DATAGRAM_RESEND;
				chk->flags |= CHUNK_FLAGS_FRAGMENT_OK;
				sctp_ucount_incr(stcb->asoc.sent_queue_retran_cnt);
				cnt_mk++;
			}
			atomic_add_int(&alt->ref_count, 1);
		}
	}
#ifdef THIS_SHOULD_NOT_BE_DONE
	if ((stcb->asoc.sent_queue_retran_cnt == 0) && (could_be_sent)) {

		sctp_ucount_incr(stcb->asoc.sent_queue_retran_cnt);
		cnt_mk++;
		could_be_sent->sent = SCTP_DATAGRAM_RESEND;
	}
#endif
	if (stcb->asoc.sent_queue_retran_cnt != cnt_mk) {
#ifdef INVARIANTS
		SCTP_PRINTF("Local Audit says there are %d for retran asoc cnt:%d we marked:%d this time\n",
			    cnt_mk, stcb->asoc.sent_queue_retran_cnt, num_mk);
#endif
#ifndef SCTP_AUDITING_ENABLED
		stcb->asoc.sent_queue_retran_cnt = cnt_mk;
#endif
	}
	if (audit_tf) {
		SCTPDBG(SCTP_DEBUG_TIMER4,
			"Audit total flight due to negative value net:%p\n",
			(void *)net);
		stcb->asoc.total_flight = 0;
		stcb->asoc.total_flight_count = 0;

		TAILQ_FOREACH(lnets, &stcb->asoc.nets, sctp_next) {
			lnets->flight_size = 0;
			SCTPDBG(SCTP_DEBUG_TIMER4,
				"Net:%p c-f cwnd:%d ssthresh:%d\n",
				(void *)lnets, lnets->cwnd, lnets->ssthresh);
		}
		TAILQ_FOREACH(chk, &stcb->asoc.sent_queue, sctp_next) {
			if (chk->sent < SCTP_DATAGRAM_RESEND) {
				if (SCTP_BASE_SYSCTL(sctp_logging_level) & SCTP_FLIGHT_LOGGING_ENABLE) {
					sctp_misc_ints(SCTP_FLIGHT_LOG_UP,
						       chk->whoTo->flight_size,
						       chk->book_size,
						       (uint32_t)(uintptr_t)chk->whoTo,
						       chk->rec.data.tsn);
				}

				sctp_flight_size_increase(chk);
				sctp_total_flight_increase(stcb, chk);
			}
		}
	}

	return (0);
}

int
sctp_t3rxt_timer(struct sctp_inpcb *inp,
    struct sctp_tcb *stcb,
    struct sctp_nets *net)
{
	struct sctp_nets *alt;
	int win_probe, num_mk, num_abandoned;

	if (SCTP_BASE_SYSCTL(sctp_logging_level) & SCTP_FR_LOGGING_ENABLE) {
		sctp_log_fr(0, 0, 0, SCTP_FR_T3_TIMEOUT);
	}
	if (SCTP_BASE_SYSCTL(sctp_logging_level) & SCTP_CWND_LOGGING_ENABLE) {
		struct sctp_nets *lnet;

		TAILQ_FOREACH(lnet, &stcb->asoc.nets, sctp_next) {
			if (net == lnet) {
				sctp_log_cwnd(stcb, lnet, 1, SCTP_CWND_LOG_FROM_T3);
			} else {
				sctp_log_cwnd(stcb, lnet, 0, SCTP_CWND_LOG_FROM_T3);
			}
		}
	}

	if ((stcb->asoc.peers_rwnd == 0) &&
	    (stcb->asoc.total_flight < net->mtu)) {
		SCTP_STAT_INCR(sctps_timowindowprobe);
		win_probe = 1;
	} else {
		win_probe = 0;
	}

	if (win_probe == 0) {

		if (sctp_threshold_management(inp, stcb, net,
		    stcb->asoc.max_send_times)) {

			return (1);
		} else {
			if (net != stcb->asoc.primary_destination) {

				struct timeval now;
				uint32_t ms_goneby;

				(void)SCTP_GETTIME_TIMEVAL(&now);
				if (net->last_sent_time.tv_sec) {
					ms_goneby = (uint32_t)(now.tv_sec - net->last_sent_time.tv_sec) * 1000;
				} else {
					ms_goneby = 0;
				}
				if ((net->dest_state & SCTP_ADDR_PF) == 0) {
					if ((ms_goneby > net->RTO) || (net->RTO == 0)) {

						sctp_send_hb(stcb, net, SCTP_SO_NOT_LOCKED);
					}
				}
			}
		}
	} else {

		if (sctp_threshold_management(inp, stcb, NULL,
		    stcb->asoc.max_send_times)) {

			return (1);
		}
	}
	if (stcb->asoc.sctp_cmt_on_off > 0) {
		if (net->pf_threshold < net->failure_threshold) {
			alt = sctp_find_alternate_net(stcb, net, 2);
		} else {

			alt = sctp_find_alternate_net(stcb, net, 1);

			net->find_pseudo_cumack = 1;
			net->find_rtx_pseudo_cumack = 1;
		}
	} else {
		alt = sctp_find_alternate_net(stcb, net, 0);
	}

	num_mk = 0;
	num_abandoned = 0;
	(void)sctp_mark_all_for_resend(stcb, net, alt, win_probe,
				      &num_mk, &num_abandoned);

	stcb->asoc.fast_retran_loss_recovery = 0;

	net->fast_retran_loss_recovery = 0;
	if ((stcb->asoc.cc_functions.sctp_cwnd_new_transmission_begins) &&
	    (net->flight_size == 0)) {
		(*stcb->asoc.cc_functions.sctp_cwnd_new_transmission_begins)(stcb, net);
	}

	stcb->asoc.sat_t3_loss_recovery = 1;
	stcb->asoc.sat_t3_recovery_tsn = stcb->asoc.sending_seq;

	sctp_backoff_on_timeout(stcb, net, win_probe, num_mk, num_abandoned);
	if ((!(net->dest_state & SCTP_ADDR_REACHABLE)) ||
	    (net->dest_state & SCTP_ADDR_PF)) {

		sctp_move_chunks_from_net(stcb, net);

		if (net->ro._s_addr) {
			sctp_free_ifa(net->ro._s_addr);
			net->ro._s_addr = NULL;
		}
		net->src_addr_selected = 0;

#if defined(__FreeBSD__) && !defined(__Userspace__)
		RO_NHFREE(&net->ro);
#else
		if (net->ro.ro_rt) {
			RTFREE(net->ro.ro_rt);
			net->ro.ro_rt = NULL;
		}
#endif

		if ((stcb->asoc.primary_destination == net) && (alt != net)) {

			if (stcb->asoc.alternate) {
				sctp_free_remote_addr(stcb->asoc.alternate);
			}
			stcb->asoc.alternate = alt;
			atomic_add_int(&stcb->asoc.alternate->ref_count, 1);
		}
	}

	if (SCTP_GET_STATE(stcb) == SCTP_STATE_COOKIE_ECHOED) {

		sctp_timer_start(SCTP_TIMER_TYPE_SEND, inp, stcb, net);
		return (0);
	}
	if (stcb->asoc.prsctp_supported) {
		struct sctp_tmit_chunk *lchk;

		lchk = sctp_try_advance_peer_ack_point(stcb, &stcb->asoc);

		if (SCTP_TSN_GT(stcb->asoc.advanced_peer_ack_point, stcb->asoc.last_acked_seq)) {
			send_forward_tsn(stcb, &stcb->asoc);
			for (; lchk != NULL; lchk = TAILQ_NEXT(lchk, sctp_next)) {
				if (lchk->whoTo != NULL) {
					break;
				}
			}
			if (lchk != NULL) {

				sctp_timer_start(SCTP_TIMER_TYPE_SEND, stcb->sctp_ep, stcb, lchk->whoTo);
			}
		}
	}
	if (SCTP_BASE_SYSCTL(sctp_logging_level) & SCTP_CWND_MONITOR_ENABLE) {
		sctp_log_cwnd(stcb, net, net->cwnd, SCTP_CWND_LOG_FROM_RTX);
	}
	return (0);
}

int
sctp_t1init_timer(struct sctp_inpcb *inp,
    struct sctp_tcb *stcb,
    struct sctp_nets *net)
{

	if (stcb->asoc.delayed_connection) {

		stcb->asoc.delayed_connection = 0;
		sctp_send_initiate(inp, stcb, SCTP_SO_NOT_LOCKED);
		return (0);
	}
	if (SCTP_GET_STATE(stcb) != SCTP_STATE_COOKIE_WAIT) {
		return (0);
	}
	if (sctp_threshold_management(inp, stcb, net,
	    stcb->asoc.max_init_times)) {

		return (1);
	}
	stcb->asoc.dropped_special_cnt = 0;
	sctp_backoff_on_timeout(stcb, stcb->asoc.primary_destination, 1, 0, 0);
	if (stcb->asoc.initial_init_rto_max < net->RTO) {
		net->RTO = stcb->asoc.initial_init_rto_max;
	}
	if (stcb->asoc.numnets > 1) {

		struct sctp_nets *alt;

		alt = sctp_find_alternate_net(stcb, stcb->asoc.primary_destination, 0);
		if (alt != stcb->asoc.primary_destination) {
			sctp_move_chunks_from_net(stcb, stcb->asoc.primary_destination);
			stcb->asoc.primary_destination = alt;
		}
	}

	sctp_send_initiate(inp, stcb, SCTP_SO_NOT_LOCKED);
	return (0);
}

int
sctp_cookie_timer(struct sctp_inpcb *inp,
    struct sctp_tcb *stcb,
    struct sctp_nets *net SCTP_UNUSED)
{
	struct sctp_nets *alt;
	struct sctp_tmit_chunk *cookie;

	TAILQ_FOREACH(cookie, &stcb->asoc.control_send_queue, sctp_next) {
		if (cookie->rec.chunk_id.id == SCTP_COOKIE_ECHO) {
			break;
		}
	}
	if (cookie == NULL) {
		if (SCTP_GET_STATE(stcb) == SCTP_STATE_COOKIE_ECHOED) {

			struct mbuf *op_err;

			op_err = sctp_generate_cause(SCTP_BASE_SYSCTL(sctp_diag_info_code),
			                             "Cookie timer expired, but no cookie");
			inp->last_abort_code = SCTP_FROM_SCTP_TIMER + SCTP_LOC_3;
			sctp_abort_an_association(inp, stcb, op_err, false, SCTP_SO_NOT_LOCKED);
		} else {
#ifdef INVARIANTS
			panic("Cookie timer expires in wrong state?");
#else
			SCTP_PRINTF("Strange in state %d not cookie-echoed yet c-e timer expires?\n", SCTP_GET_STATE(stcb));
			return (0);
#endif
		}
		return (0);
	}

	if (sctp_threshold_management(inp, stcb, cookie->whoTo,
	    stcb->asoc.max_init_times)) {

		return (1);
	}

	stcb->asoc.dropped_special_cnt = 0;
	sctp_backoff_on_timeout(stcb, cookie->whoTo, 1, 0, 0);
	alt = sctp_find_alternate_net(stcb, cookie->whoTo, 0);
	if (alt != cookie->whoTo) {
		sctp_free_remote_addr(cookie->whoTo);
		cookie->whoTo = alt;
		atomic_add_int(&alt->ref_count, 1);
	}

	if (cookie->sent != SCTP_DATAGRAM_RESEND) {
		sctp_ucount_incr(stcb->asoc.sent_queue_retran_cnt);
	}
	cookie->sent = SCTP_DATAGRAM_RESEND;
	cookie->flags |= CHUNK_FLAGS_FRAGMENT_OK;

	return (0);
}

int
sctp_strreset_timer(struct sctp_inpcb *inp, struct sctp_tcb *stcb)
{
	struct sctp_nets *alt, *net;
	struct sctp_tmit_chunk *strrst = NULL, *chk = NULL;

	if (stcb->asoc.stream_reset_outstanding == 0) {
		return (0);
	}

	(void)sctp_find_stream_reset(stcb, stcb->asoc.str_reset_seq_out, &strrst);
	if (strrst == NULL) {
		return (0);
	}
	net = strrst->whoTo;

	if (sctp_threshold_management(inp, stcb, net, stcb->asoc.max_send_times)) {

		return (1);
	}

	sctp_backoff_on_timeout(stcb, net, 1, 0, 0);
	alt = sctp_find_alternate_net(stcb, net, 0);
	strrst->whoTo = alt;
	atomic_add_int(&alt->ref_count, 1);

	TAILQ_FOREACH(chk, &stcb->asoc.control_send_queue, sctp_next) {
		if ((chk->whoTo == net) &&
		    (chk->rec.chunk_id.id == SCTP_ECN_ECHO)) {
			sctp_free_remote_addr(chk->whoTo);
			if (chk->sent != SCTP_DATAGRAM_RESEND) {
				chk->sent = SCTP_DATAGRAM_RESEND;
				chk->flags |= CHUNK_FLAGS_FRAGMENT_OK;
				sctp_ucount_incr(stcb->asoc.sent_queue_retran_cnt);
			}
			chk->whoTo = alt;
			atomic_add_int(&alt->ref_count, 1);
		}
	}
	if (!(net->dest_state & SCTP_ADDR_REACHABLE)) {

		sctp_move_chunks_from_net(stcb, net);
	}
	sctp_free_remote_addr(net);

	if (strrst->sent != SCTP_DATAGRAM_RESEND)
		sctp_ucount_incr(stcb->asoc.sent_queue_retran_cnt);
	strrst->sent = SCTP_DATAGRAM_RESEND;
	strrst->flags |= CHUNK_FLAGS_FRAGMENT_OK;

	sctp_timer_start(SCTP_TIMER_TYPE_STRRESET, inp, stcb, alt);
	return (0);
}

int
sctp_asconf_timer(struct sctp_inpcb *inp, struct sctp_tcb *stcb,
		  struct sctp_nets *net)
{
	struct sctp_nets *alt;
	struct sctp_tmit_chunk *asconf, *chk;

	if (TAILQ_EMPTY(&stcb->asoc.asconf_send_queue)) {

		sctp_send_asconf(stcb, net, SCTP_ADDR_NOT_LOCKED);
	} else {

		asconf = TAILQ_FIRST(&stcb->asoc.asconf_send_queue);
		if (asconf == NULL) {
			return (0);
		}
		net = asconf->whoTo;

		if (sctp_threshold_management(inp, stcb, net,
		    stcb->asoc.max_send_times)) {

			return (1);
		}
		if (asconf->snd_count > stcb->asoc.max_send_times) {

			SCTPDBG(SCTP_DEBUG_TIMER1, "asconf_timer: Peer has not responded to our repeated ASCONFs\n");
			sctp_asconf_cleanup(stcb);
			return (0);
		}

		sctp_backoff_on_timeout(stcb, net, 1, 0, 0);
		alt = sctp_find_alternate_net(stcb, net, 0);
		if (asconf->whoTo != alt) {
			asconf->whoTo = alt;
			atomic_add_int(&alt->ref_count, 1);
		}

		TAILQ_FOREACH(chk, &stcb->asoc.control_send_queue, sctp_next) {
			if ((chk->whoTo == net) &&
			    (chk->rec.chunk_id.id == SCTP_ECN_ECHO)) {
				sctp_free_remote_addr(chk->whoTo);
				chk->whoTo = alt;
				if (chk->sent != SCTP_DATAGRAM_RESEND) {
					chk->sent = SCTP_DATAGRAM_RESEND;
					chk->flags |= CHUNK_FLAGS_FRAGMENT_OK;
					sctp_ucount_incr(stcb->asoc.sent_queue_retran_cnt);
				}
				atomic_add_int(&alt->ref_count, 1);
			}
		}
		TAILQ_FOREACH(chk, &stcb->asoc.asconf_send_queue, sctp_next) {
			if (chk->whoTo != alt) {
				sctp_free_remote_addr(chk->whoTo);
				chk->whoTo = alt;
				atomic_add_int(&alt->ref_count, 1);
			}
			if (asconf->sent != SCTP_DATAGRAM_RESEND && chk->sent != SCTP_DATAGRAM_UNSENT)
				sctp_ucount_incr(stcb->asoc.sent_queue_retran_cnt);
			chk->sent = SCTP_DATAGRAM_RESEND;
			chk->flags |= CHUNK_FLAGS_FRAGMENT_OK;
		}
		if (!(net->dest_state & SCTP_ADDR_REACHABLE)) {

			sctp_move_chunks_from_net(stcb, net);
		}
		sctp_free_remote_addr(net);

		if (asconf->sent != SCTP_DATAGRAM_RESEND)
			sctp_ucount_incr(stcb->asoc.sent_queue_retran_cnt);
		asconf->sent = SCTP_DATAGRAM_RESEND;
		asconf->flags |= CHUNK_FLAGS_FRAGMENT_OK;

		sctp_send_asconf(stcb, alt, SCTP_ADDR_NOT_LOCKED);
	}
	return (0);
}

void
sctp_delete_prim_timer(struct sctp_inpcb *inp, struct sctp_tcb *stcb)
{
	if (stcb->asoc.deleted_primary == NULL) {
		SCTPDBG(SCTP_DEBUG_ASCONF1, "delete_prim_timer: deleted_primary is not stored...\n");
		sctp_mobility_feature_off(inp, SCTP_MOBILITY_PRIM_DELETED);
		return;
	}
	SCTPDBG(SCTP_DEBUG_ASCONF1, "delete_prim_timer: finished to keep deleted primary ");
	SCTPDBG_ADDR(SCTP_DEBUG_ASCONF1, &stcb->asoc.deleted_primary->ro._l_addr.sa);
	sctp_free_remote_addr(stcb->asoc.deleted_primary);
	stcb->asoc.deleted_primary = NULL;
	sctp_mobility_feature_off(inp, SCTP_MOBILITY_PRIM_DELETED);
	return;
}

int
sctp_shutdown_timer(struct sctp_inpcb *inp, struct sctp_tcb *stcb,
    struct sctp_nets *net)
{
	struct sctp_nets *alt;

	if (sctp_threshold_management(inp, stcb, net, stcb->asoc.max_send_times)) {

		return (1);
	}
	sctp_backoff_on_timeout(stcb, net, 1, 0, 0);

	alt = sctp_find_alternate_net(stcb, net, 0);

	sctp_send_shutdown(stcb, alt);

	sctp_timer_start(SCTP_TIMER_TYPE_SHUTDOWN, inp, stcb, alt);
	return (0);
}

int
sctp_shutdownack_timer(struct sctp_inpcb *inp, struct sctp_tcb *stcb,
    struct sctp_nets *net)
{
	struct sctp_nets *alt;

	if (sctp_threshold_management(inp, stcb, net, stcb->asoc.max_send_times)) {

		return (1);
	}
	sctp_backoff_on_timeout(stcb, net, 1, 0, 0);

	alt = sctp_find_alternate_net(stcb, net, 0);

	sctp_send_shutdown_ack(stcb, alt);

	sctp_timer_start(SCTP_TIMER_TYPE_SHUTDOWNACK, inp, stcb, alt);
	return (0);
}

static void
sctp_audit_stream_queues_for_size(struct sctp_inpcb *inp,
    struct sctp_tcb *stcb)
{
	struct sctp_stream_queue_pending *sp;
	unsigned int i, chks_in_queue = 0;
	int being_filled = 0;

	if ((stcb == NULL) || (inp == NULL))
		return;

	if (stcb->asoc.sent_queue_retran_cnt) {
		SCTP_PRINTF("Hmm, sent_queue_retran_cnt is non-zero %d\n",
			    stcb->asoc.sent_queue_retran_cnt);
		stcb->asoc.sent_queue_retran_cnt = 0;
	}
	if (stcb->asoc.ss_functions.sctp_ss_is_empty(stcb, &stcb->asoc)) {

		stcb->asoc.ss_functions.sctp_ss_init(stcb, &stcb->asoc, 0);
		if (!stcb->asoc.ss_functions.sctp_ss_is_empty(stcb, &stcb->asoc)) {

			SCTP_PRINTF("Found additional streams NOT managed by scheduler, corrected\n");
		} else {

			stcb->asoc.total_output_queue_size = 0;
		}
	}

	for (i = 0; i < stcb->asoc.streamoutcnt; i++) {
		if (!TAILQ_EMPTY(&stcb->asoc.strmout[i].outqueue)) {
			TAILQ_FOREACH(sp, &stcb->asoc.strmout[i].outqueue, next) {
				if (sp->msg_is_complete)
					being_filled++;
				chks_in_queue++;
			}
		}
	}
	if (chks_in_queue != stcb->asoc.stream_queue_cnt) {
		SCTP_PRINTF("Hmm, stream queue cnt at %d I counted %d in stream out wheel\n",
			    stcb->asoc.stream_queue_cnt, chks_in_queue);
	}
	if (chks_in_queue) {

		sctp_chunk_output(inp, stcb, SCTP_OUTPUT_FROM_T3, SCTP_SO_NOT_LOCKED);
		if ((TAILQ_EMPTY(&stcb->asoc.send_queue)) &&
		    (TAILQ_EMPTY(&stcb->asoc.sent_queue))) {

			if (being_filled == 0) {
				SCTP_PRINTF("Still nothing moved %d chunks are stuck\n",
					    chks_in_queue);
			}
		}
	} else {
		SCTP_PRINTF("Found no chunks on any queue tot:%lu\n",
			    (u_long)stcb->asoc.total_output_queue_size);
		stcb->asoc.total_output_queue_size = 0;
	}
}

int
sctp_heartbeat_timer(struct sctp_inpcb *inp, struct sctp_tcb *stcb,
    struct sctp_nets *net)
{
	uint8_t net_was_pf;

	if (net->dest_state & SCTP_ADDR_PF) {
		net_was_pf = 1;
	} else {
		net_was_pf = 0;
	}
	if (net->hb_responded == 0) {
		if (net->ro._s_addr) {

			sctp_free_ifa(net->ro._s_addr);
			net->ro._s_addr = NULL;
			net->src_addr_selected = 0;
		}
		sctp_backoff_on_timeout(stcb, net, 1, 0, 0);
		if (sctp_threshold_management(inp, stcb, net, stcb->asoc.max_send_times)) {

			return (1);
		}
	}

	if (net->partial_bytes_acked) {
		net->partial_bytes_acked = 0;
	}
	if ((stcb->asoc.total_output_queue_size > 0) &&
	    (TAILQ_EMPTY(&stcb->asoc.send_queue)) &&
	    (TAILQ_EMPTY(&stcb->asoc.sent_queue))) {
		sctp_audit_stream_queues_for_size(inp, stcb);
	}
	if (!(net->dest_state & SCTP_ADDR_NOHB) &&
	    !((net_was_pf == 0) && (net->dest_state & SCTP_ADDR_PF))) {

		uint32_t ms_gone_by;

		if ((net->last_sent_time.tv_sec > 0) ||
		    (net->last_sent_time.tv_usec > 0)) {
#if defined(__FreeBSD__) && !defined(__Userspace__)
			struct timeval diff;

			SCTP_GETTIME_TIMEVAL(&diff);
			timevalsub(&diff, &net->last_sent_time);
#else
			struct timeval diff, now;

			SCTP_GETTIME_TIMEVAL(&now);
			timersub(&now, &net->last_sent_time, &diff);
#endif
			ms_gone_by = (uint32_t)(diff.tv_sec * 1000) +
			             (uint32_t)(diff.tv_usec / 1000);
		} else {
			ms_gone_by = 0xffffffff;
		}
		if ((ms_gone_by >= net->heart_beat_delay) ||
		    (net->dest_state & SCTP_ADDR_PF)) {
			sctp_send_hb(stcb, net, SCTP_SO_NOT_LOCKED);
		}
	}
	return (0);
}

void
sctp_pathmtu_timer(struct sctp_inpcb *inp,
    struct sctp_tcb *stcb,
    struct sctp_nets *net)
{
	uint32_t next_mtu, mtu;

	next_mtu = sctp_get_next_mtu(net->mtu);

	if ((next_mtu > net->mtu) && (net->port == 0)) {
		if ((net->src_addr_selected == 0) ||
		    (net->ro._s_addr == NULL) ||
		    (net->ro._s_addr->localifa_flags & SCTP_BEING_DELETED)) {
			if ((net->ro._s_addr != NULL) && (net->ro._s_addr->localifa_flags & SCTP_BEING_DELETED)) {
				sctp_free_ifa(net->ro._s_addr);
				net->ro._s_addr = NULL;
				net->src_addr_selected = 0;
			} else  if (net->ro._s_addr == NULL) {
#if defined(INET6) && defined(SCTP_EMBEDDED_V6_SCOPE)
				if (net->ro._l_addr.sa.sa_family == AF_INET6) {
					struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *)&net->ro._l_addr;

#if defined(__APPLE__) && !defined(__Userspace__)
#if defined(APPLE_LEOPARD) || defined(APPLE_SNOWLEOPARD)
					(void)in6_embedscope(&sin6->sin6_addr, sin6, NULL, NULL);
#else
					(void)in6_embedscope(&sin6->sin6_addr, sin6, NULL, NULL, NULL);
#endif
#elif defined(SCTP_KAME)
					(void)sa6_embedscope(sin6, MODULE_GLOBAL(ip6_use_defzone));
#else
					(void)in6_embedscope(&sin6->sin6_addr, sin6);
#endif
				}
#endif

				net->ro._s_addr = sctp_source_address_selection(inp,
										stcb,
										(sctp_route_t *)&net->ro,
										net, 0, stcb->asoc.vrf_id);
#if defined(INET6) && defined(SCTP_EMBEDDED_V6_SCOPE)
				if (net->ro._l_addr.sa.sa_family == AF_INET6) {
					struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *)&net->ro._l_addr;
#ifdef SCTP_KAME
					(void)sa6_recoverscope(sin6);
#else
					(void)in6_recoverscope(sin6, &sin6->sin6_addr, NULL);
#endif
				}
#endif
			}
			if (net->ro._s_addr)
				net->src_addr_selected = 1;
		}
		if (net->ro._s_addr) {
#if defined(__FreeBSD__) && !defined(__Userspace__)
			mtu = SCTP_GATHER_MTU_FROM_ROUTE(net->ro._s_addr, &net->ro._s_addr.sa, net->ro.ro_nh);
#else
			mtu = SCTP_GATHER_MTU_FROM_ROUTE(net->ro._s_addr, &net->ro._s_addr.sa, net->ro.ro_rt);
#endif
#if defined(INET) || defined(INET6)
			if (net->port) {
				mtu -= sizeof(struct udphdr);
			}
#endif
			if (mtu > next_mtu) {
				net->mtu = next_mtu;
			} else {
				net->mtu = mtu;
			}
		}
	}

	sctp_timer_start(SCTP_TIMER_TYPE_PATHMTURAISE, inp, stcb, net);
}

void
sctp_autoclose_timer(struct sctp_inpcb *inp, struct sctp_tcb *stcb)
{
	struct timeval tn, *tim_touse;
	struct sctp_association *asoc;
	uint32_t ticks_gone_by;

	(void)SCTP_GETTIME_TIMEVAL(&tn);
	if (stcb->asoc.sctp_autoclose_ticks > 0 &&
	    sctp_is_feature_on(inp, SCTP_PCB_FLAGS_AUTOCLOSE)) {

		asoc = &stcb->asoc;

		if (asoc->time_last_rcvd.tv_sec >
		    asoc->time_last_sent.tv_sec) {
			tim_touse = &asoc->time_last_rcvd;
		} else {
			tim_touse = &asoc->time_last_sent;
		}

		ticks_gone_by = sctp_secs_to_ticks((uint32_t)(tn.tv_sec - tim_touse->tv_sec));
		if (ticks_gone_by >= asoc->sctp_autoclose_ticks) {

			sctp_chunk_output(inp, stcb, SCTP_OUTPUT_FROM_AUTOCLOSE_TMR, SCTP_SO_NOT_LOCKED);

			if (TAILQ_EMPTY(&asoc->send_queue) &&
			    TAILQ_EMPTY(&asoc->sent_queue)) {

				if (SCTP_GET_STATE(stcb) != SCTP_STATE_SHUTDOWN_SENT) {

					struct sctp_nets *net;

					if ((SCTP_GET_STATE(stcb) == SCTP_STATE_OPEN) ||
					    (SCTP_GET_STATE(stcb) == SCTP_STATE_SHUTDOWN_RECEIVED)) {
						SCTP_STAT_DECR_GAUGE32(sctps_currestab);
					}
					SCTP_SET_STATE(stcb, SCTP_STATE_SHUTDOWN_SENT);
					sctp_stop_timers_for_shutdown(stcb);
					if (stcb->asoc.alternate) {
						net = stcb->asoc.alternate;
					} else {
						net = stcb->asoc.primary_destination;
					}
					sctp_send_shutdown(stcb, net);
					sctp_timer_start(SCTP_TIMER_TYPE_SHUTDOWN,
							 stcb->sctp_ep, stcb, net);
					sctp_timer_start(SCTP_TIMER_TYPE_SHUTDOWNGUARD,
							 stcb->sctp_ep, stcb, NULL);
				}
			}
		} else {

			uint32_t tmp;

			tmp = asoc->sctp_autoclose_ticks;
			asoc->sctp_autoclose_ticks -= ticks_gone_by;
			sctp_timer_start(SCTP_TIMER_TYPE_AUTOCLOSE, inp, stcb, NULL);

			asoc->sctp_autoclose_ticks = tmp;
		}
	}
}
