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
 * onvm_sc_mgr.h - service chain functions for manager
 ********************************************************************/

#ifndef _ONVM_SC_MGR_H_
#define _ONVM_SC_MGR_H_

#include <rte_mbuf.h>
#include "onvm_common.h"

static inline uint8_t
onvm_next_action(struct onvm_service_chain* chain, uint16_t cur_nf) {
        if (unlikely(cur_nf >= chain->chain_length)) {
                return ONVM_NF_ACTION_DROP;
        }
        return chain->sc[cur_nf + 1].action;
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
        return chain->sc[cur_nf + 1].destination;
}

static inline uint16_t
onvm_sc_next_destination(struct onvm_service_chain* chain, struct rte_mbuf* pkt) {
        return onvm_next_destination(chain, onvm_get_pkt_chain_index(pkt));
}

/*get service chain*/
struct onvm_service_chain*
onvm_sc_get(void);
/*create service chain*/
struct onvm_service_chain*
onvm_sc_create(void);
#endif  // _ONVM_SC_MGR_H_
