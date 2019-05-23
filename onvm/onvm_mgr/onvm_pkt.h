/*********************************************************************
 *                     openNetVM
 *              https://sdnfv.github.io
 *
 *   BSD LICENSE
 *
 *   Copyright(c)
 *            2015-2017 George Washington University
 *            2015-2017 University of California Riverside
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

                                 onvm_pkt.h


      Header file containing all prototypes of packet processing functions


******************************************************************************/


#ifndef _ONVM_PKT_H_
#define _ONVM_PKT_H_


/*********************************Interfaces**********************************/


/*
 * Interface to process packets in a given RX queue.
 *
 * Inputs : a pointer to the rx queue
 *          an array of packets
 *          the size of the array
 *
 */
void
onvm_pkt_process_rx_batch(struct thread_info *rx, struct rte_mbuf *pkts[], uint16_t rx_count);


/*
 * Interface to process packets in a given TX queue.
 *
 * Inputs : a pointer to the tx queue
 *          an array of packets
 *          the size of the array
 *          a pointer to the client possessing the TX queue.
 *
 */
void
onvm_pkt_process_tx_batch(struct thread_info *tx, struct rte_mbuf *pkts[], uint16_t tx_count, struct onvm_nf *cl);


/*
 * Interface to send packets to all ports after processing them.
 *
 * Input : a pointer to the tx queue
 *
 */
void
onvm_pkt_flush_all_ports(struct thread_info *tx);


/*
 * Interface to send packets to all NFs after processing them.
 *
 * Input : a pointer to the tx queue
 *
 */
void
onvm_pkt_flush_all_nfs(struct thread_info *tx);


/*
 * Interface to drop a batch of packets.
 *
 * Inputs : the array of packets
 *          the size of the array
 *
 */
void
onvm_pkt_drop_batch(struct rte_mbuf **pkts, uint16_t size);



/* Interface to check and set ECN CE FLAG status after enqueue of packets to RX queue.
 *
 * Inputs :
 *          an array of packets
 *          the size/count of buffers in the array
 *          a pointer to the client possessing the RX queue.
 *
 */
//inline 
void
onvm_detect_and_set_ecn_ce(struct rte_mbuf *pkts[], uint16_t count, struct onvm_nf *cl);

/* Interface to check and set back-pressure status after enqueue of packets to RX queue.
 *
 * Inputs :
 *          an array of packets
 *          the size/count of buffers in the array
 *          a pointer to the client possessing the RX queue.
 *
 */
//inline 
void
onvm_set_back_pressure(struct rte_mbuf *pkts[], uint16_t count, __attribute__((unused)) struct onvm_nf *cl);

/* Interface to check and reset back-pressure status after dequeue of packets from a TX queue.
 *
 * Inputs :
 *          an array of packets
 *          the size/count of buffers in the array
 *          a pointer to the client possessing the TX queue.
 *
 */
//inline 
void
onvm_check_and_reset_back_pressure(struct rte_mbuf *pkts[], uint16_t count, struct onvm_nf *cl);

//inline 
void onvm_set_back_pressure_v2(struct onvm_nf *cl);
//inline 
void onvm_check_and_reset_back_pressure_v2(struct onvm_nf *cl);

/*****************************Internal functions******************************/


/*
 * Function to send packets to one port after processing them.
 *
 * Input : a pointer to the tx queue
 *
 */
void
onvm_pkt_flush_port_queue(struct thread_info *tx, uint16_t port);


/*
 * Function to send packets to one NF after processing them.
 *
 * Input : a pointer to the tx queue
 *
 */
void
onvm_pkt_flush_nf_queue(struct thread_info *thread, uint16_t client);


/*
 * Function to enqueue a packet on one port's queue.
 *
 * Inputs : a pointer to the tx queue responsible
 *          the number of the port
 *          a pointer to the packet
 *
 */
//inline 
void
onvm_pkt_enqueue_port(struct thread_info *tx, uint16_t port, struct rte_mbuf *buf);
//inline 
void
onvm_pkt_enqueue_port_v2(struct thread_info *tx, uint16_t port, struct rte_mbuf *buf, struct onvm_pkt_meta *meta, struct onvm_flow_entry *flow_entry);


/*
 * Function to enqueue a packet on one NF's queue.
 *
 * Inputs : a pointer to the tx queue responsible
 *          the number of the port
 *          a pointer to the packet
 *
 */
//inline 
void
onvm_pkt_enqueue_nf(struct thread_info *thread, struct rte_mbuf *pkt, struct onvm_pkt_meta *meta, struct onvm_flow_entry *flow_entry);


/*
 * Function to process a single packet.
 *
 * Inputs : a pointer to the tx queue responsible
 *         a pointer to the packet
 *         a pointer to the NF involved
 *
 */
//inline 
void
onvm_pkt_process_next_action(struct thread_info *tx, struct rte_mbuf *pkt,  __attribute__((unused)) struct onvm_pkt_meta *meta,
                __attribute__((unused)) struct onvm_flow_entry *flow_entry, __attribute__((unused)) struct onvm_nf *cl);

/******************************Helper functions*******************************/


/*
 * Helper function to drop a packet.
 *
 * Input : a pointer to the packet
 *
 * Ouput : an error code
 *
 */
//inline 
int
onvm_pkt_drop(struct rte_mbuf *pkt);


#endif  // _ONVM_PKT_H_
