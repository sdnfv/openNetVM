/*********************************************************************
 *                     openNetVM
 *              https://sdnfv.github.io
 *
 *   BSD LICENSE
 *
 *   Copyright(c)
 *            2015-2017 George Washington University
 *            2015-2017 University of California Riverside
 *            2010-2014 Intel Corporation
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
 * onvm_common.h - shared data between host and NFs
 ********************************************************************/

#ifndef _COMMON_H_
#define _COMMON_H_

#include <stdint.h>

#include <rte_mbuf.h>
#include <rte_ether.h>

#include "onvm_sort.h"
#include "onvm_comm_utils.h"
#include "onvm_msg_common.h"
#include "onvm_config_common.h"

//check on each node by executing command  $"getconf LEVEL1_DCACHE_LINESIZE" or cat /sys/devices/system/cpu/cpu0/cache/index0/coherency_line_size
#define ONVM_CACHE_LINE_SIZE (64)

//choose the ONVM operation mode: High Throughput=Large buffers=more Latency; LOW_LATENCY=Small Buffers=Low Throughput.
#define ENABLE_HIGH_THROUGHPUT_MODE
#ifndef ENABLE_HIGH_THROUGHPUT_MODE
#define ENABLE_LOW_LATENCY_MODE
#endif

#ifndef MIN
#define MIN(a,b) ((a) < (b)? (a):(b))
#endif

#ifndef MAX
#define MAX(a,b) ((a) > (b)? (a):(b))
#endif

#define ARBITER_PERIOD_IN_US (100)      // 250 or 100 micro seconds
//#define USE_SINGLE_NIC_PORT           // NEEDED FOR VXLAN?
#define MAX_NFS (16)                // total number of NFs allowed
#define MAX_SERVICES (16)               // total number of unique services allowed
#define MAX_NFS_PER_SERVICE (8)     // max number of NFs per service.

// NIC (Rx, Tx) and NF (Rx and Tx) Ring buffer sizes
#ifdef ENABLE_HIGH_THROUGHPUT_MODE
#define RTE_MP_RX_DESC_DEFAULT (1024)   //(1024) //512 //512 //1536 //2048 //1024 //512 (use U:1024, T:512)
#define RTE_MP_TX_DESC_DEFAULT (1024*4)   //(1024) //512 //512 //1536 //2048 //1024 //512 (use U:1024, T:512)       //For Resiliency increase to at least 1/2 of Latch Ring buffer size
#define NF_QUEUE_RINGSIZE  (1024*2)   //(16384) //4096 //(4096) //(512)  //128 //4096  //4096 //128   (use U:4096, T:512) //256
#elif defined(ENABLE_LOW_LATENCY_MODE)
#define RTE_MP_RX_DESC_DEFAULT (128)   //(1024) //512 //512 //1536 //2048 //1024 //512 (use U:1024, T:512) //128
#define RTE_MP_TX_DESC_DEFAULT (128)   //(1024) //512 //512 //1536 //2048 //1024 //512 (use U:1024, T:512) //128
#define NF_QUEUE_RINGSIZE  (512)   //(16384) //4096 //(4096) //(512)  //128 //4096  //4096 //128   (use U:4096, T:512) //512
#else
#define RTE_MP_RX_DESC_DEFAULT (256)   //(1024) //512 //512 //1536 //2048 //1024 //512 (use U:1024, T:512)
#define RTE_MP_TX_DESC_DEFAULT (256)   //(1024) //512 //512 //1536 //2048 //1024 //512 (use U:1024, T:512)
#define NF_QUEUE_RINGSIZE  (1024)   //(16384) //4096 //(4096) //(512)  //128 //4096  //4096 //128   (use U:4096, T:512) //256
#endif

#define ONVM_PACKETS_BATCH_SIZE ((uint16_t)32)    // Batch size for Rx/Tx Queue and NFs
#define PACKET_READ_SIZE (ONVM_PACKETS_BATCH_SIZE)
#define NF_MSG_QUEUE_SIZE   (128)       // NFMGR-->NF and vice-versa Messages count

// Number of Rx and Tx Threads
#define ONVM_NUM_RX_THREADS      (1)
#define ONVM_NUM_TX_THREADS      (4)
#define ONVM_NUM_MGR_AUX_THREADS (1) /* Number of auxiliary threads in manager, 1 reserved for stats */

#define ONVM_MAX_CHAIN_LENGTH (12)      // the maximum chain length
#define SDN_FT_ENTRIES  (1024)          // Max Flow Table Entries

//Packet Processing Actions
#define ONVM_NF_ACTION_DROP 0   // drop packet
#define ONVM_NF_ACTION_NEXT 1   // to whatever the next action is configured by the SDN controller in the flow table
#define ONVM_NF_ACTION_TONF 2   // send to the NF specified in the argument field (assume it is on the same host)
#define ONVM_NF_ACTION_OUT  3   // send the packet out the NIC port set in the argument field
#define ONVM_NF_ACTION_TO_NF_INSTANCE   4   //send to NF Instance ID (specified in the meta->destination. Note unlike ONVM_NF_ACTION_TONF which means to NF SERVICE ID, this is direct destination instance ID.


/******************************************************************************/
/*              MACROS (FEATURE FLAGS)  FOR ADDON FEATURES                    */
/******************************************************************************/
/** Feature enables Special NF[0] in MGR::internal Flow rule Installer. etc.. */
#define ONVM_ENABLE_SPEACILA_NF

/** Feature Flag to enable Interrupt driven NFs(wake up/sleep governed by IPC). */
#define INTERRUPT_SEM

/** Feature to Enable Packet Time stamping and to measure processing latency */
//#define ENABLE_PACKET_TIMESTAMPING

/** Feature to enable Extra Debug Logs on all components */
//#define __DEBUG_LOGS__

/** Feature to Enable NFs Tx Statistics Logs */
#define ENABLE_NF_TX_STAT_LOGS

/** Feature to enable port Tx Statistic Logs */
#define ENABLE_PORT_TX_STATS_LOGS

/** Feature to Enable Synchonous Message->Response from MGR->NF->MGR */
#define ENABLE_SYNC_MGR_TO_NF_MSG

/** VXLAN Feature Addition **/
//#define ENABLE_VXLAN

/** Enable Zookeeper Data store */
//#define ENABLE_ZOOKEEPER

/** Enable Packet Dumper **/
//#define RTE_LIBRTE_PDUMP

#ifdef RTE_LIBRTE_PDUMP
#include <rte_pdump.h>
#endif

/******************************************************************************/
// SUB FEATURES FOR DEBUG PRINT AND  MSG CONTROL
/******************************************************************************/
#ifdef __DEBUG_LOGS__
//#define __DEBUG_NFMGR_NF_LOGS__
//#define __DEBUG_NFMGR_WK_LOGS__
//#define __DEBUG_NFMGR_RSYNC_LOGS__
//#define __DEBUG_NDSYNC_LOGS__
//#define __DEBUG_BFD_LOGS__
#endif


/******************************************************************************/
// SUB FEATURES FOR ENABLE_PACKET_TIMESTAMPING
/******************************************************************************/
/** Sub features for ENABLE_PACKET_TIMESTAMPING  */
#ifdef ENABLE_PACKET_TIMESTAMPING
#define PROFILE_PACKET_PROCESSING_LATENCY
#endif

/******************************************************************************/
// SUB FEATURES FOR ONVM_ENABLE_SPEACILA_NF
/******************************************************************************/
/** Sub defines and features of SPECIAL NF */
#ifdef ONVM_ENABLE_SPEACILA_NF
/* SERVICE ID of the special NF */
#define ONVM_SPECIAL_NF_SERVICE_ID  (0)
/* Instance ID of the special NF */
#define ONVM_SPECIAL_NF_INSTANCE_ID (0)

/** Enable feature to capture and store packets with pcap-lib */
//#define ENABLE_PCAP_CAPTURE
/******************************************************************************/
// SUB FEATURES FOR ENABLE_PCAP_CAPTURE (Currently only within Special NF)
/******************************************************************************/
#ifdef ENABLE_PCAP_CAPTURE
//#define ENABLE_PCAP_CAPTURE_ON_INPUT
#ifndef ENABLE_PCAP_CAPTURE_ON_INPUT
#define ENABLE_PCAP_CAPTURE_ON_OUTPUT
#endif
#endif

#endif //ONVM_ENABLE_SPEACILA_NF

/******************************************************************************/
// SUB FEATURES FOR INTERRUPT_SEM
/******************************************************************************/
/** Sub features for INTERRUPT_SEMANTICS for NFs */
#ifdef INTERRUPT_SEM

/** Use Semaphore for IPC */
#define USE_SEMAPHORE                   // (Preferred Usage: Enabled)

/** Enable Local Backpressure feature: Pause NF from processing packets if Tx is Full **/
#define NF_LOCAL_BACKPRESSURE           // (Preferred Usage: Enabled)

/* Enable back-pressure handling to throttle NFs upstream */
#define ENABLE_NF_BACKPRESSURE          // (Preferred Usage: Enabled)

/* Enable CGROUP cpu share setting Feature */
#define ENABLE_CGROUPS_FEATURE          // (Preferred Usage: Enabled)

/* Enable NFV Resiliency Feature Flag */
#define ENABLE_NFV_RESL               // (Preferred Usage: Enabled)

/* Enable FLow Table Entry Index in the Pkts Meta Field */
#define ENABLE_FT_INDEX_IN_META         // (Preferred Usage: Enabled)

/** Feature to Enable NFs Yield and Wake Counters */
#define ENABLE_NF_WAKE_NOTIFICATION_COUNTER
#define ENABLE_NF_YIELD_NOTIFICATION_COUNTER

#endif

#if (!defined(USE_SEMAPHORE))
#define USE_POLL_MODE
#endif

#ifdef USE_ZMQ
#include <zmq.h>
#endif

/******************************************************************************/
// Feature flags to enable TIMER MODE Operations for Main and Wake up thread
/******************************************************************************/
/* Enable Main thread to operate in Timer mode (multi-task main thread) */
#define ENABLE_USE_RTE_TIMER_MODE_FOR_MAIN_THREAD   //(Usage Preference: Enabled)
/* Run Wake thread in Timer mode ( muti task wake thread for Arbiter case */
//#define ENABLE_USE_RTE_TIMER_MODE_FOR_WAKE_THREAD //(Usage Preference: Disabled)

#if defined (ENABLE_USE_RTE_TIMER_MODE_FOR_MAIN_THREAD)
#include <rte_timer.h>
#endif //ENABLE_USE_RTE_TIMER_MODE_FOR_MAIN_THREAD

/******************************************************************************/
/** NF BACKPRESSURE REALTED EXTENSION FEATURES AND OPTIONS */
/******************************************************************************/
#ifdef ENABLE_NF_BACKPRESSURE
//forward declaration either store reference of onvm_flow_entry or onvm_service_chain (latter may be sufficient)
struct onvm_flow_entry;
struct onvm_service_chain;

/* Backpressure Marking States */
#define BOTTLENECK_NF_STATUS_RESET           (0x00)
#define BOTTLENECK_NF_STATUS_WAIT_ENQUEUED   (0x01)
#define BOTTLENECK_NF_STATUS_DROP_MARKED     (0x02)

/** Global backpressure only for default chain; choose any below backpressure modes  */
//#define ENABLE_GLOBAL_BACKPRESSURE    // (Preferred Usage: Disabled)

/* #1 Throttle enqueue of packets to the upstream NFs (handle in onvm_pkts_enqueue) */
#define NF_BACKPRESSURE_APPROACH_1      // (Preferred Usage: Enabled)

/* #2 Throttle upstream NFs from getting scheduled (handle in wakeup mgr) */
#ifndef NF_BACKPRESSURE_APPROACH_1
#define NF_BACKPRESSURE_APPROACH_2    // (Preferred Usage: Disabled)
#endif //NF_BACKPRESSURE_APPROACH_1

/* Enable watermark level NFs Tx and Rx Rings  // details on count in the onvm_init.h (Preferred Usage: Enabled) */
#define ENABLE_RING_WATERMARK

/* Decouple control and data plane functionalities for Backpressure handling : Do not rely on ring buffer packets
 * Prerequisite: NF mapping must be stored in the FT entry so that chain determines which NF instances it uses. */
#define ENABLE_NF_BASED_BKPR_MARKING    // (Preferred Usage: Enabled)

/* For Bottleneck on Rx Ring; whether or not to Drop packets from Rx/Tx buf during flush_operation
 * Note: This is one of the likely cause of Out-of_order packets in the OpenNetVM (with Bridge) case:
 * //Disable drop of existing packets -- may have caveats on when next flush would operate on that Tx/Rx buffer..
 * //Repercussions in onvm_pkt.c: onvm_pkt_enqueue_nf() to handle overflow and stop putting packet in full buffer and drop new ones instead.
 * //Observation: Good for TCP use cases, but with PktGen,Moongen dents line rate approx 0.3Mpps slow down
 *  (Preferred Usage: Disabled )
 *  */
//#define DO_NOT_DROP_PKTS_ON_FLUSH_FOR_BOTTLENECK_NF   //(Preferred Usage: Disabled)

/** Sub Feature to enable to re-check for back-pressure marking, at the time of packet dequeue from the NFs Tx Ring.*/
//#define RECHECK_BACKPRESSURE_MARK_ON_TX_DEQUEUE   //(Preferred Usage: Disabled: Not worth the overhead)

// Enable extra profile logs for back-pressure: Move all prints and additional variables under this flag (as optimization)
//#define BACKPRESSURE_EXTRA_DEBUG_LOGS

/* Enable the Arbiter Logic to control the NFs scheduling and period on each core */
//#define ENABLE_ARBITER_MODE
//NFLib check for wake;/sleep state and Wakeup thread to put the the NFs to sleep after timer expiry (This feature is not working as expected..)
//#define USE_ARBITER_NF_EXEC_PERIOD

#endif //ENABLE_NF_BACKPRESSURE

#if defined(ENABLE_NF_BASED_BKPR_MARKING)
/* Perform Backpressure marking in Timer Thread context  :: Decouple control plane and data plane processing.
Note: Requires to enable timer mode main thread. (currently directly called from wake mgr; ensure timer dependency later) */
#define USE_BKPR_V2_IN_TIMER_MODE     // (Preferred Usage: Enabled (better) or Disabled is fine)
#endif

/** Additional Extensions and sub-options for Back_Pressure handling approaches */
#ifdef NF_BACKPRESSURE_APPROACH_1
/* Extension to approach 1 to make packet drops only at the beginning on the chain (i.e only at the time to enqueue to first NF) */
#define DROP_PKTS_ONLY_AT_RX_ENQUEUE     // (Preferred Usage: Enabled)

#if !defined(ENABLE_NF_BASED_BKPR_MARKING)
/* save backlog Flow Entries per NF */
#define ENABLE_SAVE_BACKLOG_FT_PER_NF   // (Preferred Usage: Enabled)
/* Sub feature for ENABLE_SAVE_BACKLOG_FT_PER_NF: Use Ring buffer to store and delete backlog Flow Entries per NF */
#define BACKPRESSURE_USE_RING_BUFFER_MODE   // (Preferred Usage: Enabled)
#endif //USE_BKPR_V2_IN_TIMER_MODE

#endif //NF_BACKPRESSURE_APPROACH_1

/* Enable ECN CE FLAG : Feature Flag to enable marking ECN_CE flag on the flows that pass through the NFs with Rx Ring buffers exceeding the watermark level.
 * Dependency: Must have ENABLE_RING_WATERMARK feature defined. and HIGH and LOW Thresholds to be set. otherwise, marking may not happen at all.. Ideally, marking should be done after dequeue from Tx, to mark if Rx is overbudget..
 * On similar lines, even the back-pressure marking must be done for all flows after dequeue from the Tx Ring..
 * (Preferred Usage: Disabled) */
#ifdef ENABLE_RING_WATERMARK
//#define ENABLE_ECN_CE
#endif //ENABLE_RING_WATERMARK

#ifdef NF_BACKPRESSURE_APPROACH_2
//Option to enable HOP by HOP propagation of back-pressure
//#define HOP_BY_HOP_BACKPRESSURE   //Preferred Usage: Discarded) not to be used!!
#endif
/* END NF BACKPRESSURE REALTED EXTENSION FEATURES AND OPTIONS */
/******************************************************************************/

/******************************************************************************/
//CGROUP Feature Related Extensions
/******************************************************************************/
// Preferred usage: Enable All 3 (USE_CGROUPS_PER_NF_INSTANCE, ENABLE_DYNAMIC_CGROUP_WEIGHT_ADJUSTMENT,USE_DYNAMIC_LOAD_FACTOR_FOR_CPU_SHARE)
#ifdef ENABLE_CGROUPS_FEATURE

/* Enable this flag to assign a distinct CGROUP for each NF instance (creates CGroup per NF instance) */
#define USE_CGROUPS_PER_NF_INSTANCE

/* To dynamically evaluate and periodically adjust weight on NFs cpu share */
#define ENABLE_DYNAMIC_CGROUP_WEIGHT_ADJUSTMENT

/* Enable Load*comp_cost (Helpful for TCP but not so for UDP (pktgen/Moongen) */
#define USE_DYNAMIC_LOAD_FACTOR_FOR_CPU_SHARE

/** Enable to store histogram of NFs computation costs: Note:  (in critical path, costing around 0.3 to 0.6Mpps) **/
#define STORE_HISTOGRAM_OF_NF_COMPUTATION_COST

#endif //#ENABLE_CGROUPS_FEATURE

#ifdef USE_CGROUPS_PER_NF_INSTANCE
/** Feature to enable Ordered Wakeup of NFs rather than blind serial wakeup **/
//#define ENABLE_ORDERED_NF_WAKEUP      // (Usage Preference: Disabled);
//#define ENABLE_ARBITER_MODE_WAKEUP      // (Usage Preference: Disabled);
#endif //USE_CGROUPS_PER_NF_INSTANCE

#ifdef STORE_HISTOGRAM_OF_NF_COMPUTATION_COST
#include "histogram.h"                          //Histogra Library
#endif //STORE_HISTOGRAM_OF_NF_COMPUTATION_COST
/******************************************************************************/

/******************************************************************************/
// NFV RESILIENCY related extensions, control macros and defines
/******************************************************************************/
#define ETHER_TYPE_RSYNC_DATA  (0x1000)
#define ETHER_TYPE_BFD         (0x2000)

#ifdef ENABLE_NFV_RESL
#define ENABLE_NF_MGR_IDENTIFIER    // Identifier for the NF Manager node
#define ENABLE_BFD                  // BFD management
#define ENABLE_SHADOW_RINGS         //enable shadow rings in the NF to save enqueued packets.
#define ENABLE_PER_SERVICE_MEMPOOL  //enable common mempool for all NFs on same service type.
#define ENABLE_REPLICA_STATE_UPDATE //enable feature to update (copy over NF state (_NF_STATE_MEMPOOL_NAME) info to local replic's state
#define ENABLE_REMOTE_SYNC_WITH_TX_LATCH    //enable feature to hold the Tx buffers until NF state/Tx ppkt table is updated.        (Remote Sync)
//#define RESL_UPDATE_MODE_PER_PACKET   //update mode Shadow Ring, Replica state, per flow TS for every packet
#ifndef RESL_UPDATE_MODE_PER_PACKET
#define RESL_UPDATE_MODE_PER_BATCH      //update mode Shadow Ring, Replica state, per flow TS for batch of packets
#define ENABLE_ND_MARKING_IN_NFS        //enable timer based ND_Marking in all NFs.
#endif

#ifdef ENABLE_REMOTE_SYNC_WITH_TX_LATCH
#define ENABLE_PER_FLOW_TS_STORE    //enable to store TS of the last processed/updated packet at each NF and last released packet at NF MGR.    (Remote Sync)
#define ENABLE_CHAIN_BYPASS_RSYNC_ISOLATION //enable to isolate chains that need no rsync to bypass same tx path and provide latency isolation
#define ENABLE_NF_PAUSE_TILL_OUTSTANDING_NDSYNC_COMMIT  //enable NF to pause (wait) if it has 1 outstanding ND to be synced and encounters second ND event in the interim.

#define PRIMARY_NODE     (0)
#define SECONDARY_NODE   (1)
#define PREDECESSOR_NODE (2)

#endif

/* Feature to enable PICO Replication mode of operation: Cut Rx while State update
 * How Pico works?: Synchrnous mode; till NF state is synced; NF cannot process packets;
 * Lock NF input and Output till 2P commit and after commit enable input and output
 * Replication Frequency: Every 1ms.
 * How to mimic it in REINFORCE?:
 * During NF sync halt Rx from sending packets to NFs and resume after state commit.
 * Note: Additionally disable Double Buffering and operate in single Buffer mode.
 * Remember: Input low packet rate to get reliable latency figures.
 * */

//#define MIMIC_PICO_REP
#ifdef MIMIC_PICO_REP
//#undef ENABLE_REMOTE_SYNC_WITH_TX_LATCH                 // this is needed feature for PICO as well, cannot be undefed..
//#undef ENABLE_PER_FLOW_TS_STORE                         //no need for TxTs
#undef ENABLE_CHAIN_BYPASS_RSYNC_ISOLATION              //no need for chain isolation
#undef ENABLE_NF_PAUSE_TILL_OUTSTANDING_NDSYNC_COMMIT   //no need for ndsync

//#define ENABLE_PICO_STATELESS_MODE                    // For stateless NF, there is no NF commit and packets are released directly..
//#define ENABLE_PICO_RX_NON_BLOCKING_MODE              //do not halt the rx_input while state transfer
//#define ENABLE_PICO_EXPLICT_NF_PAUSE_RESUME           //notify NF to pause till state commit and then resume ( issues: doesnt work due to race b/w pause action and resume message post)
#endif


/* Feature toe Enable FTMB mode of operation:
 * Note: NFLIB generate PALS; ignore VOR packets as there is only 1NF thread.
 * How FTMB works? Capture PAL for all shared variables in the NF processing.
 * PAL = 16byte packet.
 * Master transmits PAL, VOR and Data Packets to OL.
 * OL must track and commit all dependency PALs to stable storage before releasing the packet from OL.
 * Perform full NF checkpoint for every 50-200ms
 * How to mimic it in REINFORCE:
 *  setup #shared vars as variable in NF (shared with NFLIB)
 *  NFLib bypass all determinism/non-determinism check; performs continuous processing
 *  For each packet NFLib generates and transmits # PALs on the RSYNC PORT
 *  Encode LAST PAL ID on Packet and also transmit it on RSYNC PORT
 *  RSYNC PORT Node must commit differentiate PAL and Transmit it to to intended output node.
 */

//#define MIMIC_FTMB
#ifdef MIMIC_FTMB
#undef ENABLE_REPLICA_STATE_UPDATE
#undef ENABLE_REMOTE_SYNC_WITH_TX_LATCH
#undef ENABLE_PER_FLOW_TS_STORE
#undef ENABLE_CHAIN_BYPASS_RSYNC_ISOLATION
#undef ENABLE_NF_PAUSE_TILL_OUTSTANDING_NDSYNC_COMMIT
#endif

#ifdef ENABLE_SHADOW_RINGS
#ifdef RESL_UPDATE_MODE_PER_PACKET
#define SHADOW_RING_UPDATE_PER_PKT
#else
#define SHADOW_RING_UPDATE_PER_BATCH
#endif
#endif

#ifdef ENABLE_PER_FLOW_TS_STORE
//#define ENABLE_NFLIB_PER_FLOW_TS_STORE    //enable TS update for each NF
//#define ENABLE_PER_FLOW_TXTS_MAP_ENTRY

#ifdef ENABLE_NFLIB_PER_FLOW_TS_STORE
#ifdef RESL_UPDATE_MODE_PER_PACKET
#define PER_FLOW_TS_UPDATE_PER_PKT
#else
#define PER_FLOW_TS_UPDATE_PER_BATCH
#endif
#endif
#endif

#ifdef ENABLE_REPLICA_STATE_UPDATE
#ifdef RESL_UPDATE_MODE_PER_PACKET
#define REPLICA_STATE_UPDATE_MODE_PER_PACKET
#else
#define REPLICA_STATE_UPDATE_MODE_PER_BATCH
#endif
#endif

#ifdef ENABLE_REMOTE_SYNC_WITH_TX_LATCH
#define ONVM_NUM_RSYNC_THREADS ((int)1)
#define ONVM_NUM_RSYNC_PORTS    (RTE_MAX_ETHPORTS)      //(3)     //2 + 1 for rest
#define USE_BATCHED_RSYNC_TRANSACTIONS  (1) //enable single wait for multiple remote transactions in a round;
//#define BYPASS_WAIT_ON_TRANSACTIONS       //to bypass wait on transactions and assume send=success; (Do not enable!)
//#define ENABLE_OPPROTUNISTIC_MAX_POLL       //Enable to opportunistically maxout packets per transaction; induce more ppkt delay but gain throughput improvement.
#define ENABLE_RSYNC_WITH_DOUBLE_BUFFERING_MODE        //Enable double buffering scheme such that we minimize the effective time on wait_for_trans_complete() and allow to send more (double) the transactions

#define _TX_RSYNC_TX_PORT_RING_NAME     "_TX_RSYNC_TX_%u_PORT"  //"_TX_RSYNC_TX_PORT"
#define TX_RSYNC_TX_PORT_RING_SIZE      (NF_QUEUE_RINGSIZE*2) //(NF_QUEUE_RINGSIZE*2) //(NF_QUEUE_RINGSIZE) //RTE_MP_TX_DESC_DEFAULT
#define _TX_RSYNC_TX_LATCH_RING_NAME    "_TX_RSYNC_TX_%u_LATCH" //"_TX_RSYNC_TX_LATCH"
#define TX_RSYNC_TX_LATCH_RING_SIZE     (NF_QUEUE_RINGSIZE*2)   //(8*1024)
#define _TX_RSYNC_NF_LATCH_RING_NAME    "_TX_RSYNC_NF_%u_LATCH" //"_TX_RSYNC_NF_LATCH"
#define TX_RSYNC_NF_LATCH_RING_SIZE     (NF_QUEUE_RINGSIZE)   //(8*1024)

#ifdef ENABLE_RSYNC_WITH_DOUBLE_BUFFERING_MODE
#define _TX_RSYNC_TX_LATCH_DB_RING_NAME    "_TX_RSYNC_TX_%u_LATCH_DB" //"_TX_RSYNC_TX_LATCH"
#define _TX_RSYNC_NF_LATCH_DB_RING_NAME    "_TX_RSYNC_NF_%u_LATCH_DB" //"_TX_RSYNC_NF_LATCH"
#define TX_RSYNC_TX_LATCH_DB_RING_SIZE  (TX_RSYNC_TX_LATCH_RING_SIZE)
#define TX_RSYNC_NF_LATCH_DB_RING_SIZE  (TX_RSYNC_NF_LATCH_RING_SIZE)
//#define ENABLE_SIMPLE_DOUBLE_BUFFERING_MODE   //Approach 1: Simple double buffer mode
//#define ENABLE_OPTIMAL_DOUBLE_BUFFERING_MODE  //Approach 2: More optimal/greedy double buffering mode
#define ENABLE_RSYNC_MULTI_BUFFERING   (2)      //Approach 3: Multiple counter (value) of buffers that can be exhausted before switching to primary buffer
//enable to internally check and clear transactions with elapsed timers ( > 2RTT)
#define TX_RSYNC_AUTOCLEAR_ELAPSED_TRANSACTIONS_TIMERS    //Note: Must enable all the time
//Double Buffering scheme must use Batched Transactions
#ifndef USE_BATCHED_RSYNC_TRANSACTIONS
#define USE_BATCHED_RSYNC_TRANSACTIONS
#endif
#endif
#else
#define ONVM_NUM_RSYNC_THREADS ((int)0)
#endif

#define _NF_STATE_MEMPOOL_NAME "NF_STATE_MEMPOOL"
#define _NF_STATE_SIZE      (64*1024)
#define _NF_STATE_CACHE     (8)

#ifdef ENABLE_SHADOW_RINGS
#define CLIENT_SHADOW_RING_SIZE     (ONVM_PACKETS_BATCH_SIZE*2)
#endif //ENABLE_SHADOW_RINGS

#ifdef ENABLE_PER_SERVICE_MEMPOOL
#define _SERVICE_STATE_MEMPOOL_NAME "SVC_STATE_MEMPOOL"
#define _SERVICE_STATE_SIZE      (1024*1024) //(16*1024)  //4MB <for DPI case>.
#define _SERVICE_STATE_CACHE     (8)
#endif

#ifdef ENABLE_PER_FLOW_TS_STORE
#define _PER_FLOW_TS_MEMPOOL_NAME "PF_TS_MEMPOOL"
#define _PER_FLOW_TS_SIZE      (16*1024) //((16*1024)+1024)  //reduced from 64K(8K entires)  to 16K(2K entries) for now: add 1K for bookkeeping dirty_map.
#define _PER_FLOW_TS_CACHE     (8)
#define _PER_FLOW_TS_CACHE_MAX_ENTRIES      ((_PER_FLOW_TS_SIZE)/sizeof(uint64_t))
#endif


#define MAX_ACTIVE_CLIENTS  (MAX_NFS>>1)
#define MAX_STANDBY_CLIENTS  (MAX_NFS - MAX_ACTIVE_CLIENTS)
#define ACTIVE_NF_MASK   (MAX_ACTIVE_CLIENTS-1)

typedef struct onvm_per_flow_ts_info {
        uint64_t ts;
}  __attribute__((__packed__)) onvm_per_flow_ts_info_t;
#else
#ifndef ONVM_NUM_RSYNC_THREADS
#define ONVM_NUM_RSYNC_THREADS ((int)0)
#endif
#endif  //#ifdef ENABLE_NFV_RESL

//Note the NUM_OF_QUEUES in PORT seem to be only 16; so ensure the queue value with MAX_NFS ( but unforunately the init_port() doesnt fail.
#ifdef ENABLE_BFD
#define PIGGYBACK_BFD_ON_ACTIVE_PORT_TRAFFIC            //Optimization: this feature enables to track active status of remote port from ongoing traffic and only after 'x' time of idle status initiates check on BFD
#define BFD_TX_PORT_QUEUE_ID        (ONVM_NUM_TX_THREADS)//((MAX_NFS/2)+1)
#else
#define BFD_TX_PORT_QUEUE_ID   (ONVM_NUM_TX_THREADS-1)// (0)//(MAX_NFS/2)
#endif

#ifdef ENABLE_NFV_RESL //ENABLE_REMOTE_SYNC_WITH_TX_LATCH
//for external events
#define RSYNC_TX_PORT_QUEUE_ID_0    (BFD_TX_PORT_QUEUE_ID+1)
//for internal thread
#define RSYNC_TX_PORT_QUEUE_ID_1    (RSYNC_TX_PORT_QUEUE_ID_0+1)
#define RSYNC_TX_OUT_PORT       (2)
#define ONVM_NF_MGR_TX_QUEUES   (4) //1 for BFD, 2 for RSYNC
#else
#define ONVM_NF_MGR_TX_QUEUES   (0)
#endif

/* Special defines for Remote Rsync Control (as per FS-2 config) */
#define PRIMARY_OUT_PORT    (1)
#define SECONDARY_OUT_PORT  (2)

#ifdef MIMIC_FTMB
//#undef ENABLE_REMOTE_SYNC_WITH_TX_LATCH    //disable feature to hold the Tx buffers until NF state/Tx ppkt table is updated.        (Remote Sync)
//#undef ENABLE_PER_FLOW_TS_STORE    //disable  store TS of the last processed/updated packet at each NF and last released packet at NF MGR.    (Remote Sync)
//#undef ENABLE_CHAIN_BYPASS_RSYNC_ISOLATION // disable isolate chains that need no rsync to bypass same tx path and provide latency isolation
#endif

// END OF FEATURE EXTENSIONS FOR NFV_RESILEINCY
/******************************************************************************/

/** Structure to represent the Dirty state map for the in-memory NF state **/
typedef struct dirty_mon_state_map_tbl {
        uint64_t dirty_index;
        // Bit index to every 1K LSB=0-1K, MSB=63-64K
}dirty_mon_state_map_tbl_t;

typedef struct dirty_mon_state_map_tbl_txts {
        uint8_t dirty_index;
        // Bit index to every 1K LSB=0-1K, MSB=63-64K
}dirty_mon_state_map_tbl_txts_t;
/* Note: To optimize, use two level indexing.
 * First index uint64_t designate which 2nd level indexes are dirtied
 * Second level index sixty-four of the uint64_t index the chunk in the memory
 * Total space needed for lookup = (1+64)*8   = 520 bytes ( reserve 1KB)
 * Total index bits supported in lookup = 64*64 = 4K indices
 * To support 8 Bytes per flow entry and 4K entries => 32KB memory.
 * 32KB indexed by 4K indexes => 8 Bytes
 */
/******************************************************************************/
#ifdef ENABLE_VXLAN
#define DISTRIBUTED_NIC_PORT 1 // NIC port connects to the remote server
#endif //ENABLE_VXLAN
/******************************************************************************/

/******************************************************************************/
/*                   Generic Helper Functions                                 */
/******************************************************************************/
#define SET_BIT(x,bitNum) ((x)|=(1<<(bitNum-1)))
static inline void set_bit(long *x, unsigned bitNum) {
    *x |= (1L << (bitNum-1));
}

#define CLEAR_BIT(x,bitNum) ((x) &= ~(1<<(bitNum-1)))
static inline void clear_bit(long *x, unsigned bitNum) {
    *x &= (~(1L << (bitNum-1)));
}

#define TOGGLE_BIT(x,bitNum) ((x) ^= (1<<(bitNum-1)))
static inline void toggle_bit(long *x, unsigned bitNum) {
    *x ^= (1L << (bitNum-1));
}
#define TEST_BIT(x,bitNum) ((x) & (1<<(bitNum-1)))
static inline long test_bit(long x, unsigned bitNum) {
    return (x & (1L << (bitNum-1)));
}

static inline long is_upstream_NF(long chain_throttle_value, long chain_index) {
#ifndef HOP_BY_HOP_BACKPRESSURE
        long chain_index_value = 0;
        SET_BIT(chain_index_value, chain_index);
        CLEAR_BIT(chain_throttle_value, chain_index);
        return ((chain_throttle_value > chain_index_value)? (1):(0) );
#else
        long chain_index_value = 0;
        SET_BIT(chain_index_value, (chain_index+1));
        return ((chain_throttle_value & chain_index_value));
        //return is_immediate_upstream_NF(chain_throttle_value,chain_index);
#endif //HOP_BY_HOP_BACKPRESSURE
        //1 => NF component at chain_index is an upstream component w.r.t where the bottleneck is seen in the chain (do not drop/throttle)
        //0 => NF component at chain_index is an downstream component w.r.t where the bottleneck is seen in the chain (so drop/throttle)
}
static inline long is_immediate_upstream_NF(long chain_throttle_value, long chain_index) {
#ifdef HOP_BY_HOP_BACKPRESSURE
        long chain_index_value = 0;
        SET_BIT(chain_index_value, (chain_index+1));
        return ((chain_throttle_value & chain_index_value));
#else
        return is_upstream_NF(chain_throttle_value,chain_index);
#endif  //HOP_BY_HOP_BACKPRESSURE
        //1 => NF component at chain_index is an immediate upstream component w.r.t where the bottleneck is seen in the chain (do not drop/throttle)
        //0 => NF component at chain_index is an downstream component w.r.t where the bottleneck is seen in the chain (so drop/throttle)
}

static inline long get_index_of_highest_set_bit(long x) {
        long next_set_index = 0;
        //SET_BIT(chain_index_value, chain_index);
        //while ((1<<(next_set_index++)) < x);
        //for(; (x > (1<<next_set_index));next_set_index++)
        for(; (x >= (1<<next_set_index));next_set_index++);
        return next_set_index;
}

//flag operations that should be used on onvm_pkt_meta
#define ONVM_CHECK_BIT(flags, n) !!((flags) & (1 << (n)))
#define ONVM_SET_BIT(flags, n) ((flags) | (1 << (n)))
#define ONVM_CLEAR_BIT(flags, n) ((flags) & (0 << (n)))

//extern uint8_t rss_symmetric_key[40];
//size of onvm_pkt_meta cannot exceed 8 bytes, so how to add onvm_service_chain* sc pointer?
struct onvm_pkt_meta {
        uint8_t action; /* Action to be performed */
        uint8_t destination; /* where to go next */
        uint8_t src; /* who processed the packet last */
        uint8_t chain_index;    /*index of the current step in the service chain*/
#ifdef ENABLE_FT_INDEX_IN_META
        uint16_t ft_index;       /* Index of the FT if the packet is mapped in SDN Flow Table */
#endif
	uint8_t flags; /* bits for custom NF data. Use with caution to prevent collisions from different NFs. */
        uint8_t reserved_word; /* reserved byte */
        //reserved word usage: 0x01==Need NF RSYNC; 0x02=BYPASS RSYNC on TX;
};//__attribute__((__aligned__(ONVM_CACHE_LINE_SIZE)));
#define NF_NEED_ND_SYNC (0x01)
#define NF_BYPASS_RSYNC (0x02)

static inline struct onvm_pkt_meta* onvm_get_pkt_meta(struct rte_mbuf* pkt) {
        return (struct onvm_pkt_meta*)&pkt->udata64;
}

static inline uint8_t onvm_get_pkt_chain_index(struct rte_mbuf* pkt) {
        return ((struct onvm_pkt_meta*)&pkt->udata64)->chain_index;
}

/*
 * Shared port info, including statistics information for display by server.
 * Structure will be put in a memzone.
 * - All port id values share one cache line as this data will be read-only
 * during operation.
 * - All rx statistic values share cache lines, as this data is written only
 * by the server process. (rare reads by stats display)
 * - The tx statistics have values for all ports per cache line, but the stats
 * themselves are written by the NFs, so we have a distinct set, on different
 * cache lines for each NF to use.
 */
/*******************************Data Structures*******************************/
/*
 * Local buffers to put packets in, used to send packets in bursts to the
 * NFs or to the NIC
 */
struct packet_buf {
        struct rte_mbuf *buffer[PACKET_READ_SIZE];
        uint16_t count;
};

/*  
 * Packets may be transported by a tx thread or by an NF.
 * This data structure encapsulates data specific to  
 * tx threads. 
 */
struct tx_thread_info {
        unsigned first_nf;
        unsigned last_nf;
        struct packet_buf *port_tx_bufs;
};

/*
 * Generic data struct that tx threads and nfs both use.
 * Allows pkt functions to be shared
 * */
struct queue_mgr {
        unsigned id;
        enum {NF, MGR} mgr_type_t;
        union {
                struct tx_thread_info *tx_thread_info;
                struct packet_buf *to_tx_buf;
        };
        struct packet_buf *nf_rx_bufs;
};

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
        struct ether_addr mac[RTE_MAX_ETHPORTS];
        uint8_t down_status[RTE_MAX_ETHPORTS]; //store BFD live=0/dead(1,2) status
        volatile struct rx_stats rx_stats;
        volatile struct tx_stats tx_stats;
};


/** Thread state. This specifies which NFs the thread will handle and
 *  includes the packet buffers used by the thread for NFs and ports.
 *  TODO: MERGE or NOT with new code tx_thread_info and queue_mgr sructs
 */
struct thread_info {
       unsigned queue_id;
       unsigned first_cl;   //inclusive [
       unsigned last_cl;    //exclusive ) so f_cl=1 and l_cl=4 -> 1,2,3 only.
       /* FIXME: This is confusing since it is non-inclusive. It would be
        *        better to have this take the first client and the number
        *        of consecutive nfs after it to handle.
        */
       struct packet_buf *nf_rx_buf;
       struct packet_buf *port_tx_buf;
#ifdef ENABLE_CHAIN_BYPASS_RSYNC_ISOLATION
       struct packet_buf *port_tx_direct_buf;   //use this buffer to directly output bypassing the rsync if enabled!
#endif
};

struct onvm_nf_info;
/* Function prototype for NF packet handlers */
typedef int(*pkt_handler_func)(struct rte_mbuf *pkt, struct onvm_pkt_meta *meta, __attribute__ ((unused)) struct onvm_nf_info *nf_info);
/* Function prototype for NF callback handlers */
typedef int(*callback_handler_func)(__attribute__ ((unused)) struct onvm_nf_info *nf_info);
/* Function prototype for NFs running advanced rings */
typedef void(*advanced_rings_func)(struct onvm_nf_info *nf_info);
/* Function prototype for NFs that want extra initalization/setup before running */
typedef void(*setup_func)(struct onvm_nf_info *nf_info);

/* Information needed to initialize a new NF child thread */
struct onvm_nf_scale_info {
        struct onvm_nf_info *parent;
        uint16_t instance_id;
        uint16_t service_id;
        const char *tag;
        void *data;
        setup_func setup_func;
        pkt_handler_func pkt_func;
        callback_handler_func callback_func;
        advanced_rings_func adv_rings_func;
};

#ifdef INTERRUPT_SEM
/** NFs wakeup Info: used by manager to update NFs pool and wakeup stats
 */ 
struct wakeup_info {
        unsigned first_client;
        unsigned last_client;
        uint64_t num_wakeups;
};
#endif //INTERRUPT_SEM


#ifdef NF_BACKPRESSURE_APPROACH_1
typedef struct bottleneck_ft_data {
        uint16_t chain_index;           //index of NF (bottleneck) in the chain
         struct onvm_flow_entry* bft;   //flow_entry field
}bottleneck_ft_data_t;
typedef struct bottleneck_ft_info {
        uint16_t bft_count;         // num of entries in the bft[]
        uint16_t r_h;               // read_head in the bft[]
        uint16_t w_h;               // write head in the bft[]
        uint16_t max_len;           // Max size/count of bft[]
        //struct onvm_flow_entry* bft[NF_QUEUE_RINGSIZE];
        bottleneck_ft_data_t bft[NF_QUEUE_RINGSIZE*2+1];
}bottlenect_ft_info_t;

#endif //NF_BACKPRESSURE_APPROACH_1

/*
 * Define a nf(client) structure with all needed info, including
 * stats from the nfs.
 */
struct onvm_nf {
        struct rte_ring *rx_q;
        struct rte_ring *tx_q;
        struct rte_ring *msg_q;
        struct onvm_nf_info *info;
        uint16_t instance_id;
        /* Advanced ring mode or packet handler mode */
        uint8_t nf_mode;
        /* Instance ID of parent NF or 0 */
        uint16_t parent;
        /* Struct for NF to NF communication (NF tx) */
        struct queue_mgr *nf_tx_mgr;

        /* NF specifc functions */
        pkt_handler_func nf_pkt_function;
        callback_handler_func nf_callback_function;
        advanced_rings_func nf_advanced_rings_function;
        setup_func nf_setup_function;

        /*
         * Define a structure with stats from the NFs.
         *
         * These stats hold how many packets the NF will actually receive, send,
         * and how many packets were dropped because the NF's queue was full.
         * The port-info stats, in contrast, record how many packets were received
         * or transmitted on an actual NIC port.
         */
        struct {
                volatile uint64_t rx;
                volatile uint64_t rx_drop;
                volatile uint64_t tx;
                volatile uint64_t tx_drop;
#ifdef ENABLE_NF_TX_STAT_LOGS
                volatile uint64_t tx_buffer;
                volatile uint64_t tx_returned;
                volatile uint64_t act_out;
                volatile uint64_t act_tonf;
                volatile uint64_t act_drop;
                volatile uint64_t act_next;
                volatile uint64_t act_buffer;   //Note: this doesn't seem be updated anywhere other than printing in stats.
#endif

#ifdef ENABLE_NF_WAKE_NOTIFICATION_COUNTER
                volatile uint64_t wakeup_count; //maintained by wake_mgr
#endif
#ifdef ENABLE_NF_YIELD_NOTIFICATION_COUNTER
                volatile uint64_t yield_count;  //maintained by NF
#endif
#ifdef INTERRUPT_SEM
                volatile uint64_t comp_cost;    //maintaned by NF
#endif

#if defined (NF_BACKPRESSURE_APPROACH_1)
                volatile uint64_t bkpr_drop;
#endif //NF_BACKPRESSURE_APPROACH_1
#if defined (BACKPRESSURE_EXTRA_DEBUG_LOGS)
                uint16_t max_rx_q_len;
                uint16_t max_tx_q_len;
                uint16_t bkpr_count;
#endif //defined (BACKPRESSURE_EXTRA_DEBUG_LOGS)
        } stats;
#ifdef ENABLE_NF_BASED_BKPR_MARKING
        //status: not marked=0/marked for enqueue=1/enqueued as bottleneck=2
        //BOTTLENECK_NF_STATUS_RESET, BOTTLENECK_NF_STATUS_WAIT_ENQUEUED, BOTTLENECK_NF_STATUS_DROP_MARKED
        uint16_t is_bottleneck;
#ifdef USE_BKPR_V2_IN_TIMER_MODE
        // store the time when the NF is first marked as bottleneck.
        onvm_time_t s_time;
#endif
#endif
#ifdef NF_BACKPRESSURE_APPROACH_1
        bottlenect_ft_info_t bft_list;
#endif //defined (NF_BACKPRESSURE_APPROACH_1)

        /* mutex and semaphore name for NFs to wait on */
#ifdef INTERRUPT_SEM
        const char *sem_name;
        key_t shm_key;
        //0=running; 1=blocked_on_rx (no pkts to process); 2=blocked_on_tx (cannot push packets)
        rte_atomic16_t *volatile shm_server;

#ifdef USE_SEMAPHORE
        sem_t *mutex;
#endif

#ifdef NF_BACKPRESSURE_APPROACH_2
        uint8_t throttle_this_upstream_nf; // Flag to indicate whether this NF needs to be (throttled) blocked from getting scheduled;
#if 0
        uint64_t throttle_count;           // Counter keeping track of how many times the NF is marked to be throttled.
#endif
#endif // NF_BACKPRESSURE_APPROACH_2
#endif //INTERRUPT_SEM

#ifdef ENABLE_NFV_RESL
        // shared state exclusively between the active and standby NFs
        void *nf_state_mempool;
#ifdef ENABLE_PER_SERVICE_MEMPOOL
        // shared state between all the NFs of the same service type; Note: Mostly not required here in client[] structure
        void *service_state_pool;
#endif

#ifdef ENABLE_PER_FLOW_TS_STORE
        //Storehouse for each flows last processed packets TS.
        void *per_flow_ts_info;
#endif

#ifdef ENABLE_SHADOW_RINGS
        struct rte_ring *rx_sq;
        struct rte_ring *tx_sq;
#endif
#endif //#ifdef ENABLE_NFV_RESL
} __rte_cache_aligned;

#if defined (INTERRUPT_SEM) && defined (USE_SOCKET)
extern int onvm_socket_id;
#endif

#if defined (INTERRUPT_SEM) && defined (USE_ZMQ)
extern void *zmq_ctx;
extern void *onvm_socket_id;
extern void *onvm_socket_ctx;
#endif

/*
 * Define a structure to describe one NF
 * This structure is available in the NF when processing packets or executing the callback.
 */
struct onvm_nf_info {
        uint16_t instance_id;
        uint16_t service_id;
        //volatile
        uint8_t status;    //moved to status to ensure status read is not cached and is updated across different cores; but seeing no major difference in sync refresh time.
        const char *tag;
        void *data;        //TODO: figure out what it is added for? 
        pid_t pid;

#ifdef ENABLE_NFV_RESL
        void *nf_state_mempool;     // shared state exclusively between the active and standby NFs (Per Flow State)
#ifdef ENABLE_PER_SERVICE_MEMPOOL
        void *service_state_pool;   // shared state between all the NFs of the same service type (Global Coherent Sate)
#endif
#endif //#ifdef ENABLE_NFV_RESL

        uint32_t comp_cost;     //indicates the computation cost of NF in num_of_cycles

#if defined (USE_CGROUPS_PER_NF_INSTANCE)
        //char cgroup_name[256];
        uint32_t cpu_share;     //indicates current share of NFs cpu
        uint32_t core_id;       //indicates the core ID the NF is running on
        uint32_t load;          //indicates instantaneous load on the NF ( = num_of_packets on the rx_queue + pkts dropped on Rx)
        uint32_t avg_load;      //indicates the average load on the NF
        uint32_t svc_rate;      //indicates instantaneous service rate of the NF ( = num_of_packets processed in the sampling period)

#ifdef ENABLE_ARBITER_MODE_WAKEUP
        uint64_t exec_period;   //indicates the number_of_cycles/time period alloted for execution in this epoch == normalized_load*comp_cost -- how to get this metric: (total_cycles_in_epoch)*(total_load_on_core)/(load_of_nf)
#endif
#if 0   //unused and unimportant parameters: Note::TODO: also need to be cleaned up in other internal structures used in onvm_nf.c/h and stats_snapshot
        uint32_t avg_svc;       //indicates the average service rate of the NF
        uint32_t comp_pkts;     //[usage: TBD] indicates the number of pkts processed by NF over specific sampling period (demand (new pkts arrival) = Rx, better? or serviced (new pkts sent out) = Tx better?)
        uint32_t drop_rate;     //indicates the drops observed within the sampled period.
#endif
#endif

#ifdef STORE_HISTOGRAM_OF_NF_COMPUTATION_COST
        histogram_v2_t ht2;
#endif  //STORE_HISTOGRAM_OF_NF_COMPUTATION_COST

#ifdef ENABLE_ECN_CE
        histogram_v2_t ht2_q;
#endif

#ifdef ENABLE_NF_PAUSE_TILL_OUTSTANDING_NDSYNC_COMMIT
        volatile uint8_t bNDSycn;
        uint64_t bLastPktId;
#ifdef  __DEBUG_NDSYNC_LOGS__
        int64_t max_nd, min_nd, avg_nd, delta_nd;
#endif
#endif

};

/*
 * Define a structure to describe a service chain entry
 */
struct onvm_service_chain_entry {
        uint16_t destination;
        //denotes a service Id or Instance Id
        uint8_t action;
        //denotes forwarding action type.
        uint8_t service;
        //backup service id as set by policy in destination, when destination is InstanceID
};

struct onvm_service_chain {
        struct onvm_service_chain_entry sc[ONVM_MAX_CHAIN_LENGTH+1];
        uint8_t chain_length;
        uint8_t ref_cnt;
#ifdef ENABLE_NF_BACKPRESSURE
        volatile uint8_t highest_downstream_nf_index_id;     // bit index of each NF in the chain that is overflowing
#if defined(NF_BACKPRESSURE_APPROACH_2) || defined(ENABLE_NF_BASED_BKPR_MARKING)
        uint8_t nf_instances_mapped; //set when all nf_instances are populated in the below array
        uint8_t nf_instance_id[ONVM_MAX_CHAIN_LENGTH+1];
#endif //NF_BACKPRESSURE_APPROACH_2
#endif //ENABLE_NF_BACKPRESSURE
};

/* define common names for structures shared between server and NF */
#define MP_NF_RXQ_NAME "MProc_NF_%u_RX"
#define MP_NF_TXQ_NAME "MProc_NF_%u_TX"
#define _NF_MSG_QUEUE_NAME "NF_%u_MSG_QUEUE"
#define PKTMBUF_POOL_NAME "MProc_pktmbuf_pool"
#define MZ_PORT_INFO "MProc_port_info"
#define MZ_NF_INFO "MProc_nf_info"
#define MZ_SERVICES_INFO "MProc_services_info"
#define MZ_NF_PER_SERVICE_INFO "MProc_nf_per_service_info"
#define MZ_SCP_INFO "MProc_scp_info"
#define MZ_FTP_INFO "MProc_ftp_info"

#define _MGR_MSG_QUEUE_NAME "MGR_MSG_QUEUE"     //NF --> MGR messages
#define _NF_MSG_QUEUE_NAME "NF_%u_MSG_QUEUE"
#define _MGR_RSP_QUEUE_NAME "MGR_RSP_QUEUE"     //NF --> MGR response ( for sync requests)
#define _NF_MEMPOOL_NAME "NF_INFO_MEMPOOL"      //Mempool for nf_info strcutures
#define _NF_MSG_POOL_NAME "NF_MSG_MEMPOOL"      //Mempool for NF-->MGR messages and responses.

/* interrupt semaphore specific updates */
#ifdef INTERRUPT_SEM
#define SHMSZ 4                         // size of shared memory segement (page_size)
#define KEY_PREFIX 123                  // prefix len for key

#ifdef USE_SEMAPHORE
#define MP_CLIENT_SEM_NAME "MProc_NF_%u_SEM"
#endif //USE_SEMAPHORE
#ifdef USE_POLL_MODE
#define MP_CLIENT_SEM_NAME "MProc_NF_%u_SEM"
#endif //USE_POLL_MODE


//1000003 1000033 1000037 1000039 1000081 1000099 1000117 1000121 1000133
//#define SAMPLING_RATE 1000000           // sampling rate to estimate NFs computation cost
#define SAMPLING_RATE 1000003           // sampling rate to estimate NFs computation cost
#define ONVM_SPECIAL_NF 0               // special NF for flow table entry management
#endif


#ifdef ENABLE_ARBITER_MODE
#define ONVM_NUM_WAKEUP_THREADS ((int)0)       //1 ( Must remove this as well)
#else
#define ONVM_NUM_WAKEUP_THREADS ((int)1)       //1 ( Must remove this as well)
#endif

/* common names for NF states */
#define NF_WAITING_FOR_ID   (0x00)              // First step in startup process, doesn't have ID confirmed by manager yet
#define NF_STARTING         (0x01)              // When a NF is in the startup process and already has an id
#define NF_WAITING_FOR_RUN  (0x02)              // When NF asserts itself to run and ready to process packets ; requests manager to be considered for delivering packets.
#define NF_RUNNING          (0x03)              // Running normally
#define NF_PAUSED_BIT       (0x04)              // Value 4 => Third Bit indicating Paused Status
#define NF_WT_ND_SYNC_BIT   (0x08)              // Value=8 => 4th bit is set indicating paused with special status of wiat on ND Sync If NF is Waiting for ND SYNC ( Paused with speacial case of wait)
#define NF_PAUSED  (NF_PAUSED_BIT|NF_RUNNING)   // NF is not receiving packets, but may in the future
#define NF_WT_ND_SYNC  (NF_PAUSED_BIT|NF_WT_ND_SYNC_BIT)   // NF is paused and waiting till ND SYNC is not complete
#define NF_STOPPED          (0x10)              // NF has stopped and in the shutdown process
#define NF_ID_CONFLICT      (0x20)              // NF is trying to declare an ID already in use
#define NF_NO_IDS           (0X40)              // There are no available IDs for this NF
#define NF_NO_ID -1
#define ONVM_NF_HANDLE_TX   (0)     // should be true if NFs primarily pass packets to each other

/*
 * Given the rx queue name template above, get the queue name
 */
static inline const char *
get_rx_queue_name(unsigned id) {
        /* buffer for return value. Size calculated by %u being replaced
         * by maximum 3 digits (plus an extra byte for safety) */
        static char buffer[sizeof(MP_NF_RXQ_NAME) + 2];

#ifdef ENABLE_NFV_RESL
        snprintf(buffer, sizeof(buffer) - 1, MP_NF_RXQ_NAME, id&(MAX_ACTIVE_CLIENTS-1));
#else
        snprintf(buffer, sizeof(buffer) - 1, MP_NF_RXQ_NAME, id);
#endif
        return buffer;
}

/*
 * Given the tx queue name template above, get the queue name
 */
static inline const char *
get_tx_queue_name(unsigned id) {
        /* buffer for return value. Size calculated by %u being replaced
         * by maximum 3 digits (plus an extra byte for safety) */
        static char buffer[sizeof(MP_NF_TXQ_NAME) + 2];
#ifdef ENABLE_NFV_RESL
        snprintf(buffer, sizeof(buffer) - 1, MP_NF_TXQ_NAME, id&(MAX_ACTIVE_CLIENTS-1));
#else
        snprintf(buffer, sizeof(buffer) - 1, MP_NF_TXQ_NAME, id);
#endif
        return buffer;
}
/*
 * Given the name template above, get the mgr -> NF msg queue name
 */
static inline const char *
get_msg_queue_name(unsigned id) {
        /* buffer for return value. Size calculated by %u being replaced
         * by maximum 3 digits (plus an extra byte for safety) */
        static char buffer[sizeof(_NF_MSG_QUEUE_NAME) + 2];

        snprintf(buffer, sizeof(buffer) - 1, _NF_MSG_QUEUE_NAME, id);
        return buffer;
}

#ifdef ENABLE_NFV_RESL
#ifdef ENABLE_SHADOW_RINGS
#define MP_NF_RXSQ_NAME "MProc_NF_%u_RX_S"
#define MP_NF_TXSQ_NAME "MProc_NF_%u_TX_S"
static inline const char *
get_rx_squeue_name(unsigned id) {
        static char buffer[sizeof(MP_NF_RXSQ_NAME) + 2];
        snprintf(buffer, sizeof(buffer) - 1, MP_NF_RXSQ_NAME, id&(MAX_ACTIVE_CLIENTS-1));
        return buffer;
}

static inline const char *
get_tx_squeue_name(unsigned id) {
        static char buffer[sizeof(MP_NF_TXSQ_NAME) + 2];
        snprintf(buffer, sizeof(buffer) - 1, MP_NF_TXSQ_NAME, id&(MAX_ACTIVE_CLIENTS-1));
        return buffer;
}
#endif  //ENABLE_SHADOW_RINGS

#ifdef ENABLE_REMOTE_SYNC_WITH_TX_LATCH
static inline const char *
get_rsync_tx_port_ring_name(unsigned id) {
        static char buffer[sizeof(_TX_RSYNC_TX_PORT_RING_NAME) + 2];
        snprintf(buffer, sizeof(buffer) - 1, _TX_RSYNC_TX_PORT_RING_NAME, id);
        return buffer;
}
static inline const char *
get_rsync_tx_tx_state_latch_ring_name(unsigned id) {
        static char buffer[sizeof(_TX_RSYNC_TX_LATCH_RING_NAME) + 2];
        snprintf(buffer, sizeof(buffer) - 1, _TX_RSYNC_TX_LATCH_RING_NAME, id);
        return buffer;
}
static inline const char *
get_rsync_tx_nf_state_latch_ring_name(unsigned id) {
        static char buffer[sizeof(_TX_RSYNC_NF_LATCH_RING_NAME) + 2];
        snprintf(buffer, sizeof(buffer) - 1, _TX_RSYNC_NF_LATCH_RING_NAME, id);
        return buffer;
}
#ifdef ENABLE_RSYNC_WITH_DOUBLE_BUFFERING_MODE
static inline const char *
get_rsync_tx_tx_state_latch_db_ring_name(unsigned id) {
        static char buffer[sizeof(_TX_RSYNC_TX_LATCH_DB_RING_NAME) + 2];
        snprintf(buffer, sizeof(buffer) - 1, _TX_RSYNC_TX_LATCH_DB_RING_NAME, id);
        return buffer;
}
static inline const char *
get_rsync_tx_nf_state_latch_db_ring_name(unsigned id) {
        static char buffer[sizeof(_TX_RSYNC_NF_LATCH_DB_RING_NAME) + 2];
        snprintf(buffer, sizeof(buffer) - 1, _TX_RSYNC_NF_LATCH_DB_RING_NAME, id);
        return buffer;
}
#endif
#endif

static inline unsigned
get_associated_active_or_standby_nf_id(unsigned nf_id) {
        if(nf_id&MAX_ACTIVE_CLIENTS) {
                return (nf_id & ACTIVE_NF_MASK);
        }
        return (nf_id|MAX_ACTIVE_CLIENTS);
}
static inline unsigned
is_primary_active_nf_id(unsigned nf_id) {
        return ((nf_id ^ MAX_ACTIVE_CLIENTS) & MAX_ACTIVE_CLIENTS); //return (!(nf_id & MAX_ACTIVE_CLIENTS)); //return ((nf_id < MAX_ACTIVE_CLIENTS));
}
static inline unsigned
is_secondary_active_nf_id(unsigned nf_id) {
        return ((nf_id & MAX_ACTIVE_CLIENTS));
}
static inline unsigned
get_associated_active_nf_id(unsigned nf_id) {
        return (nf_id & ACTIVE_NF_MASK);
}
static inline unsigned
get_associated_standby_nf_id(unsigned nf_id) {
        return (nf_id | MAX_ACTIVE_CLIENTS);
}
#endif

#ifdef INTERRUPT_SEM
/*
 * Given the rx queue name template above, get the key of the shared memory
 */
static inline key_t
get_rx_shmkey(unsigned id)
{
        return KEY_PREFIX * 10 + id;
}

/*
 * Given the sem name template above, get the sem name
 */
static inline const char *
get_sem_name(unsigned id)
{
        /* buffer for return value. Size calculated by %u being replaced
         * by maximum 3 digits (plus an extra byte for safety) */
        static char buffer[sizeof(MP_CLIENT_SEM_NAME) + 2];

        snprintf(buffer, sizeof(buffer) - 1, MP_CLIENT_SEM_NAME, id);
        return buffer;
}
#endif
#ifdef USE_CGROUPS_PER_NF_INSTANCE
#define MP_CLIENT_CGROUP_NAME "nf_%u"
static inline const char *
get_cgroup_name(unsigned id)
{
        static char buffer[sizeof(MP_CLIENT_CGROUP_NAME) + 2];
        snprintf(buffer, sizeof(buffer) - 1, MP_CLIENT_CGROUP_NAME, id);
        return buffer;
}
#define MP_CLIENT_CGROUP_PATH "/sys/fs/cgroup/cpu/nf_%u/"
static inline const char *
get_cgroup_path(unsigned id)
{
        static char buffer[sizeof(MP_CLIENT_CGROUP_PATH) + 2];
        snprintf(buffer, sizeof(buffer) - 1, MP_CLIENT_CGROUP_PATH, id);
        return buffer;
}
#define MP_CLIENT_CGROUP_CREAT "mkdir /sys/fs/cgroup/cpu/nf_%u"
static inline const char *
get_cgroup_create_cgroup_cmd(unsigned id)
{
        static char buffer[sizeof(MP_CLIENT_CGROUP_CREAT) + 2];
        snprintf(buffer, sizeof(buffer) - 1, MP_CLIENT_CGROUP_CREAT, id);
        return buffer;
}
#define MP_CLIENT_CGROUP_ADD_TASK "echo %u > /sys/fs/cgroup/cpu/nf_%u/tasks"
static inline const char *
get_cgroup_add_task_cmd(unsigned id, pid_t pid)
{
        static char buffer[sizeof(MP_CLIENT_CGROUP_ADD_TASK) + 10];
        snprintf(buffer, sizeof(buffer) - 1, MP_CLIENT_CGROUP_ADD_TASK, pid, id);
        return buffer;
}
#define MP_CLIENT_CGROUP_SET_CPU_SHARE "echo %u > /sys/fs/cgroup/cpu/nf_%u/cpu.shares"
static inline const char *
get_cgroup_set_cpu_share_cmd(unsigned id, unsigned share)
{
        static char buffer[sizeof(MP_CLIENT_CGROUP_SET_CPU_SHARE) + 20];
        snprintf(buffer, sizeof(buffer) - 1, MP_CLIENT_CGROUP_SET_CPU_SHARE, share, id);
        return buffer;
}
#define MP_CLIENT_CGROUP_SET_CPU_SHARE_ONVM_MGR "/sys/fs/cgroup/cpu/nf_%u/cpu.shares"
static inline const char *
get_cgroup_set_cpu_share_cmd_onvm_mgr(unsigned id)
{
        static char buffer[sizeof(MP_CLIENT_CGROUP_SET_CPU_SHARE) + 20];
        snprintf(buffer, sizeof(buffer) - 1, MP_CLIENT_CGROUP_SET_CPU_SHARE_ONVM_MGR, id);
        return buffer;
}
#include <stdlib.h>
static inline int
set_cgroup_nf_cpu_share(uint16_t instance_id, uint32_t share_val) {
        /*
        unsigned long shared_bw_val = (share_val== 0) ?(1024):(1024*share_val/100); //when share_val is relative(%)
        if (share_val >=100) {
                shared_bw_val = shared_bw_val/100;
        }*/

        uint32_t shared_bw_val = (share_val== 0) ?(1024):(share_val);  //when share_val is absolute bandwidth
        const char* cg_set_cmd = get_cgroup_set_cpu_share_cmd(instance_id, shared_bw_val);
        //printf("\n CMD_TO_SET_CPU_SHARE: %s \n", cg_set_cmd);

        int ret = system(cg_set_cmd);
        return ret;
}
static inline int
set_cgroup_nf_cpu_share_from_onvm_mgr(uint16_t instance_id, uint32_t share_val) {
#ifdef SET_CPU_SHARE_FROM_NF
#else
        FILE *fp = NULL;
        uint32_t shared_bw_val = (share_val== 0) ?(1024):(share_val);  //when share_val is absolute bandwidth
        const char* cg_set_cmd = get_cgroup_set_cpu_share_cmd_onvm_mgr(instance_id);

        //printf("\n CMD_TO_SET_CPU_SHARE: %s \n", cg_set_cmd);
        fp = fopen(cg_set_cmd, "w");            //optimize with mmap if that is allowed!!
        if (fp){
                fprintf(fp,"%d",shared_bw_val);
                fclose(fp);
        }
        return 0;
#endif
}
#endif //USE_CGROUPS_PER_NF_INSTANCE

#define RTE_LOGTYPE_APP RTE_LOGTYPE_USER1

#ifdef ENABLE_NF_BACKPRESSURE
typedef struct per_core_nf_pool {
        uint16_t nf_count;
        uint32_t nf_ids[MAX_NFS];
}per_core_nf_pool_t;
#endif //ENABLE_NF_BACKPRESSURE


typedef struct sc_entries {
        struct onvm_service_chain *sc;
        uint16_t sc_count;
        uint16_t bneck_flag;
}sc_entries_list;
#ifdef ENABLE_NF_BACKPRESSURE //TODO: Replace this with ENABLE_NF_BASED_BKPR_MARKING flag and subsequent code changes in stats/flow_dir.c
//#ifdef ENABLE_NF_BASED_BKPR_MARKING

#ifdef USE_BKPR_V2_IN_TIMER_MODE
/* To store the List of Bottleneck NFs that can be operated upon by the Timer thread */ //TODO: If we add the onvm_time_t to the nfs[], then we can get rid of this entire table
typedef struct bottleneck_nf_entries {
        onvm_time_t s_time;
        uint16_t enqueue_status;        //BOTTLENECK_NF_STATUS_RESET, BOTTLENECK_NF_STATUS_WAIT_ENQUEUED, BOTTLENECK_NF_STATUS_DROP_MARKED
        uint16_t nf_id;
        uint16_t enqueued_ctr;
        uint16_t marked_ctr;
}bottleneck_nf_entries_t;
typedef struct bottlenec_nf_info {
        uint16_t entires;
        //struct rte_timer nf_timer[MAX_NFS];   // not worth it, as it would still be called at granularity of invoking the rte_timer_manage()
        bottleneck_nf_entries_t nf[MAX_NFS];
}bottlenec_nf_info_t;
bottlenec_nf_info_t bottleneck_nf_list;
#endif //USE_BKPR_V2_IN_TIMER_MODE
#endif //ENABLE_NF_BACKPRESSURE

#define WAIT_TIME_BEFORE_MARKING_OVERFLOW_IN_US   (0*SECOND_TO_MICRO_SECOND)

#ifdef ENABLE_NF_BACKPRESSURE
/******************************** DATA STRUCTURES FOR FIPO SUPPORT *********************************
*     fipo_buf_node_t:      each rte_buf_node (packet) added to the fipo_per_flow_list -- Need basic Queue add/remove
*     fipo_per_flow_list:   Ordered list of buffers for each flow   -- Need Queue add/remove
*     nf_flow_list_t:       Priority List of Flows for each NF      -- Need Queue add/remove
*     Memory sharing Model is tedious to support this..
*     Rx/Tx should access fipo_buf_node to create a pkt entry, then fipo_per_flow_list to insert into
*
******************************** DATA STRUCTURES FOR FIPO SUPPORT *********************************/
typedef struct fipo_buf_node {
        void *pkt;
        struct fipo_buf_node *next;
        struct fipo_buf_node *prev;
}fipo_buf_node_t;

typedef struct fipo_list {
        uint32_t buf_count;
        fipo_buf_node_t *head;
        fipo_buf_node_t *tail;
}fipo_list_t;
typedef fipo_list_t fipo_per_flow_list;
//Each entry of the list must be shared with the NF, i.e. unique memzone must be created per NF per flow as FIPO_%NFID_%FID
//Fix the MAX_NUMBER_OF_FLOWS, cannot have dynamic memzones per NF, too expensive as it has to have locks..
#define MAX_NUM_FIPO_FLOWS  (16)
#define MAX_BUF_PER_FLOW  ((128)/(MAX_NUM_FIPO_FLOWS))//((NF_QUEUE_RINGSIZE)/(MAX_NUM_FIPO_FLOWS))
typedef struct nf_flow_list {
        uint32_t flow_count;
        fipo_per_flow_list *head;
        fipo_per_flow_list *tail;
}nf_flow_list_t;
#endif //ENABLE_NF_BACKPRESSURE

#define TEST_INLINE_FUNCTION_CALL
#ifdef TEST_INLINE_FUNCTION_CALL
typedef int(*nf_pkt_handler)(struct rte_mbuf* pkt, struct onvm_pkt_meta* meta);
#endif

#endif  // _COMMON_H_
