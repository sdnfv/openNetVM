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
#include "onvm_nflib.h"

/* Number of packets to attempt to read from queue */
#define PKT_READ_SIZE  ((uint16_t)32)

/* maps input ports to output ports for packets */
static uint8_t output_ports[RTE_MAX_ETHPORTS];

/* this ring is used to pass packets from NFlib to NFmgr -- extern in header */
struct rte_ring *tx_ring;

/* shared data from server. We update statistics here */
static volatile struct tx_stats *tx_stats;

/* ring where the data are */
static struct rte_ring *rx_ring;

/* our client id number - tells us which rx queue to read, and NIC TX
 * queue to write to. */
static uint8_t client_id;

/*
 * Print a usage message
 */
static void
usage(const char *progname)
{
        printf("Usage: %s [EAL args] -- -n <client_id>\n\n", progname);
}

/*
 * Parse the library arguments.
 */
static int
parse_nflib_args(int argc, char *argv[])
{
        const char *progname = argv[0];
        int c;

        opterr = 0;

        while ((c = getopt (argc, argv, "n:")) != -1)
                switch (c)
                {
                case 'n':
                        client_id =  strtoul(optarg, NULL, 10);
                        break;
                case '?':
                        usage(progname);
                        if (optopt == 'n')
                                fprintf (stderr, "Option -%c requires an argument.\n", optopt);
                        else if (isprint (optopt))
                                fprintf (stderr, "Unknown option `-%c'.\n", optopt);
                        else
                                fprintf (stderr,"Unknown option character `\\x%x'.\n", optopt);
                        return 1;
                default:
                        abort ();
                }
        printf ("client id = %d\n", client_id); // debug
        return optind;
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

/*
 * The action parameter is just a pointer to pkt->udata64, so we don't currently use it.
 * ONVM_NF_ACTION_DROP  // drop packet
 * ONVM_NF_ACTION_NEXT  // to whatever the next action is configured by the SDN controller in the flow table
 * ONVM_NF_ACTION_TONF  // send to the NF specified in the argument field (assume it is on the same host)
 * ONVM_NF_ACTION_OUT   // send the packet out the NIC port set in the argument field
 */
// static int
// return_packet(struct onvm_nf_info* info __attribute__((__unused__)), struct rte_mbuf* pkt, struct onvm_pkt_action* action)
// {
//         // TODO link with the data structure of the server (ring)
//         if (action->action == ONVM_NF_ACTION_DROP) {
//                 rte_pktmbuf_free(pkt);
//                 tx_stats->tx_drop[0]++;
//         } else if(action->action == ONVM_NF_ACTION_NEXT) {
//                 if( unlikely(rte_ring_enqueue(tx_ring, (void*)pkt) == -ENOBUFS) ) {
//                         return -1;
//                 }
//         } else if(action->action == ONVM_NF_ACTION_TONF) {
//                 if( unlikely(rte_ring_enqueue(tx_ring, (void*)pkt) == -ENOBUFS) ) {
//                         return -1;
//                 }
//         } else if(action->action == ONVM_NF_ACTION_OUT) {
//                 rte_eth_tx_burst(action->destination, 0, (struct rte_mbuf **) &pkt, 1);
//                 tx_stats->tx[0]++;
//         } else {
//                 return -1;
//         }
//         // OLD
//         // sent = rte_eth_tx_burst(0, 0, (struct rte_mbuf **) pkts, rx_pkts);
//         // if (unlikely(sent < rx_pkts)){
//         //         for (i = sent; i < rx_pkts; i++)
//         //                 rte_pktmbuf_free(pkts[i]);
//         //         tx_stats->tx_drop[0] += (rx_pkts - sent);
//         // }
//         // tx_stats->tx[0] += sent;
//         return 0;
// }

int
onvm_nf_init(int argc, char *argv[], struct onvm_nf_info* info)
{
        const struct rte_memzone *mz;
        struct rte_mempool *mp;
        struct port_info *ports;
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

        tx_ring = rte_ring_lookup(get_tx_queue_name(info->client_id));
        if (tx_ring == NULL)
                rte_exit(EXIT_FAILURE, "Cannot get TX ring - is server process running?\n");

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
        return retval_parse + retval_eal;
}

/*
 * Application main function - loops through
 * receiving and processing packets. Never returns
 */
int
onvm_nf_run(struct onvm_nf_info* info, void(*handler)(struct rte_mbuf* pkt, struct onvm_pkt_action* action))
{
        void *pkts[PKT_READ_SIZE];
        struct onvm_pkt_action* action;
        info->client_id = client_id;

        printf("\nClient process %d handling packets\n", client_id);
        printf("[Press Ctrl-C to quit ...]\n");

        for (;;) {
                uint16_t i, rx_pkts = PKT_READ_SIZE;

                /* try dequeuing max possible packets first, if that fails, get the
                 * most we can. Loop body should only execute once, maximum */
                while (rx_pkts > 0 &&
                                unlikely(rte_ring_dequeue_bulk(rx_ring, pkts, rx_pkts) != 0))
                        rx_pkts = (uint16_t)RTE_MIN(rte_ring_count(rx_ring), PKT_READ_SIZE);

                /* Give each packet to the user proccessing function */
                for (i = 0; i < rx_pkts; i++) {
                        action = (struct onvm_pkt_action*) &(((struct rte_mbuf*)pkts[i])->udata64);
                        (*handler)((struct rte_mbuf*)pkts[i], action);
                        //return_packet(info, (struct rte_mbuf*)pkts[i], action);
                }
                if( unlikely(rte_ring_enqueue_bulk(tx_ring, pkts, rx_pkts) == -ENOBUFS) ) {
                        tx_stats->tx_drop[0] += rx_pkts;
                }

        }
}
