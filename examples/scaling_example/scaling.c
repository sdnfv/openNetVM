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

#include <errno.h>
#include <getopt.h>
#include <inttypes.h>
#include <signal.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/queue.h>
#include <unistd.h>

#include <rte_common.h>
#include <rte_cycles.h>
#include <rte_ethdev.h>
#include <rte_ether.h>
#include <rte_ip.h>
#include <rte_malloc.h>
#include <rte_mbuf.h>
#include <rte_mempool.h>
#include <rte_ring.h>

#include "onvm_flow_table.h"
#include "onvm_nflib.h"
#include "onvm_pkt_helper.h"

#define NF_TAG "scaling"

#define PKTMBUF_POOL_NAME "MProc_pktmbuf_pool"
#define PKT_READ_SIZE ((uint16_t)32)
#define LOCAL_EXPERIMENTAL_ETHER 0x88B5
#define DEFAULT_PKT_NUM 128
#define MAX_PKT_NUM NF_QUEUE_RINGSIZE
#define DEFAULT_NUM_CHILDREN 1

static uint16_t destination;
static uint16_t num_children = DEFAULT_NUM_CHILDREN;
static uint8_t use_direct_rings = 0;
static uint8_t use_shared_cpu_core_allocation = 0;

static uint8_t d_addr_bytes[ETHER_ADDR_LEN];
static uint16_t packet_size = ETHER_HDR_LEN;
static uint32_t packet_number = DEFAULT_PKT_NUM;

uint8_t ONVM_ENABLE_SHARED_CPU;

void
nf_setup(struct onvm_nf_context *nf_context);

void sig_handler(int sig);

void sig_handler(int sig) {
        if (sig != SIGINT && sig != SIGTERM)
                return;

        /* Specific signal handling logic can be implemented here */
}

/*
 * Print a usage message
 */
static void
usage(const char *progname) {
        printf("Usage:\n");
        printf("%s [EAL args] -- [NF_LIB args] -- -d <destination> [-a]\n", progname);
        printf("%s -F <CONFIG_FILE.json> [EAL args] -- [NF_LIB args] -- [NF args]\n\n", progname);
        printf("Flags:\n");
        printf(" - `-d DST`: Destination Service ID, functionality depends on mode\n");
        printf(" - `-n NUM_CHILDREN`: Sets the number of children for the NF to spawn\n");
        printf(" - `-c`: Set NF core allocation to shared cpu\n");
        printf(" - `-a`: Use advanced rings interface instead of default `packet_handler`\n");
}

/*
 * Parse the application arguments.
 */
static int
parse_app_args(int argc, char *argv[], const char *progname) {
        int c, dst_flag = 0;

        while ((c = getopt(argc, argv, "d:n:p:ac")) != -1) {
                switch (c) {
                        case 'c':
                                use_shared_cpu_core_allocation = 1;
                                break;
                        case 'a':
                                use_direct_rings = 1;
                                break;
                        case 'd':
                                destination = strtoul(optarg, NULL, 10);
                                dst_flag = 1;
                                break;
                        case 'n':
                                num_children = strtoul(optarg, NULL, 10);
                                if (num_children < DEFAULT_NUM_CHILDREN) {
                                        printf("The number of children should be more or equal than %d\n",
                                               DEFAULT_NUM_CHILDREN);
                                        return -1;
                                }
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
packet_handler_fwd(struct rte_mbuf *pkt, struct onvm_pkt_meta *meta,
                   __attribute__((unused)) struct onvm_nf *nf) {
        (void)pkt;
        meta->destination = *(uint16_t *)nf->data;
        meta->action = ONVM_NF_ACTION_TONF;

        return 0;
}

/*
 * Child packet handler showcasing that children can also spawn NFs on their own
 */
static int
packet_handler_child(struct rte_mbuf *pkt, struct onvm_pkt_meta *meta,
                     __attribute__((unused)) struct onvm_nf *nf) {
        (void)pkt;
        /* As this is already a child, 1 NF has been spawned */
        static int spawned_nfs = 1;
        meta->destination = *(uint16_t *)nf->data;
        meta->action = ONVM_NF_ACTION_TONF;

        /* Spawn children until we hit the set number */
        while (spawned_nfs < num_children) {
                struct onvm_nf_scale_info *scale_info = onvm_nflib_get_empty_scaling_config(nf);
                uint16_t *state_data = rte_malloc("nf_state_data", sizeof(uint16_t), 0);
                *state_data = nf->service_id;
                /* Sets service id of child */
                scale_info->service_id = destination;
                /* Run the setup function to generate packets */
                scale_info->setup_func = &nf_setup;
                if (use_shared_cpu_core_allocation)
                        scale_info->init_options = ONVM_SET_BIT(0, SHARE_CORE_BIT);
                /* Custom packet handler */
                scale_info->pkt_func = &packet_handler_fwd;
                /* Insert state data, will be used to forward packets to itself */
                scale_info->data = state_data;

                /* Spawn the child */
                if (onvm_nflib_scale(scale_info) == 0)
                        RTE_LOG(INFO, APP, "Spawning child SID %u; with packet_handler_fwd packet function\n",
                                scale_info->service_id);
                else
                        rte_exit(EXIT_FAILURE, "Can't spawn child\n");
                spawned_nfs++;
        }

        return 0;
}

/*
 * Main packet handler
 */
static int
packet_handler(struct rte_mbuf *pkt, struct onvm_pkt_meta *meta, __attribute__((unused)) struct onvm_nf *nf) {
        (void)pkt;
        static uint32_t spawned_child = 0;
        struct onvm_nf_scale_info *scale_info;
        void *data;

        /* Testing NF scaling, Spawns one child */
        if (spawned_child == 0) {
                spawned_child = 1;

                /* Prepare state data for the child */
                data = (void *)rte_malloc("nf_state_data", sizeof(uint16_t), 0);
                *(uint16_t *)data = destination;
                /* Get the filled in scale struct by inheriting parent properties */
                scale_info = onvm_nflib_inherit_parent_config(nf, data);
                scale_info->service_id = destination;
                scale_info->pkt_func = &packet_handler_child;
                if (use_shared_cpu_core_allocation)
                        scale_info->init_options = ONVM_SET_BIT(0, SHARE_CORE_BIT);
                /* Spawn the child */
                if (onvm_nflib_scale(scale_info) == 0)
                        RTE_LOG(INFO, APP, "Spawning child SID %u; with packet_handler_child packet function\n",
                                scale_info->service_id);
                else
                        rte_exit(EXIT_FAILURE, "Can't initialize the first child!\n");
        }

        meta->destination = nf->service_id;
        meta->action = ONVM_NF_ACTION_TONF;

        return 0;
}

static void
run_advanced_rings(struct onvm_nf_context *nf_context) {
        void *pkts[PKT_READ_SIZE];
        struct onvm_pkt_meta *meta;
        uint16_t i, j, nb_pkts;
        void *pktsTX[PKT_READ_SIZE];
        int tx_batch_size;
        struct rte_ring *rx_ring;
        struct rte_ring *tx_ring;
        struct rte_ring *msg_q;
        struct onvm_nf *nf;
        struct onvm_nf_msg *msg;
        struct rte_mempool *nf_msg_pool;
        static uint8_t spawned_nfs = 0;

        /* Get rings from nflib */
        nf = onvm_nflib_get_nf(nf_context->nf->instance_id);
        rx_ring = nf->rx_q;
        tx_ring = nf->tx_q;
        msg_q = nf->msg_q;
        nf_msg_pool = rte_mempool_lookup(_NF_MSG_POOL_NAME);

        printf("Process %d handling packets using advanced rings\n", nf->instance_id);
        /* Set core affinity, as this is adv rings we do it on our own */
        if (onvm_threading_core_affinitize(nf->thread_info.core) < 0)
                rte_exit(EXIT_FAILURE, "Failed to affinitize to core %d\n", nf->thread_info.core);

        /* Testing NF scaling */
        if (spawned_nfs == 0) {
                /* As this is advanced rings if we want the children to inherit the same function we need to set it
                 * first */
                nf->functions.adv_rings = &run_advanced_rings;
                struct onvm_nf_scale_info *scale_info;

                /* Spawn children until we hit the set number */
                while (spawned_nfs < num_children) {
                        /* Prepare state data for the child */
                        void *data = (void *)rte_malloc("nf_specific_data", sizeof(uint16_t), 0);
                        *(uint16_t *)data = destination;
                        /* Get the filled in scale struct by inheriting parent properties */
                        scale_info = onvm_nflib_inherit_parent_config(nf, data);
                        if (use_shared_cpu_core_allocation)
                                scale_info->init_options = ONVM_SET_BIT(0, SHARE_CORE_BIT);

                        RTE_LOG(INFO, APP, "NF %d trying to spawn child SID %u; running advanced_rings\n",
                                nf->instance_id, scale_info->service_id);
                        if (onvm_nflib_scale(scale_info) == 0)
                                RTE_LOG(INFO, APP, "Spawning child SID %u\n", scale_info->service_id);
                        else
                                rte_exit(EXIT_FAILURE, "Can't initialize the child!\n");
                        spawned_nfs++;
                }
        }

        while (rte_atomic16_read(&nf_context->keep_running) && rx_ring && tx_ring && nf) {
                /* Check for a stop message from the manager. */
                if (unlikely(rte_ring_count(msg_q) > 0)) {
                        msg = NULL;
                        rte_ring_dequeue(msg_q, (void **)(&msg));
                        if (msg->msg_type == MSG_STOP) {
                                rte_atomic16_set(&nf_context->keep_running, 0);
                        } else {
                                printf("Received message %d, ignoring", msg->msg_type);
                        }
                        rte_mempool_put(nf_msg_pool, (void *)msg);
                }

                tx_batch_size = 0;
                /* Dequeue all packets in ring up to max possible. */
                nb_pkts = rte_ring_dequeue_burst(rx_ring, pkts, PKT_READ_SIZE, NULL);

                if (unlikely(nb_pkts == 0)) {
                        if (ONVM_ENABLE_SHARED_CPU) {
                                rte_atomic16_set(nf->shared_core.sleep_state, 1);
                                sem_wait(nf->shared_core.nf_mutex);
                        }
                        continue;
                }
                /* Process all the packets */
                for (i = 0; i < nb_pkts; i++) {
                        meta = onvm_get_pkt_meta((struct rte_mbuf *)pkts[i]);
                        packet_handler_fwd((struct rte_mbuf *)pkts[i], meta, nf);
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
}

/*
 * Generates fake packets or loads them from a pcap file
 */
void
nf_setup(__attribute__((unused)) struct onvm_nf_context *nf_context) {
        uint32_t i;
        struct rte_mempool *pktmbuf_pool;

        pktmbuf_pool = rte_mempool_lookup(PKTMBUF_POOL_NAME);
        if (pktmbuf_pool == NULL) {
                onvm_nflib_stop(nf_context);
                rte_exit(EXIT_FAILURE, "Cannot find mbuf pool!\n");
        }

        for (i = 0; i < packet_number; ++i) {
                struct onvm_pkt_meta *pmeta;
                struct ether_hdr *ehdr;
                int j;

                struct rte_mbuf *pkt = rte_pktmbuf_alloc(pktmbuf_pool);
                if (pkt == NULL)
                        break;

                /* set up ether header and set new packet size */
                ehdr = (struct ether_hdr *)rte_pktmbuf_append(pkt, packet_size);

                /* Using manager mac addr for source*/
                rte_eth_macaddr_get(0, &ehdr->s_addr);
                for (j = 0; j < ETHER_ADDR_LEN; ++j) {
                        ehdr->d_addr.addr_bytes[j] = d_addr_bytes[j];
                }
                ehdr->ether_type = LOCAL_EXPERIMENTAL_ETHER;

                pmeta = onvm_get_pkt_meta(pkt);
                pmeta->destination = *(uint16_t *)nf_context->nf->data;
                pmeta->action = ONVM_NF_ACTION_TONF;
                pkt->hash.rss = i;
                pkt->port = 0;

                onvm_nflib_return_pkt(nf_context->nf, pkt);
        }
}

int
main(int argc, char *argv[]) {
        int arg_offset;
        const char *progname = argv[0];
        struct onvm_nf *nf;
        struct onvm_configuration *onvm_config;
        struct onvm_nf_context *nf_context;
        int i;

        nf_context = onvm_nflib_init_nf_context();

        /* Hack to know if we're using advanced rings before running getopts */
        for (i = argc - 1; i > 0; i--) {
                if (strcmp(argv[i], "-a") == 0)
                        use_direct_rings = 1;
                else if (strcmp(argv[i],"--") == 0)
                        break;
        }

        /*
         * If we're using direct rings also pass a custom cleanup function,
         * this can be used to handle NF specific (non onvm) cleanup logic
         */
        if (use_direct_rings) {
                onvm_nflib_start_signal_handler(nf_context, sig_handler);
        } else {
                onvm_nflib_start_signal_handler(nf_context, NULL);
        }

        if ((arg_offset = onvm_nflib_init(argc, argv, NF_TAG, nf_context)) < 0) {
                onvm_nflib_stop(nf_context);
                if (arg_offset == ONVM_SIGNAL_TERMINATION) {
                        printf("Exiting due to user termination\n");
                        return 0;
                } else {
                        rte_exit(EXIT_FAILURE, "Failed ONVM init\n");
                }
        }

        argc -= arg_offset;
        argv += arg_offset;

        if (parse_app_args(argc, argv, progname) < 0) {
                onvm_nflib_stop(nf_context);
                rte_exit(EXIT_FAILURE, "Invalid command-line arguments\n");
        }

        nf = nf_context->nf;

        /* Set the function to execute before running the NF
         * For advanced rings manually run the function */
        onvm_nflib_set_setup_function(nf, &nf_setup);

        nf->data = (void *)rte_malloc("nf_specific_data", sizeof(uint16_t), 0);
        *(uint16_t *)nf->data = nf->service_id;

        if (use_direct_rings) {
                printf("\nRUNNING ADVANCED RINGS EXPERIMENT\n");
                onvm_config = onvm_nflib_get_onvm_config();
                ONVM_ENABLE_SHARED_CPU = onvm_config->flags.ONVM_ENABLE_SHARED_CPU;
                onvm_nflib_nf_ready(nf);
                nf_setup(nf_context);
                run_advanced_rings(nf_context);
        } else {
                printf("\nRUNNING PACKET_HANDLER EXPERIMENT\n");
                onvm_nflib_run(nf_context, &packet_handler);
        }
        onvm_nflib_stop(nf_context);
        printf("If we reach here, program is ending\n");
        return 0;
}
