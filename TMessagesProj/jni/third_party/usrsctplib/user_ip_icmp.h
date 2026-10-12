/*-
 * Copyright (c) 1982, 1986, 1993
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

#ifndef _USER_IP_ICMP_H_
#define _USER_IP_ICMP_H_

struct icmp_ra_addr {
	uint32_t ira_addr;
	uint32_t ira_preference;
};

struct icmphdr {
	u_char	icmp_type;
	u_char	icmp_code;
	u_short	icmp_cksum;
};

#if defined(_WIN32)
#pragma pack (push, 1)
struct icmp6_hdr {
	uint8_t icmp6_type;
	uint8_t icmp6_code;
	uint16_t icmp6_cksum;
	union {
		uint32_t icmp6_un_data32[1];
		uint16_t icmp6_un_data16[2];
		uint8_t icmp6_un_data8[4];
	} icmp6_dataun;
};
#pragma pack(pop)

#define icmp6_data32 icmp6_dataun.icmp6_un_data32
#define icmp6_mtu icmp6_data32[0]
#endif

struct icmp {
	u_char	icmp_type;
	u_char	icmp_code;
	u_short	icmp_cksum;
	union {
		u_char ih_pptr;
		struct in_addr ih_gwaddr;
		struct ih_idseq {
			uint16_t	icd_id;
			uint16_t	icd_seq;
		} ih_idseq;
		int ih_void;

		struct ih_pmtu {
			uint16_t ipm_void;
			uint16_t ipm_nextmtu;
		} ih_pmtu;

		struct ih_rtradv {
			u_char irt_num_addrs;
			u_char irt_wpa;
			uint16_t irt_lifetime;
		} ih_rtradv;
	} icmp_hun;
#define	icmp_pptr	icmp_hun.ih_pptr
#define	icmp_gwaddr	icmp_hun.ih_gwaddr
#define	icmp_id		icmp_hun.ih_idseq.icd_id
#define	icmp_seq	icmp_hun.ih_idseq.icd_seq
#define	icmp_void	icmp_hun.ih_void
#define	icmp_pmvoid	icmp_hun.ih_pmtu.ipm_void
#define	icmp_nextmtu	icmp_hun.ih_pmtu.ipm_nextmtu
#define	icmp_num_addrs	icmp_hun.ih_rtradv.irt_num_addrs
#define	icmp_wpa	icmp_hun.ih_rtradv.irt_wpa
#define	icmp_lifetime	icmp_hun.ih_rtradv.irt_lifetime
	union {
		struct id_ts {

			uint32_t its_otime;
			uint32_t its_rtime;
			uint32_t its_ttime;
		} id_ts;
		struct id_ip  {
			struct ip idi_ip;

		} id_ip;
		struct icmp_ra_addr id_radv;
		uint32_t id_mask;
		char	id_data[1];
	} icmp_dun;
#define	icmp_otime	icmp_dun.id_ts.its_otime
#define	icmp_rtime	icmp_dun.id_ts.its_rtime
#define	icmp_ttime	icmp_dun.id_ts.its_ttime
#define	icmp_ip		icmp_dun.id_ip.idi_ip
#define	icmp_radv	icmp_dun.id_radv
#define	icmp_mask	icmp_dun.id_mask
#define	icmp_data	icmp_dun.id_data
};

#define	ICMP_MINLEN	8
#define	ICMP_TSLEN	(8 + 3 * sizeof (uint32_t))
#define	ICMP_MASKLEN	12
#define	ICMP_ADVLENMIN	(8 + sizeof (struct ip) + 8)
#define	ICMP_ADVLEN(p)	(8 + ((p)->icmp_ip.ip_hl << 2) + 8)

#define	ICMP_ECHOREPLY		0
#define	ICMP_UNREACH		3
#define		ICMP_UNREACH_NET	0
#define		ICMP_UNREACH_HOST	1
#define		ICMP_UNREACH_PROTOCOL	2
#define		ICMP_UNREACH_PORT	3
#define		ICMP_UNREACH_NEEDFRAG	4
#define		ICMP_UNREACH_SRCFAIL	5
#define		ICMP_UNREACH_NET_UNKNOWN 6
#define		ICMP_UNREACH_HOST_UNKNOWN 7
#define		ICMP_UNREACH_ISOLATED	8
#define		ICMP_UNREACH_NET_PROHIB	9
#define		ICMP_UNREACH_HOST_PROHIB 10
#define		ICMP_UNREACH_TOSNET	11
#define		ICMP_UNREACH_TOSHOST	12
#define		ICMP_UNREACH_FILTER_PROHIB 13
#define		ICMP_UNREACH_HOST_PRECEDENCE 14
#define		ICMP_UNREACH_PRECEDENCE_CUTOFF 15
#define	ICMP_SOURCEQUENCH	4
#define	ICMP_REDIRECT		5
#define		ICMP_REDIRECT_NET	0
#define		ICMP_REDIRECT_HOST	1
#define		ICMP_REDIRECT_TOSNET	2
#define		ICMP_REDIRECT_TOSHOST	3
#define	ICMP_ALTHOSTADDR	6
#define	ICMP_ECHO		8
#define	ICMP_ROUTERADVERT	9
#define		ICMP_ROUTERADVERT_NORMAL		0
#define		ICMP_ROUTERADVERT_NOROUTE_COMMON	16
#define	ICMP_ROUTERSOLICIT	10
#define	ICMP_TIMXCEED		11
#define		ICMP_TIMXCEED_INTRANS	0
#define		ICMP_TIMXCEED_REASS	1
#define	ICMP_PARAMPROB		12
#define		ICMP_PARAMPROB_ERRATPTR 0
#define		ICMP_PARAMPROB_OPTABSENT 1
#define		ICMP_PARAMPROB_LENGTH 2
#define	ICMP_TSTAMP		13
#define	ICMP_TSTAMPREPLY	14
#define	ICMP_IREQ		15
#define	ICMP_IREQREPLY		16
#define	ICMP_MASKREQ		17
#define	ICMP_MASKREPLY		18
#define	ICMP_TRACEROUTE		30
#define	ICMP_DATACONVERR	31
#define	ICMP_MOBILE_REDIRECT	32
#define	ICMP_IPV6_WHEREAREYOU	33
#define	ICMP_IPV6_IAMHERE	34
#define	ICMP_MOBILE_REGREQUEST	35
#define	ICMP_MOBILE_REGREPLY	36
#define	ICMP_SKIP		39
#define	ICMP_PHOTURIS		40
#define		ICMP_PHOTURIS_UNKNOWN_INDEX	1
#define		ICMP_PHOTURIS_AUTH_FAILED	2
#define		ICMP_PHOTURIS_DECRYPT_FAILED	3

#define	ICMP_MAXTYPE		40

#define	ICMP_INFOTYPE(type) \
	((type) == ICMP_ECHOREPLY || (type) == ICMP_ECHO || \
	(type) == ICMP_ROUTERADVERT || (type) == ICMP_ROUTERSOLICIT || \
	(type) == ICMP_TSTAMP || (type) == ICMP_TSTAMPREPLY || \
	(type) == ICMP_IREQ || (type) == ICMP_IREQREPLY || \
	(type) == ICMP_MASKREQ || (type) == ICMP_MASKREPLY)

#endif
