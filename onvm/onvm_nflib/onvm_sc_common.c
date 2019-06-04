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
 * onvm_sc_common.c - service functions for manager and NFs
 ********************************************************************/

#include "onvm_sc_common.h"
#include <errno.h>
#include <inttypes.h>
#include "onvm_common.h"

/*********************************Interfaces**********************************/

uint16_t
onvm_sc_service_to_nf_map(uint16_t service_id, struct rte_mbuf *pkt) {
        if (!nf_per_service_count || !services) {
                rte_exit(EXIT_FAILURE, "Failed to retrieve service information\n");
        }
        uint16_t num_nfs_available = nf_per_service_count[service_id];

        if (num_nfs_available == 0)
                return 0;

        if (pkt == NULL)
                return 0;

        uint16_t instance_index = pkt->hash.rss % num_nfs_available;
        uint16_t instance_id = services[service_id][instance_index];
        return instance_id;
}

int
onvm_sc_append_entry(struct onvm_service_chain *chain, uint8_t action, uint16_t destination) {
        int chain_length = chain->chain_length;

        if (unlikely(chain_length > ONVM_MAX_CHAIN_LENGTH)) {
                return ENOSPC;
        }
        /*the first entry is reserved*/
        chain_length++;
        (chain->chain_length)++;
        chain->sc[chain_length].action = action;
        chain->sc[chain_length].destination = destination;

        return 0;
}

int
onvm_sc_set_entry(struct onvm_service_chain *chain, int entry, uint8_t action, uint16_t destination) {
        if (unlikely(entry > chain->chain_length)) {
                return -1;
        }

        chain->sc[entry].action = action;
        chain->sc[entry].destination = destination;
        return 0;
}

void
onvm_sc_print(struct onvm_service_chain *chain) {
        int i;
        for (i = 1; i <= chain->chain_length; i++) {
                printf("cur_index:%d, action:%" PRIu8 ", destination:%" PRIu16 "\n", i, chain->sc[i].action,
                       chain->sc[i].destination);
        }
        printf("\n");
}
