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

                                onvm_vxlan.h

    Header file that contains functions for vxlan encapsulation/decapsulation

******************************************************************************/

#ifndef ONVM_VXLAN_H_
#define ONVM_VXLAN_H_

#include <rte_mbuf.h>
#include <rte_ether.h>
#include <rte_ip.h>

#define RTE_LOGTYPE_VXLAN RTE_LOGTYPE_USER2

#define IPV4_HEADER_LEN 20
#define UDP_HEADER_LEN  8
#define VXLAN_HEADER_LEN 8

#define IP_VERSION 0x40
#define IP_HDRLEN  0x05 /* default IP header length == five 32-bits words. */
#define IP_DEFTTL  64   /* from RFC 1340. */
#define IP_VHL_DEF (IP_VERSION | IP_HDRLEN)
#define IP_DN_FRAGMENT_FLAG 0x0040
#define VXLAN_HF_VNI 0x08000000
#define DEFAULT_VXLAN_PORT 4789

// Dummy IP addresses to use in vxlan encapsulation
// Our switches are L2 switches, so they won't look at this
#define VXLAN_SRC_IP {10, 1, 2, 3}
#define VXLAN_DST_IP {10, 4, 5, 6}

/* structure that caches offload info for the current packet */
union tunnel_offload_info {
        uint64_t data;
        struct {
                uint64_t l2_len:7; /**< L2 (MAC) Header Length. */
                uint64_t l3_len:9; /**< L3 (IP) Header Length. */
                uint64_t l4_len:8; /**< L4 Header Length. */
                uint64_t tso_segsz:16; /**< TCP TSO segment size */
                uint64_t outer_l2_len:7; /**< outer L2 Header Length */
                uint64_t outer_l3_len:16; /**< outer L3 Header Length */
        };
} __rte_cache_aligned;

int onvm_decapsulate_pkt(struct rte_mbuf *pkt);

void onvm_encapsulate_pkt(struct rte_mbuf *pkt, struct ether_addr *src_addr, struct ether_addr *dst_addr);

#endif
