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

                                 onvm_init.h

       Header for the initialisation function and global variables and
       data structures.


******************************************************************************/

#ifndef _ONVM_INIT_H_
#define _ONVM_INIT_H_

/********************************DPDK library*********************************/

#include <rte_byteorder.h>
#include <rte_cycles.h>
#include <rte_errno.h>
#include <rte_fbk_hash.h>
#include <rte_malloc.h>
#include <rte_memcpy.h>
#ifdef RTE_LIBRTE_PDUMP
#include <rte_pdump.h>
#endif

/*****************************Internal library********************************/

#include "onvm_common.h"
#include "onvm_flow_dir.h"
#include "onvm_flow_table.h"
#include "onvm_includes.h"
#include "onvm_mgr/onvm_args.h"
#include "onvm_mgr/onvm_stats.h"
#include "onvm_sc_common.h"
#include "onvm_sc_mgr.h"
#include "onvm_threading.h"

/***********************************Macros************************************/

#define MBUF_CACHE_SIZE 512
#define MBUF_OVERHEAD (sizeof(struct rte_mbuf) + RTE_PKTMBUF_HEADROOM)
#define RX_MBUF_DATA_SIZE 2048
#define MBUF_SIZE (RX_MBUF_DATA_SIZE + MBUF_OVERHEAD)

#define NF_INFO_SIZE sizeof(struct onvm_nf_init_cfg)

#define NF_MSG_SIZE sizeof(struct onvm_nf_msg)
#define NF_MSG_CACHE_SIZE 8

#define RTE_MP_RX_DESC_DEFAULT 512
#define RTE_MP_TX_DESC_DEFAULT 512
#define NF_MSG_QUEUE_SIZE 128

#define NO_FLAGS 0

#define ONVM_NUM_RX_THREADS 1
/* Number of auxiliary threads in manager, 1 reserved for stats */
#define ONVM_NUM_MGR_AUX_THREADS 1
#define ONVM_NUM_WAKEUP_THREADS 1  // Enabled when using shared core mode

/*************************External global variables***************************/

/* NF to Manager data flow */
extern struct rte_ring *incoming_msg_queue;

/* the shared port information: port numbers, rx and tx stats etc. */
extern struct port_info *ports;
extern struct core_status *cores;

extern struct rte_mempool *pktmbuf_pool;
extern struct rte_mempool *nf_msg_pool;
extern uint16_t num_nfs;
extern uint16_t num_services;
extern uint16_t default_service;
extern uint16_t **services;
extern uint16_t *nf_per_service_count;
extern unsigned num_sockets;
extern struct onvm_service_chain *default_chain;
extern struct onvm_ft *sdn_ft;
extern ONVM_STATS_OUTPUT stats_destination;
extern uint16_t global_stats_sleep_time;
extern uint32_t global_time_to_live;
extern uint32_t global_pkt_limit;
extern uint8_t global_verbosity_level;

/* Custom flags for onvm */
extern struct onvm_configuration *onvm_config;
extern uint8_t ONVM_NF_SHARE_CORES;

/* For handling shared core logic */
extern struct nf_wakeup_info *nf_wakeup_infos;

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
int
init(int argc, char *argv[]);

#endif  // _ONVM_INIT_H_
