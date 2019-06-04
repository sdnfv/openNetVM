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
 * onvm_flow_dir.h - flow director APIs
 ********************************************************************/

#ifndef _ONVM_FLOW_DIR_H_
#define _ONVM_FLOW_DIR_H_

#include "onvm_common.h"
#include "onvm_flow_table.h"

extern struct onvm_ft* sdn_ft;
extern struct onvm_ft** sdn_ft_p;

struct onvm_flow_entry {
        struct onvm_ft_ipv4_5tuple* key;
        struct onvm_service_chain* sc;
        uint64_t ref_cnt;
        uint16_t idle_timeout;
        uint16_t hard_timeout;
        uint64_t packet_count;
        uint64_t byte_count;
};

/* Get a pointer to the flow entry entry for this packet.
 * Returns:
 *  0        on success. *flow_entry points to this packet flow's flow entry
 *  -ENOENT  if flow has not been added to table. *flow_entry points to flow entry
 */
int
onvm_flow_dir_init(void);
int
onvm_flow_dir_nf_init(void);
int
onvm_flow_dir_get_pkt(struct rte_mbuf* pkt, struct onvm_flow_entry** flow_entry);
int
onvm_flow_dir_add_pkt(struct rte_mbuf* pkt, struct onvm_flow_entry** flow_entry);
/* delete the flow dir entry, but do not free the service chain (useful if a service chain is pointed to by several
 * different flows */
int
onvm_flow_dir_del_pkt(struct rte_mbuf* pkt);
/* Delete the flow dir entry and free the service chain */
int
onvm_flow_dir_del_and_free_pkt(struct rte_mbuf* pkt);
int
onvm_flow_dir_get_key(struct onvm_ft_ipv4_5tuple* key, struct onvm_flow_entry** flow_entry);
int
onvm_flow_dir_add_key(struct onvm_ft_ipv4_5tuple* key, struct onvm_flow_entry** flow_entry);
int
onvm_flow_dir_del_key(struct onvm_ft_ipv4_5tuple* key);
int
onvm_flow_dir_del_and_free_key(struct onvm_ft_ipv4_5tuple* key);
#endif  // _ONVM_FLOW_DIR_H_
