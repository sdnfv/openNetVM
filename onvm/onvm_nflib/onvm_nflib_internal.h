/*********************************************************************
 *                     openNetVM
 *              https://sdnfv.github.io
 *
 *   BSD LICENSE
 *
 *   Copyright(c)
 *            2015-2016 George Washington University
 *            2015-2016 University of California Riverside
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

                            onvm_nflib_internal.h


          Header file for all internal functions used within the API


******************************************************************************/


#ifndef _ONVM_NFLIB_INTERNAL_H_
#define _ONVM_NFLIB_INTERNAL_H_


/***************************Standard C library********************************/


#include <getopt.h>
#include <signal.h>


/*****************************Internal headers********************************/


#include "onvm_includes.h"
#include "onvm_sc_common.h"


/**********************************Macros*************************************/


// Number of packets to attempt to read from queue
#define PKT_READ_SIZE  ((uint16_t)32) 


/******************************Global Variables*******************************/


// ring used to place new nf_info struct
static struct rte_ring *nf_info_ring;


// rings used to pass packets between NFlib and NFmgr
static struct rte_ring *tx_ring, *rx_ring;


// shared data from server. We update statistics here
static volatile struct client_tx_stats *tx_stats;


// Shared data for client info
extern struct onvm_nf_info *nf_info;


// Shared pool for all clients info
static struct rte_mempool *nf_info_mp;


// User-given NF Client ID (defaults to manager assigned)
static uint16_t initial_instance_id = NF_NO_ID;


// User supplied service ID
static uint16_t service_id = -1;


// True as long as the NF should keep processing packets
static uint8_t keep_running = 1;


// Shared data for default service chain
static struct onvm_service_chain *default_chain;


/******************************Internal functions*****************************/


/*
 * Function that initialize a nf info data structure.
 *
 * Input  : the tag to name the NF
 * Output : the data structure initialized
 *
 */
static struct onvm_nf_info *
onvm_nflib_info_init(const char *tag);


/*
 * Function printing an explanation of command line instruction for a NF.
 *
 * Input : name of the executable containing the NF
 *
 */
static void
onvm_nflib_usage(const char *progname);


/*
 * Function that parses the global arguments common to all NFs.
 *
 * Input  : the number of arguments (following C standard library convention)
 *          an array of strings representing these arguments
 * Output : an error code
 *
 */
static int
onvm_nflib_parse_args(int argc, char *argv[]);


/*
 * Signal handler to catch SIGINT.
 *
 * Input : int corresponding to the signal catched
 *
 */
static void
onvm_nflib_handle_signal(int sig);


#endif  // _ONVM_NFLIB_INTERNAL_H_
