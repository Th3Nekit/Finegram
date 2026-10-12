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

#ifndef _NETINET_SCTP_STRUCTS_H_
#define _NETINET_SCTP_STRUCTS_H_

#include <netinet/sctp_os.h>
#include <netinet/sctp_header.h>
#include <netinet/sctp_auth.h>

struct sctp_timer {
	sctp_os_timer_t timer;

	int type;

	void *ep;
	void *tcb;
	void *net;
#if defined(__FreeBSD__) && !defined(__Userspace__)
	void *vnet;
#endif

	void *self;
	uint32_t ticks;
	uint32_t stopped_from;
};

struct sctp_foo_stuff {
	struct sctp_inpcb *inp;
	uint32_t        lineno;
	uint32_t        ticks;
	int             updown;
};

TAILQ_HEAD(sctpnetlisthead, sctp_nets);

struct sctp_stream_reset_list {
	TAILQ_ENTRY(sctp_stream_reset_list) next_resp;
	uint32_t seq;
	uint32_t tsn;
	uint32_t number_entries;
	uint16_t list_of_streams[];
};

TAILQ_HEAD(sctp_resethead, sctp_stream_reset_list);

#define SCTP_PCB_ANY_FLAGS	0x00000000
#define SCTP_PCB_ANY_FEATURES	0x00000000
#define SCTP_ASOC_ANY_STATE	0x00000000

typedef void (*asoc_func) (struct sctp_inpcb *, struct sctp_tcb *, void *ptr,
         uint32_t val);
typedef int (*inp_func) (struct sctp_inpcb *, void *ptr, uint32_t val);
typedef void (*end_func) (void *ptr, uint32_t val);

#if defined(__FreeBSD__) && !defined(__Userspace__)
#if defined(SCTP_MCORE_INPUT) && defined(SMP)

struct sctp_mcore_queue {
	TAILQ_ENTRY(sctp_mcore_queue) next;
	struct vnet *vn;
	struct mbuf *m;
	int off;
	int v6;
};

TAILQ_HEAD(sctp_mcore_qhead, sctp_mcore_queue);

struct sctp_mcore_ctrl {
	SCTP_PROCESS_STRUCT thread_proc;
	struct sctp_mcore_qhead que;
	struct mtx core_mtx;
	struct mtx que_mtx;
	int running;
	int cpuid;
};
#endif
#endif

struct sctp_iterator {
	TAILQ_ENTRY(sctp_iterator) sctp_nxt_itr;
#if defined(__FreeBSD__) && !defined(__Userspace__)
	struct vnet *vn;
#endif
	struct sctp_timer tmr;
	struct sctp_inpcb *inp;
	struct sctp_tcb *stcb;
	struct sctp_inpcb *next_inp;
	asoc_func function_assoc;
	inp_func function_inp;
	inp_func function_inp_end;
	end_func function_atend;
	void *pointer;
	uint32_t val;
	uint32_t pcb_flags;
	uint32_t pcb_features;
	uint32_t asoc_state;
	uint32_t iterator_flags;
	uint8_t  no_chunk_output;
	uint8_t  done_current_ep;
};

#define SCTP_ITERATOR_DO_ALL_INP	0x00000001
#define SCTP_ITERATOR_DO_SINGLE_INP	0x00000002

TAILQ_HEAD(sctpiterators, sctp_iterator);

struct sctp_copy_all {
	struct sctp_inpcb *inp;
	struct mbuf *m;
	struct sctp_sndrcvinfo sndrcv;
	ssize_t sndlen;
	int cnt_sent;
	int cnt_failed;
};

struct sctp_asconf_iterator {
	struct sctpladdr list_of_work;
	int cnt;
};

struct iterator_control {
#if defined(__FreeBSD__) && !defined(__Userspace__)
	struct mtx ipi_iterator_wq_mtx;
	struct mtx it_mtx;
#elif defined(__APPLE__) && !defined(__Userspace__)
	lck_mtx_t *ipi_iterator_wq_mtx;
	lck_mtx_t *it_mtx;
#elif defined(SCTP_PROCESS_LEVEL_LOCKS)
#if defined(__Userspace__)
	userland_mutex_t ipi_iterator_wq_mtx;
	userland_mutex_t it_mtx;
	userland_cond_t iterator_wakeup;
#else
	pthread_mutex_t ipi_iterator_wq_mtx;
	pthread_mutex_t it_mtx;
	pthread_cond_t iterator_wakeup;
#endif
#elif defined(_WIN32) && !defined(__Userspace__)
	struct spinlock it_lock;
	struct spinlock ipi_iterator_wq_lock;
	KEVENT iterator_wakeup[2];
	PFILE_OBJECT iterator_thread_obj;
#else
	void *it_mtx;
#endif
#if !(defined(_WIN32) && !defined(__Userspace__))
#if !defined(__Userspace__)
	SCTP_PROCESS_STRUCT thread_proc;
#else
	userland_thread_t thread_proc;
#endif
#endif
	struct sctpiterators iteratorhead;
	struct sctp_iterator *cur_it;
	uint32_t iterator_running;
	uint32_t iterator_flags;
};
#if !(defined(__FreeBSD__) && !defined(__Userspace__))
#define SCTP_ITERATOR_MUST_EXIT		0x00000001
#define SCTP_ITERATOR_EXITED		0x00000002
#endif
#define SCTP_ITERATOR_STOP_CUR_IT	0x00000004
#define SCTP_ITERATOR_STOP_CUR_INP	0x00000008

struct sctp_net_route {
#if defined(__FreeBSD__) && !defined(__Userspace__)
	struct nhop_object *ro_nh;
	struct llentry *ro_lle;
	char		*ro_prepend;
	uint16_t	ro_plen;
	uint16_t	ro_flags;
	uint16_t	ro_mtu;
	uint16_t	spare;
#else
	sctp_rtentry_t *ro_rt;
#endif
#if defined(__APPLE__) && !defined(__Userspace__)
#if !defined(APPLE_LEOPARD) && !defined(APPLE_SNOWLEOPARD) && !defined(APPLE_LION) && !defined(APPLE_MOUNTAINLION) && !defined(APPLE_ELCAPITAN)
	struct llentry	*ro_lle;
#endif
#if !defined(APPLE_LEOPARD) && !defined(APPLE_SNOWLEOPARD) && !defined(APPLE_LION) && !defined(APPLE_MOUNTAINLION)
	struct ifaddr *ro_srcia;
#endif
#if !defined(APPLE_LEOPARD)
	uint32_t ro_flags;
#endif
#endif
	union sctp_sockstore _l_addr;
	struct sctp_ifa *_s_addr;
};

struct htcp {
	uint16_t	alpha;
	uint8_t		beta;
	uint8_t		modeswitch;
	uint32_t	last_cong;
	uint32_t	undo_last_cong;
	uint16_t	bytes_acked;
	uint32_t	bytecount;
	uint32_t	minRTT;
	uint32_t	maxRTT;

	uint32_t	undo_maxRTT;
	uint32_t	undo_old_maxB;

	uint32_t	minB;
	uint32_t	maxB;
	uint32_t	old_maxB;
	uint32_t	Bi;
	uint32_t	lasttime;
};

struct rtcc_cc {
	struct timeval tls;
	uint64_t lbw;
	uint64_t lbw_rtt;
	uint64_t bw_bytes;
	uint64_t bw_tot_time;
	uint64_t new_tot_time;
	uint64_t bw_bytes_at_last_rttc;
	uint32_t cwnd_at_bw_set;
	uint32_t vol_reduce;
	uint16_t steady_step;
	uint16_t step_cnt;
	uint8_t  ret_from_eq;
	uint8_t  use_dccc_ecn;
	uint8_t  tls_needs_set;
	uint8_t  last_step_state;
	uint8_t  rtt_set_this_sack;
	uint8_t  last_inst_ind;
};

struct sctp_nets {
	TAILQ_ENTRY(sctp_nets) sctp_next;

	struct sctp_timer pmtu_timer;
	struct sctp_timer hb_timer;

	struct sctp_net_route ro;

	uint32_t mtu;
	uint32_t ssthresh;
	uint32_t last_cwr_tsn;
	uint32_t cwr_window_tsn;
	uint32_t ecn_ce_pkt_cnt;
	uint32_t lost_cnt;

	int lastsa;
	int lastsv;
	uint64_t rtt;
	uint32_t RTO;

	struct sctp_timer rxt_timer;

	struct timeval last_sent_time;
	union cc_control_data {
		struct htcp htcp_ca;
		struct rtcc_cc rtcc;
	} cc_mod;
	int ref_count;

	uint32_t flight_size;
	uint32_t cwnd;
	uint32_t prev_cwnd;
	uint32_t ecn_prev_cwnd;
	uint32_t partial_bytes_acked;

	unsigned int net_ack;
	unsigned int net_ack2;

	uint32_t last_active;

	uint32_t this_sack_highest_newack;

	uint32_t pseudo_cumack;

	uint32_t rtx_pseudo_cumack;

	uint32_t fast_recovery_tsn;
	uint32_t heartbeat_random1;
	uint32_t heartbeat_random2;
#ifdef INET6
	uint32_t flowlabel;
#endif
	uint8_t dscp;

	struct timeval start_time;
	uint32_t marked_retrans;

	uint32_t marked_fastretrans;
	uint32_t heart_beat_delay;

	uint16_t dest_state;

	uint16_t failure_threshold;

	uint16_t pf_threshold;

	uint16_t error_count;

	uint16_t port;

	uint8_t fast_retran_loss_recovery;
	uint8_t will_exit_fast_recovery;

	uint8_t fast_retran_ip;
	uint8_t hb_responded;
	uint8_t saw_newack;
	uint8_t src_addr_selected;
	uint8_t indx_of_eligible_next_to_use;
	uint8_t addr_is_local;

	uint8_t find_pseudo_cumack;

	uint8_t find_rtx_pseudo_cumack;

	uint8_t new_pseudo_cumack;

	uint8_t window_probe;
	uint8_t RTO_measured;
	uint8_t last_hs_used;
	uint8_t lan_type;
	uint8_t rto_needed;
#if defined(__FreeBSD__) && !defined(__Userspace__)
	uint32_t flowid;
	uint8_t flowtype;
#endif
};

struct sctp_data_chunkrec {
	uint32_t tsn;
	uint32_t mid;
	uint16_t sid;
	uint32_t ppid;
	uint32_t context;
	uint32_t cwnd_at_send;

	uint32_t fast_retran_tsn;
	struct timeval timetodrop;
	uint32_t fsn;
	uint8_t doing_fast_retransmit;
	uint8_t rcv_flags;

	uint8_t state_flags;
	uint8_t chunk_was_revoked;
	uint8_t fwd_tsn_cnt;
};

TAILQ_HEAD(sctpchunk_listhead, sctp_tmit_chunk);

#define CHUNK_FLAGS_PR_SCTP_TTL	        SCTP_PR_SCTP_TTL
#define CHUNK_FLAGS_PR_SCTP_BUF	        SCTP_PR_SCTP_BUF
#define CHUNK_FLAGS_PR_SCTP_RTX         SCTP_PR_SCTP_RTX

#define CHUNK_FLAGS_FRAGMENT_OK	        0x0100

struct chk_id {
	uint8_t id;
	uint8_t can_take_data;
};

struct sctp_tmit_chunk {
	union {
		struct sctp_data_chunkrec data;
		struct chk_id chunk_id;
	}     rec;
	struct sctp_association *asoc;
	struct timeval sent_rcv_time;
	struct mbuf *data;
	struct mbuf *last_mbuf;
	struct sctp_nets *whoTo;
	TAILQ_ENTRY(sctp_tmit_chunk) sctp_next;
	int32_t sent;
	uint16_t snd_count;
	uint16_t flags;
	uint16_t send_size;
	uint16_t book_size;
	uint16_t mbcnt;
	uint16_t auth_keyid;
	uint8_t holds_key_ref;
	uint8_t pad_inplace;
	uint8_t do_rtt;
	uint8_t book_size_scale;
	uint8_t no_fr_allowed;
	uint8_t copy_by_ref;
	uint8_t window_probe;
};

struct sctp_queued_to_read {
	uint16_t sinfo_stream;
	uint16_t sinfo_flags;

	uint32_t sinfo_ppid;
	uint32_t sinfo_context;
	uint32_t sinfo_timetolive;
	uint32_t sinfo_tsn;
	uint32_t sinfo_cumtsn;
	sctp_assoc_t sinfo_assoc_id;

	uint32_t mid;
	uint32_t length;
	uint32_t held_length;
	uint32_t top_fsn;
	uint32_t fsn_included;
	struct sctp_nets *whoFrom;
	struct mbuf *data;

	struct mbuf *tail_mbuf;
	struct mbuf *aux_data;
	struct sctp_tcb *stcb;
	TAILQ_ENTRY(sctp_queued_to_read) next;
	TAILQ_ENTRY(sctp_queued_to_read) next_instrm;
	struct sctpchunk_listhead reasm;
	uint16_t port_from;
	uint16_t spec_flags;
	uint8_t  do_not_ref_stcb;
	uint8_t  end_added;
	uint8_t  pdapi_aborted;
	uint8_t  pdapi_started;
	uint8_t  some_taken;
	uint8_t  last_frag_seen;
	uint8_t  first_frag_seen;
	uint8_t  on_read_q;
	uint8_t  on_strm_q;
};

#define SCTP_ON_ORDERED 1
#define SCTP_ON_UNORDERED 2

struct sctp_stream_queue_pending {
	struct mbuf *data;
	struct mbuf *tail_mbuf;
	struct timeval ts;
	struct sctp_nets *net;
	TAILQ_ENTRY (sctp_stream_queue_pending) next;
	TAILQ_ENTRY (sctp_stream_queue_pending) ss_next;
	uint32_t fsn;
	uint32_t length;
	uint32_t timetolive;
	uint32_t ppid;
	uint32_t context;
	uint16_t sinfo_flags;
	uint16_t sid;
	uint16_t act_flags;
	uint16_t auth_keyid;
	uint8_t  holds_key_ref;
	uint8_t  msg_is_complete;
	uint8_t  some_taken;
	uint8_t  sender_all_done;
	uint8_t  put_last_out;
	uint8_t  discard_rest;
	uint8_t  processing;
};

TAILQ_HEAD(sctpwheelunrel_listhead, sctp_stream_in);
struct sctp_stream_in {
	struct sctp_readhead inqueue;
	struct sctp_readhead uno_inqueue;
	uint32_t last_mid_delivered;
	uint16_t sid;
	uint8_t  delivery_started;
	uint8_t  pd_api_started;
};

TAILQ_HEAD(sctpwheel_listhead, sctp_stream_out);
TAILQ_HEAD(sctplist_listhead, sctp_stream_queue_pending);

struct scheduling_data {
	struct sctp_stream_out *locked_on_sending;

	struct sctp_stream_out *last_out_stream;
	union {
		struct sctpwheel_listhead wheel;
		struct sctplist_listhead list;
	} out;
};

struct ss_rr {

	TAILQ_ENTRY(sctp_stream_out) next_spoke;
};

struct ss_prio {

	TAILQ_ENTRY(sctp_stream_out) next_spoke;

	uint16_t priority;
};

struct ss_fb {

	TAILQ_ENTRY(sctp_stream_out) next_spoke;

	int32_t rounds;
};

union scheduling_parameters {
	struct ss_rr rr;
	struct ss_prio prio;
	struct ss_fb fb;
};

#define SCTP_STREAM_CLOSED           0x00
#define SCTP_STREAM_OPENING          0x01
#define SCTP_STREAM_OPEN             0x02
#define SCTP_STREAM_RESET_PENDING    0x03
#define SCTP_STREAM_RESET_IN_FLIGHT  0x04

struct sctp_stream_out {
	struct sctp_streamhead outqueue;
	union scheduling_parameters ss_params;
	uint32_t chunks_on_queues;
#if defined(SCTP_DETAILED_STR_STATS)
	uint32_t abandoned_unsent[SCTP_PR_SCTP_MAX + 1];
	uint32_t abandoned_sent[SCTP_PR_SCTP_MAX + 1];
#else

	uint32_t abandoned_unsent[1];
	uint32_t abandoned_sent[1];
#endif

	uint32_t next_mid_ordered;
	uint32_t next_mid_unordered;
	uint16_t sid;
	uint8_t last_msg_incomplete;
	uint8_t state;
};

#define SCTP_MAX_STREAMS_AT_ONCE_RESET 200

TAILQ_HEAD(sctp_asconf_addrhead, sctp_asconf_addr);
struct sctp_asconf_addr {
	TAILQ_ENTRY(sctp_asconf_addr) next;
	struct sctp_asconf_addr_param ap;
	struct sctp_ifa *ifa;
	uint8_t sent;
	uint8_t special_del;
};

struct sctp_scoping {
	uint8_t ipv4_addr_legal;
	uint8_t ipv6_addr_legal;
#if defined(__Userspace__)
	uint8_t conn_addr_legal;
#endif
	uint8_t loopback_scope;
	uint8_t ipv4_local_scope;
	uint8_t local_scope;
	uint8_t site_scope;
};

#define SCTP_TSN_LOG_SIZE 40

struct sctp_tsn_log {
	void     *stcb;
	uint32_t tsn;
	uint32_t seq;
	uint16_t strm;
	uint16_t sz;
	uint16_t flgs;
	uint16_t in_pos;
	uint16_t in_out;
	uint16_t resv;
};

#define SCTP_FS_SPEC_LOG_SIZE 200
struct sctp_fs_spec_log {
	uint32_t sent;
	uint32_t total_flight;
	uint32_t tsn;
	uint16_t book;
	uint8_t incr;
	uint8_t decr;
};

struct sctp_nonpad_sndrcvinfo {
	uint16_t sinfo_stream;
	uint16_t sinfo_ssn;
	uint16_t sinfo_flags;
	uint32_t sinfo_ppid;
	uint32_t sinfo_context;
	uint32_t sinfo_timetolive;
	uint32_t sinfo_tsn;
	uint32_t sinfo_cumtsn;
	sctp_assoc_t sinfo_assoc_id;
	uint16_t sinfo_keynumber;
	uint16_t sinfo_keynumber_valid;
};

struct sctp_cc_functions {
	void (*sctp_set_initial_cc_param)(struct sctp_tcb *stcb, struct sctp_nets *net);
	void (*sctp_cwnd_update_after_sack)(struct sctp_tcb *stcb,
	                                    struct sctp_association *asoc,
	                                    int accum_moved ,int reneged_all, int will_exit);
	void (*sctp_cwnd_update_exit_pf)(struct sctp_tcb *stcb, struct sctp_nets *net);
	void (*sctp_cwnd_update_after_fr)(struct sctp_tcb *stcb,
			struct sctp_association *asoc);
	void (*sctp_cwnd_update_after_timeout)(struct sctp_tcb *stcb,
			struct sctp_nets *net);
	void (*sctp_cwnd_update_after_ecn_echo)(struct sctp_tcb *stcb,
			struct sctp_nets *net, int in_window, int num_pkt_lost);
	void (*sctp_cwnd_update_after_packet_dropped)(struct sctp_tcb *stcb,
			struct sctp_nets *net, struct sctp_pktdrop_chunk *cp,
			uint32_t *bottle_bw, uint32_t *on_queue);
	void (*sctp_cwnd_update_after_output)(struct sctp_tcb *stcb,
			struct sctp_nets *net, int burst_limit);
	void (*sctp_cwnd_update_packet_transmitted)(struct sctp_tcb *stcb,
			struct sctp_nets *net);
	void (*sctp_cwnd_update_tsn_acknowledged)(struct sctp_nets *net,
			struct sctp_tmit_chunk *);
	void (*sctp_cwnd_new_transmission_begins)(struct sctp_tcb *stcb,
			struct sctp_nets *net);
	void (*sctp_cwnd_prepare_net_for_sack)(struct sctp_tcb *stcb,
			struct sctp_nets *net);
	int (*sctp_cwnd_socket_option)(struct sctp_tcb *stcb, int set, struct sctp_cc_option *);
	void (*sctp_rtt_calculated)(struct sctp_tcb *, struct sctp_nets *, struct timeval *);
};

struct sctp_ss_functions {
	void (*sctp_ss_init)(struct sctp_tcb *stcb, struct sctp_association *asoc,
		int holds_lock);
	void (*sctp_ss_clear)(struct sctp_tcb *stcb, struct sctp_association *asoc,
		int clear_values, int holds_lock);
	void (*sctp_ss_init_stream)(struct sctp_tcb *stcb, struct sctp_stream_out *strq, struct sctp_stream_out *with_strq);
	void (*sctp_ss_add_to_stream)(struct sctp_tcb *stcb, struct sctp_association *asoc,
		struct sctp_stream_out *strq, struct sctp_stream_queue_pending *sp, int holds_lock);
	int (*sctp_ss_is_empty)(struct sctp_tcb *stcb, struct sctp_association *asoc);
	void (*sctp_ss_remove_from_stream)(struct sctp_tcb *stcb, struct sctp_association *asoc,
		struct sctp_stream_out *strq, struct sctp_stream_queue_pending *sp, int holds_lock);
	struct sctp_stream_out* (*sctp_ss_select_stream)(struct sctp_tcb *stcb,
		struct sctp_nets *net, struct sctp_association *asoc);
	void (*sctp_ss_scheduled)(struct sctp_tcb *stcb, struct sctp_nets *net,
		struct sctp_association *asoc, struct sctp_stream_out *strq, int moved_how_much);
	void (*sctp_ss_packet_done)(struct sctp_tcb *stcb, struct sctp_nets *net,
		struct sctp_association *asoc);
	int (*sctp_ss_get_value)(struct sctp_tcb *stcb, struct sctp_association *asoc,
		struct sctp_stream_out *strq, uint16_t *value);
	int (*sctp_ss_set_value)(struct sctp_tcb *stcb, struct sctp_association *asoc,
		struct sctp_stream_out *strq, uint16_t value);
	int (*sctp_ss_is_user_msgs_incomplete)(struct sctp_tcb *stcb, struct sctp_association *asoc);
};

TAILQ_HEAD(sctp_asconf_head, sctp_asconf);
struct sctp_asconf {
	TAILQ_ENTRY(sctp_asconf) next;
	uint32_t serial_number;
	uint16_t snd_count;
	struct mbuf *data;
	uint16_t len;
};

TAILQ_HEAD(sctp_asconf_ackhead, sctp_asconf_ack);
struct sctp_asconf_ack {
	TAILQ_ENTRY(sctp_asconf_ack) next;
	uint32_t serial_number;
	struct sctp_nets *last_sent_to;
	struct mbuf *data;
	uint16_t len;
};

struct sctp_association {

	int state;

	struct sctp_asconf_addrhead asconf_queue;

	struct timeval time_entered;
	struct timeval time_last_rcvd;
	struct timeval time_last_sent;
	struct timeval time_last_sat_advance;
	struct sctp_nonpad_sndrcvinfo def_send;

	struct sctp_timer dack_timer;
	struct sctp_timer asconf_timer;
	struct sctp_timer strreset_timer;
	struct sctp_timer shut_guard_timer;
	struct sctp_timer autoclose_timer;
	struct sctp_timer delete_prim_timer;

	struct sctpladdr sctp_restricted_addrs;

	struct sctp_ifa *asconf_addr_del_pending;

	struct sctp_nets *deleted_primary;

	struct sctpnetlisthead nets;

	struct sctpchunk_listhead free_chunks;

	struct sctpchunk_listhead control_send_queue;

	struct sctpchunk_listhead asconf_send_queue;

	struct sctpchunk_listhead sent_queue;
	struct sctpchunk_listhead send_queue;

	struct scheduling_data ss_data;

	struct sctp_iterator *stcb_starting_point_for_iterator;

	struct sctp_asconf_ackhead asconf_ack_sent;

	struct sctp_tmit_chunk *str_reset;

	struct sctp_laddr *last_used_address;

	struct sctp_stream_in *strmin;
	struct sctp_stream_out *strmout;
	uint8_t *mapping_array;

	struct sctp_nets *primary_destination;
	struct sctp_nets *alternate;

	struct sctp_nets *last_net_cmt_send_started;

	struct sctp_nets *last_data_chunk_from;

	struct sctp_nets *last_control_chunk_from;

	struct sctp_resethead resetHead;

	struct sctp_readhead pending_reply_queue;

	struct sctp_cc_functions cc_functions;

	uint32_t congestion_control_module;

	struct sctp_ss_functions ss_functions;

	uint32_t stream_scheduling_module;

	uint32_t vrf_id;
	uint32_t cookie_preserve_req;

	uint32_t asconf_seq_out;
	uint32_t asconf_seq_out_acked;

	uint32_t asconf_seq_in;

	uint32_t str_reset_seq_out;

	uint32_t str_reset_seq_in;

	uint32_t my_vtag;

	uint32_t peer_vtag;

	uint32_t my_vtag_nonce;
	uint32_t peer_vtag_nonce;

	uint32_t assoc_id;

	uint32_t smallest_mtu;

	uint32_t this_sack_highest_gap;

	uint32_t last_acked_seq;

	uint32_t sending_seq;

	uint32_t init_seq_number;

	uint32_t advanced_peer_ack_point;

	uint32_t cumulative_tsn;

	uint32_t mapping_array_base_tsn;

	uint32_t highest_tsn_inside_map;

	uint8_t *nr_mapping_array;
	uint32_t highest_tsn_inside_nr_map;

	uint32_t fast_recovery_tsn;
	uint32_t sat_t3_recovery_tsn;
	uint32_t tsn_last_delivered;

	struct sctp_queued_to_read *control_pdapi;

	uint32_t tsn_of_pdapi_last_delivered;
	uint32_t pdapi_ppid;
	uint32_t context;
	uint32_t last_reset_action[SCTP_MAX_RESET_PARAMS];
	uint32_t last_sending_seq[SCTP_MAX_RESET_PARAMS];
	uint32_t last_base_tsnsent[SCTP_MAX_RESET_PARAMS];
#ifdef SCTP_ASOCLOG_OF_TSNS

	struct sctp_tsn_log  in_tsnlog[SCTP_TSN_LOG_SIZE];
	struct sctp_tsn_log  out_tsnlog[SCTP_TSN_LOG_SIZE];
	uint32_t cumack_log[SCTP_TSN_LOG_SIZE];
	uint32_t cumack_logsnt[SCTP_TSN_LOG_SIZE];
	uint16_t tsn_in_at;
	uint16_t tsn_out_at;
	uint16_t tsn_in_wrapped;
	uint16_t tsn_out_wrapped;
	uint16_t cumack_log_at;
	uint16_t cumack_log_atsnt;
#endif
#ifdef SCTP_FS_SPEC_LOG
	struct sctp_fs_spec_log fslog[SCTP_FS_SPEC_LOG_SIZE];
	uint16_t fs_index;
#endif

	uint32_t peers_rwnd;
	uint32_t my_rwnd;
	uint32_t my_last_reported_rwnd;
	uint32_t sctp_frag_point;

	uint32_t total_output_queue_size;

	uint32_t sb_cc;
	uint32_t sb_send_resv;
	uint32_t my_rwnd_control_len;
#ifdef INET6
	uint32_t default_flowlabel;
#endif
	uint32_t pr_sctp_cnt;
	int ctrl_queue_cnt;

	unsigned int stream_queue_cnt;
	unsigned int send_queue_cnt;
	unsigned int sent_queue_cnt;
	unsigned int sent_queue_cnt_removeable;

	unsigned int sent_queue_retran_cnt;

	unsigned int size_on_reasm_queue;
	unsigned int cnt_on_reasm_queue;
	unsigned int fwd_tsn_cnt;

	unsigned int total_flight;

	unsigned int total_flight_count;

	unsigned int numnets;

	unsigned int overall_error_count;

	unsigned int cnt_msg_on_sb;

	unsigned int size_on_all_streams;
	unsigned int cnt_on_all_streams;

	uint32_t heart_beat_delay;

	uint32_t sctp_autoclose_ticks;

	unsigned int pre_open_streams;

	unsigned int max_inbound_streams;

	uint32_t cookie_life;

	unsigned int delayed_ack;
	unsigned int old_delayed_ack;
	unsigned int sack_freq;
	unsigned int data_pkts_seen;

	unsigned int numduptsns;
	int dup_tsns[SCTP_MAX_DUP_TSNS];
	uint32_t initial_init_rto_max;
	uint32_t initial_rto;
	uint32_t minrto;
	uint32_t maxrto;

	sctp_auth_chklist_t *local_auth_chunks;
	sctp_auth_chklist_t *peer_auth_chunks;
	sctp_hmaclist_t *local_hmacs;
	sctp_hmaclist_t *peer_hmacs;
	struct sctp_keyhead shared_keys;
	sctp_authinfo_t authinfo;

	uint32_t refcnt;
	uint32_t chunks_on_out_queue;

	uint32_t peers_adaptation;
	uint32_t default_mtu;
	uint16_t peer_hmac_id;

	uint16_t stale_cookie_count;

	uint16_t str_of_pdapi;
	uint16_t ssn_of_pdapi;

	uint16_t streamincnt;
	uint16_t streamoutcnt;
	uint16_t strm_realoutsize;
	uint16_t strm_pending_add_size;

	uint16_t max_init_times;
	uint16_t max_send_times;

	uint16_t def_net_failure;

	uint16_t def_net_pf_threshold;

	uint16_t mapping_array_size;

	uint16_t last_strm_seq_delivered;
	uint16_t last_strm_no_delivered;

	uint16_t last_revoke_count;
	int16_t num_send_timers_up;

	uint16_t stream_locked_on;
	uint16_t ecn_echo_cnt_onq;

	uint16_t free_chunk_cnt;
	uint8_t stream_locked;
	uint8_t authenticated;

	uint8_t send_sack;

	uint32_t max_burst;

	uint32_t fr_max_burst;

	uint8_t sat_network;
	uint8_t sat_network_lockout;
	uint8_t burst_limit_applied;

	uint8_t hb_random_values[4];
	uint8_t fragmented_delivery_inprogress;
	uint8_t fragment_flags;
	uint8_t last_flags_delivered;
	uint8_t hb_ect_randombit;
	uint8_t hb_random_idx;
	uint8_t default_dscp;
	uint8_t asconf_del_pending;
	uint8_t trigger_reset;

	uint8_t ecn_supported;
	uint8_t prsctp_supported;
	uint8_t auth_supported;
	uint8_t asconf_supported;
	uint8_t reconfig_supported;
	uint8_t nrsack_supported;
	uint8_t pktdrop_supported;
	uint8_t idata_supported;

	uint8_t peer_req_out;

	uint8_t local_strreset_support;
	uint8_t peer_supports_nat;

	struct sctp_scoping scope;

	uint8_t used_alt_asconfack;
	uint8_t fast_retran_loss_recovery;
	uint8_t sat_t3_loss_recovery;
	uint8_t dropped_special_cnt;
	uint8_t seen_a_sack_this_pkt;
	uint8_t stream_reset_outstanding;
	uint8_t stream_reset_out_is_outstanding;
	uint8_t delayed_connection;
	uint8_t ifp_had_enobuf;
	uint8_t saw_sack_with_frags;
	uint8_t saw_sack_with_nr_frags;
	uint8_t in_asocid_hash;
	uint8_t assoc_up_sent;
	uint8_t adaptation_needed;
	uint8_t adaptation_sent;

	uint8_t cmt_dac_pkts_rcvd;
	uint8_t sctp_cmt_on_off;
	uint8_t iam_blocking;
	uint8_t cookie_how[8];

	uint8_t sctp_cmt_pf;
	uint8_t use_precise_time;
	uint64_t sctp_features;
	uint32_t max_cwnd;
	uint16_t port;

	uint32_t marked_retrans;
	uint32_t timoinit;
	uint32_t timodata;
	uint32_t timosack;
	uint32_t timoshutdown;
	uint32_t timoheartbeat;
	uint32_t timocookie;
	uint32_t timoshutdownack;
	struct timeval start_time;
	struct timeval discontinuity_time;
	uint64_t abandoned_unsent[SCTP_PR_SCTP_MAX + 1];
	uint64_t abandoned_sent[SCTP_PR_SCTP_MAX + 1];
};

#endif
