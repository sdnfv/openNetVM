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

#include "shared/common.h"
#include "onvm_mgr/args.h"
#include "onvm_mgr/init.h"

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

/* Argument struct for TX threads. The thread will handle TX for clients
   with ids first_cl ... (last_cl - 1) */
struct tx_arg {
       unsigned first_cl;
       unsigned last_cl;
       /* FIXME: This is confusing since it is non-inclusive. It would be
        *        better to have this take the first client and the number
        *        of consecutive clients after it to handle.
        */
};

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
                const uint64_t rx = clients[i].stats.rx;
                const uint64_t rx_drop = clients[i].stats.rx_drop;
                const uint64_t tx = clients_stats->tx[i];
                const uint64_t tx_drop = clients_stats->tx_drop[i];
                const uint64_t act_drop = clients[i].stats.act_drop;
                const uint64_t act_next = clients[i].stats.act_next;
                const uint64_t act_out = clients[i].stats.act_out;
                const uint64_t act_tonf = clients[i].stats.act_tonf;

                printf("Client %2u - rx: %9"PRIu64", rx_drop: %9"PRIu64", next: %9"PRIu64", drop: %9"PRIu64"\n"
                                "            tx: %9"PRIu64", tx_drop: %9"PRIu64", out : %9"PRIu64", tonf: %9"PRIu64"\n",
                                i, rx, rx_drop, act_next, act_drop, tx, tx_drop, act_out, act_tonf);
        }

        printf("\n");
}

/*
 * Stats thread periodically prints per-port and per-NF stats.
 */
static int
stats_thread_main(__attribute__((unused)) void *dummy) {
        const unsigned sleeptime = 1;
        RTE_LOG(INFO, APP, "Core %d displaying statistics\n", rte_lcore_id());

        /* Longer initial pause so above printf is seen */
        sleep(sleeptime * 3);

        /* Loop forever: sleep always returns 0 or <= param */
        while (sleep(sleeptime) <= sleeptime)
                do_stats_display();
        return 0;
}

/*
 * Function to set all the client statistic values to zero.
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
 * TODO: Split this into two functions and rename--I'm not sure why it has rx
 * 	 in the name.
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
process_rx_packet_batch(struct rte_mbuf *pkts[], uint16_t rx_count) {
        struct client *cl;
        uint16_t i, j;
        struct onvm_pkt_meta *meta;

        for (i = 0; i < tx_count; i++) {
                meta = (struct onvm_pkt_meta*) &(((struct rte_mbuf*)pkts[i])->udata64);
                meta->src = 0; // FIXME: this should be an ID to represent the NIC port
                /* PERF: this might hurt performance since it will cause cache
                 * invalidations. Ideally the data modified by the NF manager
                 * would be a different line than that modified/read by NFs.
                 * That may not be possible.
                 */
        }

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
 * Handle the packets from a client. Check what the next action is
 * and forward the packet either to the NIC or to another NF Client.
 */
static void
process_tx_packet_batch(struct rte_mbuf *pkts[], uint16_t tx_count, struct client *cl) {
        uint16_t i;
        struct onvm_pkt_meta *meta;

        for (i = 0; i < tx_count; i++) {
                meta = (struct onvm_pkt_meta*) &(((struct rte_mbuf*)pkts[i])->udata64);
                meta->src = cl->client_id;
                if (meta->action == ONVM_NF_ACTION_DROP) {
                        rte_pktmbuf_free(pkts[i]);
                        cl->stats.act_drop++;
                } else if (meta->action == ONVM_NF_ACTION_NEXT) {
                        /* TODO: Here we drop the packet : there will be a flow table
                        in the future to know what to do with the packet next */
                        cl->stats.act_next++;
                        rte_pktmbuf_free(pkts[i]);
                        printf("Select ONVM_NF_ACTION_NEXT : this shouldn't happen.\n");
                } else if (meta->action == ONVM_NF_ACTION_TONF) {
                        cl->stats.act_tonf++;
                        enqueue_rx_packet(meta->destination, pkts[i], TO_CLIENT);
                } else if (meta->action == ONVM_NF_ACTION_OUT) {
                        cl->stats.act_out++;
                        enqueue_rx_packet(meta->destination, pkts[i], TO_PORT);
                } else {
                        rte_pktmbuf_free(pkts[i]);
                        return;
                }
        }
}

/*
 * Function called by the master lcore of the DPDK process to receive packets
 * from NIC and distributed them to NF-0.
 */
static void
rx_thread_main(void) {
        uint16_t i, rx_count;
        struct rte_mbuf *pkts[PACKET_READ_SIZE];

        for (;;) {
                /* Read ports */
                for (i = 0; i < ports->num_ports; i++) {
                        rx_count = rte_eth_rx_burst(ports->id[i], 0, \
                                        pkts, PACKET_READ_SIZE);
                        ports->rx_stats.rx[ports->id[i]] += rx_count;

                        /* Now process the NIC packets read */
                        if (likely(rx_count > 0)) {
                                process_rx_packet_batch(pkts, rx_count);
                        }
                }

                /* Send a burst to every client */
                flush_rx_queue(0);
        }
}


static int
tx_thread_main(void *arg) {
        struct client *cl;
        unsigned i, tx_count;
        struct rte_mbuf *pkts[PACKET_READ_SIZE];
        unsigned first_cl = ((struct tx_arg*)arg)->first_cl;
        unsigned last_cl  = ((struct tx_arg*)arg)->last_cl;

        if (first_cl == last_cl - 1) {
                RTE_LOG(INFO, APP, "  Handle client %d TX queue with core %d\n", first_cl, rte_lcore_id());
        } else if (first_cl < last_cl) {
                RTE_LOG(INFO, APP, "  Handle clients %d to %d TX queue with core %d\n", first_cl, last_cl - 1, rte_lcore_id());
        }

        for (;;) {
                /* Read packets from the client's tx queue and process them as needed */
                for (i = first_cl; i < last_cl; i++) {
                        tx_count = PACKET_READ_SIZE;
                        cl = &clients[i];
                        /* try dequeuing max possible packets first, if that fails, get the
                         * most we can. Loop body should only execute once, maximum */
                        while (tx_count > 0 &&
                                unlikely(rte_ring_dequeue_bulk(cl->tx_q, (void **) pkts, tx_count) != 0)) {
                                tx_count = (uint16_t)RTE_MIN(rte_ring_count(cl->tx_q),
                                        PACKET_READ_SIZE);
                        }

                        /* Now process the Client packets read */
                        if (likely(tx_count > 0)) {
                                process_tx_packet_batch(pkts, tx_count, cl);
                            }
                }

                /* TODO: figure out what the problem was with this code
                 * I think we turned it off because of a consistency
                 * problem between threads, but I may be misremembering.
                 * Since we aren't flushing here, it is possible that
                 * the last packets in a flow will never get flushed out.
                 * (we only flush in proccess_batch if the queue is full)
                 */

                /* Send a burst to every port */
                //for (i = 0; i < ports->num_ports; i++) {
                //        flush_tx_queue(i);
                //}
                /* Send a burst to every client */
                //for (i = 0; i < num_clients; i++) {
                //        flush_rx_queue(i);
                //}
        }
        return 0;
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

        unsigned cur_lcore = rte_lcore_id();
        RTE_LOG(INFO, APP, "Master core running on core %d\n", cur_lcore);

        /* Reserve 3 cores for: RX, stats, and final send out */
        unsigned tx_lcores = rte_lcore_count() - 3;
        RTE_LOG(INFO, APP, "%d cores available for handling client TX queue\n", tx_lcores + 1);

        /* Evenly assign clients to TX threads */
        unsigned next_client = 0;
        for (; tx_lcores > 0; tx_lcores--) {
                struct tx_arg *arg_lcore = malloc(sizeof(struct tx_arg));
                arg_lcore->first_cl = next_client;
                next_client += (num_clients - 1 - next_client)/tx_lcores
                            + ((num_clients - 1 - next_client)%tx_lcores > 0);
                arg_lcore->last_cl = next_client;
                cur_lcore = rte_get_next_lcore(cur_lcore, 1, 1);
                if (rte_eal_remote_launch(tx_thread_main, (void*)arg_lcore,  cur_lcore) == -EBUSY) {
                        RTE_LOG(ERR, APP, "Core %d is already busy\n", cur_lcore);
                        return -1;
                }
        }

        /* Assign one TX thread to handle the last client since sending
         * out is more expensive. This assumes you run a linear chain and
         * packets always leave the system from the last NF.
         */
        cur_lcore = rte_get_next_lcore(cur_lcore, 1, 1);
        struct tx_arg *arg_lcore = malloc(sizeof(struct tx_arg));
        arg_lcore->first_cl = num_clients-1;
        arg_lcore->last_cl = num_clients;
        if (rte_eal_remote_launch(tx_thread_main, (void*)arg_lcore,  cur_lcore) == -EBUSY) {
                RTE_LOG(ERR, APP, "Core %d is already busy\n", cur_lcore);
                return -1;
        }

        /* Start the stats display function on another core. */
        unsigned stat_lcore = rte_get_next_lcore(cur_lcore, 1, 1);
        if (rte_eal_remote_launch(stats_thread_main, NULL, stat_lcore) == -EBUSY) {
                RTE_LOG(ERR, APP, "Core %d is already busy\n", stat_lcore);
                return -1;
        }

        /* Master thread handles RX packets from NIC */
        rx_thread_main();
        return 0;
}
