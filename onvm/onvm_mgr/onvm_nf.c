/*********************************************************************
 *                     openNetVM
 *              https://sdnfv.github.io
 *
 *   BSD LICENSE
 *
 *   Copyright(c)
 *            2015-2019 George Washington University
 *            2015-2019 University of California Riverside
 *            2010-2019 Intel Corporation. All rights reserved.
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

#include "onvm_nf.h"
#include "onvm_mgr.h"
#include "onvm_stats.h"
#include <rte_lpm.h>

/* ID 0 is reserved */
uint16_t next_instance_id = 1;
uint16_t starting_instance_id = 1;

/************************Internal functions prototypes************************/

/*
 * Function starting a NF.
 *
 * Input  : a pointer to the NF's informations
 * Output : an error code
 *
 */
inline static int
onvm_nf_start(struct onvm_nf_init_cfg *nf_init_cfg);

/*
 * Function to mark a NF as ready.
 *
 * Input  : a pointer to the NF's informations
 * Output : an error code
 *
 */
inline static int
onvm_nf_ready(struct onvm_nf *nf);

/*
 * Function stopping a NF.
 *
 * Input  : a pointer to the NF's informations
 * Output : an error code
 *
 */
inline static int
onvm_nf_stop(struct onvm_nf *nf);

/*
 * Function that initializes an LPM object
 *
 * Input  : the address of an lpm_request struct
 * Output : a return code based on initialization of the LPM object
 *
 */
static void
onvm_nf_init_lpm_region(struct lpm_request *req_lpm);

/********************************Interfaces***********************************/

uint16_t
onvm_nf_next_instance_id(void) {
        struct onvm_nf *nf;
        uint16_t instance_id;

        if (num_nfs >= MAX_NFS)
                return MAX_NFS;

        /* Do a first pass for NF IDs bigger than current next_instance_id */
        while (next_instance_id < MAX_NFS) {
                instance_id = next_instance_id++;
                /* Check if this id is occupied by another NF */
                nf = &nfs[instance_id];
                if (!onvm_nf_is_valid(nf))
                        return instance_id;
        }

        /* Reset to starting position */
        next_instance_id = starting_instance_id;

        /* Do a second pass for other NF IDs */
        while (next_instance_id < MAX_NFS) {
                instance_id = next_instance_id++;
                /* Check if this id is occupied by another NF */
                nf = &nfs[instance_id];
                if (!onvm_nf_is_valid(nf))
                        return instance_id;
        }

        /* This should never happen, means our num_nfs counter is wrong */
        RTE_LOG(ERR, APP, "Tried to allocated a next instance ID but num_nfs is corrupted\n");
        return MAX_NFS;
}

void
onvm_nf_check_status(void) {
        int i;
        void *msgs[MAX_NFS];
        struct onvm_nf *nf;
        struct onvm_nf_msg *msg;
        struct onvm_nf_init_cfg *nf_init_cfg;
        struct lpm_request *req_lpm;
        uint16_t stop_nf_id;
        int num_msgs = rte_ring_count(incoming_msg_queue);

        if (num_msgs == 0)
                return;

        if (rte_ring_dequeue_bulk(incoming_msg_queue, msgs, num_msgs, NULL) == 0)
                return;

        for (i = 0; i < num_msgs; i++) {
                msg = (struct onvm_nf_msg *)msgs[i];

                switch (msg->msg_type) {
                        case MSG_REQUEST_LPM_REGION:
                                // TODO: Add stats event handler here
                                req_lpm = (struct lpm_request *)msg->msg_data;
                                onvm_nf_init_lpm_region(req_lpm);
                                break;
                        case MSG_NF_STARTING:
                                nf_init_cfg = (struct onvm_nf_init_cfg *)msg->msg_data;
                                if (onvm_nf_start(nf_init_cfg) == 0) {
                                        onvm_stats_gen_event_nf_info("NF Starting", &nfs[nf_init_cfg->instance_id]);
                                }
                                break;
                        case MSG_NF_READY:
                                nf = (struct onvm_nf *)msg->msg_data;
                                if (onvm_nf_ready(nf) == 0) {
                                        onvm_stats_gen_event_nf_info("NF Ready", nf);
                                }
                                break;
                        case MSG_NF_STOPPING:
                                nf = (struct onvm_nf *)msg->msg_data;
                                if (nf == NULL)
                                        break;

                                /* Saved as onvm_nf_stop frees the memory */
                                stop_nf_id = nf->instance_id;
                                if (onvm_nf_stop(nf) == 0) {
                                        onvm_stats_gen_event_info("NF Stopping", ONVM_EVENT_NF_STOP, &stop_nf_id);
                                }
                                break;
                }

                rte_mempool_put(nf_msg_pool, (void *)msg);
        }
}

int
onvm_nf_send_msg(uint16_t dest, uint8_t msg_type, void *msg_data) {
        int ret;
        struct onvm_nf_msg *msg;

        ret = rte_mempool_get(nf_msg_pool, (void **)(&msg));
        if (ret != 0) {
                RTE_LOG(INFO, APP, "Oh the huge manatee! Unable to allocate msg from pool :(\n");
                return ret;
        }

        msg->msg_type = msg_type;
        msg->msg_data = msg_data;

        return rte_ring_enqueue(nfs[dest].msg_q, (void *)msg);
}

/******************************Internal functions*****************************/

inline static int
onvm_nf_start(struct onvm_nf_init_cfg *nf_init_cfg) {
        struct onvm_nf *spawned_nf;
        uint16_t nf_id;
        int ret;
        // TODO dynamically allocate memory here - make rx/tx ring
        // take code from init_shm_rings in init.c
        // flush rx/tx queue at the this index to start clean?

        if (nf_init_cfg == NULL || nf_init_cfg->status != NF_WAITING_FOR_ID)
                return 1;

        // if NF passed its own id on the command line, don't assign here
        // assume user is smart enough to avoid duplicates
        nf_id = nf_init_cfg->instance_id == (uint16_t)NF_NO_ID ? onvm_nf_next_instance_id() : nf_init_cfg->instance_id;
        spawned_nf = &nfs[nf_id];

        if (nf_id >= MAX_NFS) {
                // There are no more available IDs for this NF
                nf_init_cfg->status = NF_NO_IDS;
                return 1;
        }

        if (nf_init_cfg->service_id >= MAX_SERVICES) {
                // Service ID must be less than MAX_SERVICES and greater than 0
                nf_init_cfg->status = NF_SERVICE_MAX;
                return 1;
        }

        if (nf_per_service_count[nf_init_cfg->service_id] >= MAX_NFS_PER_SERVICE) {
                // Maximum amount of NF's per service spawned
                nf_init_cfg->status = NF_SERVICE_COUNT_MAX;
                return 1;
        }

        if (onvm_nf_is_valid(spawned_nf)) {
                // This NF is trying to declare an ID already in use
                nf_init_cfg->status = NF_ID_CONFLICT;
                return 1;
        }

        // Keep reference to this NF in the manager
        nf_init_cfg->instance_id = nf_id;

        /* If not successful return will contain the error code */
        ret = onvm_threading_get_core(&nf_init_cfg->core, nf_init_cfg->init_options, cores);
        if (ret != 0) {
                nf_init_cfg->status = ret;
                return 1;
        }

        spawned_nf->instance_id = nf_id;
        spawned_nf->service_id = nf_init_cfg->service_id;
        spawned_nf->status = NF_STARTING;
        spawned_nf->tag = nf_init_cfg->tag;
        spawned_nf->thread_info.core = nf_init_cfg->core;
        spawned_nf->flags.time_to_live = nf_init_cfg->time_to_live;
        spawned_nf->flags.pkt_limit = nf_init_cfg->pkt_limit;
        // Let the NF continue its init process
        nf_init_cfg->status = NF_STARTING;
        return 0;
}

inline static int
onvm_nf_ready(struct onvm_nf *nf) {
        // Ensure we've already called nf_start for this NF
        if (nf->status != NF_STARTING)
                return -1;

        uint16_t service_count = nf_per_service_count[nf->service_id]++;
        services[nf->service_id][service_count] = nf->instance_id;
        num_nfs++;
        // Register this NF running within its service
        nf->status = NF_RUNNING;
        return 0;
}

inline static int
onvm_nf_stop(struct onvm_nf *nf) {
        uint16_t nf_id;
        uint16_t nf_status;
        uint16_t service_id;
        uint16_t nb_pkts, i;
        int mapIndex;
        struct onvm_nf_msg *msg;
        struct rte_mempool *nf_info_mp;
        struct rte_mbuf *pkts[PACKET_READ_SIZE];

        if (nf == NULL)
                return 1;

        nf_id = nf->instance_id;
        service_id = nf->service_id;
        nf_status = nf->status;

        /* Cleanup the allocated tag */
        if (nf->tag) {
                rte_free(nf->tag);
                nf->tag = NULL;
        }

        /* Cleanup should only happen if NF was starting or running */
        if (nf_status != NF_STARTING && nf_status != NF_RUNNING && nf_status != NF_PAUSED)
                return 1;

        nf->status = NF_STOPPED;
        nfs[nf->instance_id].status = NF_STOPPED;

        /* Tell parent we stopped running */
        if (nfs[nf_id].thread_info.parent != 0)
                rte_atomic16_dec(&nfs[nfs[nf_id].thread_info.parent].thread_info.children_cnt);

        /* Remove the NF from the core it was running on */
        cores[nf->thread_info.core].nf_count--;
        cores[nf->thread_info.core].is_dedicated_core = 0;

        /* Clean up possible left over objects in rings */
        while ((nb_pkts = rte_ring_dequeue_burst(nfs[nf_id].rx_q, (void **)pkts, PACKET_READ_SIZE, NULL)) > 0) {
                for (i = 0; i < nb_pkts; i++)
                        rte_pktmbuf_free(pkts[i]);
        }
        while ((nb_pkts = rte_ring_dequeue_burst(nfs[nf_id].tx_q, (void **)pkts, PACKET_READ_SIZE, NULL)) > 0) {
                for (i = 0; i < nb_pkts; i++)
                        rte_pktmbuf_free(pkts[i]);
        }
        nf_msg_pool = rte_mempool_lookup(_NF_MSG_POOL_NAME);
        while (rte_ring_dequeue(nfs[nf_id].msg_q, (void**)(&msg)) == 0) {
                rte_mempool_put(nf_msg_pool, (void*)msg);
        }

        /* Free info struct */
        /* Lookup mempool for nf struct */
        nf_info_mp = rte_mempool_lookup(_NF_MEMPOOL_NAME);
        if (nf_info_mp == NULL)
                return 1;

        rte_mempool_put(nf_info_mp, (void*)nf);

        /* Further cleanup is only required if NF was succesfully started */
        if (nf_status != NF_RUNNING && nf_status != NF_PAUSED)
                return 0;

        /* Decrease the total number of RUNNING NFs */
        num_nfs--;

        /* Reset stats */
        onvm_stats_clear_nf(nf_id);

        /* Remove this NF from the service map.
         * Need to shift all elements past it in the array left to avoid gaps */
        nf_per_service_count[service_id]--;
        for (mapIndex = 0; mapIndex < MAX_NFS_PER_SERVICE; mapIndex++) {
                if (services[service_id][mapIndex] == nf_id) {
                        break;
                }
        }

        if (mapIndex < MAX_NFS_PER_SERVICE) {  // sanity error check
                services[service_id][mapIndex] = 0;
                for (; mapIndex < MAX_NFS_PER_SERVICE - 1; mapIndex++) {
                        // Shift the NULL to the end of the array
                        if (services[service_id][mapIndex + 1] == 0) {
                                // Short circuit when we reach the end of this service's list
                                break;
                        }
                        services[service_id][mapIndex] = services[service_id][mapIndex + 1];
                        services[service_id][mapIndex + 1] = 0;
                }
        }

        return 0;
}

static void
onvm_nf_init_lpm_region(struct lpm_request *req_lpm) {
        struct rte_lpm_config conf;
        struct rte_lpm* lpm_region;

        conf.max_rules = req_lpm->max_num_rules;
        conf.number_tbl8s = req_lpm->num_tbl8s;

        lpm_region = rte_lpm_create(req_lpm->name, req_lpm->socket_id, &conf);
        if (lpm_region) {
                req_lpm->status = 0;
        } else {
                req_lpm->status = -1;
        }
}
