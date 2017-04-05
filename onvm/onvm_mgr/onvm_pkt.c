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
 ********************************************************************/


/******************************************************************************
                                 onvm_pkt.c

            This file contains all functions related to receiving or
            transmitting packets.

******************************************************************************/


#include "onvm_mgr.h"
#include "onvm_pkt.h"
#include "onvm_nf.h"



/**********************Internal Functions Prototypes**************************/


/*
 * Function to send packets to one port after processing them.
 *
 * Input : a pointer to the tx queue
 *
 */
static void
onvm_pkt_flush_port_queue(struct thread_info *tx, uint16_t port);


/*
 * Function to send packets to one NF after processing them.
 *
 * Input : a pointer to the tx queue
 *
 */
static void
onvm_pkt_flush_nf_queue(struct thread_info *thread, uint16_t client);


/*
 * Function to enqueue a packet on one port's queue.
 *
 * Inputs : a pointer to the tx queue responsible
 *          the number of the port
 *          a pointer to the packet
 *
 */
inline static void
onvm_pkt_enqueue_port(struct thread_info *tx, uint16_t port, struct rte_mbuf *buf);


/*
 * Function to enqueue a packet on one NF's queue.
 *
 * Inputs : a pointer to the tx queue responsible
 *          the number of the port
 *          a pointer to the packet
 *
 */
inline static void
onvm_pkt_enqueue_nf(struct thread_info *thread, uint16_t dst_service_id, struct rte_mbuf *pkt);


/*
 * Function to process a single packet.
 *
 * Inputs : a pointer to the tx queue responsible
 *          a pointer to the packet
 *          a pointer to the NF involved
 *
 */
inline static void
onvm_pkt_process_next_action(struct thread_info *tx, struct rte_mbuf *pkt, struct client *cl);


/*
 * Helper function to drop a packet.
 *
 * Input : a pointer to the packet
 *
 * Ouput : an error code
 *
 */
static int
onvm_pkt_drop(struct rte_mbuf *pkt);


/**********************************Interfaces*********************************/


void
onvm_pkt_process_rx_batch(struct thread_info *rx, struct rte_mbuf *pkts[], uint16_t rx_count) {
        uint16_t i;
        struct onvm_pkt_meta *meta;
        struct onvm_flow_entry *flow_entry;
        struct onvm_service_chain *sc;
        int ret;

        if (rx == NULL || pkts == NULL)
                return;

        for (i = 0; i < rx_count; i++) {
                meta = (struct onvm_pkt_meta*) &(((struct rte_mbuf*)pkts[i])->udata64);
                meta->src = 0;
                meta->chain_index = 0;
                ret = onvm_flow_dir_get_pkt(pkts[i], &flow_entry);
                if (ret >= 0) {
                        sc = flow_entry->sc;
                        meta->action = onvm_sc_next_action(sc, pkts[i]);
                        meta->destination = onvm_sc_next_destination(sc, pkts[i]);
                } else {
                        meta->action = onvm_sc_next_action(default_chain, pkts[i]);
                        meta->destination = onvm_sc_next_destination(default_chain, pkts[i]);
                }
                /* PERF: this might hurt performance since it will cause cache
                 * invalidations. Ideally the data modified by the NF manager
                 * would be a different line than that modified/read by NFs.
                 * That may not be possible.
                 */

                (meta->chain_index)++;
                onvm_pkt_enqueue_nf(rx, meta->destination, pkts[i]);
        }

        onvm_pkt_flush_all_nfs(rx);
}


void
onvm_pkt_process_tx_batch(struct thread_info *tx, struct rte_mbuf *pkts[], uint16_t tx_count, struct client *cl) {
        uint16_t i;
        struct onvm_pkt_meta *meta;

        if (tx == NULL || pkts == NULL || cl == NULL)
                return;

        for (i = 0; i < tx_count; i++) {
                meta = (struct onvm_pkt_meta*) &(((struct rte_mbuf*)pkts[i])->udata64);
                meta->src = cl->instance_id;
                if (meta->action == ONVM_NF_ACTION_DROP) {
                        // if the packet is drop, then <return value> is 0
                        // and !<return value> is 1.
                        cl->stats.act_drop += !onvm_pkt_drop(pkts[i]);
                } else if (meta->action == ONVM_NF_ACTION_NEXT) {
                        /* TODO: Here we drop the packet : there will be a flow table
                        in the future to know what to do with the packet next */
                        cl->stats.act_next++;
                        onvm_pkt_process_next_action(tx, pkts[i], cl);
                } else if (meta->action == ONVM_NF_ACTION_TONF) {
                        cl->stats.act_tonf++;
                        onvm_pkt_enqueue_nf(tx, meta->destination, pkts[i]);
                } else if (meta->action == ONVM_NF_ACTION_OUT) {
                        cl->stats.act_out++;
                        onvm_pkt_enqueue_port(tx, meta->destination, pkts[i]);
                } else {
                        printf("ERROR invalid action : this shouldn't happen.\n");
                        onvm_pkt_drop(pkts[i]);
                        return;
                }
        }
}


void
onvm_pkt_flush_all_ports(struct thread_info *tx) {
        uint16_t i;

        if (tx == NULL)
                return;

        for (i = 0; i < ports->num_ports; i++)
                onvm_pkt_flush_port_queue(tx, ports->id[i]);
}


void
onvm_pkt_flush_all_nfs(struct thread_info *tx) {
        uint16_t i;

        if (tx == NULL)
                return;

        for (i = 0; i < MAX_CLIENTS; i++)
                onvm_pkt_flush_nf_queue(tx, i);
}

void
onvm_pkt_drop_batch(struct rte_mbuf **pkts, uint16_t size) {
        uint16_t i;

        if (pkts == NULL)
                return;

        for (i = 0; i < size; i++)
                rte_pktmbuf_free(pkts[i]);
}


/****************************Internal functions*******************************/


static void
onvm_pkt_flush_port_queue(struct thread_info *tx, uint16_t port) {
        uint16_t i, sent;
        volatile struct tx_stats *tx_stats;

        if (tx == NULL)
                return;

        if (tx->port_tx_buf[port].count == 0)
                return;

        tx_stats = &(ports->tx_stats);
        sent = rte_eth_tx_burst(port,
                                tx->queue_id,
                                tx->port_tx_buf[port].buffer,
                                tx->port_tx_buf[port].count);
        if (unlikely(sent < tx->port_tx_buf[port].count)) {
                for (i = sent; i < tx->port_tx_buf[port].count; i++) {
                        onvm_pkt_drop(tx->port_tx_buf[port].buffer[i]);
                }
                tx_stats->tx_drop[port] += (tx->port_tx_buf[port].count - sent);
        }
        tx_stats->tx[port] += sent;

        tx->port_tx_buf[port].count = 0;
}


static void
onvm_pkt_flush_nf_queue(struct thread_info *thread, uint16_t client) {
        uint16_t i;
        struct client *cl;

        if (thread == NULL)
                return;

        if (thread->nf_rx_buf[client].count == 0)
                return;

        cl = &clients[client];

        // Ensure destination NF is running and ready to receive packets
        if (!onvm_nf_is_valid(cl))
                return;

        if (rte_ring_enqueue_bulk(cl->rx_q, (void **)thread->nf_rx_buf[client].buffer,
                        thread->nf_rx_buf[client].count) != 0) {
                for (i = 0; i < thread->nf_rx_buf[client].count; i++) {
                        onvm_pkt_drop(thread->nf_rx_buf[client].buffer[i]);
                }
                cl->stats.rx_drop += thread->nf_rx_buf[client].count;
        } else {
                cl->stats.rx += thread->nf_rx_buf[client].count;
        }
        thread->nf_rx_buf[client].count = 0;
}


inline static void
onvm_pkt_enqueue_port(struct thread_info *tx, uint16_t port, struct rte_mbuf *buf) {

        if (tx == NULL || buf == NULL)
                return;


        tx->port_tx_buf[port].buffer[tx->port_tx_buf[port].count++] = buf;
        if (tx->port_tx_buf[port].count == PACKET_READ_SIZE) {
                onvm_pkt_flush_port_queue(tx, port);
        }
}


inline static void
onvm_pkt_enqueue_nf(struct thread_info *thread, uint16_t dst_service_id, struct rte_mbuf *pkt) {
        struct client *cl;
        uint16_t dst_instance_id;


        if (thread == NULL || pkt == NULL)
                return;

        // map service to instance and check one exists
        dst_instance_id = onvm_nf_service_to_nf_map(dst_service_id, pkt);
        if (dst_instance_id == 0) {
                onvm_pkt_drop(pkt);
                return;
        }

        // Ensure destination NF is running and ready to receive packets
        cl = &clients[dst_instance_id];
        if (!onvm_nf_is_valid(cl)) {
                onvm_pkt_drop(pkt);
                return;
        }

        thread->nf_rx_buf[dst_instance_id].buffer[thread->nf_rx_buf[dst_instance_id].count++] = pkt;
        if (thread->nf_rx_buf[dst_instance_id].count == PACKET_READ_SIZE) {
                onvm_pkt_flush_nf_queue(thread, dst_instance_id);
        }
}


inline static void
onvm_pkt_process_next_action(struct thread_info *tx, struct rte_mbuf *pkt, struct client *cl) {

        if (tx == NULL || pkt == NULL || cl == NULL)
                return;

        struct onvm_flow_entry *flow_entry;
        struct onvm_service_chain *sc;
        struct onvm_pkt_meta *meta = onvm_get_pkt_meta(pkt);
        int ret;

        ret = onvm_flow_dir_get_pkt(pkt, &flow_entry);
        if (ret >= 0) {
                sc = flow_entry->sc;
                meta->action = onvm_sc_next_action(sc, pkt);
                meta->destination = onvm_sc_next_destination(sc, pkt);
        } else {
                meta->action = onvm_sc_next_action(default_chain, pkt);
                meta->destination = onvm_sc_next_destination(default_chain, pkt);
        }

        switch (meta->action) {
                case ONVM_NF_ACTION_DROP:
                        // if the packet is drop, then <return value> is 0
                        // and !<return value> is 1.
                        cl->stats.act_drop += !onvm_pkt_drop(pkt);
                        break;
                case ONVM_NF_ACTION_TONF:
                        cl->stats.act_tonf++;
                        onvm_pkt_enqueue_nf(tx, meta->destination, pkt);
                        break;
                case ONVM_NF_ACTION_OUT:
                        cl->stats.act_out++;
                        onvm_pkt_enqueue_port(tx, meta->destination, pkt);
                        break;
                default:
                        break;
        }
        (meta->chain_index)++;
}


/*******************************Helper function*******************************/


static int
onvm_pkt_drop(struct rte_mbuf *pkt) {
        rte_pktmbuf_free(pkt);
        if (pkt != NULL) {
                return 1;
        }
        return 0;
}
