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
                          onvm_stats.c

   This file contain the implementation of all functions related to
   statistics display in the manager.

******************************************************************************/

#include <unistd.h>
#include <time.h>
#include <stdio.h>

#include "onvm_mgr.h"
#include "onvm_stats.h"
#include "onvm_nf.h"
#ifdef ENABLE_BFD
#include "onvm_bfd.h"
#endif
#ifdef ENABLE_REMOTE_SYNC_WITH_TX_LATCH
#include "onvm_rsync.h"
#endif
/************************Internal Functions Prototypes************************/
/*
 * Function displaying statistics for all NFs
 *
 */
#define onvm_stats_display_nfs(x,y) onvm_stats_display_clients(x,y)


/*
 * Flush the file streams we're writing stats to
 */
static void
onvm_stats_flush(void);

/*
 * Truncate the files back to being empty, so each iteration we reset the data
 */
static void
onvm_stats_truncate(void);

/*
 * Free and reset the cJSON variables to build new json string with
 */
static void
onvm_json_reset_objects(void);


void onvm_stats_display_mode(unsigned difftime);
/*********************Stats Output Streams************************************/

static FILE *stats_out;
static FILE *json_stats_out;


/****************************Global variables***************************************/

/* Holds current timestamp, might want to make this not global */
char buffer[20];

/****************************Interfaces***************************************/

void
onvm_stats_init(uint8_t verbocity_level) {
        if (verbocity_level == ONVM_RAW_STATS_DUMP) {
                printf("#YYYY-MM-DD HH:MM:SS,nic_rx_pkts,nic_rx_pps,nic_tx_pkts,nic_tx_pps\n");
                printf("#YYYY-MM-DD HH:MM:SS,instance_id,service_id,rx,tx,rx_pps,tx_pps,rx_drop,tx_drop,rx_drop_rate,tx_drop_rate,act_out,act_tonf,act_drop,act_next,act_buffer,act_returned\n");
        }
}

void
onvm_stats_set_output(ONVM_STATS_OUTPUT output) {
        if (output != ONVM_STATS_NONE) {
                switch (output) {
                        case ONVM_STATS_STDOUT:
                                stats_out = stdout;
                                break;
                        case ONVM_STATS_STDERR:
                                stats_out = stderr;
                                break;
                        case ONVM_STATS_WEB:
                                stats_out = fopen(ONVM_STATS_FILE, ONVM_STATS_FOPEN_ARGS);
                                json_stats_out = fopen(ONVM_JSON_STATS_FILE, ONVM_STATS_FOPEN_ARGS);

                                if (stats_out == NULL || json_stats_out == NULL) {
                                        rte_exit(-1, "Error opening stats files\n");
                                }

                                onvm_json_root = NULL;
                                onvm_json_port_stats_arr = NULL;
                                onvm_json_nf_stats_arr = NULL;
                                break;
                        default:
                                rte_exit(-1, "Error handling stats output file\n");
                                break;
                }
        }
}

void
onvm_stats_cleanup(void) {
        if (stats_destination == ONVM_STATS_WEB) {
                fclose(stats_out);
                fclose(json_stats_out);
        }
}

#ifdef ENABLE_REMOTE_SYNC_WITH_TX_LATCH
void onvm_stats_display_rsync_tx_thread_stats(void);
void onvm_stats_display_rsync_tx_thread_stats(void) {
        int j = 0;
        fprintf(stats_out, "Remote Sync rings\n");
        fprintf(stats_out,"-----------------\n");
        for(j=0; j < MIN(ports->num_ports, ONVM_NUM_RSYNC_PORTS); j++) {
                fprintf(stats_out, "Idx:%d, Tx_port_Ring:%d, TX_Latch_Ring:%d, NF_Latch_Ring:%d", j, rte_ring_count(tx_port_ring[j]), rte_ring_count(tx_tx_state_latch_ring[j]), rte_ring_count(tx_nf_state_latch_ring[j]));
#ifdef ENABLE_RSYNC_WITH_DOUBLE_BUFFERING_MODE
#ifdef ENABLE_RSYNC_MULTI_BUFFERING
                int k = 0;
                for(k=0; k < ENABLE_RSYNC_MULTI_BUFFERING; k++)
                        fprintf(stats_out, "DB[%d]: tx_ts_latch_ring:%d, nf_latch_ring:%d\n",k,rte_ring_count(tx_tx_state_latch_db_ring[k][j]), rte_ring_count(tx_nf_state_latch_db_ring[k][j]));
#else
                fprintf(stats_out, "DB: tx_ts_latch_ring:%d, nf_latch_ring:%d\n",rte_ring_count(tx_tx_state_latch_db_ring[j]), rte_ring_count(tx_nf_state_latch_db_ring[j]));
#endif
#endif
        }
}
#endif
void
onvm_stats_display_all(unsigned difftime, uint8_t verbosity_level) {
        if (stats_out == stdout) {
                if (verbosity_level != ONVM_RAW_STATS_DUMP)
                        onvm_stats_clear_terminal();
        } else {
                onvm_stats_truncate();
                onvm_json_reset_objects();
        }
       onvm_stats_clear_terminal();
       onvm_stats_display_ports(difftime, verbosity_level);
       onvm_stats_display_mode(difftime);
       onvm_stats_display_clients(difftime);
       onvm_stats_display_chains(difftime);
       if (stats_out != stdout && stats_out != stderr) {
              fprintf(stats_out,"%s\n", cJSON_Print(onvm_json_root));
       }
       onvm_stats_flush();
#ifdef ENABLE_REMOTE_SYNC_WITH_TX_LATCH
       onvm_stats_display_rsync_tx_thread_stats();
#endif
#ifdef ENABLE_BFD
       onvm_print_bfd_status(difftime, stats_out);
#endif
#ifdef ENABLE_REMOTE_SYNC_WITH_TX_LATCH
       onvm_print_rsync_stats(difftime, stats_out);
#endif
}


void
onvm_stats_clear_all_clients(void) {
        unsigned i;

        for (i = 0; i < MAX_NFS; i++) {
                nfs[i].stats.rx = nfs[i].stats.rx_drop = 0;
#ifdef ENABLE_NF_TX_STAT_LOGS
                nfs[i].stats.act_drop = nfs[i].stats.act_tonf = 0;
                nfs[i].stats.act_next = nfs[i].stats.act_out = 0;
#endif
        }
}

void
onvm_stats_clear_client(uint16_t id) {
        nfs[id].stats.rx = nfs[id].stats.rx_drop = 0;
#ifdef ENABLE_NF_TX_STAT_LOGS
        nfs[id].stats.act_drop = nfs[id].stats.act_tonf = 0;
        nfs[id].stats.act_next = nfs[id].stats.act_out = 0;
#endif
}


/****************************Internal functions*******************************/
#define USE_EXTENDED_PORT_STATS
#ifdef USE_EXTENDED_PORT_STATS
static int 
get_port_stats_rate(double period_time)
{
        int i;
        uint64_t port_rx_rate=0, port_rx_err_rate=0, port_imissed_rate=0, \
                port_rx_nombuf_rate=0, port_tx_rate=0, port_tx_err_rate=0;

        struct rte_eth_stats port_stats[ports->num_ports];
        static struct rte_eth_stats port_prev_stats[5];
        for (i = 0; i < ports->num_ports; i++){
                rte_eth_stats_get(ports->id[i], &port_stats[i]);
                fprintf(stats_out,"Port:%"PRIu8", rx:%"PRIu64", rx_err:%"PRIu64", rx_imissed:%"PRIu64" "
                //        "ibadcrc:%"PRIu64", ibadlen:%"PRIu64", illerrc:%"PRIu64", errbc:%"PRIu64" "
                        "rx_nombuf:%"PRIu64", tx:%"PRIu64", tx_err:%"PRIu64"",
                        ports->id[i], port_stats[i].ipackets, port_stats[i].ierrors, port_stats[i].imissed,
                        //port_stats[i].ibadcrc, port_stats[i].ibadlen, port_stats[i].illerrc, port_stats[i].errbc,
                        port_stats[i].rx_nombuf, port_stats[i].opackets,
                        port_stats[i].oerrors);

                port_rx_rate = (port_stats[i].ipackets - port_prev_stats[i].ipackets) / period_time;
                port_rx_err_rate = (port_stats[i].ierrors - port_prev_stats[i].ierrors) / period_time;
                port_imissed_rate = (port_stats[i].imissed - port_prev_stats[i].imissed) / period_time;
                port_rx_nombuf_rate = (port_stats[i].rx_nombuf - port_prev_stats[i].rx_nombuf) / period_time;
                port_tx_rate = (port_stats[i].opackets - port_prev_stats[i].opackets) / period_time;
                port_tx_err_rate = (port_stats[i].oerrors - port_prev_stats[i].oerrors) / period_time;

                fprintf(stats_out,"Port:%"PRIu8", rx_rate:%"PRIu64", rx_err_rate:%"PRIu64", rx_imissed_rate:%"PRIu64" "
                        "rx_nombuf_rate:%"PRIu64", tx_rate:%"PRIu64", tx_err_rate:%"PRIu64"\n",
                        ports->id[i], port_rx_rate, port_rx_err_rate, port_imissed_rate,
                        port_rx_nombuf_rate, port_tx_rate, port_tx_err_rate);

                port_prev_stats[i].ipackets = port_stats[i].ipackets;
                port_prev_stats[i].ierrors = port_stats[i].ierrors;
                port_prev_stats[i].imissed = port_stats[i].imissed;
                port_prev_stats[i].rx_nombuf = port_stats[i].rx_nombuf;
                port_prev_stats[i].opackets = port_stats[i].opackets;
                port_prev_stats[i].oerrors = port_stats[i].oerrors;
        }

        return 0;
}
#endif

void onvm_stats_display_mode(__attribute__((unused)) unsigned difftime) {
        fprintf(stats_out, "ONVM MODE\n");
        fprintf(stats_out, "---------\n");

#ifdef ENABLE_NF_BACKPRESSURE
        fprintf(stats_out, "NFVNICE:[ON]\t ");
        fprintf(stats_out, "---------\n");
#endif

#ifdef ENABLE_NFV_RESL
        fprintf(stats_out, "RESILIENCY:[ON]\t ");
#ifdef MIMIC_PICO_REP
        fprintf(stats_out, "PICO_REP:\t ");
#ifdef ENABLE_PICO_STATELESS_MODE
        fprintf(stats_out, " STATELESS_MODE: \n");
#else
        fprintf(stats_out,"\n");
#endif

#elif defined(MIMIC_FTMB)
        fprintf(stats_out, "FTMB \n");
#else
        fprintf(stats_out, "REINFORCE: ");
#ifdef ENABLE_REPLICA_STATE_UPDATE
        fprintf(stats_out, "LCL_REP=ON, ");
#endif

#ifdef ENABLE_REMOTE_SYNC_WITH_TX_LATCH
        fprintf(stats_out, "REM_REP=ON: ");
#endif
#ifdef ENABLE_PER_FLOW_TS_STORE
        fprintf(stats_out, "TxTS=ON: ");
#endif
#ifdef ENABLE_CHAIN_BYPASS_RSYNC_ISOLATION
        fprintf(stats_out, "ISOLAT=ON: ");
#endif
#ifdef ENABLE_NF_PAUSE_TILL_OUTSTANDING_NDSYNC_COMMIT
        fprintf(stats_out, "CORRECTENSS=ON: ");
#endif
#endif
#else
        fprintf(stats_out, "RESILIENCY:[OFF]\t ");
        fprintf(stats_out, "---------\n");
#endif

}

void
onvm_stats_display_ports(unsigned difftime, uint8_t verbosity_level) {
        unsigned i;
        uint64_t nic_rx_pkts = 0;
        uint64_t nic_tx_pkts = 0;
        uint64_t nic_rx_pps = 0;
        uint64_t nic_tx_pps = 0;
        uint64_t nic_tx_drop_pkts = 0;
        uint64_t nic_tx_drop_pps = 0;
        char* port_label = NULL;
		
		if (verbosity_level == ONVM_RAW_STATS_DUMP){};
        fprintf(stats_out, "PORTS\n");
        fprintf(stats_out,"-----\n");
        for (i = 0; i < ports->num_ports; i++)
                fprintf(stats_out,"Port %u: '%s'\t", (unsigned)ports->id[i],
                                onvm_stats_print_MAC(ports->id[i]));
        fprintf(stats_out,"\n");

        /* Arrays to store last TX/RX count to calculate rate */
        static uint64_t tx_last[RTE_MAX_ETHPORTS];
        static uint64_t tx_drop_last[RTE_MAX_ETHPORTS];
        static uint64_t rx_last[RTE_MAX_ETHPORTS];

        for (i = 0; i < ports->num_ports; i++) {
                nic_rx_pkts = ports->rx_stats.rx[ports->id[i]];
                nic_tx_pkts = ports->tx_stats.tx[ports->id[i]];

                nic_rx_pps = (nic_rx_pkts - rx_last[i]) / difftime;
                nic_tx_pps = (nic_tx_pkts - tx_last[i]) / difftime;

                nic_tx_drop_pkts = ports->tx_stats.tx_drop[ports->id[i]];
                nic_tx_drop_pps = (nic_tx_drop_pkts -tx_drop_last[i]) / difftime;

                fprintf(stats_out,"Port %u - rx: %9"PRIu64"  (%9"PRIu64" pps)    "
                                "tx: %9"PRIu64"  (%9"PRIu64" pps)    "
                                "tx_drop: %9"PRIu64"  (%9"PRIu64" pps) \n",
                                (unsigned)ports->id[i],
                                nic_rx_pkts,
                                nic_rx_pps,
                                nic_tx_pkts,
                                nic_tx_pps,
                                nic_tx_drop_pkts,
                                nic_tx_drop_pps
                                /*
                                ports->rx_stats.rx[ports->id[i]],
                                (ports->rx_stats.rx[ports->id[i]] - rx_last[i])
                                        /difftime,
                                        ports->tx_stats.tx[ports->id[i]],
                                (ports->tx_stats.tx[ports->id[i]] - tx_last[i])
                                        /difftime,
                                        ports->tx_stats.tx_drop[ports->id[i]],
                                (ports->tx_stats.tx_drop[ports->id[i]] - tx_drop_last[i])
                                        /difftime
                                 */
                                );
                /* Only print this information out if we haven't already printed it to the console above */
                if (stats_out != stdout && stats_out != stderr) {
                        ONVM_SNPRINTF(port_label, 8, "Port %d", i);

                        cJSON_AddItemToArray(onvm_json_port_stats_arr,
                                             onvm_json_port_stats[i] = cJSON_CreateObject());
                        cJSON_AddStringToObject(onvm_json_port_stats[i], "Label", port_label);
                        cJSON_AddNumberToObject(onvm_json_port_stats[i], "RX", nic_rx_pps);
                        cJSON_AddNumberToObject(onvm_json_port_stats[i], "TX", nic_tx_pps);

                        free(port_label);
                        port_label = NULL;
                }

                rx_last[i] = ports->rx_stats.rx[ports->id[i]];
                tx_last[i] = ports->tx_stats.tx[ports->id[i]];
                tx_drop_last[i] = ports->tx_stats.tx_drop[ports->id[i]];
        }
        //#else
        get_port_stats_rate(difftime);
        //#endif
}


void
onvm_stats_display_clients(__attribute__((unused)) unsigned difftime) {
        unsigned i=0;
        char* nf_label = NULL;
        /* Arrays to store last TX/RX count for NFs to calculate rate */
        static uint64_t nf_tx_last[MAX_NFS];
        static uint64_t nf_rx_last[MAX_NFS];

        /* Arrays to store last TX/RX pkts dropped for NFs to calculate drop rate */
        static uint64_t nf_tx_drop_last[MAX_NFS];
        static uint64_t nf_rx_drop_last[MAX_NFS];

#ifdef INTERRUPT_SEM

#ifdef ENABLE_NF_YIELD_NOTIFICATION_COUNTER
        /* Arrays to store last wake up count for NFs to calculate NFs wakeup rate */
        static uint64_t prev_nf_yield_count[MAX_NFS];
        uint64_t yield_rate = 0;
#endif

#ifdef ENABLE_NF_WAKE_NOTIFICATION_COUNTER
        static uint64_t prev_wkmgr_wakeup_count[MAX_NFS];
        uint64_t avg_pkts_per_wakeup = 0;
        uint64_t good_pkts_per_wakeup = 0;
#endif

#if defined (NF_BACKPRESSURE_APPROACH_1)
        /* Arrays to store last Rx PAckets dropped due to back-pressure for NFs to calculate backpressure drop rate */
        static uint64_t prev_bkpr_drop[MAX_NFS];
#endif
        uint64_t rx_qlen = 0;
        uint64_t tx_qlen = 0;
        uint64_t comp_cost= 0;
#endif

#ifdef ENABLE_GLOBAL_BACKPRESSURE
        fprintf(stats_out,"OverflowFlag=[%d], Highest_DS_SID=[%d], Lowest_DS_SID=[%d], Num Throttles=[%"PRIu64"] \n", downstream_nf_overflow, highest_downstream_nf_service_id, lowest_upstream_to_throttle, throttle_count);
#endif

#if 0
        static const char *NF_MSG[3];
        NF_MSG[0] = "\nNF IID / SID     rx_pps  /  tx_pps     rx_drop  /  tx_drop         out   /    tonf     /   drop\n-----------------------------------------------------------------------------------------------\n";
        NF_MSG[1] = "\nNF IID / SID     rx_pps  /  tx_pps            rx  /  tx                out   /    tonf     /   drop\n"
                      "               drop_pps  /  drop_pps     rx_drop  /  tx_drop           next  /    buf      /   ret\n---------------------------------------------------------------------------------------------------\n";
        NF_MSG[2] = "";
        fprintf(stats_out, "%s", NF_MSG[verbosity_level-1]);
#endif
		fprintf(stats_out,"-------\n");
        for (i = 0; i < MAX_NFS; i++) {
                if (!onvm_nf_is_valid(&nfs[i]))
                        continue;
                const uint64_t rx = nfs[i].stats.rx;
                const uint64_t rx_drop = nfs[i].stats.rx_drop;
                const uint64_t tx = nfs[i].stats.tx;
                const uint64_t tx_drop = nfs[i].stats.tx_drop;

                const uint64_t rx_pps = (rx - nf_rx_last[i])/difftime;
                const uint64_t tx_pps = (tx - nf_tx_last[i])/difftime;
                const uint64_t tx_drop_rate = (tx_drop - nf_tx_drop_last[i])/difftime;
                const uint64_t rx_drop_rate = (rx_drop - nf_rx_drop_last[i])/difftime;

                nf_rx_last[i] = nfs[i].stats.rx;
                nf_tx_last[i] = nfs[i].stats.tx;
                nf_rx_drop_last[i] = rx_drop;
                nf_tx_drop_last[i] = tx_drop;

#ifndef INTERRUPT_SEM
#ifdef ENABLE_NF_TX_STAT_LOGS
                const uint64_t act_drop = nfs[i].stats.act_drop;
                const uint64_t act_next = nfs[i].stats.act_next;
                const uint64_t act_out = nfs[i].stats.act_out;
                const uint64_t act_tonf = nfs[i].stats.act_tonf;
                const uint64_t act_buffer = nfs[i].stats.tx_buffer;
                const uint64_t act_returned = nfs[i].stats.tx_returned;

                fprintf(stats_out,"Client %2u - rx: %9"PRIu64" rx_drop: %9"PRIu64" next: %9"PRIu64" drop: %9"PRIu64" ret: %9"PRIu64"\n"
                                    "tx: %9"PRIu64" tx_drop: %9"PRIu64" out:  %9"PRIu64" tonf: %9"PRIu64" buf: %9"PRIu64"\n",
                                nfs[i].info->instance_id,
                                rx, rx_drop, act_next, act_drop, act_returned,
                                tx, tx_drop, act_out, act_tonf, act_buffer);
#else
                fprintf(stats_out,"Client %2u - rx: %9"PRIu64" rx_drop: %9"PRIu64" \n"
                                    "tx: %9"PRIu64" tx_drop: %9"PRIu64" \n",
                                nfs[i].info->instance_id, rx, rx_drop, tx, tx_drop );
#endif

#else

#ifdef ENABLE_NF_WAKE_NOTIFICATION_COUNTER
                /* periodic/rate specific statistics of NF instance */ 
                const uint64_t avg_wakeups = ( nfs[i].stats.wakeup_count -  prev_wkmgr_wakeup_count[i]);
                prev_wkmgr_wakeup_count[i]= nfs[i].stats.wakeup_count;
                if (avg_wakeups > 0 ) {
                        avg_pkts_per_wakeup = (tx_pps+tx_drop_rate)/avg_wakeups;
                        good_pkts_per_wakeup = (tx_pps)/avg_wakeups;
                } else {
                        avg_pkts_per_wakeup=0;
                        good_pkts_per_wakeup=0;
                }
#endif

#ifdef ENABLE_NF_YIELD_NOTIFICATION_COUNTER
                const uint64_t yields =  (nfs[i].stats.yield_count - prev_nf_yield_count[i]);
                prev_nf_yield_count[i] = nfs[i].stats.yield_count;
                if(yields) {
                        yield_rate = (tx_pps)/yields;
                }
#endif

                rx_qlen = rte_ring_count(nfs[i].rx_q);
                tx_qlen = rte_ring_count(nfs[i].tx_q);
                comp_cost = nfs[i].stats.comp_cost;


                fprintf(stats_out,"NF Inst %2u:[Svc:%2u] [STS:%2u], comp_cost=%"PRIu64", msg_flag(blocked)=%d,"
#ifdef ENABLE_NF_WAKE_NOTIFICATION_COUNTER
                "avg_wakeups=%"PRIu64", avg_ppw=%"PRIu64", avg_good_ppw=%"PRIu64""
#endif
#ifdef ENABLE_NF_WAKE_NOTIFICATION_COUNTER
                "yields=%"PRIu64", pkts_per_yield=%"PRIu64""
#endif
                "rx_rate=%"PRIu64", rx_drop_rate=%"PRIu64", rx_qlen=%"PRIu64", rx_drop=%"PRIu64""
                "tx_rate=%"PRIu64", tx_drop_rate=%"PRIu64", tx_qlen=%"PRIu64", tx_drop=%"PRIu64"\n" //"next: %9"PRIu64" drop: %9"PRIu64" ret: %9"PRIu64", out: %9"PRIu64" tonf: %9"PRIu64" buf: %9"PRIu64"\n",
                ,nfs[i].info->instance_id, nfs[i].info->service_id, nfs[i].info->status, comp_cost,rte_atomic16_read(nfs[i].shm_server),
#ifdef ENABLE_NF_WAKE_NOTIFICATION_COUNTER
                avg_wakeups, avg_pkts_per_wakeup, good_pkts_per_wakeup,
#endif
#ifdef ENABLE_NF_YIELD_NOTIFICATION_COUNTER
                yields, yield_rate,
#endif
                rx_pps,rx_drop_rate, rx_qlen,rx_drop,
                tx_pps,tx_drop_rate, tx_qlen,tx_drop //,act_next, act_drop, act_returned, act_out, act_tonf, act_buffer
                );
#endif

                //fprintf(stats_out,"rx_overflow=[%d], ThrottleNF_Flag=[%d], Highest_DS_SID=[%d] NF_Throttle_count=[%"PRIu64"], \n", nfs[i].rx_buffer_overflow, nfs[i].throttle_this_upstream_nf, nfs[i].highest_downstream_nf_index_id, nfs[i].throttle_count);
#ifdef NF_BACKPRESSURE_APPROACH_1
                fprintf(stats_out,"bkpr_drop=%"PRIu64", bkpr_drop_rate=%"PRIu64"\n",nfs[i].stats.bkpr_drop,(nfs[i].stats.bkpr_drop - prev_bkpr_drop[i])/ difftime);
                prev_bkpr_drop[i] = nfs[i].stats.bkpr_drop;
#endif //NF_BACKPRESSURE_APPROACH_1

#ifdef NF_BACKPRESSURE_APPROACH_2
                fprintf(stats_out,"ThrottleNF_Flag=[%d], \n", nfs[i].throttle_this_upstream_nf);
#endif //NF_BACKPRESSURE_APPROACH_2


#ifdef USE_CGROUPS_PER_NF_INSTANCE
                fprintf(stats_out,"NF_Core_Id [%d], NF_comp cost=[%d], NF_CPU_SHARE=[%d]\n", nfs[i].info->core_id, nfs[i].info->comp_cost, nfs[i].info->cpu_share);
#endif //USE_CGROUPS_PER_NF_INSTANCE
                fprintf(stats_out,"\n");

                /* Only print this information out if we haven't already printed it to the console above */
                if (stats_out != stdout && stats_out != stderr) {
                       ONVM_SNPRINTF(nf_label, 6, "NF %d", i);

                       cJSON_AddItemToArray(onvm_json_nf_stats_arr,
                                            onvm_json_nf_stats[i] = cJSON_CreateObject());

                       cJSON_AddStringToObject(onvm_json_nf_stats[i],
                                               "Label", nf_label);
                       cJSON_AddNumberToObject(onvm_json_nf_stats[i],
                                               "RX", rx_pps);
                       cJSON_AddNumberToObject(onvm_json_nf_stats[i],
                                               "TX", tx_pps);
                       cJSON_AddNumberToObject(onvm_json_nf_stats[i],
                                               "TX_Drop_Rate", tx_drop_rate);
                       cJSON_AddNumberToObject(onvm_json_nf_stats[i],
                                               "RX_Drop_Rate", rx_drop_rate);

                       free(nf_label);
                       nf_label = NULL;
                }
#ifdef  __DEBUG_NDSYNC_LOGS__
                fprintf(stats_out,"(ND_SYNC Notification Delay): Min: %li\t Max:%li\t Avg: %li \n", nfs[i].info->min_nd, nfs[i].info->max_nd, nfs[i].info->avg_nd);
#endif
        }

        //if (verbosity_level == ONVM_RAW_STATS_DUMP)
        //        return;
#if 0
        if (print_total_stats) {
                fprintf(stats_out, "\nService id totals\n");
                fprintf(stats_out, "-----------------\n");
                for (i = 0; i < MAX_SERVICES; i++) {
                        uint16_t nfs_for_service = nf_per_service_count[i];
                        const char *nf_count = nfs_for_service==1 ? "NF " : "NFs";
                        if (nfs_for_service == 0)
                                continue;
                        if (verbosity_level == 2) {
                                fprintf(stats_out, "SID %-2u %2u%s - %9"PRIu64" / %-9"PRIu64"  %11"PRIu64" / %-11"PRIu64"  %11"PRIu64" / %-11"PRIu64" / %-11"PRIu64"\n"
                                                     "               %9"PRIu64" / %-9"PRIu64"  %11"PRIu64" / %-11"PRIu64"  %11"PRIu64" / %-11"PRIu64" / %-11"PRIu64"\n",
                                                i, nfs_for_service, nf_count,
                                                rx_pps_for_service[i],  tx_pps_for_service[i], rx_for_service[i], tx_for_service[i], act_out_for_service[i], act_tonf_for_service[i], act_drop_for_service[i],
                                                rx_drop_rate_for_service[i], tx_drop_rate_for_service[i], rx_drop_for_service[i], tx_drop_for_service[i], act_next_for_service[i], act_buffer_for_service[i], act_returned_for_service[i]);
                        } else {
                                fprintf(stats_out, "SID %-2u %2u%s - %9"PRIu64" / %-9"PRIu64"  %9"PRIu64" / %-9"PRIu64"  %11"PRIu64" / %-11"PRIu64" / %-11"PRIu64" \n",
                                                i, nfs_for_service, nf_count,
                                                rx_pps_for_service[i],  tx_pps_for_service[i],  rx_drop_for_service[i],  tx_drop_for_service[i],
                                                act_out_for_service[i],  act_tonf_for_service[i],  act_drop_for_service[i]);
                        }
                }
        }
#endif
        fprintf(stats_out,"CHAINS\n");
        fprintf(stats_out,"-------\n");
        (void)difftime;
        onvm_flow_dir_print_stats(stats_out);
}

void onvm_stats_display_chains(unsigned difftime) {
        fprintf(stats_out,"CHAINS\n");
        fprintf(stats_out,"-------\n");
        (void)difftime;
        onvm_flow_dir_print_stats(stats_out);
}

/***************************Helper functions**********************************/

void
onvm_stats_clear_terminal(void) {
        const char clr[] = { 27, '[', '2', 'J', '\0' };
        const char topLeft[] = { 27, '[', '1', ';', '1', 'H', '\0' };

        fprintf(stats_out, "%s%s", clr, topLeft);
}


const char *
onvm_stats_print_MAC(uint8_t port) {
        static const char err_address[] = "00:00:00:00:00:00";
        static char addresses[RTE_MAX_ETHPORTS][sizeof(err_address)];

        if (unlikely(port >= RTE_MAX_ETHPORTS))
                return err_address;

        if (unlikely(addresses[port][0] == '\0')) {
                struct ether_addr mac;
                rte_eth_macaddr_get(port, &mac);
                snprintf(addresses[port], sizeof(addresses[port]),
                                "%02x:%02x:%02x:%02x:%02x:%02x\n",
                                mac.addr_bytes[0], mac.addr_bytes[1],
                                mac.addr_bytes[2], mac.addr_bytes[3],
                                mac.addr_bytes[4], mac.addr_bytes[5]);
        }

        return addresses[port];
}

void
onvm_print_ethaddr(const char *name, struct ether_addr *eth_addr)
{
        char buf[ETHER_ADDR_FMT_SIZE];
        ether_format_addr(buf, ETHER_ADDR_FMT_SIZE, eth_addr);
        fprintf(stats_out,"%s: %s\n", name, buf);
}

static void
onvm_stats_flush(void) {
        if (stats_out == stdout || stats_out == stderr) {
                return;
        }

        fflush(stats_out);
        fflush(json_stats_out);
}


static void
onvm_stats_truncate(void) {
        if (stats_out == stdout || stats_out == stderr) {
                return;
        }

        stats_out = freopen(NULL, ONVM_STATS_FOPEN_ARGS, stats_out);
        json_stats_out = freopen(NULL, ONVM_STATS_FOPEN_ARGS, json_stats_out);

        /* Ensure we're able to open all the files we need */
        if (stats_out == NULL) {
                rte_exit(-1, "Error truncating stats files\n");
        }
}


static void
onvm_json_reset_objects(void) {
        time_t current_time;

        if (onvm_json_root) {
                cJSON_Delete(onvm_json_root);
                onvm_json_root = NULL;
        }

        onvm_json_root = cJSON_CreateObject();

        current_time = time(0);

        cJSON_AddStringToObject(onvm_json_root, ONVM_JSON_TIMESTAMP_KEY,
                              ctime(&current_time));
        cJSON_AddItemToObject(onvm_json_root, ONVM_JSON_PORT_STATS_KEY,
                              onvm_json_port_stats_arr = cJSON_CreateArray());
        cJSON_AddItemToObject(onvm_json_root, ONVM_JSON_NF_STATS_KEY,
                              onvm_json_nf_stats_arr = cJSON_CreateArray());
}

static onvm_stats_snapshot_t sn_local[MAX_NFS] = {{0,},};
int get_onvm_nf_stats_snapshot_v2(unsigned nf_index, onvm_stats_snapshot_t *snapshot, unsigned difftime) {
        if(!snapshot || nf_index>= MAX_NFS || !difftime) return 0;

        struct onvm_nf *cl = &nfs[nf_index];
        if (!onvm_nf_is_valid(cl)) return 0;

        const uint64_t rx = cl->stats.rx;
        const uint64_t rx_drop = cl->stats.rx_drop;
        const uint64_t tx = cl->stats.tx;
        const uint64_t tx_drop = cl->stats.tx_drop;

        snapshot->rx_delta  = (rx - sn_local[nf_index].rx_delta);
        snapshot->rx_rate   = (snapshot->rx_delta)/difftime;
        snapshot->tx_delta  = (tx - sn_local[nf_index].tx_delta);
        snapshot->tx_rate   = (snapshot->tx_delta)/difftime;

        snapshot->rx_drop_delta = (rx_drop - sn_local[nf_index].rx_drop_delta);
        snapshot->rx_drop_rate  = (snapshot->rx_drop_delta)/difftime;
        snapshot->tx_drop_delta = (tx_drop - sn_local[nf_index].tx_drop_delta);
        snapshot->tx_drop_rate  = (snapshot->tx_drop_delta)/difftime;


        //In static NF snapshot entry store the actual values and not deltas
        sn_local[nf_index].rx_delta = rx;
        sn_local[nf_index].tx_delta = tx;
        sn_local[nf_index].rx_drop_delta = rx_drop;
        sn_local[nf_index].tx_drop_delta = tx_drop;

        return 1;
}
