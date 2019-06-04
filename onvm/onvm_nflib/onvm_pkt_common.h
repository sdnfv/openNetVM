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

                                 onvm_pkt_common.h


      Header file containing all prototypes of packet processing functions


******************************************************************************/

#ifndef _ONVM_PKT_COMMON_H_
#define _ONVM_PKT_COMMON_H_

#include "onvm_common.h"
#include "onvm_flow_dir.h"
#include "onvm_includes.h"
#include "onvm_sc_common.h"
#include "onvm_sc_mgr.h"

extern struct port_info *ports;
extern struct onvm_service_chain *default_chain;

/*********************************Interfaces**********************************/

/*
 * Interface to process packets in a given TX queue.
 *
 * Inputs : a pointer to the tx queue
 *          an array of packets
 *          the size of the array
 *          a pointer to the NF possessing the TX queue.
 *
 */
void
onvm_pkt_process_tx_batch(struct queue_mgr *tx_mgr, struct rte_mbuf *pkts[], uint16_t tx_count, struct onvm_nf *nf);

/*
 * Interface to send packets to all NFs after processing them.
 *
 * Input : a pointer to the tx queue
 *         a pointer to the NF possessing the TX queue.
 *
 */
void
onvm_pkt_flush_all_nfs(struct queue_mgr *tx_mgr, struct onvm_nf *source_nf);

/*
 * Function to send packets to one NF after processing them.
 *
 * Input : a pointer to the tx queue
 *         a pointer to the NF possessing the TX queue.
 *
 */
void
onvm_pkt_flush_nf_queue(struct queue_mgr *tx_mgr, uint16_t nf_id, struct onvm_nf *source_nf);

/*
 * Function to enqueue a packet on one NF's queue.
 *
 * Inputs : a pointer to the tx queue responsible
 *          a destination nf id
 *          a pointer to the packet
 *          a pointer to the NF possessing the TX queue.
 *
 */
void
onvm_pkt_enqueue_nf(struct queue_mgr *tx_mgr, uint16_t dst_service_id, struct rte_mbuf *pkt, struct onvm_nf *source_nf);

/*
 * Function to send packets to one port after processing them.
 *
 * Input : a pointer to the tx queue
 *         the number of the port
 *
 */
void
onvm_pkt_flush_port_queue(struct queue_mgr *tx_mgr, uint16_t port);

/*
 * Give packets to TX thread so it can do useful work.
 *
 * Inputs : a pointer to the packet buf
 *          a pointer to the NF
 *
 */
void
onvm_pkt_enqueue_tx_thread(struct packet_buf *pkt_buf, struct onvm_nf *nf);

#endif  // _ONVM_PKT_COMMON_H_
