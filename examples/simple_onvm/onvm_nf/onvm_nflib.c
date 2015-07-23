/*********************************************************************
 *                     openNetVM
 *       https://github.com/sdnfv/openNetVM
 *
 *  Copyright 2015 George Washington University
 *            2015 University of California Riverside
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 * onvm_nflib.c - client lib NF for simple onvm
 ********************************************************************/


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
#include <rte_memory.h>
#include <rte_memzone.h>
#include <rte_tailq.h>
#include <rte_eal.h>
#include <rte_atomic.h>
#include <rte_branch_prediction.h>
#include <rte_log.h>
#include <rte_per_lcore.h>
#include <rte_launch.h>
#include <rte_lcore.h>
#include <rte_ring.h>
#include <rte_debug.h>
#include <rte_mempool.h>
#include <rte_mbuf.h>
#include <rte_interrupts.h>
#include <rte_pci.h>
#include <rte_ether.h>
#include <rte_ethdev.h>
#include <rte_string_fns.h>

#include "common.h"
#include "onvm_nflib.h"

/* Number of packets to attempt to read from queue */
#define PKT_READ_SIZE  ((uint16_t)32)

/* rings used to pass packets between NFlib and NFmgr */
static struct rte_ring *tx_ring, *rx_ring;

/* shared data from server. We update statistics here */
static volatile struct client_tx_stats *tx_stats;

/* our client id number - tells us which rx queue to read, and NIC TX
 * queue to write to. */
static uint8_t client_id;

/*
 * Print a usage message
 */
static void
usage(const char *progname) {
        printf("Usage: %s [EAL args] -- -n <client_id>\n\n", progname);
}

/*
 * Parse the library arguments.
 */
static int
parse_nflib_args(int argc, char *argv[]) {
        const char *progname = argv[0];
        int c;

        opterr = 0;
        while ((c = getopt (argc, argv, "n:")) != -1)
                switch (c) {
                case 'n':
                        client_id = strtoul(optarg, NULL, 10);
                        break;
                case '?':
                        usage(progname);
                        if (optopt == 'n')
                                fprintf(stderr, "Option -%c requires an argument.\n", optopt);
                        else if (isprint(optopt))
                                fprintf(stderr, "Unknown option `-%c'.\n", optopt);
                        else
                                fprintf(stderr, "Unknown option character `\\x%x'.\n", optopt);
                        return 1;
                default:
                        abort();
                }
        return optind;
}

int
onvm_nf_init(int argc, char *argv[], struct onvm_nf_info* info) {
        const struct rte_memzone *mz;
        struct rte_mempool *mp;
        int retval_eal, retval_parse;

        if ((retval_eal = rte_eal_init(argc, argv)) < 0)
                return -1;
        argc -= retval_eal;
        argv += retval_eal;

        if ((retval_parse = parse_nflib_args(argc, argv)) < 0)
                rte_exit(EXIT_FAILURE, "Invalid command-line arguments\n");
        info->client_id = client_id;

        if (rte_eth_dev_count() == 0)
                rte_exit(EXIT_FAILURE, "No Ethernet ports - bye\n");

        rx_ring = rte_ring_lookup(get_rx_queue_name(client_id));
        if (rx_ring == NULL)
                rte_exit(EXIT_FAILURE, "Cannot get RX ring - is server process running?\n");

        tx_ring = rte_ring_lookup(get_tx_queue_name(client_id));
        if (tx_ring == NULL)
                rte_exit(EXIT_FAILURE, "Cannot get TX ring - is server process running?\n");

        mp = rte_mempool_lookup(PKTMBUF_POOL_NAME);
        if (mp == NULL)
                rte_exit(EXIT_FAILURE, "Cannot get mempool for mbufs\n");

        mz = rte_memzone_lookup(MZ_CLIENT_INFO);
        if (mz == NULL)
                rte_exit(EXIT_FAILURE, "Cannot get tx info structure\n");
        tx_stats = mz->addr;

        RTE_LOG(INFO, APP, "Finished Process Init.\n");
        return retval_eal;
}

/*
 * Application main function - loops through
 * receiving and processing packets. Never returns
 */
int
onvm_nf_run(struct onvm_nf_info* info, void(*handler)(struct rte_mbuf* pkt, struct onvm_pkt_action* action)) {
        void *pkts[PKT_READ_SIZE];
        struct onvm_pkt_action* action;
        info->client_id = client_id;

        printf("\nClient process %d handling packets\n", client_id);
        printf("[Press Ctrl-C to quit ...]\n");

        for (;;) {
                uint16_t i, j, nb_pkts = PKT_READ_SIZE;

                /* try dequeuing max possible packets first, if that fails, get the
                 * most we can. Loop body should only execute once, maximum */
                while (nb_pkts > 0 &&
                                unlikely(rte_ring_dequeue_bulk(rx_ring, pkts, nb_pkts) != 0))
                        nb_pkts = (uint16_t)RTE_MIN(rte_ring_count(rx_ring), PKT_READ_SIZE);

                /* Give each packet to the user proccessing function */
                for (i = 0; i < nb_pkts; i++) {
                        action = (struct onvm_pkt_action*) &(((struct rte_mbuf*)pkts[i])->udata64);
                        (*handler)((struct rte_mbuf*)pkts[i], action);
                }

                if (unlikely(rte_ring_enqueue_bulk(tx_ring, pkts, nb_pkts) == -ENOBUFS)) {
                        tx_stats->tx_drop[client_id] += nb_pkts;
                        for (j = 0; j < nb_pkts; j++) {
                                rte_pktmbuf_free(pkts[j]);
                        }
                } else {
                        tx_stats->tx[client_id] += nb_pkts;
                }
        }
}
