/*********************************************************************
 *                     openNetVM
 *              https://sdnfv.github.io
 *
 *   BSD LICENSE
 *
 *   Copyright(c)
 *            2015-2017 George Washington University
 *            2015-2017 University of California Riverside
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
 * onvm_pkt_helper.c - packet helper routines
 ********************************************************************/

#include "onvm_pkt_helper.h"
#include "onvm_common.h"

#include <inttypes.h>

#include <rte_branch_prediction.h>
#include <rte_mbuf.h>

#include <rte_ethdev.h>
#include <rte_ether.h>
#include <rte_ip.h>
#include <rte_tcp.h>
#include <rte_udp.h>

#include <rte_memcpy.h>
#include <rte_mempool.h>

int
onvm_pkt_set_mac_addr(struct rte_mbuf* pkt, unsigned src_port_id, unsigned dst_port_id, struct port_info* ports) {
        struct ether_hdr* eth;

        if (unlikely(pkt == NULL)) {  // We do not expect to swap macs for empty packets
                return -1;
        }

        /*
         * Get the ethernet header from the pkt
         */
        eth = onvm_pkt_ether_hdr(pkt);

        /*
         * Get the MAC addresses of the src and destination NIC ports,
         * and set the ethernet header's fields to them.
         */
        ether_addr_copy(&ports->mac[src_port_id], &eth->s_addr);
        ether_addr_copy(&ports->mac[dst_port_id], &eth->d_addr);

        return 0;
}

int
onvm_pkt_swap_src_mac_addr(struct rte_mbuf* pkt, unsigned dst_port_id, struct port_info* ports) {
        struct ether_hdr* eth;

        if (unlikely(pkt == NULL)) {  // We do not expect to swap macs for empty packets
                return -1;
        }

        /*
         * Get the ethernet header from the pkt
         */
        eth = onvm_pkt_ether_hdr(pkt);

        /*
         * Copy the source mac address to the destination field.
         */
        ether_addr_copy(&eth->s_addr, &eth->d_addr);

        /*
         * Get the mac address of the specified destination port id
         * and set the source field to it.
         */
        ether_addr_copy(&ports->mac[dst_port_id], &eth->s_addr);

        return 0;
}

int
onvm_pkt_swap_dst_mac_addr(struct rte_mbuf* pkt, unsigned src_port_id, struct port_info* ports) {
        struct ether_hdr* eth;

        if (unlikely(pkt == NULL)) {  // We do not expect to swap macs for empty packets
                return -1;
        }

        /*
         * Get the ethernet header from the pkt
         */
        eth = onvm_pkt_ether_hdr(pkt);

        /*
         * Copy the destination mac address to the source field.
         */
        ether_addr_copy(&eth->d_addr, &eth->s_addr);

        /*
         * Get the mac address of specified source port id
         * and set the destination field to it.
         */
        ether_addr_copy(&ports->mac[src_port_id], &eth->d_addr);

        return 0;
}

struct ether_hdr*
onvm_pkt_ether_hdr(struct rte_mbuf* pkt) {
        if (unlikely(pkt == NULL)) {
                return NULL;
        }
        return rte_pktmbuf_mtod(pkt, struct ether_hdr*);
}

struct tcp_hdr*
onvm_pkt_tcp_hdr(struct rte_mbuf* pkt) {
        struct ipv4_hdr* ipv4 = onvm_pkt_ipv4_hdr(pkt);

        if (unlikely(
                ipv4 ==
                NULL)) {  // Since we aren't dealing with IPv6 packets for now, we can ignore anything that isn't IPv4
                return NULL;
        }

        if (ipv4->next_proto_id != IP_PROTOCOL_TCP) {
                return NULL;
        }

        uint8_t* pkt_data = rte_pktmbuf_mtod(pkt, uint8_t*) + sizeof(struct ether_hdr) + sizeof(struct ipv4_hdr);
        return (struct tcp_hdr*)pkt_data;
}

struct udp_hdr*
onvm_pkt_udp_hdr(struct rte_mbuf* pkt) {
        struct ipv4_hdr* ipv4 = onvm_pkt_ipv4_hdr(pkt);

        if (unlikely(
                ipv4 ==
                NULL)) {  // Since we aren't dealing with IPv6 packets for now, we can ignore anything that isn't IPv4
                return NULL;
        }

        if (ipv4->next_proto_id != IP_PROTOCOL_UDP) {
                return NULL;
        }

        uint8_t* pkt_data = rte_pktmbuf_mtod(pkt, uint8_t*) + sizeof(struct ether_hdr) + sizeof(struct ipv4_hdr);
        return (struct udp_hdr*)pkt_data;
}

struct ipv4_hdr*
onvm_pkt_ipv4_hdr(struct rte_mbuf* pkt) {
        struct ipv4_hdr* ipv4 = (struct ipv4_hdr*)(rte_pktmbuf_mtod(pkt, uint8_t*) + sizeof(struct ether_hdr));

        /* In an IP packet, the first 4 bits determine the version.
         * The next 4 bits are called the Internet Header Length, or IHL.
         * DPDK's ipv4_hdr struct combines both the version and the IHL into one uint8_t.
         */
        uint8_t version = (ipv4->version_ihl >> 4) & 0b1111;
        if (unlikely(version != 4)) {
                return NULL;
        }
        return ipv4;
}

int
onvm_pkt_is_tcp(struct rte_mbuf* pkt) {
        return onvm_pkt_tcp_hdr(pkt) != NULL;
}

int
onvm_pkt_is_udp(struct rte_mbuf* pkt) {
        return onvm_pkt_udp_hdr(pkt) != NULL;
}

int
onvm_pkt_is_ipv4(struct rte_mbuf* pkt) {
        return onvm_pkt_ipv4_hdr(pkt) != NULL;
}

void
onvm_pkt_print(struct rte_mbuf* pkt) {
        struct ipv4_hdr* ipv4 = onvm_pkt_ipv4_hdr(pkt);
        if (likely(ipv4 != NULL)) {
                onvm_pkt_print_ipv4(ipv4);
        }

        struct tcp_hdr* tcp = onvm_pkt_tcp_hdr(pkt);
        if (tcp != NULL) {
                onvm_pkt_print_tcp(tcp);
        }

        struct udp_hdr* udp = onvm_pkt_udp_hdr(pkt);
        if (udp != NULL) {
                onvm_pkt_print_udp(udp);
        }
}

void
onvm_pkt_print_tcp(struct tcp_hdr* hdr) {
        printf("Source Port: %" PRIu16 "\n", rte_be_to_cpu_16(hdr->src_port));
        printf("Destination Port: %" PRIu16 "\n", rte_be_to_cpu_16(hdr->dst_port));
        printf("Sequence number: %" PRIu32 "\n", rte_be_to_cpu_32(hdr->sent_seq));
        printf("Acknowledgement number: %" PRIu32 "\n", rte_be_to_cpu_32(hdr->recv_ack));
        printf("Data offset: %" PRIu8 "\n", hdr->data_off);

        /* TCP defines 9 different 1-bit flags, but DPDK's flags field only leaves room for 8.
         * I think DPDK's struct puts the first flag in the last bit of the data offset field.
         */
        uint16_t flags = ((hdr->data_off << 8) | hdr->tcp_flags) & 0b111111111;

        printf("Flags: %" PRIx16 "\n", flags);
        printf("\t(");
        if ((flags >> 8) & 0x1)
                printf("NS,");
        if ((flags >> 7) & 0x1)
                printf("CWR,");
        if ((flags >> 6) & 0x1)
                printf("ECE,");
        if ((flags >> 5) & 0x1)
                printf("URG,");
        if ((flags >> 4) & 0x1)
                printf("ACK,");
        if ((flags >> 3) & 0x1)
                printf("PSH,");
        if ((flags >> 2) & 0x1)
                printf("RST,");
        if ((flags >> 1) & 0x1)
                printf("SYN,");
        if (flags & 0x1)
                printf("FIN,");
        printf(")\n");

        printf("Window Size: %" PRIu16 "\n", rte_be_to_cpu_16(hdr->rx_win));
        printf("Checksum: %" PRIu16 "\n", rte_be_to_cpu_16(hdr->cksum));
        printf("Urgent Pointer: %" PRIu16 "\n", rte_be_to_cpu_16(hdr->tcp_urp));
}

void
onvm_pkt_print_udp(struct udp_hdr* hdr) {
        printf("Source Port: %" PRIu16 "\n", hdr->src_port);
        printf("Destination Port: %" PRIu16 "\n", hdr->dst_port);
        printf("Length: %" PRIu16 "\n", hdr->dgram_len);
        printf("Checksum: %" PRIu16 "\n", hdr->dgram_cksum);
}

void
onvm_pkt_print_ipv4(struct ipv4_hdr* hdr) {
        printf("IHL: %" PRIu8 "\n", hdr->version_ihl & 0b1111);
        printf("DSCP: %" PRIu8 "\n", hdr->type_of_service & 0b111111);
        printf("ECN: %" PRIu8 "\n", (hdr->type_of_service >> 6) & 0b11);
        printf("Total Length: %" PRIu16 "\n", rte_be_to_cpu_16(hdr->total_length));
        printf("Identification: %" PRIu16 "\n", rte_be_to_cpu_16(hdr->packet_id));

        uint8_t flags = (hdr->fragment_offset >> 13) & 0b111;  // there are three 1-bit flags, but only 2 are used
        printf("Flags: %" PRIx8 "\n", flags);
        printf("\t(");
        if ((flags >> 1) & 0x1)
                printf("DF,");
        if (flags & 0x1)
                printf("MF,");
        printf("\n");

        printf("Fragment Offset: %" PRIu16 "\n", rte_be_to_cpu_16(hdr->fragment_offset) & 0b1111111111111);
        printf("TTL: %" PRIu8 "\n", hdr->time_to_live);
        printf("Protocol: %" PRIu8, hdr->next_proto_id);

        if (hdr->next_proto_id == IP_PROTOCOL_TCP) {
                printf(" (TCP)");
        }

        if (hdr->next_proto_id == IP_PROTOCOL_UDP) {
                printf(" (UDP)");
        }

        printf("\n");

        printf("Header Checksum: %" PRIu16 "\n", hdr->hdr_checksum);
        printf("Source IP: %" PRIu32 " (%" PRIu8 ".%" PRIu8 ".%" PRIu8 ".%" PRIu8 ")\n", hdr->src_addr,
               hdr->src_addr & 0xFF, (hdr->src_addr >> 8) & 0xFF, (hdr->src_addr >> 16) & 0xFF,
               (hdr->src_addr >> 24) & 0xFF);
        printf("Destination IP: %" PRIu32 " (%" PRIu8 ".%" PRIu8 ".%" PRIu8 ".%" PRIu8 ")\n", hdr->dst_addr,
               hdr->dst_addr & 0xFF, (hdr->dst_addr >> 8) & 0xFF, (hdr->dst_addr >> 16) & 0xFF,
               (hdr->dst_addr >> 24) & 0xFF);
}

void
onvm_pkt_print_ether(struct ether_hdr* hdr) {
        const char* type = NULL;
        if (unlikely(hdr == NULL)) {
                return;
        }
        printf("Source MAC: %02x:%02x:%02x:%02x:%02x:%02x\n", hdr->s_addr.addr_bytes[0], hdr->s_addr.addr_bytes[1],
               hdr->s_addr.addr_bytes[2], hdr->s_addr.addr_bytes[3], hdr->s_addr.addr_bytes[4],
               hdr->s_addr.addr_bytes[5]);
        printf("Dest MAC: %02x:%02x:%02x:%02x:%02x:%02x\n", hdr->d_addr.addr_bytes[0], hdr->d_addr.addr_bytes[1],
               hdr->d_addr.addr_bytes[2], hdr->d_addr.addr_bytes[3], hdr->d_addr.addr_bytes[4],
               hdr->d_addr.addr_bytes[5]);
        switch (hdr->ether_type) {
                case ETHER_TYPE_IPv4:
                        type = "IPv4";
                        break;
                case ETHER_TYPE_IPv6:
                        type = "IPv6";
                        break;
                case ETHER_TYPE_ARP:
                        type = "ARP";
                        break;
                case ETHER_TYPE_RARP:
                        type = "Reverse ARP";
                        break;
                case ETHER_TYPE_VLAN:
                        type = "VLAN";
                        break;
                case ETHER_TYPE_1588:
                        type = "1588 Precise Time";
                        break;
                case ETHER_TYPE_SLOW:
                        type = "Slow";
                        break;
                case ETHER_TYPE_TEB:
                        type = "Transparent Ethernet Bridging (TEP)";
                        break;
                default:
                        type = "unknown";
                        break;
        }
        printf("Type: %s\n", type);
}

int
onvm_pkt_parse_ip(char* ip_str, uint32_t* dest) {
        int ret;
        int ip[4];

        if (ip_str == NULL || dest == NULL) {
                return -1;
        }

        ret = sscanf(ip_str, "%u.%u.%u.%u", &ip[3], &ip[2], &ip[1], &ip[0]);
        if (ret != 4) {
                return -1;
        }
        *dest = IPv4(ip[0], ip[1], ip[2], ip[3]);
        return 0;
}

int
onvm_pkt_parse_mac(char* mac_str, uint8_t* dest) {
        int ret, i;
        int mac[ETHER_ADDR_LEN];

        if (mac_str == NULL || dest == NULL) {
                return -1;
        }

        ret = sscanf(mac_str, "%x:%x:%x:%x:%x:%x", &mac[0], &mac[1], &mac[2], &mac[3], &mac[4], &mac[5]);
        if (ret != ETHER_ADDR_LEN) {
                return -1;
        }

        for (i = 0; i < ETHER_ADDR_LEN; i++) {
                dest[i] = mac[i];
        }
        return 0;
}

uint32_t
onvm_pkt_get_checksum_offload_flags(uint8_t port_id) {
        struct rte_eth_dev_info dev_info;
        uint32_t hw_offload_flags = 0;

        rte_eth_dev_info_get(port_id, &dev_info);

        if (dev_info.tx_offload_capa & DEV_TX_OFFLOAD_IPV4_CKSUM) {
                hw_offload_flags |= SUPPORTS_IPV4_CHECKSUM_OFFLOAD;
        }
        if (dev_info.tx_offload_capa & DEV_TX_OFFLOAD_TCP_CKSUM) {
                hw_offload_flags |= SUPPORTS_TCP_CHECKSUM_OFFLOAD;
        }
        if (dev_info.tx_offload_capa & DEV_TX_OFFLOAD_UDP_CKSUM) {
                hw_offload_flags |= SUPPORTS_UDP_CHECKSUM_OFFLOAD;
        }
        return hw_offload_flags;
}

/**
 * Calculates TCP or UDP checksum.
 * This is the same implementation as rte_ipv4_udptcp_cksum(),
 * except that this implementation can process packets with IP options.
 */
static uint16_t
calculate_tcpudp_cksum(const struct ipv4_hdr* ip, const void* l4_hdr, const uint32_t l3_len, uint8_t protocol) {
        uint32_t cksum = 0;
        uint32_t l4_len = ip->total_length - l3_len;

        /* pseudo header checksum */
        struct {
                uint32_t saddr;
                uint32_t daddr;
                uint8_t reserved; /* all zeros */
                uint8_t protocol;
                uint16_t total_length; /* the length of the TCP/UDP header and data */
        } __attribute__((__packed__)) ph;

        ph.saddr = ip->src_addr;
        ph.daddr = ip->dst_addr;
        ph.reserved = 0;
        ph.protocol = protocol;
        ph.total_length = (uint16_t)l4_len;

        cksum += rte_raw_cksum(&ph, sizeof(ph));

        /* packet checksum */
        cksum += rte_raw_cksum(l4_hdr, l4_len);

        while (cksum & 0xffff0000) {
                cksum = ((cksum & 0xffff0000) >> 16) + (cksum & 0xffff);
        }
        return ~cksum;
}

/**
 * Calculates IP checksum.
 * This is the same implementation as rte_ipv4_cksum(),
 * exception that this implementation can process packets with IP options.
 */
static uint16_t
calculate_ip_cksum(const struct ipv4_hdr* ip, const uint32_t l3_len) {
        uint16_t cksum = rte_raw_cksum(ip, l3_len);
        return (cksum == 0xffff) ? cksum : ~cksum;
}

void
onvm_pkt_set_checksums(struct rte_mbuf* pkt) {
        uint32_t hw_cksum_support = onvm_pkt_get_checksum_offload_flags(pkt->port);
        struct ipv4_hdr* ip = onvm_pkt_ipv4_hdr(pkt);
        struct tcp_hdr* tcp = onvm_pkt_tcp_hdr(pkt);
        struct udp_hdr* udp = onvm_pkt_udp_hdr(pkt);

        if (ip != NULL) {
                ip->hdr_checksum = 0;
                pkt->l2_len = sizeof(struct ether_hdr);
                pkt->l3_len = (ip->version_ihl & 0b1111) * 4;
                pkt->ol_flags |= PKT_TX_IPV4;

                if (tcp != NULL) {
                        tcp->cksum = 0;
                        pkt->l4_len = (tcp->data_off >> 4) & 0b1111;

                        if (hw_cksum_support & SUPPORTS_TCP_CHECKSUM_OFFLOAD) {
                                tcp->cksum = rte_ipv4_phdr_cksum(ip, pkt->ol_flags);
                                pkt->ol_flags |= PKT_TX_TCP_CKSUM;
                        } else {
                                /* software TCP checksumming */
                                tcp->cksum = calculate_tcpudp_cksum(ip, tcp, pkt->l3_len, IP_PROTOCOL_TCP);
                        }
                }

                if (udp != NULL) {
                        udp->dgram_cksum = 0;
                        pkt->l4_len = 8;

                        if (hw_cksum_support & SUPPORTS_UDP_CHECKSUM_OFFLOAD) {
                                udp->dgram_cksum = rte_ipv4_phdr_cksum(ip, pkt->ol_flags);
                                pkt->ol_flags |= PKT_TX_UDP_CKSUM;
                        } else {
                                /* software UDP checksumming */
                                udp->dgram_cksum = calculate_tcpudp_cksum(ip, udp, pkt->l3_len, IP_PROTOCOL_UDP);
                        }
                }

                if (hw_cksum_support & SUPPORTS_IPV4_CHECKSUM_OFFLOAD) {
                        pkt->ol_flags |= PKT_TX_IP_CKSUM;
                } else {
                        /* software IP checksumming */
                        ip->hdr_checksum = calculate_ip_cksum(ip, pkt->l3_len);
                }
        }
}
