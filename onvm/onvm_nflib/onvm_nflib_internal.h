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

//#ifdef INTERRUPT_SEM  //move maro to makefile, otherwise uncomemnt or need to include these after including onvm_common.h
#include <unistd.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <semaphore.h>
#include <fcntl.h>
#include <mqueue.h>
#include <sys/stat.h>

//#endif //INTERRUPT_SEM

/*****************************Internal headers********************************/


#include "onvm_includes.h"
#include "onvm_sc_common.h"
#include "onvm_flow_dir.h"
#include "onvm_nflib.h"
/**********************************Macros*************************************/
// Number of packets to attempt to read from queue
#define NF_PKT_BATCH_SIZE  (ONVM_PACKETS_BATCH_SIZE)    //((uint16_t)1)

// profiled overhead of rte_rdtsc() approx. 18~27cycles per call
#define RTDSC_CYCLE_COST    (20*2)

/******************************************************************************/
//                  NFLIB specific Feature flags
/******************************************************************************/
/* ENABLE TIMER BASED WEIGHT COMPUTATION IN NF_LIB */

/* Feature to enable computation of NF computation cost periodically using the timer */
//enable: ENABLE_TIMER_BASED_NF_CYCLE_COMPUTATION
#define ENABLE_TIMER_BASED_NF_CYCLE_COMPUTATION

/* Feature to enable static ID assignment/request to the NF */
#define ENABLE_STATIC_ID                //Enable NF to request its own id

/* Feature to enable profiling the latency of key NFLIB operations */
#define ENABLE_LOCAL_LATENCY_PROFILER   // Profile latency for NF stages

/* Test feature to profile the memcopy overhead in NFLIB */
//#define TEST_MEMCPY_OVERHEAD          // Profile memcpy overhead on NF performance


#if defined(ENABLE_TIMER_BASED_NF_CYCLE_COMPUTATION)
#include <rte_timer.h>
// Periodicity for Stats extraction in NFLIB for NFs computation cost
#define NF_STATS_PERIOD_IN_MS 1       //(use 1 or 10 or 100ms)
#endif //ENABLE_TIMER_BASED_NF_CYCLE_COMPUTATION


#ifdef TEST_MEMCPY_OVERHEAD
#define MEMCPY_SIZE (0.125*1024)
#define TEST_MEMCPY_MODE_PER_PACKET
#ifndef TEST_MEMCPY_MODE_PER_PACKET
#define TEST_MEMCPY_MODE_PER_BATCH
#endif //TEST_MEMCPY_MODE_PER_PACKET
#endif //TEST_MEMCPY_OVERHEAD

/******************************Global Variables*******************************/
// Shared data for host port information
struct port_info *ports;

// ring used to place new nf_info struct
static struct rte_ring *mgr_msg_ring;

// ring used for mgr -> NF messages
static struct rte_ring *nf_msg_ring;

// rings used to pass packets between NFlib and NFmgr
static struct rte_ring *tx_ring, *rx_ring;

#if defined(ENABLE_SHADOW_RINGS)
static struct rte_ring *tx_sring, *rx_sring;
#endif

//ring used for syncrhonous response msg to MGR
#ifdef ENABLE_SYNC_MGR_TO_NF_MSG
static struct rte_ring *mgr_rsp_ring;
#endif

// Shared data from Manager. We update statistics here
struct onvm_nf *nfs = NULL;      //obtain at the time of init.
struct onvm_nf *this_nf = NULL;  //update this once the Instance ID is obtained.

// Shared data from manager, has information used for nf_side tx
uint16_t **services;
uint16_t *nf_per_service_count;

// Shared data for client info
extern struct onvm_nf_info *nf_info;

// Shared pool for all nfs info
static struct rte_mempool *nf_info_mp;

// Shared pool for mgr <--> NF messages
static struct rte_mempool *nf_msg_pool;

// Shared pool for packets
static struct rte_mempool *pktmbuf_pool;

// User-given NF Client ID (defaults to manager assigned)
static uint16_t initial_instance_id = NF_NO_ID;


// User supplied service ID
static uint16_t service_id = -1;


// True as long as the NF should keep processing packets
static uint8_t keep_running = 1;


// Shared data for default service chain
//static 
struct onvm_service_chain *default_chain;


#ifdef INTERRUPT_SEM
// to track packets per NF <used for sampling computation cost>
uint64_t counter = 1;
// flag (shared mem variable) to track state of NF and trigger wakeups
// flag_p=1 => NF sleeping (waiting on semaphore)
// flag_p=0 => NF is running and processing (not waiting on semaphore)
// flag_p=2 => "Internal NF Msg to wakeup NF and do processing .. Yet To be Finalized."   
static rte_atomic16_t * flag_p;
//static rte_atomic16_t *volatile flag_p;

#ifdef USE_SEMAPHORE
// Mutex for sem_wait
static sem_t *mutex;
#endif //USE_SIGNAL
#endif  //INTERRUPT_SEM

#if defined(ENABLE_TIMER_BASED_NF_CYCLE_COMPUTATION)
struct rte_timer stats_timer;
#endif //ENABLE_TIMER_BASED_NF_CYCLE_COMPUTATION

#ifdef ENABLE_LOCAL_LATENCY_PROFILER
onvm_interval_timer_t ts, g_ts;
#endif //ENABLE_LOCAL_LATENCY_PROFILER

#ifdef TEST_MEMCPY_OVERHEAD
void *base_memory = NULL;
static inline void allocate_base_memory(void);
static inline void do_memcopy(void *from_pointer);
#endif //TEST_MEMCPY_OVERHEAD

pkt_handler_func pkt_hdl_func = NULL;
callback_handler_func cb_func = NULL;
#define ONVM_NO_CALLBACK NULL
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

static inline void
onvm_nflib_cleanup(__attribute__((unused)) struct onvm_nf_info *nf_info);

static inline void
onvm_nflib_start_nf(struct onvm_nf_info *nf_info);

static inline int
onvm_nflib_notify_ready(struct onvm_nf_info *nf_info);

static inline void
onvm_nflib_dequeue_messages(struct onvm_nf_info *nf_info);

#ifdef INTERRUPT_SEM
/*
 * Function to initalize the shared cpu support
 *
 * Input  : Number of NF instances
 */ 
static inline void
init_shared_cpu_info(uint16_t instance_id);

#define YEILD_DUE_TO_EXPLICIT_REQ   (0)
#define YIELD_DUE_TO_EMPTY_RX_RING  (1)
#define YIELD_DUE_TO_FULL_TX_RING   (1)
void onvm_nf_yeild(__attribute__((unused))struct onvm_nf_info* info, uint8_t reason_rxtx);

#endif  //INTERRUPT_SEM

#ifdef ENABLE_TIMER_BASED_NF_CYCLE_COMPUTATION
static inline void
init_nflib_timers(void);
#endif //#ENABLE_TIMER_BASED_NF_CYCLE_COMPUTATION

#ifdef ENABLE_NFV_RESL
static inline void
        onvm_nflib_wait_till_notification(__attribute__((unused))struct onvm_nf_info* info);

#ifdef ENABLE_REPLICA_STATE_UPDATE
void *pReplicaStateMempool = NULL;
#endif

#ifdef ENABLE_NF_PAUSE_TILL_OUTSTANDING_NDSYNC_COMMIT
//volatile uint8_t bNDSync=0;    //status indicating if the outstanding ND is committed or not
// 0 = No Outstanding ND Sync; 1 = Outstanding ND Sync yet to be committed.
#endif

#endif //ENABLE_NFV_RESL
dirty_mon_state_map_tbl_t *dirty_state_map = NULL;  //reson to keep it ouside is that NFs can still make  use of it regardless of resiliency

#ifdef MIMIC_FTMB
//shared variable for FTMB mode
uint8_t SV_ACCES_PER_PACKET;
#endif

#endif  // _ONVM_NFLIB_INTERNAL_H_
