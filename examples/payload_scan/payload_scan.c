/*********************************************************************
 *                     openNetVM
 *              https://sdnfv.github.io
 *
 *   BSD LICENSE
 *
 *   Copyright(c)
 *            2015-2017 George Washington University
 *            2015-2017 University of California Riverside
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
 * payload_scan.c - an example using onvm. Scan a packet payload for a string
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

#include <rte_common.h>
#include <rte_mbuf.h>
#include <rte_ip.h>

#include "onvm_nflib.h"
#include "onvm_pkt_helper.h"

#define NF_TAG "payload_scan"

/* Struct that contains information about this NF */
struct onvm_nf_info *nf_info;

/* Number of packets between prints */
static uint32_t print_delay = 10000000;

/* Struct holding information about packet stats*/
static struct onvm_pkt_stats stats;

/* Destination NF ID */
static uint16_t destination;

/* String to search within packet payload*/
static char *search_term = NULL;

/* Shared data structure containing host port info */
extern struct port_info *ports;

struct onvm_pkt_stats {
    uint64_t pkt_drop;
    uint64_t pkt_accept;
    uint64_t pkt_total;
    uint64_t pkt_not_ipv4;
    uint64_t pkt_not_tcp_udp;
};

/*
 * Print a usage message
 */
static void
usage(const char *progname) {
        printf("Usage:\n");
        printf("%s [EAL args] -- [NF_LIB args] -- -p <print_delay>\n", progname);
        printf("%s -F <CONFIG_FILE.json> [EAL args] -- [NF_LIB args] -- [NF args]\n\n", progname);        
        printf("Flags:\n");
        printf(" - `-p <print_delay>`: number of packets between each print, e.g. `-p 1` prints every packets.\n");
}

/*
 * Parse the application arguments.
 */
static int
parse_app_args(int argc, char *argv[], const char *progname) {
        int c, dst_flag = 0, input_flag = 0;

        while ((c = getopt (argc, argv, "d:s:p")) != -1) {
                switch (c) {
                case 'd':
                        destination = strtoul(optarg, NULL, 10);
                        dst_flag = 1;
                        break;
                case 's':
                        search_term = malloc(sizeof(char) * (strlen(optarg)));
                        strcpy(search_term, optarg);
                        RTE_LOG(INFO, APP, "Search term = %s\n", search_term);
                        input_flag = 1;
                        break;
                case 'p':
                        print_delay = strtoul(optarg, NULL, 10);
                        RTE_LOG(INFO, APP, "Print delay = %d\n", print_delay);
                        break;
                case '?':
                        usage(progname);
                        if (optopt == 'p')
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
                RTE_LOG(INFO, APP, "Payload NF requires a destination NF with the -d flag.\n");
                return -1;
        }
        if (!input_flag) {
                RTE_LOG(INFO, APP, "Payload NF requires a search term string with the -s flag.\n");
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
do_stats_display(void) {
        const char clr[] = { 27, '[', '2', 'J', '\0' };
        const char topLeft[] = { 27, '[', '1', ';', '1', 'H', '\0' };

        /* Clear screen and move to top left */
        printf("%s%s", clr, topLeft);
        printf("Search term: %s\n", search_term);
        printf("Packets accepted: %lu\n", stats.pkt_accept);
        printf("Packets dropped: %lu\n", stats.pkt_drop);
        printf("Packets not IPv4: %lu\n", stats.pkt_not_ipv4);
        printf("Packets not UDP/TCP: %lu\n", stats.pkt_not_tcp_udp);
        printf("Packets total: %lu", stats.pkt_total);

        printf("\n\n");
}

static int
packet_handler(struct rte_mbuf *pkt, struct onvm_pkt_meta *meta, __attribute__((unused)) struct onvm_nf_info *nf_info) { // assuming UDP packets
        int udp_pkt, tcp_pkt;
        static uint32_t counter = 0;
        uint8_t *pkt_data;

        if (++counter == print_delay) {
                do_stats_display();
                counter = 0;
        }

        stats.pkt_total++;

        if (!onvm_pkt_is_ipv4(pkt)) {
                meta->action = ONVM_NF_ACTION_DROP;
                stats.pkt_not_ipv4++;
                return 0;
        }

        udp_pkt = onvm_pkt_is_udp(pkt);
        tcp_pkt = onvm_pkt_is_tcp(pkt);
	pkt_data = NULL;

        if (!udp_pkt && !tcp_pkt) {
                meta->action = ONVM_NF_ACTION_DROP;
                stats.pkt_not_tcp_udp++;
                return 0;
        }

        if (udp_pkt) {
                pkt_data = rte_pktmbuf_mtod_offset(pkt, uint8_t *, sizeof(struct ether_hdr) + sizeof(struct ipv4_hdr) +
                                                                            sizeof(struct udp_hdr));
	} else {
                pkt_data = rte_pktmbuf_mtod_offset(pkt, uint8_t *, sizeof(struct ether_hdr) + sizeof(struct ipv4_hdr) +
                                                                   sizeof(struct tcp_hdr));
        }

        if (strstr((const char *) pkt_data, search_term) != NULL) {
            meta->action = ONVM_NF_ACTION_TONF;
            meta->destination = destination;
            stats.pkt_accept++;
	} else {
            meta->action = ONVM_NF_ACTION_DROP;
            stats.pkt_drop++;
        }

        return 0;
}


int main(int argc, char *argv[]) {
        int arg_offset;
        const char *progname = argv[0];

        if ((arg_offset = onvm_nflib_init(argc, argv, NF_TAG, &nf_info)) < 0)
                return -1;

        argc -= arg_offset;
        argv += arg_offset;

        stats.pkt_drop = 0;
        stats.pkt_accept = 0;
        stats.pkt_total = 0;

        if (parse_app_args(argc, argv, progname) < 0) {
                onvm_nflib_stop(nf_info);
                rte_exit(EXIT_FAILURE, "Invalid command-line arguments\n");
        }

        onvm_nflib_run(nf_info, &packet_handler);
        printf("If we reach here, program is ending\n");
        return 0;
}
