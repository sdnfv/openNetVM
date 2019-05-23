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
 * onvm_sc_mgr.h - service chain functions for manager
 ********************************************************************/

#ifndef _SC_MGR_H_
#define _SC_MGR_H_

#include <rte_mbuf.h>
#include "onvm_common.h"


typedef struct active_sc_entries {
        uint32_t sc_count;
        struct onvm_service_chain *sc[SDN_FT_ENTRIES];
} __rte_cache_aligned active_sc_entries_t;
extern active_sc_entries_t active_sc_list;

static inline uint8_t
onvm_next_action(struct onvm_service_chain* chain, uint16_t cur_nf) {
	if (unlikely(cur_nf >= chain->chain_length)) {
		return ONVM_NF_ACTION_DROP;
	}
	return chain->sc[cur_nf+1].action;
}

static inline uint8_t
onvm_sc_next_action(struct onvm_service_chain* chain, struct rte_mbuf* pkt) {
	return onvm_next_action(chain, onvm_get_pkt_chain_index(pkt));
}

static inline uint16_t
onvm_next_destination(struct onvm_service_chain* chain, uint16_t cur_nf) {
	if (unlikely(cur_nf >= chain->chain_length)) {
		return 0;
	}
	return chain->sc[cur_nf+1].destination;
}

static inline uint16_t
onvm_sc_next_destination(struct onvm_service_chain* chain, struct rte_mbuf* pkt) {
	return onvm_next_destination(chain, onvm_get_pkt_chain_index(pkt));
}

static inline int
onvm_sc_get_next_action_and_destionation(struct onvm_service_chain* chain,
        struct rte_mbuf* pkt, uint16_t* next_act, uint16_t* next_dst ) {
        uint16_t index = onvm_get_pkt_chain_index(pkt);
        if (unlikely(index >= chain->chain_length)) {
                *next_act = ONVM_NF_ACTION_DROP;
                return -1;
        }
        *next_act = chain->sc[index].action;
        *next_dst = chain->sc[index].destination;

        return index;
}

/*get service chain*/
struct onvm_service_chain* onvm_sc_get(void);
/*create service chain*/
struct onvm_service_chain* onvm_sc_create(void);
/* Get List of active service chains */
const active_sc_entries_t* onvm_sc_get_all_active_chains(void);
#endif  // _SC_MGR_H_
