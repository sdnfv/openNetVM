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

                                 onvm_init.h

       Header for the initialisation function and global variables and
       data structures.


******************************************************************************/


#ifndef _ONVM_INIT_H_
#define _ONVM_INIT_H_


/********************************DPDK library*********************************/

#include <rte_byteorder.h>
#include <rte_memcpy.h>
#include <rte_malloc.h>
#include <rte_fbk_hash.h>
#include <rte_cycles.h>
#include <rte_errno.h>


/*****************************Internal library********************************/


#include "onvm_mgr/onvm_args.h"
#include "shared/onvm_includes.h"
#include "shared/common.h"
#include "shared/onvm_sc_mgr.h"
#include "shared/onvm_sc_common.h"
#include "shared/onvm_flow_table.h"
#include "shared/onvm_flow_dir.h"


/***********************************Macros************************************/


#define MBUFS_PER_CLIENT 1536
#define MBUFS_PER_PORT 1536
#define MBUF_CACHE_SIZE 512
#define MBUF_OVERHEAD (sizeof(struct rte_mbuf) + RTE_PKTMBUF_HEADROOM)
#define RX_MBUF_DATA_SIZE 2048
#define MBUF_SIZE (RX_MBUF_DATA_SIZE + MBUF_OVERHEAD)

#define NF_INFO_SIZE sizeof(struct onvm_nf_info)
#define NF_INFO_CACHE 8

#define RTE_MP_RX_DESC_DEFAULT 512
#define RTE_MP_TX_DESC_DEFAULT 512
#define CLIENT_QUEUE_RINGSIZE 128

#define NO_FLAGS 0

#define ONVM_NUM_RX_THREADS 1

#define DYNAMIC_CLIENTS 0
#define STATIC_CLIENTS 1


/******************************Data structures********************************/


/*
 * Define a client structure with all needed info, including
 * stats from the clients.
 */
struct client {
        struct rte_ring *rx_q;
        struct rte_ring *tx_q;
        struct onvm_nf_info *info;
        uint16_t instance_id;
        /* these stats hold how many packets the client will actually receive,
         * and how many packets were dropped because the client's queue was full.
         * The port-info stats, in contrast, record how many packets were received
         * or transmitted on an actual NIC port.
         */
        struct {
                volatile uint64_t rx;
                volatile uint64_t rx_drop;
                volatile uint64_t act_out;
                volatile uint64_t act_tonf;
                volatile uint64_t act_drop;
                volatile uint64_t act_next;
                volatile uint64_t act_buffer;
        } stats;
};


/*
 * Shared port info, including statistics information for display by server.
 * Structure will be put in a memzone.
 * - All port id values share one cache line as this data will be read-only
 * during operation.
 * - All rx statistic values share cache lines, as this data is written only
 * by the server process. (rare reads by stats display)
 * - The tx statistics have values for all ports per cache line, but the stats
 * themselves are written by the clients, so we have a distinct set, on different
 * cache lines for each client to use.
 */
struct rx_stats{
        uint64_t rx[RTE_MAX_ETHPORTS];
};


struct tx_stats{
        uint64_t tx[RTE_MAX_ETHPORTS];
        uint64_t tx_drop[RTE_MAX_ETHPORTS];
};


struct port_info {
        uint8_t num_ports;
        uint8_t id[RTE_MAX_ETHPORTS];
        volatile struct rx_stats rx_stats;
        volatile struct tx_stats tx_stats;
};



/*************************External global variables***************************/


extern struct client *clients;

extern struct rte_ring *nf_info_queue;

/* the shared port information: port numbers, rx and tx stats etc. */
extern struct port_info *ports;

extern struct rte_mempool *pktmbuf_pool;
extern uint16_t num_clients;
extern uint16_t num_services;
extern uint16_t default_service;
extern uint16_t **services;
extern uint16_t *nf_per_service_count;
extern unsigned num_sockets;
extern struct onvm_service_chain *default_chain;
extern struct onvm_ft *sdn_ft;


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
