/*********************************************************************
 *                     openNetVM
 *              https://sdnfv.github.io
 *
 *   BSD LICENSE
 *
 *   Copyright(c)
 *            2015-2020 George Washington University
 *            2015-2020 University of California Riverside
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
 * l3switch.h - This application performs L3 forwarding.
 ********************************************************************/

#include "onvm_flow_table.h"

#ifndef __L3_SWITCH_H_
#define __L3_SWICTH_H_

/* Hash parameters. */
#ifdef RTE_ARCH_64
/* default to 4 million hash entries (approx) */
#define L3FWD_HASH_ENTRIES              (1024*1024*4)
#else
/* 32-bit has less address-space for hugepage memory, limit to 1M entries */
#define L3FWD_HASH_ENTRIES              (1024*1024*1)
#endif

#define HASH_ENTRY_NUMBER_DEFAULT       4
#define NB_SOCKETS        8

/*Struct that holds all NF state information */
struct state_info {
        struct lpm_request *l3switch_req;
        struct rte_lpm *lpm_tbl;
        struct onvm_ft *em_tbl;
        struct rte_ether_addr ports_eth_addr[RTE_MAX_ETHPORTS];
        uint64_t port_statistics[RTE_MAX_ETHPORTS];
        xmm_t val_eth[RTE_MAX_ETHPORTS];
        uint64_t dest_eth_addr[RTE_MAX_ETHPORTS];
        uint64_t packets_dropped;
        uint32_t print_delay;
        uint32_t hash_entry_number;
        int8_t l3fwd_lpm_on;
        int8_t l3fwd_em_on;
};

/* Function pointers for LPM or EM functionality. */

int
setup_lpm(struct state_info *stats);

int
setup_hash(struct state_info *stats);

uint16_t
lpm_get_ipv4_dst_port(void *ipv4_hdr, uint16_t portid, struct state_info *stats);

uint16_t
em_get_ipv4_dst_port(struct rte_mbuf *pkt, struct state_info *stats);

int
get_initialized_ports(uint8_t if_out);

#endif // __L3_SWICTH_H_
