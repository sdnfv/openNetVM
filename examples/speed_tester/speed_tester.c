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
 * speed_tester.c - create pkts and loop through NFs.
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
#include <rte_mempool.h>
#include <rte_cycles.h>


#include "onvm_nflib.h"
#include "onvm_pkt_helper.h"

#define NF_TAG "speed"

#define NUM_PKTS 128
#define PKTMBUF_POOL_NAME "MProc_pktmbuf_pool"

/* Struct that contains information about this NF */
struct onvm_nf_info *nf_info;

/* number of package between each print */
static uint32_t print_delay = 10000000;
static uint16_t destination;

/*
 * Print a usage message
 */
static void
usage(const char *progname) {
        printf("Usage: %s [EAL args] -- [NF_LIB args] -- -d <destination> -p <print_delay>\n\n", progname);
}

/*
 * Parse the application arguments.
 */
static int
parse_app_args(int argc, char *argv[], const char *progname) {
        int c, dst_flag = 0;

        while ((c = getopt (argc, argv, "d:p:")) != -1) {
                switch (c) {
                case 'd':
                        destination = strtoul(optarg, NULL, 10);
                        dst_flag = 1;
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

        if (!dst_flag) {
                RTE_LOG(INFO, APP, "Speed tester NF requires a destination NF with the -d flag.\n");
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
do_stats_display(struct rte_mbuf* pkt) {
        static uint64_t last_cycles;
        static uint64_t cur_pkts = 0;
        static uint64_t last_pkts = 0;
        const char clr[] = { 27, '[', '2', 'J', '\0' };
        const char topLeft[] = { 27, '[', '1', ';', '1', 'H', '\0' };
        (void)pkt;

        uint64_t cur_cycles = rte_get_tsc_cycles();
        cur_pkts += print_delay;

        /* Clear screen and move to top left */
        printf("%s%s", clr, topLeft);

        printf("Total packets: %9"PRIu64" \n", cur_pkts);
        printf("TX pkts per second: %9"PRIu64" \n", (cur_pkts - last_pkts)
                * rte_get_timer_hz() / (cur_cycles - last_cycles));
        printf("Packets per group: %d\n", NUM_PKTS);

        last_pkts = cur_pkts;
        last_cycles = cur_cycles;

        printf("\n\n");
}

static int
packet_handler(struct rte_mbuf* pkt, struct onvm_pkt_meta* meta) {
        static uint32_t counter = 0;
        if (counter++ == print_delay) {
                do_stats_display(pkt);
                counter = 0;
        }

        if(pkt->port == 3) {
                /* one of our fake pkts to forward */
                meta->destination = destination;
                meta->action = ONVM_NF_ACTION_TONF;
        }
        else {
                /* Drop real incoming packets */
                meta->action = ONVM_NF_ACTION_DROP;
        }
        return 0;
}


int main(int argc, char *argv[]) {
        int arg_offset;

        const char *progname = argv[0];

        if ((arg_offset = onvm_nflib_init(argc, argv, NF_TAG)) < 0)
                return -1;
        argc -= arg_offset;
        argv += arg_offset;

        if (parse_app_args(argc, argv, progname) < 0) {
                onvm_nflib_stop();
                rte_exit(EXIT_FAILURE, "Invalid command-line arguments\n");
        }

        struct rte_mempool *pktmbuf_pool;
        struct rte_mbuf* pkts[NUM_PKTS];
        int i;

        pktmbuf_pool = rte_mempool_lookup(PKTMBUF_POOL_NAME);
        if(pktmbuf_pool == NULL) {
                onvm_nflib_stop();
                rte_exit(EXIT_FAILURE, "Cannot find mbuf pool!\n");
        }
        printf("Creating %d packets to send to %d\n", NUM_PKTS, destination);
        for (i=0; i < NUM_PKTS; i++) {
                struct onvm_pkt_meta* pmeta;
                pkts[i] = rte_pktmbuf_alloc(pktmbuf_pool);
                pmeta = onvm_get_pkt_meta(pkts[i]);
                pmeta->destination = destination;
                pmeta->action = ONVM_NF_ACTION_TONF;
                pkts[i]->port = 3;
                pkts[i]->hash.rss = i;
                onvm_nflib_return_pkt(pkts[i]);
        }

        onvm_nflib_run(nf_info, &packet_handler);
        printf("If we reach here, program is ending");
        return 0;
}
