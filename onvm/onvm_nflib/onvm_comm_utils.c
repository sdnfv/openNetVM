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
 * onvm_pkt_helper.c - packet helper routines
 ********************************************************************/

#include "onvm_comm_utils.h"
//#include "onvm_common.h"
#include <rte_branch_prediction.h>
#include <rte_mbuf.h>
#include <rte_ether.h>
#include <rte_ethdev.h>
#include <rte_ip.h>
#include <rte_tcp.h>
#include <rte_udp.h>

#ifdef HAS_CLOCK_GETTIME_MONOTONIC
  struct timespec start, stop;
  struct timespec gstart, gstop;
#else
  struct timeval start, stop;
  struct timeval gstart, gstop;
#endif


inline int onvm_util_get_cur_time(onvm_time_t* ct) {
#ifdef HAS_CLOCK_GETTIME_MONOTONIC
        if (clock_gettime(USE_THIS_CLOCK, (struct timespec *) &ct->t) == -1) {
                perror("clock_gettime");
                return 1;
        }
#else
        if (gettimeofday(&ct->t, NULL) == -1) {
                perror("gettimeofday");
                return 1;
        }
#endif
        return 0;
}


inline int onvm_util_get_start_time(onvm_interval_timer_t* ct) {
#ifdef HAS_CLOCK_GETTIME_MONOTONIC
        if (clock_gettime(USE_THIS_CLOCK, &ct->ts.t) == -1) {
                perror("clock_gettime");
                return 1;
        }
#else
        if (gettimeofday(&ct->ts.t, NULL) == -1) {
                perror("gettimeofday");
                return 1;
        }
#endif
        return 0;
}

inline int onvm_util_get_stop_time(onvm_interval_timer_t* ct) {
#ifdef HAS_CLOCK_GETTIME_MONOTONIC
        if (clock_gettime(USE_THIS_CLOCK, &ct->tp.t) == -1) {
                perror("clock_gettime");
                return 1;
        }
#else
        if (gettimeofday(&ct->tp.t, NULL) == -1) {
                perror("gettimeofday");
                return 1;
        }
#endif
        return 0;
}

inline int64_t onvm_util_get_elapsed_time(onvm_interval_timer_t* ct) {
        onvm_util_get_stop_time(ct);
#ifdef HAS_CLOCK_GETTIME_MONOTONIC
        int64_t delta = ((ct->tp.t.tv_sec - ct->ts.t.tv_sec) * 1000000000
                        + (ct->tp.t.tv_nsec - ct->ts.t.tv_nsec));
#else
        int64_t delta = (ct->tp.t.tv_sec - ct->ts.t.tv_sec) * 1000000000 +
        (ct->tp.t.tv_usec - ct->ts.t.tv_usec) * 1000;
#endif
        return delta;
}

inline int64_t onvm_util_get_diff_time_now(onvm_time_t* cs) {
        onvm_time_t cp;
        onvm_util_get_cur_time(&cp);
#ifdef HAS_CLOCK_GETTIME_MONOTONIC
        int64_t delta = ((cp.t.tv_sec - cs->t.tv_sec) * 1000000000
                        + (cp.t.tv_nsec - cs->t.tv_nsec));
#else
        int64_t delta = (cp.t.tv_sec - cs->t.tv_sec) * 1000000000 +
        (cp.t.tv_usec - cs->t.tv_usec) * 1000;
#endif
        return delta;
}

inline unsigned long onvm_util_get_difftime_us(onvm_time_t *cs, onvm_time_t *cp) {
#ifdef HAS_CLOCK_GETTIME_MONOTONIC
        unsigned long delta = ((cp->t.tv_sec - cs->t.tv_sec) * SECOND_TO_MICRO_SECOND
                        + ((cp->t.tv_nsec - cs->t.tv_nsec)/1000));
#else
        int64_t delta = (cp->t.tv_sec - cs->t.tv_sec) * SECOND_TO_MICRO_SECOND +
        (cp->t.tv_usec - cs->t.tv_usec);
#endif
        return delta;
}

//inline 
uint64_t onvm_util_get_current_cpu_cycles(void) {
        return rte_rdtsc_precise();
}

//inline 
uint64_t onvm_util_get_diff_cpu_cycles(uint64_t start, uint64_t end) {
        if(end > start) {
                return (uint64_t) (end -start);
        }
        return 0;
}

//inline 
uint64_t onvm_util_get_diff_cpu_cycles_in_us(uint64_t start, uint64_t end) {
        if(end > start) {
                return (uint64_t) (((end -start)*SECOND_TO_MICRO_SECOND)/rte_get_tsc_hz());
        }
        return 0;
}

//inline 
uint64_t onvm_util_get_elapsed_cpu_cycles(uint64_t start) {
        return onvm_util_get_diff_cpu_cycles(start, onvm_util_get_current_cpu_cycles());
}

//inline 
uint64_t onvm_util_get_elapsed_cpu_cycles_in_us(uint64_t start) {
        return onvm_util_get_diff_cpu_cycles_in_us(start, onvm_util_get_current_cpu_cycles());
}

#if (USE_TS_TYPE == TS_TYPE_LOGICAL)
uint64_t gPkts=0;
#endif
#if (USE_TS_TYPE == TS_TYPE_SYS_CLOCK)
#define USE_DISTINCT_FIELDS
#endif
//inline 
int onvm_util_mark_timestamp_on_RX_packets(struct rte_mbuf **pkts, uint16_t nb_pkts) {
        unsigned i;
#if (USE_TS_TYPE == TS_TYPE_LOGICAL)
        uint64_t now = gPkts++;
#elif (USE_TS_TYPE == TS_TYPE_CPU_CYCLES)
        uint64_t now = onvm_util_get_current_cpu_cycles();
#else //(USE_TS_TYPE == TS_TYPE_SYS_CLOCK)
        onvm_time_t now;
        onvm_util_get_cur_time(&now);
#endif

            for (i = 0; i < nb_pkts; i++) {
#if (USE_TS_TYPE == TS_TYPE_LOGICAL) || (USE_TS_TYPE == TS_TYPE_CPU_CYCLES)
                pkts[i]->ol_flags = now;
#else
                //uint64_t ns = req->tv_sec * 1000000000 + req->tv_nsec;
                pkts[i]->ol_flags = (uint64_t) (now.t.tv_sec * 1000000000 + now.t.tv_sec);
#if USE_DISTINCT_FILEDS
                pkts[i]->ol_flags = now.t.tv_nsec;
                pkts[i]->tx_offload = now.t.tv_sec;
#endif
#endif
            }
        return 0;
}
static struct {
    uint64_t total_cycles;
    uint64_t total_pkts;
} latency_numbers;
//inline 
int onvm_util_calc_chain_processing_latency(struct rte_mbuf **pkts, uint16_t nb_pkts) {

#if (USE_TS_TYPE == TS_TYPE_LOGICAL)
        uint64_t now = gPkts;
#elif (USE_TS_TYPE == TS_TYPE_CPU_CYCLES)
        uint64_t now = onvm_util_get_current_cpu_cycles();
#else
        //onvm_time_t stop, start;
        onvm_time_t stop;
        onvm_util_get_cur_time(&stop);
        uint64_t now = stop.t.tv_sec * 1000000000 + stop.t.tv_nsec;
        //single U64 precision (tv_nsec) is not good; often results in wrong values
        //uint64_t now = onvm_util_get_current_cpu_cycles();
        //uint64_t now = stop.t.tv_nsec;
#endif
        uint64_t cycles = 0;
        unsigned i;

        for (i = 0; i < nb_pkts; i++) {
#if (USE_TS_TYPE == TS_TYPE_LOGICAL)
                cycles += now - pkts[i]->ol_flags;
#elif (USE_TS_TYPE == TS_TYPE_CPU_CYCLES)
                cycles += now - pkts[i]->ol_flags;
#else
                //cycles += now - pkts[i]->ol_flags;
#if USE_DISTINCT_FIELDS
                start.t.tv_sec = pkts[i]->tx_offload;
                start.t.tv_nsec = pkts[i]->ol_flags;
                cycles += ((stop.t.tv_sec - start.t.tv_sec) * 1000000000
                        + (stop.t.tv_nsec - start.t.tv_nsec));
#else
                cycles += now - pkts[i]->ol_flags;
#endif
#endif
                pkts[i]->tx_offload=pkts[i]->ol_flags=0;
        }

        latency_numbers.total_cycles += cycles;
        latency_numbers.total_pkts += nb_pkts;
        uint64_t latency = latency_numbers.total_cycles / latency_numbers.total_pkts;

        if (latency_numbers.total_pkts > (25 * 1000 * 1000ULL)) {
#if (USE_TS_TYPE == TS_TYPE_LOGICAL)
                printf("Latency = %"PRIu64" Logical clock  (%"PRIu64"usec)\n",
                      latency,(((latency)*SECOND_TO_MICRO_SECOND)/TS_RX_PACKET_RATE));
#elif (USE_TS_TYPE == TS_TYPE_CPU_CYCLES)
                printf("Latency = %"PRIu64" CPU cycles  (%"PRIu64"usec)\n",
                      latency,(((latency)*SECOND_TO_MICRO_SECOND)/rte_get_tsc_hz()));
#else
                printf("Latency = %"PRIu64" ns (%"PRIu64" usec\t %"PRIu64" ticks)\n",
                      latency,((latency/1000)), (latency*rte_get_tsc_hz())/1000000000);
#endif
                latency_numbers.total_cycles = latency_numbers.total_pkts = 0;
        }
        return 0;
}

//inline 
int onvm_util_get_marked_packet_timestamp(struct rte_mbuf **pkts, uint64_t *ts_info, uint16_t nb_pkts) {
        unsigned i;
        for (i = 0; i < nb_pkts; i++) {
#if (USE_TS_TYPE == TS_TYPE_LOGICAL)
                ts_info[i] = pkts[i]->ol_flags;
#elif (USE_TS_TYPE == TS_TYPE_CPU_CYCLES)
                ts_info[i] = pkts[i]->ol_flags;
#else
                //pkts[i]->tx_offload; pkts[i]->ol_flags;
                ts_info[i] = pkts[i]->tx_offload;
#endif
        }
        return 0;
}
