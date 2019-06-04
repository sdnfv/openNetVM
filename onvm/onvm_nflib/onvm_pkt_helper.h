/*********************************************************************
 *                     openNetVM
 *              https://sdnfv.github.io
 *
 *   BSD LICENSE
 *
 *   Copyright(c)
 *            2015-2019 George Washington University
 *            2015-2019 University of California Riverside
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

#include <inttypes.h>
#include <rte_ether.h>
#include <rte_mempool.h>

struct port_info;
struct rte_mbuf;
struct tcp_hdr;
struct udp_hdr;
struct ipv4_hdr;

#define IP_PROTOCOL_TCP 6
#define IP_PROTOCOL_UDP 17

#define SUPPORTS_IPV4_CHECKSUM_OFFLOAD (1 << 0)
#define SUPPORTS_TCP_CHECKSUM_OFFLOAD (1 << 1)
#define SUPPORTS_UDP_CHECKSUM_OFFLOAD (1 << 2)

#define PROTO_UDP 0x11
#define IPV4_VERSION_IHL 69
#define IPV4_TTL 64
#define UDP_SAMPLE_SRC_PORT 12345
#define UDP_SAMPLE_DST_PORT 54321
#define IPV4_SAMPLE_SRC (uint32_t) IPv4(10, 0, 0, 1)
#define IPV4_SAMPLE_DST (uint32_t) IPv4(10, 0, 0, 2)
#define SAMPLE_NIC_PORT 0

/* Returns the bitflags in the tcp header */
#define ONVM_PKT_GET_FLAGS(tcp, flags)                                               \
        do {                                                                         \
                (flags) = (((tcp)->data_off << 8) | (tcp)->tcp_flags) & 0b111111111; \
        } while (0)

/* Sets the bitflags in the tcp header */
#define ONVM_PKT_SET_FLAGS(tcp, flags)                   \
        do {                                             \
                (tcp)->tcp_flags = (flags)&0xFF;         \
                (tcp)->data_off |= ((flags) >> 8) & 0x1; \
        } while (0)

/**
 * Assign the source and destination MAC address of the packet to the specified
 * source and destination port ID.
 */
int
onvm_pkt_set_mac_addr(struct rte_mbuf* pkt, unsigned src_port_id, unsigned dst_port_id, struct port_info* ports);

/**
 * Swap the source MAC address of a packet with a specified destination port's MAC address.
 */
int
onvm_pkt_swap_src_mac_addr(struct rte_mbuf* pkt, unsigned dst_port_id, struct port_info* ports);

/**
 * Swap the destination MAC address of a packet with a specified source port's MAC address.
 */
int
onvm_pkt_swap_dst_mac_addr(struct rte_mbuf* pkt, unsigned src_port_id, struct port_info* ports);

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

/**
 * Parsing ip addr of form X.X.X.X into decimal form
 */
int
onvm_pkt_parse_ip(char* ip_str, uint32_t* dest);

/**
 * Parse uint32 IP into a string
 */
void
onvm_pkt_parse_char_ip(char* ip_dest, uint32_t ip_src);

/**
 * Parsing mac addr of form xx:xx:xx:xx:xx:xx into dest array
 */
int
onvm_pkt_parse_mac(char* mac_str, uint8_t* dest);

/**
 * Packet checksum calculation routines
 */
uint32_t
onvm_pkt_get_checksum_offload_flags(uint8_t port_id);

/**
 * Set the packet checksums for the passed in pkt
 */
void
onvm_pkt_set_checksums(struct rte_mbuf* pkt);

/**
 * Fill the packet UDP header
 */
int
onvm_pkt_fill_udp(struct udp_hdr* udp_hdr, uint16_t src_port, uint16_t dst_port, uint16_t payload_len);

/**
 * Fill the packet IP header
 */
int
onvm_pkt_fill_ipv4(struct ipv4_hdr* iph, uint32_t src, uint32_t dst, uint8_t l4_proto);

/**
 * Fill the ether header
 */
int
onvm_pkt_fill_ether(struct ether_hdr* eth_hdr, int port, struct ether_addr* dst_mac_addr, struct port_info* ports);

/**
 * Swap the ether header values
 */
int
onvm_pkt_swap_ether_hdr(struct ether_hdr* ether_hdr);

/**
 * Generates a UDP packet with the provided values
 */
int
onvm_pkt_swap_ip_hdr(struct ipv4_hdr* ip_hdr);

/**
 * Generates a TCP packet with the provided values
 */
int
onvm_pkt_swap_tcp_hdr(struct tcp_hdr* tcp_hdr);

/**
 * Generates a UDP packet with the provided values
 */
struct rte_mbuf*
onvm_pkt_generate_tcp(struct rte_mempool* pktmbuf_pool, struct tcp_hdr* tcp_hdr, struct ipv4_hdr* iph,
                      struct ether_hdr* eth_hdr, uint8_t* options, size_t option_len, uint8_t* payload,
                      size_t payload_len);

/**
 * Generates a UDP packet with the provided values
 */
struct rte_mbuf*
onvm_pkt_generate_udp(struct rte_mempool* pktmbuf_pool, struct udp_hdr* udp_hdr, struct ipv4_hdr* iph,
                      struct ether_hdr* eth_hdr, uint8_t* payload, size_t payload_len);

/**
 * Generates a sample UDP packet
 */
struct rte_mbuf*
onvm_pkt_generate_udp_sample(struct rte_mempool* pktmbuf_pool);

#endif  // _ONVM_PKT_HELPER_H_"
