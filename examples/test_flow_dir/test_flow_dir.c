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
 *  test_flow_dir.c - an example using onvm_flow_dir_* APIs.
 ********************************************************************/

#include <assert.h>
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
#include <rte_malloc.h>
#include <rte_mbuf.h>
#include <rte_memory.h>
#include <rte_memzone.h>

#include "onvm_flow_dir.h"
#include "onvm_flow_table.h"
#include "onvm_nflib.h"
#include "onvm_pkt_helper.h"
#include "onvm_sc_common.h"
#include "onvm_sc_mgr.h"

#define NF_TAG "test_flow_dir"

/* Number of packets between each print. */
static uint32_t print_delay = 1000000;

static uint32_t destination;

/* Populate flow disabled by default */
static int populate_flow = 0;

static struct onvm_flow_entry *flow_entry = NULL;

/* Pointer to the manager flow table */
extern struct onvm_ft* sdn_ft;
/*
 * Print a usage message
 */
static void
usage(const char *progname) {
        printf("Usage:\n");
        printf("%s [EAL args] -- [NF_LIB args] -- -d <destination> -p <print_delay>\n", progname);
        printf("%s -F <CONFIG_FILE.json> [EAL args] -- [NF_LIB args] -- [NF args]\n\n", progname);
        printf("Flags:\n");
        printf(" - `-d <dst>`: destination service ID to foward to\n");
        printf(" - `-p <print_delay>`: number of packets between each print, e.g. `-p 1` prints every packets.\n");
        printf(" -  -s : Prepopulate sample flow table rules. \n");

}

/*
 * Parse the application arguments.
 */
static int
parse_app_args(int argc, char *argv[], const char *progname) {
        int c;

        while ((c = getopt(argc, argv, "d:p:s")) != -1) {
                switch (c) {
                        case 'd':
                                destination = strtoul(optarg, NULL, 10);
                                break;
                        case 'p':
                                print_delay = strtoul(optarg, NULL, 10);
                                break;
                        case 's':
                                populate_flow = 1;
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
        struct rte_ipv4_hdr *ip;

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

/*
 * This function populates a set of flows to the sdn flow table.
 * For each new flow a service chain is created and appended.
 * Each service chain is of max 4 length. This function is not enabled by default.
 * Users may update the predefined struct with their own rules here.
 */

static void
populate_sample_ipv4(void) {
        struct test_add_key {
                struct onvm_ft_ipv4_5tuple key;
                uint8_t destinations[ONVM_MAX_CHAIN_LENGTH];
        };

        struct test_add_key keys[] = {
                {{RTE_IPV4(100, 10, 0, 0), RTE_IPV4(100, 10, 0, 0),  1, 1, IPPROTO_TCP}, {2,3}},
                {{RTE_IPV4(102, 10, 0, 0), RTE_IPV4(101, 10, 0, 1),  0, 1, IPPROTO_TCP}, {4,3,2,1}},
                {{RTE_IPV4(103, 10, 0, 0), RTE_IPV4(102, 10, 0, 1),  0, 1, IPPROTO_TCP}, {2,1}},
                {{RTE_IPV4(10, 11, 1, 17), RTE_IPV4(10, 11, 1, 17),  1234, 1234, IPPROTO_UDP}, {4,3,2}},
        };

        uint32_t num_keys = RTE_DIM(keys);
        for (uint32_t i = 0; i < num_keys; i++) {
                struct onvm_ft_ipv4_5tuple key;
                key = keys[i].key;
                int ret = onvm_ft_lookup_key(sdn_ft, &key, (char **)&flow_entry);
                if (ret >= 0) {
                        continue;
                } else {
                        printf("\nAdding Key: ");
                        _onvm_ft_print_key(&key);
                        ret = onvm_ft_add_key(sdn_ft, &key, (char **)&flow_entry);
                        if (ret < 0) {
                                printf("Unable to add key.");
                                continue;
                        }
                        printf("Creating new service chain.\n");
                        flow_entry->sc = onvm_sc_create();
                        uint32_t num_dest = RTE_DIM(keys[i].destinations);
                        for (uint32_t j = 0; j < num_dest; j++) {
                                if (keys[i].destinations[j] == 0) {
                                        continue;
                                }
                                printf("Appending Destination: %d \n", keys[i].destinations[j]);
                                onvm_sc_append_entry(flow_entry->sc, ONVM_NF_ACTION_TONF, keys[i].destinations[j]);
                        }
                }
        }
}

static void
nf_setup(__attribute__((unused)) struct onvm_nf_local_ctx *nf_local_ctx) {
        flow_entry = (struct onvm_flow_entry *) rte_calloc(NULL, 1, sizeof(struct onvm_flow_entry), 0);
        if (flow_entry == NULL) {
                rte_exit(EXIT_FAILURE, "Unable to allocate flow entry\n");
        }
}

static int
packet_handler(struct rte_mbuf *pkt, struct onvm_pkt_meta *meta,
               __attribute__((unused)) struct onvm_nf_local_ctx *nf_local_ctx) {
        static uint32_t counter = 0;
        int ret;

        if (!onvm_pkt_is_ipv4(pkt)) {
                meta->action = ONVM_NF_ACTION_DROP;
                return 0;
        }

        if (++counter == print_delay) {
                do_stats_display(pkt);
                counter = 0;
        }

        struct onvm_ft_ipv4_5tuple key;
        onvm_ft_fill_key(&key, pkt);
        ret = onvm_ft_lookup_key(sdn_ft, &key, (char **)&flow_entry);
        if (ret >= 0) {
                meta->action = ONVM_NF_ACTION_NEXT;
        } else {
                ret = onvm_ft_add_key(sdn_ft, &key, (char **)&flow_entry);
                if (ret < 0) {
                        meta->action = ONVM_NF_ACTION_DROP;
                        meta->destination = 0;
                        return 0;
                }
                flow_entry->sc = onvm_sc_create();
                onvm_sc_append_entry(flow_entry->sc, ONVM_NF_ACTION_TONF, destination);
                meta->action = ONVM_NF_ACTION_TONF;
                meta->destination = destination;
        }
        return 0;
}

int
main(int argc, char *argv[]) {
        int arg_offset;
        struct onvm_nf_local_ctx *nf_local_ctx;
        struct onvm_nf_function_table *nf_function_table;
        const char *progname = argv[0];

        nf_local_ctx = onvm_nflib_init_nf_local_ctx();
        onvm_nflib_start_signal_handler(nf_local_ctx, NULL);

        nf_function_table = onvm_nflib_init_nf_function_table();
        nf_function_table->pkt_handler = &packet_handler;
        nf_function_table->setup = &nf_setup;

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
        destination = nf_local_ctx->nf->service_id + 1;

        if (parse_app_args(argc, argv, progname) < 0)
                rte_exit(EXIT_FAILURE, "Invalid command-line arguments\n");

        /* Map the sdn_ft table */
        onvm_flow_dir_nf_init();

        if (populate_flow) {
                printf("Populating flow table. \n");
                populate_sample_ipv4();
        }

        onvm_nflib_run(nf_local_ctx);

        onvm_nflib_stop(nf_local_ctx);
        rte_free(flow_entry);
        flow_entry = NULL;
        printf("If we reach here, program is ending\n");
        return 0;
}
