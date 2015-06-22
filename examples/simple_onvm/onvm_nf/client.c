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
 * client.c - client NF for simple onvm
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
#include <rte_launch.h>
#include <rte_lcore.h>
#include <rte_debug.h>
#include <rte_mempool.h>
#include <rte_mbuf.h>
#include <rte_interrupts.h>
#include <rte_pci.h>
#include <rte_ether.h>
#include <rte_ethdev.h>
#include <rte_string_fns.h>

#include "common.h"

/* Number of packets to attempt to read from queue */
#define PKT_READ_SIZE  ((uint16_t)32)

/* our client id number - tells us which rx queue to read, and NIC TX
 * queue to write to. */
static uint8_t client_id = 0;


/* maps input ports to output ports for packets */
static uint8_t output_ports[RTE_MAX_ETHPORTS];


/* shared data from server. We update statistics here */
static volatile struct tx_stats *tx_stats;


/*
 * print a usage message
 */
static void
usage(const char *progname)
{
        printf("Usage: %s [EAL args] -- -n <client_id>\n\n", progname);
}

/*
 * Convert the client id number from a string to an int.
 */
static int
parse_client_num(const char *client)
{
        char *end = NULL;
        unsigned long temp;

        if (client == NULL || *client == '\0')
                return -1;

        temp = strtoul(client, &end, 10);
        if (end == NULL || *end != '\0')
                return -1;

        client_id = (uint8_t)temp;
        return 0;
}

/*
 * Parse the application arguments to the client app.
 */
static int
parse_app_args(int argc, char *argv[])
{
        int option_index, opt;
        char **argvopt = argv;
        const char *progname = NULL;
        static struct option lgopts[] = { /* no long options */
                {NULL, 0, 0, 0 }
        };
        progname = argv[0];

        while ((opt = getopt_long(argc, argvopt, "n:", lgopts,
                &option_index)) != EOF){
                switch (opt){
                        case 'n':
                                if (parse_client_num(optarg) != 0){
                                        usage(progname);
                                        return -1;
                                }
                                break;
                        default:
                                usage(progname);
                                return -1;
                }
        }
        return 0;
}

/*
 * set up output ports so that all traffic on port gets sent out
 * its paired port. Index using actual port numbers since that is
 * what comes in the mbuf structure.
 */
static void configure_output_ports(const struct port_info *ports)
{
        int i;
        if (ports->num_ports > RTE_MAX_ETHPORTS)
                rte_exit(EXIT_FAILURE, "Too many ethernet ports. RTE_MAX_ETHPORTS = %u\n",
                                (unsigned)RTE_MAX_ETHPORTS);
        for (i = 0; i < ports->num_ports; i+=1){
                uint8_t p1 = ports->id[i];
                output_ports[p1] = p1;
        }
}

static void nf_app_function(struct rte_mbuf *buf) {
        /* do nothing */

        printf("pkt on port %d and size %d\n" , buf->port, buf->pkt_len);
}

/*
 * Application main function - loops through
 * receiving and processing packets. Never returns
 */
int
main(int argc, char *argv[])
{
        const struct rte_memzone *mz;
        struct rte_ring *rx_ring;
        struct rte_mempool *mp;
        struct port_info *ports;
        int retval;
        void *pkts[PKT_READ_SIZE];

        if ((retval = rte_eal_init(argc, argv)) < 0)
                return -1;
        argc -= retval;
        argv += retval;

        if (parse_app_args(argc, argv) < 0)
                rte_exit(EXIT_FAILURE, "Invalid command-line arguments\n");

        if (rte_eth_dev_count() == 0)
                rte_exit(EXIT_FAILURE, "No Ethernet ports - bye\n");

        rx_ring = rte_ring_lookup(get_rx_queue_name(client_id));
        if (rx_ring == NULL)
                rte_exit(EXIT_FAILURE, "Cannot get RX ring - is server process running?\n");

        mp = rte_mempool_lookup(PKTMBUF_POOL_NAME);
        if (mp == NULL)
                rte_exit(EXIT_FAILURE, "Cannot get mempool for mbufs\n");

        mz = rte_memzone_lookup(MZ_PORT_INFO);
        if (mz == NULL)
                rte_exit(EXIT_FAILURE, "Cannot get port info structure\n");
        ports = mz->addr;
        tx_stats = &(ports->tx_stats[client_id]);

        configure_output_ports(ports);

        RTE_LOG(INFO, APP, "Finished Process Init.\n");

        printf("\nClient process %d handling packets\n", client_id);
        printf("[Press Ctrl-C to quit ...]\n");

        for (;;) {
                uint16_t i, rx_pkts = PKT_READ_SIZE;
                uint16_t sent;

                /* try dequeuing max possible packets first, if that fails, get the
                 * most we can. Loop body should only execute once, maximum */
                while (rx_pkts > 0 &&
                                unlikely(rte_ring_dequeue_bulk(rx_ring, pkts, rx_pkts) != 0))
                        rx_pkts = (uint16_t)RTE_MIN(rte_ring_count(rx_ring), PKT_READ_SIZE);

                /* Give each packet to the user proccessing function */
                for (i = 0; i < rx_pkts; i++) {
                        nf_app_function(pkts[i]);
                }

                sent = rte_eth_tx_burst(0, 0, (struct rte_mbuf **) pkts, rx_pkts);
                if (unlikely(sent < rx_pkts)){
                        for (i = sent; i < rx_pkts; i++)
                                rte_pktmbuf_free(pkts[i]);
                        tx_stats->tx_drop[0] += (rx_pkts - sent);
                }
                tx_stats->tx[0] += sent;

                // need_flush = 1;
        }
}
