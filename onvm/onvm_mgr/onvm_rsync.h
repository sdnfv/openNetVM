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

                                 onvm_special_nf0.h

     This file contains the prototypes for all functions related to packet
     processing within the NF Manager's Plugin/special NF NF[0].

******************************************************************************/


#ifndef _ONVM_RSYNC_H_
#define _ONVM_RSYNC_H_

#include "onvm_includes.h"
/********************************Globals and varaible declarations ***********************************/
typedef struct rsync_stats {

        /* Counters to keep track of number of packets transferred for Tx table and NF state sync */
        uint64_t tx_state_sync_pkt_counter;
        uint64_t nf_state_sync_pkt_counter[MAX_NFS];
        uint64_t tx_state_sync_in_pkt_counter;
        uint64_t nf_state_sync_in_pkt_counter[MAX_NFS];
        uint64_t transactions_out_counter;
        uint64_t transactions_in_counter;

        /* Counters to measure the actual packets enqueued vs Dropped  to these specific internal rings */
        uint64_t enq_coun_tx_tx_state_latch_ring[RTE_MAX_ETHPORTS];
        uint64_t drop_count_tx_tx_state_latch_ring[RTE_MAX_ETHPORTS];

        uint64_t enq_count_tx_nf_state_latch_ring[RTE_MAX_ETHPORTS];
        uint64_t drop_count_tx_nf_state_latch_ring[RTE_MAX_ETHPORTS];

        //perhaps better to ignore the double buffer enqueue counters
#ifdef ENABLE_RSYNC_WITH_DOUBLE_BUFFERING_MODE
#ifdef ENABLE_RSYNC_MULTI_BUFFERING
        uint64_t enq_coun_tx_tx_state_latch_db_ring[ENABLE_RSYNC_MULTI_BUFFERING][RTE_MAX_ETHPORTS];
        uint64_t drop_count_tx_tx_state_latch_db_ring[ENABLE_RSYNC_MULTI_BUFFERING][RTE_MAX_ETHPORTS];

        uint64_t enq_count_tx_nf_state_latch_db_ring[ENABLE_RSYNC_MULTI_BUFFERING][RTE_MAX_ETHPORTS];
        uint64_t drop_count_tx_nf_state_latch_db_ring[ENABLE_RSYNC_MULTI_BUFFERING][RTE_MAX_ETHPORTS];
#else
        uint64_t enq_coun_tx_tx_state_latch_db_ring[RTE_MAX_ETHPORTS];
        uint64_t drop_count_tx_tx_state_latch_db_ring[RTE_MAX_ETHPORTS];

        uint64_t enq_count_tx_nf_state_latch_db_ring[RTE_MAX_ETHPORTS];
        uint64_t drop_count_tx_nf_state_latch_db_ring[RTE_MAX_ETHPORTS];
#endif
#endif

        uint64_t enq_count_tx_port_ring[RTE_MAX_ETHPORTS];
        uint64_t drop_count_tx_port_ring[RTE_MAX_ETHPORTS];

}rsync_stats_t;
extern rsync_stats_t rsync_stat;
extern uint8_t replay_mode;
/********************************Interfaces***********************************/
#define CHECK_IF_ANY_ONE_BIT_SET(a) (a && !(a & (a-1)))
/********************************Interfaces***********************************/
int rsync_main(__attribute__((unused)) void *arg);

int rsync_start(__attribute__((unused)) void *arg);

int rsync_process_rsync_in_pkts(__attribute__((unused)) struct thread_info *rx, struct rte_mbuf *pkts[], uint16_t rx_count);

int onvm_print_rsync_stats(unsigned difftime, FILE *fout);

int notify_replay_mode(uint8_t mode); //0=stop, 1 =start //helper function to enble notify to remote replica node about start/stop of replay.
/****************************Internal functions*******************************/
#ifdef MIMIC_PICO_REP
extern volatile uint8_t rx_halt;
#endif
#endif  //
