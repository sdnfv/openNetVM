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
 * flow_table.c - a simple flow table NF
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
#include <rte_ether.h>
#include <rte_hash.h>
#include <rte_ip.h>
#include <rte_lcore.h>
#include <rte_mbuf.h>
#include <rte_memory.h>
#include <rte_memzone.h>
#include <rte_ring.h>
#include <rte_tcp.h>

#include "flow_table.h"
#include "onvm_flow_dir.h"
#include "onvm_flow_table.h"
#include "onvm_nflib.h"
#include "onvm_pkt_helper.h"
#include "onvm_sc_common.h"
#include "sdn.h"

#define NF_TAG "flow_table"

/* Struct that contains information about this NF */
struct onvm_nf_local_ctx *global_termination_context;

struct onvm_nf *nf;

struct rte_ring *ring_to_sdn;
struct rte_ring *ring_from_sdn;
#define SDN_RING_SIZE 65536

/* number of package between each print */
static uint32_t print_delay = 10000;

uint16_t def_destination;
static uint32_t total_flows;

/* Setup rings to hold buffered packets destined for SDN controller */
static void
setup_rings(void) {
        /* Remove old ring buffers */
        ring_to_sdn = rte_ring_lookup("ring_to_sdn");
        ring_from_sdn = rte_ring_lookup("ring_from_sdn");
        if (ring_to_sdn) {
                rte_ring_free(ring_to_sdn);
        }
        if (ring_from_sdn) {
                rte_ring_free(ring_from_sdn);
        }

        ring_to_sdn = rte_ring_create("ring_to_sdn", SDN_RING_SIZE, rte_socket_id(), RING_F_SP_ENQ | RING_F_SC_DEQ);
        ring_from_sdn = rte_ring_create("ring_from_sdn", SDN_RING_SIZE, rte_socket_id(), RING_F_SP_ENQ | RING_F_SC_DEQ);
        if (ring_to_sdn == NULL || ring_from_sdn == NULL) {
                onvm_nflib_stop(global_termination_context);
                rte_exit(EXIT_FAILURE, "Unable to create SDN rings\n");
        }
}

/* Clear out rings on exit. Requires DPDK v2.2.0+ */
static void
cleanup(void) {
        printf("Freeing memory for SDN rings.\n");
        rte_ring_free(ring_to_sdn);
        rte_ring_free(ring_from_sdn);
        printf("Freeing memory for hash table.\n");
        rte_hash_free(sdn_ft->hash);
}

static int
parse_app_args(int argc, char *argv[]) {
        const char *progname = argv[0];
        int c;

        opterr = 0;

        while ((c = getopt(argc, argv, "p:")) != -1)
                switch (c) {
                        case 'p':
                                print_delay = strtoul(optarg, NULL, 10);
                                break;
                        case '?':
                                usage(progname);
                                if (optopt == 'p')
                                        fprintf(stderr, "Option -%c requires an argument.\n", optopt);
                                else if (isprint(optopt))
                                        fprintf(stderr, "Unknown option `-%c'.\n", optopt);
                                else
                                        fprintf(stderr, "Unknown option character `\\x%x'.\n", optopt);
                                return -1;
                        default:
                                return -1;
                }
        return optind;
}

static void
do_stats_display(struct rte_mbuf *pkt, int32_t tbl_index) {
        const char clr[] = {27, '[', '2', 'J', '\0'};
        const char topLeft[] = {27, '[', '1', ';', '1', 'H', '\0'};
        static uint64_t total_pkts = 0;
        /* Fix unused variable warnings: */
        (void)pkt;

        struct onvm_flow_entry *flow_entry = (struct onvm_flow_entry *)onvm_ft_get_data(sdn_ft, tbl_index);
        total_pkts += print_delay;

        /* Clear screen and move to top left */
        printf("%s%s", clr, topLeft);

        printf("FLOW TABLE NF\n");
        printf("-----\n");
        printf("Total pkts   : %" PRIu64 "\n", total_pkts);
        printf("Total flows  : %d\n", total_flows);
        printf("Flow ID      : %d\n", tbl_index);
        printf("Flow pkts    : %" PRIu64 "\n", flow_entry->packet_count);
        // printf("Flow Action  : %d\n", flow_entry->action);
        // printf("Flow Dest    : %d\n", flow_entry->destination);
        printf("\n\n");

#ifdef DEBUG_PRINT
        struct ipv4_hdr *ip;
        ip = onvm_pkt_ipv4_hdr(pkt);
        if (ip != NULL) {
                onvm_pkt_print(pkt);
        } else {
                printf("Not an IP4 packet\n");
        }
#endif
}

static int
flow_table_hit(struct rte_mbuf *pkt, struct onvm_pkt_meta *meta) {
        (void)pkt;
        meta->chain_index = 0;
        meta->action = ONVM_NF_ACTION_NEXT;
        meta->destination = 0;

        return 0;
}

static int
flow_table_miss(struct rte_mbuf *pkt, struct onvm_pkt_meta *meta) {
        int ret;

        /* Buffer new flows until we get response from SDN controller. */
        ret = rte_ring_enqueue(ring_to_sdn, pkt);
        if (ret != 0) {
                printf("ERROR enqueing to SDN ring\n");
                meta->action = ONVM_NF_ACTION_DROP;
                meta->destination = 0;
                return 0;
        }

        return 1;
}

static int
packet_handler(struct rte_mbuf *pkt, struct onvm_pkt_meta *meta,
               __attribute__((unused)) struct onvm_nf_local_ctx *nf_local_ctx) {
        static uint32_t counter = 0;

        int32_t tbl_index;
        int action;
        struct onvm_flow_entry *flow_entry;

        if (!onvm_pkt_is_ipv4(pkt)) {
                printf("Non-ipv4 packet\n");
                meta->action = ONVM_NF_ACTION_TONF;
                meta->destination = def_destination;
                return 0;
        }

        tbl_index = onvm_flow_dir_get_pkt(pkt, &flow_entry);
        if (tbl_index >= 0) {
#ifdef DEBUG_PRINT
                printf("Found existing flow %d\n", tbl_index);
#endif
                /* Existing flow */
                action = flow_table_hit(pkt, meta);
        } else if (tbl_index == -ENOENT) {
#ifdef DEBUG_PRINT
                printf("Unkown flow\n");
#endif
                /* New flow */
                action = flow_table_miss(pkt, meta);
        } else {
#ifdef DEBUG_PRINT
                printf("Error in flow lookup: %d (ENOENT=%d, EINVAL=%d)\n", tbl_index, ENOENT, EINVAL);
                onvm_pkt_print(pkt);
#endif
                onvm_nflib_stop(global_termination_context);
                rte_exit(EXIT_FAILURE, "Error in flow lookup\n");
        }

        if (++counter == print_delay && print_delay != 0) {
                if (tbl_index >= 0) {
                        do_stats_display(pkt, tbl_index);
                        counter = 0;
                }
        }

        return action;
}

int
main(int argc, char *argv[]) {
        int arg_offset;
        struct onvm_nf_local_ctx *nf_local_ctx;
        struct onvm_nf_function_table *nf_function_table;
        unsigned sdn_core = 0;

        nf_local_ctx = onvm_nflib_init_nf_local_ctx();
        global_termination_context = nf_local_ctx;
        onvm_nflib_start_signal_handler(nf_local_ctx, NULL);

        nf_function_table = onvm_nflib_init_nf_function_table();
        nf_function_table->pkt_handler = &packet_handler;

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
        if (parse_app_args(argc, argv) < 0) {
                onvm_nflib_stop(nf_local_ctx);
                rte_exit(EXIT_FAILURE, "Invalid command-line arguments\n");
        }
        printf("Flow table running on %d\n", rte_lcore_id());

        nf = nf_local_ctx->nf;
        def_destination = nf_local_ctx->nf->service_id + 1;
        printf("Setting up hash table with default destination: %d\n", def_destination);
        total_flows = 0;

        /* Setup the SDN connection thread */
        printf("Setting up SDN rings and thread.\n");
        setup_rings();
        sdn_core = rte_lcore_id();
        sdn_core = rte_get_next_lcore(sdn_core, 1, 1);
        rte_eal_remote_launch(setup_securechannel, NULL, sdn_core);

        /* Map sdn_ft table */
        onvm_flow_dir_nf_init();
        printf("Starting packet handler.\n");
        onvm_nflib_run(nf_local_ctx);
        
        onvm_nflib_stop(nf_local_ctx);
        printf("NF exiting...\n");
        cleanup();
        return 0;
}
