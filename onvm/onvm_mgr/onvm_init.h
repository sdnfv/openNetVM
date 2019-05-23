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

                                 onvm_init.h

       Header for the initialisation function and global variables and
       data structures.


******************************************************************************/


#ifndef _ONVM_INIT_H_
#define _ONVM_INIT_H_

/***************************Standard C library********************************/

//#ifdef INTERRUPT_SEM  //move maro to makefile, otherwise uncomemnt or need to include these after including common.h
#include <sys/shm.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <semaphore.h>
#include <fcntl.h>
#include <mqueue.h>
//#endif //INTERRUPT_SEM

/********************************DPDK library*********************************/

#include <rte_byteorder.h>
#include <rte_memcpy.h>
#include <rte_malloc.h>
#include <rte_fbk_hash.h>
#include <rte_cycles.h>
#include <rte_errno.h>
#ifdef RTE_LIBRTE_PDUMP
#include <rte_pdump.h>
#endif


/*****************************Internal library********************************/


#include "onvm_mgr/onvm_args.h"
#include "onvm_mgr/onvm_stats.h"
#include "onvm_includes.h"
#include "onvm_common.h"
#include "onvm_sc_mgr.h"
#include "onvm_sc_common.h"
#include "onvm_flow_table.h"
#include "onvm_flow_dir.h"


/***********************************Macros************************************/


#define MBUFS_PER_NF 1536 //65536 //10240 //1536                            (use U: 1536, T:1536)
#define MBUFS_PER_PORT 1536 //(10240) //2048 //10240 //65536 //10240 //1536    (use U: 10240, T:10240)
#define MBUF_CACHE_SIZE 512
#define MBUF_OVERHEAD (sizeof(struct rte_mbuf) + RTE_PKTMBUF_HEADROOM)
#define RX_MBUF_DATA_SIZE 2048
#define MBUF_SIZE (RX_MBUF_DATA_SIZE + MBUF_OVERHEAD)

#define NF_INFO_SIZE sizeof(struct onvm_nf_info)
#define NF_INFO_CACHE (8)    //(8); 16 seems slightly better than 8 increase of 0.1mpps at a client NF; may be same...

#define NF_MSG_SIZE sizeof(struct onvm_nf_msg)
#define NF_MSG_CACHE_SIZE 8

//For TCP UDP use 70,40
//For TCP TCP, IO use 80 20
// Note: Based on the approach the tuned values change. For NF Throttling (80/75,20/25) works better, for Packet Throttling (70,50 or 70,40 or 80,40) seems better -- must be tuned and set accordingly.
#ifdef NF_BACKPRESSURE_APPROACH_1
#define CLIENT_QUEUE_RING_THRESHOLD (80)
#define CLIENT_QUEUE_RING_THRESHOLD_GAP (20) //(25)
#else  // defined NF_BACKPRESSURE_APPROACH_2 or other
#define CLIENT_QUEUE_RING_THRESHOLD (80)
#define CLIENT_QUEUE_RING_THRESHOLD_GAP (20)
#endif //NF_BACKPRESSURE_APPROACH_1

#define CLIENT_QUEUE_RING_WATER_MARK_SIZE ((uint32_t)((NF_QUEUE_RINGSIZE*CLIENT_QUEUE_RING_THRESHOLD)/100))
#define CLIENT_QUEUE_RING_LOW_THRESHOLD ((CLIENT_QUEUE_RING_THRESHOLD > CLIENT_QUEUE_RING_THRESHOLD_GAP) ? (CLIENT_QUEUE_RING_THRESHOLD-CLIENT_QUEUE_RING_THRESHOLD_GAP):(CLIENT_QUEUE_RING_THRESHOLD))
#define CLIENT_QUEUE_RING_LOW_WATER_MARK_SIZE ((uint32_t)((NF_QUEUE_RINGSIZE*CLIENT_QUEUE_RING_LOW_THRESHOLD)/100))
#define ECN_EWMA_ALPHA  (0.25)
#define CLIENT_QUEUE_RING_ECN_MARK_SIZE ((uint32_t)(((1-ECN_EWMA_ALPHA)*CLIENT_QUEUE_RING_WATER_MARK_SIZE) + ((ECN_EWMA_ALPHA)*CLIENT_QUEUE_RING_LOW_WATER_MARK_SIZE)))///2)
#define NO_FLAGS 0

#define DYNAMIC_CLIENTS 1
#define STATIC_CLIENTS 0


/******************************Data structures********************************/
//all moved to common.h

/*************************External global variables***************************/


extern struct onvm_nf *nfs;
/* NF to Manager data flow */
extern struct rte_ring *incoming_msg_queue;
extern struct rte_ring *mgr_msg_queue;      // Ring for Messages to Mgr
#ifdef ENABLE_SYNC_MGR_TO_NF_MSG
extern struct rte_ring *mgr_rsp_queue;      // Ring for Responses to Mgr
#endif
extern struct rte_mempool *nf_info_pool;    // Pool for NF_IFO of all NFs
extern struct rte_mempool *nf_msg_pool;     // Pool for Messages to NF

/* the shared port information: port numbers, rx and tx stats etc. */
extern struct port_info *ports;

extern struct rte_mempool *pktmbuf_pool;
extern struct rte_mempool *nf_msg_pool;
extern volatile uint16_t num_nfs;
extern uint16_t num_services;
extern uint16_t default_service;
extern uint16_t **services;
extern uint16_t *nf_per_service_count;
extern unsigned num_sockets;
extern struct onvm_service_chain *default_chain;
extern struct onvm_ft *sdn_ft;
extern ONVM_STATS_OUTPUT stats_destination;
extern uint16_t global_stats_sleep_time;
extern uint8_t global_verbosity_level;

#ifdef ENABLE_NF_MGR_IDENTIFIER
extern uint32_t nf_mgr_id;
#endif

#ifdef ENABLE_PER_SERVICE_MEMPOOL
/* Service state pool that can be accessed by other modules/files in MGR : Mainly NF.c that sets up the NFs pool based on the instantiated service type. */
extern void **services_state_pool;
#endif

#ifdef ENABLE_PER_FLOW_TS_STORE
/* TS Info that can be accessed and updated by MGR Tx Threads and exported */
extern void *onvm_mgr_tx_per_flow_ts_info;
#ifdef ENABLE_RSYNC_WITH_DOUBLE_BUFFERING_MODE
#ifdef ENABLE_RSYNC_MULTI_BUFFERING
extern void *onvm_mgr_tx_per_flow_ts_info_db[ENABLE_RSYNC_MULTI_BUFFERING];
#else
extern void *onvm_mgr_tx_per_flow_ts_info_db;
#endif
#endif
#endif

#ifdef ENABLE_REMOTE_SYNC_WITH_TX_LATCH
extern int node_role; //0= PREDECESSOR; 1=PRIMARY; 2=SECONDARY
#endif

#ifdef ENABLE_REMOTE_SYNC_WITH_TX_LATCH
extern struct rte_ring *tx_port_ring[RTE_MAX_ETHPORTS];     //ONVM_NUM_RSYNC_PORTS      //ring used by NFs and Other Tx threads to transmit out port packets
extern struct rte_ring *tx_tx_state_latch_ring[RTE_MAX_ETHPORTS];  //ONVM_NUM_RSYNC_PORTS //ring used by TX_RSYNC to store packets till 2 Phase commit of TS STAT Update
extern struct rte_ring *tx_nf_state_latch_ring[RTE_MAX_ETHPORTS]; //ONVM_NUM_RSYNC_PORTS//ring used by TX_RSYNC to store packets till 2 phase commit of NFs in the chain resulting in non-determinism.
#ifdef ENABLE_RSYNC_WITH_DOUBLE_BUFFERING_MODE
#ifdef ENABLE_RSYNC_MULTI_BUFFERING
extern struct rte_ring *tx_tx_state_latch_db_ring[ENABLE_RSYNC_MULTI_BUFFERING][RTE_MAX_ETHPORTS]; //additional Double buffer ring used by TX_RSYNC to store packets till 2 Phase commit of TS STAT Update
extern struct rte_ring *tx_nf_state_latch_db_ring[ENABLE_RSYNC_MULTI_BUFFERING][RTE_MAX_ETHPORTS]; //additional Double buffer ring used by TX_RSYNC to store packets till 2 phase commit of NFs in the chain resulting in non-determinism.
#else
extern struct rte_ring *tx_tx_state_latch_db_ring[RTE_MAX_ETHPORTS]; //additional Double buffer ring used by TX_RSYNC to store packets till 2 Phase commit of TS STAT Update
extern struct rte_ring *tx_nf_state_latch_db_ring[RTE_MAX_ETHPORTS]; //additional Double buffer ring used by TX_RSYNC to store packets till 2 phase commit of NFs in the chain resulting in non-determinism.
#endif
#endif

#if 0
extern struct rte_ring *tx_port_ring;           //ring used by NFs and Other Tx threads to transmit out port packets
extern struct rte_ring *tx_tx_state_latch_ring; //ring used by TX_RSYNC to store packets till 2 Phase commit of TS STAT Update
extern struct rte_ring *tx_nf_state_latch_ring; //ring used by TX_RSYNC to store packets till 2 phase commit of NFs in the chain resulting in non-determinism.
#endif

#endif

#ifdef ENABLE_VXLAN
#ifndef ENABLE_ZOOKEEPER
extern uint8_t remote_eth_addr[6];
extern struct ether_addr remote_eth_addr_struct;
#endif //ENABLE_ZOOKEEPER
#endif //ENABLE_VXLAN
/**********************************Functions**********************************/

/*
 * Function that initialize all data structures, memory mapping and global
 * variables.
 *
 * Input  : the number of arguments (following C conventions)
 *          an array of the arguments as strings
 * Output : an error code
 *
 */
int init(int argc, char *argv[]);

#endif  // _ONVM_INIT_H_
