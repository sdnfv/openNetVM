/*********************************************************************
 *                     openNetVM
 *              https://sdnfv.github.io
 *
 *   BSD LICENSE
 *
 *   Copyright(c)
 *            2020 National Institute of Technology Karnataka, Surathkal
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
 * forward_tb.c - an example using onvm. Simulates a queue with a Token Bucket
 * and forwards packets to a DST NF.
 ********************************************************************/

#include <errno.h>
#include <getopt.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/queue.h>
#include <unistd.h>

#include <rte_common.h>
#include <rte_ip.h>
#include <rte_mbuf.h>
#include <rte_cycles.h>

#include "onvm_nflib.h"
#include "onvm_pkt_helper.h"

#define NF_TAG "simple_fwd_tb"

#define PKT_READ_SIZE ((uint16_t) 32)

/* number of packets between each print */
static uint32_t print_delay = 1000000;

static uint32_t destination;

/* Token Bucket */
static uint64_t tb_rate = 1000; // rate at which tokens are generated (in Mbps)
static uint64_t tb_depth = 10000; // depth of the token bucket (in bytes)
static uint64_t tb_tokens = 10000; // number of the tokens in the bucket at any given time (in bytes)

static uint64_t last_cycle;
static uint64_t cur_cycles;

/* For advanced rings scaling */
rte_atomic16_t signal_exit_flag;

void sig_handler(int sig);

/*
 * Print a usage message
 */
static void
usage(const char *progname) {
        printf("Usage:\n");
        printf("%s [EAL args] -- [NF_LIB args] -- -d <destination> -p <print_delay> -D <tb_depth> -R <tb_rate>\n", progname);
        printf("%s -F <CONFIG_FILE.json> [EAL args] -- [NF_LIB args] -- [NF args]\n\n", progname);
        printf("Flags:\n");
        printf(" - `-d <dst>`: destination service ID to foward to\n");
        printf(" - `-p <print_delay>`: number of packets between each print, e.g. `-p 1` prints every packets.\n");
        printf(" - `-D <tb_depth>`: depth of token bucket (in bytes)\n");
        printf(" - `-R <tb_rate>`: rate of token regeneration (in Mbps) \n");
}

/*
 * Parse the application arguments.
 */
static int
parse_app_args(int argc, char *argv[], const char *progname) {

        int c, dst_flag = 0;

        while ((c = getopt(argc, argv, "d:p:D:R:")) != -1) {
                switch (c) {
                        case 'd':
                                destination = strtoul(optarg, NULL, 10);
                                dst_flag = 1;
                                break;
                        case 'p':
                                print_delay = strtoul(optarg, NULL, 10);
                                break;
                        case 'D':
                                tb_depth = strtoul(optarg, NULL, 10);
                                tb_tokens = tb_depth;
                                break;
                        case 'R':
                                tb_rate = strtoul(optarg, NULL, 10);
                                break;
                        case '?':
                                usage(progname);
                                if (optopt == 'd')
                                        RTE_LOG(INFO, APP, "Option -%c requires an argument.\n", optopt);
                                else if (optopt == 'p')
                                        RTE_LOG(INFO, APP, "Option -%c requires an argument.\n", optopt);
                                else if (isprint(optopt))
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
                RTE_LOG(INFO, APP, "Simple Forward with Token Bucket NF requires destination flag -d.\n");
                return -1;
        }

        return optind;
}

/*
 * This function displays stats. It uses ANSI terminal codes to clear
 * screen when called. It is called from a single non-master
 * thread in the server process, when the process is run with more
 * than one lcore enabled.
 */
static void
do_stats_display(struct rte_mbuf *pkt) {
        const char clr[] = {27, '[', '2', 'J', '\0'};
        const char topLeft[] = {27, '[', '1', ';', '1', 'H', '\0'};
        static uint64_t pkt_process = 0;
        struct ipv4_hdr *ip;

        pkt_process += print_delay;

        /* Clear screen and move to top left */
        printf("%s%s", clr, topLeft);

        printf("PACKETS\n");
        printf("-----\n");
        printf("Port : %d\n", pkt->port);
        printf("Size : %d\n", pkt->pkt_len);
        printf("N°   : %" PRIu64 "\n", pkt_process);
        printf("\n\n");

        ip = onvm_pkt_ipv4_hdr(pkt);
        if (ip != NULL) {
                onvm_pkt_print(pkt);
        } else {
                printf("No IP4 header found\n");
        }
}

static int
packet_handler_tb(struct rte_mbuf *pkt, struct onvm_pkt_meta *meta,
               __attribute__((unused)) struct onvm_nf_local_ctx *nf_local_ctx) {
        static uint32_t counter = 0;
        uint64_t tokens_produced;
        
        /* Generate tokens only if `tb_tokens` is insufficient */
        if (tb_tokens < pkt->pkt_len) {
                cur_cycles = rte_get_tsc_cycles();
                /* Wait until sufficient tokens are available */
                while ((((cur_cycles - last_cycle) * tb_rate * 1000000) / rte_get_timer_hz()) + tb_tokens < pkt->pkt_len) {
                        cur_cycles = rte_get_tsc_cycles();
                }
                tokens_produced = (((cur_cycles - last_cycle) * tb_rate * 1000000) / rte_get_timer_hz());
                /* Update tokens to a max of tb_depth */
                if (tokens_produced + tb_tokens > tb_depth) {
                        tb_tokens = tb_depth;
                } else {
                        tb_tokens += tokens_produced;
                }

                last_cycle = cur_cycles;
        }

        if (++counter == print_delay) {
                do_stats_display(pkt);
                counter = 0;
        }

        tb_tokens -= pkt->pkt_len;

        meta->action = ONVM_NF_ACTION_TONF;
        meta->destination = destination;
        return 0;
}

void sig_handler(int sig) {
        if (sig != SIGINT && sig != SIGTERM)
                return;

        /* Will stop the processing for all spawned threads in advanced rings mode */
        rte_atomic16_set(&signal_exit_flag, 1);
}

static int
thread_main_loop(struct onvm_nf_local_ctx *nf_local_ctx) {
        void *pkts[PKT_READ_SIZE];
        struct onvm_pkt_meta *meta;
        uint16_t i, nb_pkts;
        void *pktsTX[PKT_READ_SIZE];
        int tx_batch_size;
        struct rte_ring *rx_ring;
        struct rte_ring *tx_ring;
        struct rte_ring *msg_q;
        struct onvm_nf *nf;
        struct onvm_nf_msg *msg;
        struct rte_mempool *nf_msg_pool;

        nf = nf_local_ctx->nf;

        onvm_nflib_nf_ready(nf);

        /* Get rings from nflib */
        rx_ring = nf->rx_q;
        tx_ring = nf->tx_q;
        msg_q = nf->msg_q;
        nf_msg_pool = rte_mempool_lookup(_NF_MSG_POOL_NAME);

        printf("Process %d handling packets using advanced rings\n", nf->instance_id);
        if (onvm_threading_core_affinitize(nf->thread_info.core) < 0)
                rte_exit(EXIT_FAILURE, "Failed to affinitize to core %d\n", nf->thread_info.core);

        last_cycle = rte_get_tsc_cycles();

        while (!rte_atomic16_read(&signal_exit_flag)) {
                /* Check for a stop message from the manager */
                if (unlikely(rte_ring_count(msg_q) > 0)) {
                        msg = NULL;
                        rte_ring_dequeue(msg_q, (void **)(&msg));
                        if (msg->msg_type == MSG_STOP) {
                                rte_atomic16_set(&signal_exit_flag, 1);
                        } else {
                                printf("Received message %d, ignoring", msg->msg_type);
                        }
                        rte_mempool_put(nf_msg_pool, (void *)msg);
                }

                tx_batch_size = 0;
                nb_pkts = rte_ring_dequeue_burst(rx_ring, pkts, PKT_READ_SIZE, NULL);

                /* Process all the dequeued packets */
                for (i = 0; i < nb_pkts; i++) {
                        meta = onvm_get_pkt_meta((struct rte_mbuf *)pkts[i]);
                        packet_handler_tb((struct rte_mbuf *)pkts[i], meta, nf_local_ctx);
                        pktsTX[tx_batch_size++] = pkts[i];
                }

                rte_ring_enqueue_bulk(tx_ring, pktsTX, tx_batch_size, NULL);
        }
        return 0;
}

int
main(int argc, char *argv[]) {
        struct onvm_nf_local_ctx *nf_local_ctx;
        struct onvm_nf_function_table *nf_function_table;
        int arg_offset;

        const char *progname = argv[0];

        nf_local_ctx = onvm_nflib_init_nf_local_ctx();
        /* If we're using advanced rings also pass a custom cleanup function,
         * this can be used to handle NF specific (non onvm) cleanup logic */
        rte_atomic16_init(&signal_exit_flag);
        rte_atomic16_set(&signal_exit_flag, 0);
        onvm_nflib_start_signal_handler(nf_local_ctx, sig_handler);
        /* No need to define a function table as adv rings won't run onvm_nflib_run */
        nf_function_table = NULL;

        if ((arg_offset = onvm_nflib_init(argc, argv, NF_TAG, nf_local_ctx, nf_function_table)) < 0) {
                onvm_nflib_stop(nf_local_ctx);
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
                onvm_nflib_stop(nf_local_ctx);
                rte_exit(EXIT_FAILURE, "Invalid command-line arguments\n");
        }

        thread_main_loop(nf_local_ctx);
        onvm_nflib_stop(nf_local_ctx);     

        printf("If we reach here, program is ending\n");
        return 0;
}
