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
                                 onvm_wakemgr.c

            This file contains all functions related to NF wakeup management.

******************************************************************************/

#include "onvm_mgr.h"
#include "onvm_pkt.h"
#include "onvm_nf.h"
#include "onvm_wakemgr.h"


#ifdef INTERRUPT_SEM

#include <signal.h>
#include <rte_timer.h>
//#define USE_NF_WAKE_THRESHOLD
#ifdef USE_NF_WAKE_THRESHOLD
unsigned nfs_wakethr[MAX_NFS] = {[0 ... MAX_NFS-1] = 1};
#endif

struct wakeup_info *wakeup_infos;

/***********************Internal Functions************************************/
static inline int
whether_wakeup_client(int instance_id);
static inline void handle_wakeup_ordered(__attribute__((unused))struct wakeup_info *wakeup_info);
static inline void handle_wakeup_old(struct wakeup_info *wakeup_info);
static inline void
wakeup_client(int instance_id, struct wakeup_info *wakeup_info);
static inline int
wakeup_client_internal(int instance_id);

#define WAKE_INTERVAL_IN_US     (ARBITER_PERIOD_IN_US)      //100 micro seconds
#define USLEEP_INTERVAL         (10)                        //50 micro seconds
//Note: sleep of 50us and wake_interval of 100us reduces CPU utilization from 100 to 0.3
//Ideal: Get rid of wake thread and merge the functionality with the main_thread.

#ifdef ENABLE_USE_RTE_TIMER_MODE_FOR_WAKE_THREAD
struct rte_timer wake_timer[ONVM_NUM_WAKEUP_THREADS];
static void wake_timer_cb(struct rte_timer *ptr_timer, void *ptr_data);
int initialize_wake_timers(void *data);
#endif //USE_RTE_TIMER_MODE_FOR_WAKE_THREAD


typedef struct core_nf_timers {
        struct rte_timer    timer;
        uint16_t            timer_status;   //0=OFF, 1=ACTIVE,
        uint16_t            index;          //index in the sorted list;
        uint16_t            nf_id;
        uint16_t            core_id;
        uint16_t            next_nf_id;
        uint64_t            exec_period;
}core_nf_timers_t;
core_nf_timers_t    core_timers[MAX_CORES_ON_NODE];

#ifdef ENABLE_USE_RTE_TIMER_MODE_FOR_WAKE_THREAD
int
initialize_per_core_timers(void);
static void  arbiter_wakeup_client(uint16_t core_id, uint16_t index);
int launch_core_nf_timer(uint16_t core_id, uint16_t index, uint16_t nf_id, uint64_t exec_period);
#endif //ENABLE_USE_RTE_TIMER_MODE_FOR_WAKE_THREAD
/***********************Timer Functions************************************/
#ifdef ENABLE_USE_RTE_TIMER_MODE_FOR_WAKE_THREAD
static void wake_timer_cb(__attribute__((unused)) struct rte_timer *ptr_timer, void *ptr_data) {

        if(ptr_data) {
                //Ensure, it is called by only 1 wake thread; preferably tag first wake thread to do it in case of multiple wake threads..
                compute_and_order_nf_wake_priority();
                //logic changed to evaluate only the nf load, comp.cost and wakeup ordering; Handle actual wakeup in the main thread (poll mode)
                //check_and_enqueue_or_dequeue_nfs_from_bottleneck_watch_list();
                //handle_wakeup(NULL); //handle_wakeup((struct wakeup_info *)ptr_data);
        }
}

int
initialize_wake_timers(void *data) {
        static uint8_t index = 0;

        if(ONVM_NUM_WAKEUP_THREADS && index >= ONVM_NUM_WAKEUP_THREADS) return -1;

        rte_timer_init(&wake_timer[index]);

        uint64_t ticks = 0;
        ticks = ((uint64_t)WAKE_INTERVAL_IN_US *(rte_get_timer_hz()/1000000));

        rte_timer_reset_sync(&wake_timer[index],
                ticks,
                PERIODICAL,
                rte_lcore_id(),
                &wake_timer_cb, data
                );

        index++;
        return 0;
}
int
initialize_per_core_timers (void) {
        uint16_t core_id = 0;
        for(core_id=0; core_id < MAX_CORES_ON_NODE; core_id++) {
                rte_timer_init(&core_timers[core_id].timer);
                core_timers[core_id].timer_status=0;
                core_timers[core_id].core_id = core_id;
                core_timers[core_id].nf_id=0;
                core_timers[core_id].next_nf_id = 0;
        }
        //will be reset and launched at the time of nf_wakeups
        return 0;
}

static void per_core_timer_cb(__attribute__((unused)) struct rte_timer *tim, void *arg) {

        core_nf_timers_t *pCoreTimer = (core_nf_timers_t*)arg;
        if(pCoreTimer ) {
#ifdef __DEBUG_NFMGR_WK_LOGS__
                printf("Timer Expired Callback core [%d]  client [%d] at index [%d] for period [%zu]\n ",pCoreTimer->core_id, pCoreTimer->nf_id, pCoreTimer->index, pCoreTimer->exec_period);
#endif
                //stop the current client (force sleep the current client)
                check_and_block_nf(pCoreTimer->nf_id);
                pCoreTimer->timer_status=0;
                //wakeup next client
                arbiter_wakeup_client(pCoreTimer->core_id, ++(pCoreTimer->index));
        }
}

int launch_core_nf_timer(uint16_t core_id, uint16_t index, uint16_t nf_id, uint64_t exec_period) {
        core_timers[core_id].index = index;
        core_timers[core_id].nf_id = nf_id;
        core_timers[core_id].core_id = core_id;
        core_timers[core_id].exec_period = exec_period;
        if(exec_period) {
                if(core_timers[core_id].timer_status || rte_timer_pending(&core_timers[core_id].timer)) {
#ifdef __DEBUG_NFMGR_WK_LOGS__
                        printf("Force Stopping the timer!\n ");
#endif
                        rte_timer_stop(&core_timers[core_id].timer);
                }
#ifdef __DEBUG_NFMGR_WK_LOGS__
                printf("core [%d] Waking client [%d] at index [%d] for period [%zu]\n ",core_id, nf_id, index, exec_period);
#endif
                core_timers[core_id].timer_status=1;
                rte_timer_reset(&core_timers[core_id].timer,
                        exec_period,
                        SINGLE,
                        rte_lcore_id(),
                        &per_core_timer_cb, &core_timers[core_id]);
        }//otherwise skip the timer, no need for nf_wakeup_timer

        return 0;
}

static void  arbiter_wakeup_client(uint16_t core_id, uint16_t index) {
        //Get the instance_id
        if(index < nf_sched_param.nf_list_per_core[core_id].count) {
                uint16_t instance_id = nf_sched_param.nf_list_per_core[core_id].nf_ids[index];
                uint64_t exec_period = nf_sched_param.nf_list_per_core[core_id].run_time[instance_id];  //remember this indexing by instance_id;


                //try to wake_up_client
                int ret = wakeup_client_internal(instance_id);
                //if wake_up succeeded then launch timer that can signal it to stop and wakes up the next index in the list
                if(ret != -1) {
                        launch_core_nf_timer(core_id, index, instance_id, exec_period);
                }
                //skip this and continue with next in the list
                else {

                }
        }
}
#endif //ENABLE_USE_RTE_TIMER_MODE_FOR_WAKE_THREAD
/***********************Timer Functions************************************/

/***********************Internal Functions************************************/
/*
 * Return status:
 *                  0: wakeup signal not required
 *                  1: wakeup signal is required
 *                 -1: issue forced block/goto_sleep signal
 */
static inline int
whether_wakeup_client(int instance_id)
{
        /* if NF is not valid or Valid but Paused then do not wake up the NF */
        if (unlikely(!onvm_nf_is_valid(&nfs[instance_id]))||(onvm_nf_is_paused(&nfs[instance_id]))) {
        //if (unlikely(!onvm_nf_is_valid(&nfs[instance_id]))) {
                return 0;
        }
        /*if(unlikely(onvm_nf_is_paused(&nfs[instance_id]))){
                printf("Non Waking [%d] due to NF PAUSED!\n", instance_id);
                return 0;
        }*/

#ifdef ENABLE_NFV_RESL
        //IF NF is waiting on NDSYNC then do not wake it up.
        if(onvm_nf_is_waiting_on_NDSYNC(&nfs[instance_id])) {
                //printf("Non Waking [%d] due to ND SYNC!\n", instance_id);
                return 0;
        }

        //TODO: Remove this  check!
        //If PRIMARY(ACTIVE) NF IS ALIVE, then DO NOT WAKE THE (SECONDARY) STANDBY NF;
        //if((!is_primary_active_nf_id(instance_id)) && (NF_RUNNING == nfs[get_associated_active_or_standby_nf_id(instance_id)].info->status)) {
        if(unlikely((is_secondary_active_nf_id(instance_id))) && likely((onvm_nf_is_valid(&nfs[get_associated_active_or_standby_nf_id(instance_id)])) ) ) {
                return 0;
        }
        //IF ONLY EITHER IS ALIVE THEN WAKEUP THIS ONE
#endif

#ifdef NF_BACKPRESSURE_APPROACH_2
#ifdef ENABLE_GLOBAL_BACKPRESSURE
        /* Block the upstream (earlier) NFs from getting scheduled, if there is NF at downstream that is bottlenecked! */
        if (global_bkpr_mode && downstream_nf_overflow) {
                if (nfs[instance_id].info != NULL && is_upstream_NF(highest_downstream_nf_service_id,nfs[instance_id].info->service_id)) {
                        throttle_count++;
                        return -1;
                }
        }
        else
#endif
        //service chain case
        if (nfs[instance_id].throttle_this_upstream_nf) {
#if 0
                nfs[instance_id].throttle_count++;
#endif
                return -1;
        }
#endif //NF_BACKPRESSURE_APPROACH_2


#ifdef USE_NF_WAKE_THRESHOLD
        uint16_t cur_entries;
        cur_entries = rte_ring_count(nfs[instance_id].rx_q);
        if (cur_entries >= nfs_wakethr[instance_id]) {
                return 1;
        }
#else
        if(rte_ring_count(nfs[instance_id].rx_q)) return 1;
#endif
        return 0;
}

static inline void
notify_client(__attribute__((unused)) int instance_id)
{
#ifdef USE_SEMAPHORE
        sem_post(nfs[instance_id].mutex);
#endif
}

static inline int
wakeup_client_internal(int instance_id) {
        int ret = whether_wakeup_client(instance_id);
        if ( 1 == ret) {
                check_and_wakeup_nf((uint16_t)instance_id);
        }
#ifdef NF_BACKPRESSURE_APPROACH_2
        else if (-1 == ret) {
                check_and_block_nf((uint16_t)instance_id);
        }
#endif //NF_BACKPRESSURE_APPROACH_2
        return ret;
}

static inline void
wakeup_client(int instance_id, struct wakeup_info *wakeup_info)  {

        int ret = wakeup_client_internal(instance_id);

        if(1 == ret && wakeup_info) {
                wakeup_info->num_wakeups += 1;
        }
        return;
}

static inline void handle_wakeup_old(struct wakeup_info *wakeup_info) {

        unsigned i=0;
        for (i = wakeup_info->first_client; i < wakeup_info->last_client; i++) {
                wakeup_client(i, wakeup_info);
        }
}
static inline void handle_wakeup_ordered(__attribute__((unused))struct wakeup_info *wakeup_info) {

        #if defined (ENABLE_ORDERED_NF_WAKEUP)
        /* Now wake up the NFs as per sorted priority:
         * Next step Handle slack period before wake-up and schedule NFs for wake up; otherwise
         * we are at the mercy of OS Scheduler to schedule the NFs in each core */
        unsigned i=0;
        if(nf_sched_param.sorted) {
                for(i=0; i<MAX_CORES_ON_NODE; i++) {
                        if(nf_sched_param.nf_list_per_core[i].sorted && nf_sched_param.nf_list_per_core[i].count) {
                                #ifndef USE_ARBITER_NF_EXEC_PERIOD
                                unsigned nf_id=0;
                                for(nf_id=0; nf_id < nf_sched_param.nf_list_per_core[i].count; nf_id++) {
                                        wakeup_client(nf_sched_param.nf_list_per_core[i].nf_ids[nf_id], NULL /*wakeup_info*/);
                                }
                                #else
                                arbiter_wakeup_client(i, 0);
                                #endif  //USE_ARBITER_NF_EXEC_PERIOD
                        }
                }
        }
        //in case the data is not ready; wakeup NFs as usual
        else {
                if(wakeup_info) handle_wakeup_old(wakeup_info);
        }
        #else
        handle_wakeup_old(wakeup_info);
        #endif  //ENABLE_ORDERED_NF_WAKEUP
}

inline void handle_wakeup(__attribute__((unused))struct wakeup_info *wakeup_info) {
        handle_wakeup_ordered(wakeup_info);
        return;
}
#define WAKE_THREAD_SLEEP_INTERVAL_NS  (1000)
int
wakemgr_main(void *arg) {

#ifdef ENABLE_USE_RTE_TIMER_MODE_FOR_WAKE_THREAD
        initialize_wake_timers(arg);
#endif

        struct timespec req = {0,WAKE_THREAD_SLEEP_INTERVAL_NS}, res = {0,0};
        while (true) {
                //do it more periodically: poll mode (better than 100microsec delay) -- remove usleep(USLEEP_INTERVAL)
                check_and_enqueue_or_dequeue_nfs_from_bottleneck_watch_list();

#ifdef ENABLE_USE_RTE_TIMER_MODE_FOR_WAKE_THREAD
                rte_timer_manage();
#endif
                handle_wakeup((struct wakeup_info *)arg);
                nanosleep(&req, &res); //usleep(USLEEP_INTERVAL);
        }

        return 0;
}

inline void check_and_wakeup_nf(uint16_t instance_id) {
        if (rte_atomic16_read(nfs[instance_id].shm_server) ==1) {
                rte_atomic16_set(nfs[instance_id].shm_server, 0);
                notify_client(instance_id);
#ifdef ENABLE_NF_WAKE_NOTIFICATION_COUNTER
                nfs[instance_id].stats.wakeup_count+=1;
#endif
        }
}

inline void check_and_block_nf(uint16_t instance_id) {
        /* Make sure to set the flag here and check for flag in nf_lib and block */
        //printf("\n***************Blocking NF! %d**************\n", instance_id);
        //exit(10);
        rte_atomic16_set(nfs[instance_id].shm_server, 1);
}
#endif //INTERRUPT_SEM
