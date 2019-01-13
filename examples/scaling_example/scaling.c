/*********************************************************************
 *                     openNetVM
 *              https://sdnfv.github.io
 *
 *   BSD LICENSE
 *
 *   Copyright(c)
 *            2015-2018 George Washington University
 *            2015-2018 University of California Riverside
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
 * scaling.c - Example usage of the provided scaling functionality.
 ********************************************************************/

#include <unistd.h>
#include <stdint.h>
#include <stdio.h>
#include <inttypes.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/queue.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>
#include <signal.h>

#include <rte_common.h>
#include <rte_mbuf.h>
#include <rte_ip.h>
#include <rte_mempool.h>
#include <rte_cycles.h>
#include <rte_ring.h>
#include <rte_ethdev.h>
#include <rte_ether.h>
#include <rte_malloc.h>

#include "onvm_nflib.h"
#include "onvm_pkt_helper.h"
#include "onvm_flow_table.h"

#define NF_TAG "scaling"

#define PKTMBUF_POOL_NAME "MProc_pktmbuf_pool"
#define PKT_READ_SIZE  ((uint16_t)32)
#define LOCAL_EXPERIMENTAL_ETHER 0x88B5
#define DEFAULT_PKT_NUM 128
#define MAX_PKT_NUM NF_QUEUE_RINGSIZE

static uint16_t destination;
static uint8_t use_direct_rings = 0;
static uint8_t keep_running = 1;

static uint8_t d_addr_bytes[ETHER_ADDR_LEN];
static uint16_t packet_size = ETHER_HDR_LEN;
static uint32_t packet_number = DEFAULT_PKT_NUM;

void nf_setup(struct onvm_nf_info *nf_info);

/*
 * Print a usage message
 */
static void
usage(const char *progname) {
        printf("Usage: %s [EAL args] -- [NF_LIB args] -- -d <destination> [-a]\n\n", progname);
}

/*
 * Parse the application arguments.
 */
static int
parse_app_args(int argc, char *argv[], const char *progname) {
        int c, dst_flag = 0;

        while ((c = getopt(argc, argv, "d:p:a")) != -1) {
                switch (c) {
                case 'a':
                        use_direct_rings = 1;
                        break;
                case 'd':
                        destination = strtoul(optarg, NULL, 10);
                        dst_flag = 1;
                        break;
                case '?':
                        usage(progname);
                        if (isprint(optopt))
                                RTE_LOG(INFO, APP, "Unknown option `-%c'.\n", optopt);
                        else
                                RTE_LOG(INFO, APP, "Unknown option character `\\x%x'.\n", optopt);
                        return -1;
                default:
                        usage(progname);
                        return -1;
                }
        }

        if (!dst_flag) {
                RTE_LOG(INFO, APP, "Destination id not passed, running default example funcitonality.\n");
                return -1;
        }

        return optind;
}

/*
 * Basic packet handler, just forwards all packets
 */
static int
packet_handler_fwd(struct rte_mbuf *pkt, struct onvm_pkt_meta *meta, __attribute__((unused)) struct onvm_nf_info *nf_info) {
        (void)pkt;
        meta->destination = *(uint16_t *)nf_info->data;
        meta->action = ONVM_NF_ACTION_TONF;

        return 0;
}

/*
 * Child packet handler showcasing that children can also spawn NFs on their own
 */
static int
packet_handler_child(struct rte_mbuf *pkt, struct onvm_pkt_meta *meta, __attribute__((unused)) struct onvm_nf_info *nf_info) {
        (void)pkt;
        static int ret = 0;
        static int spawned = 0;
        meta->destination = *(uint16_t *)nf_info->data;
        meta->action = ONVM_NF_ACTION_TONF;

        /* Spawn as many children as possible */
        while (ret == 0 && spawned < 2) {
                struct onvm_nf_scale_info *scale_info = onvm_nflib_get_empty_scaling_config(nf_info);
                uint16_t *state_data = rte_malloc("nf_state_data", sizeof(uint16_t), 0);
                *state_data = nf_info->service_id; 
                /* Sets service id of child */
                scale_info->service_id = destination;
                /* Run the setup function to generate packets */
                scale_info->setup_func = &nf_setup;
                /* Custom packet handler */
                scale_info->pkt_func = &packet_handler_fwd;
                /* Insert state data, will be used to forward packets to itself */
                scale_info->data = state_data;

                /* Spawn the child */
                ret= onvm_nflib_scale(scale_info);
                if (ret == 0)
                        RTE_LOG(INFO, APP, "Spawning child SID %u; with packet_handler_fwd packet function\n", scale_info->service_id);
                spawned++;
        }

        return 0;
}

/*
 * Main packet handler
 */
static int
packet_handler(struct rte_mbuf *pkt, struct onvm_pkt_meta *meta, __attribute__((unused)) struct onvm_nf_info *nf_info) {
        (void)pkt;
        static uint32_t spawned = 0;
        static int ret;
        struct onvm_nf_scale_info *scale_info;
        void *data;

        /* Testing NF scaling, Spawns one child */ 
        if (spawned == 0) {
                spawned = 1;

                /* Prepare state data for the child */
                data = (void *)rte_malloc("nf_state_data", sizeof(uint16_t), 0);
                *(uint16_t *)data = destination;
                /* Get the filled in scale struct by inheriting parent properties */
                scale_info = onvm_nflib_inherit_parent_config(nf_info, data);
                scale_info->service_id = destination;
                scale_info->pkt_func = &packet_handler_child;

                /* Spawn the child */
                ret = onvm_nflib_scale(scale_info);
                if (ret == 0)
                        RTE_LOG(INFO, APP, "Spawning child SID %u; with packet_handler_child packet function\n", scale_info->service_id);
                else
                        rte_exit(EXIT_FAILURE, "Can't initialize the first child! Make sure the corelist has at least 2 cores\n");

        }

        meta->destination = nf_info->service_id;
        meta->action = ONVM_NF_ACTION_TONF;

        return 0;
}


static void
handle_signal(int sig) {
        if (sig == SIGINT || sig == SIGTERM)
                keep_running = 0;
}

static void
run_advanced_rings(struct onvm_nf_info *nf_info) {
        void *pkts[PKT_READ_SIZE];
        struct onvm_pkt_meta* meta;
        uint16_t i, j, nb_pkts;
        void *pktsTX[PKT_READ_SIZE];
        int tx_batch_size;
        struct rte_ring *rx_ring;
        struct rte_ring *tx_ring;
        volatile struct onvm_nf *nf;
        static uint8_t spawned_nfs = 0;

        /* Listen for ^C and docker stop so we can exit gracefully */
        signal(SIGINT, handle_signal);
        signal(SIGTERM, handle_signal);

        printf("Process %d handling packets using advanced rings\n", nf_info->instance_id);
        printf("[Press Ctrl-C to quit ...]\n");

        /* Get rings from nflib */
        nf = onvm_nflib_get_nf(nf_info->instance_id);
        rx_ring = nf->rx_q;
        tx_ring = nf->tx_q;

        /* Testing NF scaling */ 
        if (spawned_nfs == 0) {
                /* As this is advanced rings if we want the children to inheir the same function we need to set it first */
                nf->nf_advanced_rings_function = &run_advanced_rings;
                struct onvm_nf_scale_info *scale_info;
                /* Spawn as many children as possible */
                do {
                        spawned_nfs++;
                        if (spawned_nfs>2)
                                break;
                        /* Prepare state data for the child */
                        void *data = (void *)rte_malloc("nf_specific_data", sizeof(uint16_t), 0);
                        *(uint16_t *)data = destination;
                        /* Get the filled in scale struct by inheriting parent properties */
                        scale_info = onvm_nflib_inherit_parent_config(nf_info, data);
                        RTE_LOG(INFO, APP, "Tring to spawn child SID %u; running advanced_rings\n", scale_info->service_id);
                } while(onvm_nflib_scale(scale_info)==0);
        }


        while (keep_running && rx_ring && tx_ring && nf) {
                tx_batch_size = 0;
                /* Dequeue all packets in ring up to max possible. */
                nb_pkts = rte_ring_dequeue_burst(rx_ring, pkts, PKT_READ_SIZE, NULL);

                if (unlikely(nb_pkts == 0)) {
                        continue;
                }
                /* Process all the packets */
                for (i = 0; i < nb_pkts; i++) {
                        meta = onvm_get_pkt_meta((struct rte_mbuf*)pkts[i]);
                        packet_handler_fwd((struct rte_mbuf*)pkts[i], meta, nf_info);
                        pktsTX[tx_batch_size++] = pkts[i];
                }

                if (unlikely(tx_batch_size > 0 && rte_ring_enqueue_bulk(tx_ring, pktsTX, tx_batch_size, NULL) == 0)) {
                        nf->stats.tx_drop += tx_batch_size;
                        for (j = 0; j < tx_batch_size; j++) {
                                rte_pktmbuf_free(pktsTX[j]);
                        }
                } else {
                        nf->stats.tx += tx_batch_size;
                }
        }
        /* Waiting for spawned NFs to exit */
        for (i = 0; i < MAX_NFS; i++) {
                struct onvm_nf *nf_cur = onvm_nflib_get_nf(i);
                while(nf_cur && nf_cur->parent == nf->instance_id && nf_cur->info != NULL) {
                        sleep(1);
                }
        }
        onvm_nflib_stop(nf_info);
}


/*
 * Generates fake packets or loads them from a pcap file
 */
void
nf_setup(__attribute__((unused)) struct onvm_nf_info *nf_info) {
        uint32_t i;
        struct rte_mempool *pktmbuf_pool;

        pktmbuf_pool = rte_mempool_lookup(PKTMBUF_POOL_NAME);
        if (pktmbuf_pool == NULL) {
                onvm_nflib_stop(nf_info);
                rte_exit(EXIT_FAILURE, "Cannot find mbuf pool!\n");
        }

        for (i = 0; i < packet_number; ++i) {
                struct onvm_pkt_meta* pmeta;
                struct ether_hdr *ehdr;
                int j;

                struct rte_mbuf *pkt = rte_pktmbuf_alloc(pktmbuf_pool);

                /* set up ether header and set new packet size */
                ehdr = (struct ether_hdr *) rte_pktmbuf_append(pkt, packet_size);

                /* Using manager mac addr for source*/
                rte_eth_macaddr_get(0, &ehdr->s_addr);
                for (j = 0; j < ETHER_ADDR_LEN; ++j) {
                        ehdr->d_addr.addr_bytes[j] = d_addr_bytes[j];
                }
                ehdr->ether_type = LOCAL_EXPERIMENTAL_ETHER;

                pmeta = onvm_get_pkt_meta(pkt);
                pmeta->destination = *(uint16_t *)nf_info->data;
                pmeta->action = ONVM_NF_ACTION_TONF;
                pkt->hash.rss = i;
                pkt->port = 0;

                onvm_nflib_return_pkt(nf_info, pkt);
        }
}

int main(int argc, char *argv[]) {
        int arg_offset;
        const char *progname = argv[0];
        struct onvm_nf_info *nf_info;

        if ((arg_offset = onvm_nflib_init(argc, argv, NF_TAG, &nf_info)) < 0)
                return -1;

        argc -= arg_offset;
        argv += arg_offset;

        if (parse_app_args(argc, argv, progname) < 0) {
                onvm_nflib_stop(nf_info);
                rte_exit(EXIT_FAILURE, "Invalid command-line arguments\n");
        }

        /* Set the function to execute before running the NF
         * For advanced rings manually run the function */
        onvm_nflib_set_setup_function(nf_info, &nf_setup);

        nf_info->data = (void *)rte_malloc("nf_specific_data", sizeof(uint16_t), 0);
        *(uint16_t*)nf_info->data = nf_info->service_id;

        if (use_direct_rings) {
                printf("\nRUNNING ADVANCED RINGS EXPERIMENT\n");
                onvm_nflib_nf_ready(nf_info);
                nf_setup(nf_info);
                run_advanced_rings(nf_info);
        } else {
                printf("\nRUNNING PACKET_HANDLER EXPERIMENT\n");
                onvm_nflib_run(nf_info, &packet_handler);
        }
        printf("If we reach here, program is ending\n");
        return 0;
}
