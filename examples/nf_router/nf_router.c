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
 * nf_router.c - route packets based on the provided config.
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

#include <rte_arp.h>
#include <rte_common.h>
#include <rte_ip.h>
#include <rte_malloc.h>
#include <rte_mbuf.h>

#include "onvm_nflib.h"
#include "onvm_pkt_helper.h"

#define NF_TAG "router"

/* router information */
uint8_t nf_count;
char *cfg_filename;
struct forward_nf *fwd_nf;

struct forward_nf {
        uint32_t ip;
        uint8_t dest;
};

/* number of package between each print */
static uint32_t print_delay = 1000000;

/*
 * Print a usage message
 */
static void
usage(const char *progname) {
        printf("Usage:\n");
        printf("%s [EAL args] -- [NF_LIB args] -- <router_config> -p <print_delay>\n", progname);
        printf("%s -F <CONFIG_FILE.json> [EAL args] -- [NF_LIB args] -- [NF args]\n\n", progname);
        printf("Flags:\n");
        printf(" - `-f <router_cfg>`: router configuration, has a list of (IPs, dest) tuples \n");
        printf(" - `-p <print_delay>`: number of packets between each print, e.g. `-p 1` prints every packets.\n");
}

/*
 * Parse the application arguments.
 */
static int
parse_app_args(int argc, char *argv[], const char *progname) {
        int c = 0;

        while ((c = getopt(argc, argv, "f:p:")) != -1) {
                switch (c) {
                        case 'f':
                                cfg_filename = strdup(optarg);
                                break;
                        case 'p':
                                print_delay = strtoul(optarg, NULL, 10);
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
 * This function parses the forward config. It takes the filename
 * and fills up the forward nf array. This includes the ip and dest
 * address of the onvm_nf
 */
static int
parse_router_config(void) {
        int ret, temp, i;
        char ip[32];
        FILE *cfg;

        cfg = fopen(cfg_filename, "r");
        if (cfg == NULL) {
                rte_exit(EXIT_FAILURE, "Error openning server \'%s\' config\n", cfg_filename);
        }
        ret = fscanf(cfg, "%*s %d", &temp);
        if (temp <= 0) {
                rte_exit(EXIT_FAILURE, "Error parsing config, need at least one forward NF configuration\n");
        }
        nf_count = temp;

        fwd_nf = (struct forward_nf *)rte_malloc("router fwd_nf info", sizeof(struct forward_nf) * nf_count, 0);
        if (fwd_nf == NULL) {
                rte_exit(EXIT_FAILURE, "Malloc failed, can't allocate forward_nf array\n");
        }

        for (i = 0; i < nf_count; i++) {
                ret = fscanf(cfg, "%s %d", ip, &temp);
                if (ret != 2) {
                        rte_exit(EXIT_FAILURE, "Invalid backend config structure\n");
                }

                ret = onvm_pkt_parse_ip(ip, &fwd_nf[i].ip);
                if (ret < 0) {
                        rte_exit(EXIT_FAILURE, "Error parsing config IP address #%d\n", i);
                }

                if (temp < 0) {
                        rte_exit(EXIT_FAILURE, "Error parsing config dest #%d\n", i);
                }
                fwd_nf[i].dest = temp;
        }

        fclose(cfg);
        printf("\nDest config (%d):\n", nf_count);
        for (i = 0; i < nf_count; i++) {
                printf("%" PRIu8 ".%" PRIu8 ".%" PRIu8 ".%" PRIu8 " ", (fwd_nf[i].ip >> 24) & 0xFF, (fwd_nf[i].ip >> 16) & 0xFF,
                       (fwd_nf[i].ip >> 8) & 0xFF, fwd_nf[i].ip & 0xFF);
                printf(" %d\n", fwd_nf[i].dest);
        }

        return ret;
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
packet_handler(struct rte_mbuf *pkt, struct onvm_pkt_meta *meta,
               __attribute__((unused)) struct onvm_nf_local_ctx *nf_local_ctx) {
        static uint32_t counter = 0;
        struct ether_hdr *eth_hdr;
        struct arp_hdr *in_arp_hdr;
        struct ipv4_hdr *ip;
        int i;

        ip = onvm_pkt_ipv4_hdr(pkt);

        /* If the packet doesn't have an IP header check if its an ARP, if so fwd it to the matched NF */
        if (ip == NULL) {
                eth_hdr = onvm_pkt_ether_hdr(pkt);
                if (rte_cpu_to_be_16(eth_hdr->ether_type) == ETHER_TYPE_ARP) {
                        in_arp_hdr = rte_pktmbuf_mtod_offset(pkt, struct arp_hdr *, sizeof(struct ether_hdr));
                        for (i = 0; i < nf_count; i++) {
                                if (rte_be_to_cpu_32(in_arp_hdr->arp_data.arp_tip) == fwd_nf[i].ip) {
                                        meta->destination = fwd_nf[i].dest;
                                        meta->action = ONVM_NF_ACTION_TONF;
                                        return 0;
                                }
                        }
                }
                meta->action = ONVM_NF_ACTION_DROP;
                meta->destination = 0;
                return 0;
        }

        if (++counter == print_delay) {
                do_stats_display(pkt);
                counter = 0;
        }

        for (i = 0; i < nf_count; i++) {
                if (fwd_nf[i].ip == rte_be_to_cpu_32(ip->dst_addr)) {
                        meta->destination = fwd_nf[i].dest;
                        meta->action = ONVM_NF_ACTION_TONF;
                        return 0;
                }
        }

        meta->action = ONVM_NF_ACTION_DROP;
        meta->destination = 0;

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
        parse_router_config();

        onvm_nflib_run(nf_local_ctx);

        onvm_nflib_stop(nf_local_ctx);
        printf("If we reach here, program is ending\n");
        return 0;
}
