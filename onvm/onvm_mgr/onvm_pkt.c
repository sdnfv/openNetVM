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
 ********************************************************************/


/******************************************************************************
                                 onvm_pkt.c

            This file contains all functions related to receiving or
            transmitting packets.

******************************************************************************/


#include "onvm_mgr.h"
#include "onvm_pkt.h"
#include "onvm_nf.h"

#ifdef ENABLE_REMOTE_SYNC_WITH_TX_LATCH
#include "onvm_rsync.h"
#endif

/**********************************Interfaces*********************************/
//#define USE_KEY_MODE_FOR_FLOW_ENTRY       //Note: Enabling this flag is costing upto 4Mpps (reason: softrss() call)
static inline int get_flow_entry( struct rte_mbuf *pkt, struct onvm_flow_entry **flow_entry);
static inline int get_flow_entry( struct rte_mbuf *pkt, struct onvm_flow_entry **flow_entry) {
        int ret = -1;
        if(flow_entry)*flow_entry = NULL;
#ifdef USE_KEY_MODE_FOR_FLOW_ENTRY
        struct onvm_ft_ipv4_5tuple fk;
        if ((ret = onvm_ft_fill_key(&fk, pkt))) {
                return ret;
        }
        ret = onvm_flow_dir_get_key(&fk, flow_entry);
#else  // #elif defined (USE_KEY_MODE_FOR_FLOW_ENTRY)
        ret = onvm_flow_dir_get_pkt(pkt, flow_entry);
#endif
        return ret;
}

void
onvm_pkt_process_rx_batch(struct thread_info *rx, struct rte_mbuf *pkts[], uint16_t rx_count) {
        uint16_t i;
        struct onvm_pkt_meta *meta = NULL;
        struct onvm_flow_entry *flow_entry = NULL;

#if 0 //redundant checks
        if (rx == NULL || pkts == NULL)
                return;
#endif

        for (i = 0; i < rx_count; i++) {
                meta = (struct onvm_pkt_meta*) &(((struct rte_mbuf*)pkts[i])->udata64);
                meta->src = 0;
                meta->chain_index = 0;
                meta->reserved_word=0;

                get_flow_entry(pkts[i], &flow_entry);
                if (likely(flow_entry && flow_entry->sc )) {
                        meta->action = onvm_sc_next_action(flow_entry->sc, pkts[i]);
                        meta->destination = onvm_sc_next_destination(flow_entry->sc, pkts[i]);
#ifdef ENABLE_FT_INDEX_IN_META
                        meta->ft_index = (uint16_t)flow_entry->entry_index;
#endif
                } else {
                        meta->action = onvm_sc_next_action(default_chain, pkts[i]);
                        meta->destination = onvm_sc_next_destination(default_chain, pkts[i]);
                }
                /* PERF: this might hurt performance since it will cause cache
                 * invalidations. Ideally the data modified by the NF manager
                 * would be a different line than that modified/read by NFs.
                 * That may not be possible.
                 */

                //Fix: Note action for some flows can be configured to directly drop or send out on specific port
                switch (meta->action) {
                default:
                case ONVM_NF_ACTION_DROP:
                        onvm_pkt_drop(pkts[i]);
                        break;
                case ONVM_NF_ACTION_TO_NF_INSTANCE:
                case ONVM_NF_ACTION_TONF:
                        (meta->chain_index)++;
                        onvm_pkt_enqueue_nf(rx, pkts[i], meta, flow_entry);
                        break;
                case ONVM_NF_ACTION_OUT:
                        (meta->chain_index)++;
#ifdef ENABLE_CHAIN_BYPASS_RSYNC_ISOLATION
                        onvm_pkt_enqueue_port_v2(rx, meta->destination, pkts[i],meta,flow_entry);
#else
                        onvm_pkt_enqueue_port(rx, meta->destination, pkts[i]);
#endif
                        break;
                }
        }
        onvm_pkt_flush_all_nfs(rx);
}


void
onvm_pkt_process_tx_batch(struct thread_info *tx, struct rte_mbuf *pkts[], uint16_t tx_count, struct onvm_nf *cl) {
        uint16_t i;
        struct onvm_pkt_meta *meta = NULL;
        struct onvm_flow_entry *flow_entry = NULL;

        if (tx == NULL || pkts == NULL || cl == NULL)
                return;

        for (i = 0; i < tx_count; i++) {
                meta = (struct onvm_pkt_meta*) &(((struct rte_mbuf*)pkts[i])->udata64);
                meta->src = cl->instance_id;

                switch (meta->action) {
                case ONVM_NF_ACTION_DROP:
                        onvm_pkt_drop(pkts[i]);
#ifdef ENABLE_NF_TX_STAT_LOGS
                        cl->stats.act_drop++;
#endif
                        break;
                case ONVM_NF_ACTION_NEXT:
#ifdef ENABLE_NF_TX_STAT_LOGS
                        cl->stats.act_next++;
#endif
                        get_flow_entry(pkts[i], &flow_entry);
                        onvm_pkt_process_next_action(tx, pkts[i], meta, flow_entry, cl);
                        break;
                case ONVM_NF_ACTION_TO_NF_INSTANCE:
                case ONVM_NF_ACTION_TONF:
#ifdef ENABLE_NF_TX_STAT_LOGS
                        cl->stats.act_tonf++;
#endif
                        (meta->chain_index)++;
                        get_flow_entry(pkts[i], &flow_entry);
                        onvm_pkt_enqueue_nf(tx, pkts[i], meta, flow_entry);
                        break;
                case ONVM_NF_ACTION_OUT:
#ifdef ENABLE_NF_TX_STAT_LOGS
                        cl->stats.act_out++;
#endif
                        (meta->chain_index)++;
#ifdef ENABLE_CHAIN_BYPASS_RSYNC_ISOLATION
                        onvm_pkt_enqueue_port_v2(tx, meta->destination, pkts[i],meta,flow_entry);
#else
                        onvm_pkt_enqueue_port(tx, meta->destination, pkts[i]);
#endif
                        break;
                default:
                        printf("onvm_pkt_process_tx_batch(%d): ERROR invalid action: this shouldn't happen.\n", meta->action);
                        onvm_pkt_drop(pkts[i]);
                        break;
                }


               /*
               if (meta->action == ONVM_NF_ACTION_DROP) {
                        // if the packet is drop, then <return value> is 0 and !<return value> is 1.
                        onvm_pkt_drop(pkts[i]);
                        #ifdef ENABLE_NF_TX_STAT_LOGS
                        //cl->stats.act_drop += !onvm_pkt_drop(pkts[i]);
                        cl->stats.act_drop += 1;
                        #endif
                } else if (meta->action == ONVM_NF_ACTION_NEXT) {
                        #ifdef ENABLE_NF_TX_STAT_LOGS
                        cl->stats.act_next++;
                        #endif
                        get_flow_entry(pkts[i], &flow_entry);
                        onvm_pkt_process_next_action(tx, pkts[i], meta, flow_entry, cl);
                } else if (meta->action == ONVM_NF_ACTION_TONF || meta->action == ONVM_NF_ACTION_TO_NF_INSTANCE) {
                        #ifdef ENABLE_NF_TX_STAT_LOGS
                        cl->stats.act_tonf++;
                        #endif
                        (meta->chain_index)++;
                        get_flow_entry(pkts[i], &flow_entry);
                        onvm_pkt_enqueue_nf(tx, pkts[i], meta, flow_entry);
                } else if (meta->action == ONVM_NF_ACTION_OUT) {
                        #ifdef ENABLE_NF_TX_STAT_LOGS
                        cl->stats.act_out++;
                        #endif
                        onvm_pkt_enqueue_port(tx, meta->destination, pkts[i]);
                } else {
                        printf("ERROR invalid action : this shouldn't happen.\n");
                        onvm_pkt_drop(pkts[i]);
                        return;
                }
                */
        }
}


void
onvm_pkt_flush_all_ports(struct thread_info *tx) {
        uint16_t i;

#if 0 //redundant
        if (tx == NULL)
                return;
#endif

        for (i = 0; i < ports->num_ports; i++)
                onvm_pkt_flush_port_queue(tx, i);
}


void
onvm_pkt_flush_all_nfs(struct thread_info *tx) {
        uint16_t i;

#if 0 //redundant
        if (tx == NULL)
                return;
#endif

        for (i = 0; i < MAX_NFS; i++)
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


void
onvm_pkt_flush_port_queue(struct thread_info *tx, uint16_t port) {
        uint16_t i, sent;
#ifndef ENABLE_REMOTE_SYNC_WITH_TX_LATCH
        volatile struct tx_stats *tx_stats;
#endif

        if (unlikely(tx == NULL))
                return;

/**** START OF BYPASS ISOLATION ***/
#ifdef ENABLE_CHAIN_BYPASS_RSYNC_ISOLATION
                if(tx->port_tx_direct_buf[port].count) {
#ifdef PROFILE_PACKET_PROCESSING_LATENCY
        onvm_util_calc_chain_processing_latency(tx->port_tx_direct_buf[port].buffer, tx->port_tx_direct_buf[port].count);
#endif
                        volatile struct tx_stats *tx_stat = &(ports->tx_stats);
                        sent = rte_eth_tx_burst(port,
                                        tx->queue_id,
                                        tx->port_tx_direct_buf[port].buffer,
                                        tx->port_tx_direct_buf[port].count);
                        if (unlikely(sent < tx->port_tx_direct_buf[port].count)) {
                                for (i = sent; i < tx->port_tx_direct_buf[port].count; i++) {
                                        onvm_pkt_drop(tx->port_tx_direct_buf[port].buffer[i]);
                                }
#ifdef ENABLE_PORT_TX_STATS_LOGS
                                tx_stat->tx_drop[port] += (tx->port_tx_direct_buf[port].count - sent);
#endif
                        }
#ifdef ENABLE_PORT_TX_STATS_LOGS
                       tx_stat->tx[port] += sent;
#endif
                       tx->port_tx_direct_buf[port].count=0;
                }
#endif
/*****END OF BYPASS ISLOATION *****/


        if (unlikely(tx->port_tx_buf[port].count == 0))
                return;


#ifdef PROFILE_PACKET_PROCESSING_LATENCY
        //onvm_util_calc_chain_processing_latency(tx->port_tx_buf[port].buffer, tx->port_tx_buf[port].count);
#endif

#ifdef ENABLE_REMOTE_SYNC_WITH_TX_LATCH
        uint16_t enq_port = port;//0; //((port > ONVM_NUM_RSYNC_PORTS)?(ONVM_NUM_RSYNC_PORTS-1):(port));
        sent = rte_ring_enqueue_burst(tx_port_ring[enq_port], (void **)tx->port_tx_buf[port].buffer, tx->port_tx_buf[port].count,NULL);
        //sent = rte_ring_enqueue_burst(tx_port_ring, (void **)tx->port_tx_buf[port].buffer, tx->port_tx_buf[port].count);
        if (unlikely(sent < tx->port_tx_buf[port].count)) {
                for (i = sent; i < tx->port_tx_buf[port].count; i++) {
                        onvm_pkt_drop(tx->port_tx_buf[port].buffer[i]);
                }
#ifdef ENABLE_PORT_TX_STATS_LOGS
                rsync_stat.drop_count_tx_port_ring[enq_port] +=(tx->port_tx_buf[port].count - sent); //tx_stats->tx_drop[port] += (tx->port_tx_buf[port].count - sent);
#endif
        }
#ifdef ENABLE_PORT_TX_STATS_LOGS
        rsync_stat.enq_count_tx_port_ring[enq_port] += sent; //tx_stats->tx[port] += sent;
#endif

#else

#ifdef PROFILE_PACKET_PROCESSING_LATENCY
        onvm_util_calc_chain_processing_latency(tx->port_tx_buf[port].buffer, tx->port_tx_buf[port].count);
#endif
        tx_stats = &(ports->tx_stats);
        sent = rte_eth_tx_burst(port,
                                tx->queue_id,
                                tx->port_tx_buf[port].buffer,
                                tx->port_tx_buf[port].count);

        if (unlikely(sent < tx->port_tx_buf[port].count)) {
                for (i = sent; i < tx->port_tx_buf[port].count; i++) {
                        onvm_pkt_drop(tx->port_tx_buf[port].buffer[i]);
                }
#ifdef ENABLE_PORT_TX_STATS_LOGS
                tx_stats->tx_drop[port] += (tx->port_tx_buf[port].count - sent);
#endif
        }
#ifdef ENABLE_PORT_TX_STATS_LOGS
        tx_stats->tx[port] += sent;
#endif
#endif

        tx->port_tx_buf[port].count = 0;
}


void
onvm_pkt_flush_nf_queue(struct thread_info *thread, uint16_t client) {
        struct onvm_nf *cl;

        if (unlikely(thread == NULL))
                return;

        if (thread->nf_rx_buf[client].count == 0)
                return;

        cl = &nfs[client];

        // Ensure destination NF is running and ready to receive packets
        if (unlikely(!onvm_nf_is_valid(cl)))
                return;

        int enq_status = rte_ring_enqueue_bulk(cl->rx_q, (void **)thread->nf_rx_buf[client].buffer,
                                thread->nf_rx_buf[client].count,NULL);


#if defined(ENABLE_NF_BACKPRESSURE) || defined (ENABLE_ECN_CE)
        if (unlikely((0 == enq_status)||(rte_ring_count(cl->rx_q) >= CLIENT_QUEUE_RING_WATER_MARK_SIZE))) {

#ifdef ENABLE_ECN_CE    //Note: TODO:Better to mark ECN_CE ON DEQUEUE
                //if (-EDQUOT == enq_status) {
                //if (0 == enq_status) {
                        onvm_detect_and_set_ecn_ce(thread->nf_rx_buf[client].buffer, thread->nf_rx_buf[client].count, cl);
                //}
#endif

#ifdef ENABLE_NF_BASED_BKPR_MARKING
                onvm_set_back_pressure_v2(cl);
#else
                onvm_set_back_pressure(thread->nf_rx_buf[client].buffer, thread->nf_rx_buf[client].count, cl);
#endif
        }
#endif

        //if (unlikely(-ENOBUFS == enq_status)) { //old DPDK API
        if (unlikely(0 == enq_status)) { //new DPDK API
        // In case of Failure with NoBUFS hold on to the packets in the thread buffer, till they can be flushed, and rather drop new packets that need to be enqueued
#ifdef  DO_NOT_DROP_PKTS_ON_FLUSH_FOR_BOTTLENECK_NF
                //do nothing..
#else
                //drop the packets from this list
                uint16_t i;
                for (i = 0; i < thread->nf_rx_buf[client].count; i++) {
                       onvm_pkt_drop(thread->nf_rx_buf[client].buffer[i]);
                }
                cl->stats.rx_drop += thread->nf_rx_buf[client].count;
                thread->nf_rx_buf[client].count = 0;
#endif
        }
        else {
                cl->stats.rx += thread->nf_rx_buf[client].count;
                thread->nf_rx_buf[client].count = 0;
        }
}


inline void
onvm_pkt_enqueue_port(struct thread_info *tx, uint16_t port, struct rte_mbuf *buf) {

        if (tx == NULL || buf == NULL)
                return;
        //if(unlikely(port>= RTE_MAX_ETHPORTS)) return;
        if(unlikely(port >= ports->num_ports)) {
                onvm_pkt_drop(buf);
                return;
        }
        tx->port_tx_buf[port].buffer[tx->port_tx_buf[port].count++] = buf;
        if (tx->port_tx_buf[port].count == PACKET_READ_SIZE) {
                onvm_pkt_flush_port_queue(tx, port);
        }
}

inline void
onvm_pkt_enqueue_port_v2(struct thread_info *tx, uint16_t port, struct rte_mbuf *buf, __attribute__((unused)) struct onvm_pkt_meta *meta, __attribute__((unused)) struct onvm_flow_entry *flow_entry) {

        if (tx == NULL || buf == NULL)
                return;

        if(unlikely(port >= ports->num_ports)) {
                onvm_pkt_drop(buf);
                return;
        }
        //uint8_t bypass = 0;
#ifdef ENABLE_CHAIN_BYPASS_RSYNC_ISOLATION
        uint8_t bypass = meta->reserved_word&NF_BYPASS_RSYNC; // buf->port;
        if(bypass) {
                tx->port_tx_direct_buf[port].buffer[tx->port_tx_direct_buf[port].count++] = buf;
                if (tx->port_tx_direct_buf[port].count == PACKET_READ_SIZE) {
                        onvm_pkt_flush_port_queue(tx, port);
                }
                return;
        }
#endif

        tx->port_tx_buf[port].buffer[tx->port_tx_buf[port].count++] = buf;
        if (tx->port_tx_buf[port].count == PACKET_READ_SIZE) {
                onvm_pkt_flush_port_queue(tx, port);
        }
}

inline void
onvm_pkt_enqueue_nf(struct thread_info *thread, struct rte_mbuf *pkt, struct onvm_pkt_meta *meta, __attribute__((unused)) struct onvm_flow_entry *flow_entry) {
        struct onvm_nf *cl;
        uint16_t dst_instance_id;

        //redundant checks as this is a private call and ensures always fine.
#if 0
        if (thread == NULL || pkt == NULL || meta == NULL)
                return;
#endif

        if(likely(ONVM_NF_ACTION_TO_NF_INSTANCE == meta->action)) {
                dst_instance_id = meta->destination;
        } else {
                // map service id to instance id and check one exists
                dst_instance_id = onvm_nf_service_to_nf_map(meta->destination, pkt);
                if(unlikely(dst_instance_id >= MAX_NFS)) {
                        //printf("\n Dropping packet: dst_instance_id=[%d] meta->action[%d], meta->destination[%d]", dst_instance_id,meta->action, meta->destination);
                        onvm_pkt_drop(pkt);
                        return;
                }

                printf("\n Resolved packet: dst_instance_id=[%d] meta->action[%d], meta->destination[%d]", dst_instance_id,meta->action, meta->destination);
                if(likely (flow_entry && flow_entry->sc)) {
                        //update the action for this flow entry to use the instance mapping directly
                        onvm_sc_set_entry(flow_entry->sc, meta->chain_index, ONVM_NF_ACTION_TO_NF_INSTANCE, dst_instance_id, 0);
                } /* else {
                        onvm_sc_set_entry(default_chain, meta->chain_index, ONVM_NF_ACTION_TO_NF_INSTANCE, dst_instance_id, 0);
                } */
        }
#if 0
        if(unlikely(dst_instance_id >= MAX_NFS)) {
                //printf("\n Dropping packet: dst_instance_id=[%d] meta->action[%d], meta->destination[%d]", dst_instance_id,meta->action, meta->destination);
                onvm_pkt_drop(pkt);
                return;
        }
#endif
        // Ensure destination NF is running and ready to receive packets
        cl = &nfs[dst_instance_id];
        //if (unlikely(!onvm_nf_is_valid(cl)) || unlikely(onvm_nf_is_paused(cl))) {
        if(unlikely(!onvm_nf_is_processing(cl))){
                //Note: To address corner case:: if both primary and secondary are paused, then use primary NF to buffer and do not drop!
                //Issue with Make before break; <Race condition on packet processing on Tx path when both are active; Rx is not problem as NFs are scheduled on single core.; or is Rx the sole problem??
                //TODO: Ideal solution: Do the switch of NF instance ID in the control path when NF_START/STOP events occur; keep Data plane to purely forward to mapped instances.
#ifdef ENABLE_NFV_RESL
                //check if associated standby/active instance is available
                dst_instance_id = get_associated_active_or_standby_nf_id(dst_instance_id);
                cl = &nfs[dst_instance_id];
                if(unlikely(!onvm_nf_is_valid(cl))) {
                        onvm_pkt_drop(pkt);
                        return;
                }
#if 0 //Moved this to onvm_nf_stop();
                //Primary will always be RUNNING; only Standby NF will go through RUNNING-->PAUSED-->RUNNING Cycles. Note: This needs to be done inc control plane when the Primary NF is moved to stop state.
                if(likely(is_secondary_active_nf_id(dst_instance_id)) && likely(onvm_nf_is_paused(cl))) {
                        cl->info->status ^=NF_PAUSED_BIT;
                }
#endif
                printf("\n Re-Resolved packet: dst_instance_id=[%d] meta->action[%d], meta->destination[%d]", dst_instance_id,meta->action, meta->destination);
                if(likely (flow_entry && flow_entry->sc)) {
                        //update the action for this flow entry to use the instance mapping directly
                        onvm_sc_set_entry(flow_entry->sc, meta->chain_index, ONVM_NF_ACTION_TO_NF_INSTANCE, dst_instance_id, 0);
                } /* else {
                        onvm_sc_set_entry(default_chain, meta->chain_index, ONVM_NF_ACTION_TO_NF_INSTANCE, dst_instance_id, 0);
                } */

                /* TODO: This is the place also to check/initiate chain migration
                 * When? Both active and Standby NFs are down
                 * How?
                 * Who performs?
                 * Synchronization?
                 * ) */
#else
                onvm_pkt_drop(pkt);
                return;
#endif
        }

        #ifdef ENABLE_NF_BACKPRESSURE
        // First regardless of the approach, fill in the NF MAP of service chain if not already done
        // second: if approach is throttle by buffer drop, check if this chain needs upstreams to drop and if this one such upstream NF, then drop packet and return.
        if (flow_entry && flow_entry->sc) {

#if defined(NF_BACKPRESSURE_APPROACH_2) || defined(ENABLE_NF_BASED_BKPR_MARKING)
                // this information is needed only for NF based throttling approach; packet drop approach is more in-line.
                if(flow_entry->sc->nf_instance_id[meta->chain_index] != (uint8_t)cl->instance_id)
                        flow_entry->sc->nf_instance_id[meta->chain_index] = (uint8_t)cl->instance_id;
#endif

#ifdef NF_BACKPRESSURE_APPROACH_1
                // We want to throttle the packets at the upstream only iff (a) the packet belongs to the service chain whose Downstream NF indicates overflow, (b) this NF is upstream component for the service chain, and not a downstream NF (c) this NF is marked for throttle
#ifdef DROP_PKTS_ONLY_AT_RX_ENQUEUE
                if ((flow_entry->sc->highest_downstream_nf_index_id) && (meta->chain_index == 1)) {
#else
                if ((flow_entry->sc->highest_downstream_nf_index_id) && (is_upstream_NF(flow_entry->sc->highest_downstream_nf_index_id, meta->chain_index))) {
#endif //DROP_PKTS_ONLY_AT_RX_ENQUEUE
                        onvm_pkt_drop(pkt);
                        cl->stats.bkpr_drop+=1;
                        return;
                }
#endif //NF_BACKPRESSURE_APPROACH_1
        }

        //global chain case (using def_chain and no flow_entry): action only for approach#1 to drop packets.
        #if defined(ENABLE_GLOBAL_BACKPRESSURE) && defined(NF_BACKPRESSURE_APPROACH_1)
        else if (downstream_nf_overflow) {
                #ifdef DROP_PKTS_ONLY_AT_RX_ENQUEUE
                if (cl->info != NULL && (meta->chain_index == 1)) {
                #else
                if (cl->info != NULL && is_upstream_NF(highest_downstream_nf_service_id,cl->info->service_id)) {
                #endif //DROP_PKTS_ONLY_AT_RX_ENQUEUE
                        onvm_pkt_drop(pkt);
                        cl->stats.bkpr_drop+=1;
                        throttle_count++;
                        return;
                }
        }
        #endif //ENABLE_GLOBAL_BACKPRESSURE && NF_BACKPRESSURE_APPROACH_1
        #endif //ENABLE_NF_BACKPRESSURE

#ifdef DO_NOT_DROP_PKTS_ON_FLUSH_FOR_BOTTLENECK_NF
        if (unlikely(thread->nf_rx_buf[dst_instance_id].count == PACKET_READ_SIZE)) {
                onvm_pkt_flush_nf_queue(thread, dst_instance_id);
                // if Flush failed and no buffers were cleared, then drop the current buffer
                if(unlikely(thread->nf_rx_buf[dst_instance_id].count == PACKET_READ_SIZE)) {
                        onvm_pkt_drop(pkt);
                        cl->stats.rx_drop+=1;
                        return;
                }
        }
        thread->nf_rx_buf[dst_instance_id].buffer[thread->nf_rx_buf[dst_instance_id].count++] = pkt;
#else
        thread->nf_rx_buf[dst_instance_id].buffer[thread->nf_rx_buf[dst_instance_id].count++] = pkt;
        if (thread->nf_rx_buf[dst_instance_id].count == PACKET_READ_SIZE) {
                onvm_pkt_flush_nf_queue(thread, dst_instance_id);
        }
#endif //DO_NOT_DROP_PKTS_ON_FLUSH_FOR_BOTTLENECK_NF
}

inline void
onvm_pkt_process_next_action(struct thread_info *tx, struct rte_mbuf *pkt, __attribute__((unused)) struct onvm_pkt_meta *meta,
                __attribute__((unused)) struct onvm_flow_entry *flow_entry, __attribute__((unused)) struct onvm_nf *cl) {

#if 0
        //redundant checks: ignore
        if (tx == NULL || pkt == NULL || meta == NULL || cl == NULL)
                        return;
#endif

        if (flow_entry) {
                meta->action = onvm_sc_next_action(flow_entry->sc, pkt);
                meta->destination = onvm_sc_next_destination(flow_entry->sc, pkt);
#ifdef ENABLE_FT_INDEX_IN_META
                meta->ft_index = (uint16_t)flow_entry->entry_index;
#endif
        } else {
                meta->action = onvm_sc_next_action(default_chain, pkt);
                meta->destination = onvm_sc_next_destination(default_chain, pkt);
        }

        switch (meta->action) {
                case ONVM_NF_ACTION_DROP:
                        onvm_pkt_drop(pkt);
#ifdef ENABLE_NF_TX_STAT_LOGS
                        cl->stats.act_drop++;
                        // if the packet is drop, then <return value> is 0 and !<return value> is 1.
                        //cl->stats.act_drop += !onvm_pkt_drop(pkt);
#endif
                        break;
                case ONVM_NF_ACTION_TO_NF_INSTANCE:
                case ONVM_NF_ACTION_TONF:
#ifdef ENABLE_NF_TX_STAT_LOGS
                        cl->stats.act_tonf++;
#endif
                        (meta->chain_index)++;
                        onvm_pkt_enqueue_nf(tx, pkt, meta, flow_entry);
                        break;
                case ONVM_NF_ACTION_OUT:
#ifdef ENABLE_NF_TX_STAT_LOGS
                        cl->stats.act_out++;
#endif
                        (meta->chain_index)++;
#ifdef ENABLE_CHAIN_BYPASS_RSYNC_ISOLATION
                        onvm_pkt_enqueue_port_v2(tx, meta->destination, pkt,meta,flow_entry);
#else
                        onvm_pkt_enqueue_port(tx, meta->destination, pkt);
#endif
                        break;
                default:
                        break;
        }
}

/*******************************Helper function*******************************/
inline int
onvm_pkt_drop(struct rte_mbuf *pkt) {
        rte_pktmbuf_free(pkt);
        if (pkt != NULL) {
                return 1;
        }
        return 0;
}


void
onvm_detect_and_set_ecn_ce(struct rte_mbuf *pkts[], uint16_t count, __attribute__((unused)) struct onvm_nf *cl) {
        uint16_t i;
#define CE_BITS ((uint8_t)0x03)
        struct ipv4_hdr* ip = NULL;
        for(i = 0; i < count; i++) {
                ip = onvm_pkt_ipv4_hdr(pkts[i]);
                if (likely(ip && (ip->next_proto_id == IP_PROTOCOL_TCP))) {
                        //printf("Before: [%d], After: [%d] After1: [%d]", ip->type_of_service, ip->type_of_service|CE_BITS, ip->type_of_service|(CE_BITS << 6));
                        ip->type_of_service |= (CE_BITS);   //verified this works
                }
        }
}

#ifdef ENABLE_NF_BACKPRESSURE

#ifdef ENABLE_SAVE_BACKLOG_FT_PER_NF
static inline void write_ft_to_cl_bft(struct onvm_nf *cl, struct onvm_flow_entry *flow_entry, uint16_t chain_index);
static inline void read_all_ft_frm_cl_bft(struct onvm_nf *cl);
static inline void write_ft_to_cl_bft(struct onvm_nf *cl, struct onvm_flow_entry *flow_entry, uint16_t  chain_index) {
#if defined(BACKPRESSURE_USE_RING_BUFFER_MODE)
        //if((cl->bft_list.w_h+1) == cl->bft_list.r_h) {
        if(((cl->bft_list.w_h+1)%cl->bft_list.max_len) == cl->bft_list.r_h) {
                printf("\n***** BFT_R TERMINATING DUE TO OVERFLOW OF RING BUFFER BOTTLENECK STORE!!****** \n");
                return; //exit(1);
        }
        cl->bft_list.bft[cl->bft_list.w_h].bft=flow_entry;
        cl->bft_list.bft[cl->bft_list.w_h].chain_index=chain_index;
        cl->bft_list.bft_count++; //cl->bft_list.bft[cl->bft_list.bft_count++]=flow_entry;
        //cl->bft_list.w_h = (((cl->bft_list.w_h) == cl->bft_list.max_len)?(0):(cl->bft_list.w_h+1));
        if((++(cl->bft_list.w_h)) == cl->bft_list.max_len) cl->bft_list.w_h=0;
#else
        uint16_t free_index = 0;
        while(cl->bft_list.bft[cl->bft_list.bft_count].bft != NULL) {
                cl->bft_list.bft_count++;
                free_index++;
                if(free_index ==  cl->bft_list.max_len) break;
                if (cl->bft_list.bft_count == cl->bft_list.max_len) cl->bft_list.bft_count=0; 
        }
        if(free_index < cl->bft_list.max_len) {
                if(cl->bft_list.bft[cl->bft_list.bft_count].bft != NULL) { printf("\n ************** OverWriting the Existing entry!!! Dangerous !!! ************** \n "); exit(3);}
                cl->bft_list.bft[cl->bft_list.bft_count].bft=flow_entry;
                cl->bft_list.bft[cl->bft_list.bft_count].chain_index=chain_index;
        }
        //else {
        //        printf("\n *****************  FAILED TO STORE THE OVERFLOW ENTRY AT NF: [%d] *****************\n ",cl->instance_id);
        //        exit(0);
        //}
#endif
        flow_entry->idle_timeout |= (1<< (chain_index-1));     // Note: overloading idle timeout for storing chain_index
        flow_entry->hard_timeout = cl->instance_id;            // Note: overloading hard_timeout for storing cl_instance id where it is last marked for bottleneck.
#if 0
        //if (cl->bft_list.bft_count >= NF_QUEUE_RINGSIZE) {printf("\n***** TERMINATING DUE TO OVERFLOW OF RING BUFFER BOTTLENECK STORE!!****** \n");  } //exit(1); continue;
                //cl->bft_list.bft[cl->bft_list.bft_count++]=flow_entry;
#endif
}
static inline void read_all_ft_frm_cl_bft(struct onvm_nf *cl) {

#if defined(BACKPRESSURE_USE_RING_BUFFER_MODE)
        if( cl->bft_list.w_h == cl->bft_list.r_h) return; //empty
        struct onvm_flow_entry *flow_entry = NULL;
        uint16_t chain_index = 0, deleted_nodes=0;
        uint16_t ttl_nodes = (cl->bft_list.w_h > cl->bft_list.r_h)? (cl->bft_list.w_h):(cl->bft_list.w_h+cl->bft_list.max_len-cl->bft_list.r_h);
        //uint16_t to_del=ttl_nodes;
        //printf("\n CQ[S]: W_h:[%u], r_h:[%u], lc:[%u], to_del:[%u], del_nd:[%u], ttl_nd:[%u]",cl->bft_list.w_h,cl->bft_list.r_h,cl->bft_list.bft_count, to_del, deleted_nodes, ttl_nodes);
        while(ttl_nodes--) {
                flow_entry  = cl->bft_list.bft[cl->bft_list.r_h].bft;
                chain_index = cl->bft_list.bft[cl->bft_list.r_h].chain_index; //flow_entry->idle_timeout;
                if(flow_entry) {
                        CLEAR_BIT(flow_entry->sc->highest_downstream_nf_index_id, chain_index);
                        cl->bft_list.bft[cl->bft_list.r_h].bft = NULL;
                        cl->bft_list.bft[cl->bft_list.r_h].chain_index = 0;
                        //cl->bft_list.r_h = (((cl->bft_list.r_h+1) == cl->bft_list.max_len)?(0):(cl->bft_list.r_h+1));
                        if((++(cl->bft_list.r_h)) == cl->bft_list.max_len) cl->bft_list.r_h=0;
                        cl->bft_list.bft_count--;
                        deleted_nodes++;
                }
        }
        //printf("\n CQ[E]: W_h:[%u], r_h:[%u], lc:[%u], to_del:[%u], del_nd:[%u], ttl_nd:[%d]",cl->bft_list.w_h,cl->bft_list.r_h,cl->bft_list.bft_count, to_del, deleted_nodes, (int)ttl_nodes);
#else
        struct onvm_flow_entry *flow_entry = NULL;
        unsigned rx_q_count=cl->bft_list.max_len;  //NF_QUEUE_RINGSIZE; //cl->bft_list.bft_count;
        uint16_t i =0;
        for(i=0; i<rx_q_count; ++i) {
                flow_entry = cl->bft_list.bft[i].bft; //cl->bft_list.bft[rx_q_count-1-i];
                if(NULL == flow_entry) continue;

                //CLEAR_BIT(flow_entry->sc->highest_downstream_nf_index_id, flow_entry->idle_timeout);
                CLEAR_BIT(flow_entry->sc->highest_downstream_nf_index_id, cl->bft_list.bft[i].chain_index);
                cl->bft_list.bft[i].bft = NULL;
                cl->bft_list.bft[i].chain_index = 0;
                if(cl->bft_list.bft_count) cl->bft_list.bft_count--;
        }
#endif
}
#endif //ENABLE_SAVE_BACKLOG_FT_PER_NF

#ifdef ENABLE_NF_BASED_BKPR_MARKING
void onvm_set_back_pressure_v2(struct onvm_nf *cl) {

        //check if already marked; then ignore the call; else mark now to avoid multiple writes.
        if(cl->is_bottleneck) {
                return ;
        }
        cl->is_bottleneck = 1;

#ifdef USE_BKPR_V2_IN_TIMER_MODE
        // Let the timer thread to take action.
        enqueu_nf_to_bottleneck_watch_list(cl->instance_id);
#else
        //take action inline
        onvm_mark_all_entries_for_bottleneck(cl->instance_id);
#endif
}
#endif

void
onvm_set_back_pressure(struct rte_mbuf *pkts[], uint16_t count, __attribute__((unused)) struct onvm_nf *cl) {
        /*** Make sure this function is called only on error status on rx_enqueue() ***/
        /*** Detect the NF Rx Buffer overflow and signal this NF instance in the service chain as bottlenecked -- source of back-pressure -- all NFs prior to this in chain must throttle (either not scheduler or drop packets). ***/

#ifdef ENABLE_NF_BASED_BKPR_MARKING
        onvm_set_back_pressure_v2(cl);
        return;
#endif

        #ifdef ENABLE_NF_BACKPRESSURE
        struct onvm_pkt_meta *meta = NULL;
        uint16_t i;
        struct onvm_flow_entry *flow_entry = NULL;
        //unsigned rx_q_count = rte_ring_count(cl->rx_q);

#if defined (BACKPRESSURE_EXTRA_DEBUG_LOGS)
        unsigned rx_q_count = rte_ring_count(cl->rx_q);
        cl->stats.max_rx_q_len =  (rx_q_count>cl->stats.max_rx_q_len)?(rx_q_count):(cl->stats.max_rx_q_len);
        unsigned tx_q_count = rte_ring_count(cl->tx_q);
        cl->stats.max_tx_q_len =  (tx_q_count>cl->stats.max_tx_q_len)?(tx_q_count):(cl->stats.max_tx_q_len);
#endif


        //Inside this function indicates NFs Rx buffer has exceeded water-mark

#ifdef ENABLE_GLOBAL_BACKPRESSURE
        /** global single chain scenario:: Note: This only works for the default chain case where service ID of chain is always in increasing order **/
        if(global_bkpr_mode) {
                downstream_nf_overflow = 1;
                SET_BIT(highest_downstream_nf_service_id, cl->info->service_id);//highest_downstream_nf_service_id = cl->info->service_id;
                return;
        }
#endif
        /** Flow Entry based/Per Service Chain classification scenario **/

        // Flow Entry mode
        for(i = 0; i < count; i++) {
                get_flow_entry(pkts[i], &flow_entry); //int ret = get_flow_entry(pkts[i], &flow_entry);
                if (flow_entry && flow_entry->sc) {
                        meta = onvm_get_pkt_meta(pkts[i]);
                        // Enable below line to skip the 1st NF in the chain Note: <=1 => skip Flow_rule_installer and the First NF in the chain; <1 => skip only the Flow_rule_installer NF
                        if(meta->chain_index < 1) continue;

                        //Check the Flow Entry mark status and Add mark if not already done!
                        if(!(TEST_BIT(flow_entry->sc->highest_downstream_nf_index_id, meta->chain_index))) {
                                SET_BIT(flow_entry->sc->highest_downstream_nf_index_id, meta->chain_index);

                                // Add Flow Entry to the List of FTs marked as bottleneck
                                #ifdef ENABLE_SAVE_BACKLOG_FT_PER_NF
                                write_ft_to_cl_bft(cl, flow_entry, meta->chain_index);
                                #endif //ENABLE_SAVE_BACKLOG_FT_PER_NF
                        //}
                                #ifdef NF_BACKPRESSURE_APPROACH_2
                                uint8_t index = 1;
                                //for(; index < meta->chain_index; index++ ) {
                                for(index=(meta->chain_index -1); index >=1 ; index-- ) {
                                        if(!nfs[flow_entry->sc->nf_instance_id[index]].throttle_this_upstream_nf) {
                                                nfs[flow_entry->sc->nf_instance_id[index]].throttle_this_upstream_nf=1;
                                        }
                                        #ifdef HOP_BY_HOP_BACKPRESSURE
                                        break;
                                        #endif //HOP_BY_HOP_BACKPRESSURE
                                }
                                #endif  //NF_BACKPRESSURE_APPROACH_2
                                //approach: extend the service chain to keep track of client_nf_ids that service the chain, in-order to know which NFs to throttle in the wakeup thread..?
                                //Test and Set

                                //reset flow_entry and meta
                                flow_entry = NULL;
                                meta = NULL;
                        }
                }
        }
#if defined (BACKPRESSURE_EXTRA_DEBUG_LOGS)
        cl->stats.bkpr_count++;
#endif

        #endif //ENABLE_NF_BACKPRESSURE
}


#ifdef ENABLE_NF_BASED_BKPR_MARKING
void
onvm_check_and_reset_back_pressure_v2(__attribute__((unused)) struct onvm_nf *cl) {

#ifdef USE_BKPR_V2_IN_TIMER_MODE
        // Timer thread will take action.
        //Note check is rudimentary for TIMER_MODE; only to disallow ECE and BKPR marking when not necessary.
        return; //onvm_clear_all_entries_for_bottleneck(cl->instance_id);
#else
        if(cl->is_bottleneck) {
                if (rte_ring_count(cl->rx_q) < CLIENT_QUEUE_RING_LOW_WATER_MARK_SIZE) {
                        //take action inline:clear all associated flow table entries from being bottleneck
                        onvm_clear_all_entries_for_bottleneck(cl->instance_id);
                        cl->is_bottleneck = 0;
                        return; //Note this means NF need not be RECHECK_BACKPRESSURE_MARK_ON_TX_DEQUEUE nor  for CE marking
                }
        }
#endif //USE_BKPR_V2_IN_TIMER_MODE

        // check if rx_q_size is still above high threshold
#ifdef RECHECK_BACKPRESSURE_MARK_ON_TX_DEQUEUE
        if(rte_ring_count(cl->rx_q) >=CLIENT_QUEUE_RING_WATER_MARK_SIZE) {
                onvm_set_back_pressure_v2(cl);
        }
#endif //RECHECK_BACKPRESSURE_MARK_ON_TX_DEQUEUE

        return;
}
#endif //ENABLE_NF_BASED_BKPR_MARKING


void
onvm_check_and_reset_back_pressure(struct rte_mbuf *pkts[], uint16_t count, struct onvm_nf *cl) {

#ifdef ENABLE_NF_BASED_BKPR_MARKING
        onvm_check_and_reset_back_pressure_v2(cl);
        return;
#endif

        #ifdef ENABLE_NF_BACKPRESSURE
        struct onvm_pkt_meta *meta = NULL;
        struct onvm_flow_entry *flow_entry = NULL;
        uint16_t i;
        unsigned rx_q_count = rte_ring_count(cl->rx_q);

        // check if rx_q_size has decreased to acceptable level
        if (rx_q_count >= CLIENT_QUEUE_RING_LOW_WATER_MARK_SIZE) {

#if defined(RECHECK_BACKPRESSURE_MARK_ON_TX_DEQUEUE) || defined(ENABLE_ECN_CE)
                if(rx_q_count >=CLIENT_QUEUE_RING_WATER_MARK_SIZE) {

                        #ifdef RECHECK_BACKPRESSURE_MARK_ON_TX_DEQUEUE
                        onvm_set_back_pressure(pkts,count,cl);
                        #endif //RECHECK_BACKPRESSURE_MARK_ON_TX_DEQUEUE

#ifdef ENABLE_ECN_CE
                        onvm_detect_and_set_ecn_ce(pkts, count, cl);
#endif
                }
#endif
                return;
        }

        //Inside here indicates NFs Rx buffer has resumed to acceptable level (watermark - hysterisis)

#ifdef ENABLE_GLOBAL_BACKPRESSURE
        /** global single chain scenario:: Note: This only works for the default chain case where service ID of chain is always in increasing order **/
        if(global_bkpr_mode) {
                if (downstream_nf_overflow) {
                        // If service id is of any downstream that is/are bottlenecked then "move the lowest literally to next higher number" and when it is same as highsest reset bottlenext flag to zero
                        //  if(rte_ring_count(cl->rx_q) < CLIENT_QUEUE_RING_WATER_MARK_SIZE) {
                        if(rte_ring_count(cl->rx_q) < CLIENT_QUEUE_RING_LOW_WATER_MARK_SIZE) {
                                if (TEST_BIT(highest_downstream_nf_service_id, cl->info->service_id)) { //if (cl->info->service_id == highest_downstream_nf_service_id) {
                                        CLEAR_BIT(highest_downstream_nf_service_id, cl->info->service_id);
                                        if (highest_downstream_nf_service_id == 0) {
                                                downstream_nf_overflow = 0;
                                        }
                                }
                        }
                }
                return;
        }
#endif

#ifdef ENABLE_SAVE_BACKLOG_FT_PER_NF
        read_all_ft_frm_cl_bft(cl);
        return;
#endif //ENABLE_SAVE_BACKLOG_FT_PER_NF

        for(i = 0; i < count; i++) {
                int ret = get_flow_entry(pkts[i], &flow_entry);
                if (ret >= 0 && flow_entry && flow_entry->sc) {
                        if(flow_entry->sc->highest_downstream_nf_index_id ) {
                                meta = onvm_get_pkt_meta(pkts[i]);
                                if(TEST_BIT(flow_entry->sc->highest_downstream_nf_index_id, meta->chain_index )) {
                                        // also reset the chain's downstream NFs cl->downstream_nf_overflow and cl->highest_downstream_nf_index_id=0. But How?? <track the nf_instance_id in the service chain.
                                        CLEAR_BIT(flow_entry->sc->highest_downstream_nf_index_id, meta->chain_index);
                                        #ifdef NF_BACKPRESSURE_APPROACH_2
                                        // detect the start nf_index based on new val of highest_downstream_nf_index_id
                                        unsigned nf_index=(flow_entry->sc->highest_downstream_nf_index_id == 0)? (1): (get_index_of_highest_set_bit(flow_entry->sc->highest_downstream_nf_index_id));
                                        for(; nf_index < meta->chain_index; nf_index++) {
                                                if(nfs[flow_entry->sc->nf_instance_id[nf_index]].throttle_this_upstream_nf) {
                                                        nfs[flow_entry->sc->nf_instance_id[nf_index]].throttle_this_upstream_nf=0;
                                                }
                                        }
                                        #endif //NF_BACKPRESSURE_APPROACH_2
                                }
                        }
                        flow_entry = NULL;
                        meta = NULL;
                }
        }
        #endif //ENABLE_NF_BACKPRESSURE
}
#endif // ENABLE_NF_BACKPRESSURE
