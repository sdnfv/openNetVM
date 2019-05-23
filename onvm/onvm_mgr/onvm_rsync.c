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

                              onvm_special_nf0.c

       This file contains all functions related to NF management.

******************************************************************************/
#include "onvm_mgr.h"
#include "onvm_pkt.h"
#include "onvm_init.h"
#include "onvm_nf.h"
#include "onvm_special_nf0.h"
#include "onvm_rsync.h"
#include "onvm_ft_install.h"
#include <rte_common.h>
#include <rte_mbuf.h>
#include <rte_ip.h>
#include <rte_ether.h>
#include <rte_arp.h>

//#define RSYNC_USE_DPDK_TIMER    //(prefer disabled for REINFOCE) option only for TxTs  to either do inline or perform in timer context; NF state transfer always in timer context.

#define NF_CHECKPOINT_PERIOD_IN_US      (1000)    // use high precision 1ms, 10ms or 100ms; ensure that it is at least 1RTT (measure to be ~500us between b2b connected nodes + 8K buffering)
#define TX_TS_CHECKPOINT_PERIOD_IN_US   (10)       // Perform More often 100us => (@15MPPS: for every 1500 data packets perform 1 checkpoint=(1--64 packets tx_ts_packets) Therefore w.c. overhead = 4.25%),
//Given that we have buffer of 8K we can perform checkpoints much slower atleast 5 times slow i.e 500us ==> 7.5K packets perform 1 checkpoint. Therefore w.c. overhead= 0.85%

//port on which the TxTs, NF State and SVC State are synced (transmitted out).
#define RSYNC_OUT_PORT  (RSYNC_TX_OUT_PORT) //(1)

#ifdef MIMIC_PICO_REP
#define RSYNC_USE_DPDK_TIMER
volatile uint8_t rx_halt = 0;

#ifndef ENABLE_PICO_STATELESS_MODE
#undef TX_TS_CHECKPOINT_PERIOD_IN_US
#define TX_TS_CHECKPOINT_PERIOD_IN_US   (1000)
#endif
#undef NF_CHECKPOINT_PERIOD_IN_US
#define NF_CHECKPOINT_PERIOD_IN_US      (1000) // fix to 1ms as in PicoReplication
static int rsync_start_simple_pico(__attribute__((unused)) void *arg);
#ifndef ENABLE_PICO_STATELESS_MODE
static int rsync_start_simple_pico_nf(__attribute__((unused)) void *arg);
#endif
#endif

#ifdef ENABLE_REMOTE_SYNC_WITH_TX_LATCH
/* Estimate for determining the most preferred/ optimal Number of Packets that need to be considered for Tx Stats update. */
#define MAX_PACKETS_IN_AVG_RTT_AT_HIGH_ARRV_RATE    (5000)  //10Mpps (normal rate); => for ~ 500us RTT, to maximize handle packets worth 1RTT = 10Mpps * 500us
#ifdef ENABLE_RSYNC_MULTI_BUFFERING
#define MAX_PACKETS_IN_A_ROUND      (MIN(MAX_PACKETS_IN_AVG_RTT_AT_HIGH_ARRV_RATE, TX_RSYNC_TX_LATCH_RING_SIZE)/(ENABLE_RSYNC_MULTI_BUFFERING+1))  //note: this value can be dynamically adjusted based on the Hysteresis of Rx port rates.
#else
#define MAX_PACKETS_IN_A_ROUND      (MIN(MAX_PACKETS_IN_AVG_RTT_AT_HIGH_ARRV_RATE, TX_RSYNC_TX_LATCH_RING_SIZE)/2)  //note: this value can be dynamically adjusted based on the Hysteresis of Rx port rates.
#endif
//Batch size for Tx Update and NF State transfer:
#define PACKET_READ_SIZE_LARGE  (PACKET_READ_SIZE * 2)
//Return status indicators of the Tx port packet processing and requeue to Internal Rings
//Transaction related defines and Options for Wait and No-Wait based commit properties.
#define CHECK_FOR_COMMIT_WITH_WAIT (0)
#define CHECK_FOR_COMMIT_WITH_NO_WAIT (1)
#define MAX_RSYNC_TRANSACTIONS (250)    //256 (sizeof(uint8_t)*CHAR_BIT)
#define REMOTE_SYNC_WAIT_INTERVAL   (1*100*1000)  ////125 micro seconds => 250RTT, ideal: (1*100*1000) 100 micro seconds
#define MAX_TRANS_COMMIT_WAIT_COUNTER   (2)     //depends on RTT (between 2 nodes, it is observed to be around 350 micro seconds)
#define MAX_WAIT_TIME_FOR_TRANSACTION_COMMIT ((1*REMOTE_SYNC_WAIT_INTERVAL*MAX_TRANS_COMMIT_WAIT_COUNTER)/(1000))
//#define MAX_WAIT_TIME_FOR_TRANSACTION_COMMIT (150)

#ifdef __DEBUG_NFMGR_RSYNC_LOGS__
#define ENABLE_EXTRA_RSYNC_PRINT_MSGS
#endif

struct rte_timer tx_ts_checkpoint_timer;
struct rte_timer nf_status_checkpoint_timer;

//Return Status Indicators for extrat_and_parse_tx_port_buffers()
#define NEED_REMOTE_TS_TABLE_SYNC   (0x01)
#define NEED_REMOTE_NF_STATE_SYNC   (0x02)
#define TX_TS_LATCH_BUFFER_FULL     (0x10)
#define NF_STATE_LATCH_BUFFER_FULL  (0x20)

#ifdef ENABLE_PER_FLOW_TS_STORE
#ifdef ENABLE_PER_FLOW_TXTS_MAP_ENTRY
static dirty_mon_state_map_tbl_txts_t *dirty_state_map_tx_ts = NULL;
#else
static dirty_mon_state_map_tbl_t *dirty_state_map_tx_ts = NULL;
#endif //#ifdef ENABLE_PER_FLOW_TXTS_MAP_ENTRY
static onvm_per_flow_ts_info_t   *tx_ts_table = NULL;

#ifdef ENABLE_RSYNC_WITH_DOUBLE_BUFFERING_MODE
#ifdef ENABLE_RSYNC_MULTI_BUFFERING
#ifdef ENABLE_PER_FLOW_TXTS_MAP_ENTRY
static dirty_mon_state_map_tbl_txts_t *dirty_state_map_tx_ts_db[ENABLE_RSYNC_MULTI_BUFFERING];
#else
static dirty_mon_state_map_tbl_t *dirty_state_map_tx_ts_db[ENABLE_RSYNC_MULTI_BUFFERING];
#endif //#ifdef ENABLE_PER_FLOW_TXTS_MAP_ENTRY
static onvm_per_flow_ts_info_t   *tx_ts_table_db[ENABLE_RSYNC_MULTI_BUFFERING];
#else
#ifdef ENABLE_PER_FLOW_TXTS_MAP_ENTRY
static dirty_mon_state_map_tbl_txts_t *dirty_state_map_tx_ts_db = NULL;
#else
static dirty_mon_state_map_tbl_t *dirty_state_map_tx_ts_db = NULL;
#endif //#ifdef ENABLE_PER_FLOW_TXTS_MAP_ENTRY
static onvm_per_flow_ts_info_t   *tx_ts_table_db = NULL;
#endif
#endif

#endif

//Transfer size for NF and SVC State
#define DIRTY_MAP_PER_CHUNK_SIZE (_NF_STATE_SIZE/(sizeof(uint64_t)*CHAR_BIT))

//Transfer size for TxTs (PacketProcessingProgress) Information.
#define TXTS_DIRTY_MAP_CHUNK_SIZE  (_PER_FLOW_TS_SIZE/(sizeof(uint64_t)*CHAR_BIT))
#ifdef ENABLE_PER_FLOW_TXTS_MAP_ENTRY
#define MAX_TX_TS_ENTRIES        ((_PER_FLOW_TS_SIZE -sizeof(dirty_mon_state_map_tbl_txts_t))/(sizeof(uint64_t)))
#define TXTS_ENTRIES_PER_CHUNK   ((TX_TS_DIRTY_MAP_CHUNK_SIZE/(sizeof(uint64_t)*sizeof(uint64_t)))) // must change to two level
#else
#define MAX_TX_TS_ENTRIES        ((_PER_FLOW_TS_SIZE -sizeof(dirty_mon_state_map_tbl_t))/(sizeof(uint64_t)))
#define TXTS_ENTRIES_PER_CHUNK   ((TX_TS_DIRTY_MAP_CHUNK_SIZE/sizeof(uint64_t)))
#endif

typedef struct remote_node_config {
        uint8_t mac_addr_bytes[ETHER_ADDR_LEN];
        uint32_t ip_addr;
}remote_node_config_t;
static remote_node_config_t rsync_node_info = {
                .mac_addr_bytes={0x8C, 0xDC, 0xD4, 0xAC, 0x6B, 0x21},
                .ip_addr=IPv4(10,10,1,4)
};

#define STATE_TYPE_TX_TS_TABLE  (0x01)
#define STATE_TYPE_NF_MEMPOOL   (0x02)
#define STATE_TYPE_SVC_MEMPOOL  (0x04)
#define MSG_START_OF_REPLAY     (0x08)
#define MSG_END_OF_REPLAY       (0x09)
#define STATE_REQ_TO_RSP_LSH    (4)
#define STATE_TYPE_TX_TS_ACK    (STATE_TYPE_TX_TS_TABLE << STATE_REQ_TO_RSP_LSH)    //(0x10)
#define STATE_TYPE_NF_MEM_ACK   STATE_TYPE_NF_MEMPOOL << STATE_REQ_TO_RSP_LSH)      //(0x20)
#define STATE_TYPE_SVC_MEM_ACK  STATE_TYPE_SVC_MEMPOOL << STATE_REQ_TO_RSP_LSH)     // (0x40)
#define STATE_TYPE_REQ_MASK     (0x0F)
#define STATE_TYPE_RSP_MASK     (0xF0)

#define MAX_STATE_SIZE_PER_PACKET   (1024)
typedef struct state_tx_meta {
        uint8_t state_type;     //TX_TS_TYPE; NF_STATE; SERV_STATE;
        uint8_t trans_id;       //Id of the atomic transaction
        uint8_t flags;          //Additional Flags: TBD; ex. last packet or still more to follow.
        uint8_t nf_or_svc_id;   //Id of the NF(NF_STATE) or SVC (SERVICE_STATE)
        uint32_t start_offset;  //Offset in the global mempool
        //uint32_t reserved;      //Note Size per packet is Fixed to 1024 Bytes.
} __attribute__((__packed__)) state_tx_meta_t;
#define STATE_FLAG_LAST_PACKET_MARKER   (0x01)

typedef struct state_transfer_packet_hdr {
        state_tx_meta_t meta;
        uint8_t data[MAX_STATE_SIZE_PER_PACKET];
} __attribute__((__packed__)) state_transfer_packet_hdr_t;

typedef struct transfer_ack_packet_hdr {
        state_tx_meta_t meta;
} __attribute__((__packed__)) transfer_ack_packet_hdr_t;
extern struct rte_mempool *pktmbuf_pool;
extern uint32_t nf_mgr_id;

//To maintain the statistics for rsync activity
rsync_stats_t rsync_stat;

//To distinguish replay and no-replay modes: 0=NORMAL_MODE, 1=REPLAY_MODE
uint8_t replay_mode;

//Track ongoing transactions and associated timers
static volatile uint8_t trans_queue[MAX_RSYNC_TRANSACTIONS];
static onvm_time_t trans_ts[MAX_RSYNC_TRANSACTIONS];
#ifdef ENABLE_NF_PAUSE_TILL_OUTSTANDING_NDSYNC_COMMIT
static uint8_t trans_notify[MAX_RSYNC_TRANSACTIONS];
#endif

/***********************Internal Functions************************************/
static inline int send_packets_out(uint8_t port_id, uint16_t queue_id, struct rte_mbuf **tx_pkts, uint16_t nb_pkts);
static inline int log_transaction_and_send_packets_out(uint8_t trans_id, uint8_t bNotify, uint8_t port_id, uint16_t queue_id, struct rte_mbuf **tx_pkts, uint16_t nb_pkts);
static struct rte_mbuf* craft_state_update_packet(uint8_t port, state_tx_meta_t meta, uint8_t *pData, uint32_t data_len);

int transmit_tx_port_packets(void);
static int transmit_tx_tx_state_latch_rings( __attribute__((unused))uint8_t to_db);
static int transmit_tx_nf_state_latch_rings( __attribute__((unused))uint8_t to_db);
static int extract_and_parse_tx_port_packets( __attribute__((unused))uint8_t to_db);

//Functions to transmit local state to remote node
static int rsync_tx_ts_state_to_remote( __attribute__((unused))uint8_t to_db);
static int rsync_nf_state_to_remote(__attribute__((unused)) uint8_t bNDSync);
static int rsync_wait_for_commit_ack(uint8_t trans_id, uint8_t no_wait);

//Functions to sync local state from remote node packets
static int rsync_tx_ts_state_from_remote(state_tx_meta_t *meta, uint8_t *data,  uint16_t data_len);
static int rsync_nf_state_from_remote(state_tx_meta_t *meta, uint8_t *data,  uint16_t data_len);

//Functions to send response for remote node updates
//static int rsync_tx_ts_state_ack_resp_to_remote();
//static int rsync_nf_state_ack_resp_to_remote();

static inline int initialize_rsync_timers(void);

/***********************Internal Functions************************************/
static inline void notify_ndsync_commit_to_blocked_NFs(void) {
#ifdef ENABLE_NF_PAUSE_TILL_OUTSTANDING_NDSYNC_COMMIT
        uint16_t nf_id = 0;

        //Start with Each NF Instance ID which is active and valid and start gathering all changed NF state;
        for (nf_id = 1; nf_id < MAX_NFS; nf_id++) {
                struct onvm_nf *cl = &nfs[nf_id];
                //Get the Processing (Actively Running and Processing packets) NF Instance ID */
                //if(1){
                if(unlikely(onvm_nf_is_NDSYNC_set(cl))) {
                //if (unlikely(onvm_nf_is_waiting_on_NDSYNC(cl))){
                        //clear Status Bits and notify resume to the NF

                        //if(!cl->info) continue;
                        //cl->info->status ^=(NF_PAUSED_BIT|NF_WT_ND_SYNC_BIT);
                        //if(cl->info->status & NF_PAUSED_BIT)cl->info->status ^=(NF_PAUSED_BIT);
                        if(cl->info->status & NF_WT_ND_SYNC_BIT)cl->info->status ^=(NF_WT_ND_SYNC_BIT);
                        onvm_nf_send_msg(cl->info->instance_id,MSG_RESUME,MSG_MODE_ASYNCHRONOUS,NULL);
                        //printf("\n Client [%d] is delivered SYNC to resume processing ND committed [%d]! \n", nf_id, cl->info->status);
                        //sleep(2);
                }
        }
#endif
}
/***********************TRANSACTION FUNCTIONS**********************************/
#ifdef ENABLE_EXTRA_RSYNC_PRINT_MSGS
static int print_trans_status(void) {

        int i = 0;
        printf("\n Transaction List:[");
        for(i=0; i < MAX_RSYNC_TRANSACTIONS; i++) {
                printf("%d\t ", trans_queue[i]);
        }
        printf("]\n");
        return 0;
}
#endif
static int get_transaction_id(void) {
        static uint8_t next_trans_id = 1;
        //return ((++trans_id)%MAX_RSYNC_TRANSACTIONS);
        uint8_t i = next_trans_id%MAX_RSYNC_TRANSACTIONS,j=0;
//RECHECK:
        for(j=0; j< MAX_RSYNC_TRANSACTIONS; j++) {
                if(i && trans_queue[i] == 0) {
                        next_trans_id = i+1;
                        return i;
                }
                i++;
                i%=MAX_RSYNC_TRANSACTIONS;
        }
        //goto RECHECK;
        return -1;
}
static uint8_t log_transaction_id(uint8_t tid, __attribute__((unused)) uint8_t bNotify) {
        onvm_util_get_cur_time(&trans_ts[tid]);
        rsync_stat.transactions_out_counter++;
#ifdef ENABLE_NF_PAUSE_TILL_OUTSTANDING_NDSYNC_COMMIT
        if(bNotify) {
                //printf("\n Got Transaction with NDSYNC COMMIT!!\n");
                trans_notify[tid] = 1;
        }
#endif
#ifdef ENABLE_EXTRA_RSYNC_PRINT_MSGS
        trans_queue[tid] = 1;
        printf("On Transaction LOG for [%d]\n", tid);
        print_trans_status();
#else
        return (trans_queue[tid] = 1);
#endif
}
static uint8_t clear_transaction_id (uint8_t tid) {
        trans_ts[tid].t.tv_nsec = trans_ts[tid].t.tv_sec=0;
#ifdef ENABLE_NF_PAUSE_TILL_OUTSTANDING_NDSYNC_COMMIT
        if(trans_notify[tid]) {
                trans_notify[tid]=0;
                notify_ndsync_commit_to_blocked_NFs();
        }
#endif
#ifdef ENABLE_EXTRA_RSYNC_PRINT_MSGS
        printf("On Transaction Clear for [%d]\n", tid);
        trans_queue[tid]=0;
        print_trans_status();
#else
        return (trans_queue[tid]=0); //return (trans_queue[tid]^= tid);
#endif
}

static uint8_t check_and_clear_elapsed_transactions(void) {
#ifdef TX_RSYNC_AUTOCLEAR_ELAPSED_TRANSACTIONS_TIMERS
        uint8_t i = 0;
        onvm_time_t cur_tm;
        onvm_util_get_cur_time(&cur_tm);
        for(i=0; i<MAX_RSYNC_TRANSACTIONS;i++) {
                if((trans_queue[i]) && ((MAX_WAIT_TIME_FOR_TRANSACTION_COMMIT) <= onvm_util_get_difftime_us(&trans_ts[i], &cur_tm))) {
                        clear_transaction_id(i);
                }
        }
#endif
        return 0;
}
/**
 * Note: The Sync logic wait time greatly impacts the overall throughput and latency factors for all the NFs in the chain.
 * Wait Time:   0(Bypass mode)          10ns            100ns           500ns       200us       500us
 * Baseline:
 * Monitor:     8.45Mpps(11-643us)      8.15Mpps        8.11Mpps        2.90Mpps    2.06Mpps    1.08Mpps
 * VTAG:        8.9Mpps                 6.25Mpps        4.28Mpps        1.5Mpps     1.65Mpps!   0.95Mpps
 * DPI:         4.10Mpps                4.10Mpps        3.57Mpps        1.11Mpps    1.51Mpps!   0.91Mpps
 * With single NF, average packet latency is around 330us; Ideally expect around 20-30 but we are 10 times higher;
 * Hence choose a value of around 200-500us as operational speed; i.e the time it takes to send the packets and get the ack back.
 * If we choose to assume like FTMB, once packets are sent out they are committed (Output commit property); then we can use the BYPASS MODE and achieve full flexibility
 * Note: The value we choose here and the rationale for choosing the BFD timer values need to be coherent.
 */

//Return = 0 on complete; >0 otherwsie
static int rsync_wait_for_commit_ack(uint8_t trans_id, __attribute__((unused)) uint8_t no_wait) {
#ifdef BYPASS_WAIT_ON_TRANSACTIONS
        clear_transaction_id(trans_id);
        return 0;
#endif
#ifdef TX_RSYNC_AUTOCLEAR_ELAPSED_TRANSACTIONS_TIMERS
        //Note this can be enabled only in two ONVM Nodes that actually send trans commit response or we have autoclear enabled
        if(likely(no_wait)) {
                return trans_queue[trans_id];
        }
#endif

        int wait_counter = 0; //hack till remote_node also sends
        struct timespec req = {0,REMOTE_SYNC_WAIT_INTERVAL}, res = {0,0};
        do {
#if defined(MIMIC_PICO_REP) && !defined(ENABLE_PICO_RX_NON_BLOCKING_MODE)
                rx_halt=1;
#endif
                nanosleep(&req, &res);
                if((++wait_counter) > MAX_TRANS_COMMIT_WAIT_COUNTER) break;
        }while(trans_queue[trans_id]);
        clear_transaction_id(trans_id);
#ifdef MIMIC_PICO_REP
                rx_halt=0;
#endif
        return 0;
}

static int rsync_wait_for_commit_acks(uint8_t *trans_id_list, uint8_t count, __attribute__((unused)) uint8_t no_wait) {
#ifdef BYPASS_WAIT_ON_TRANSACTIONS
        return 0;
#endif
        uint8_t i;
#ifdef TX_RSYNC_AUTOCLEAR_ELAPSED_TRANSACTIONS_TIMERS
        //Note this can be enabled only in two ONVM Nodes that actually send trans commit response
        if(likely(no_wait)) {
                for(i=0; i< count; i++) {
                        if(trans_queue[trans_id_list[i]]) {
                                return trans_id_list[i]; //notify transaction is still pending
                        } else {
                                trans_id_list[i]=0; //transaction has been cleared already!
                        }
                }
                return 0; //notify transaction is completed!
        }
#endif

        uint8_t wait_needed=0;//TEST_HACK to bypass wait_on_acks //return wait_needed;
        //poll/wait till trans_queue[ids[]] is cleared.
        int wait_counter = 0; //hack till remote_node also sends
        struct timespec req = {0,REMOTE_SYNC_WAIT_INTERVAL}, res = {0,0};
        do {
#if defined(MIMIC_PICO_REP) && !defined(ENABLE_PICO_RX_NON_BLOCKING_MODE)
                rx_halt=1;
#endif
                wait_needed= 0;
                for(i=0; i< count; i++) {
                        if(trans_queue[trans_id_list[i]]) {
                                wait_needed = 1;
                        } else {
                                trans_id_list[i]=0;
                        }
                }
                if(!wait_needed) {
                        return 0; break;
                }
                nanosleep(&req, &res);
                if((++wait_counter) > MAX_TRANS_COMMIT_WAIT_COUNTER) break;
        }while(wait_needed);

#ifdef MIMIC_PICO_REP
        rx_halt=0;
        //need notifier to clear the transactions
        //clear the transactions.
        for(i=0; i< count; i++) {
                clear_transaction_id(trans_id_list[i]);
        }
        return 0;
#else
        return trans_id_list[0];
#endif
}
/***********************DPDK TIMER FUNCTIONS**********************************/
static void
tx_ts_checkpoint_timer_cb(__attribute__((unused)) struct rte_timer *ptr_timer,
                __attribute__((unused)) void *ptr_data) {
        rsync_start(ptr_data);
        return;
}
static void
nf_status_checkpoint_timer_cb(__attribute__((unused)) struct rte_timer *ptr_timer,
        __attribute__((unused)) void *ptr_data) {

#if defined(MIMIC_PICO_REP)
#if !defined(ENABLE_PICO_STATELESS_MODE)
        rsync_start_simple_pico(ptr_data);
        return;
#else
#endif
        //rsync_start_simple_pico_nf(ptr_data);
        return;
#endif

        static uint8_t nf_sync_trans_id = 0;
        if(!nf_sync_trans_id) {
                int tnx_id = rsync_nf_state_to_remote(1);
                if(likely(tnx_id >=0 )) {
                        nf_sync_trans_id = (uint8_t) tnx_id;
                }
        }
        if(likely(nf_sync_trans_id)) {
                if( 0 == rsync_wait_for_commit_ack(nf_sync_trans_id, CHECK_FOR_COMMIT_WITH_NO_WAIT)) {
                        transmit_tx_nf_state_latch_rings(0);
                        nf_sync_trans_id = 0;
                }
        }
        return;
}
static inline int initialize_rsync_timers(void) {
        //return 0;
        uint64_t ticks = ((uint64_t)TX_TS_CHECKPOINT_PERIOD_IN_US *(rte_get_timer_hz()/1000000));
        rte_timer_reset_sync(&tx_ts_checkpoint_timer,ticks,PERIODICAL,
                        rte_lcore_id(), &tx_ts_checkpoint_timer_cb, NULL);
#if !defined(RSYNC_USE_DPDK_TIMER)   //in either case disable TxTs
        rte_timer_init(&tx_ts_checkpoint_timer);
#endif

        ticks = ((uint64_t)NF_CHECKPOINT_PERIOD_IN_US *(rte_get_timer_hz()/1000000));
        rte_timer_reset_sync(&nf_status_checkpoint_timer,ticks,PERIODICAL,
                        rte_lcore_id(), &nf_status_checkpoint_timer_cb, NULL);
        //rte_timer_init(&nf_status_checkpoint_timer);

        return 0;
}
/***********************DPDK TIMER FUNCTIONS**********************************/
/***********************Internal Functions************************************/
/***********************DPDK TIMER FUNCTIONS**********************************/
static void bswap_rsync_hdr_data(state_tx_meta_t *meta, int to_be) {
        if(to_be) {
                meta->start_offset = rte_cpu_to_be_32(meta->start_offset);
                //meta->reserved = rte_cpu_to_be_32(meta->reserved);

        } else {
                meta->start_offset = rte_be_to_cpu_32(meta->start_offset);
                //meta->reserved =  rte_be_to_cpu_32(meta->reserved);
                //uint8_t *pdata = rsync_req->data;
        }
}
#ifdef ENABLE_EXTRA_RSYNC_PRINT_MSGS
static
inline int rsync_print_rsp_packet(transfer_ack_packet_hdr_t *rsync_pkt);
static
inline int rsync_print_rsp_packet(transfer_ack_packet_hdr_t *rsync_pkt) {

        printf("TYPE: %" PRIu8 "\n", rsync_pkt->meta.state_type & 0b11111111);
        printf("NF_ID: %" PRIu8 "\n", rsync_pkt->meta.nf_or_svc_id & 0b11111111);
        printf("TRAN_ID: %" PRIu8 "\n", rsync_pkt->meta.trans_id & 0b11111111);
        printf("FLAGS: %" PRIu8 "\n", rsync_pkt->meta.flags & 0b11111111);
        printf("start Offset: %" PRIu32 "\n", rte_be_to_cpu_32(rsync_pkt->meta.start_offset));
        //printf("Reserved: %" PRIu32 "\n", rte_be_to_cpu_32(rsync_pkt->meta.reserved));
        return rsync_pkt->meta.state_type;
}
#endif
static struct rte_mbuf* craft_state_update_packet(uint8_t port, state_tx_meta_t meta, uint8_t *pData, uint32_t data_len) {
        struct rte_mbuf *out_pkt = NULL;
        //struct onvm_pkt_meta *pmeta = NULL;
        struct ether_hdr *eth_hdr = NULL;
        state_transfer_packet_hdr_t *s_hdr = NULL;
        size_t pkt_size = 0;

        //Allocate New Packet
        out_pkt = rte_pktmbuf_alloc(pktmbuf_pool);
        if (out_pkt == NULL) {
                rte_pktmbuf_free(out_pkt); //rte_free(out_pkt);
                rte_exit(EXIT_FAILURE, "onvm_rsync:Failed to alloc packet!! \n");
                return NULL;
        }

        //set packet properties
        pkt_size = sizeof(struct ether_hdr) + sizeof(struct state_transfer_packet_hdr);
        out_pkt->data_len = MAX(pkt_size, data_len);    //todo: crirical error if 0 or lesser than pkt_len: mooongen discards; check again and confirm
        out_pkt->pkt_len = pkt_size;

        //Set Ethernet Header info
        eth_hdr = onvm_pkt_ether_hdr(out_pkt);
        ether_addr_copy(&ports->mac[port], &eth_hdr->s_addr);
        eth_hdr->ether_type = rte_cpu_to_be_16(ETHER_TYPE_RSYNC_DATA);
        ether_addr_copy((const struct ether_addr *)&rsync_node_info.mac_addr_bytes, &eth_hdr->d_addr);

        //SET RSYNC DATA
        s_hdr = rte_pktmbuf_mtod_offset(out_pkt, state_transfer_packet_hdr_t*, sizeof(struct ether_hdr));
        s_hdr->meta = meta;
        bswap_rsync_hdr_data(&s_hdr->meta, 1);
        if(data_len) {
                rte_memcpy(s_hdr->data, pData, data_len);
        }

        //SEND PACKET OUT/SET METAINFO
        //pmeta = onvm_get_pkt_meta(out_pkt);
        //pmeta->destination = port;
        //pmeta->action = ONVM_NF_ACTION_OUT;

        return out_pkt;
        //return send_packets_out(port, RSYNC_TX_PORT_QUEUE_ID_1, &out_pkt, 1);
}

/***********************TX STATE TABLE UPDATE**********************************/
static int rsync_tx_ts_state_to_remote( __attribute__((unused))uint8_t to_db) {
#ifdef ENABLE_PER_FLOW_TS_STORE
        uint16_t i=0;
        struct rte_mbuf *pkts[PACKET_READ_SIZE_LARGE];
        state_tx_meta_t meta = {.state_type= STATE_TYPE_TX_TS_TABLE, .nf_or_svc_id=0, .start_offset=0, /*.reserved=nf_mgr_id, */.trans_id=0};
        uint8_t btrans_initiated = 0;
#ifdef ENABLE_PER_FLOW_TXTS_MAP_ENTRY
        dirty_mon_state_map_tbl_txts_t *dtx = dirty_state_map_tx_ts;
#else
        dirty_mon_state_map_tbl_t *dtx = dirty_state_map_tx_ts;
#endif //#ifdef ENABLE_PER_FLOW_TXTS_MAP_ENTRY

#ifdef ENABLE_RSYNC_WITH_DOUBLE_BUFFERING_MODE
        if(to_db) {
#ifdef ENABLE_RSYNC_MULTI_BUFFERING
                dtx = dirty_state_map_tx_ts_db[to_db-1];
#else
                dtx = dirty_state_map_tx_ts_db;
#endif
        }
#endif
        if(likely(dtx && dtx->dirty_index)) {
                int trans_id = get_transaction_id();
                if(unlikely(trans_id < 0)) {
                        printf("\n rsync_tx_ts_state_to_remote(to_db=%d): Failed to acquire the transaction ID:%d\n", to_db,trans_id);
                        return -1;
                }meta.trans_id = (uint8_t) trans_id;
                //Note: Must always ensure that dirty_map is carried over first; so that the remote replica can use this value to update only the changed states
                uint64_t dirty_index = dtx->dirty_index;
                uint64_t copy_index = 0;
                uint64_t copy_setbit = 0;
                //uint16_t copy_offset = 0;
                for(;dirty_index;copy_index++) {
                        copy_setbit = (1L<<(copy_index));
                        if(dirty_index&copy_setbit) {
                                meta.start_offset = copy_index*TXTS_DIRTY_MAP_CHUNK_SIZE;
                                if(CHECK_IF_ANY_ONE_BIT_SET(dirty_index)) {
                                        meta.flags |= STATE_FLAG_LAST_PACKET_MARKER;
                                }
                                pkts[i++] = craft_state_update_packet(RSYNC_OUT_PORT,meta, (((uint8_t*)onvm_mgr_tx_per_flow_ts_info)+meta.start_offset),TXTS_DIRTY_MAP_CHUNK_SIZE);
                                dirty_index^=copy_setbit;
                                if( i == PACKET_READ_SIZE_LARGE) {
                                        log_transaction_and_send_packets_out(meta.trans_id, 0, RSYNC_OUT_PORT, RSYNC_TX_PORT_QUEUE_ID_0, pkts, i); //send_packets_out(out_port, 0, pkts, i);
#ifdef ENABLE_PORT_TX_STATS_LOGS
                                        rsync_stat.tx_state_sync_pkt_counter +=i;
#endif
                                        i=0;
                                        btrans_initiated=1;
                                }
                        }
                }
                dtx->dirty_index =0;
                //check if packets are created and need to be transmitted out;
                if(i) {
                        //exit(222);
                        if(btrans_initiated) {
                                send_packets_out(RSYNC_OUT_PORT, RSYNC_TX_PORT_QUEUE_ID_0, pkts, i);
                        }
                        else {
                                log_transaction_and_send_packets_out(meta.trans_id, 0, RSYNC_OUT_PORT, RSYNC_TX_PORT_QUEUE_ID_0, pkts, i);
                        }
#ifdef ENABLE_PORT_TX_STATS_LOGS
                        rsync_stat.tx_state_sync_pkt_counter +=i;
#endif
                }
#ifdef ENABLE_LOCAL_LATENCY_PROFILER
                //fprintf(stdout, "STATE REPLICATION TIME: %li ns\n", onvm_util_get_elapsed_time(&ts));
#endif
                return meta.trans_id;
        }
#endif //ENABLE_PER_FLOW_TS_STORE
        return 0;
}
static int rsync_nf_state_to_remote(uint8_t bNDSync) {
        //Note: Size of NF_STATE_SIZE=64K and total_chunks=64 => size_per_chunk=1K -- can fit in 1 packet But
        //      size of SVC_STATE_SIZE=4MB and total_chunks=64 => size_per_chunk=64K -- so, each service each chunk requires 64 sends. -- must optimize -- DPI is an exception..
        uint16_t i=0;
        uint8_t active_services[MAX_SERVICES] = {0};
        struct rte_mbuf *pkts[PACKET_READ_SIZE_LARGE];
        state_tx_meta_t meta = {.state_type= STATE_TYPE_NF_MEMPOOL, .nf_or_svc_id=0, .start_offset=0, /*.reserved=nf_mgr_id, */.trans_id=0};
        void *pReplicaStateMempool = NULL;
        dirty_mon_state_map_tbl_t *dirty_state_map_nf = NULL;
        uint16_t nf_id = 0;
        uint8_t btrans_initiated = 0;
        int trans_id=-1;
        for(;nf_id < MAX_SERVICES; nf_id++) active_services[nf_id]=0;

        //Start with Each NF Instance ID which is active and valid and start gathering all changed NF state;
        for (nf_id = 0; nf_id < MAX_NFS; nf_id++) {
                //Get the Processing (Actively Running and Processing packets) NF Instance ID */
                if (likely(onvm_nf_is_processing(&nfs[nf_id]))){
#ifdef ENABLE_NF_PAUSE_TILL_OUTSTANDING_NDSYNC_COMMIT
                        if(onvm_nf_is_NDSYNC_set(&nfs[nf_id])) {
                        //if(onvm_nf_is_waiting_on_NDSYNC(&nfs[nf_id])) {
                                bNDSync=1;
                        }
#endif
                        //store the corresponding service id for Global state transfer :: What if there are multiple active NFs for same service; we should still ensure that svc is used only once.
                        active_services[nfs[nf_id].info->service_id] = 1;//active_services[j++] = nfs[nf_id].info->service_id;

                        //retrieve the associated standby nf_id;
                        //alt_nf_id = get_associated_active_or_standby_nf_id(nf_id);

                        //sync state from the standby NFs memory: To avoid sync issues-- still possible; TODO: must ensure that update (write in NFLIB must not happen while we read here). How?
                        pReplicaStateMempool = nfs[get_associated_active_or_standby_nf_id(nf_id)].nf_state_mempool;
                        if(likely(NULL != pReplicaStateMempool)) {
                                dirty_state_map_nf = (dirty_mon_state_map_tbl_t*)pReplicaStateMempool;

                        } else continue;

                        if(likely(dirty_state_map_nf && dirty_state_map_nf->dirty_index)) {
                                meta.nf_or_svc_id = nf_id;
                                if(unlikely(0 == meta.trans_id)) {
                                        trans_id = get_transaction_id();
                                        if(unlikely(trans_id < 0)) {
#ifdef ENABLE_EXTRA_RSYNC_PRINT_MSGS
                                                printf("\n rsync_nf_state_to_remote(nf): Failed to acquire the transaction ID\n");
#endif
                                                return 0; //return -1;
                                        }meta.trans_id = (uint8_t) trans_id;
                                }

                                //Note: Must always ensure that dirty_map is carried over first; so that the remote replica can use this value to update only the changed states
                                uint64_t dirty_index = dirty_state_map_nf->dirty_index;
                                uint64_t copy_index = 0;
                                uint64_t copy_setbit = 0;
                                //uint16_t copy_offset = 0;
                                for(;dirty_index;copy_index++) {
                                        copy_setbit = (1L<<(copy_index));
                                        if(dirty_index&copy_setbit) {
                                                meta.start_offset = copy_index*DIRTY_MAP_PER_CHUNK_SIZE;
                                                pkts[i++] = craft_state_update_packet(RSYNC_OUT_PORT,meta, (((uint8_t*)pReplicaStateMempool)+meta.start_offset),DIRTY_MAP_PER_CHUNK_SIZE);
#ifdef ENABLE_PORT_TX_STATS_LOGS
                                                        rsync_stat.nf_state_sync_pkt_counter[nf_id] +=i;
#endif

                                                dirty_index^=copy_setbit;
                                                //If we exhaust all the packets, then we must send out packets before processing further state
                                                if( i == PACKET_READ_SIZE_LARGE) {
                                                        if(btrans_initiated) {
                                                                //printf("\n $$$$ Sending [%d] packets for NF Instdance [%d] State Sync $$$$\n", i, nf_id);
                                                                send_packets_out(RSYNC_OUT_PORT, RSYNC_TX_PORT_QUEUE_ID_1, pkts, i);
                                                        } else {
                                                                log_transaction_and_send_packets_out(meta.trans_id, bNDSync, RSYNC_OUT_PORT, RSYNC_TX_PORT_QUEUE_ID_1, pkts, i); //send_packets_out(out_port, 0, pkts, i);
                                                                btrans_initiated=1;
                                                        }
                                                        i=0;
                                                }
                                        }
                                }
                                dirty_state_map_nf->dirty_index =0;
                        }
                }
        }
        /*
        //Either Send State update for each NF or batch with other NF State? ( IF batch multiple NFs, then move it out of for_loop )
        //check if packets are created and need to be transmitted out;
        if(i) {
                if(btrans_initiated) {
                        //printf("\n $$$$ Sending [%d] packets for NF Instance [%d] State Sync $$$$\n", i, nf_id);
                        send_packets_out(RSYNC_OUT_PORT, RSYNC_TX_PORT_QUEUE_ID_1, pkts, i);
                } else {
                        log_transaction_and_send_packets_out(meta.trans_id, bNDSync, RSYNC_OUT_PORT, RSYNC_TX_PORT_QUEUE_ID_1, pkts, i);
                }
#ifdef ENABLE_PORT_TX_STATS_LOGS
                rsync_stat.nf_state_sync_pkt_counter[nf_id] +=i;
#endif
                i=0;
                btrans_initiated=0;
        }*/

        //Start now for NF Service ID that are marked as active and valid while gathering all changed NF state; TODO: Chunk size correction 1K to 64K
        for (nf_id = 0; nf_id < MAX_SERVICES; nf_id++) {
                meta.state_type = STATE_TYPE_SVC_MEMPOOL;
                //Get the Processing (Actively Running and Processing packets) NF Instance ID */
                if (likely(active_services[nf_id])){
                        //sync state from the standby NFs memory: To avoid sync issues-- still possible; TODO: must ensure that update (write in NFLIB must not happen while we read here). How?
                        pReplicaStateMempool = services_state_pool[nf_id];
                        if(likely(NULL != pReplicaStateMempool)) {
                                dirty_state_map_nf = (dirty_mon_state_map_tbl_t*)pReplicaStateMempool;

                        } else continue;

                        if(likely(dirty_state_map_nf && dirty_state_map_nf->dirty_index)) {
                                meta.nf_or_svc_id = nf_id;
                                if(unlikely(0 == meta.trans_id)) {
                                        trans_id = get_transaction_id();
                                        if(unlikely(trans_id < 0)) {
#ifdef ENABLE_EXTRA_RSYNC_PRINT_MSGS
                                                printf("\n rsync_nf_state_to_remote(svc): Failed to acquire the transaction ID\n");
#endif
                                                return 0; //return -1;
                                        }meta.trans_id = (uint8_t) trans_id;
                                }

                                //Note: Must always ensure that dirty_map is carried over first; so that the remote replica can use this value to update only the changed states
                                uint64_t dirty_index = dirty_state_map_nf->dirty_index;
                                uint64_t copy_index = 0;
                                uint64_t copy_setbit = 0;
                                //uint16_t copy_offset = 0;
                                for(;dirty_index;copy_index++) {
                                        copy_setbit = (1L<<(copy_index));
                                        if(dirty_index&copy_setbit) {
                                                meta.start_offset = copy_index*DIRTY_MAP_PER_CHUNK_SIZE;
                                                pkts[i++] = craft_state_update_packet(RSYNC_OUT_PORT,meta, (((uint8_t*)pReplicaStateMempool)+meta.start_offset),DIRTY_MAP_PER_CHUNK_SIZE);
                                                dirty_index^=copy_setbit;
                                                //If we exhaust all the packets, then we must send out packets before processing further state
                                                if(i == PACKET_READ_SIZE_LARGE) {
                                                        if(btrans_initiated) {
                                                                //printf("\n $$$$ Sending [%d] packets for NF Instdance [%d] State Sync $$$$\n", i, nf_id);
                                                                send_packets_out(RSYNC_OUT_PORT, RSYNC_TX_PORT_QUEUE_ID_1, pkts, i);
                                                        } else {
                                                                log_transaction_and_send_packets_out(meta.trans_id, bNDSync, RSYNC_OUT_PORT, RSYNC_TX_PORT_QUEUE_ID_1, pkts, i); //send_packets_out(out_port, 0, pkts, i);
                                                                btrans_initiated=1;
                                                        }
                                                        i=0;
                                                }
                                        }
                                }
                                dirty_state_map_nf->dirty_index =0;
                        }
                }
        }
        //Either Send State update for each NF or batch with other NF State? ( IF batch multiple NFs, then move it out of for_loop )
        //check if packets are created and need to be transmitted out;
        if(i) {
                if(btrans_initiated) {
                        //printf("\n $$$$ Sending [%d] packets for NF Instdance [%d] State Sync $$$$\n", i, nf_id);
                        send_packets_out(RSYNC_OUT_PORT, RSYNC_TX_PORT_QUEUE_ID_1, pkts, i);
                } else {
                        log_transaction_and_send_packets_out(meta.trans_id, bNDSync, RSYNC_OUT_PORT, RSYNC_TX_PORT_QUEUE_ID_1, pkts, i);
                }
                i=0;
                btrans_initiated=0;
        }
        return meta.trans_id;
        return 0;
}

static int rsync_tx_ts_state_from_remote(__attribute__((unused)) state_tx_meta_t *meta, __attribute__((unused)) uint8_t *pData, __attribute__((unused)) uint16_t data_len) {
#ifdef ENABLE_PER_FLOW_TS_STORE
#if 0
        uint16_t i=0;
        struct rte_mbuf *pkts[PACKET_READ_SIZE_LARGE];
        uint8_t type   = rsync_req->meta.state_type & 0b11111111;
        uint8_t nf_id  = rsync_req->meta.nf_or_svc_id & 0b11111111;
        uint8_t tnx_id = rsync_req->meta.trans_id & 0b11111111;
        uint8_t flags  = rsync_req->meta.flags & 0b11111111;
        uint16_t s_offt= rte_be_to_cpu_16(rsync_req->meta.start_offset);
        //uint32_t resv =  rte_be_to_cpu_32(rsync_req->meta.reserved);
        uint8_t *pdata = rsync_req->data;
        //bswap_rsync_hdr_data(&rsync_req->meta, 0);
#endif
        uint8_t* pDst = (((uint8_t*)onvm_mgr_tx_per_flow_ts_info)+meta->start_offset);
        rte_memcpy(pDst, pData, MIN(data_len, TXTS_DIRTY_MAP_CHUNK_SIZE)); //should be MIN(pkt->data_len, TXTS_DIRTY_MAP_CHUNK_SIZE)
#endif
        return 0;
}

static int rsync_nf_state_from_remote(state_tx_meta_t *meta, uint8_t *pData,  uint16_t data_len) {
        void *pReplicaStateMempool = NULL;
        if (likely(STATE_TYPE_NF_MEMPOOL == meta->state_type)) {
                //ideally sync should happen only on the standby NF ID; Sender could be any of them, recepient should always be standby
                pReplicaStateMempool = nfs[get_associated_active_or_standby_nf_id(meta->nf_or_svc_id)].nf_state_mempool;
        }else if(STATE_TYPE_SVC_MEMPOOL == meta->state_type) {
                pReplicaStateMempool = services_state_pool[meta->nf_or_svc_id];
        } else return 1;
        if(NULL == pReplicaStateMempool) return 2;

        uint8_t* pDst = (((uint8_t*)pReplicaStateMempool)+meta->start_offset);
        rte_memcpy(pDst, pData, MIN(data_len, DIRTY_MAP_PER_CHUNK_SIZE)); //should be MIN(pkt->data_len, DIRTY_MAP_PER_CHUNK_SIZE)
        return 0;
}
#define ENABLE_REPLAY_LATENCY_PROFILER
#ifdef ENABLE_REPLAY_LATENCY_PROFILER
static onvm_interval_timer_t ts;
#endif
static int rsycn_handle_start_of_replay(void) {
#ifdef ENABLE_REPLAY_LATENCY_PROFILER
        onvm_util_get_start_time(&ts);
#endif
        printf("\n Replay started at %li rtdsc_cycles\n", onvm_util_get_current_cpu_cycles());
        //move all standby NFs to Active:: Happens automatically on first play packet.//Assumption: NFs and NF chain is already setup by orchestrator and controller.
        //disable rsync commit during replay but enable packet drop check logic.
        return 0;
}
static int rsycn_handle_end_of_replay(void) {
        //no additional action is needed!
#ifdef ENABLE_REPLAY_LATENCY_PROFILER
        printf("\n REPLAY_TIME (START-->-END_OF_REPLAY): %li ns\n", onvm_util_get_elapsed_time(&ts));
#endif
        //Enable Rsync commit logic: Assumption, we can update state on other designated secondary or actual primary (acts as secondary if it is back-up).
        //Disable TxTs check for Ts correctness.
        return 0;
}
/***********************Internal Functions************************************/
/***********************TX STATE TABLE UPDATE**********************************/
#ifdef ENABLE_PER_FLOW_TS_STORE
static inline uint64_t tx_ts_map_tag_index_to_dirty_chunk_bit_index(uint16_t tx_tbl_index) {
        uint32_t start_offset = sizeof(dirty_mon_state_map_tbl_t) + tx_tbl_index*sizeof(onvm_per_flow_ts_info_t);
        uint32_t end_offset = start_offset + sizeof(onvm_per_flow_ts_info_t);
        uint64_t dirty_map_bitmask = 0;
        dirty_map_bitmask |= (1<< (start_offset/TXTS_DIRTY_MAP_CHUNK_SIZE));
        dirty_map_bitmask |= (1<< (end_offset/TXTS_DIRTY_MAP_CHUNK_SIZE));
        //printf("\n For %d, 0x%lx\n",(int)tx_tbl_index, dirty_map_bitmask);
        return dirty_map_bitmask;
}
static inline void tx_ts_update_dirty_state_index(uint16_t tx_tbl_index, __attribute__((unused)) uint8_t to_db) {
#ifdef ENABLE_RSYNC_WITH_DOUBLE_BUFFERING_MODE
        if(to_db) {
#ifdef ENABLE_RSYNC_MULTI_BUFFERING
                if(dirty_state_map_tx_ts_db[to_db-1]) {
#ifdef ENABLE_PER_FLOW_TXTS_MAP_ENTRY
                        (dirty_state_map_tx_ts_db[to_db-1][tx_tbl_index].dirty_index) = 1;
#else
                        dirty_state_map_tx_ts_db[to_db-1]->dirty_index |= tx_ts_map_tag_index_to_dirty_chunk_bit_index(tx_tbl_index);
#endif
                }
#else
                if(dirty_state_map_tx_ts_db) {
#ifdef ENABLE_PER_FLOW_TXTS_MAP_ENTRY
                        (dirty_state_map_tx_ts_db[tx_tbl_index].dirty_index) = 1;
#else
                        dirty_state_map_tx_ts_db->dirty_index |= tx_ts_map_tag_index_to_dirty_chunk_bit_index(tx_tbl_index);
#endif
                }
#endif
        } else
#endif
        if(dirty_state_map_tx_ts) {
#ifdef ENABLE_PER_FLOW_TXTS_MAP_ENTRY
                dirty_state_map_tx_ts[tx_tbl_index].dirty_index = 1;
#else
                dirty_state_map_tx_ts->dirty_index |= tx_ts_map_tag_index_to_dirty_chunk_bit_index(tx_tbl_index);
#endif
        }
        return;
}
#endif
static inline int update_flow_tx_ts_table(__attribute__((unused)) uint64_t flow_index,  __attribute__((unused)) uint64_t ts, __attribute__((unused)) uint8_t to_db) { // __attribute__((unused)) struct onvm_pkt_meta* meta,  __attribute__((unused)) struct onvm_flow_entry *flow_entry) {
#ifdef ENABLE_PER_FLOW_TS_STORE
        if(unlikely(flow_index >=MAX_TX_TS_ENTRIES)){
                printf("\n Incorrect Index:%lld\n", (long long)flow_index);
                return 0;
        }
#ifdef ENABLE_RSYNC_WITH_DOUBLE_BUFFERING_MODE
        if(to_db) {
#ifdef ENABLE_RSYNC_MULTI_BUFFERING
                if(tx_ts_table_db[to_db-1]) {
                        if(unlikely(replay_mode)){  //only in replay mode; check if TS is less than already synced value; in that case; do not update but request to drop the packet
                                if(unlikely(ts <= tx_ts_table_db[to_db-1][flow_index].ts)) return 1;
                        }
                        tx_ts_table_db[to_db-1][flow_index].ts = ts;
                        tx_ts_update_dirty_state_index(flow_index,to_db);
                }
#else
                if(tx_ts_table_db) {
                        if(unlikely(replay_mode)){  //only in replay mode; check if TS is less than already synced value; in that case; do not update but request to drop the packet
                                if(unlikely(ts <= tx_ts_table[flow_index].ts)) return 1;
                        }
                        tx_ts_table_db[flow_index].ts = ts;
                        tx_ts_update_dirty_state_index(flow_index, to_db);
                }
#endif
        } else
#endif
        if(tx_ts_table) {
                if(unlikely(replay_mode)){  //only in replay mode; check if TS is less than already synced value; in that case; do not update but request to drop the packet
                        if(unlikely(ts <= tx_ts_table[flow_index].ts)) return 1;
                }
                tx_ts_table[flow_index].ts = ts;
                tx_ts_update_dirty_state_index(flow_index, to_db);
        }
#endif
        return 0;
}

static inline int initialize_tx_ts_table(void) {
#ifdef ENABLE_PER_FLOW_TS_STORE
        if(onvm_mgr_tx_per_flow_ts_info) {

#ifdef ENABLE_PER_FLOW_TXTS_MAP_ENTRY
                dirty_state_map_tx_ts = (dirty_mon_state_map_tbl_txts_t*)onvm_mgr_tx_per_flow_ts_info;
                tx_ts_table = (onvm_per_flow_ts_info_t*)(dirty_state_map_tx_ts+SDN_FT_ENTRIES);
#else
                dirty_state_map_tx_ts = (dirty_mon_state_map_tbl_t*)onvm_mgr_tx_per_flow_ts_info;
                tx_ts_table = (onvm_per_flow_ts_info_t*)(dirty_state_map_tx_ts+1);
#endif
        }
#ifdef ENABLE_RSYNC_WITH_DOUBLE_BUFFERING_MODE
#ifdef ENABLE_RSYNC_MULTI_BUFFERING
        int i = 0;
        for(; i <ENABLE_RSYNC_MULTI_BUFFERING; ++i ) {
                if(onvm_mgr_tx_per_flow_ts_info_db[i]) {
#ifdef ENABLE_PER_FLOW_TXTS_MAP_ENTRY
                        dirty_state_map_tx_ts_db[i] = (dirty_mon_state_map_tbl_txts_t*)onvm_mgr_tx_per_flow_ts_info_db[i];
                        tx_ts_table_db[i] = (onvm_per_flow_ts_info_t*)(dirty_state_map_tx_ts+SDN_FT_ENTRIES);
#else
                        dirty_state_map_tx_ts_db[i] = (dirty_mon_state_map_tbl_t*)onvm_mgr_tx_per_flow_ts_info_db[i];
                        tx_ts_table_db[i] = (onvm_per_flow_ts_info_t*)(dirty_state_map_tx_ts+1);
#endif
                }
        }
#else
        if(onvm_mgr_tx_per_flow_ts_info_db) {
#ifdef ENABLE_PER_FLOW_TXTS_MAP_ENTRY
                dirty_state_map_tx_ts_db = (dirty_mon_state_map_tbl_txts_t*)onvm_mgr_tx_per_flow_ts_info_db;
                tx_ts_table_db = (onvm_per_flow_ts_info_t*)(dirty_state_map_tx_ts+SDN_FT_ENTRIES);
#else
                dirty_state_map_tx_ts_db = (dirty_mon_state_map_tbl_t*)onvm_mgr_tx_per_flow_ts_info_db;
                tx_ts_table_db = (onvm_per_flow_ts_info_t*)(dirty_state_map_tx_ts+1);
#endif
        }
#endif
#endif
#endif
        return 0;
}
/***********************TX STATE TABLE UPDATE**********************************/

/***********************Internal Functions************************************/
/***********************PACKET TRANSMIT FUNCTIONS******************************/
static inline int send_packets_out(uint8_t port_id, uint16_t queue_id, struct rte_mbuf **tx_pkts, uint16_t nb_pkts) {

        if(unlikely(port_id >= ports->num_ports)){
                uint16_t k = 0;
                for(;k<nb_pkts;k++) {
                        onvm_pkt_drop(tx_pkts[k]);
                }
                return nb_pkts;
        }

#ifdef ENABLE_PCAP_CAPTURE_ON_OUTPUT
        onvm_util_log_packets(port_id,tx_pkts,NULL,nb_pkts);
#endif

#ifdef PROFILE_PACKET_PROCESSING_LATENCY
        onvm_util_calc_chain_processing_latency(tx_pkts, nb_pkts);
#endif
        if(nb_pkts > RTE_MP_TX_DESC_DEFAULT)
        {
                printf("Got %d packets >> %d ring buffer size on Port:%d Queue:%d ", nb_pkts, RTE_MP_TX_DESC_DEFAULT, port_id, queue_id);
                exit(2);
        }
        uint16_t sent_packets = rte_eth_tx_burst(port_id,queue_id, tx_pkts, nb_pkts);

        if(unlikely(sent_packets < nb_pkts)) {
                uint16_t i = sent_packets;
                for(; i< nb_pkts;i++)
                        onvm_pkt_drop(tx_pkts[i]);
        }

        {
                volatile struct tx_stats *tx_stats = &(ports->tx_stats);
                tx_stats->tx[port_id] += sent_packets;
                tx_stats->tx_drop[port_id] += (nb_pkts - sent_packets);
        }
        return sent_packets;
}
static inline int log_transaction_and_send_packets_out(uint8_t trans_id, uint8_t bNotify, uint8_t port_id, uint16_t queue_id, struct rte_mbuf **tx_pkts, uint16_t nb_pkts) {
        log_transaction_id(trans_id, bNotify);
        return send_packets_out(port_id, queue_id, tx_pkts, nb_pkts);
}
//Bypass Function to directly enqueue to Tx Port Ring and Flush to ETH Ports
int transmit_tx_port_packets(void) {
        uint16_t j, count= PACKET_READ_SIZE_LARGE, sent=0;
        struct rte_mbuf *pkts[PACKET_READ_SIZE_LARGE];
#if 0
#ifdef MIMIC_PICO_REP
        struct timespec req = {0,REMOTE_SYNC_WAIT_INTERVAL}, res = {0,0};
#endif
#endif
        for(j=0; j < MIN(ports->num_ports, ONVM_NUM_RSYNC_PORTS); j++) {
                unsigned tx_count = rte_ring_count(tx_port_ring[j]);
                //printf("\n %d Pkts in %d port\n", tx_count, j);
                while(tx_count) {
                        if(unlikely(0==(count = rte_ring_dequeue_burst(tx_port_ring[j], (void**)pkts, PACKET_READ_SIZE_LARGE,NULL)))) break;
#if 0
                        uint16_t i;
                        if(unlikely(j == (ONVM_NUM_RSYNC_PORTS-1))) {
                                for(i=0; i < count;i++) {
                                        uint8_t port = (onvm_get_pkt_meta((struct rte_mbuf*) pkts[i]))->destination;
                                        sent = send_packets_out(port,RSYNC_TX_PORT_QUEUE_ID_0, &pkts[i],1);
                                }
                        } else
#endif
                        {
#if 1 // note modified to use single port; switch to 1 if use multiple port array tx_port_ring[]
                                sent = send_packets_out(j,RSYNC_TX_PORT_QUEUE_ID_0, pkts,count);
#if 0
#ifdef MIMIC_PICO_REP
                                if(sent < count) {
                                        nanosleep(&req, &res); // avoid processed packet drops to boost throughput..
                                }
#endif
#endif
#else
                                for(i=0; i < count;i++) {
                                        uint8_t port = (onvm_get_pkt_meta((struct rte_mbuf*) pkts[i]))->destination;
                                        sent = send_packets_out(port,RSYNC_TX_PORT_QUEUE_ID_0, &pkts[i],1);
                                }
#endif
                        }
                        if(likely(tx_count > count)) tx_count-=count;
                        else break;
                }
        }
        return sent;
}

//Function to transmit/release the Tx packets (that were waiting for Tx state update completion)
static int transmit_tx_tx_state_latch_rings( __attribute__((unused))uint8_t to_db) {

        uint16_t j, count= PACKET_READ_SIZE_LARGE, sent=0,tx_count = 0;
        struct rte_mbuf *pkts[PACKET_READ_SIZE_LARGE];
        struct rte_ring *latch_ring;

        for(j=0; j < MIN(ports->num_ports, ONVM_NUM_RSYNC_PORTS); j++) {

#ifdef ENABLE_RSYNC_WITH_DOUBLE_BUFFERING_MODE
                if(to_db){
#ifdef ENABLE_RSYNC_MULTI_BUFFERING
                        latch_ring = tx_tx_state_latch_db_ring[to_db-1][j];
#else
                        latch_ring = tx_tx_state_latch_db_ring[j];
#endif
                } else
#endif
                        latch_ring = tx_tx_state_latch_ring[j]; //tx_count = rte_ring_count(tx_tx_state_latch_ring[j]);

                tx_count = rte_ring_count(latch_ring);

                //printf("\n %d Pkts in tx_tx_state_latch_ring[%d] port\n", tx_count, j);
                while(tx_count) {
                        count = rte_ring_sc_dequeue_burst(latch_ring, (void**)pkts, PACKET_READ_SIZE_LARGE,NULL);
                        if(unlikely(0 == count)) {break;}
                        //Logic is to allocate equal number of ring buffers as port; so no need to check for now.
#if 0
                        uint16_t i;
                        if(unlikely(j == (ONVM_NUM_RSYNC_PORTS-1))) {
                                for(i=0; i < count;i++) {
                                        uint8_t port = (onvm_get_pkt_meta((struct rte_mbuf*) pkts[i]))->destination;
                                        sent = send_packets_out(port,RSYNC_TX_PORT_QUEUE_ID_1, &pkts[i],1);
                                }
                        } else
#endif
                        {
                                sent = send_packets_out(j,RSYNC_TX_PORT_QUEUE_ID_1, pkts,count);
                        }
                        if(likely(tx_count > count)) tx_count-=count;
                        else break;
                }
        }
        return sent;
}
//Function to transmit/release the Tx packets (that were waiting for NF state update completion)
static int transmit_tx_nf_state_latch_rings( __attribute__((unused))uint8_t to_db) {

        uint16_t j, count= PACKET_READ_SIZE_LARGE, sent=0, tx_count = 0;
        struct rte_mbuf *pkts[PACKET_READ_SIZE_LARGE];
        struct rte_ring *latch_ring;

        for(j=0; j < MIN(ports->num_ports, ONVM_NUM_RSYNC_PORTS); j++) {
#ifdef ENABLE_RSYNC_WITH_DOUBLE_BUFFERING_MODE
                if(to_db){
#ifdef ENABLE_RSYNC_MULTI_BUFFERING
                        latch_ring = tx_nf_state_latch_db_ring[to_db-1][j];
#else
                        latch_ring = tx_nf_state_latch_db_ring[j];
#endif
                } else
#endif
                        latch_ring = tx_nf_state_latch_ring[j];

                tx_count = rte_ring_count(latch_ring);

                //printf("\n %d Pkts in tx_nf_state_latch[%d] port\n", tx_count, j);
                while(tx_count) {
                        count = rte_ring_sc_dequeue_burst(latch_ring, (void**)pkts, PACKET_READ_SIZE_LARGE,NULL);
                        if(unlikely(0==count)){break;}
#if 0
                        uint16_t i;
                        if(unlikely(j == (ONVM_NUM_RSYNC_PORTS-1))) {
                                for(i=0; i < count;i++) {
                                        uint8_t port = (onvm_get_pkt_meta((struct rte_mbuf*) pkts[i]))->destination;
                                        sent = send_packets_out(port,RSYNC_TX_PORT_QUEUE_ID_1, &pkts[i],1);
                                }
                        } else
#endif
                        {
                                sent = send_packets_out(j,RSYNC_TX_PORT_QUEUE_ID_1, pkts,count);
                        }
                        if(tx_count > count) tx_count-=count;
                        else break;
                }
        }
        return sent;
}
static inline int get_flow_entry_index(__attribute__((unused)) struct rte_mbuf *pkt, __attribute__((unused)) struct onvm_pkt_meta *meta) {
#ifdef ENABLE_FT_INDEX_IN_META
        return meta->ft_index;
#else
        {
                //printf("\n Extracting from Flow DIR Entry: \n");
                struct onvm_flow_entry *flow_entry = NULL;
                onvm_flow_dir_get_pkt(pkt, &flow_entry);
                if(flow_entry) return flow_entry->entry_index;
        }
#endif
        return -1;
}
/* This Function extracts the packets from the Tx_Port_RingBuffers and
 * 1. For Each packet:
 *  a) Extract the Time Stamp Information and Update to TX TS Table
 *  b) Determine where to Destination Ring buffer: where to Enqueue the packet
 *      i) if Marked Critical in Meta; then  Enqueue to NF_STATE_LATCH ring
 *      ii)if Not Marked Critical in Meta; then  Enqueue to TX_STATE_LATCH ring
 * Note: The NF STATE LATCH for subsequent packets need to persist. therefore
 * When NF marks in meta; either NF or NFLIB must mark the info in FT
 * This rsync module can clear this FT information only when the NF State
 * is synchronized to remote node.
 */
static int extract_and_parse_tx_port_packets(__attribute__((unused)) uint8_t to_db) {
        int ret = 0;
        uint16_t i, j, count= PACKET_READ_SIZE_LARGE, sent=0;
        uint64_t ts[PACKET_READ_SIZE_LARGE];
        uint16_t out_pkts_nf_count, out_pkts_tx_count, out_pkts_dr_count;
        uint8_t tx_ret_sts=0;
        struct rte_mbuf *in_pkts[PACKET_READ_SIZE_LARGE];
        struct rte_mbuf *out_pkts_tx[PACKET_READ_SIZE_LARGE];
        struct rte_mbuf *out_pkts_nf[PACKET_READ_SIZE_LARGE];
        struct rte_mbuf *out_pkts_drop[PACKET_READ_SIZE_LARGE];
        struct onvm_pkt_meta* meta;
        struct rte_ring *latch_ring_tx;
        struct rte_ring *latch_ring_nf;

        for(j=0; j < MIN(ports->num_ports, ONVM_NUM_RSYNC_PORTS); j++) {
                unsigned tx_count = rte_ring_count(tx_port_ring[j]);
                unsigned max_count = 0;
                unsigned rem_count = 0;

#ifdef ENABLE_RSYNC_WITH_DOUBLE_BUFFERING_MODE
                if(to_db){
#ifdef ENABLE_RSYNC_MULTI_BUFFERING
                        latch_ring_tx = tx_tx_state_latch_db_ring[to_db-1][j];
                        latch_ring_nf = tx_nf_state_latch_db_ring[to_db-1][j];
#else
                        latch_ring_tx = tx_tx_state_latch_db_ring[j];
                        latch_ring_nf = tx_nf_state_latch_db_ring[j];
#endif
                } else
#endif
                {
                        latch_ring_tx = tx_tx_state_latch_ring[j];
                        latch_ring_nf = tx_nf_state_latch_ring[j];
                }

                rem_count = rte_ring_free_count(latch_ring_tx);
                if(unlikely(0 == rem_count)) {
                        ret |= TX_TS_LATCH_BUFFER_FULL;
                        continue; //assuming all/some packets make it to latch_ring_tx, will waste, so switch to next port without processing any from current port.
                }
                //printf("\n %d packets in tx_port_ring[%d]\n", tx_count, j);
                while(tx_count) {
                        //retrieve batch of packets
                        count = rte_ring_dequeue_burst(tx_port_ring[j], (void**)in_pkts, PACKET_READ_SIZE_LARGE,NULL);    //MIN(rem_count,PACKET_READ_SIZE_LARGE)
                        //extract timestamp for these batch of packets
                        onvm_util_get_marked_packet_timestamp((struct rte_mbuf**)in_pkts, ts, count);
                        out_pkts_tx_count = 0; out_pkts_nf_count = 0;out_pkts_dr_count=0;
                        for(i=0; i < count;i++) {
                                meta = onvm_get_pkt_meta((struct rte_mbuf *)in_pkts[i]);
                                //uint8_t port = meta->destination;
                                uint8_t dest = meta->reserved_word&NF_NEED_ND_SYNC; //Hack
                                int flow_index = get_flow_entry_index(in_pkts[i], meta);
                                if(flow_index >= 0) {
#ifdef ENABLE_PER_FLOW_TS_STORE
                                tx_ret_sts = update_flow_tx_ts_table(flow_index, ts[i],to_db);
                                ret |=NEED_REMOTE_TS_TABLE_SYNC;
#endif
                                }
                                //If Flow/Packet needs NF state to be synchronized
                                if(unlikely(dest)) {
                                        out_pkts_nf[out_pkts_nf_count++] = in_pkts[i];
                                        ret |=NEED_REMOTE_NF_STATE_SYNC;
                                        //out_pkts_tx[out_pkts_tx_count++] = in_pkts[i];  //hack: test performance: save in tx_ts anyway the sync NF will be done before Tx_ts in this case.
                                } else {
                                        if(unlikely(tx_ret_sts)) {
                                                out_pkts_drop[out_pkts_dr_count++]=in_pkts[i];
                                        }
                                        else out_pkts_tx[out_pkts_tx_count++] = in_pkts[i];
                                }
                        }
                        if(unlikely(out_pkts_dr_count)){
                                uint16_t k = 0;
                                for(;k<out_pkts_dr_count;k++) {
                                        onvm_pkt_drop(out_pkts_drop[k]);
                                }
                        }
                        if(likely(out_pkts_tx_count)){
                                sent = rte_ring_enqueue_burst(latch_ring_tx, (void**)out_pkts_tx,  out_pkts_tx_count,NULL);
                                if (unlikely(sent < out_pkts_tx_count)) {
                                        uint8_t k = sent;
                                        for(;k<out_pkts_tx_count;k++) {
                                                onvm_pkt_drop(out_pkts_tx[k]);
                                        }
                                        ret |= TX_TS_LATCH_BUFFER_FULL;
                                }
#ifdef ENABLE_PORT_TX_STATS_LOGS
#ifdef ENABLE_RSYNC_WITH_DOUBLE_BUFFERING_MODE
                                if(to_db){
#ifdef ENABLE_RSYNC_MULTI_BUFFERING
                                        rsync_stat.enq_coun_tx_tx_state_latch_db_ring[to_db-1][j] += sent;
                                        rsync_stat.drop_count_tx_tx_state_latch_db_ring[to_db-1][j] += (out_pkts_tx_count -sent);
#else
                                        rsync_stat.enq_coun_tx_tx_state_latch_db_ring[j] += sent;
                                        rsync_stat.drop_count_tx_tx_state_latch_db_ring[j] += (out_pkts_tx_count -sent);
#endif
                                } else
#endif
                                {
                                        rsync_stat.enq_coun_tx_tx_state_latch_ring[j] += sent;
                                        rsync_stat.drop_count_tx_tx_state_latch_ring[j] += (out_pkts_tx_count -sent);
                                }
#endif
                        }
                        if(unlikely(out_pkts_nf_count)){
                                sent = rte_ring_enqueue_burst(latch_ring_nf, (void**)out_pkts_nf,  out_pkts_nf_count,NULL);

                                if (unlikely(sent < out_pkts_nf_count)) {
                                        uint16_t k = sent;
                                        for(;k<out_pkts_nf_count;k++) {
                                                onvm_pkt_drop(out_pkts_nf[k]);
                                        }
                                        ret |= NF_STATE_LATCH_BUFFER_FULL;
                                }
#ifdef ENABLE_PORT_TX_STATS_LOGS
#ifdef ENABLE_RSYNC_WITH_DOUBLE_BUFFERING_MODE
                                if(to_db){
#ifdef ENABLE_RSYNC_MULTI_BUFFERING
                                        rsync_stat.enq_count_tx_nf_state_latch_db_ring[to_db-1][j] += sent;
                                        rsync_stat.drop_count_tx_nf_state_latch_db_ring[to_db-1][j] += (out_pkts_nf_count -sent);
#else
                                        rsync_stat.enq_count_tx_nf_state_latch_db_ring[j] += sent;
                                        rsync_stat.drop_count_tx_nf_state_latch_db_ring[j] += (out_pkts_nf_count -sent);
#endif
                                } else
#endif
                                {
                                        rsync_stat.enq_count_tx_nf_state_latch_ring[j] += sent;
                                        rsync_stat.drop_count_tx_nf_state_latch_ring[j] += (out_pkts_nf_count -sent);
                                }
#endif
                        }
                        max_count+=count;
                        if(tx_count > count){
                                tx_count-=count;
                        }
                        else {
#ifdef ENABLE_OPPROTUNISTIC_MAX_POLL
                                if(max_count >= MAX_PACKETS_IN_A_ROUND) {
                                        break;
                                }
                                tx_count = rte_ring_count(tx_port_ring[j]);
#else
                                //naive scoreboarding approach- that esnures to process 1 lot of packets (as seen at start of processing)
                                break;
#endif
                        }
                }
        }
        return ret;
}
/***********************PACKET TRANSMIT FUNCTIONS******************************/
/* PACKET RECEIVE FUNCTIONS */
static inline int rsync_process_req_packet(__attribute__((unused)) state_transfer_packet_hdr_t *rsync_req, uint8_t in_port, uint16_t data_len) {

        state_tx_meta_t meta_out = rsync_req->meta;
        struct rte_mbuf *pkt = NULL;

        bswap_rsync_hdr_data(&meta_out, 0);
#ifdef ENABLE_EXTRA_RSYNC_PRINT_MSGS
        printf("\n Received RSYNC Request Packet with Transaction:[%d] for [Type:%d, SVC/NFID:%d, offset:[%d]] !\n", meta_out.trans_id, meta_out.state_type, meta_out.nf_or_svc_id, meta_out.start_offset);
#endif

        //For Tx_TS State:  copy sent data from the start_offset to the mempool.
        //For NF_STATE_MEMORY: <Communicate to Standby NF, if none; then it must be instantiated first; then send message to NFLIB so that it can copy the state
        //FOR_SVC_STATE_MEMORY:

        switch(meta_out.state_type) {
        case STATE_TYPE_TX_TS_TABLE:
                //update TX_TS_TABLE and send Response to TID
                rsync_tx_ts_state_from_remote(&meta_out, rsync_req->data, MIN(data_len, MAX_STATE_SIZE_PER_PACKET));
                rsync_stat.tx_state_sync_in_pkt_counter++;
                break;
        case STATE_TYPE_NF_MEMPOOL:
                //update NF_MEMPOOL_TABLE and send Response to TID
                rsync_nf_state_from_remote(&meta_out, rsync_req->data, MIN(data_len, MAX_STATE_SIZE_PER_PACKET));
                rsync_stat.nf_state_sync_in_pkt_counter[meta_out.nf_or_svc_id]++;
                break;
        case STATE_TYPE_SVC_MEMPOOL:
                //update SVC_MEMPOOL_TABLE and send Response to TID
                rsync_nf_state_from_remote(&meta_out, rsync_req->data, MIN(data_len, MAX_STATE_SIZE_PER_PACKET));
                break;
        case MSG_START_OF_REPLAY:
                if(!replay_mode)replay_mode=1;
                rsycn_handle_start_of_replay();
                return meta_out.state_type;
                break;
        case MSG_END_OF_REPLAY:
                if(replay_mode) replay_mode=0;
                rsycn_handle_end_of_replay();
                return meta_out.state_type;
                break;
        default:
                //unknown packet type;
                return meta_out.state_type;
                break;
        }
        //rte_be_to_cpu_16(pkt->data_len)
        //send response packet//printf("Prepare Response Message with state:%d\n", meta_out.state_type);
        meta_out.state_type = (meta_out.state_type<<STATE_REQ_TO_RSP_LSH);
        pkt = craft_state_update_packet(in_port,meta_out,NULL,0);
        if(pkt) {
                send_packets_out(in_port, RSYNC_TX_PORT_QUEUE_ID_0, &pkt, 1);
        }

        return 0;
}
static inline int rsync_process_rsp_packet(__attribute__((unused)) transfer_ack_packet_hdr_t *rsync_rsp) {
#if 0
        //Parse the transaction id and notify/unblock processing thread to release the packets out.
#endif
        uint8_t trans_id = rsync_rsp->meta.trans_id;
        rsync_stat.transactions_in_counter++;
        clear_transaction_id(trans_id);

#ifdef ENABLE_EXTRA_RSYNC_PRINT_MSGS
        //printf("\n Received RSYNC Response Packet with Transaction:[%d] for [Type:%d, SVC/NFID:%d, offset:[%d]] !\n", rsync_rsp->meta.trans_id, rsync_rsp->meta.state_type, rsync_rsp->meta.nf_or_svc_id, rsync_rsp->meta.start_offset);
        printf("\n Received RSYNC Response:: Transaction:[%d] for [Type:%d, SVC/NFID:%d] got committed!\n", trans_id, rsync_rsp->meta.state_type, rsync_rsp->meta.nf_or_svc_id);
#endif

        //will it be better to copy to temp and byte swap then byteswap packet memory?
        //bswap_rsync_hdr_data(&rsync_rsp->meta, 0);

        return 0;
}
/******************************APIs********************************************/
int rsync_process_rsync_in_pkts(__attribute__((unused)) struct thread_info *rx, struct rte_mbuf *pkts[], uint16_t rx_count) {
        uint16_t i=0;
        transfer_ack_packet_hdr_t *rsycn_pkt = NULL;
        state_transfer_packet_hdr_t *rsync_req = NULL;

        //Validate packet properties
        //if(pkts[i]->pkt_len < (sizeof(struct ether_hdr) + sizeof(struct transfer_ack_packet_hdr_t));
        //if(pkts[i]->data_len < (sizeof(struct ether_hdr) + sizeof(struct state_transfer_packet_hdr_t));

        //process each packet
        for(i=0; i < rx_count; i++) {
                //eth = rte_pktmbuf_mtod(pkts[i], struct ether_hdr *);
                rsycn_pkt = (transfer_ack_packet_hdr_t*)(rte_pktmbuf_mtod(pkts[i], uint8_t*) + sizeof(struct ether_hdr));
#ifdef ENABLE_EXTRA_RSYNC_PRINT_MSGS
                printf("Received RSYNC Message Type [%d]:\n",rsync_print_rsp_packet(rsycn_pkt));
#endif
                if(rsycn_pkt) {
                        if( STATE_TYPE_RSP_MASK & rsycn_pkt->meta.state_type) {
                                //process the response packet: check for Tran ID and unblock 2 phase commit..
                                rsync_process_rsp_packet(rsycn_pkt);
                        }
                        else {
                                rsync_req = (state_transfer_packet_hdr_t*)(rte_pktmbuf_mtod(pkts[i], uint8_t*) + sizeof(struct ether_hdr));
                                rsync_process_req_packet(rsync_req, pkts[i]->port, rte_be_to_cpu_16(pkts[i]->data_len));
                                //process rsync_req packet: check the nf_svd_id; extract data and update mempool memory of respective NFs
                                //Once you receive last flag or flag with different Transaction ID then, Generate response packet for the (current) marked transaction.
                        }
                }
        }
        //release all the packets and return
        //onvm_pkt_drop_batch(pkts,rx_count);
        if(pkts) return rx_count;
        return 0;
}
#ifdef USE_BATCHED_RSYNC_TRANSACTIONS
#define MAX_EXTRACT_AND_PARSE_LOOP_COUNTER (2)
#else
#define MAX_EXTRACT_AND_PARSE_LOOP_COUNTER (1)
#endif

#if defined(MIMIC_PICO_REP)
#ifndef ENABLE_PICO_STATELESS_MODE
static int rsync_start_simple_pico_nf(__attribute__((unused)) void *arg);
static int rsync_start_simple_pico_nf(__attribute__((unused)) void *arg){

#if defined(ENABLE_PICO_EXPLICT_NF_PAUSE_RESUME) //this approach has wakeup issue: Why? -- When Pause is sent and Resume is sent together ( resume before NF actually pauses), NF sees the pause and blocks, Resume message is never dequeued, unless second resume is sent, which cannot happen either.
        uint16_t nf_id = 0;
        uint16_t nfs_paused[MAX_NFS]={0};
        uint8_t msg_count=0;
        //PAUSE the Active NFs
        for (nf_id = 1; nf_id < MAX_NFS; nf_id++) {
                nfs_paused[nf_id]=0;
                //Get the Processing (Actively Running and Processing packets) NF Instance ID */
                if (likely(onvm_nf_is_processing(&nfs[nf_id]))){
                        onvm_nf_send_msg(nfs[nf_id].info->instance_id,MSG_PAUSE,MSG_MODE_ASYNCHRONOUS,NULL);
                        nfs_paused[nf_id]=1;
                        msg_count++;
                }
        }
        //RTE_LOG(INFO, APP, "Total NFs moved to Pause %d:\n",msg_count);
        //printf("Total NFs moved to Pause %d:\n",msg_count);
#endif

        //check and Initiate remote NF Sync
        int trans_id = rsync_nf_state_to_remote(0);
        if(trans_id>0) {
                //printf("\nTranscation ID: %d", trans_id);
                rsync_wait_for_commit_ack(trans_id, CHECK_FOR_COMMIT_WITH_WAIT);
                //Now release the packets from NF
                transmit_tx_nf_state_latch_rings(0);
        }

#if defined(ENABLE_PICO_EXPLICT_NF_PAUSE_RESUME)
        //RESUME the PAUSED NFs
        for (nf_id = 0; nf_id < MAX_NFS; nf_id++) {

                //Get the Processing (Actively Running and Processing packets) NF Instance ID */
                if (likely(nfs_paused[nf_id])){
                        onvm_nf_send_msg(nfs[nf_id].info->instance_id,MSG_RESUME,MSG_MODE_ASYNCHRONOUS,NULL);
                        nfs_paused[nf_id]=1;
                }
        }
#endif

        return trans_id;
}
#endif
static int rsync_start_simple_pico(__attribute__((unused)) void *arg);
static int rsync_start_simple_pico(__attribute__((unused)) void *arg) {

#if 0
        rx_halt=1;
        //check and Initiate remote NF Sync
        int trans_id = rsync_nf_state_to_remote(0);
        if(trans_id>0) {
                //printf("\nTranscation ID: %d", trans_id);
                rsync_wait_for_commit_ack(trans_id, CHECK_FOR_COMMIT_WITH_WAIT);
                //Now release the packets from NF
                transmit_tx_nf_state_latch_rings(0);
        }
        rx_halt=0;
#endif

#if defined(MIMIC_PICO_REP) && !defined(ENABLE_PICO_RX_NON_BLOCKING_MODE)
        rx_halt =1;
#endif

#if !defined(ENABLE_PICO_STATELESS_MODE)        //If stateful then sync NF state
        rsync_start_simple_pico_nf(arg);
#endif

        transmit_tx_port_packets();

#if defined(MIMIC_PICO_REP) && !defined(ENABLE_PICO_RX_NON_BLOCKING_MODE)
        if(rx_halt)rx_halt=0;
#endif

        //Note: There is an issue without lock: while updating any new flow comes with new non-determinism then it might be released much earlier.
        return 0;
}
#endif
/* Simple Scheme without Double Buffering: Still more efficient than PICO Replication.
 * We can use this scheme as baseline for Pico Replication comparison. or FTMB approach
 * that performs VM checkpointing with Output commit on logged packets == committing Tx Ts.
 */
#if !defined(ENABLE_RSYNC_WITH_DOUBLE_BUFFERING_MODE)
static int rsync_start_simple(__attribute__((unused)) void *arg);
static int rsync_start_simple(__attribute__((unused)) void *arg) {
        //TEST_HACK to directly transfer out the packets
        //return transmit_tx_port_packets();
#ifdef USE_BATCHED_RSYNC_TRANSACTIONS
        uint8_t trans_ids[2] = {0,0},tid=0;
#endif

        //First Extract and Parse Tx Port Packets and update TS info in Tx Table
        int ret = extract_and_parse_tx_port_packets(0);
        //ret = 0; //TEST_HACK to bypass TS Table and NF Shared Memory packet transfer

        //Check and Initiate Remote Sync of Tx State
        if(ret & NEED_REMOTE_TS_TABLE_SYNC) {
                int trans_id = rsync_tx_ts_state_to_remote(0);
                if(trans_id >= 0) {
#ifdef USE_BATCHED_RSYNC_TRANSACTIONS
                        trans_ids[tid++] = trans_id;
#else
                        rsync_wait_for_commit_ack(trans_id, CHECK_FOR_COMMIT_WITH_WAIT);

                        //Now release the packets from Tx State Latch Ring
                        transmit_tx_tx_state_latch_rings(0);
#endif
                }
        }

        //TODO:communicate to Peer Node (Predecessor/Remote Node) to release the logged packets till TS.
        //How? -- there can be packets in fastchain and some in slow chain. How will you notify? -- rely on best effort (every 1ms) it will refresh.
        //14.88Mpps => 14.88K (~15K, 1.25MB) for 1ms,  and 100ms => (~1500K packets, 125MB) data.

        //check and Initiate remote NF Sync
        if(ret & NEED_REMOTE_NF_STATE_SYNC) {
                int trans_id = rsync_nf_state_to_remote(1);
                if(trans_id>=0) {
#ifdef USE_BATCHED_RSYNC_TRANSACTIONS
                        trans_ids[tid++] = trans_id;
#else
                        rsync_wait_for_commit_ack(trans_id, CHECK_FOR_COMMIT_WITH_WAIT);
                        //Now release the packets from NF
                        transmit_tx_nf_state_latch_rings(0);
#endif
                }
        }

#ifdef USE_BATCHED_RSYNC_TRANSACTIONS
        //optimize by batching transactions.. transfer all transactions and wait or acks
        if(0 == rsync_wait_for_commit_acks(trans_ids,tid,CHECK_FOR_COMMIT_WITH_WAIT)) {
                //Now release the packets from Tx State Latch Ring
                transmit_tx_tx_state_latch_rings(0);
                //Now release the packets from NF
                transmit_tx_nf_state_latch_rings(0);
        }
#endif
        //Note: There is an issue without lock: while updating any new flow comes with new non-determinism then it might be released much earlier.
        return 0;
}
#endif
/* Alternate variant: for correctness checking, which uses only the secondary/double buffers only
 * Instead of primary complete use the secondary/double buffer related resources.
 * Test purpose only, Do not use this for any use case.
 */
int rsync_start_only_db(__attribute__((unused)) void *arg);
int rsync_start_only_db(__attribute__((unused)) void *arg) {
        //TEST_HACK to directly transfer out the packets
        //return transmit_tx_port_packets();
#ifdef USE_BATCHED_RSYNC_TRANSACTIONS
        uint8_t trans_ids[2] = {0,0},tid=0;
#endif

        //First Extract and Parse Tx Port Packets and update TS info in Tx Table
        int ret = extract_and_parse_tx_port_packets(1);
        //ret = 0; //TEST_HACK to bypass TS Table and NF Shared Memory packet transfer

        //Check and Initiate Remote Sync of Tx State
        if(ret & NEED_REMOTE_TS_TABLE_SYNC) {
                int trans_id = rsync_tx_ts_state_to_remote(1);
                if(trans_id >= 0) {
#ifdef USE_BATCHED_RSYNC_TRANSACTIONS
                        trans_ids[tid++] = trans_id;
#else
                        rsync_wait_for_commit_ack(trans_id, CHECK_FOR_COMMIT_WITH_WAIT);

                        //Now release the packets from Tx State Latch Ring
                        transmit_tx_tx_state_latch_rings(1);
#endif
                }
        }

        //TODO:communicate to Peer Node (Predecessor/Remote Node) to release the logged packets till TS.
        //How? -- there can be packets in fastchain and some in slow chain. How will you notify? -- rely on best effort (every 1ms) it will refresh.
        //14.88Mpps => 14.88K (~15K, 1.25MB) for 1ms,  and 100ms => (~1500K packets, 125MB) data.

        //check and Initiate remote NF Sync
        if(ret & NEED_REMOTE_NF_STATE_SYNC) {
                int trans_id = rsync_nf_state_to_remote(1);
                if(trans_id>=0) {
#ifdef USE_BATCHED_RSYNC_TRANSACTIONS
                        trans_ids[tid++] = trans_id;
#else
                        rsync_wait_for_commit_ack(trans_id, CHECK_FOR_COMMIT_WITH_WAIT);
                        //Now release the packets from NF
                        transmit_tx_nf_state_latch_rings(1);
#endif
                }
        }

#ifdef USE_BATCHED_RSYNC_TRANSACTIONS
        //optimize by batching transactions.. transfer all transactions and wait or acks
        if(0 == rsync_wait_for_commit_acks(trans_ids,tid,CHECK_FOR_COMMIT_WITH_WAIT)) {
                //Now release the packets from Tx State Latch Ring
                transmit_tx_tx_state_latch_rings(1);
                //Now release the packets from NF
                transmit_tx_nf_state_latch_rings(1);
        }
#endif
        //Note: There is an issue without lock: while updating any new flow comes with new non-determinism then it might be released much earlier.
        return 0;
}
#ifdef ENABLE_RSYNC_MULTI_BUFFERING
static int rsync_start_simple_multi_db(__attribute__((unused)) void *arg);
#if 0
static int rsync_start_simple_multi_db(__attribute__((unused)) void *arg) {
        static uint8_t db_mode = 0;
        static uint8_t trans_ids[2] = {0,0},tid=0;
        static uint8_t trans_ids_db[ENABLE_RSYNC_MULTI_BUFFERING][2] = {{0,0},},tid_db[ENABLE_RSYNC_MULTI_BUFFERING]={0,};
        int ret = 0, trans_id =0, i=0;
        //int use_db_mode=0;

        if(tid){ //if((wait_mode & 1)) {
                if(0 == rsync_wait_for_commit_acks(trans_ids,tid,CHECK_FOR_COMMIT_WITH_NO_WAIT)) {
                        //Primary buffer transactions are complete; release buffers and start processing in primary buffer
                        tid=0; trans_ids[0] = trans_ids[1]=0;
                        //Now release the packets from Tx State Latch Ring
                        transmit_tx_tx_state_latch_rings(0);
                        //Now release the packets from NF
                        transmit_tx_nf_state_latch_rings(0);
                        db_mode=0;
                        //use_db_mode=0;
                } else {
                        //transaction on primary buffer still pending;
                        //use_db_mode=1;
                        //db_mode=1;
                }
        } else {
                db_mode=0;
        }
        for(i=0; i< ENABLE_RSYNC_MULTI_BUFFERING;++i) {
                if(tid_db[i]){ //if((wait_mode & 2)) {
                        if(0 == rsync_wait_for_commit_acks(trans_ids_db[i],tid_db[i],CHECK_FOR_COMMIT_WITH_NO_WAIT)) {
                                //Primary buffer transactions are complete; release buffers and start processing in primary buffer
                                tid_db[i]=0; trans_ids_db[i][0] = trans_ids_db[i][1]=0;
                                //Now release the packets from Tx State Latch Ring
                                transmit_tx_tx_state_latch_rings(i+1);
                                //Now release the packets from NF
                                transmit_tx_nf_state_latch_rings(i+1);

                                //if(use_db_mode && (use_db_mode > (i+1))) use_db_mode=i+1;
                                //if(db_mode && (db_mode > (i+1))) db_mode=i+1;

                        } else {
                                //use_db_mode+=1;db_mode+=1;
                                //if(db_mode>ENABLE_RSYNC_MULTI_BUFFERING) return 0;
                                //if(use_db_mode>ENABLE_RSYNC_MULTI_BUFFERING) return 0;
                        }
                } else {
                        //if(db_mode && (db_mode > (i+1))) db_mode=i+1;
                        //if(use_db_mode && (use_db_mode > (i+1))) use_db_mode=i+1;
                        ////if(db_mode && ((i+1) < use_db_mode)) use_db_mode=i+1;
                }
        }


        //check again for wait_mode
        if(db_mode  && (db_mode>ENABLE_RSYNC_MULTI_BUFFERING)) { //if(tid && (0==db_mode)){ //if(tid && tid_db) { //if((wait_mode) && ((wait_mode & 3)== wait_mode)) {
#ifdef ENABLE_EXTRA_RSYNC_PRINT_MSGS
                //cannot proceeed until trans is completed; so must wait;
                printf("\n Cannot Proceed for db_mode:%d [%d:[%d,%d]]\n", db_mode, tid, trans_ids[0], trans_ids[1]);
                printf("\n DB_TIDS:");
                for(i=0;i<ENABLE_RSYNC_MULTI_BUFFERING;++i) {
                        printf("%d:[%d[%d, %d]]\t ",i, tid_db[i],trans_ids_db[i][0], trans_ids_db[i][1]);
                }printf("\n");
#endif

                return 0;
        }
        //if(db_mode == 2) printf("\n $$$$$$$$$$$$$$$$$$$$ Selecting db=2$$$$$$$$$$$$ \n");
        //db_mode=use_db_mode;

        //First Extract and Parse Tx Port Packets and update TS info in Tx Table
        ret = extract_and_parse_tx_port_packets(db_mode);
        //ret = 0; //TEST_HACK to bypass TS Table and NF Shared Memory packet transfer

        //Check and Initiate Remote Sync of Tx State
        if(ret & NEED_REMOTE_TS_TABLE_SYNC) {
                trans_id = rsync_tx_ts_state_to_remote(db_mode);
                if(trans_id >= 0) {
                        if(db_mode) {
                                trans_ids_db[db_mode-1][(tid_db[db_mode-1])++] = (uint8_t) trans_id;
                        }
                        else trans_ids[tid++] = (uint8_t)  trans_id;
                }
        }

        //TODO:communicate to Peer Node (Predecessor/Remote Node) to release the logged packets till TS.
        //How? -- there can be packets in fastchain and some in slow chain. How will you notify? -- rely on best effort (every 1ms) it will refresh.
        //14.88Mpps => 14.88K (~15K, 1.25MB) for 1ms,  and 100ms => (~1500K packets, 125MB) data.

        //check and Initiate remote NF Sync
        if(ret & NEED_REMOTE_NF_STATE_SYNC) {
                int trans_id = rsync_nf_state_to_remote(1);
                if(trans_id>=0) {
                        if(db_mode){
                                trans_ids_db[db_mode-1][(tid_db[db_mode-1])++] = (uint8_t)  trans_id;
                        }
                        else {
                                trans_ids[tid++] = (uint8_t)  trans_id;
                        }
                }
        }
        if(ret) {
                ++db_mode;
        }
        //Note: There is an issue without lock: while updating any new flow comes with new non-determinism then it might be released much earlier. -- this is okay to release? mostly no..;
        return 0;
}
#else
//use this version of the function: more simple and less state.
static inline int rsync_start_simple_multi_db(__attribute__((unused)) void *arg) {
        static uint8_t trans_ids[2] = {0,0},tid=0;
        static uint8_t trans_ids_db[ENABLE_RSYNC_MULTI_BUFFERING][2] = {{0,0},},tid_db[ENABLE_RSYNC_MULTI_BUFFERING]={0,};
        int ret = 0, trans_id =0, i=0, buff_avail=-1;
//#define ENABLE_OPPROTUNISTIC_MAX_POLL    //noticed that enabling this logic in this function is resulting in throughput drop from 11.5Mpps to 8.07Mpps and also increases latency from 480us to 780us for basic monitor NF... Why??
#ifdef ENABLE_OPPROTUNISTIC_MAX_POLL
        int avail_buf_count=0,cur_buf_index=-1;
RECHECK_AND_REEXTRACT:
#endif

        if(node_role != PRIMARY_NODE) {
                return transmit_tx_port_packets();
        }

        if(likely(tid)) //if(tid)
        {
                //if(0 == rsync_wait_for_commit_acks(trans_ids,tid,CHECK_FOR_COMMIT_WITH_NO_WAIT))
                if(likely(0 == rsync_wait_for_commit_acks(trans_ids,tid,CHECK_FOR_COMMIT_WITH_NO_WAIT)))
                {
                        //Primary buffer transactions are complete; release buffers and start processing in primary buffer
                        tid=0; trans_ids[0] = trans_ids[1]=0;
                        //Now release the packets from NF
                        transmit_tx_nf_state_latch_rings(0);
                        //Now release the packets from Tx State Latch Ring
                        transmit_tx_tx_state_latch_rings(0);
                        //set buffer available at free index=0
                        buff_avail=0;
                        //use_db_mode=0;
#ifdef ENABLE_OPPROTUNISTIC_MAX_POLL
                        avail_buf_count++;
#endif
                } /*else {
                        //transaction on primary buffer still pending;
                        //buff_avail=-1;
                }*/
        } else {
                buff_avail=0;
#ifdef ENABLE_OPPROTUNISTIC_MAX_POLL
                avail_buf_count++;
#endif
        }

        for(i=0; i< ENABLE_RSYNC_MULTI_BUFFERING;++i) {
                if(likely(tid_db[i])){ //if((wait_mode & 2)) {
                        //if(0 == rsync_wait_for_commit_acks(trans_ids_db[i],tid_db[i],CHECK_FOR_COMMIT_WITH_NO_WAIT)) {
                        if(likely(0 == rsync_wait_for_commit_acks(trans_ids_db[i],tid_db[i],CHECK_FOR_COMMIT_WITH_NO_WAIT)))  {
                                //Primary buffer transactions are complete; release buffers and start processing in primary buffer
                                tid_db[i]=0; trans_ids_db[i][0] = trans_ids_db[i][1]=0;
                                //Now release the packets from NF
                                transmit_tx_nf_state_latch_rings((i+1));
                                //Now release the packets from Tx State Latch Ring
                                transmit_tx_tx_state_latch_rings((i+1));
                                //set the buffer available index  to i+1;
                                if(buff_avail < 0) {
                                        buff_avail=i+1;
                                }
#ifdef ENABLE_OPPROTUNISTIC_MAX_POLL
                                avail_buf_count++;
#endif
                        }
                } else {
                        if(buff_avail < 0) { // use this free buffer only if primary is not free.
                                buff_avail=i+1;
                        }
#ifdef ENABLE_OPPROTUNISTIC_MAX_POLL
                        avail_buf_count++;
#endif
                }
        }

        if(unlikely(buff_avail < 0)) {
#ifdef ENABLE_EXTRA_RSYNC_PRINT_MSGS
                printf("\n No buffers Available:%d \t [%d:[%d,%d]]\n",buff_avail, tid, trans_ids[0], trans_ids[1]);
                printf("\n DB_TIDS:");
                for(i=0;i<ENABLE_RSYNC_MULTI_BUFFERING;++i) {
                        printf("%d:[%d[%d, %d]]\t ",i, tid_db[i],trans_ids_db[i][0], trans_ids_db[i][1]);
                }printf("\n");
#endif
                return 0;
        }


#ifdef ENABLE_OPPROTUNISTIC_MAX_POLL
        //Check whether to repeat the current buffer or use the new identified free buffer?
        if(cur_buf_index>=0) {
                ret = extract_and_parse_tx_port_packets(cur_buf_index); //((avail_buf_count==1)?(1):(0))
        }
        else {
                ret = extract_and_parse_tx_port_packets(buff_avail); //((avail_buf_count==1)?(1):(0))
        }
#else
        //First Extract and Parse Tx Port Packets and update TS info in Tx Table
        ret = extract_and_parse_tx_port_packets(buff_avail);
#endif
        //ret = 0; //TEST_HACK to bypass TS Table and NF Shared Memory packet transfer

#ifdef ENABLE_OPPROTUNISTIC_MAX_POLL
        //Last transaction buffer and still space left, then reprocess to fetch more packets from port_ring_buffer!
        if((1 == avail_buf_count) && (!(ret & TX_TS_LATCH_BUFFER_FULL))) {
                cur_buf_index = buff_avail;
                goto RECHECK_AND_REEXTRACT;
        }
#endif


        //check and Initiate remote NF Sync
        //if(ret & NEED_REMOTE_NF_STATE_SYNC)
        if(unlikely(ret & NEED_REMOTE_NF_STATE_SYNC))
        {
                trans_id = rsync_nf_state_to_remote(1);
                if(trans_id>=0) {
                        if(buff_avail){
                                trans_ids_db[buff_avail-1][(tid_db[buff_avail-1])++] = (uint8_t)  trans_id;
                        }
                        else {
                                trans_ids[tid++] = (uint8_t)  trans_id;
                        }
                }
        }
        //Note: There is an issue without lock: while updating any new flow comes with new non-determinism then it might be released much earlier. -- this is okay to release? mostly no..;

        //Check and Initiate Remote Sync of Tx State
        //if(ret & NEED_REMOTE_TS_TABLE_SYNC)//
        if(likely(ret & NEED_REMOTE_TS_TABLE_SYNC))
        {
                trans_id = rsync_tx_ts_state_to_remote(buff_avail);
                if(trans_id >= 0) {
                        if(buff_avail) {
                                trans_ids_db[buff_avail-1][(tid_db[buff_avail-1])++] = (uint8_t) trans_id;
                        }
                        else {
                                trans_ids[tid++] = (uint8_t)  trans_id;
                        }
                }
        }

        //TODO:communicate to Peer Node (Predecessor/Remote Node) to release the logged packets till TS.
        //How? -- there can be packets in fastchain and some in slow chain. How will you notify? -- rely on best effort (every 1ms) it will refresh.
        //14.88Mpps => 14.88K (~15K, 1.25MB) for 1ms,  and 100ms => (~1500K packets, 125MB) data.

        return 0;
}
#endif
#endif
/* Simple Double Buffering Scheme: Avoid wait on transaction completion. Instead,
 * Flip from the Primary buffer to Double Buffer and each time process 1 transaction.
 * If both the Primary and double buffer transactions are pending then we need to
 * wait till one of them completes and then continue processing on available buffer.
 */
#ifdef ENABLE_SIMPLE_DOUBLE_BUFFERING_MODE
static int rsync_start_simple_db(__attribute__((unused)) void *arg);
static int rsync_start_simple_db(__attribute__((unused)) void *arg) {
        static uint8_t db_mode = 0;
        static uint8_t trans_ids[2] = {0,0},tid=0;
        static uint8_t trans_ids_db[2] = {0,0},tid_db=0;
        int ret = 0, trans_id =0, use_db_mode=0;

        if(tid){ //if((wait_mode & 1)) {
                if(0 == rsync_wait_for_commit_acks(trans_ids,tid,CHECK_FOR_COMMIT_WITH_NO_WAIT)) {
                        //Primary buffer transactions are complete; release buffers and start processing in primary buffer
                        tid=0; trans_ids[0] = trans_ids[1]=0;
                        //Now release the packets from Tx State Latch Ring
                        transmit_tx_tx_state_latch_rings(0);
                        //Now release the packets from NF
                        transmit_tx_nf_state_latch_rings(0);
                        db_mode=0;
                } else {
                        //transaction on primary buffer still pending;
                        use_db_mode=1;
                }
        }
        if(tid_db){ //if((wait_mode & 2)) {
                if(0 == rsync_wait_for_commit_acks(trans_ids_db,tid_db,CHECK_FOR_COMMIT_WITH_NO_WAIT)) {
                        //Primary buffer transactions are complete; release buffers and start processing in primary buffer
                        tid_db=0; trans_ids_db[0] = trans_ids_db[1]=0;
                        //Now release the packets from Tx State Latch Ring
                        transmit_tx_tx_state_latch_rings(1);
                        //Now release the packets from NF
                        transmit_tx_nf_state_latch_rings(1);

                        db_mode=1;
                } else {
                        if(use_db_mode) return 0;
                }
        }

        //check again for wait_mode
        if(tid && tid_db) { //if((wait_mode) && ((wait_mode & 3)== wait_mode)) {
                //cannot proceeed until trans is completed; so must wait;
                //printf("\n Cannot Proceed for Wait_mode:%d and db_mode:%d\n", wait_mode, db_mode);
                return 0;
        } else {
                db_mode=use_db_mode;
        }

        //First Extract and Parse Tx Port Packets and update TS info in Tx Table
        ret = extract_and_parse_tx_port_packets(db_mode);   //last_buf is tracked with use_db_mode
        //ret = 0; //TEST_HACK to bypass TS Table and NF Shared Memory packet transfer

        //Check and Initiate Remote Sync of Tx State
        if(ret & NEED_REMOTE_TS_TABLE_SYNC) {
                trans_id = rsync_tx_ts_state_to_remote(db_mode);
                if(trans_id >= 0) {
                        if(db_mode) trans_ids_db[tid_db++] = (uint8_t) trans_id;
                        else trans_ids[tid++] = (uint8_t)  trans_id;
                }
        }

        //TODO:communicate to Peer Node (Predecessor/Remote Node) to release the logged packets till TS.
        //How? -- there can be packets in fastchain and some in slow chain. How will you notify? -- rely on best effort (every 1ms) it will refresh.
        //14.88Mpps => 14.88K (~15K, 1.25MB) for 1ms,  and 100ms => (~1500K packets, 125MB) data.

        //check and Initiate remote NF Sync
        if(ret & NEED_REMOTE_NF_STATE_SYNC) {
                int trans_id = rsync_nf_state_to_remote(1);
                if(trans_id>=0) {
                        if(db_mode) trans_ids_db[tid_db++] = (uint8_t)  trans_id;
                        else trans_ids[tid++] = (uint8_t)  trans_id;
                }
        }
        //Note: There is an issue without lock: while updating any new flow comes with new non-determinism then it might be released much earlier.
        return 0;
}
#endif
/* More efficient and complicated Double Buffering Scheme:
 * Use the primary buffer for active transaction and commit each time (ensures keeping latency sensitivity)
 * In the interim, use the double buffer as secondary to efficiently pre-process the packets and update Tx_Ts
 * thereby minimize the overall processing cost and amortize the latency but do no commit/initiate the
 * transaction until we hit the resource limit or get notified of the completion of ongoing transaction.
 * Thus ensure to lower latency than the standard double buffering scheme and potentially
 * achieve same or slightly improved throughput across the NFs.
 */
#ifdef ENABLE_OPTIMAL_DOUBLE_BUFFERING_MODE
static int rsync_start_optimal_db(__attribute__((unused)) void *arg);
static int rsync_start_optimal_db(__attribute__((unused)) void *arg) {
        static uint8_t trans_ids[2] = {0,0}, trans_ids_db[2] = {0,0}; //uint8_t trans_ids[2] = {0,0},tid=0;
        static uint8_t tid=0, to_db=0, tid_db=0;
        static int ret_db = 0;

        if(tid) {
                if(0 == rsync_wait_for_commit_acks(trans_ids,tid,CHECK_FOR_COMMIT_WITH_NO_WAIT)) {
                        //Primary buffer transactions are complete; release buffers and start processing in primary buffer
                        tid=0; trans_ids[0] = trans_ids[1]=0;
                        //Now release the packets from Tx State Latch Ring
                        transmit_tx_tx_state_latch_rings(0);
                        //Now release the packets from NF
                        transmit_tx_nf_state_latch_rings(0);
                }
        }
        if(tid_db) {
                if (0 == rsync_wait_for_commit_acks(trans_ids_db,tid_db,CHECK_FOR_COMMIT_WITH_NO_WAIT)) {
                        //Double buffer transactions are complete; release Double buffers and start processing in Double buffer
                        tid_db=0; trans_ids_db[0] = trans_ids_db[1]=0;
                        //Now release the packets from Tx State Latch Ring
                        transmit_tx_tx_state_latch_rings(1);
                        //Now release the packets from NF
                        transmit_tx_nf_state_latch_rings(1);
                }
        }
        // if still both the regular and DB buffers/transactions are in wait state then cannot process!
        if(tid && tid_db) {
                return 0;
        } else  //added to avoid compiler optimize the code and re-oder it to top most.
        //If Primary buffer is free: But IF packets have been processed in tid_bd but without transactions; i.e if ret_db or to_db is set then commit those first and then
        if(0 == tid) {
                if(ret_db||to_db) {
                        //Check and Initiate Remote Sync of Tx State
                        if(ret_db & NEED_REMOTE_TS_TABLE_SYNC) {
                                int trans_id = rsync_tx_ts_state_to_remote(to_db);
                                if(trans_id>=0) {
                                        trans_ids_db[tid_db++] = trans_id;
                                }
                        }
                        //check and Initiate remote NF Sync
                        if(ret_db & NEED_REMOTE_NF_STATE_SYNC) {
                                int trans_id = rsync_nf_state_to_remote(1);
                                if(trans_id>=0) {
                                        trans_ids_db[tid_db++] = trans_id;
                                }
                        }
                        ret_db=0;
                        to_db=0;
                }
                //start processing Tx port packets
                int ret = extract_and_parse_tx_port_packets(to_db=0); //not sure about last_buf safe to assume not the last buf
                //Check and Initiate Remote Sync of Tx State
                if(ret & NEED_REMOTE_TS_TABLE_SYNC) {
                        int trans_id = rsync_tx_ts_state_to_remote(to_db);
                        if(trans_id>=0) {
                                trans_ids[tid++] = trans_id;
                        }
                }
                //check and Initiate remote NF Sync
                if(ret & NEED_REMOTE_NF_STATE_SYNC) {
                        int trans_id = rsync_nf_state_to_remote(1);
                        if(trans_id>=0) {
                                trans_ids[tid++] = trans_id;
                        }
                }
        } else if(0 == tid_db) { //Else check If Double Buffer is Free
                //start processing Tx port packets: continue to process these packets as much as possible; until we run out of buffer;
                ret_db |= extract_and_parse_tx_port_packets(to_db=1); //last_buf=1
                //check if any of the buffers are full; then also need to move tthis to commit and wiat till earlier transaction completes.
                if((ret_db & TX_TS_LATCH_BUFFER_FULL) || (ret_db & NF_STATE_LATCH_BUFFER_FULL)) {
                        //Check and Initiate Remote Sync of Tx State
                        if(ret_db & NEED_REMOTE_TS_TABLE_SYNC) {
                                int trans_id = rsync_tx_ts_state_to_remote(to_db);
                                if(trans_id>=0) {
                                        trans_ids_db[tid_db++] = trans_id;
                                }
                        }
                        //check and Initiate remote NF Sync
                        if(ret_db & NEED_REMOTE_NF_STATE_SYNC) {
                                int trans_id = rsync_nf_state_to_remote(1);
                                if(trans_id>=0) {
                                        trans_ids_db[tid_db++] = trans_id;
                                }
                        }
                        ret_db=0;
                }
        }
        //Note: There is an issue without lock: while updating any new flow comes with new non-determinism then it might be released much earlier.
        return 0;
}
#endif
inline int rsync_start(__attribute__((unused)) void *arg) {

#ifdef MIMIC_PICO_REP
#ifdef ENABLE_PICO_STATELESS_MODE
        rsync_start_simple_pico(arg);
#else
        //rsync_start_simple_pico(arg); //move to nf_timer_cb (if we need stateless to operate at full rate then enable here and disable in nf_timer_cb)
#endif
        return 0;
#endif

#ifndef ENABLE_RSYNC_WITH_DOUBLE_BUFFERING_MODE
        return rsync_start_simple(arg);

#endif

#ifdef ENABLE_SIMPLE_DOUBLE_BUFFERING_MODE
        return rsync_start_simple_db(arg);
        //return rsync_start_only_db(arg);
#endif

#ifdef ENABLE_RSYNC_MULTI_BUFFERING
        return rsync_start_simple_multi_db(arg);
#endif

#ifdef ENABLE_OPTIMAL_DOUBLE_BUFFERING_MODE
        return rsync_start_optimal_db(arg);
#endif
        return (0);
}

int onvm_print_rsync_stats(unsigned difftime, FILE *fout) {
        static rsync_stats_t prev_state;
        fprintf(fout, "RSYNC\n");
        //fprintf(fout,"-----\n");
        uint8_t i = 0;
        if(difftime==0)difftime=1;
        fprintf(fout, "Total TxTs State SYNC Packets Out:%"PRIu64" (%"PRIu64" pps)   In:%"PRIu64" (%"PRIu64" pps)\n",
                        rsync_stat.tx_state_sync_pkt_counter, (rsync_stat.tx_state_sync_pkt_counter -prev_state.tx_state_sync_pkt_counter)/difftime,
                        rsync_stat.tx_state_sync_in_pkt_counter, (rsync_stat.tx_state_sync_in_pkt_counter -prev_state.tx_state_sync_in_pkt_counter)/difftime);

        for(i=1; i< MAX_NFS; i++) {
                if(!onvm_nf_is_processing(&nfs[i]))continue;
                fprintf(fout, "NF[%d]: Total State SYNC Packets Out:%"PRIu64" (%"PRIu64" pps) In:%"PRIu64" (%"PRIu64" pps) \n",i,
                                        rsync_stat.nf_state_sync_pkt_counter[i],(rsync_stat.nf_state_sync_pkt_counter[i] -prev_state.nf_state_sync_pkt_counter[i])/difftime,
                                        rsync_stat.nf_state_sync_in_pkt_counter[i],(rsync_stat.nf_state_sync_in_pkt_counter[i] -prev_state.nf_state_sync_in_pkt_counter[i])/difftime);
        }
        fprintf(fout, "Total Transactions:: Out:%"PRIu64" (%"PRIu64" pps) In:%"PRIu64" (%"PRIu64" pps) \n",
                        rsync_stat.transactions_out_counter, (rsync_stat.transactions_out_counter -prev_state.transactions_out_counter)/difftime,
                        rsync_stat.transactions_in_counter, (rsync_stat.transactions_in_counter -prev_state.transactions_in_counter)/difftime);
        /*
        for(i=0; i< ports->num_ports; i++) {
                fprintf(fout, "Port:%d, Total Tx Port Packets:%"PRIu64" (%"PRIu64" pps) Drop Packets:%"PRIu64" (%"PRIu64" pps) \n",i,
                                rsync_stat.enq_count_tx_port_ring[i], (rsync_stat.enq_count_tx_port_ring[i] -prev_state.enq_count_tx_port_ring[i])/difftime,
                                rsync_stat.drop_count_tx_port_ring[i], (rsync_stat.drop_count_tx_port_ring[i] -prev_state.drop_count_tx_port_ring[i])/difftime);
                fprintf(fout, "Port:%d, Total Tx State Latch Packets:%"PRIu64" (%"PRIu64" pps) Drop Packets:%"PRIu64" (%"PRIu64" pps) \n",i,
                                rsync_stat.enq_coun_tx_tx_state_latch_ring[i], (rsync_stat.enq_coun_tx_tx_state_latch_ring[i] -prev_state.enq_coun_tx_tx_state_latch_ring[i])/difftime,
                                rsync_stat.drop_count_tx_tx_state_latch_ring[i], (rsync_stat.drop_count_tx_tx_state_latch_ring[i] -prev_state.drop_count_tx_tx_state_latch_ring[i])/difftime);
                fprintf(fout, "Port:%d, Total NF State Latch Packets:%"PRIu64" (%"PRIu64" pps) Drop Packets:%"PRIu64" (%"PRIu64" pps) \n",i,
                                rsync_stat.enq_count_tx_nf_state_latch_ring[i], (rsync_stat.enq_count_tx_nf_state_latch_ring[i] -prev_state.enq_count_tx_nf_state_latch_ring[i])/difftime,
                                rsync_stat.drop_count_tx_nf_state_latch_ring[i], (rsync_stat.drop_count_tx_nf_state_latch_ring[i] -prev_state.drop_count_tx_nf_state_latch_ring[i])/difftime);
#ifdef ENABLE_RSYNC_WITH_DOUBLE_BUFFERING_MODE
#ifdef ENABLE_RSYNC_MULTI_BUFFERING
                uint8_t j=0;
                for(;j<ENABLE_RSYNC_MULTI_BUFFERING;++j) {
                        fprintf(fout, "Port:%d, DB:%d Total Tx State Latch Packets:%"PRIu64" (%"PRIu64" pps) DB Drop Packets:%"PRIu64" (%"PRIu64" pps) \n",i,j,
                                        rsync_stat.enq_coun_tx_tx_state_latch_db_ring[j][i], (rsync_stat.enq_coun_tx_tx_state_latch_db_ring[j][i] -prev_state.enq_coun_tx_tx_state_latch_db_ring[j][i])/difftime,
                                        rsync_stat.drop_count_tx_tx_state_latch_db_ring[j][i], (rsync_stat.drop_count_tx_tx_state_latch_db_ring[j][i] -prev_state.drop_count_tx_tx_state_latch_db_ring[j][i])/difftime);
                        fprintf(fout, "Port:%d,DB:%d Total NF State Latch Packets:%"PRIu64" (%"PRIu64" pps) Drop Packets:%"PRIu64" (%"PRIu64" pps) \n",i,j,
                                        rsync_stat.enq_count_tx_nf_state_latch_db_ring[j][i], (rsync_stat.enq_count_tx_nf_state_latch_db_ring[j][i] -prev_state.enq_count_tx_nf_state_latch_db_ring[j][i])/difftime,
                                        rsync_stat.drop_count_tx_nf_state_latch_db_ring[j][i], (rsync_stat.drop_count_tx_nf_state_latch_db_ring[j][i] -prev_state.drop_count_tx_nf_state_latch_db_ring[j][i])/difftime);
                }
#else
                fprintf(fout, "Port:%d, DB Total Tx State Latch Packets:%"PRIu64" (%"PRIu64" pps) DB Drop Packets:%"PRIu64" (%"PRIu64" pps) \n",i,
                                rsync_stat.enq_coun_tx_tx_state_latch_db_ring[i], (rsync_stat.enq_coun_tx_tx_state_latch_db_ring[i] -prev_state.enq_coun_tx_tx_state_latch_db_ring[i])/difftime,
                                rsync_stat.drop_count_tx_tx_state_latch_db_ring[i], (rsync_stat.drop_count_tx_tx_state_latch_db_ring[i] -prev_state.drop_count_tx_tx_state_latch_db_ring[i])/difftime);
                fprintf(fout, "Port:%d, Total NF State Latch Packets:%"PRIu64" (%"PRIu64" pps) Drop Packets:%"PRIu64" (%"PRIu64" pps) \n",i,
                                rsync_stat.enq_count_tx_nf_state_latch_db_ring[i], (rsync_stat.enq_count_tx_nf_state_latch_db_ring[i] -prev_state.enq_count_tx_nf_state_latch_db_ring[i])/difftime,
                                rsync_stat.drop_count_tx_nf_state_latch_db_ring[i], (rsync_stat.drop_count_tx_nf_state_latch_db_ring[i] -prev_state.drop_count_tx_nf_state_latch_db_ring[i])/difftime);
#endif
#endif
        } */
        prev_state = rsync_stat;
        return 0;
}

int notify_replay_mode(uint8_t mode) {
        state_tx_meta_t meta = {.state_type= MSG_END_OF_REPLAY, .nf_or_svc_id=0, .start_offset=0, /*.reserved=nf_mgr_id, */.trans_id=0};
        if(mode) {
                meta.state_type = MSG_START_OF_REPLAY;
        }
        struct rte_mbuf*out_pkt = craft_state_update_packet(SECONDARY_OUT_PORT,meta, NULL,0);
        return send_packets_out(SECONDARY_OUT_PORT, RSYNC_TX_PORT_QUEUE_ID_1, &out_pkt, 1);
}

int
rsync_main(__attribute__((unused)) void *arg) {

        if(NULL == pktmbuf_pool) {
                /*
                pktmbuf_pool = rte_mempool_lookup(PKTMBUF_POOL_NAME);
                if(NULL == pktmbuf_pool) {
                        return -1;
                }
                */
                rte_exit(EXIT_FAILURE, "rsync_main:Failed to get PktMbufPool\n");
        }

        //Initalize the Timer for performing periodic NF State Snapshotting
        initialize_rsync_timers();

        //Initalize the Tx Timestamp Table for all flow entries
        initialize_tx_ts_table();

        while (true) {
#ifndef RSYNC_USE_DPDK_TIMER
                //start Tx port Packet Processing
                rsync_start(arg); //moved to timer thread.
#endif
                check_and_clear_elapsed_transactions();

                //check for timer Expiry
                rte_timer_manage();

        }
        return 0;
}
#endif //ENABLE_REMOTE_SYNC_WITH_TX_LATCH
