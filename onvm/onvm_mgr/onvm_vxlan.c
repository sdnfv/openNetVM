/*********************************************************************
 *                     openNetVM
 *              https://sdnfv.github.io
 *
 *   BSD LICENSE
 *
 *   Copyright(c)
 *            2015-2016 George Washington University
 *            2015-2016 University of California Riverside
 *            2010-2014 Intel Corporation. All rights reserved.
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
 *     * Neither the name of Intel Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
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
 ********************************************************************/


/******************************************************************************

                                onvm_vxlan.c

    Header file that contains functions for vxlan encapsulation/decapsulation

                                Based on:
https://github.com/sdnfv/onvm-dpdk/blob/master/examples/tep_termination/vxlan.c


******************************************************************************/

#include <rte_mbuf.h>
#include <rte_ether.h>
#include <rte_ip.h>
#include <rte_hash_crc.h>
#include <rte_byteorder.h>
#include <rte_udp.h>
#include <rte_tcp.h>
#include <rte_sctp.h>

#include "onvm_vxlan.h"
#include "onvm_common.h"
#include "onvm_pkt_helper.h"

static uint64_t process_inner_cksums(struct ether_hdr *eth_hdr, union tunnel_offload_info *info);
static uint16_t get_psd_sum(void *l3_hdr, uint16_t ethertype, uint64_t ol_flags);

void
onvm_encapsulate_pkt(struct rte_mbuf *pkt, struct ether_addr *src_addr, struct ether_addr *dst_addr)
{
        uint64_t ol_flags = 0;
        uint32_t old_len = pkt->pkt_len;
        union tunnel_offload_info tx_offload = { .data = 0 };
        struct ether_hdr *phdr = rte_pktmbuf_mtod(pkt, struct ether_hdr *);
        struct onvm_pkt_meta *old_meta = onvm_get_pkt_meta(pkt);
        const uint8_t src_ip[4] = VXLAN_SRC_IP;
        const uint8_t dst_ip[4] = VXLAN_DST_IP;

        /* Allocate space for new ethernet, IPv4, UDP and VXLAN headers */
        size_t new_data_len = sizeof(struct ether_hdr) + sizeof(struct ipv4_hdr)
                            + sizeof(struct udp_hdr) + sizeof(struct vxlan_hdr)
                            + sizeof(struct onvm_pkt_meta);
        struct ether_hdr *pneth = (struct ether_hdr *) rte_pktmbuf_prepend(pkt, new_data_len);

        struct ipv4_hdr *ip = (struct ipv4_hdr *) &pneth[1];
        struct udp_hdr *udp = (struct udp_hdr *) &ip[1];
        struct vxlan_hdr *vxlan = (struct vxlan_hdr *) &udp[1];
        struct onvm_pkt_meta *dst_meta = (struct onvm_pkt_meta *) &vxlan[1];
        int i;

        /* set up outer Ethernet header*/
        pneth->s_addr = *src_addr;
        pneth->d_addr = *dst_addr;
        pneth->ether_type = rte_cpu_to_be_16(ETHER_TYPE_IPv4);

        /* set up outer IP header
         * since our switches are L2 switches, this really doesn't matter,
         * save that we can recognize it on the receiving side */
        ip->version_ihl = IP_VHL_DEF;
        ip->type_of_service = 0;
        ip->total_length = 0;
        ip->packet_id = 0;
        ip->fragment_offset = IP_DN_FRAGMENT_FLAG;
        ip->time_to_live = IP_DEFTTL;
        ip->next_proto_id = IPPROTO_UDP;
        ip->hdr_checksum = 0;

        ip->src_addr = 0;
        ip->dst_addr = 0;
        for (i = 0; i < 4; i++) {
                ip->src_addr |= src_ip[i] << (8 * i);
                ip->dst_addr |= dst_ip[i] << (8 * i);
        }
        ip->total_length = rte_cpu_to_be_16(pkt->data_len - sizeof(struct ether_hdr));

        /* outer IP checksum */
        ol_flags |= PKT_TX_OUTER_IP_CKSUM;
        ip->hdr_checksum = 0;

        /* inner IP checksum offload */
        ol_flags |= process_inner_cksums(phdr, &tx_offload);
        pkt->l2_len = tx_offload.l2_len;
        pkt->l3_len = tx_offload.l3_len;
        pkt->l4_len = tx_offload.l4_len;
        pkt->l2_len += ETHER_VXLAN_HLEN;

        pkt->outer_l2_len = sizeof(struct ether_hdr);
        pkt->outer_l3_len = sizeof(struct ipv4_hdr);

        pkt->ol_flags |= ol_flags;
        pkt->tso_segsz = tx_offload.tso_segsz;

        /*VXLAN HEADER*/
        vxlan->vx_flags = rte_cpu_to_be_32(VXLAN_HF_VNI);
        vxlan->vx_vni = rte_cpu_to_be_32(0);

        /*UDP HEADER*/
        udp->dgram_cksum = 0;
        udp->dgram_len = rte_cpu_to_be_16(old_len
                                + sizeof(struct udp_hdr)
                                + sizeof(struct vxlan_hdr));

        udp->dst_port = rte_cpu_to_be_16(DEFAULT_VXLAN_PORT);
        udp->src_port = rte_cpu_to_be_16(DEFAULT_VXLAN_PORT);

        /* Copy onvm_pkt_meta data into the packet data */
        dst_meta->action = old_meta->action;
        dst_meta->destination = rte_cpu_to_be_16(old_meta->destination);
        dst_meta->src = rte_cpu_to_be_16(old_meta->src);
        dst_meta->chain_index = old_meta->chain_index;

        return;
}

int
onvm_decapsulate_pkt(struct rte_mbuf *pkt)
{
        size_t outer_header_len;
        uint16_t dst_port;
        struct udp_hdr *udp_hdr;
        struct onvm_pkt_meta *pkt_meta;
        struct onvm_pkt_meta *dst_meta;

        if (!onvm_pkt_is_udp(pkt))
                return -1;

        udp_hdr = onvm_pkt_udp_hdr(pkt);
        dst_port = rte_be_to_cpu_16(udp_hdr->dst_port);

        /** check udp destination port, 4789 is the default vxlan port
         * (rfc7348) or that the rx offload flag is set (i40e only
         * currently)*/
        if (dst_port != DEFAULT_VXLAN_PORT &&
                (pkt->packet_type & RTE_PTYPE_TUNNEL_MASK) == 0)
                return -1;
        outer_header_len = sizeof(struct ether_hdr) + sizeof(struct ipv4_hdr)
                            + sizeof(struct udp_hdr) + sizeof(struct vxlan_hdr);

        rte_pktmbuf_adj(pkt, outer_header_len);

        /* Now, the head of the packet data points to the onvm_pkt_meta */
        pkt_meta = rte_pktmbuf_mtod(pkt, struct onvm_pkt_meta *);
        dst_meta = onvm_get_pkt_meta(pkt);

        dst_meta->action = pkt_meta->action;
        dst_meta->destination = rte_be_to_cpu_16(pkt_meta->destination);
        dst_meta->src = rte_be_to_cpu_16(pkt_meta->src);
        dst_meta->chain_index = pkt_meta->chain_index;

        rte_pktmbuf_adj(pkt, sizeof(struct onvm_pkt_meta));

        return 0;
}

/**
 * Calculate the checksum of a packet in hardware
 */
static uint64_t
process_inner_cksums(struct ether_hdr *eth_hdr, union tunnel_offload_info *info)
{
        void *l3_hdr = NULL;
        uint8_t l4_proto;
        uint16_t ethertype;
        struct ipv4_hdr *ipv4_hdr;
        struct ipv6_hdr *ipv6_hdr;
        struct udp_hdr *udp_hdr;
        struct tcp_hdr *tcp_hdr;
        struct sctp_hdr *sctp_hdr;
        uint64_t ol_flags = 0;

        info->l2_len = sizeof(struct ether_hdr);
        ethertype = rte_be_to_cpu_16(eth_hdr->ether_type);

        if (ethertype == ETHER_TYPE_VLAN) {
                struct vlan_hdr *vlan_hdr = (struct vlan_hdr *)(eth_hdr + 1);
                info->l2_len  += sizeof(struct vlan_hdr);
                ethertype = rte_be_to_cpu_16(vlan_hdr->eth_proto);
        }

        l3_hdr = (char *)eth_hdr + info->l2_len;

        if (ethertype == ETHER_TYPE_IPv4) {
                ipv4_hdr = (struct ipv4_hdr *)l3_hdr;
                ipv4_hdr->hdr_checksum = 0;
                ol_flags |= PKT_TX_IPV4;
                ol_flags |= PKT_TX_IP_CKSUM;
                info->l3_len = sizeof(struct ipv4_hdr);
                l4_proto = ipv4_hdr->next_proto_id;
        } else if (ethertype == ETHER_TYPE_IPv6) {
                ipv6_hdr = (struct ipv6_hdr *)l3_hdr;
                info->l3_len = sizeof(struct ipv6_hdr);
                l4_proto = ipv6_hdr->proto;
                ol_flags |= PKT_TX_IPV6;
        } else
                return 0; /* packet type not supported, nothing to do */

        if (l4_proto == IPPROTO_UDP) {
                udp_hdr = (struct udp_hdr *)((char *)l3_hdr + info->l3_len);
                ol_flags |= PKT_TX_UDP_CKSUM;
                udp_hdr->dgram_cksum = get_psd_sum(l3_hdr,
                                ethertype, ol_flags);
        } else if (l4_proto == IPPROTO_TCP) {
                tcp_hdr = (struct tcp_hdr *)((char *)l3_hdr + info->l3_len);
                ol_flags |= PKT_TX_TCP_CKSUM;
                tcp_hdr->cksum = get_psd_sum(l3_hdr, ethertype,
                                ol_flags);

        } else if (l4_proto == IPPROTO_SCTP) {
                sctp_hdr = (struct sctp_hdr *)((char *)l3_hdr + info->l3_len);
                sctp_hdr->cksum = 0;
                ol_flags |= PKT_TX_SCTP_CKSUM;
        }

        return ol_flags;
}

static uint16_t
get_psd_sum(void *l3_hdr, uint16_t ethertype, uint64_t ol_flags)
{
        if (ethertype == ETHER_TYPE_IPv4)
                return rte_ipv4_phdr_cksum(l3_hdr, ol_flags);
        else /* assume ethertype == ETHER_TYPE_IPv6 */
                return rte_ipv6_phdr_cksum(l3_hdr, ol_flags);
}
