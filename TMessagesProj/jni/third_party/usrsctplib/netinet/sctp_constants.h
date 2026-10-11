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

#ifndef _NETINET_SCTP_CONSTANTS_H_
#define _NETINET_SCTP_CONSTANTS_H_

#if defined(_WIN32) && defined(__Userspace__)
extern void getwintimeofday(struct timeval *tv);

#endif

#define SCTP_OVER_UDP_TUNNELING_PORT 9899

#define SCTP_DEFAULT_SACK_FREQ 2

#define SCTP_ADDRESS_LIMIT 1080

#define SCTP_LARGEST_INIT_ACCEPTED (65535 - 2048)

#define SCTP_MAX_CHUNK_LENGTH 0xffff

#define SCTP_MAX_CAUSE_LENGTH 0xffff

#define SCTP_COUNT_LIMIT 40

#define SCTP_ZERO_COPY_TICK_DELAY (((100 * hz) + 999) / 1000)
#define SCTP_ZERO_COPY_SENDQ_TICK_DELAY (((100 * hz) + 999) / 1000)

#define SCTP_ADDRESS_TICK_DELAY 2

#define SCTP_VERSION_STRING "KAME-BSD 1.1"

#define SCTP_AUDIT_SIZE 256

#define SCTP_KTRHEAD_NAME "sctp_iterator"
#define SCTP_KTHREAD_PAGES 0

#define SCTP_MCORE_NAME "sctp_core_worker"

#define SCTP_DEFAULT_VRF_SIZE 4

#define ALPHA_BASE	(1<<7)
#define BETA_MIN	(1<<6)
#define BETA_MAX	102

#define SCTP_CWND_LOG_FROM_FR	1
#define SCTP_CWND_LOG_FROM_RTX	2
#define SCTP_CWND_LOG_FROM_BRST	3
#define SCTP_CWND_LOG_FROM_SS	4
#define SCTP_CWND_LOG_FROM_CA	5
#define SCTP_CWND_LOG_FROM_SAT	6
#define SCTP_BLOCK_LOG_INTO_BLK 7
#define SCTP_BLOCK_LOG_OUTOF_BLK 8
#define SCTP_BLOCK_LOG_CHECK     9
#define SCTP_STR_LOG_FROM_INTO_STRD 10
#define SCTP_STR_LOG_FROM_IMMED_DEL 11
#define SCTP_STR_LOG_FROM_INSERT_HD 12
#define SCTP_STR_LOG_FROM_INSERT_MD 13
#define SCTP_STR_LOG_FROM_INSERT_TL 14
#define SCTP_STR_LOG_FROM_MARK_TSN  15
#define SCTP_STR_LOG_FROM_EXPRS_DEL 16
#define SCTP_FR_LOG_BIGGEST_TSNS    17
#define SCTP_FR_LOG_STRIKE_TEST     18
#define SCTP_FR_LOG_STRIKE_CHUNK    19
#define SCTP_FR_T3_TIMEOUT          20
#define SCTP_MAP_PREPARE_SLIDE      21
#define SCTP_MAP_SLIDE_FROM         22
#define SCTP_MAP_SLIDE_RESULT       23
#define SCTP_MAP_SLIDE_CLEARED	    24
#define SCTP_MAP_SLIDE_NONE         25
#define SCTP_FR_T3_MARK_TIME        26
#define SCTP_FR_T3_MARKED           27
#define SCTP_FR_T3_STOPPED          28
#define SCTP_FR_MARKED              30
#define SCTP_CWND_LOG_NOADV_SS      31
#define SCTP_CWND_LOG_NOADV_CA      32
#define SCTP_MAX_BURST_APPLIED      33
#define SCTP_MAX_IFP_APPLIED        34
#define SCTP_MAX_BURST_ERROR_STOP   35
#define SCTP_INCREASE_PEER_RWND     36
#define SCTP_DECREASE_PEER_RWND     37
#define SCTP_SET_PEER_RWND_VIA_SACK 38
#define SCTP_LOG_MBCNT_INCREASE     39
#define SCTP_LOG_MBCNT_DECREASE     40
#define SCTP_LOG_MBCNT_CHKSET       41
#define SCTP_LOG_NEW_SACK           42
#define SCTP_LOG_TSN_ACKED          43
#define SCTP_LOG_TSN_REVOKED        44
#define SCTP_LOG_LOCK_TCB           45
#define SCTP_LOG_LOCK_INP           46
#define SCTP_LOG_LOCK_SOCK          47
#define SCTP_LOG_LOCK_SOCKBUF_R     48
#define SCTP_LOG_LOCK_SOCKBUF_S     49
#define SCTP_LOG_LOCK_CREATE        50
#define SCTP_LOG_INITIAL_RTT        51
#define SCTP_LOG_RTTVAR             52
#define SCTP_LOG_SBALLOC            53
#define SCTP_LOG_SBFREE             54
#define SCTP_LOG_SBRESULT           55
#define SCTP_FR_DUPED               56
#define SCTP_FR_MARKED_EARLY        57
#define SCTP_FR_CWND_REPORT         58
#define SCTP_FR_CWND_REPORT_START   59
#define SCTP_FR_CWND_REPORT_STOP    60
#define SCTP_CWND_LOG_FROM_SEND     61
#define SCTP_CWND_INITIALIZATION    62
#define SCTP_CWND_LOG_FROM_T3       63
#define SCTP_CWND_LOG_FROM_SACK     64
#define SCTP_CWND_LOG_NO_CUMACK     65
#define SCTP_CWND_LOG_FROM_RESEND   66
#define SCTP_FR_LOG_CHECK_STRIKE    67
#define SCTP_SEND_NOW_COMPLETES     68
#define SCTP_CWND_LOG_FILL_OUTQ_CALLED 69
#define SCTP_CWND_LOG_FILL_OUTQ_FILLS  70
#define SCTP_LOG_FREE_SENT             71
#define SCTP_NAGLE_APPLIED          72
#define SCTP_NAGLE_SKIPPED          73
#define SCTP_WAKESND_FROM_SACK      74
#define SCTP_WAKESND_FROM_FWDTSN    75
#define SCTP_NOWAKE_FROM_SACK       76
#define SCTP_CWNDLOG_PRESEND        77
#define SCTP_CWNDLOG_ENDSEND        78
#define SCTP_AT_END_OF_SACK         79
#define SCTP_REASON_FOR_SC          80
#define SCTP_BLOCK_LOG_INTO_BLKA    81
#define SCTP_ENTER_USER_RECV        82
#define SCTP_USER_RECV_SACKS        83
#define SCTP_SORECV_BLOCKSA         84
#define SCTP_SORECV_BLOCKSB         85
#define SCTP_SORECV_DONE            86
#define SCTP_SACK_RWND_UPDATE       87
#define SCTP_SORECV_ENTER           88
#define SCTP_SORECV_ENTERPL         89
#define SCTP_MBUF_INPUT             90
#define SCTP_MBUF_IALLOC            91
#define SCTP_MBUF_IFREE             92
#define SCTP_MBUF_ICOPY             93
#define SCTP_MBUF_SPLIT             94
#define SCTP_SORCV_FREECTL          95
#define SCTP_SORCV_DOESCPY          96
#define SCTP_SORCV_DOESLCK          97
#define SCTP_SORCV_DOESADJ          98
#define SCTP_SORCV_BOTWHILE         99
#define SCTP_SORCV_PASSBF          100
#define SCTP_SORCV_ADJD            101
#define SCTP_UNKNOWN_MAX           102
#define SCTP_RANDY_STUFF           103
#define SCTP_RANDY_STUFF1          104
#define SCTP_STRMOUT_LOG_ASSIGN	   105
#define SCTP_STRMOUT_LOG_SEND	   106
#define SCTP_FLIGHT_LOG_DOWN_CA    107
#define SCTP_FLIGHT_LOG_UP         108
#define SCTP_FLIGHT_LOG_DOWN_GAP   109
#define SCTP_FLIGHT_LOG_DOWN_RSND  110
#define SCTP_FLIGHT_LOG_UP_RSND    111
#define SCTP_FLIGHT_LOG_DOWN_RSND_TO    112
#define SCTP_FLIGHT_LOG_DOWN_WP    113
#define SCTP_FLIGHT_LOG_UP_REVOKE  114
#define SCTP_FLIGHT_LOG_DOWN_PDRP  115
#define SCTP_FLIGHT_LOG_DOWN_PMTU  116
#define SCTP_SACK_LOG_NORMAL	   117
#define SCTP_SACK_LOG_EXPRESS	   118
#define SCTP_MAP_TSN_ENTERS        119
#define SCTP_THRESHOLD_CLEAR       120
#define SCTP_THRESHOLD_INCR        121
#define SCTP_FLIGHT_LOG_DWN_WP_FWD 122
#define SCTP_FWD_TSN_CHECK         123
#define SCTP_LOG_MAX_TYPES 124

#define SCTP_LOG_EVENT_UNKNOWN 0
#define SCTP_LOG_EVENT_CWND  1
#define SCTP_LOG_EVENT_BLOCK 2
#define SCTP_LOG_EVENT_STRM  3
#define SCTP_LOG_EVENT_FR    4
#define SCTP_LOG_EVENT_MAP   5
#define SCTP_LOG_EVENT_MAXBURST 6
#define SCTP_LOG_EVENT_RWND  7
#define SCTP_LOG_EVENT_MBCNT 8
#define SCTP_LOG_EVENT_SACK  9
#define SCTP_LOG_LOCK_EVENT 10
#define SCTP_LOG_EVENT_RTT  11
#define SCTP_LOG_EVENT_SB   12
#define SCTP_LOG_EVENT_NAGLE 13
#define SCTP_LOG_EVENT_WAKE 14
#define SCTP_LOG_MISC_EVENT 15
#define SCTP_LOG_EVENT_CLOSE 16
#define SCTP_LOG_EVENT_MBUF 17
#define SCTP_LOG_CHUNK_PROC 18
#define SCTP_LOG_ERROR_RET  19

#define SCTP_LOG_MAX_EVENT 20

#define SCTP_LOCK_UNKNOWN 2

#define SCTP_MAX_NUM_OF_ASOC	40000

#define SCTP_SCALE_FOR_ADDR	2

#define SCTP_DEFAULT_MULTIPLE_ASCONFS	0

#define SCTP_RWND_HIWAT_SHIFT 3

#define SCTP_PARTIAL_DELIVERY_SHIFT 1

#define SCTP_HMAC		SCTP_AUTH_HMAC_ID_SHA1
#define SCTP_SIGNATURE_SIZE	SCTP_AUTH_DIGEST_LEN_SHA1
#define SCTP_SIGNATURE_ALOC_SIZE SCTP_SIGNATURE_SIZE

#define PROTO_SIGNATURE_A	0x30000000
#define SCTP_VERSION_NUMBER	0x3

#define MAX_TSN	0xffffffff

#define SCTP_ITERATOR_MAX_AT_ONCE 20

#define SCTP_ITERATOR_TICKS 1

#define SCTP_IGNORE_CWND_ON_FR 1

#define SCTP_NO_FR_UNLESS_SEGMENT_SMALLER 1

#define SCTP_DEF_MAX_BURST 4
#define SCTP_DEF_HBMAX_BURST 4
#define SCTP_DEF_FRMAX_BURST 4

#define SCTP_RTT_FROM_NON_DATA 0
#define SCTP_RTT_FROM_DATA     1

#define PR_SCTP_UNORDERED_FLAG 0x0001

#define SCTP_FIRST_MBUF_RESV 68

#define SCTP_DATAGRAM_UNSENT		0
#define SCTP_DATAGRAM_SENT		1
#define SCTP_DATAGRAM_RESEND1		2

#define SCTP_DATAGRAM_RESEND2		3

#define SCTP_DATAGRAM_RESEND		4
#define SCTP_DATAGRAM_ACKED		10010
#define SCTP_DATAGRAM_MARKED		20010
#define SCTP_FORWARD_TSN_SKIP		30010
#define SCTP_DATAGRAM_NR_ACKED		40010

#define SCTP_OUTPUT_FROM_USR_SEND       0
#define SCTP_OUTPUT_FROM_T3       	1
#define SCTP_OUTPUT_FROM_INPUT_ERROR    2
#define SCTP_OUTPUT_FROM_CONTROL_PROC   3
#define SCTP_OUTPUT_FROM_SACK_TMR       4
#define SCTP_OUTPUT_FROM_SHUT_TMR       5
#define SCTP_OUTPUT_FROM_HB_TMR         6
#define SCTP_OUTPUT_FROM_SHUT_ACK_TMR   7
#define SCTP_OUTPUT_FROM_ASCONF_TMR     8
#define SCTP_OUTPUT_FROM_STRRST_TMR     9
#define SCTP_OUTPUT_FROM_AUTOCLOSE_TMR  10
#define SCTP_OUTPUT_FROM_EARLY_FR_TMR   11
#define SCTP_OUTPUT_FROM_STRRST_REQ     12
#define SCTP_OUTPUT_FROM_USR_RCVD       13
#define SCTP_OUTPUT_FROM_COOKIE_ACK     14
#define SCTP_OUTPUT_FROM_DRAIN          15
#define SCTP_OUTPUT_FROM_CLOSING        16
#define SCTP_OUTPUT_FROM_SOCKOPT        17

#define SCTP_SIZE32(x)	((((x) + 3) >> 2) << 2)

#define IS_SCTP_CONTROL(a) (((a)->chunk_type != SCTP_DATA) && ((a)->chunk_type != SCTP_IDATA))
#define IS_SCTP_DATA(a) (((a)->chunk_type == SCTP_DATA) || ((a)->chunk_type == SCTP_IDATA))

#define SCTP_HEARTBEAT_INFO		0x0001
#if defined(__Userspace__)
#define SCTP_CONN_ADDRESS		0x0004
#endif
#define SCTP_IPV4_ADDRESS		0x0005
#define SCTP_IPV6_ADDRESS		0x0006
#define SCTP_STATE_COOKIE		0x0007
#define SCTP_UNRECOG_PARAM		0x0008
#define SCTP_COOKIE_PRESERVE		0x0009
#define SCTP_HOSTNAME_ADDRESS		0x000b
#define SCTP_SUPPORTED_ADDRTYPE		0x000c

#define SCTP_STR_RESET_OUT_REQUEST	0x000d
#define SCTP_STR_RESET_IN_REQUEST	0x000e
#define SCTP_STR_RESET_TSN_REQUEST	0x000f
#define SCTP_STR_RESET_RESPONSE		0x0010
#define SCTP_STR_RESET_ADD_OUT_STREAMS	0x0011
#define SCTP_STR_RESET_ADD_IN_STREAMS	0x0012

#define SCTP_MAX_RESET_PARAMS 2
#define SCTP_STREAM_RESET_TSN_DELTA	0x1000

#define SCTP_ECN_CAPABLE		0x8000

#define SCTP_RANDOM			0x8002
#define SCTP_CHUNK_LIST			0x8003
#define SCTP_HMAC_LIST			0x8004

#define SCTP_PAD			0x8005

#define SCTP_SUPPORTED_CHUNK_EXT	0x8008

#define SCTP_PRSCTP_SUPPORTED		0xc000

#define SCTP_ADD_IP_ADDRESS		0xc001
#define SCTP_DEL_IP_ADDRESS		0xc002
#define SCTP_ERROR_CAUSE_IND		0xc003
#define SCTP_SET_PRIM_ADDR		0xc004
#define SCTP_SUCCESS_REPORT		0xc005
#define SCTP_ULP_ADAPTATION		0xc006

#define SCTP_HAS_NAT_SUPPORT		0xc007
#define SCTP_NAT_VTAGS			0xc008

#define SCTP_ECT0_BIT		0x02
#define SCTP_ECT1_BIT		0x01
#define SCTP_CE_BITS		0x03

#define SCTP_FLEXIBLE_ADDRESS	0x20
#define SCTP_NO_HEARTBEAT	0x40

#define SCTP_STICKY_OPTIONS_MASK	0x0c

#define SCTP_STATE_EMPTY		0x0000
#define SCTP_STATE_INUSE		0x0001
#define SCTP_STATE_COOKIE_WAIT		0x0002
#define SCTP_STATE_COOKIE_ECHOED	0x0004
#define SCTP_STATE_OPEN			0x0008
#define SCTP_STATE_SHUTDOWN_SENT	0x0010
#define SCTP_STATE_SHUTDOWN_RECEIVED	0x0020
#define SCTP_STATE_SHUTDOWN_ACK_SENT	0x0040
#define SCTP_STATE_SHUTDOWN_PENDING	0x0080
#define SCTP_STATE_CLOSED_SOCKET	0x0100
#define SCTP_STATE_ABOUT_TO_BE_FREED    0x0200
#define SCTP_STATE_PARTIAL_MSG_LEFT     0x0400
#define SCTP_STATE_WAS_ABORTED          0x0800
#define SCTP_STATE_IN_ACCEPT_QUEUE      0x1000
#define SCTP_STATE_MASK			0x007f

#define SCTP_GET_STATE(_stcb) \
	((_stcb)->asoc.state & SCTP_STATE_MASK)
#define SCTP_SET_STATE(_stcb, _state) \
	sctp_set_state(_stcb, _state)
#define SCTP_CLEAR_SUBSTATE(_stcb, _substate) \
	(_stcb)->asoc.state &= ~(_substate)
#define SCTP_ADD_SUBSTATE(_stcb, _substate) \
	sctp_add_substate(_stcb, _substate)

#define SCTP_ADDR_REACHABLE		0x001
#define SCTP_ADDR_NO_PMTUD              0x002
#define SCTP_ADDR_NOHB			0x004
#define SCTP_ADDR_BEING_DELETED		0x008
#define SCTP_ADDR_NOT_IN_ASSOC		0x010
#define SCTP_ADDR_OUT_OF_SCOPE		0x080
#define SCTP_ADDR_UNCONFIRMED		0x200
#define SCTP_ADDR_REQ_PRIMARY           0x400

#define SCTP_ADDR_PF                    0x800

#define SCTP_BOUND_V6		0x01
#define SCTP_BOUND_V4		0x02

#define SCTP_DEFAULT_MBUFS_IN_CHAIN 5

#define SCTP_DEFAULT_COOKIE_LIFE	60000

#define SCTP_MAPPING_ARRAY	512

#define SCTP_INITIAL_MAPPING_ARRAY  16

#define SCTP_MAPPING_ARRAY_INCR     32

#define SCTP_TIMER_INIT 	0
#define SCTP_TIMER_RECV 	1
#define SCTP_TIMER_SEND 	2
#define SCTP_TIMER_HEARTBEAT	3
#define SCTP_TIMER_PMTU		4
#define SCTP_TIMER_MAXSHUTDOWN	5
#define SCTP_TIMER_SIGNATURE	6

#define SCTP_NUM_TMRS	7

#define SCTP_TIMER_TYPE_NONE		0
#define SCTP_TIMER_TYPE_SEND		1
#define SCTP_TIMER_TYPE_INIT		2
#define SCTP_TIMER_TYPE_RECV		3
#define SCTP_TIMER_TYPE_SHUTDOWN	4
#define SCTP_TIMER_TYPE_HEARTBEAT	5
#define SCTP_TIMER_TYPE_COOKIE		6
#define SCTP_TIMER_TYPE_NEWCOOKIE	7
#define SCTP_TIMER_TYPE_PATHMTURAISE	8
#define SCTP_TIMER_TYPE_SHUTDOWNACK	9
#define SCTP_TIMER_TYPE_ASCONF		10
#define SCTP_TIMER_TYPE_SHUTDOWNGUARD	11
#define SCTP_TIMER_TYPE_AUTOCLOSE	12
#define SCTP_TIMER_TYPE_STRRESET	13
#define SCTP_TIMER_TYPE_INPKILL		14
#define SCTP_TIMER_TYPE_ASOCKILL	15
#define SCTP_TIMER_TYPE_ADDR_WQ		16
#define SCTP_TIMER_TYPE_PRIM_DELETED	17

#define SCTP_TIMER_TYPE_LAST		18

#define SCTP_IS_TIMER_TYPE_VALID(t)	(((t) > SCTP_TIMER_TYPE_NONE) && \
					 ((t) < SCTP_TIMER_TYPE_LAST))

#if defined(__APPLE__) && !defined(__Userspace__)

#define SCTP_MAIN_TIMER_DEFAULT		10

#endif

#define SCTP_MAX_DUP_TSNS	20

#define SCTP_RETRY_DROPPED_THRESH 4

#define SCTP_ASOC_MAX_CHUNKS_ON_QUEUE 512

#define SCTP_MINFR_MSEC_TIMER 250

#define SCTP_MINFR_MSEC_FLOOR 20

#define SCTP_INIT_SEC	1

#define SCTP_SEND_SEC	1

#define SCTP_RECV_MSEC	200

#define SCTP_HB_DEFAULT_MSEC	30000

#define SCTP_DEFAULT_SECRET_LIFE_SEC 3600

#define SCTP_RTO_UPPER_BOUND	(60000)
#define SCTP_RTO_LOWER_BOUND	(1000)
#define SCTP_RTO_INITIAL	(1000)

#define SCTP_INP_KILL_TIMEOUT 20
#define SCTP_ASOC_KILL_TIMEOUT 10

#define SCTP_DEF_MAX_INIT		8
#define SCTP_DEF_MAX_SEND		10
#define SCTP_DEF_MAX_PATH_RTX		5
#define SCTP_DEF_PATH_PF_THRESHOLD	SCTP_DEF_MAX_PATH_RTX

#define SCTP_DEF_PMTU_RAISE_SEC	600

#define SCTP_OSTREAM_INITIAL 10
#define SCTP_ISTREAM_INITIAL 2048

#define SCTP_MINIMAL_RWND		(4096)

#define SCTP_ADDRMAX		16

#define SCTP_DEBUG_TIMER1	0x00000001
#define SCTP_DEBUG_TIMER2	0x00000002
#define SCTP_DEBUG_TIMER3	0x00000004
#define SCTP_DEBUG_TIMER4	0x00000008
#define SCTP_DEBUG_OUTPUT1	0x00000010
#define SCTP_DEBUG_OUTPUT2	0x00000020
#define SCTP_DEBUG_OUTPUT3	0x00000040
#define SCTP_DEBUG_OUTPUT4	0x00000080
#define SCTP_DEBUG_UTIL1	0x00000100
#define SCTP_DEBUG_UTIL2	0x00000200
#define SCTP_DEBUG_AUTH1	0x00000400
#define SCTP_DEBUG_AUTH2	0x00000800
#define SCTP_DEBUG_INPUT1	0x00001000
#define SCTP_DEBUG_INPUT2	0x00002000
#define SCTP_DEBUG_INPUT3	0x00004000
#define SCTP_DEBUG_INPUT4	0x00008000
#define SCTP_DEBUG_ASCONF1	0x00010000
#define SCTP_DEBUG_ASCONF2	0x00020000
#define SCTP_DEBUG_OUTPUT5	0x00040000
#define SCTP_DEBUG_XXX		0x00080000
#define SCTP_DEBUG_PCB1		0x00100000
#define SCTP_DEBUG_PCB2		0x00200000
#define SCTP_DEBUG_PCB3		0x00400000
#define SCTP_DEBUG_PCB4		0x00800000
#define SCTP_DEBUG_INDATA1	0x01000000
#define SCTP_DEBUG_INDATA2	0x02000000
#define SCTP_DEBUG_INDATA3	0x04000000
#define SCTP_DEBUG_CRCOFFLOAD	0x08000000
#define SCTP_DEBUG_USRREQ1	0x10000000
#define SCTP_DEBUG_USRREQ2	0x20000000
#define SCTP_DEBUG_PEEL1	0x40000000
#if defined(__Userspace__)
#define SCTP_DEBUG_USR 		0x80000000
#else
#define SCTP_DEBUG_XXXXX	0x80000000
#endif
#define SCTP_DEBUG_ALL		0x7ff3ffff
#define SCTP_DEBUG_NOISY	0x00040000

#define SCTP_SWS_SENDER_DEF	1420

#define SCTP_SWS_RECEIVER_DEF	3000

#define SCTP_INITIAL_CWND 4380

#define SCTP_DEFAULT_MTU 1500

#define SCTP_MIN_RWND	1500

#define SCTP_DEFAULT_MAXSEGMENT 65535

#define SCTP_CHUNK_BUFFER_SIZE	512
#define SCTP_PARAM_BUFFER_SIZE	512

#define SCTP_SMALL_CHUNK_STORE 260

#define SCTP_HOW_MANY_SECRETS	2

#define SCTP_NUMBER_OF_SECRETS	8
#define SCTP_SECRET_SIZE	32

#define SCTP_NOTIFY_ASSOC_UP                     1
#define SCTP_NOTIFY_ASSOC_DOWN                   2
#define SCTP_NOTIFY_INTERFACE_DOWN               3
#define SCTP_NOTIFY_INTERFACE_UP                 4
#define SCTP_NOTIFY_SENT_DG_FAIL                 5
#define SCTP_NOTIFY_UNSENT_DG_FAIL               6
#define SCTP_NOTIFY_SPECIAL_SP_FAIL              7
#define SCTP_NOTIFY_ASSOC_LOC_ABORTED            8
#define SCTP_NOTIFY_ASSOC_REM_ABORTED            9
#define SCTP_NOTIFY_ASSOC_RESTART               10
#define SCTP_NOTIFY_PEER_SHUTDOWN               11
#define SCTP_NOTIFY_ASCONF_ADD_IP               12
#define SCTP_NOTIFY_ASCONF_DELETE_IP            13
#define SCTP_NOTIFY_ASCONF_SET_PRIMARY          14
#define SCTP_NOTIFY_PARTIAL_DELVIERY_INDICATION 15
#define SCTP_NOTIFY_INTERFACE_CONFIRMED         16
#define SCTP_NOTIFY_STR_RESET_RECV              17
#define SCTP_NOTIFY_STR_RESET_SEND              18
#define SCTP_NOTIFY_STR_RESET_FAILED_OUT        19
#define SCTP_NOTIFY_STR_RESET_FAILED_IN         20
#define SCTP_NOTIFY_STR_RESET_DENIED_OUT        21
#define SCTP_NOTIFY_STR_RESET_DENIED_IN         22
#define SCTP_NOTIFY_AUTH_NEW_KEY                23
#define SCTP_NOTIFY_AUTH_FREE_KEY               24
#define SCTP_NOTIFY_NO_PEER_AUTH                25
#define SCTP_NOTIFY_SENDER_DRY                  26
#define SCTP_NOTIFY_REMOTE_ERROR                27
#define SCTP_NOTIFY_ASSOC_TIMEDOUT              28

#define SCTP_DEFAULT_SPLIT_POINT_MIN 2904

#if defined(__Userspace__)
#define SCTP_DIAG_INFO_LEN 256
#else
#define SCTP_DIAG_INFO_LEN 128
#endif

#define SCTP_FROM_SCTP_INPUT        0x10000000
#define SCTP_FROM_SCTP_PCB          0x20000000
#define SCTP_FROM_SCTP_INDATA       0x30000000
#define SCTP_FROM_SCTP_TIMER        0x40000000
#define SCTP_FROM_SCTP_USRREQ       0x50000000
#define SCTP_FROM_SCTPUTIL          0x60000000
#define SCTP_FROM_SCTP6_USRREQ      0x70000000
#define SCTP_FROM_SCTP_ASCONF       0x80000000
#define SCTP_FROM_SCTP_OUTPUT       0x90000000
#define SCTP_FROM_SCTP_PEELOFF      0xa0000000
#define SCTP_FROM_SCTP_SYSCTL       0xb0000000
#define SCTP_FROM_SCTP_CC_FUNCTIONS 0xc0000000

#define SCTP_LOC_1  0x00000001
#define SCTP_LOC_2  0x00000002
#define SCTP_LOC_3  0x00000003
#define SCTP_LOC_4  0x00000004
#define SCTP_LOC_5  0x00000005
#define SCTP_LOC_6  0x00000006
#define SCTP_LOC_7  0x00000007
#define SCTP_LOC_8  0x00000008
#define SCTP_LOC_9  0x00000009
#define SCTP_LOC_10 0x0000000a
#define SCTP_LOC_11 0x0000000b
#define SCTP_LOC_12 0x0000000c
#define SCTP_LOC_13 0x0000000d
#define SCTP_LOC_14 0x0000000e
#define SCTP_LOC_15 0x0000000f
#define SCTP_LOC_16 0x00000010
#define SCTP_LOC_17 0x00000011
#define SCTP_LOC_18 0x00000012
#define SCTP_LOC_19 0x00000013
#define SCTP_LOC_20 0x00000014
#define SCTP_LOC_21 0x00000015
#define SCTP_LOC_22 0x00000016
#define SCTP_LOC_23 0x00000017
#define SCTP_LOC_24 0x00000018
#define SCTP_LOC_25 0x00000019
#define SCTP_LOC_26 0x0000001a
#define SCTP_LOC_27 0x0000001b
#define SCTP_LOC_28 0x0000001c
#define SCTP_LOC_29 0x0000001d
#define SCTP_LOC_30 0x0000001e
#define SCTP_LOC_31 0x0000001f
#define SCTP_LOC_32 0x00000020
#define SCTP_LOC_33 0x00000021
#define SCTP_LOC_34 0x00000022
#define SCTP_LOC_35 0x00000023
#define SCTP_LOC_36 0x00000024
#define SCTP_LOC_37 0x00000025

#define SCTP_NORMAL_PROC      0
#define SCTP_PCBFREE_NOFORCE  1
#define SCTP_PCBFREE_FORCE    2

#define SCTP_ADDR_IS_CONFIRMED 8
#define SCTP_ADDR_DYNAMIC_ADDED 6
#define SCTP_IN_COOKIE_PROC 100
#define SCTP_ALLOC_ASOC  1
#define SCTP_LOAD_ADDR_2 2
#define SCTP_LOAD_ADDR_3 3
#define SCTP_LOAD_ADDR_4 4
#define SCTP_LOAD_ADDR_5 5

#define SCTP_DONOT_SETSCOPE 0
#define SCTP_DO_SETSCOPE 1

#define SCTP_DEFAULT_ADD_MORE 1452

#ifndef SCTP_PCBHASHSIZE

#define SCTP_PCBHASHSIZE 256
#endif
#ifndef SCTP_TCBHASHSIZE
#define SCTP_TCBHASHSIZE 1024
#endif

#ifndef SCTP_CHUNKQUEUE_SCALE
#define SCTP_CHUNKQUEUE_SCALE 10
#endif

#if defined(__FreeBSD__) && !defined(__Userspace__)

#define SCTP_CLOCK_GRANULARITY	1
#else

#define SCTP_CLOCK_GRANULARITY	10
#endif
#define IP_HDR_SIZE 40

#define SCTP_CALLED_DIRECTLY_NOCMPSET     0
#define SCTP_CALLED_AFTER_CMPSET_OFCLOSE  1
#define SCTP_CALLED_FROM_INPKILL_TIMER    2

#define SCTP_FREE_SHOULD_USE_ABORT          1
#define SCTP_FREE_SHOULD_USE_GRACEFUL_CLOSE 0

#ifndef IPPROTO_SCTP
#define IPPROTO_SCTP 132
#endif

#define SCTP_MAX_DATA_BUNDLING		256

#define SCTP_UINT16_GT(a, b) (((a < b) && ((uint16_t)(b - a) > (1U<<15))) || \
                              ((a > b) && ((uint16_t)(a - b) < (1U<<15))))
#define SCTP_UINT16_GE(a, b) (SCTP_UINT16_GT(a, b) || (a == b))
#define SCTP_UINT32_GT(a, b) (((a < b) && ((uint32_t)(b - a) > (1U<<31))) || \
                              ((a > b) && ((uint32_t)(a - b) < (1U<<31))))
#define SCTP_UINT32_GE(a, b) (SCTP_UINT32_GT(a, b) || (a == b))

#define SCTP_SSN_GT(a, b) SCTP_UINT16_GT(a, b)
#define SCTP_SSN_GE(a, b) SCTP_UINT16_GE(a, b)
#define SCTP_TSN_GT(a, b) SCTP_UINT32_GT(a, b)
#define SCTP_TSN_GE(a, b) SCTP_UINT32_GE(a, b)
#define SCTP_MID_GT(i, a, b) (((i) == 1) ? SCTP_UINT32_GT(a, b) : SCTP_UINT16_GT((uint16_t)a, (uint16_t)b))
#define SCTP_MID_GE(i, a, b) (((i) == 1) ? SCTP_UINT32_GE(a, b) : SCTP_UINT16_GE((uint16_t)a, (uint16_t)b))
#define SCTP_MID_EQ(i, a, b) (((i) == 1) ? a == b : (uint16_t)a == (uint16_t)b)

#define SCTP_IS_TSN_PRESENT(arry, gap) ((arry[(gap >> 3)] >> (gap & 0x07)) & 0x01)
#define SCTP_SET_TSN_PRESENT(arry, gap) (arry[(gap >> 3)] |= (0x01 << ((gap & 0x07))))
#define SCTP_UNSET_TSN_PRESENT(arry, gap) (arry[(gap >> 3)] &= ((~(0x01 << ((gap & 0x07)))) & 0xff))
#define SCTP_CALC_TSN_TO_GAP(gap, tsn, mapping_tsn) do { \
	                if (tsn >= mapping_tsn) { \
						gap = tsn - mapping_tsn; \
					} else { \
						gap = (MAX_TSN - mapping_tsn) + tsn + 1; \
					} \
                  } while (0)

#define SCTP_RETRAN_DONE -1
#define SCTP_RETRAN_EXIT -2

#define SCTP_NUMBER_IN_VTAG_BLOCK 15

#define SCTP_STACK_VTAG_HASH_SIZE   32

#define SCTP_TIME_WAIT 60

#define SCTP_LOCAL_LAN_RTT 900
#define SCTP_LAN_UNKNOWN  0
#define SCTP_LAN_LOCAL    1
#define SCTP_LAN_INTERNET 2

#define SCTP_SEND_BUFFER_SPLITTING 0x00000001
#define SCTP_RECV_BUFFER_SPLITTING 0x00000002

#define SCTP_DEF_ASOC_RESC_LIMIT 10
#define SCTP_DEF_SYSTEM_RESC_LIMIT 1000

#define SCTP_SO_LOCKED		1
#define SCTP_SO_NOT_LOCKED	0

#define SCTP_ADDR_LOCKED 1
#define SCTP_ADDR_NOT_LOCKED 0

#define IN4_ISPRIVATE_ADDRESS(a) \
   ((((uint8_t *)&(a)->s_addr)[0] == 10) || \
    ((((uint8_t *)&(a)->s_addr)[0] == 172) && \
     (((uint8_t *)&(a)->s_addr)[1] >= 16) && \
     (((uint8_t *)&(a)->s_addr)[1] <= 32)) || \
    ((((uint8_t *)&(a)->s_addr)[0] == 192) && \
     (((uint8_t *)&(a)->s_addr)[1] == 168)))

#define IN4_ISLOOPBACK_ADDRESS(a) \
    (((uint8_t *)&(a)->s_addr)[0] == 127)

#define IN4_ISLINKLOCAL_ADDRESS(a) \
    ((((uint8_t *)&(a)->s_addr)[0] == 169) && \
     (((uint8_t *)&(a)->s_addr)[1] == 254))

#define SCTP_SOCKET_OPTION_LIMIT (64 * 1024)

#if defined(__Userspace__)
#if defined(_WIN32)
#define SCTP_GETTIME_TIMEVAL(x)	getwintimeofday(x)
#define SCTP_GETPTIME_TIMEVAL(x) getwintimeofday(x)
#else
#define SCTP_GETTIME_TIMEVAL(x)	gettimeofday(x, NULL)
#define SCTP_GETPTIME_TIMEVAL(x) gettimeofday(x, NULL)
#endif
#endif
#if defined(_KERNEL)
#define SCTP_GETTIME_TIMEVAL(x) (getmicrouptime(x))
#define SCTP_GETPTIME_TIMEVAL(x) (microuptime(x))
#endif

#if defined(_KERNEL) || defined(__Userspace__)
#define sctp_sowwakeup(inp, so) \
do { \
	if (inp->sctp_flags & SCTP_PCB_FLAGS_DONT_WAKE) { \
		inp->sctp_flags |= SCTP_PCB_FLAGS_WAKEOUTPUT; \
	} else { \
		sowwakeup(so); \
	} \
} while (0)

#if defined(__FreeBSD__) || defined(_WIN32) || defined(__Userspace__)
#define sctp_sowwakeup_locked(inp, so) \
do { \
	if (inp->sctp_flags & SCTP_PCB_FLAGS_DONT_WAKE) { \
		SOCKBUF_UNLOCK(&((so)->so_snd)); \
		inp->sctp_flags |= SCTP_PCB_FLAGS_WAKEOUTPUT; \
	} else { \
		sowwakeup_locked(so); \
	} \
} while (0)
#else
#define sctp_sowwakeup_locked(inp, so) \
do { \
	if (inp->sctp_flags & SCTP_PCB_FLAGS_DONT_WAKE) { \
		SOCKBUF_UNLOCK(&((so)->so_snd)); \
		inp->sctp_flags |= SCTP_PCB_FLAGS_WAKEOUTPUT; \
	} else { \
		sowwakeup(so); \
	} \
} while (0)
#endif

#define sctp_sorwakeup(inp, so) \
do { \
	if (inp->sctp_flags & SCTP_PCB_FLAGS_DONT_WAKE) { \
		inp->sctp_flags |= SCTP_PCB_FLAGS_WAKEINPUT; \
	} else { \
		sorwakeup(so); \
	} \
} while (0)

#if defined(__FreeBSD__) || defined(_WIN32) || defined(__Userspace__)
#define sctp_sorwakeup_locked(inp, so) \
do { \
	if (inp->sctp_flags & SCTP_PCB_FLAGS_DONT_WAKE) { \
		inp->sctp_flags |= SCTP_PCB_FLAGS_WAKEINPUT; \
		SOCKBUF_UNLOCK(&((so)->so_rcv)); \
	} else { \
		sorwakeup_locked(so); \
	} \
} while (0)
#else

#define sctp_sorwakeup_locked(inp, so) \
do { \
	if (inp->sctp_flags & SCTP_PCB_FLAGS_DONT_WAKE) { \
		inp->sctp_flags |= SCTP_PCB_FLAGS_WAKEINPUT; \
		SOCKBUF_UNLOCK(&((so)->so_rcv)); \
	} else { \
		sorwakeup(so); \
	} \
} while (0)
#endif

#endif
#endif
