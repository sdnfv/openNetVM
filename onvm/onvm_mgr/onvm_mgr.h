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

                               onvm_mgr.h


         Header file containing all shared headers and data structures


******************************************************************************/


#ifndef _ONVM_MGR_H_
#define _ONVM_MGR_H_


/******************************Standard C library*****************************/


#include <netinet/ip.h>
#include <stdbool.h>
#include <math.h>


/********************************DPDK library*********************************/


#include <rte_byteorder.h>
#include <rte_memcpy.h>
#include <rte_malloc.h>
#include <rte_fbk_hash.h>


/******************************Internal headers*******************************/


#include "onvm_mgr/onvm_args.h"
#include "onvm_mgr/onvm_init.h"
#include "onvm_includes.h"
#include "onvm_sc_mgr.h"
#include "onvm_flow_table.h"
#include "onvm_flow_dir.h"


/***********************************Macros************************************/


#define PACKET_READ_SIZE ((uint16_t)32)

#define TO_PORT 0
#define TO_NF 1


/***************************Shared global variables***************************/


/* ID to be assigned to the next NF that starts */
extern uint16_t next_instance_id;

/* Client information, rx/tx/msg rings, nf_info, inst_id and stats */
extern struct onvm_nf *nfs;


/*******************************Data Structures*******************************/


/*
 * Local buffers to put packets in, used to send packets in bursts to the
 * NFs or to the NIC
 */
struct packet_buf {
        struct rte_mbuf *buffer[PACKET_READ_SIZE];
        uint16_t count;
};


/** Thread state. This specifies which NFs the thread will handle and
 *  includes the packet buffers used by the thread for NFs and ports.
 */
struct thread_info {
       unsigned queue_id;
       unsigned first_nf;
       unsigned last_nf;
       /* FIXME: This is confusing since it is non-inclusive. It would be
        *        better to have this take the first NF and the number
        *        of consecutive NFs after it to handle.
        */
       struct packet_buf *nf_rx_buf;
       struct packet_buf *port_tx_buf;
};

#endif  // _ONVM_MGR_H_
