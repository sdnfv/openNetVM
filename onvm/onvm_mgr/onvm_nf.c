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

                              onvm_nf.c

       This file contains all functions related to NF management.

******************************************************************************/


#include "onvm_mgr.h"
#include "onvm_nf.h"
#include "onvm_stats.h"

#ifdef ENABLE_ZOOKEEPER
#include "onvm_zookeeper.h"
#endif

uint16_t next_instance_id = 1; //start from 1.

// Global mode variables (default service chain without flow_Table entry: can support only 1 flow (i.e all flows have same NFs)
#ifdef ENABLE_GLOBAL_BACKPRESSURE
uint8_t  global_bkpr_mode=0;
uint16_t downstream_nf_overflow = 0;
uint16_t highest_downstream_nf_service_id=0;
uint16_t lowest_upstream_to_throttle = 0;
uint64_t throttle_count = 0;
#endif //#ifdef ENABLE_GLOBAL_BACKPRESSURE

#define EWMA_LOAD_ADECAY (0.5)  //(0.1) or or (0.002) or (.004)

//sorted list of NFs based on load requirement on the core
//nfs_per_core_t nf_list_per_core[MAX_CORES_ON_NODE];
nf_schedule_info_t nf_sched_param;

static inline void assign_nf_cgroup_weight(uint16_t nf_id);
static inline void assign_all_nf_cgroup_weight(void);
void compute_nf_exec_period_and_cgroup_weight(void);
void compute_and_assign_nf_cgroup_weight(void);
inline void monitor_nf_node_liveliness_via_pid_monitoring(void);
int nf_sort_func(const void * a, const void *b);
inline void extract_nf_load_and_svc_rate_info(__attribute__((unused)) unsigned long interval);
inline void setup_nfs_priority_per_core_list(__attribute__((unused)) unsigned long interval);
inline int onvm_nf_register_run(struct onvm_nf_info *nf_info);
#define DEFAULT_NF_CPU_SHARE    (1024)

//Local Data structure to compute nf_load and comp_cost contention on each core
typedef struct nf_core_and_cc_info {
        uint32_t total_comp_cost;       //total computation cost on the core (sum of all NFs computation cost)
        uint32_t total_nf_count;        //total count of the NFs on the core (sum of all NFs)
        uint32_t total_pkts_served;     //total pkts processed on the core (sum of all NFs packet processed).
        uint32_t total_load;            //total pkts (avergae) queued up on the core for processing.
        uint64_t total_load_cost_fct;   //total product of current load and computation cost on core (aggregate demand in total cycles)
}nf_core_and_cc_info_t;

/********************************Interfaces***********************************/
/*
 * This function computes and assigns weights to each nfs cgroup based on its contention and requirements
 * PRerequisite: nfs[]->info->comp_cost and  nfs[]->info->load should be already updated.  -- updated by extract_nf_load_and_svc_rate_info()
 */
static inline void assign_nf_cgroup_weight(uint16_t nf_id) {
        #ifdef USE_CGROUPS_PER_NF_INSTANCE
        if ((onvm_nf_is_valid(&nfs[nf_id])) && (nfs[nf_id].info->comp_cost)) {
                //set_cgroup_nf_cpu_share(nfs[nf_id].info->instance_id, nfs[nf_id].info->cpu_share);
                set_cgroup_nf_cpu_share_from_onvm_mgr(nfs[nf_id].info->instance_id, nfs[nf_id].info->cpu_share);
        }
        #else
        if(nf_id) nf_id = 0;
        #endif //USE_CGROUPS_PER_NF_INSTANCE

}
static inline void assign_all_nf_cgroup_weight(void) {
        uint16_t nf_id = 0;
        for (nf_id=0; nf_id < MAX_NFS; nf_id++) {
                assign_nf_cgroup_weight(nf_id);
        }
}
void compute_nf_exec_period_and_cgroup_weight(void) {

#if defined (USE_CGROUPS_PER_NF_INSTANCE)

#if defined(ENABLE_ARBITER_MODE_WAKEUP) || !defined(USE_DYNAMIC_LOAD_FACTOR_FOR_CPU_SHARE)
        const uint64_t total_cycles_in_epoch = ARBITER_PERIOD_IN_US *(rte_get_timer_hz()/1000000);
#endif
        static nf_core_and_cc_info_t nfs_on_core[MAX_CORES_ON_NODE];

        uint16_t nf_id = 0;
        memset(nfs_on_core, 0, sizeof(nfs_on_core));

        //First build the total cost and contention info per core
        for (nf_id=0; nf_id < MAX_NFS; nf_id++) {
                if (onvm_nf_is_valid(&nfs[nf_id])){
                        nfs_on_core[nfs[nf_id].info->core_id].total_comp_cost += nfs[nf_id].info->comp_cost;
                        nfs_on_core[nfs[nf_id].info->core_id].total_nf_count++;
                        nfs_on_core[nfs[nf_id].info->core_id].total_load += nfs[nf_id].info->load;            //nfs[nf_id].info->avg_load;
                        nfs_on_core[nfs[nf_id].info->core_id].total_pkts_served += nfs[nf_id].info->svc_rate; //nfs[nf_id].info->avg_svc;
                        nfs_on_core[nfs[nf_id].info->core_id].total_load_cost_fct += (nfs[nf_id].info->comp_cost*nfs[nf_id].info->load);
                }
        }

        //evaluate and assign the cost of each NF
        for (nf_id=0; nf_id < MAX_NFS; nf_id++) {
                if ((onvm_nf_is_valid(&nfs[nf_id])) && (nfs[nf_id].info->comp_cost)) {

                        // share of NF = 1024* NF_comp_cost/Total_comp_cost
                        //Note: ideal share of NF is 100%(1024) so for N NFs sharing core => N*100 or (N*1024) then divide the cost proportionally
#ifndef USE_DYNAMIC_LOAD_FACTOR_FOR_CPU_SHARE
                        //Static accounting based on computation_cost_only
                        if(nfs_on_core[nfs[nf_id].info->core_id].total_comp_cost) {
                                nfs[nf_id].info->cpu_share = (uint32_t) ((DEFAULT_NF_CPU_SHARE*nfs_on_core[nfs[nf_id].info->core_id].total_nf_count)*(nfs[nf_id].info->comp_cost))
                                                /((nfs_on_core[nfs[nf_id].info->core_id].total_comp_cost));

                                nfs[nf_id].info->exec_period = ((nfs[nf_id].info->comp_cost)*total_cycles_in_epoch)/nfs_on_core[nfs[nf_id].info->core_id].total_comp_cost; //(total_cycles_in_epoch)*(total_load_on_core)/(load_of_nf)
                        }
                        else {
                                nfs[nf_id].info->cpu_share = (uint32_t)DEFAULT_NF_CPU_SHARE;
                                nfs[nf_id].info->exec_period = 0;

                        }
                        #ifdef __DEBUG_NFMGR_NF_LOGS__
                        printf("\n ***** Client [%d] with cost [%d] on core [%d] with total_demand [%d] shared by [%d] NFs, got cpu share [%d]***** \n ", nfs[nf_id].info->instance_id, nfs[nf_id].info->comp_cost, nfs[nf_id].info->core_id,
                                                                                                                                                   nfs_on_core[nfs[nf_id].info->core_id].total_comp_cost,
                                                                                                                                                   nfs_on_core[nfs[nf_id].info->core_id].total_nf_count,
                                                                                                                                                   nfs[nf_id].info->cpu_share);
                        #endif //__DEBUG_NFMGR_NF_LOGS__

#else
                        uint64_t num = 0;
                        //Dynamic: Based on accounting the product of Load*comp_cost factors. We can define the weights Alpha(\u03b1) and Beta(\u03b2) for apportioning Load and Comp_Costs: (\u03b1*nfs[nf_id].info->load)*(\u03b2*nfs[nf_id].info->comp_cost) | \u03b2*\u03b1 = 1.
                        if (nfs_on_core[nfs[nf_id].info->core_id].total_load_cost_fct) {

                                num = (uint64_t)(nfs_on_core[nfs[nf_id].info->core_id].total_nf_count)*(DEFAULT_NF_CPU_SHARE)*(nfs[nf_id].info->comp_cost)*(nfs[nf_id].info->load);
                                nfs[nf_id].info->cpu_share = (uint32_t) (num/nfs_on_core[nfs[nf_id].info->core_id].total_load_cost_fct);
                                //nfs[nf_id].info->cpu_share = ((uint64_t)(((DEFAULT_NF_CPU_SHARE*nfs_on_core[nfs[nf_id].info->core_id].total_nf_count)*(nfs[nf_id].info->comp_cost*nfs[nf_id].info->load)))
                                //                /((nfs_on_core[nfs[nf_id].info->core_id].total_load_cost_fct)));
#ifdef ENABLE_ARBITER_MODE_WAKEUP
                                nfs[nf_id].info->exec_period = ((nfs[nf_id].info->comp_cost)*(nfs[nf_id].info->load)*total_cycles_in_epoch)/nfs_on_core[nfs[nf_id].info->core_id].total_load_cost_fct; //(total_cycles_in_epoch)*(total_load_on_core)/(load_of_nf)
#endif
                        }
                        else {
                                nfs[nf_id].info->cpu_share = (uint32_t)DEFAULT_NF_CPU_SHARE;
#ifdef ENABLE_ARBITER_MODE_WAKEUP
                                nfs[nf_id].info->exec_period = 0;
#endif
                        }
                        #ifdef __DEBUG_NFMGR_NF_LOGS__
                        printf("\n ***** Client [%d] with cost [%d] and load [%d] on core [%d] with total_demand_comp_cost=%"PRIu64", shared by [%d] NFs, got num=%"PRIu64", cpu share [%d]***** \n ", nfs[nf_id].info->instance_id, nfs[nf_id].info->comp_cost, nfs[nf_id].info->load, nfs[nf_id].info->core_id,
                                                                                                                                                   nfs_on_core[nfs[nf_id].info->core_id].total_load_cost_fct,
                                                                                                                                                   nfs_on_core[nfs[nf_id].info->core_id].total_nf_count,
                                                                                                                                                   num, nfs[nf_id].info->cpu_share);
                        #endif //__DEBUG_NFMGR_NF_LOGS__
#endif //USE_DYNAMIC_LOAD_FACTOR_FOR_CPU_SHARE

                }
        }
#endif // #if defined (USE_CGROUPS_PER_NF_INSTANCE)
}
void compute_and_assign_nf_cgroup_weight(void) {
#if defined (USE_CGROUPS_PER_NF_INSTANCE)


        static int update_rate = 0;
        if (update_rate != 10) {
                update_rate++;
                return;
        }
        update_rate = 0;
#if defined(ENABLE_ARBITER_MODE_WAKEUP) || !defined(USE_DYNAMIC_LOAD_FACTOR_FOR_CPU_SHARE)
        const uint64_t total_cycles_in_epoch = ARBITER_PERIOD_IN_US *(rte_get_timer_hz()/1000000);
#endif
        static nf_core_and_cc_info_t nf_pool_per_core[MAX_CORES_ON_NODE]; // = {{0,0},}; ////nf_core_and_cc_info_t nf_pool_per_core[rte_lcore_count()+1]; // = {{0,0},};
        uint16_t nf_id = 0;
        memset(nf_pool_per_core, 0, sizeof(nf_pool_per_core));

        //First build the total cost and contention info per core
        for (nf_id=0; nf_id < MAX_NFS; nf_id++) {
                if (onvm_nf_is_valid(&nfs[nf_id])){
                        nf_pool_per_core[nfs[nf_id].info->core_id].total_comp_cost += nfs[nf_id].info->comp_cost;
                        nf_pool_per_core[nfs[nf_id].info->core_id].total_nf_count++;
                        nf_pool_per_core[nfs[nf_id].info->core_id].total_load += nfs[nf_id].info->load;            //nfs[nf_id].info->avg_load;
                        nf_pool_per_core[nfs[nf_id].info->core_id].total_pkts_served += nfs[nf_id].info->svc_rate; //nfs[nf_id].info->avg_svc;
                        nf_pool_per_core[nfs[nf_id].info->core_id].total_load_cost_fct += (nfs[nf_id].info->comp_cost*nfs[nf_id].info->load);
                }
        }

        //evaluate and assign the cost of each NF
        for (nf_id=0; nf_id < MAX_NFS; nf_id++) {
                if ((onvm_nf_is_valid(&nfs[nf_id])) && (nfs[nf_id].info->comp_cost)) {

                        // share of NF = 1024* NF_comp_cost/Total_comp_cost
                        //Note: ideal share of NF is 100%(1024) so for N NFs sharing core => N*100 or (N*1024) then divide the cost proportionally
#ifndef USE_DYNAMIC_LOAD_FACTOR_FOR_CPU_SHARE
                        //Static accounting based on computation_cost_only
                        if(nf_pool_per_core[nfs[nf_id].info->core_id].total_comp_cost) {
                                nfs[nf_id].info->cpu_share = (uint32_t) ((DEFAULT_NF_CPU_SHARE*nf_pool_per_core[nfs[nf_id].info->core_id].total_nf_count)*(nfs[nf_id].info->comp_cost))
                                                /((nf_pool_per_core[nfs[nf_id].info->core_id].total_comp_cost));

                                nfs[nf_id].info->exec_period = ((nfs[nf_id].info->comp_cost)*total_cycles_in_epoch)/nf_pool_per_core[nfs[nf_id].info->core_id].total_comp_cost; //(total_cycles_in_epoch)*(total_load_on_core)/(load_of_nf)
                        }
                        else {
                                nfs[nf_id].info->cpu_share = (uint32_t)DEFAULT_NF_CPU_SHARE;
                                nfs[nf_id].info->exec_period = 0;

                        }
                        #ifdef __DEBUG_NFMGR_NF_LOGS__
                        printf("\n ***** Client [%d] with cost [%d] on core [%d] with total_demand [%d] shared by [%d] NFs, got cpu share [%d]***** \n ", nfs[nf_id].info->instance_id, nfs[nf_id].info->comp_cost, nfs[nf_id].info->core_id,
                                                                                                                                                   nf_pool_per_core[nfs[nf_id].info->core_id].total_comp_cost,
                                                                                                                                                   nf_pool_per_core[nfs[nf_id].info->core_id].total_nf_count,
                                                                                                                                                   nfs[nf_id].info->cpu_share);
                        #endif //__DEBUG_NFMGR_NF_LOGS__

#else
                        uint64_t num = 0;
                        //Dynamic: Based on accounting the product of Load*comp_cost factors. We can define the weights Alpha(α) and Beta(β) for apportioning Load and Comp_Costs: (α*nfs[nf_id].info->load)*(β*nfs[nf_id].info->comp_cost) | β*α = 1.
                        if (nf_pool_per_core[nfs[nf_id].info->core_id].total_load_cost_fct) {

                                num = (uint64_t)(nf_pool_per_core[nfs[nf_id].info->core_id].total_nf_count)*(DEFAULT_NF_CPU_SHARE)*(nfs[nf_id].info->comp_cost)*(nfs[nf_id].info->load);
                                nfs[nf_id].info->cpu_share = (uint32_t) (num/nf_pool_per_core[nfs[nf_id].info->core_id].total_load_cost_fct);
                                //nfs[nf_id].info->cpu_share = ((uint64_t)(((DEFAULT_NF_CPU_SHARE*nf_pool_per_core[nfs[nf_id].info->core_id].total_nf_count)*(nfs[nf_id].info->comp_cost*nfs[nf_id].info->load)))
                                //                /((nf_pool_per_core[nfs[nf_id].info->core_id].total_load_cost_fct)));
#ifdef ENABLE_ARBITER_MODE_WAKEUP
                                nfs[nf_id].info->exec_period = ((nfs[nf_id].info->comp_cost)*(nfs[nf_id].info->load)*total_cycles_in_epoch)/nf_pool_per_core[nfs[nf_id].info->core_id].total_load_cost_fct; //(total_cycles_in_epoch)*(total_load_on_core)/(load_of_nf)
#endif
                        }
                        else {
                                nfs[nf_id].info->cpu_share = (uint32_t)DEFAULT_NF_CPU_SHARE;
#ifdef ENABLE_ARBITER_MODE_WAKEUP
                                nfs[nf_id].info->exec_period = 0;
#endif
                        }
                        #ifdef __DEBUG_NFMGR_NF_LOGS__
                        printf("\n ***** Client [%d] with cost [%d] and load [%d] on core [%d] with total_demand_comp_cost=%"PRIu64", shared by [%d] NFs, got num=%"PRIu64", cpu share [%d]***** \n ", nfs[nf_id].info->instance_id, nfs[nf_id].info->comp_cost, nfs[nf_id].info->load, nfs[nf_id].info->core_id,
                                                                                                                                                   nf_pool_per_core[nfs[nf_id].info->core_id].total_load_cost_fct,
                                                                                                                                                   nf_pool_per_core[nfs[nf_id].info->core_id].total_nf_count,
                                                                                                                                                   num, nfs[nf_id].info->cpu_share);
                        #endif //__DEBUG_NFMGR_NF_LOGS__
#endif //USE_DYNAMIC_LOAD_FACTOR_FOR_CPU_SHARE

                        //set_cgroup_nf_cpu_share(nfs[nf_id].info->instance_id, nfs[nf_id].info->cpu_share);
                        set_cgroup_nf_cpu_share_from_onvm_mgr(nfs[nf_id].info->instance_id, nfs[nf_id].info->cpu_share);
                }
        }
#endif // #if defined (USE_CGROUPS_PER_NF_INSTANCE)
}


//inline 
void extract_nf_load_and_svc_rate_info(__attribute__((unused)) unsigned long interval) {
#if defined (USE_CGROUPS_PER_NF_INSTANCE) && defined(INTERRUPT_SEM)
        uint16_t nf_id = 0;
        for (; nf_id < MAX_NFS; nf_id++) {
                struct onvm_nf *cl = &nfs[nf_id];
                if (onvm_nf_is_valid(cl)){
                        static onvm_stats_snapshot_t st;
                        get_onvm_nf_stats_snapshot_v2(nf_id,&st,0);
                        cl->info->load      =  (st.rx_delta + st.rx_drop_delta);//(cl->stats.rx - cl->stats.prev_rx + cl->stats.rx_drop - cl->stats.prev_rx_drop); //rte_ring_count(cl->rx_q);
                        cl->info->avg_load  =  ((cl->info->avg_load == 0) ? (cl->info->load):((cl->info->avg_load + cl->info->load) /2));   // (((1-EWMA_LOAD_ADECAY)*cl->info->avg_load) + (EWMA_LOAD_ADECAY*cl->info->load))
                        cl->info->svc_rate  =  (st.tx_delta); //(clients_stats->tx[nf_id] -  clients_stats->prev_tx[nf_id]);
#if 0
                        cl->info->avg_svc   =  ((cl->info->avg_svc == 0) ? (cl->info->svc_rate):((cl->info->avg_svc + cl->info->svc_rate) /2));
                        cl->info->drop_rate =  (st.rx_drop_rate);
#endif

                        #ifdef STORE_HISTOGRAM_OF_NF_COMPUTATION_COST
                        //Get the Median Computation cost, instead of running average; else running average is expected to be set already.
                        cl->info->comp_cost = hist_extract_v2(&cl->info->ht2, VAL_TYPE_MEDIAN);
                        #endif //STORE_HISTOGRAM_OF_NF_COMPUTATION_COST
                }
                else if (cl && cl->info) {
                        cl->info->load      = 0;
                        cl->info->avg_load  = 0;
                        cl->info->svc_rate  = 0;
#if 0
                        cl->info->avg_svc   = 0;
#endif
                }
        }

        //compute the execution_period_and_cgroup_weight    -- better to separate the two??
        compute_nf_exec_period_and_cgroup_weight();

        //sort and prepare the list of nfs_per_core_per_pool in the decreasing order of priority; use this list to wake up the NFs
        setup_nfs_priority_per_core_list(interval);
#endif
}

/* API to comptute_and_order_the_nfs based on aggr load (arrival rate * comp.cost):: supercedes extract_nf_load_and_svc_rate_info()
 * Note: This must be called by any/only 1 thread on a periodic basis.
 */
void compute_and_order_nf_wake_priority(void) {
        //Decouple The evaluation and wakee-up logic : move the code to main thread, which can perform this periodically;
        /* Firs:t extract load charactersitics in this epoch
         * Second: sort and prioritize NFs based on the demand matrix in this epoch
         * Finally: wake up the tasks in the identified priority
         * */
        #if defined (USE_CGROUPS_PER_NF_INSTANCE)
        extract_nf_load_and_svc_rate_info(0);   //setup_nfs_priority_per_core_list(0);
        #endif  //USE_CGROUPS_PER_NF_INSTANCE
        return;
}

int nf_sort_func(const void * a, const void *b) {
        uint32_t nfid1 = *(const uint32_t*)a;
        uint32_t nfid2 = *(const uint32_t*)b;
        struct onvm_nf *cl1 = &nfs[nfid1];
        struct onvm_nf *cl2 = &nfs[nfid2];

        if(!cl1 || !cl2) return 0;

        #if defined (USE_CGROUPS_PER_NF_INSTANCE)
        if(cl1->info->load < cl2->info->load) return 1;
        else if (cl1->info->load > cl2->info->load) return (-1);
        #endif //USE_CGROUPS_PER_NF_INSTANCE

        return 0;

}


void setup_nfs_priority_per_core_list(__attribute__((unused)) unsigned long interval) {
        #ifdef USE_CGROUPS_PER_NF_INSTANCE
        memset(&nf_sched_param, 0, sizeof(nf_sched_param));
        uint16_t nf_id = 0;
        for (nf_id=0; nf_id < MAX_NFS; nf_id++) {
                if ((onvm_nf_is_valid(&nfs[nf_id])) /* && (nfs[nf_id].info->comp_cost)*/) {
                        nf_sched_param.nf_list_per_core[nfs[nf_id].info->core_id].nf_ids[nf_sched_param.nf_list_per_core[nfs[nf_id].info->core_id].count++] = nf_id;
#ifdef ENABLE_ARBITER_MODE_WAKEUP
                        nf_sched_param.nf_list_per_core[nfs[nf_id].info->core_id].run_time[nf_id] = nfs[nf_id].info->exec_period;
#endif
                }
        }
        uint16_t core_id = 0;
        for(core_id=0; core_id < MAX_CORES_ON_NODE; core_id++) {
                if(!nf_sched_param.nf_list_per_core[core_id].count) continue;
                onvm_sort_generic(nf_sched_param.nf_list_per_core[core_id].nf_ids, ONVM_SORT_TYPE_CUSTOM, SORT_DESCENDING, nf_sched_param.nf_list_per_core[core_id].count, sizeof(nf_sched_param.nf_list_per_core[core_id].nf_ids[0]), nf_sort_func);
                nf_sched_param.nf_list_per_core[core_id].sorted=1;
#if 0
                {
                        unsigned x = 0;
                        printf("\n********** Sorted NFs on Core [%d]: ", core_id);
                        for (x=0; x< nf_sched_param.nf_list_per_core[core_id].count; x++) {
                                printf("[%d],", nf_sched_param.nf_list_per_core[core_id].nf_ids[x]);
                        }
                }
#endif
        }
        nf_sched_param.sorted=1;
        #endif //USE_CGROUPS_PER_NF_INSTANCE
}


//#include <sys/types.h>
//#include <signal.h>
inline void monitor_nf_node_liveliness_via_pid_monitoring(void) {
        uint16_t nf_id = 0;

        for (; nf_id < MAX_NFS; nf_id++) {
                if (onvm_nf_is_valid(&nfs[nf_id])){
                        if (kill(nfs[nf_id].info->pid, 0)) {
                                printf("\n\n******* Moving NF with InstanceID:%d state %d to STOPPED\n\n",nfs[nf_id].info->instance_id, nfs[nf_id].info->status);
                                nfs[nf_id].info->status = NF_STOPPED;
                                if (!onvm_nf_stop(nfs[nf_id].info))num_nfs--;   // directly perform in the calling thread; rather than enqueue/dequeue in nf_info_queue
                                //**** TO DO: Take necessary actions here: It still doesn't clean-up until the new_nf_pool is populated by adding/killing another NF instance.
                                // Still the IDs are not recycled.. missing some additional changes:: found bug in the way the IDs are recycled-- fixed change in onvm_nf_next_instance_id()
                        }
                }
        }
}

inline int
onvm_nf_is_valid(struct onvm_nf *cl) {
        return cl->info && (cl->info->status&NF_RUNNING);  //can be both running and paused
        return cl->info && cl->info->status == NF_RUNNING;
}
inline int
onvm_nf_is_paused(struct onvm_nf *cl){
        return cl->info->status&NF_PAUSED_BIT;
}
inline int
onvm_nf_is_processing(struct onvm_nf *cl) {
        return ((cl->info) && ((cl->info->status == NF_RUNNING)||(cl->info->status&NF_WT_ND_SYNC_BIT)));    //only in running state.
}
inline int
onvm_nf_is_waiting_on_NDSYNC(__attribute__((unused))struct onvm_nf *cl) {
#ifdef ENABLE_NF_PAUSE_TILL_OUTSTANDING_NDSYNC_COMMIT
        return ((cl->info) && (cl->info->status&NF_WT_ND_SYNC_BIT));
        //return ((cl->info) && (cl->info->bNDSycn)); //this is wrong, bNDSync will always be ON ( even if only 1 event, and 0 outstanding)
#endif
        return 0;
}

inline int
onvm_nf_is_NDSYNC_set(__attribute__((unused))struct onvm_nf *cl) {
#ifdef ENABLE_NF_PAUSE_TILL_OUTSTANDING_NDSYNC_COMMIT
        return ((cl->info) && (cl->info->bNDSycn));
        return ((cl->info) && (cl->info->status&NF_WT_ND_SYNC_BIT));

#endif
        return 0;
}

inline int
onvm_nf_is_instance_id_free(struct onvm_nf *cl) {
        return cl && (NULL == cl->info);
}

uint16_t
onvm_nf_next_instance_id(void) {
        struct onvm_nf *cl;
        int loop_count = 1;
        for(; loop_count < MAX_NFS; loop_count++) {
                if(next_instance_id >= MAX_NFS) {
                        next_instance_id = 1;   // 0 is Special NF in OVM_MGR now.
                }
                cl = &nfs[next_instance_id++];
                if (onvm_nf_is_instance_id_free(cl)) {
                        return cl->instance_id;
                }
        }
        return MAX_NFS;

}
#ifdef ENABLE_SYNC_MGR_TO_NF_MSG
//inline 
int onvm_nf_recv_resp_msg(void) ;
//inline 
int onvm_nf_recv_resp_msg(void) {
        int i, ret=0;
        void *new_msgs[1];
        struct onvm_nf_msg *msg;
        int num_msgs = MIN(rte_ring_count(mgr_rsp_queue), 1);

        if(!num_msgs) return 0;

        //if (rte_ring_dequeue_bulk(mgr_rsp_queue, new_msgs, 1,NULL) != 0) return 0; //Note: dpdk interface ret value changed earlier 15.1 0=success now 0=failure
        if (0 == rte_ring_dequeue_bulk(mgr_rsp_queue, new_msgs, 1,NULL)) return 0;

        for (i = 0; i < num_msgs; i++) {
#ifdef ENABLE_MSG_CONSTRUCT_NF_INFO_NOTIFICATION
                msg = (struct onvm_nf_msg*) new_msgs[i];

                switch (msg->msg_type) {
                case MSG_NF_SYNC_RESP:
                        ret = (int)((struct onvm_nf_info*)msg->msg_data)->instance_id;
                        break;
                default:
                        ret = 0;
                        break;
                }
                rte_mempool_put(nf_msg_pool, (void*)msg);
#else
#endif
        }
        return ret;
}
#endif //ENABLE_SYNC_MGR_TO_NF_MSG

//inline 
void onvm_nf_recv_and_process_msgs(void) ;
//inline 
void onvm_nf_recv_and_process_msgs(void) {
        int i;
        void *new_msgs[MAX_NFS];
        struct onvm_nf_info *nf;
        struct onvm_nf_msg *msg;
        int num_msgs = MIN(rte_ring_count(mgr_msg_queue), MAX_NFS);

        if(!num_msgs) return;

        //if (rte_ring_dequeue_bulk(mgr_msg_queue, new_msgs, num_msgs,NULL) != 0) return; //Note: dpdk interface ret value changed earlier 15.1 0=success now 0=failure
        if (0 == rte_ring_dequeue_bulk(mgr_msg_queue, new_msgs, num_msgs,NULL)) return;

        for (i = 0; i < num_msgs; i++) {
#ifdef ENABLE_MSG_CONSTRUCT_NF_INFO_NOTIFICATION
                msg = (struct onvm_nf_msg*) new_msgs[i];

                switch (msg->msg_type) {
                case MSG_NF_STARTING:
                        nf = (struct onvm_nf_info*)msg->msg_data;
                        onvm_nf_start(nf);
                        break;
                case MSG_NF_READY:
                        nf = (struct onvm_nf_info*)msg->msg_data;
                        onvm_nf_register_run(nf);
                        num_nfs++;
                        break;
                case MSG_NF_STOPPING:
                        nf = (struct onvm_nf_info*)msg->msg_data;
                        onvm_nf_stop(nf);
                        num_nfs--;
                        break;
                default:
                        break;
                }
#else
                nf = (struct onvm_nf_info *) new_msgs[i];

                if (nf->status == NF_WAITING_FOR_ID) {
                        onvm_nf_start(nf);
                } else if (nf->status == NF_WAITING_FOR_RUN) {
                        onvm_nf_register_run(nf);
                        num_nfs++;
                } else if (nf->status == NF_STOPPED) {
                        onvm_nf_stop(nf);
                        num_nfs--;
                }
#endif
                rte_mempool_put(nf_msg_pool, (void*)msg);
        }
}
void
onvm_nf_check_status(void) {

        /* Add PID monitoring to assert active NFs (non crashed) */
        monitor_nf_node_liveliness_via_pid_monitoring();

        /* Process Manager Messages */
        onvm_nf_recv_and_process_msgs();

}

/* This function must update the status params of NF
 *  1. NF Load characterisitics         (at smaller duration)   -- better to move to Arbiter
 *  2. NF CGROUP Weight Determination   (at smaller duration)   -- better to move to Arbiter
 *  3. Assign CGROUP Weight             (at larger duration)    -- handle here
 */
void
onvm_nf_stats_update(__attribute__((unused)) unsigned long interval) {


        //move this functionality to arbiter instead;
        #if defined (USE_CGROUPS_PER_NF_INSTANCE)
        //extract_nf_load_and_svc_rate_info(interval);
        #endif

        /* Ideal location to re-compute the NF weight */
        #if defined (USE_CGROUPS_PER_NF_INSTANCE) && defined(ENABLE_DYNAMIC_CGROUP_WEIGHT_ADJUSTMENT)
        //compute_and_assign_nf_cgroup_weight(); //
        {
                //compute_nf_exec_period_and_cgroup_weight(); //must_be_already computed in the arbiter_context; what if arbiter is not running??
                assign_all_nf_cgroup_weight();
        }
        #endif //USE_CGROUPS_PER_NF_INSTANCE
}

//inline 
int
onvm_nf_service_to_nf_map_V2(struct onvm_pkt_meta *meta,  __attribute__((unused)) struct rte_mbuf *pkt, __attribute__((unused)) struct onvm_flow_entry *flow_entry) {
        int nfID =-1;

        uint16_t service_id = meta->destination;
        uint16_t num_nfs_available = nf_per_service_count[service_id];
        if (num_nfs_available == 0)
                return nfID;

#ifdef ENABLE_NFV_RESL
        int i = 0;
        for(i=0; i<num_nfs_available; i++) {
                if(is_primary_active_nf_id(services[service_id][i])){
                        nfID = services[service_id][i];
                        break;
                }
        }
#else
        nfID = services[service_id][pkt->hash.rss % num_nfs_available];
#endif
        //If no active then set first available Instance
        if(nfID < 0) {
                nfID = services[service_id][0];
        }
        return nfID;
}

//inline 
uint16_t
onvm_nf_service_to_nf_map(uint16_t service_id, __attribute__((unused)) struct rte_mbuf *pkt) {
        uint16_t num_nfs_available = nf_per_service_count[service_id];

        if (num_nfs_available == 0)
                return MAX_NFS; //0;

#ifdef ENABLE_NFV_RESL
        //Ideally, return the Primary Active service always; only when primary is down must return secondary; Need better logic; But beware: this is fast path cannot add more complexity here.
        // I think, it would be simpler to maintain primary and secondary as two separate lists.
        return services[service_id][0];

        //Added this piece of code for Isolation test: so that two chains pick two diffrent NFs ( also need 2 rules i.e. two same NFs with 2 services)
        static uint16_t used=0;
        uint16_t instance_index = used % num_nfs_available; used++;
        uint16_t instance_id = services[service_id][instance_index];
        return instance_id;
        //for(; nf_index < num_nfs_available; nf_index++) {
        //
        //}
#else
        //Note: The num_nfs_available can change dynamically; this results in two problems:
        // a) When new instance is added or existing is terminated the instance_index mapping can change based on hash.rss value
        // b) this current logic/sequence implies even for packets with mapped FT entry will be forced to resolve svc_id to instance id -- No binding of Flow to Instance (May be Good/bad)??

        if (pkt == NULL)
                return MAX_NFS; //0;

        uint16_t instance_index = pkt->hash.rss % num_nfs_available;
        uint16_t instance_id = services[service_id][instance_index];
        return instance_id;
#endif
}
//inline 
int
onvm_nf_send_msg(uint16_t dest, uint8_t msg_type, __attribute__((unused)) uint8_t msg_mode, void *msg_data) {
        int ret;
        struct onvm_nf_msg *msg;
        ret = rte_mempool_get(nf_msg_pool, (void**)(&msg));
        if (ret != 0) {
                RTE_LOG(INFO, APP, "Unable to allocate msg from pool:\n");
                return ret;
        }
        msg->msg_type = msg_type;
        msg->msg_data = msg_data;
#ifdef ENABLE_SYNC_MGR_TO_NF_MSG
        msg->is_sync = msg_mode;
#endif
        ret = rte_ring_sp_enqueue(nfs[dest].msg_q, (void*)msg);
#ifdef INTERRUPT_SEM
        //if(!ret) {
                check_and_wakeup_nf(dest);
        //}
#endif
        return ret;
}
#define MAX_SYNC_RETRY_COUNT (3)
//inline 
int
onvm_nf_send_msg_sync(uint16_t dest, uint8_t msg_type, void *msg_data) {
#ifdef ENABLE_SYNC_MGR_TO_NF_MSG
        if( 0 == onvm_nf_send_msg(dest, msg_type, MSG_MODE_SYNCHRONOUS, msg_data)) {
                // TODO: wait_for_response_notification_message from the destination NF;
                //is harmful what if NF fails to reply-- should have wait_timeout and also must not block processing of other NF messages.
                unsigned retry_count = MAX_SYNC_RETRY_COUNT;
                uint16_t resp_dest=0;
                struct timespec req = {0,1000}, res = {0,0};
                do {
                        RTE_LOG(INFO, APP, "Core %d: Waiting for Response from %"PRIu16" NF \n", rte_lcore_id(), dest);
                        if((resp_dest = onvm_nf_recv_resp_msg())) {
                                RTE_LOG(INFO, APP, "Core %d: Got Response from %"PRIu16" NF \n", rte_lcore_id(), resp_dest);
                                //if(resp_dest == dest);
                                break;
                        }
                        nanosleep(&req, &res); //sleep(1);
                }while(retry_count--);
        }
#else
        return onvm_nf_send_msg(dest, msg_type, MSG_MODE_SYNCHRONOUS, msg_data);
#endif
        return 0;
}
/******************************Internal functions*****************************/

//inline 
int onvm_nf_register_run(struct onvm_nf_info *nf_info) {

        uint16_t service_count = nf_per_service_count[nf_info->service_id]++;
#ifdef ENABLE_NFV_RESL
        //Temporary solution: make sure to move the PRIMARY(Active NF) to the Top and move remaining to the bottom. Why? To fix how packets get forwarded to the NF Instance: InstList[hash%NInstances] or InstList[0];
        if( (service_count > 0) && (is_primary_active_nf_id(nf_info->instance_id) ) ) {
                int mapIndex = service_count;
                for (; (mapIndex > 0 && is_secondary_active_nf_id(services[nf_info->service_id][mapIndex - 1])); mapIndex--) {
                        services[nf_info->service_id][mapIndex] = services[nf_info->service_id][mapIndex - 1];
                }
                services[nf_info->service_id][mapIndex]=nf_info->instance_id;
        } else {
                services[nf_info->service_id][service_count] = nf_info->instance_id;
        }

        //FOR REPLICA OR STANDBY NF: When Active  is already running start the STANDBY in PAUSE MODE; else if NO ACTIVE then start STANDBY IN RUNNING MODE
        if(unlikely(is_secondary_active_nf_id(nf_info->instance_id))) {
                if(likely(onvm_nf_is_valid(&nfs[get_associated_active_nf_id(nf_info->instance_id)]) ) ) {
                        //Start the secondary NF in paused state to avoid packet processing.
                        nf_info->status = NF_PAUSED;
                        //onvm_nf_send_msg(nf_info->instance_id,MSG_PAUSE,MSG_MODE_ASYNCHRONOUS,NULL); //TODO: Note: this message can be avoided and NFLib can internally check if state is paused then block.
                } else {
                        //can start the NF in Running state and enable it to process packets.
                        nf_info->status = NF_RUNNING;
                }
        }
        //Main concern: When Standby is already running; must signal the standby NF to stop processing packets and move standby to PAUSED state. then move active to RUNNING
        else { //if((is_primary_active_nf_id(nf_info->instance_id)) && (onvm_nf_is_valid(&nfs[get_associated_active_or_standby_nf_id(nf_info->instance_id)]) ) ) {
                if(unlikely(onvm_nf_is_valid(&nfs[get_associated_standby_nf_id(nf_info->instance_id)]) ) ) {
                        // First Run the primary in Pause state and then explicitly resume after notifying the secondary NF to pause!
                        //nf_info->status = NF_PAUSED;

                        //Next Pause the Secondary node (this might take a while to pause as NF may be busy processing packets)
                        nfs[get_associated_standby_nf_id(nf_info->instance_id)].info->status = NF_PAUSED;
                        onvm_nf_send_msg(get_associated_standby_nf_id(nf_info->instance_id),MSG_PAUSE,MSG_MODE_ASYNCHRONOUS,NULL);
                        //rte_atomic16_set(nfs[get_associated_standby_nf_id(nf_info->instance_id)].shm_server, 1);

                        //Now resume the Primary node
                        //onvm_nf_send_msg(nf_info->instance_id,MSG_RESUME,MSG_MODE_ASYNCHRONOUS,NULL); // First Run the primary in Pause state and then explicitly resume after notifying the secondary NF to pause!

                        //TODO: Note this scenario is resulting in Data path toggle backnforth between selection of NF Instance ID while enqueuing packets;
                        //Must switch logic so that control path (here) can update the Active/Affected Function Chains with the right Instance Id
                        nf_info->status = NF_RUNNING;
                } else {
                        //can immediately start the NF in running state to process packets
                        nf_info->status = NF_RUNNING;
                }
        }
        /* int mapIndex = 0;
        for(mapIndex = 0; mapIndex <= service_count; mapIndex++) {
                printf("\n services[%d][%d]= %d", nf_info->service_id, mapIndex, services[nf_info->service_id][mapIndex]);
        } */
#else
        // Register this NF running within its service
        services[nf_info->service_id][service_count] = nf_info->instance_id;
        nf_info->status = NF_RUNNING;
#endif

/** TODO: Verify if this is the correct place to call the ZooKeerper NF START? **/
#ifdef ENABLE_ZOOKEEPER
        // Register this NF with ZooKeeper
        // Need to add 1 to service_count because var++ is used above
        onvm_zk_nf_start(nf_info->service_id, service_count + 1, nf_info->instance_id);
#endif

        //nf_info->status = NF_RUNNING;
        return nf_per_service_count[nf_info->service_id];
}

//inline 
int
onvm_nf_start(struct onvm_nf_info *nf_info) {

        if((NULL == nf_info) || (NF_WAITING_FOR_ID != nf_info->status))
                return 1;

        // if NF passed its own id on the command line, don't assign here
        // assume user is smart enough to avoid duplicates
        uint16_t nf_id = nf_info->instance_id == (uint16_t)NF_NO_ID
                ? onvm_nf_next_instance_id()
                : nf_info->instance_id;

        if (nf_id >= MAX_NFS) {
                // There are no more available IDs for this NF
                printf("\n Invalid NF_ID! Rejecting the NF Start!\n ");
                nf_info->status = NF_NO_IDS;
                return 1;
        }

        if (onvm_nf_is_valid(&nfs[nf_id])) {
                // This NF is trying to declare an ID already in use
                printf("\n Invalid NF (conflicting ID!!) Rejecting the NF Start!\n ");
                nf_info->status = NF_ID_CONFLICT;
                return 1;
        }

        // Keep reference to this NF in the manager
        nf_info->instance_id = nf_id;
        nfs[nf_id].info = nf_info;
        nfs[nf_id].instance_id = nf_id;

#ifdef ENABLE_NFV_RESL
        //also update NF shared State mempool pointer from pre-setup nfs table
        nf_info->nf_state_mempool = nfs[nf_id].nf_state_mempool;
#ifdef ENABLE_PER_SERVICE_MEMPOOL
        if(services_state_pool && (nf_info->service_id < num_services)) {
                nf_info->service_state_pool = services_state_pool[nf_info->service_id];
                nfs[nf_id].service_state_pool = nf_info->service_state_pool;
        }
#endif

#endif
        // Register this NF running within its service
        //uint16_t service_count = nf_per_service_count[nf_info->service_id]++;
        //services[nf_info->service_id][service_count] = nf_id;

        // Let the NF continue its init process
        nf_info->status = NF_STARTING;
        return 0;
}


//inline 
int
onvm_nf_stop(struct onvm_nf_info *nf_info) {
        uint16_t nf_id;
        uint16_t service_id;
        int mapIndex;
        struct rte_mempool *nf_info_mp;

        if(unlikely(nf_info == NULL)){
                printf(" Null Entry for NF! Bad request for Stop!!\n ");
                return 1;
        }


        nf_id = nf_info->instance_id;
        service_id = nf_info->service_id;

#if defined(ENABLE_NFV_RESL)
        //For Primary Instance DOWN; ensure that if corresponding secondary is active move it to RUNNING state;
        if(likely(is_primary_active_nf_id(nf_id))) {
                uint16_t stdby_nfid = get_associated_standby_nf_id(nf_id);
                struct onvm_nf *cl = &nfs[stdby_nfid];
                if(likely((onvm_nf_is_valid(cl) && onvm_nf_is_paused(cl)))) {
                        cl->info->status ^=NF_PAUSED_BIT;
                        onvm_nf_send_msg(stdby_nfid, MSG_RESUME, MSG_MODE_ASYNCHRONOUS,NULL);
                }
        }
#if defined (ENABLE_PER_SERVICE_MEMPOOL)
        nfs[nf_id].service_state_pool = NULL;
#endif
#endif
        /* Clean up dangling pointers to info struct */
        nfs[nf_id].info = NULL;

        /* Reset stats */
        onvm_stats_clear_client(nf_id);

        /* Remove this NF from the service map.
         * Need to shift all elements past it in the array left to avoid gaps */
        nf_per_service_count[service_id]--;
        for (mapIndex = 0; mapIndex < MAX_NFS_PER_SERVICE; mapIndex++) {
                if (services[service_id][mapIndex] == nf_id) {
                        break;
                }
        }

        if (mapIndex < MAX_NFS_PER_SERVICE) {  // sanity error check
                services[service_id][mapIndex] = 0;
                for (; mapIndex < MAX_NFS_PER_SERVICE - 1; mapIndex++) {
                        // Shift the NULL to the end of the array
                        if (services[service_id][mapIndex + 1] == 0) {
                                // Short circuit when we reach the end of this service's list
                                break;
                        }
                        services[service_id][mapIndex] = services[service_id][mapIndex + 1];
                        services[service_id][mapIndex + 1] = 0;
                }
        }

#ifdef ENABLE_ZOOKEEPER
        uint16_t service_count = nf_per_service_count[service_id];
        // Unregister this NF with ZooKeeper
        onvm_zk_nf_stop(service_id, service_count, nf_id);
#endif

        /* Free info struct */
        /* Lookup mempool for nf_info struct */
        nf_info_mp = rte_mempool_lookup(_NF_MEMPOOL_NAME);
        if (nf_info_mp == NULL) {
                printf(" Null Entry for NF_INFO_MAP in MEMPOOL! No pool available!!\n ");
                return 1;
        }
        rte_mempool_put(nf_info_mp, (void*)nf_info);
        printf(" NF reclaimed to NF pool!!! \n ");
        return 0;
}


//Note: This function assumes that the NF mapping is setup in the sc and 2)
#ifdef ENABLE_NF_BASED_BKPR_MARKING
static sc_entries_list sc_list[SDN_FT_ENTRIES];
#endif

int
onvm_mark_all_entries_for_bottleneck(uint16_t nf_id) {
        int ret = 0;
#ifdef ENABLE_NF_BASED_BKPR_MARKING

#ifdef ENABLE_GLOBAL_BACKPRESSURE
        /*** Note: adding this global is expensive (around 1.5Mpps drop) and better to remove the default chain Backpressure feature.. or do int inside default chain usage some way */
        /** global single chain scenario:: Note: This only works for the default chain case where service ID of chain is always in increasing order **/
        if(global_bkpr_mode) {
                downstream_nf_overflow = 1;
                SET_BIT(highest_downstream_nf_service_id, nfs[nf_id].info->service_id);//highest_downstream_nf_service_id = cl->info->service_id;
                return 0;
        }
#endif //ENABLE_GLOBAL_BACKPRESSURE

        uint32_t ttl_chains = extract_active_service_chains(NULL, sc_list,SDN_FT_ENTRIES);
#if 0
        const active_sc_entries_t* act_chains = onvm_sc_get_all_active_chains();
        if(act_chains->sc_count != ttl_chains) {
                printf("Invalid number of chains detected! [%d] vs [%d] ", act_chains->sc_count, ttl_chains);
        }
#endif

        //There must be valid chains
        if(ttl_chains) {
                uint32_t s_inx = 0;
                for(s_inx=0; s_inx <SDN_FT_ENTRIES; s_inx++) {
                        if(sc_list[s_inx].sc) {
                                int i =0;
                                for(i=1;i<=sc_list[s_inx].sc->chain_length;++i) {
#ifdef ENABLE_NFV_RESL
                                        //if resiliency is enabled: then also need to check mirrored status of active/standby NF if it is alive.
                                        uint16_t alt_nf_id = get_associated_active_or_standby_nf_id(nf_id);
                                        if((nf_id == sc_list[s_inx].sc->nf_instance_id[i]) ||(alt_nf_id == sc_list[s_inx].sc->nf_instance_id[i]) ) {
#else
                                        if(nf_id == sc_list[s_inx].sc->nf_instance_id[i]) {
#endif
                                                //mark this sc with this index;;
                                                if(!(TEST_BIT(sc_list[s_inx].sc->highest_downstream_nf_index_id, i))) {
                                                        SET_BIT(sc_list[s_inx].sc->highest_downstream_nf_index_id, i);
#ifdef NF_BACKPRESSURE_APPROACH_1
                                                        break;
#endif
#ifdef NF_BACKPRESSURE_APPROACH_2
                                                        uint32_t index = (i-1);
                                                        for(; index >=1 ; index-- ) { //for(; index < meta->chain_index; index++ ) {
                                                                if(!nfs[sc_list[s_inx].sc->nf_instance_id[index]].throttle_this_upstream_nf) {
                                                                        nfs[sc_list[s_inx].sc->nf_instance_id[index]].throttle_this_upstream_nf=1;
                                                                }
#ifdef HOP_BY_HOP_BACKPRESSURE
                                                                break;
#endif //HOP_BY_HOP_BACKPRESSURE
                                                        }
#endif  //NF_BACKPRESSURE_APPROACH_2
                                                }
                                        }
                                }
                        }
                        else {
                                break;  //reached end of schains list;
                        }
                }
        }
#else
        //sc_list[0].bneck_flag = 0;  //to fix compilation error, better add sc_list inside backpressure define
        ret = nf_id;
#endif
        return ret;
}
int
onvm_clear_all_entries_for_bottleneck(uint16_t nf_id) {
        int ret = 0;
#ifdef ENABLE_NF_BASED_BKPR_MARKING

#ifdef ENABLE_GLOBAL_BACKPRESSURE
        /*** Note: adding this global is expensive (around 1.5Mpps drop) and better to remove the default chain Backpressure feature.. or do int inside default chain usage some way */
        /** global single chain scenario:: Note: This only works for the default chain case where service ID of chain is always in increasing order **/
        if(global_bkpr_mode) {
                if (downstream_nf_overflow) {
                        struct onvm_nf *cl = &nfs[nf_id];
                        // If service id is of any downstream that is/are bottlenecked then "move the lowest literally to next higher number" and when it is same as highsest reset bottlenext flag to zero
                        //  if(rte_ring_count(cl->rx_q) < CLIENT_QUEUE_RING_WATER_MARK_SIZE) {
                        //if(rte_ring_count(cl->rx_q) < CLIENT_QUEUE_RING_LOW_WATER_MARK_SIZE)
                        {
                                if (TEST_BIT(highest_downstream_nf_service_id, cl->info->service_id)) { //if (cl->info->service_id == highest_downstream_nf_service_id) {
                                        CLEAR_BIT(highest_downstream_nf_service_id, cl->info->service_id);
                                        if (highest_downstream_nf_service_id == 0) {
                                                downstream_nf_overflow = 0;
                                        }
                                }
                        }
                }
                return 0;
        }
#endif //ENABLE_GLOBAL_BACKPRESSURE

        uint32_t bneck_chains = 0;
        uint32_t ttl_chains = extract_active_service_chains(&bneck_chains, sc_list,SDN_FT_ENTRIES);

        //There must be chains with bottleneck indications
        if(ttl_chains && bneck_chains) {
                uint32_t s_inx = 0;
                for(s_inx=0; s_inx <SDN_FT_ENTRIES; s_inx++) {
                        if(NULL == sc_list[s_inx].sc) break;    //reached end of chains list
                        if(sc_list[s_inx].bneck_flag) {
                                int i =0;
                                for(i=1;i<=sc_list[s_inx].sc->chain_length;++i) {
#ifdef ENABLE_NFV_RESL
                                        //if resiliency is enabled: then also need to check mirrored status of active/standby NF if it is alive.
                                        uint16_t alt_nf_id = get_associated_active_or_standby_nf_id(nf_id);
                                        if((nf_id == sc_list[s_inx].sc->nf_instance_id[i]) ||(alt_nf_id == sc_list[s_inx].sc->nf_instance_id[i]) ) {
#else
                                        if(nf_id == sc_list[s_inx].sc->nf_instance_id[i]) {
#endif
                                                //clear this sc with this index;;
                                                if((TEST_BIT(sc_list[s_inx].sc->highest_downstream_nf_index_id, i))) {
                                                        CLEAR_BIT(sc_list[s_inx].sc->highest_downstream_nf_index_id, i);
                                                        //break;
#ifdef NF_BACKPRESSURE_APPROACH_2
                                                        // detect the start nf_index based on new val of highest_downstream_nf_index_id
                                                        int nf_index=(sc_list[s_inx].sc->highest_downstream_nf_index_id == 0)? (1): (get_index_of_highest_set_bit(sc_list[s_inx].sc->highest_downstream_nf_index_id));
                                                        for(; nf_index < i; nf_index++) {
                                                                if(nfs[sc_list[s_inx].sc->nf_instance_id[nf_index]].throttle_this_upstream_nf) {
                                                                        nfs[sc_list[s_inx].sc->nf_instance_id[nf_index]].throttle_this_upstream_nf=0;
                                                                }
                                                        }
#endif  //NF_BACKPRESSURE_APPROACH_2
                                                }
                                        }
                                }
                        }
                }
        }
#else
        ret = nf_id;
#endif
        return ret;
}

int enqueu_nf_to_bottleneck_watch_list(uint16_t nf_id) {
#ifdef USE_BKPR_V2_IN_TIMER_MODE
        //IF already marked then return
        if(bottleneck_nf_list.nf[nf_id].enqueue_status) return 1;

        bottleneck_nf_list.nf[nf_id].enqueue_status = BOTTLENECK_NF_STATUS_WAIT_ENQUEUED;
        bottleneck_nf_list.nf[nf_id].nf_id = nf_id;
        onvm_util_get_cur_time(&bottleneck_nf_list.nf[nf_id].s_time);
        bottleneck_nf_list.nf[nf_id].enqueued_ctr+=1;
        bottleneck_nf_list.entires++;
#ifdef ENABLE_NFV_RESL
        //if resiliency is enabled: then also need to mirror the status to active/standby NF if it is alive.
        uint16_t alt_nf_id = get_associated_active_or_standby_nf_id(nf_id);
        {//if(onvm_nf_is_valid(&nfs[alt_nf_id])) {
                bottleneck_nf_list.nf[alt_nf_id].enqueue_status = BOTTLENECK_NF_STATUS_WAIT_ENQUEUED;
                bottleneck_nf_list.nf[alt_nf_id].nf_id = alt_nf_id;
                onvm_util_get_cur_time(&bottleneck_nf_list.nf[alt_nf_id].s_time);
                bottleneck_nf_list.nf[alt_nf_id].enqueued_ctr+=1;
                bottleneck_nf_list.entires++;
        }
#endif
        return 0;
#else
        return nf_id;
#endif
}
//TODO: Still there is a possible corner case: when NF is already marked; and then the corresponding active or standby NF is started, then it wont get the marked status.
int dequeue_nf_from_bottleneck_watch_list(uint16_t nf_id) {
#ifdef USE_BKPR_V2_IN_TIMER_MODE
        //if(!bottleneck_nf_list.nf[nf_id].enqueue_status) return 1;    //Note: changed for resiliency case
         {
                bottleneck_nf_list.nf[nf_id].enqueue_status = BOTTLENECK_NF_STATUS_RESET;
                bottleneck_nf_list.nf[nf_id].enqueued_ctr-=1;
                bottleneck_nf_list.entires--;
                if(nfs[nf_id].is_bottleneck) nfs[nf_id].is_bottleneck = 0;
        }
#ifdef ENABLE_NFV_RESL
        //if resiliency is enabled: then also need to mirror the status to active/standby NF if it is alive.
        uint16_t alt_nf_id = get_associated_active_or_standby_nf_id(nf_id);
        //if(bottleneck_nf_list.nf[alt_nf_id].enqueue_status)
        { //if((onvm_nf_is_valid(&nfs[alt_nf_id]))&&(bottleneck_nf_list.nf[alt_nf_id].enqueue_status)) {
                bottleneck_nf_list.nf[alt_nf_id].enqueue_status = BOTTLENECK_NF_STATUS_RESET;
                bottleneck_nf_list.nf[alt_nf_id].enqueued_ctr-=1;
                bottleneck_nf_list.entires--;
                if(nfs[alt_nf_id].is_bottleneck) nfs[alt_nf_id].is_bottleneck = 0;
        }
#endif
        return 0;
#else
        return nf_id;
#endif
}
#if defined(USE_BKPR_V2_IN_TIMER_MODE)
static inline void mark_nf_backpressure_from_bottleneck_watch_list(uint16_t nf_id);
static inline void mark_nf_backpressure_from_bottleneck_watch_list(uint16_t nf_id) {
        bottleneck_nf_list.nf[nf_id].enqueue_status = BOTTLENECK_NF_STATUS_DROP_MARKED;
#if defined (BACKPRESSURE_EXTRA_DEBUG_LOGS)
        bottleneck_nf_list.nf[nf_id].marked_ctr+=1;
        nfs[nf_id].stats.bkpr_count++;
#endif

#ifdef ENABLE_NFV_RESL
        bottleneck_nf_list.nf[get_associated_active_or_standby_nf_id(nf_id)].enqueue_status = BOTTLENECK_NF_STATUS_DROP_MARKED;
#endif
}
#endif //USE_BKPR_V2_IN_TIMER_MODE

int check_and_enqueue_or_dequeue_nfs_from_bottleneck_watch_list(void) {
#if defined(USE_BKPR_V2_IN_TIMER_MODE)
        int ret = 0;
        uint16_t nf_id = 0;
        onvm_time_t now;
        onvm_util_get_cur_time(&now);
        struct onvm_nf *cl = NULL;
        for(; nf_id < MAX_NFS; nf_id++) {
                cl = &nfs[nf_id];
                if(cl->is_bottleneck) {
                        if(rte_ring_count(nfs[nf_id].rx_q) < CLIENT_QUEUE_RING_LOW_WATER_MARK_SIZE) {
                                onvm_clear_all_entries_for_bottleneck(nf_id);
                                dequeue_nf_from_bottleneck_watch_list(nf_id);
                        }
                        //ring count is still beyond the water mark threshold: change to drop state only if timer expired; else continue to monitor
                        else if(rte_ring_count(nfs[nf_id].rx_q) >= CLIENT_QUEUE_RING_WATER_MARK_SIZE) {
                                if((0 == WAIT_TIME_BEFORE_MARKING_OVERFLOW_IN_US)||((WAIT_TIME_BEFORE_MARKING_OVERFLOW_IN_US) < onvm_util_get_difftime_us(&bottleneck_nf_list.nf[nf_id].s_time, &now))) {
                                        mark_nf_backpressure_from_bottleneck_watch_list(nf_id);
                                        onvm_mark_all_entries_for_bottleneck(nf_id);
                                }
                                //else //time has not expired.. continue to monitor..
                        }
                }
#if 0
                //no action if status is clear!
                if(BOTTLENECK_NF_STATUS_RESET == bottleneck_nf_list.nf[nf_id].enqueue_status) {
                        continue;
                }
                //is in enqueue list AND marked: then reset only when rx_q recedes below low mark.
                else if (BOTTLENECK_NF_STATUS_DROP_MARKED & bottleneck_nf_list.nf[nf_id].enqueue_status) {
                        if(rte_ring_count(nfs[nf_id].rx_q) < CLIENT_QUEUE_RING_LOW_WATER_MARK_SIZE) {
                                onvm_clear_all_entries_for_bottleneck(nf_id);
                                dequeue_nf_from_bottleneck_watch_list(nf_id);
                        }
                        //else keep as marked.
                }
                //is in enqueue list but not yet marked(
                else if(BOTTLENECK_NF_STATUS_WAIT_ENQUEUED & bottleneck_nf_list.nf[nf_id].enqueue_status) {
                        //ring count is still beyond the water mark threshold: change to drop state only if timer expired; else continue to monitor
                        if(rte_ring_count(nfs[nf_id].rx_q) >= CLIENT_QUEUE_RING_WATER_MARK_SIZE) {
                                if((0 == WAIT_TIME_BEFORE_MARKING_OVERFLOW_IN_US)||((WAIT_TIME_BEFORE_MARKING_OVERFLOW_IN_US) <= onvm_util_get_difftime_us(&bottleneck_nf_list.nf[nf_id].s_time, &now))) {
                                        mark_nf_backpressure_from_bottleneck_watch_list(nf_id);
                                        onvm_mark_all_entries_for_bottleneck(nf_id);
                                }
                                //else //time has not expired.. continue to monitor..
                        }
                        //ring count has dropped below the low threshold then regardless of timer dequeue and clear (not necessary as it is not still marked) the bottleneck and back pressure marks
                        else  if(rte_ring_count(nfs[nf_id].rx_q) < CLIENT_QUEUE_RING_LOW_WATER_MARK_SIZE) {
                                dequeue_nf_from_bottleneck_watch_list(nf_id);
                        }
                        //else{} //ring count is between the High and Low threshold; then ? keep it list and continue to monitor
                }
#endif
        }
        return ret;
#else
        return 0;
#endif //USE_BKPR_V2_IN_TIMER_MODE
}
