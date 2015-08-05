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
 * main.c - simple onvm host code
 ********************************************************************/


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <stdarg.h>
#include <inttypes.h>
#include <sys/queue.h>
#include <errno.h>
#include <netinet/ip.h>
#include <stdbool.h>

#include <rte_common.h>
#include <rte_memory.h>
#include <rte_memzone.h>
#include <rte_tailq.h>
#include <rte_eal.h>
#include <rte_byteorder.h>
#include <rte_launch.h>
#include <rte_per_lcore.h>
#include <rte_lcore.h>
#include <rte_branch_prediction.h>
#include <rte_atomic.h>
#include <rte_ring.h>
#include <rte_log.h>
#include <rte_debug.h>
#include <rte_mempool.h>
#include <rte_memcpy.h>
#include <rte_mbuf.h>
#include <rte_ether.h>
#include <rte_interrupts.h>
#include <rte_pci.h>
#include <rte_ethdev.h>
#include <rte_malloc.h>
#include <rte_fbk_hash.h>
#include <rte_string_fns.h>

#include "common.h"
#include "args.h"
#include "init.h"

/*
 * When doing reads from the NIC or the client queues,
 * use this batch size
 */
#define PACKET_READ_SIZE ((uint16_t)32)

#define TO_PORT 0
#define TO_CLIENT 1

/*
 * Local buffers to put packets in, used to send packets in bursts to the
 * clients or to the NIC
 */
struct packet_buf {
        struct rte_mbuf *buffer[PACKET_READ_SIZE];
        uint16_t count;
};

/* One buffer per client rx queue - dynamically allocate array
 * and one buffer per port tx queue. */
static struct packet_buf *cl_rx_buf;
static struct packet_buf *port_tx_buf;

static const char *
get_printable_mac_addr(uint8_t port) {
        static const char err_address[] = "00:00:00:00:00:00";
        static char addresses[RTE_MAX_ETHPORTS][sizeof(err_address)];

        if (unlikely(port >= RTE_MAX_ETHPORTS))
                return err_address;
        if (unlikely(addresses[port][0] == '\0')) {
                struct ether_addr mac;
                rte_eth_macaddr_get(port, &mac);
                snprintf(addresses[port], sizeof(addresses[port]),
                                "%02x:%02x:%02x:%02x:%02x:%02x\n",
                                mac.addr_bytes[0], mac.addr_bytes[1], mac.addr_bytes[2],
                                mac.addr_bytes[3], mac.addr_bytes[4], mac.addr_bytes[5]);
        }
        return addresses[port];
}

/*
 * This function displays the recorded statistics for each port
 * and for each client. It uses ANSI terminal codes to clear
 * screen when called. It is called from a single non-master
 * thread in the server process, when the process is run with more
 * than one lcore enabled.
 */
static void
do_stats_display(void) {
        unsigned i;
        const char clr[] = { 27, '[', '2', 'J', '\0' };
        const char topLeft[] = { 27, '[', '1', ';', '1', 'H', '\0' };

        /* Clear screen and move to top left */
        printf("%s%s", clr, topLeft);

        printf("PORTS\n");
        printf("-----\n");
        for (i = 0; i < ports->num_ports; i++)
                printf("Port %u: '%s'\t", (unsigned)ports->id[i],
                                get_printable_mac_addr(ports->id[i]));
        printf("\n\n");
        for (i = 0; i < ports->num_ports; i++) {
                printf("Port %u - rx: %9"PRIu64"\t"
                                "tx: %9"PRIu64"\n",
                                (unsigned)ports->id[i], ports->rx_stats.rx[ports->id[i]],
                                ports->tx_stats.tx[ports->id[i]]);
        }

        printf("\nCLIENTS\n");
        printf("-------\n");
        for (i = 0; i < num_clients; i++) {
                const unsigned long long rx = clients[i].stats.rx;
                const unsigned long long rx_drop = clients[i].stats.rx_drop;
                const uint64_t tx = clients_stats->tx[i];
                const unsigned long long act_drop = clients[i].stats.act_drop;
                const unsigned long long act_next = clients[i].stats.act_next;
                const unsigned long long act_out = clients[i].stats.act_out;
                const unsigned long long act_tonf = clients[i].stats.act_tonf;
                const uint64_t tx_drop = clients_stats->tx_drop[i];
                printf("Client %2u - rx: %9llu, rx_drop: %9llu, next: %9llu, drop: %9llu\n"
                                "            tx: %9"PRIu64", tx_drop: %9"PRIu64", out : %9llu, tonf: %9llu\n",
                                i, rx, rx_drop, act_next, act_drop, tx, tx_drop, act_out, act_tonf);
        }

        printf("\n");
}

/*
 * The function called from each non-master lcore used by the process.
 * The test_and_set function is used to randomly pick a single lcore on which
 * the code to display the statistics will run. Otherwise, the code just
 * repeatedly sleeps.
 */
static int
sleep_lcore(__attribute__((unused)) void *dummy) {
        /* Used to pick a display thread - static, so zero-initialised */
        static rte_atomic32_t display_stats;

        /* Only one core should display stats */
        if (rte_atomic32_test_and_set(&display_stats)) {
                const unsigned sleeptime = 1;
                printf("Core %u displaying statistics\n", rte_lcore_id());

                /* Longer initial pause so above printf is seen */
                sleep(sleeptime * 3);

                /* Loop forever: sleep always returns 0 or <= param */
                while (sleep(sleeptime) <= sleeptime)
                        do_stats_display();
        }
        return 0;
}

/*
 * Function to set all the client statistic values to zero.
 * Called at program startup.
 */
static void
clear_stats(void) {
        unsigned i;

        for (i = 0; i < num_clients; i++) {
                clients[i].stats.rx = clients[i].stats.rx_drop = 0;
                clients[i].stats.act_drop = clients[i].stats.act_tonf = 0;
                clients[i].stats.act_next = clients[i].stats.act_out = 0;
	}
}

/*
 * Send a burst of traffic to a client, assuming there are packets
 * available to be sent to this client
 */
static void
flush_rx_queue(uint16_t client) {
        uint16_t i;
        struct client *cl;

        if (cl_rx_buf[client].count == 0)
                return;

        cl = &clients[client];
        if (rte_ring_enqueue_bulk(cl->rx_q, (void **)cl_rx_buf[client].buffer,
                        cl_rx_buf[client].count) != 0) {
                for (i = 0; i < cl_rx_buf[client].count; i++) {
                        rte_pktmbuf_free(cl_rx_buf[client].buffer[i]);
                }
                cl->stats.rx_drop += cl_rx_buf[client].count;
        } else {
                cl->stats.rx += cl_rx_buf[client].count;
        }
        cl_rx_buf[client].count = 0;
}

/**
 * Send a burst of packets out a NIC port.
 */
static void
flush_tx_queue(uint16_t port) {
        uint16_t i, sent;
        volatile struct tx_stats *tx_stats;

        if (port_tx_buf[port].count == 0)
                return;

        tx_stats = &(ports->tx_stats);
        sent = rte_eth_tx_burst(port, 0, port_tx_buf[port].buffer, port_tx_buf[port].count);
        if (unlikely(sent < port_tx_buf[port].count)) {
                for (i = sent; i < port_tx_buf[port].count; i++) {
                        rte_pktmbuf_free(port_tx_buf[port].buffer[i]);
                }
                tx_stats->tx_drop[port] += (port_tx_buf[port].count - sent);
        }
        tx_stats->tx[port] += sent;

        port_tx_buf[port].count = 0;
}

/*
 * Marks a packet down to be sent to a particular client process or to a port.
 */
static inline void
enqueue_rx_packet(uint16_t id, struct rte_mbuf *buf, int to_client) {
        if (to_client) {
                cl_rx_buf[id].buffer[cl_rx_buf[id].count++] = buf;
                if (cl_rx_buf[id].count == PACKET_READ_SIZE) {
                        flush_rx_queue(id);
                }
        } else {
                port_tx_buf[id].buffer[port_tx_buf[id].count++] = buf;
                if (port_tx_buf[id].count == PACKET_READ_SIZE) {
                        flush_tx_queue(id);
                }
        }
}

/*
 * This function takes a group of packets and routes them
 * to the first client process. Simply forwarding the packets
 * without checking any of the packet contents.
 */
static void
process_rx_packets(struct rte_mbuf *pkts[], uint16_t rx_count) {
        uint16_t j;
        struct client *cl;

        cl = &clients[0];
        if (unlikely(rte_ring_enqueue_bulk(cl->rx_q, (void**) pkts, rx_count) != 0)) {
                for (j = 0; j < rx_count; j++)
                        rte_pktmbuf_free(pkts[j]);
                cl->stats.rx_drop += rx_count;
        } else {
                cl->stats.rx += rx_count;
        }
}

/*
 * Handle the packets whose come from a client. Check what the next action is
 * and forward the packet either to the NIC or to another NF Client.
 */
static void
process_tx_packets(struct rte_mbuf *pkts[], uint16_t tx_count, struct client *cl) {
        uint16_t i;
        struct onvm_pkt_action *action;

        for (i = 0; i < tx_count; i++) {
                action = (struct onvm_pkt_action*) &(((struct rte_mbuf*)pkts[i])->udata64);
                if (action->action == ONVM_NF_ACTION_DROP) {
                        rte_pktmbuf_free(pkts[i]);
                        cl->stats.act_drop++;
                } else if (action->action == ONVM_NF_ACTION_NEXT) {
                        /* Here we drop the packet : there will be a flow table
                        in the future to know what to do with the packet next */
                        cl->stats.act_next++;
                        rte_pktmbuf_free(pkts[i]);
                        printf("Select ONVM_NF_ACTION_NEXT : this shouldn't happen.\n");
                } else if (action->action == ONVM_NF_ACTION_TONF) {
                        cl->stats.act_tonf++;
                        enqueue_rx_packet(action->destination, pkts[i], TO_CLIENT);
                } else if (action->action == ONVM_NF_ACTION_OUT) {
                        cl->stats.act_out++;
                        enqueue_rx_packet(action->destination, pkts[i], TO_PORT);
                } else {
                        rte_pktmbuf_free(pkts[i]);
                        return;
                }
        }
}

/*
 * Function called by the master lcore of the DPDK process.
 */
static void
do_rx_tx(void) {
	uint16_t i, rx_count, tx_count;
        struct rte_mbuf *rx_pkts[PACKET_READ_SIZE], *tx_pkts[PACKET_READ_SIZE];
        struct client *cl;

        for (;;) {
                /* Read ports */
		for (i = 0; i < ports->num_ports; i++) {
			rx_count = rte_eth_rx_burst(ports->id[i], 0, \
					rx_pkts, PACKET_READ_SIZE);
			ports->rx_stats.rx[ports->id[i]] += rx_count;

			/* Now process the NIC packets read */
			if (likely(rx_count > 0)) {
				process_rx_packets(rx_pkts, rx_count);
			}
		}

                /* Read packets from the client's tx queue and process them as needed */
                for (i = 0; i < num_clients; i++) {
                        tx_count = PACKET_READ_SIZE;
                        cl = &clients[i];
                        /* try dequeuing max possible packets first, if that fails, get the
                         * most we can. Loop body should only execute once, maximum */
                        while (tx_count > 0 &&
                                unlikely(rte_ring_dequeue_bulk(cl->tx_q, (void **) tx_pkts, tx_count) != 0)) {
                                tx_count = (uint16_t)RTE_MIN(rte_ring_count(cl->tx_q),
                                        PACKET_READ_SIZE);
                        }

                        /* Now process the Client packets read */
                        if (likely(tx_count > 0)) {
                                process_tx_packets(tx_pkts, tx_count, cl);
                            }
                }

                /* Send a burst to every port */
                for (i = 0; i < ports->num_ports; i++) {
                        flush_tx_queue(i);
                }
                /* Send a burst to every client */
                for (i = 0; i < num_clients; i++) {
                        flush_rx_queue(i);
                }
        }
}

int
main(int argc, char *argv[]) {
        /* initialise the system */
        if (init(argc, argv) < 0 )
                return -1;
        RTE_LOG(INFO, APP, "Finished Process Init.\n");

	cl_rx_buf = calloc(num_clients, sizeof(struct packet_buf));
        port_tx_buf = calloc(RTE_MAX_ETHPORTS, sizeof(struct packet_buf));
	
	/* clear statistics */
        clear_stats();

        /* put all other cores to sleep bar master */
        rte_eal_mp_remote_launch(sleep_lcore, NULL, SKIP_MASTER);

        do_rx_tx();
        return 0;
}
