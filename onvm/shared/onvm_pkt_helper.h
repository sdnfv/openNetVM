/*********************************************************************
 *                     openNetVM
 *       https://github.com/sdnfv/openNetVM
 *
 *  Copyright 2015 George Washington University
 *            2015 University of California Riverside
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
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
void
onvm_pkt_mac_addr_swap(struct rte_mbuf* pkt, unsigned dst_port);

/**
 * Return a pointer to the tcp/udp/ip header in the packet, or NULL if not a TCP packet
 */
struct tcp_hdr*
onvm_pkt_tcp_hdr(struct rte_mbuf* pkt);

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

#endif  // _ONVM_PKT_HELPER_H_"
