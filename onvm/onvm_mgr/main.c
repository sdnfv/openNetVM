/*********************************************************************
 *                     openNetVM
 *              https://sdnfv.github.io
 *
 *   BSD LICENSE
 *
 *   Copyright(c)
 *            2015-2016 George Washington University
 *            2015-2016 University of California Riverside
 *            2010-2014 Intel Corporation. All rights reserved.
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
 *     * Neither the name of Intel Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
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
#include <math.h>

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
#include "shared/onvm_sc_mgr.h"
#include "shared/onvm_flow_table.h"
#include "shared/onvm_flow_dir.h"

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

/* ID to be assigned to the next NF that starts */
static uint16_t next_instance_id;

/** Thread state. This specifies which NFs the thread will handle and
 *  includes the packet buffers used by the thread for NFs and ports.
 */
struct thread_info {
       unsigned queue_id;
       unsigned first_cl;
       unsigned last_cl;
       /* FIXME: This is confusing since it is non-inclusive. It would be
        *        better to have this take the first client and the number
        *        of consecutive clients after it to handle.
        */
       struct packet_buf *nf_rx_buf;
       struct packet_buf *port_tx_buf;
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

/**
 * Helper function to determine if an item in the clients array represents a valid NF
 * A "valid" NF consists of:
 *  - A non-null index that also contains an associated info struct
 *  - A status set to NF_RUNNING
 */
static inline int
is_valid_nf(struct client *cl)
{
        return cl && cl->info && cl->info->status == NF_RUNNING;
}

/*
 * This function displays the recorded statistics for each port
 * and for each client. It uses ANSI terminal codes to clear
 * screen when called. It is called from a single non-master
 * thread in the server process, when the process is run with more
 * than one lcore enabled.
 */
static void
do_stats_display(unsigned sleeptime) {
        unsigned i;
        /* Arrays to store last TX/RX count to calculate rate */
        static uint64_t tx_last[RTE_MAX_ETHPORTS];
        static uint64_t rx_last[RTE_MAX_ETHPORTS];
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
                printf("Port %u - rx: %9"PRIu64"  (%9"PRIu64" pps)\t"
                                 "tx: %9"PRIu64"  (%9"PRIu64" pps)\n",
                                (unsigned)ports->id[i],
                                ports->rx_stats.rx[ports->id[i]],
                                (ports->rx_stats.rx[ports->id[i]] - rx_last[i])/sleeptime,
                                ports->tx_stats.tx[ports->id[i]],
                                (ports->tx_stats.tx[ports->id[i]] - tx_last[i])/sleeptime);
                rx_last[i] = ports->rx_stats.rx[ports->id[i]];
                tx_last[i] = ports->tx_stats.tx[ports->id[i]];
        }

        printf("\nCLIENTS\n");
        printf("-------\n");
        for (i = 0; i < MAX_CLIENTS; i++) {
                if (!is_valid_nf(&clients[i]))
                        continue;
                const uint64_t rx = clients[i].stats.rx;
                const uint64_t rx_drop = clients[i].stats.rx_drop;
                const uint64_t tx = clients_stats->tx[i];
                const uint64_t tx_drop = clients_stats->tx_drop[i];
                const uint64_t act_drop = clients[i].stats.act_drop;
                const uint64_t act_next = clients[i].stats.act_next;
                const uint64_t act_out = clients[i].stats.act_out;
                const uint64_t act_tonf = clients[i].stats.act_tonf;
                const uint64_t act_buffer = clients_stats->tx_buffer[i];
                const uint64_t act_returned = clients_stats->tx_returned[i];

                printf("Client %2u - rx: %9"PRIu64" rx_drop: %9"PRIu64" next: %9"PRIu64" drop: %9"PRIu64" ret: %9"PRIu64"\n"
                                "            tx: %9"PRIu64" tx_drop: %9"PRIu64" out:  %9"PRIu64" tonf: %9"PRIu64" buf: %9"PRIu64" \n",
                                clients[i].info->instance_id, rx, rx_drop, act_next, act_drop, act_returned, tx, tx_drop, act_out, act_tonf, act_buffer);
        }

        printf("\n");
}

/**
 * Verifies that the next client id the manager gives out is unused
 * This lets us account for the case where an NF has a manually specified id and we overwrite it
 * This function modifies next_instance_id to be the proper value
 */
static int
find_next_instance_id(void) {
        struct client *cl;
        while (next_instance_id < MAX_CLIENTS) {
                cl = &clients[next_instance_id];
                if (!is_valid_nf(cl))
                        break;
                next_instance_id++;
        }
        return next_instance_id;

}

/**
 * Set up a newly started NF
 * Assign it an ID (if it hasn't self-declared)
 * Store info struct in our internal list of clients
 * Returns 1 (TRUE) on successful start, 0 if there is an error (ID conflict)
 */
static inline int
start_new_nf(struct onvm_nf_info *nf_info)
{
        //TODO dynamically allocate memory here - make rx/tx ring
        // take code from init_shm_rings in init.c
        // flush rx/tx queue at the this index to start clean?

        // if NF passed its own id on the command line, don't assign here
        // assume user is smart enough to avoid duplicates
        uint16_t nf_id = nf_info->instance_id == (uint16_t)NF_NO_ID
                ? next_instance_id++
                : nf_info->instance_id;

        if (nf_id >= MAX_CLIENTS) {
                // There are no more available IDs for this NF
                nf_info->status = NF_NO_IDS;
                return 0;
        }

        if (is_valid_nf(&clients[nf_id])) {
                // This NF is trying to declare an ID already in use
                nf_info->status = NF_ID_CONFLICT;
                return 0;
        }

        // Keep reference to this NF in the manager
        nf_info->instance_id = nf_id;
        clients[nf_id].info = nf_info;
        clients[nf_id].instance_id = nf_id;

        // Register this NF running within its service
        uint16_t service_count = nf_per_service_count[nf_info->service_id]++;
        service_to_nf[nf_info->service_id][service_count] = nf_id;

        // Let the NF continue its init process
        nf_info->status = NF_STARTING;
        return 1;
}

/**
 * Clean up after an NF has stopped
 * Remove references to soon-to-be-freed info struct
 * Clean up stats values
 */
static inline void
stop_running_nf(struct onvm_nf_info *nf_info)
{
        uint16_t nf_id = nf_info->instance_id;
        uint16_t service_id = nf_info->service_id;
        int mapIndex;
        struct rte_mempool *nf_info_mp;

        /* Clean up dangling pointers to info struct */
        clients[nf_id].info = NULL;

        /* Reset stats */
        clients[nf_id].stats.rx = clients[nf_id].stats.rx_drop = 0;
        clients[nf_id].stats.act_drop = clients[nf_id].stats.act_tonf = 0;
        clients[nf_id].stats.act_next = clients[nf_id].stats.act_out = 0;

        /* Remove this NF from the service map.
         * Need to shift all elements past it in the array left to avoid gaps */
        nf_per_service_count[service_id]--;
        for(mapIndex = 0; mapIndex < MAX_CLIENTS_PER_SERVICE; mapIndex++) {
                if (service_to_nf[service_id][mapIndex] == nf_id) {
                        break;
                }
        }

        if (mapIndex < MAX_CLIENTS_PER_SERVICE) { // sanity error check
                service_to_nf[service_id][mapIndex] = 0;
                for (mapIndex++; mapIndex < MAX_CLIENTS_PER_SERVICE - 1; mapIndex++) {
                        // Shift the NULL to the end of the array
                        if (service_to_nf[service_id][mapIndex + 1] == 0) {
                                // Short circuit when we reach the end of this service's list
                                break;
                        }
                        service_to_nf[service_id][mapIndex] = service_to_nf[service_id][mapIndex + 1];
                        service_to_nf[service_id][mapIndex + 1] = 0;
                }
        }

        /* Free info struct */
        /* Lookup mempool for nf_info struct */
        nf_info_mp = rte_mempool_lookup(_NF_MEMPOOL_NAME);
        if (nf_info_mp == NULL)
                return;

        rte_mempool_put(nf_info_mp, (void*)nf_info);
}

static void
do_check_new_nf_status(void) {
        int i;
        int added_clients;
        int removed_clients;
        void *new_nfs[MAX_CLIENTS];
        struct onvm_nf_info *nf;
        int num_new_nfs = rte_ring_count(nf_info_queue);
        int dequeue_val = rte_ring_dequeue_bulk(nf_info_queue, new_nfs, num_new_nfs);

        if (dequeue_val != 0)
                return;

        added_clients = 0;
        removed_clients = 0;
        for (i = 0; i < num_new_nfs; i++) {
                nf = (struct onvm_nf_info *)new_nfs[i];

                // Sets next_instance_id variable to next available
                find_next_instance_id();

                if (nf->status == NF_WAITING_FOR_ID) {
                        /* We're starting up a new NF.
                         * Function returns TRUE on successful start */
                        if (start_new_nf(nf))
                                added_clients++;
                } else if (nf->status == NF_STOPPED) {
                        /* An existing NF is stopping */
                        stop_running_nf(nf);
                        removed_clients++;
                }
        }

        num_clients += added_clients;
        num_clients -= removed_clients;
}

/*
 * Stats thread periodically prints per-port and per-NF stats.
 */
static void
master_thread_main(void) {
        const unsigned sleeptime = 1;

        RTE_LOG(INFO, APP, "Core %d: Running master thread\n", rte_lcore_id());

        /* Longer initial pause so above printf is seen */
        sleep(sleeptime * 3);

        /* Loop forever: sleep always returns 0 or <= param */
        while (sleep(sleeptime) <= sleeptime) {
                do_check_new_nf_status();
                do_stats_display(sleeptime);
        }
}

/*
 * Function to set all the client statistic values to zero.
 */
static void
clear_stats(void) {
        unsigned i;

        for (i = 0; i < MAX_CLIENTS; i++) {
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
flush_nf_queue(struct thread_info *thread, uint16_t client) {
        uint16_t i;
        struct client *cl;

        if (thread->nf_rx_buf[client].count == 0)
                return;

        cl = &clients[client];

        // Ensure destination NF is running and ready to receive packets
        if (!is_valid_nf(cl))
                return;

        if (rte_ring_enqueue_bulk(cl->rx_q, (void **)thread->nf_rx_buf[client].buffer,
                        thread->nf_rx_buf[client].count) != 0) {
                for (i = 0; i < thread->nf_rx_buf[client].count; i++) {
                        rte_pktmbuf_free(thread->nf_rx_buf[client].buffer[i]);
                }
                cl->stats.rx_drop += thread->nf_rx_buf[client].count;
        } else {
                cl->stats.rx += thread->nf_rx_buf[client].count;
        }
        thread->nf_rx_buf[client].count = 0;
}

/**
 * Send a burst of packets out a NIC port.
 */
static void
flush_port_queue(struct thread_info *tx, uint16_t port) {
        uint16_t i, sent;
        volatile struct tx_stats *tx_stats;

        if (tx->port_tx_buf[port].count == 0)
                return;

        tx_stats = &(ports->tx_stats);
        sent = rte_eth_tx_burst(port, tx->queue_id, tx->port_tx_buf[port].buffer, tx->port_tx_buf[port].count);
        if (unlikely(sent < tx->port_tx_buf[port].count)) {
                for (i = sent; i < tx->port_tx_buf[port].count; i++) {
                        rte_pktmbuf_free(tx->port_tx_buf[port].buffer[i]);
                }
                tx_stats->tx_drop[port] += (tx->port_tx_buf[port].count - sent);
        }
        tx_stats->tx[port] += sent;

        tx->port_tx_buf[port].count = 0;
}

/**
 * This function take a service id input and returns an instance id to route the packet to
 * This uses the packet's RSS Hash mod the number of available services to decide
 * Returns 0 (manager reserved ID) if no NFs are available from the desired service
 */
static inline uint16_t
service_to_nf_map(uint16_t service_id, struct rte_mbuf *pkt) {
        uint16_t num_nfs_available = nf_per_service_count[service_id];

        if (num_nfs_available == 0)
                return 0;

        uint16_t instance_index = pkt->hash.rss % num_nfs_available;
        uint16_t instance_id = service_to_nf[service_id][instance_index];
        return instance_id;
}

/**
 * Add a packet to a buffer destined for an NF's RX queue.
 */
static inline void
enqueue_nf_packet(struct thread_info *thread, uint16_t dst_service_id, struct rte_mbuf *pkt) {
        struct client *cl;
        uint16_t dst_instance_id;

        // map service to instance and check one exists
        dst_instance_id = service_to_nf_map(dst_service_id, pkt);
        if (dst_instance_id == 0)
                return;

        // Ensure destination NF is running and ready to receive packets
        cl = &clients[dst_instance_id];
        if (!is_valid_nf(cl))
                return;

        thread->nf_rx_buf[dst_instance_id].buffer[thread->nf_rx_buf[dst_instance_id].count++] = pkt;
        if (thread->nf_rx_buf[dst_instance_id].count == PACKET_READ_SIZE) {
                flush_nf_queue(thread, dst_instance_id);
        }
}

/**
 * Add a packet to a buffer destined for a port's TX queue.
 */
static inline void
enqueue_port_packet(struct thread_info *tx, uint16_t port, struct rte_mbuf *buf) {
	if (port != ONVM_PORT_BROADCAST) {
        	tx->port_tx_buf[port].buffer[tx->port_tx_buf[port].count++] = buf;
        	if (tx->port_tx_buf[port].count == PACKET_READ_SIZE) {
        	        flush_port_queue(tx, port);
        	}
	} else {
		int i;

		for (i = 0; i < rte_eth_dev_count(); i++) {
        		tx->port_tx_buf[i].buffer[tx->port_tx_buf[i].count++] = buf;
        		if (tx->port_tx_buf[i].count == PACKET_READ_SIZE) {
        	        	flush_port_queue(tx, i);
        		}
		}
	}
}

/*
 * Process a packet with action NEXT
 */
static inline void
process_next_action_packet(struct thread_info *tx, struct rte_mbuf *pkt, struct client *cl) {
	struct onvm_flow_entry *flow_entry;
	struct onvm_service_chain *sc;
	struct onvm_pkt_meta *meta = onvm_get_pkt_meta(pkt);
	int ret;

	ret = onvm_flow_dir_get_pkt(pkt, &flow_entry);
	if (ret >= 0) {
		sc = flow_entry->sc;
		meta->action = onvm_sc_next_action(sc, pkt);
		meta->destination = onvm_sc_next_destination(sc, pkt);
	}
	else {
		meta->action = onvm_sc_next_action(default_chain, pkt);
		meta->destination = onvm_sc_next_destination(default_chain, pkt);
	}

	switch(meta->action) {
		case ONVM_NF_ACTION_DROP:
                        cl->stats.act_drop++;
			rte_pktmbuf_free(pkt);
			break;
		case ONVM_NF_ACTION_TONF:
                        cl->stats.act_tonf++;
			enqueue_nf_packet(tx, meta->destination, pkt);
			break;
		case ONVM_NF_ACTION_OUT:
                        cl->stats.act_out++;
			enqueue_port_packet(tx, meta->destination, pkt);
			break;
		default:
			break;
	}
	(meta->chain_index)++;
}

/*
 * This function takes a group of packets and routes them
 * to the first client process. Simply forwarding the packets
 * without checking any of the packet contents.
 */
static void
process_rx_packet_batch(struct thread_info *rx, struct rte_mbuf *pkts[], uint16_t rx_count) {
        uint16_t i;
        struct onvm_pkt_meta *meta;
	struct onvm_flow_entry *flow_entry;
	struct onvm_service_chain *sc;
	int ret;

        for (i = 0; i < rx_count; i++) {
                meta = (struct onvm_pkt_meta*) &(((struct rte_mbuf*)pkts[i])->udata64);
		(void)meta;
                meta->src = 0;
                meta->chain_index = 0;
		ret = onvm_flow_dir_get_pkt(pkts[i], &flow_entry);
		if (ret >= 0) {
			sc = flow_entry->sc;
			meta->action = onvm_sc_next_action(sc, pkts[i]);
			meta->destination = onvm_sc_next_destination(sc, pkts[i]);
		}
		else {
			meta->action = onvm_sc_next_action(default_chain, pkts[i]);
			meta->destination = onvm_sc_next_destination(default_chain, pkts[i]);
		}
                /* PERF: this might hurt performance since it will cause cache
                 * invalidations. Ideally the data modified by the NF manager
                 * would be a different line than that modified/read by NFs.
                 * That may not be possible.
                 */

                (meta->chain_index)++;
		enqueue_nf_packet(rx, meta->destination, pkts[i]);
        }

	for (i = 0; i < MAX_CLIENTS; i++) {
		if (rx->nf_rx_buf[i].count != 0) {
			flush_nf_queue(rx, i);
		}
	}
}

/*
 * Handle the packets from a client. Check what the next action is
 * and forward the packet either to the NIC or to another NF Client.
 */
static void
process_tx_packet_batch(struct thread_info *tx, struct rte_mbuf *pkts[], uint16_t tx_count, struct client *cl) {
        uint16_t i;
        struct onvm_pkt_meta *meta;

        for (i = 0; i < tx_count; i++) {
                meta = (struct onvm_pkt_meta*) &(((struct rte_mbuf*)pkts[i])->udata64);
                meta->src = cl->instance_id;
                if (meta->action == ONVM_NF_ACTION_DROP) {
                        rte_pktmbuf_free(pkts[i]);
                        cl->stats.act_drop++;
                } else if (meta->action == ONVM_NF_ACTION_NEXT) {
                        /* TODO: Here we drop the packet : there will be a flow table
                        in the future to know what to do with the packet next */
                        cl->stats.act_next++;
			process_next_action_packet(tx, pkts[i], cl);
                } else if (meta->action == ONVM_NF_ACTION_TONF) {
                        cl->stats.act_tonf++;
                        enqueue_nf_packet(tx, meta->destination, pkts[i]);
                } else if (meta->action == ONVM_NF_ACTION_OUT) {
                        cl->stats.act_out++;
                        enqueue_port_packet(tx, meta->destination, pkts[i]);
                } else {
                        printf("ERROR invalid action : this shouldn't happen.\n");
                        rte_pktmbuf_free(pkts[i]);
                        return;
                }
        }
}

/*
 * Function to receive packets from the NIC
 * and distribute them to the default service
 */
static int
rx_thread_main(void *arg) {
        uint16_t i, rx_count;
        struct rte_mbuf *pkts[PACKET_READ_SIZE];
        struct thread_info *rx = (struct thread_info*)arg;

        RTE_LOG(INFO, APP, "Core %d: Running RX thread for RX queue %d\n", rte_lcore_id(), rx->queue_id);

        for (;;) {
                /* Read ports */
                for (i = 0; i < ports->num_ports; i++) {
                        rx_count = rte_eth_rx_burst(ports->id[i], rx->queue_id, \
                                        pkts, PACKET_READ_SIZE);
                        ports->rx_stats.rx[ports->id[i]] += rx_count;

                        /* Now process the NIC packets read */
                        if (likely(rx_count > 0)) {
                                process_rx_packet_batch(rx, pkts, rx_count);
                        }
                }
        }

        return 0;
}


static int
tx_thread_main(void *arg) {
        struct client *cl;
        unsigned i, tx_count;
        struct rte_mbuf *pkts[PACKET_READ_SIZE];
        struct thread_info* tx = (struct thread_info*)arg;

        if (tx->first_cl == tx->last_cl - 1) {
                RTE_LOG(INFO, APP, "Core %d: Running TX thread for NF %d\n", rte_lcore_id(), tx->first_cl);
        } else if (tx->first_cl < tx->last_cl) {
                RTE_LOG(INFO, APP, "Core %d: Running TX thread for NFs %d to %d\n", rte_lcore_id(), tx->first_cl, tx->last_cl-1);
        }

        for (;;) {
                /* Read packets from the client's tx queue and process them as needed */
                for (i = tx->first_cl; i < tx->last_cl; i++) {
                        tx_count = PACKET_READ_SIZE;
                        cl = &clients[i];
                        if (!is_valid_nf(cl))
                                continue;
                        /* try dequeuing max possible packets first, if that fails, get the
                         * most we can. Loop body should only execute once, maximum */
                        while (tx_count > 0 &&
                                unlikely(rte_ring_dequeue_bulk(cl->tx_q, (void **) pkts, tx_count) != 0)) {
                                tx_count = (uint16_t)RTE_MIN(rte_ring_count(cl->tx_q),
                                        PACKET_READ_SIZE);
                        }

                        /* Now process the Client packets read */
                        if (likely(tx_count > 0)) {
                                process_tx_packet_batch(tx, pkts, tx_count, cl);
                        }
                }

                /* Send a burst to every port */
                for (i = 0; i < ports->num_ports; i++) {
                       if (tx->port_tx_buf[i].count != 0) {
                               flush_port_queue(tx, i);
                       }
                }

                /* Send a burst to every client */
                for (i = 0; i < MAX_CLIENTS; i++) {
                       if(tx->nf_rx_buf[i].count != 0) {
                               flush_nf_queue(tx, i);
                       }
                }
        }
        return 0;
}

int
main(int argc, char *argv[]) {
        unsigned cur_lcore, rx_lcores, tx_lcores;
        unsigned clients_per_tx, temp_num_clients;
        unsigned i;

        /* initialise the system */

        /* Reserve ID 0 for internal manager things */
        next_instance_id = 1;
        if (init(argc, argv) < 0 )
                return -1;
        RTE_LOG(INFO, APP, "Finished Process Init.\n");

        /* clear statistics */
        clear_stats();

        /* Reserve n cores for: 1 Stats, 1 final Tx out, and ONVM_NUM_RX_THREADS for Rx */
        cur_lcore = rte_lcore_id();
        rx_lcores = ONVM_NUM_RX_THREADS;
        tx_lcores = rte_lcore_count() - rx_lcores - 1;

        /* Offset cur_lcore to start assigning TX cores */
        cur_lcore += (rx_lcores-1);

        RTE_LOG(INFO, APP, "%d cores available in total\n", rte_lcore_count());
        RTE_LOG(INFO, APP, "%d cores available for handling manager RX queues\n", rx_lcores);
        RTE_LOG(INFO, APP, "%d cores available for handling TX queues\n", tx_lcores);
        RTE_LOG(INFO, APP, "%d cores available for handling stats\n", 1);

        /* Evenly assign NFs to TX threads */

        /*
         * If num clients is zero, then we are running in dynamic NF mode.
         * We do not have a way to tell the total number of NFs running so
         * we have to calculate clients_per_tx using MAX_CLIENTS then.
         * We want to distribute the number of running NFs across available
         * TX threads
         */
        if (num_clients == 0) clients_per_tx = ceil((float)MAX_CLIENTS/tx_lcores), temp_num_clients = (unsigned)MAX_CLIENTS;
        else clients_per_tx = ceil((float)num_clients/tx_lcores), temp_num_clients = (unsigned)num_clients;

        for (i = 0; i < tx_lcores; i++) {
                struct thread_info *tx = calloc(1,sizeof(struct thread_info));
                tx->queue_id = i;
                tx->port_tx_buf = calloc(RTE_MAX_ETHPORTS, sizeof(struct packet_buf));
                tx->nf_rx_buf = calloc(MAX_CLIENTS, sizeof(struct packet_buf));
                tx->first_cl = RTE_MIN(i * clients_per_tx + 1, temp_num_clients);
                tx->last_cl = RTE_MIN((i+1) * clients_per_tx + 1, temp_num_clients);
                cur_lcore = rte_get_next_lcore(cur_lcore, 1, 1);
                if (rte_eal_remote_launch(tx_thread_main, (void*)tx,  cur_lcore) == -EBUSY) {
                        RTE_LOG(ERR, APP, "Core %d is already busy, can't use for client %d TX\n", cur_lcore, tx->first_cl);
                        return -1;
                }
        }

        /* Launch RX thread main function for each RX queue on cores */
        for (i = 0; i < rx_lcores; i++) {
                struct thread_info *rx = calloc(1, sizeof(struct thread_info));
                rx->queue_id = i;
		rx->port_tx_buf = NULL;
		rx->nf_rx_buf = calloc(MAX_CLIENTS, sizeof(struct packet_buf));
                cur_lcore = rte_get_next_lcore(cur_lcore, 1, 1);
                if (rte_eal_remote_launch(rx_thread_main, (void *)rx, cur_lcore) == -EBUSY) {
                        RTE_LOG(ERR, APP, "Core %d is already busy, can't use for RX queue id %d\n", cur_lcore, rx->queue_id);
                        return -1;
                }
        }

        /* Master thread handles statistics and NF management */
        master_thread_main();
        return 0;
}
