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
 *            2016-2017 Hewlett Packard Enterprise Development LP
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
                                   main.c

     File containing the main function of the manager and all its worker
     threads.

******************************************************************************/


#include <signal.h>

#include "onvm_mgr.h"
#include "onvm_stats.h"
#include "onvm_pkt.h"
#include "onvm_nf.h"
#include "onvm_wakemgr.h"
#include "onvm_special_nf0.h"
#include "onvm_rsync.h"


#ifdef ENABLE_BFD
#include "onvm_bfd.h"
onvm_bfd_init_config_t bfd_config;
static int bfd_handle_callback(uint8_t port, uint8_t status);
#endif

#ifdef ENABLE_REMOTE_SYNC_WITH_TX_LATCH
int node_role = PRIMARY_NODE; //0= PREDECESSOR; 1=PRIMARY; 2=SECONDARY
//int node_role = SECONDARY_NODE;
//int node_role = PREDECESSOR_NODE;
#endif
/****************************Internal Declarations****************************/

#define MAX_SHUTDOWN_ITERS 10

// True as long as the main thread loop should keep running
static uint8_t main_keep_running = 1;

// We'll want to shut down the TX/RX threads second so that we don't
// race the stats display to be able to print, so keep this varable separate
static uint8_t worker_keep_running = 1;

/****************************Internal Declarations****************************/
#ifdef TEST_INLINE_FUNCTION_CALL
int process_nf_function_inline(struct rte_mbuf* pkt, struct onvm_pkt_meta* meta);
int process_nf_function_inline(__attribute__((unused)) struct rte_mbuf* pkt, __attribute__((unused)) struct onvm_pkt_meta* meta) {
        return 0;
}
nf_pkt_handler nf_process_packet = process_nf_function_inline;
#endif

#ifdef ENABLE_ZOOKEEPER
#include "onvm_zookeeper.h"
#include "onvm_zk_common.h"
#else
uint8_t remote_eth_addr[6];
struct ether_addr remote_eth_addr_struct;
#endif

typedef struct thread_core_map_t {
        unsigned rx_th_core[ONVM_NUM_RX_THREADS];
        unsigned tx_t_core[8];
#ifdef INTERRUPT_SEM
        unsigned wk_th_core[ONVM_NUM_WAKEUP_THREADS];
#endif
        unsigned mn_th_core;
}thread_core_map_t;
static thread_core_map_t thread_core_map;

#ifdef ENABLE_USE_RTE_TIMER_MODE_FOR_MAIN_THREAD

#define NF_STATUS_CHECK_PERIOD_IN_MS    (500)       // (500) 500ms or 0.5seconds
#define NF_STATUS_CHECK_PERIOD_IN_US    (50)       // use high precision 100us to ensure that we do it quickly to recover/restore
#define DISPLAY_STATS_PERIOD_IN_MS      (1000)      // 1000ms or Every second
#define NF_LOAD_EVAL_PERIOD_IN_MS       (1)         // 1ms
#define USLEEP_INTERVAL_IN_US           (25)        // 50 micro seconds (even if set to 50, best precision with nanosleep  >100micro)
//#define ARBITER_PERIOD_IN_US            (100)       // 250 micro seconds or 100 micro seconds
//Note: Running arbiter at 100micro to 250 micro seconds is fine provided we have the buffers available as:
//RTT (measured with bridge and 1 basic NF) =0.2ms B=10Gbps => B*delay ( 2*RTT*Bw) = 2*200*10^-6 * 10*10^9 = 4Mb = 0.5MB
//Assuming avg pkt size of 1000 bytes => 500 *10^3/1000 = 500 packets. (~512 packets)
//For smaller pkt size of 64 bytes => 500*10^3/64 = 7812 packets. (~8K packets)

struct rte_timer display_stats_timer;   //Timer to periodically Display the statistics  (1 second)
struct rte_timer nf_status_check_timer; //Timer to periodically check new NFs registered or old NFs de-registerd   (0.5 second)
struct rte_timer nf_load_eval_timer;    //Timer to periodically evaluate the NF Load characteristics    (1ms)
struct rte_timer main_arbiter_timer;    //Timer to periodically run the Arbiter   (100us to at-most 250 micro seconds)

int initialize_rx_timers(int index, void *data);
int initialize_tx_timers(int index, void *data);
int initialize_master_timers(void);

static void display_stats_timer_cb(__attribute__((unused)) struct rte_timer *ptr_timer,
        __attribute__((unused)) void *ptr_data);
static void nf_status_check_timer_cb(__attribute__((unused)) struct rte_timer *ptr_timer,
        __attribute__((unused)) void *ptr_data);
static void nf_load_stats_timer_cb(__attribute__((unused)) struct rte_timer *ptr_timer,
        __attribute__((unused)) void *ptr_data);
static void arbiter_timer_cb(__attribute__((unused)) struct rte_timer *ptr_timer,
        __attribute__((unused)) void *ptr_data);

static void
display_stats_timer_cb(__attribute__((unused)) struct rte_timer *ptr_timer,
        __attribute__((unused)) void *ptr_data) {

        static const unsigned diff_time_sec = (unsigned) (DISPLAY_STATS_PERIOD_IN_MS/1000);
        onvm_stats_display_all(diff_time_sec, global_verbosity_level);
        return;
}

static void
nf_status_check_timer_cb(__attribute__((unused)) struct rte_timer *ptr_timer,
        __attribute__((unused)) void *ptr_data) {

        onvm_nf_check_status();
        return;
}

static void
nf_load_stats_timer_cb(__attribute__((unused)) struct rte_timer *ptr_timer,
        __attribute__((unused)) void *ptr_data) {
        static nf_stats_time_info_t nf_stat_time;
        if(nf_stat_time.in_read == 0) {
                if( onvm_util_get_cur_time(&nf_stat_time.prev_time) == 0) {
                        nf_stat_time.in_read = 1;
                }
                return ;
        }

        if(0 == onvm_util_get_cur_time(&nf_stat_time.cur_time)) {
                unsigned long difftime_us = onvm_util_get_difftime_us(&nf_stat_time.prev_time, &nf_stat_time.cur_time);
                if(difftime_us) {
                        onvm_nf_stats_update(difftime_us);
                }
                nf_stat_time.prev_time = nf_stat_time.cur_time;
        }
        return;
}

static void
arbiter_timer_cb(__attribute__((unused)) struct rte_timer *ptr_timer,
        __attribute__((unused)) void *ptr_data) {
#ifdef INTERRUPT_SEM
        check_and_enqueue_or_dequeue_nfs_from_bottleneck_watch_list();  // this sets time bound of arbiter timer interval
        compute_and_order_nf_wake_priority();
        handle_wakeup(NULL);

#endif
        //printf("\n Inside arbiter_timer_cb() %"PRIu64", on core [%d] \n", rte_rdtsc_precise(), rte_lcore_id());
        return;
}

int
initialize_master_timers(void) {

        rte_timer_init(&nf_status_check_timer);
        rte_timer_init(&display_stats_timer);
        rte_timer_init(&nf_load_eval_timer);
        rte_timer_init(&main_arbiter_timer);

        uint64_t ticks = 0;

        //ticks = ((uint64_t)NF_STATUS_CHECK_PERIOD_IN_MS *(rte_get_timer_hz()/1000));
        ticks = ((uint64_t)NF_STATUS_CHECK_PERIOD_IN_US *(rte_get_timer_hz()/1000000));
        rte_timer_reset_sync(&nf_status_check_timer,
                ticks,
                PERIODICAL,
                rte_lcore_id(), //timer_core
                &nf_status_check_timer_cb, NULL
                );

        ticks = ((uint64_t)DISPLAY_STATS_PERIOD_IN_MS *(rte_get_timer_hz()/1000));
        rte_timer_reset_sync(&display_stats_timer,
                ticks,
                PERIODICAL,
                rte_lcore_id(), //timer_core
                &display_stats_timer_cb, NULL
                );

        ticks = ((uint64_t)NF_LOAD_EVAL_PERIOD_IN_MS *(rte_get_timer_hz()/1000));
        rte_timer_reset_sync(&nf_load_eval_timer,
                ticks,
                PERIODICAL,
                rte_lcore_id(), //timer_core
                &nf_load_stats_timer_cb, NULL
                );
        //bypassing the call for now..
        //rte_timer_init(&nf_load_eval_timer);

        if( 0 == ONVM_NUM_WAKEUP_THREADS) {
                ticks = ((uint64_t)ARBITER_PERIOD_IN_US *(rte_get_timer_hz()/1000000));
                rte_timer_reset_sync(&main_arbiter_timer,
                        ticks,
                        PERIODICAL,
                        rte_lcore_id(),
                        &arbiter_timer_cb, NULL
                        );
                //Note: This call effectively nullifies the timer
                //rte_timer_init(&main_arbiter_timer);
        }
        return 0;
}
#endif //ENABLE_USE_RTE_TIMER_MODE_FOR_MAIN_THREAD
/*******************************Worker threads********************************/
#define MAIN_THREAD_POLL
#ifndef MAIN_THREAD_POLL
#define MAIN_THREAD_SLEEP_INTERVAL_NS  (1000)
#endif
/*
 * Stats thread periodically prints per-port and per-NF stats.
 */
static void
master_thread_main(void) {
        const unsigned sleeptime = 1;

        RTE_LOG(INFO, APP, "Core %d: Running master thread\n", rte_lcore_id());

#ifdef ONVM_ENABLE_SPEACILA_NF
        start_special_nf0();
#endif

        /* Longer initial pause so above printf is seen */
        sleep(sleeptime * 3);

        for(;main_keep_running;) {

#ifdef ENABLE_USE_RTE_TIMER_MODE_FOR_MAIN_THREAD
                if(initialize_master_timers() == 0) {
#ifndef MAIN_THREAD_POLL
                        struct timespec req = {0,MAIN_THREAD_SLEEP_INTERVAL_NS}, res = {0,0};
#endif
                        do {
                                rte_timer_manage();
#ifdef ONVM_ENABLE_SPEACILA_NF
                                (void)process_special_nf0_rx_packets();
#endif
#ifndef MAIN_THREAD_POLL
                                nanosleep(&req, &res);
#endif
                        } while(main_keep_running);
                        //while (nanosleep(&req, &res) == 0) { //while (usleep(USLEEP_INTERVAL_IN_US) == 0) { // while(1) {}
                } else
#endif //ENABLE_USE_RTE_TIMER_MODE_FOR_MAIN_THREAD
                /* Loop forever: sleep always returns 0 or <= param */
                do {
                        onvm_nf_check_status();
                        onvm_stats_display_all(sleeptime, global_verbosity_level);
#ifdef ONVM_ENABLE_SPEACILA_NF
                                (void)process_special_nf0_rx_packets();
#endif
                }while(main_keep_running);
                //while (sleep(sleeptime) <= sleeptime) {}
        }

        /* Close out file references and things */
        onvm_stats_cleanup();
        RTE_LOG(INFO, APP, "Core %d: Initiating shutdown sequence\n", rte_lcore_id());

#ifdef RTE_LIBRTE_PDUMP
        rte_pdump_uninit();
#endif
        /* Stop all RX and TX threads */
        worker_keep_running = 0;

        /* Tell all NFs to stop */
        int i = 0;
        for (i = 1; i < MAX_NFS; i++) {
                if (nfs[i].info == NULL) {
                        continue;
                }
                RTE_LOG(INFO, APP, "Core %d: Notifying NF %"PRIu16" to shut down\n", rte_lcore_id(), i);
                onvm_nf_send_msg(i, MSG_STOP, MSG_MODE_ASYNCHRONOUS, NULL);
        }
        /* Wait to process all exits */
        for (i=0; i < MAX_SHUTDOWN_ITERS && num_nfs > 0; i++) {
                onvm_nf_check_status();
                RTE_LOG(INFO, APP, "Core %d: Waiting for %"PRIu16" NFs to exit\n", rte_lcore_id(), num_nfs);
                sleep(1);
        }
        if (num_nfs > 0) {
                RTE_LOG(INFO, APP, "Core %d: Up to %"PRIu16" NFs may still be running and must be killed manually\n", rte_lcore_id(), num_nfs);
        }

#ifdef USE_SEMAPHORE
        for (i = 1; i < MAX_NFS; i++) {
                sem_close(nfs[i].mutex);
                sem_unlink(nfs[i].sem_name);
        }
#endif
        RTE_LOG(INFO, APP, "Core %d: Master thread done\n", rte_lcore_id());
}

/*
 * Function to receive packets from the NIC
 * and distribute them to the default service
 */
void initiate_node_failover(void);
void replay_and_terminate_failover(void);
static int
rx_thread_main(void *arg) {
        uint16_t i, rx_count;
        struct rte_mbuf *pkts[PACKET_READ_SIZE];
        struct thread_info *rx = (struct thread_info*)arg;

        RTE_LOG(INFO,
                APP,
                "Core %d: Running RX thread for RX queue %d\n",
                rte_lcore_id(),
                rx->queue_id);

        for (;worker_keep_running;) {
                /* Read ports */
                for (i = 0; i < ports->num_ports; i++) {
                        rx_count = rte_eth_rx_burst(ports->id[i], rx->queue_id, \
                                        pkts, PACKET_READ_SIZE);

#ifdef ENABLE_REMOTE_SYNC_WITH_TX_LATCH
                        if(unlikely(PREDECESSOR_NODE == node_role) && unlikely(replay_mode)) {
                                //do replay and then continue with
                                replay_and_terminate_failover();
                        }
#endif
                        /* Now process the NIC packets read */
                        if (likely(rx_count > 0)) {
                                ports->rx_stats.rx[ports->id[i]] += rx_count;
#ifdef ENABLE_PACKET_TIMESTAMPING
                                onvm_util_mark_timestamp_on_RX_packets(pkts, rx_count);
#endif
#ifdef ENABLE_PCAP_CAPTURE_ON_INPUT
                                onvm_util_log_packets(ports->id[i],pkts,NULL,rx_count);
#endif
#ifdef MIMIC_PICO_REP
                                if(rx_halt) {
                                        onvm_pkt_drop_batch(pkts, rx_count);
                                        //printf("\n Dropping batch of packets!\n");
                                        continue;
                                }
#endif
                                if(unlikely(i != pkts[0]->port)) { printf("\n got packet with Incorrect i=%d Port mapping! Pkt=%d, ",i, pkts[0]->port);}
                                // If there is no running NF, we drop all the packets of the batch.
                                if (likely(num_nfs)) {
                                        onvm_pkt_process_rx_batch(rx, pkts, rx_count);
                                } else {
                                        (void)onv_pkt_send_to_special_nf0(rx, pkts, rx_count); //onvm_pkt_drop_batch(pkts, rx_count);
                                }
                        }
                }
        }
        return 0;
}

#define PACKET_READ_SIZE_TX ((uint16_t)(PACKET_READ_SIZE*4))
static int
tx_thread_main(void *arg) {
        struct onvm_nf *cl;
        unsigned i, tx_count;
        struct rte_mbuf *pkts[PACKET_READ_SIZE_TX];
        struct thread_info* tx = (struct thread_info*)arg;

        RTE_LOG(INFO,
               APP,
               "Core %d: Running TX thread for NFs %d to %d\n",
               rte_lcore_id(),
               tx->first_cl,
               tx->last_cl-1);

        for (;worker_keep_running;) {
                /* Read packets from the client's tx queue and process them as needed */
                for (i = tx->first_cl; i < tx->last_cl; i++) {
                        cl = &nfs[i];

                        //if (unlikely(!onvm_nf_is_valid(cl))||(onvm_nf_is_paused(cl))) continue;
                        //if(unlikely(NULL == cl->info)) continue;
                        //else if(unlikely(onvm_nf_is_paused(cl))) continue;

                        /* Do not pull the packets from the NF if it is not active */
                        if(unlikely(!onvm_nf_is_processing(cl))) continue;

                        /* try dequeuing max possible packets first, if that fails, get the
                         * most we can. Loop body should only execute once, maximum
                        while (tx_count > 0 &&
                                unlikely(rte_ring_dequeue_bulk(cl->tx_q, (void **) pkts, tx_count) != 0)) {
                                tx_count = (uint16_t)RTE_MIN(rte_ring_count(cl->tx_q),
                                                PACKET_READ_SIZE);
                        }
                        */
                        tx_count = rte_ring_dequeue_burst(cl->tx_q, (void **) pkts, PACKET_READ_SIZE_TX,NULL); //rte_ring_dequeue_bulk(cl->tx_q, (void **) pkts, PACKET_READ_SIZE_TX);

                        /* Now process the Client packets read */
                        if (likely(tx_count > 0)) {

#ifdef ENABLE_NF_BACKPRESSURE
#ifdef ENABLE_NF_BASED_BKPR_MARKING
                                onvm_check_and_reset_back_pressure_v2(cl);
                                //Note: TODO: With _v2, ECN_CE currently doesnt work; Need to move this marking also in wake_mgr_context.
#else
                                onvm_check_and_reset_back_pressure(pkts, tx_count, cl);
#endif
                                //Note: TODO: Must change ECN_CE to packet independent marking scheme or change the approach.
#ifdef ENABLE_ECN_CE
                                if(cl->info->ht2_q.ewma_avg >= CLIENT_QUEUE_RING_ECN_MARK_SIZE) { //if(cl->info->ht2_q.ewma_avg >= CLIENT_QUEUE_RING_WATER_MARK_SIZE) {
                                        onvm_detect_and_set_ecn_ce(pkts, tx_count, cl);
                                }
#endif //ENABLE_ECN_CE

#endif

                                onvm_pkt_process_tx_batch(tx, pkts, tx_count, cl);
                                //RTE_LOG(INFO,APP,"Core %d: processing %d TX packets for NF: %d \n", rte_lcore_id(),tx_count, i);
                        }
                        else continue;
                }

                /* Send a burst to every port */
                onvm_pkt_flush_all_ports(tx);

                /* Send a burst to every NF */
                onvm_pkt_flush_all_nfs(tx);
        }

        return 0;
}

static inline int create_rx_threads(unsigned *pcur_lcore, unsigned rx_lcores) {
        unsigned i = 0;
        for (i = 0; i < rx_lcores; i++) {
                struct thread_info *rx = calloc(1, sizeof(struct thread_info));
                rx->queue_id = i;
                rx->port_tx_buf = NULL;
                rx->nf_rx_buf = calloc(MAX_NFS, sizeof(struct packet_buf));
#ifdef ENABLE_CHAIN_BYPASS_RSYNC_ISOLATION
                rx->port_tx_direct_buf = NULL;   //use this buffer to directly output bypassing the rsync if enabled!
#endif
                *pcur_lcore = rte_get_next_lcore(*pcur_lcore, 1, 1);
                if (rte_eal_remote_launch(rx_thread_main, (void *)rx, *pcur_lcore) == -EBUSY) {
                        RTE_LOG(ERR, APP, "Core %d is already busy, can't use for RX queue id %d\n", *pcur_lcore, rx->queue_id);
                        return -1;
                }
                thread_core_map.rx_th_core[i]=*pcur_lcore;
        }
        return 0;
}
static inline int create_tx_threads(unsigned *pcur_lcore, unsigned tx_lcores, unsigned num_nfs) {
        unsigned i = 0;
        unsigned clients_per_tx = ceil((float)num_nfs/tx_lcores);
        for (; i < tx_lcores; i++) {
                struct thread_info *tx = calloc(1, sizeof(struct thread_info));
                tx->queue_id = i;
                tx->port_tx_buf = calloc(RTE_MAX_ETHPORTS, sizeof(struct packet_buf));
                tx->nf_rx_buf = calloc(MAX_NFS, sizeof(struct packet_buf));
#ifdef ENABLE_CHAIN_BYPASS_RSYNC_ISOLATION
                tx->port_tx_direct_buf = calloc(RTE_MAX_ETHPORTS, sizeof(struct packet_buf));   //use this buffer to directly output bypassing the rsync if enabled!
#endif

                tx->first_cl = RTE_MIN(i * clients_per_tx, num_nfs);       //(inclusive) read from NF[0] to NF[clients_per_tx-1]
                tx->last_cl = RTE_MIN((i+1) * clients_per_tx, num_nfs);

                //Dedicate 1 Tx for NF0 and next Tx for all NFs
                //if(i==0) tx->first_cl = 0;tx->last_cl=1;
                //else tx->first_cl = 1;tx->last_cl=temp_num_clients;

                *pcur_lcore = rte_get_next_lcore(*pcur_lcore, 1, 1);
                if (rte_eal_remote_launch(tx_thread_main, (void*)tx,  *pcur_lcore) == -EBUSY) {
                        RTE_LOG(ERR,APP, "Core %d is already busy, can't use for client %d TX\n", *pcur_lcore,tx->first_cl);
                        return -1;
                }
                thread_core_map.tx_t_core[i]=*pcur_lcore;
                RTE_LOG(INFO, APP, "Tx thread [%d] on core [%d] cores for [%d:%d]\n", i+1, *pcur_lcore, tx->first_cl, tx->last_cl);
        }
        return 0;
}
#ifdef INTERRUPT_SEM
static inline int create_wakeup_threads(unsigned *pcur_lcore, unsigned wakeup_lcores, unsigned num_nfs) {
        if(wakeup_lcores) {
                int clients_per_wakethread = ceil(num_nfs / wakeup_lcores);
                wakeup_infos = (struct wakeup_info *)calloc(wakeup_lcores, sizeof(struct wakeup_info));
                if (wakeup_infos == NULL) {
                        printf("can not alloc space for wakeup_info\n");
                        exit(1);
                }
                unsigned i = 0;
                for (; i < wakeup_lcores; i++) {
                        wakeup_infos[i].first_client = RTE_MIN(i * clients_per_wakethread + 1, num_nfs);
                        wakeup_infos[i].last_client = RTE_MIN((i+1) * clients_per_wakethread + 1, num_nfs);
                        *pcur_lcore = rte_get_next_lcore(*pcur_lcore, 1, 1);

                        thread_core_map.wk_th_core[i]=*pcur_lcore;
                        //initialize_wake_core_timers(i, (void*)&wakeup_infos); //better to do it inside the registred thread callback function.

                        rte_eal_remote_launch(wakemgr_main, (void*)&wakeup_infos[i], *pcur_lcore);
                        //printf("wakeup lcore_id=%d, first_client=%d, last_client=%d\n", cur_lcore, wakeup_infos[i].first_client, wakeup_infos[i].last_client);
                        RTE_LOG(INFO, APP, "Core %d: Running wakeup thread, first_client=%d, last_client=%d\n", *pcur_lcore, wakeup_infos[i].first_client, wakeup_infos[i].last_client);

                }
        }
        return 0;
}
#endif
#ifdef ENABLE_REMOTE_SYNC_WITH_TX_LATCH
static inline int create_rsync_threads(unsigned *pcur_lcore, unsigned rsync_lcores) {
        if(rsync_lcores) {
                unsigned i = 0;
                for (; i < rsync_lcores; i++) {
                        *pcur_lcore = rte_get_next_lcore(*pcur_lcore, 1, 1);
                        rte_eal_remote_launch(rsync_main, NULL, *pcur_lcore);
                        RTE_LOG(INFO, APP, "Core %d: Running rsync thread \n", *pcur_lcore);
                }
        }
        return 0;
}
#endif
/*******************************Main function*********************************/
int
main(int argc, char *argv[]) {
        unsigned cur_lcore, rx_lcores, tx_lcores;
        //unsigned clients_per_tx;
        unsigned temp_num_clients;
        //unsigned i;

        /* initialise the system */
#ifdef INTERRUPT_SEM
        unsigned wakeup_lcores;
#ifdef ENABLE_REMOTE_SYNC_WITH_TX_LATCH
        unsigned rsync_lcores;
#endif
#endif

        register_signal_handler();

        /* Reserve ID 0 for internal manager things */
        next_instance_id = 1;
        if (init(argc, argv) < 0 )
                return -1;
        RTE_LOG(INFO, APP, "Finished Process Init.\n");

        /* clear statistics */
        onvm_stats_clear_all_clients();

        /* Reserve n cores for: 1 main thread, ONVM_NUM_RX_THREADS for Rx, ONVM_NUM_WAKEUP_THREADS for wakeup and remaining for Tx */
        if(rte_lcore_count() < (2+ONVM_NUM_RX_THREADS+ONVM_NUM_WAKEUP_THREADS+ONVM_NUM_RSYNC_THREADS)) {
                rte_exit(EXIT_FAILURE, "Need Minimum of [%d] cores! \n",(2+ONVM_NUM_RX_THREADS+ONVM_NUM_WAKEUP_THREADS));
        }
        cur_lcore = rte_lcore_id();
        rx_lcores = ONVM_NUM_RX_THREADS;
#ifdef INTERRUPT_SEM
        wakeup_lcores = ONVM_NUM_WAKEUP_THREADS;
#ifdef ENABLE_REMOTE_SYNC_WITH_TX_LATCH
        rsync_lcores = ONVM_NUM_RSYNC_THREADS;
#endif
        tx_lcores = MIN((rte_lcore_count() - rx_lcores - ONVM_NUM_WAKEUP_THREADS -ONVM_NUM_RSYNC_THREADS - 1), ONVM_NUM_TX_THREADS); //tx_lcores -= wakeup_lcores; //tx_lcores= (tx_lcores>2)?(2):(tx_lcores);
        //tx_lcores=1;
#else
        tx_lcores = MIN((rte_lcore_count() - rx_lcores - 1), ONVM_NUM_TX_THREADS);
#endif

        /* Offset cur_lcore to start assigning TX cores */
        //cur_lcore += (rx_lcores-1);

        RTE_LOG(INFO, APP, "%d cores available in total\n", rte_lcore_count());
        RTE_LOG(INFO, APP, "%d cores available for handling manager RX queues\n", rx_lcores);
        RTE_LOG(INFO, APP, "%d cores available for handling TX queues\n", tx_lcores);
#ifdef INTERRUPT_SEM
        RTE_LOG(INFO, APP, "%d cores available for handling wakeup\n", wakeup_lcores);        
#endif
        RTE_LOG(INFO, APP, "%d cores available for handling stats(main)\n", 1);

        /* Evenly assign NFs to TX threads */

        /*
         * If num nfs is zero, then we are running in dynamic NF mode.
         * We do not have a way to tell the total number of NFs running so
         * we have to calculate clients_per_tx using MAX_NFS then.
         * We want to distribute the number of running NFs across available
         * TX threads
         */
        if (num_nfs == 0) {
                temp_num_clients = (unsigned)MAX_NFS;
        } else {
                temp_num_clients = (unsigned)num_nfs;
                num_nfs = 0;
        }

        //printf("$$$$$$$$$: MAX_ETH_PORTS=%d $$$$$$$$\n", RTE_MAX_ETHPORTS)RTE_MAX_ETHPORTS=32;
        //num_nfs = temp_num_clients;
        create_tx_threads(&cur_lcore, tx_lcores, temp_num_clients);
       
        /* Launch RX thread main function for each RX queue on cores */
        create_rx_threads(&cur_lcore, rx_lcores);

#ifdef ENABLE_REMOTE_SYNC_WITH_TX_LATCH
        create_rsync_threads(&cur_lcore, rsync_lcores);
#endif

#ifdef INTERRUPT_SEM
        create_wakeup_threads(&cur_lcore, wakeup_lcores, temp_num_clients);
#endif

#ifdef ENABLE_BFD
        bfd_config.bfd_identifier = nf_mgr_id;
        bfd_config.num_ports = ports->num_ports;
        bfd_config.cb_func = bfd_handle_callback;
        int i = 0;
        for (i = 0; i < ports->num_ports; i++) {
                bfd_config.session_mode[i] = BFD_SESSION_MODE_ACTIVE;
        }
        onvm_bfd_init(&bfd_config);
#endif

#ifdef ENABLE_VXLAN
        uint16_t nic_port = DISTRIBUTED_NIC_PORT;
        printf("Distributed Mode: nic_port: %u mac addr: %s\n", ports->id[nic_port],
        onvm_stats_print_MAC(ports->id[nic_port]));
        rte_eth_macaddr_get(nic_port, &ports->mac[nic_port]);
#ifdef ENABLE_ZOOKEEPER
        // Do Zookeeper init
        onvm_zk_connect(ZK_CONNECT_BLOCKING);
        RTE_LOG(INFO, APP, "Connected to ZooKeeper, id %" PRId64 "\n", onvm_zk_client_id());

        const char *port_mac = onvm_stats_print_MAC(ports->id[nic_port]);
        int ret = onvm_zk_init(port_mac);
        if (ret != ZOK) {
                RTE_LOG(ERR, APP, "Error doing zookeeper init, bailing. %s\n", zk_status_to_string(ret));
                return -1;
        }
#else
        printf("Zookeeper is disabled, use static setting\n");
        ether_addr_copy((struct ether_addr *)&remote_eth_addr, &remote_eth_addr_struct);
        onvm_print_ethaddr("remote addr:", &remote_eth_addr_struct);
#endif
#endif


        /* Master thread handles statistics and NF management */
        thread_core_map.mn_th_core=rte_lcore_id();
        master_thread_main();

#ifdef ENABLE_ZOOKEEPER
        onvm_zk_disconnect();
#endif
        return 0;
}

/*******************************Helper functions********************************/
#define ENABLE_REPLAY_LATENCY_PROFILER
#ifdef ENABLE_REPLAY_LATENCY_PROFILER
static onvm_interval_timer_t ts;
#endif
void replay_and_terminate_failover(void) {
#ifdef ENABLE_PCAP_CAPTURE
        onvm_util_replay_all_packets(PRIMARY_OUT_PORT, 1000); //SECONDARY_OUT_PORT
#endif

#ifdef ENABLE_REMOTE_SYNC_WITH_TX_LATCH
        notify_replay_mode(replay_mode=0);
#endif
#ifdef ENABLE_REPLAY_LATENCY_PROFILER
        printf("\n TERMINATE REPLAY(START-->-END_OF_REPLAY): %li ns\n", onvm_util_get_elapsed_time(&ts));
#endif
}
void initiate_node_failover(void) {
#ifdef ENABLE_REMOTE_SYNC_WITH_TX_LATCH

#ifdef ENABLE_REPLAY_LATENCY_PROFILER
        onvm_util_get_start_time(&ts);
        printf("\n Initiate Replay started at %li rtdsc_cycles\n", onvm_util_get_current_cpu_cycles());
#endif
        //turn on replay_mode=1;
        notify_replay_mode(replay_mode=1);
        //rest of sequence dealt outside
        //i1.pcap_replay();
        //2.notify_replay_mode(replay_mode=0);
#endif
        //either do in the main thread context of rx thread context: better rx_thread as rx_thread must pause sending any buffers during replay
        //replay_and_terminate_failover();

}
#ifdef ENABLE_BFD
static int bfd_handle_callback(uint8_t port, uint8_t status) {
        //check for valid port and BFD_StateValue
        printf("\n BFD::Port[%d] moved to status[%d]\n",port,status);
        if(port < ports->num_ports) {
                if(status == BFD_STATUS_REMOTE_DOWN || status == BFD_STATUS_LOCAL_DOWN) {
                        ports->down_status[port]=status;
#ifdef ENABLE_REMOTE_SYNC_WITH_TX_LATCH
                        if(unlikely(PREDECESSOR_NODE == node_role)  && unlikely(PRIMARY_OUT_PORT == port)){
                                initiate_node_failover();
                        }
#endif
                        return 1;
                }
                /* if(status == AdminDown || status == Down) {
                        return 1;
                }*/
        }
        return 0;
}
#endif
static void signal_handler(int sig,  __attribute__((unused)) siginfo_t *info,  __attribute__((unused)) void *secret) {
        //2 means terminal interrupt, 3 means terminal quit, 9 means kill and 15 means termination
        printf("Got Signal [%d]\n", sig);
        if(info) {
                printf("[signo: %d,errno: %d,code: %d]\n", info->si_signo, info->si_errno, info->si_code);
        }
        if(sig == SIGWINCH) return;

        if(SIGINT == sig || SIGTERM == sig)
                main_keep_running = 0;

        else if(sig == SIGFPE) return;
        else signal(sig,SIG_DFL);
        //exit(101);
}

void
register_signal_handler(void) {
        unsigned i;
        struct sigaction act;
        memset(&act, 0, sizeof(act));
        sigemptyset(&act.sa_mask);
        act.sa_flags = SA_SIGINFO;
        act.sa_handler = (void *)signal_handler;

        for (i = 1; i < 31; i++) {
                if(i == SIGWINCH)continue;
                if(i == SIGSEGV)continue;
                //if(i==SIGFPE)continue;
                sigaction(i, &act, 0);
        }
}


