/*********************************************************************
 *                     openNetVM
 *              https://sdnfv.github.io
 *
 *   BSD LICENSE
 *
 *   Copyright(c)
 *            2015-2016 George Washington University
 *            2015-2016 University of California Riverside
 *   All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * The name of the author may not be used to endorse or promote
 *       products derived from this software without specific prior
 *       written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * onvm_pkt_helper.h - packet helper routines
 ********************************************************************/

#ifndef _ONVM_PKT_HELPER_H_
#define _ONVM_PKT_HELPER_H_

struct rte_mbuf;
struct tcp_hdr;
struct udp_hdr;
struct ipv4_hdr;

#define IP_PROTOCOL_TCP 6
#define IP_PROTOCOL_UDP 17

/**
 * Flip the source and destination mac address of a packet
 */
int
onvm_pkt_mac_addr_swap(struct rte_mbuf* pkt, unsigned dst_port);

/**
 * Return a pointer to the tcp/udp/ip header in the packet, or NULL if not a TCP packet
 */
struct tcp_hdr*
onvm_pkt_tcp_hdr(struct rte_mbuf* pkt);

struct ether_hdr*
onvm_pkt_ether_hdr(struct rte_mbuf* pkt);

struct udp_hdr*
onvm_pkt_udp_hdr(struct rte_mbuf* pkt);

struct ipv4_hdr*
onvm_pkt_ipv4_hdr(struct rte_mbuf* pkt);

/**
 * Check the type of a packet. Return 1 if packet is of the specified type, else 0
 */
int
onvm_pkt_is_tcp(struct rte_mbuf* pkt);

int
onvm_pkt_is_udp(struct rte_mbuf* pkt);

int
onvm_pkt_is_ipv4(struct rte_mbuf* pkt);

/**
 * Print out a packet or header.  Check to be sure DPDK doesn't already do any of these
 */
void
onvm_pkt_print(struct rte_mbuf* pkt);  // calls the funcs below

void
onvm_pkt_print_tcp(struct tcp_hdr* hdr);

void
onvm_pkt_print_udp(struct udp_hdr* hdr);

void
onvm_pkt_print_ipv4(struct ipv4_hdr* hdr);

void
onvm_pkt_print_ether(struct ether_hdr* hdr);

#endif  // _ONVM_PKT_HELPER_H_"
