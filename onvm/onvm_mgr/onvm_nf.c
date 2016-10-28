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
 ********************************************************************/


/******************************************************************************

                              onvm_nf.c

       This file contains all functions related to NF management.

******************************************************************************/


#include "onvm_mgr.h"
#include "onvm_nf.h"
#include "onvm_stats.h"

uint16_t next_instance_id = 0;


/************************Internal functions prototypes************************/


/*
 * Function starting a NF.
 *
 * Input  : a pointer to the NF's informations
 * Output : an error code
 *
 */
inline static int
onvm_nf_start(struct onvm_nf_info *nf_info);


/*
 * Function stopping a NF.
 *
 * Input  : a pointer to the NF's informations
 * Output : an error code
 *
 */
inline static int
onvm_nf_stop(struct onvm_nf_info *nf_info);


/********************************Interfaces***********************************/


inline int
onvm_nf_is_valid(struct client *cl) {
        return cl && cl->info && cl->info->status == NF_RUNNING;
}


uint16_t
onvm_nf_next_instance_id(void) {
        struct client *cl;
        while (next_instance_id < MAX_CLIENTS) {
                cl = &clients[next_instance_id];
                if (!onvm_nf_is_valid(cl))
                        break;
                next_instance_id++;
        }
        return next_instance_id;
}


void
onvm_nf_check_status(void) {
        int i;
        void *new_nfs[MAX_CLIENTS];
        struct onvm_nf_info *nf;
        int num_new_nfs = rte_ring_count(nf_info_queue);

        if (rte_ring_dequeue_bulk(nf_info_queue, new_nfs, num_new_nfs) != 0)
                return;

        for (i = 0; i < num_new_nfs; i++) {
                nf = (struct onvm_nf_info *) new_nfs[i];

                if (nf->status == NF_WAITING_FOR_ID) {
                        if (!onvm_nf_start(nf))
                                num_clients++;
                } else if (nf->status == NF_STOPPED) {
                        if (!onvm_nf_stop(nf))
                                num_clients--;
                }
        }
}


inline uint16_t
onvm_nf_service_to_nf_map(uint16_t service_id, struct rte_mbuf *pkt) {
        uint16_t num_nfs_available = nf_per_service_count[service_id];

        if (num_nfs_available == 0)
                return 0;

        if (pkt == NULL)
                return 0;

        uint16_t instance_index = pkt->hash.rss % num_nfs_available;
        uint16_t instance_id = services[service_id][instance_index];
        return instance_id;
}


/******************************Internal functions*****************************/


inline static int
onvm_nf_start(struct onvm_nf_info *nf_info) {
        // TODO dynamically allocate memory here - make rx/tx ring
        // take code from init_shm_rings in init.c
        // flush rx/tx queue at the this index to start clean?

        if(nf_info == NULL)
                return 1;

        // if NF passed its own id on the command line, don't assign here
        // assume user is smart enough to avoid duplicates
        uint16_t nf_id = nf_info->instance_id == (uint16_t)NF_NO_ID
                ? onvm_nf_next_instance_id()
                : nf_info->instance_id;

        if (nf_id >= MAX_CLIENTS) {
                // There are no more available IDs for this NF
                nf_info->status = NF_NO_IDS;
                return 1;
        }

        if (onvm_nf_is_valid(&clients[nf_id])) {
                // This NF is trying to declare an ID already in use
                nf_info->status = NF_ID_CONFLICT;
                return 1;
        }

        // Keep reference to this NF in the manager
        nf_info->instance_id = nf_id;
        clients[nf_id].info = nf_info;
        clients[nf_id].instance_id = nf_id;

        // Register this NF running within its service
        uint16_t service_count = nf_per_service_count[nf_info->service_id]++;
        services[nf_info->service_id][service_count] = nf_id;

        // Let the NF continue its init process
        nf_info->status = NF_STARTING;
        return 0;
}


inline static int
onvm_nf_stop(struct onvm_nf_info *nf_info) {
        uint16_t nf_id;
        uint16_t service_id;
        int mapIndex;
        struct rte_mempool *nf_info_mp;

        if(nf_info == NULL)
                return 1;

        nf_id = nf_info->instance_id;
        service_id = nf_info->service_id;

        /* Clean up dangling pointers to info struct */
        clients[nf_id].info = NULL;

        /* Reset stats */
        onvm_stats_clear_client(nf_id);

        /* Remove this NF from the service map.
         * Need to shift all elements past it in the array left to avoid gaps */
        nf_per_service_count[service_id]--;
        for (mapIndex = 0; mapIndex < MAX_CLIENTS_PER_SERVICE; mapIndex++) {
                if (services[service_id][mapIndex] == nf_id) {
                        break;
                }
        }

        if (mapIndex < MAX_CLIENTS_PER_SERVICE) {  // sanity error check
                services[service_id][mapIndex] = 0;
                for (; mapIndex < MAX_CLIENTS_PER_SERVICE - 1; mapIndex++) {
                        // Shift the NULL to the end of the array
                        if (services[service_id][mapIndex + 1] == 0) {
                                // Short circuit when we reach the end of this service's list
                                break;
                        }
                        services[service_id][mapIndex] = services[service_id][mapIndex + 1];
                        services[service_id][mapIndex + 1] = 0;
                }
        }

        /* Free info struct */
        /* Lookup mempool for nf_info struct */
        nf_info_mp = rte_mempool_lookup(_NF_MEMPOOL_NAME);
        if (nf_info_mp == NULL)
                return 1;

        rte_mempool_put(nf_info_mp, (void*)nf_info);

        return 0;
}
