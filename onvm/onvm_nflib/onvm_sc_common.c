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
 * onvm_sc_common.c - service functions for manager and NFs
 ********************************************************************/

#include <inttypes.h>
#include <errno.h>
#include "onvm_sc_common.h"

int onvm_sc_append_entry(struct onvm_service_chain *chain, uint8_t action,
                uint16_t destination) {
#if 0
        int chain_length = chain->chain_length;
#else
        uint8_t chain_length = chain->chain_length;
        if (unlikely(chain_length > ONVM_MAX_CHAIN_LENGTH)) {
                return ENOSPC;
        }
        /*the first entry is reserved*/
        chain_length++;
        (chain->chain_length)++;
        chain->sc[chain_length].action = action;
        chain->sc[chain_length].service = destination;
        chain->sc[chain_length].destination = destination;
        //onvm_sc_print(chain);
#endif
        return 0;
}

//Use this API to MAP the resolved InstanceID
int onvm_sc_set_entry(struct onvm_service_chain *chain, int entry,
                uint8_t action, uint16_t destination, uint8_t service) {
        if (unlikely(entry > chain->chain_length)) {
                return -1;
        }

        chain->sc[entry].action = action;
        if(service) {
                chain->sc[entry].service = service;
        }/* else if(chain->sc[entry].destination) {
                chain->sc[entry].service = (uint8_t)chain->sc[entry].destination;
        }*/
        chain->sc[entry].destination = destination;

        //onvm_sc_print(chain);
        return 0;
}

void onvm_sc_print(struct onvm_service_chain *chain) {
        int i;
        for (i = 1; i <= chain->chain_length; i++) {
                printf("cur_index:%d, action:%"PRIu8", destination:%"PRIu16"\n",
                                i, chain->sc[i].action,
                                chain->sc[i].destination);
        }
        //printf("refcnt:%"PRIu8", downstream:%"PRIu16"\n",chain->ref_cnt, chain->highest_downstream_nf_index_id);
        printf("\n");
}
