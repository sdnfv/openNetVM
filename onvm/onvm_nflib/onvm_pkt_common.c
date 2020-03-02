/*********************************************************************
 *                     openNetVM
 *              https://sdnfv.github.io
 *
 *   BSD LICENSE
 *
 *   Copyright(c)
 *            2015-2019 George Washington University
 *            2015-2019 University of California Riverside
 *            2010-2019 Intel Corporation. All rights reserved.
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
                                 onvm_pkt_common.c

            This file contains all functions related to receiving or
            transmitting packets.

******************************************************************************/

#include "onvm_pkt_common.h"

/**********************Internal Functions Prototypes**************************/

/*
 * Function to enqueue a packet on one port's queue.
 *
 * Inputs : a pointer to the tx queue responsible
 *          the number of the port
 *          a pointer to the packet
 *
 */
static inline void
onvm_pkt_enqueue_port(struct queue_mgr *tx_mgr, uint16_t port, struct rte_mbuf *buf);

/*
 * Function to process a single packet.
 *
 * Inputs : a pointer to the tx queue responsible
 *          a pointer to the packet
 *          a pointer to the NF involved
 *
 */
static inline void
onvm_pkt_process_next_action(struct queue_mgr *tx_mgr, struct rte_mbuf *pkt, struct onvm_nf *nf);

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
onvm_pkt_process_tx_batch(struct queue_mgr *tx_mgr, struct rte_mbuf *pkts[], uint16_t tx_count, struct onvm_nf *nf) {
        uint16_t i;
        struct onvm_pkt_meta *meta;
        struct packet_buf *out_buf;

        if (tx_mgr == NULL || pkts == NULL || nf == NULL)
                return;

        for (i = 0; i < tx_count; i++) {
                meta = (struct onvm_pkt_meta *)&(((struct rte_mbuf *)pkts[i])->udata64);
                meta->src = nf->instance_id;
                if (meta->action == ONVM_NF_ACTION_DROP) {
                        // if the packet is drop, then <return value> is 0
                        // and !<return value> is 1.
                        nf->stats.act_drop++;
                        nf->stats.tx += !onvm_pkt_drop(pkts[i]);
                } else if (meta->action == ONVM_NF_ACTION_NEXT) {
                        /* TODO: Here we drop the packet : there will be a flow table
                        in the future to know what to do with the packet next */
                        nf->stats.act_next++;
                        onvm_pkt_process_next_action(tx_mgr, pkts[i], nf);
                } else if (meta->action == ONVM_NF_ACTION_TONF) {
                        nf->stats.act_tonf++;
                        onvm_pkt_enqueue_nf(tx_mgr, meta->destination, pkts[i], nf);
                } else if (meta->action == ONVM_NF_ACTION_OUT) {
                        nf->stats.act_out++;
                        if (tx_mgr->mgr_type_t == MGR) {
                                onvm_pkt_enqueue_port(tx_mgr, meta->destination, pkts[i]);
                        } else {
                                out_buf = tx_mgr->to_tx_buf;
                                out_buf->buffer[out_buf->count++] = pkts[i];
                                if (out_buf->count == PACKET_READ_SIZE) {
                                        onvm_pkt_enqueue_tx_thread(out_buf, nf);
                                }
                        }
                } else {
                        printf("ERROR invalid action : this shouldn't happen.\n");
                        onvm_pkt_drop(pkts[i]);
                        return;
                }
        }
}

void
onvm_pkt_flush_all_nfs(struct queue_mgr *tx_mgr, struct onvm_nf *source_nf) {
        uint16_t i;

        if (tx_mgr == NULL)
                return;

        for (i = 0; i < MAX_NFS; i++)
                onvm_pkt_flush_nf_queue(tx_mgr, i, source_nf);
}

void
onvm_pkt_flush_nf_queue(struct queue_mgr *tx_mgr, uint16_t nf_id, struct onvm_nf *source_nf) {
        uint16_t i;
        struct onvm_nf *nf;
        struct packet_buf *nf_buf;

        if (tx_mgr == NULL)
                return;

        nf_buf = &tx_mgr->nf_rx_bufs[nf_id];
        if (nf_buf->count == 0)
                return;

        nf = &nfs[nf_id];

        // Ensure destination NF is running and ready to receive packets
        if (!onvm_nf_is_valid(nf))
                return;

        if (rte_ring_enqueue_bulk(nf->rx_q, (void **)nf_buf->buffer, nf_buf->count, NULL) == 0) {
                for (i = 0; i < nf_buf->count; i++) {
                        onvm_pkt_drop(nf_buf->buffer[i]);
                }
                nf->stats.rx_drop += nf_buf->count;
                if (source_nf != NULL)
                        source_nf->stats.tx_drop += nf_buf->count;
        } else {
                nf->stats.rx += nf_buf->count;
                if (source_nf != NULL)
                        source_nf->stats.tx += nf_buf->count;
        }
        nf_buf->count = 0;
}

void
onvm_pkt_enqueue_nf(struct queue_mgr *tx_mgr, uint16_t dst_service_id, struct rte_mbuf *pkt,
                    struct onvm_nf *source_nf) {
        struct onvm_nf *nf;
        uint16_t dst_instance_id;
        struct packet_buf *nf_buf;

        if (tx_mgr == NULL || pkt == NULL)
                return;

        // map service to instance and check one exists
        dst_instance_id = onvm_sc_service_to_nf_map(dst_service_id, pkt);
        if (dst_instance_id == 0) {
                onvm_pkt_drop(pkt);
                if (source_nf != NULL)
                        source_nf->stats.tx_drop++;
                return;
        }

        // Ensure destination NF is running and ready to receive packets
        nf = &nfs[dst_instance_id];
        if (!onvm_nf_is_valid(nf)) {
                onvm_pkt_drop(pkt);
                if (source_nf != NULL)
                        source_nf->stats.tx_drop++;
                return;
        }

        nf_buf = &tx_mgr->nf_rx_bufs[dst_instance_id];
        nf_buf->buffer[nf_buf->count++] = pkt;
        if (nf_buf->count == PACKET_READ_SIZE) {
                onvm_pkt_flush_nf_queue(tx_mgr, dst_instance_id, source_nf);
        }
}

void
onvm_pkt_flush_port_queue(struct queue_mgr *tx_mgr, uint16_t port) {
        uint16_t i, sent;
        volatile struct tx_stats *tx_stats;
        struct packet_buf *port_buf;

        if (tx_mgr == NULL || tx_mgr->mgr_type_t != MGR)
                return;

        port_buf = &tx_mgr->tx_thread_info->port_tx_bufs[port];
        if (port_buf->count == 0)
                return;

        tx_stats = &(ports->tx_stats);
        sent = rte_eth_tx_burst(port, tx_mgr->id, port_buf->buffer, port_buf->count);
        if (unlikely(sent < port_buf->count)) {
                for (i = sent; i < port_buf->count; i++) {
                        onvm_pkt_drop(port_buf->buffer[i]);
                }
                tx_stats->tx_drop[port] += (port_buf->count - sent);
        }
        tx_stats->tx[port] += sent;

        port_buf->count = 0;
}

void
onvm_pkt_enqueue_tx_thread(struct packet_buf *pkt_buf, struct onvm_nf *nf) {
        uint16_t i;

        if (pkt_buf->count == 0)
                return;

        if (unlikely(pkt_buf->count > 0 &&
                     rte_ring_enqueue_bulk(nf->tx_q, (void **)pkt_buf->buffer, pkt_buf->count, NULL) == 0)) {
                nf->stats.tx_drop += pkt_buf->count;
                for (i = 0; i < pkt_buf->count; i++) {
                        rte_pktmbuf_free(pkt_buf->buffer[i]);
                }
        } else {
                nf->stats.tx += pkt_buf->count;
        }
        pkt_buf->count = 0;
}

/****************************Internal functions*******************************/

inline static void
onvm_pkt_enqueue_port(struct queue_mgr *tx_mgr, uint16_t port, struct rte_mbuf *buf) {
        struct packet_buf *port_buf;

        if (tx_mgr == NULL || buf == NULL || !ports->init[port])
                return;

        port_buf = &tx_mgr->tx_thread_info->port_tx_bufs[port];
        port_buf->buffer[port_buf->count++] = buf;
        if (port_buf->count == PACKET_READ_SIZE) {
                onvm_pkt_flush_port_queue(tx_mgr, port);
        }
}

inline static void
onvm_pkt_process_next_action(struct queue_mgr *tx_mgr, struct rte_mbuf *pkt, struct onvm_nf *nf) {
        if (tx_mgr == NULL || pkt == NULL || nf == NULL)
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
                        nf->stats.act_drop += !onvm_pkt_drop(pkt);
                        break;
                case ONVM_NF_ACTION_TONF:
                        nf->stats.act_tonf++;
                        onvm_pkt_enqueue_nf(tx_mgr, meta->destination, pkt, nf);
                        break;
                case ONVM_NF_ACTION_OUT:
                        nf->stats.act_out++;
                        onvm_pkt_enqueue_port(tx_mgr, meta->destination, pkt);
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
