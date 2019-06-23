/*********************************************************************
 *                     openNetVM
 *              https://sdnfv.github.io
 *
 *   BSD LICENSE
 *
 *   Copyright(c)
 *            2015-2019 George Washington University
 *            2015-2019 University of California Riverside
 *            2010-2019 Intel Corporation. All rights reserved.
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
                                 onvm_stats.h

            This file contains all function prototypes related to
            statistics display.

******************************************************************************/

#ifndef _ONVM_STATS_H_
#define _ONVM_STATS_H_

#include <locale.h>
#include <time.h>
#include "cJSON.h"

#define ONVM_STR_STATS_STDOUT "stdout"
#define ONVM_STR_STATS_STDERR "stderr"
#define ONVM_STR_STATS_WEB "web"

extern const char *NF_MSG[3];
#define ONVM_STATS_MSG "\n"\
        "NF TAG         IID / SID / CORE    rx_pps  /  tx_pps        rx_drop  /  tx_drop           out   /    tonf     /   drop\n"\
        "----------------------------------------------------------------------------------------------------------------------\n"
#define ONVM_STATS_ADV_MSG "\n"\
        "NF TAG         IID / SID / CORE    rx_pps  /  tx_pps             rx  /  tx                out   /    tonf     /   drop\n"\
        "               PNT / S|W / CHLD  drop_pps  /  drop_pps      rx_drop  /  tx_drop           next  /    buf      /   ret\n"\
        "----------------------------------------------------------------------------------------------------------------------\n"
#define ONVM_STATS_SHARED_CORE_MSG "\n"\
        "NF TAG         IID / SID / CORE    rx_pps  /  tx_pps             rx  /  tx                out   /    tonf     /   drop\n"\
        "               PNT / S|W / CHLD  drop_pps  /  drop_pps      rx_drop  /  tx_drop           next  /    buf      /   ret\n"\
        "                                  wakeups  /  wakeup_rt\n"\
        "----------------------------------------------------------------------------------------------------------------------\n"
#define ONVM_STATS_RAW_DUMP_PORT_MSG \
        "#YYYY-MM-DD HH:MM:SS,nic_rx_pkts,nic_rx_pps,nic_tx_pkts,nic_tx_pps\n"
#define ONVM_STATS_RAW_DUMP_NF_MSG \
        "#YYYY-MM-DD HH:MM:SS,nf_tag,instance_id,service_id,core,parent,state,children_cnt,"\
        "rx,tx,rx_pps,tx_pps,rx_drop,tx_drop,rx_drop_rate,tx_drop_rate,"\
        "act_out,act_tonf,act_drop,act_next,act_buffer,act_returned,num_wakeups,wakeup_rate\n"
#define ONVM_STATS_REG_CONTENT \
        "%-14s %2u  /  %-2u / %2u    %9" PRIu64 " / %-9" PRIu64 "   %11" PRIu64 " / %-11" PRIu64\
        "  %11" PRIu64 " / %-11" PRIu64 " / %-11" PRIu64 " \n"
#define ONVM_STATS_REG_TOTALS \
        "SID %-2u %2u%s -                   %9" PRIu64 " / %-9" PRIu64 "   %11" PRIu64\
        " / %-11" PRIu64 "  %11" PRIu64 " / %-11" PRIu64 " / %-11" PRIu64 "\n"
#define ONVM_STATS_REG_PORTS \
        "Port %u - rx: %9" PRIu64 "  (%9" PRIu64 " pps)\t"\
        "tx: %9" PRIu64 "  (%9" PRIu64 " pps)\n"
#define ONVM_STATS_ADV_CONTENT \
        "%-14s %2u  /  %-2u / %2u    %9" PRIu64 " / %-9" PRIu64 "   %11" PRIu64 " / %-11" PRIu64\
        "  %11" PRIu64 " / %-11" PRIu64 " / %-11" PRIu64\
        "\n            %5" PRId16 "  /  %c  /  %u    %9" PRIu64 " / %-9" PRIu64 "   %11" PRIu64 " / %-11" PRIu64\
        "  %11" PRIu64 " / %-11" PRIu64 " / %-11" PRIu64 "\n"
#define ONVM_STATS_SHARED_CORE_CONTENT \
        "                               %11" PRIu64 " / %-11" PRIu64"\n"
#define ONVM_STATS_ADV_TOTALS \
        "SID %-2u %2u%s -                   %9" PRIu64 " / %-9" PRIu64 "   %11" PRIu64\
        " / %-11" PRIu64 "  %11" PRIu64 " / %-11" PRIu64 " / %-11" PRIu64\
        "\n                                 %9" PRIu64 " / %-9" PRIu64 "   %11" PRIu64\
        " / %-11" PRIu64 "  %11" PRIu64 " / %-11" PRIu64 " / %-11" PRIu64 "\n"
#define ONVM_STATS_RAW_DUMP_CONTENT \
        "%s,%s,%u,%u,%u,%u,%c,%u,%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",%" PRIu64\
        ",%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",%" PRIu64\
        ",%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",%" PRIu64 "\n"
#define ONVM_STATS_RAW_DUMP_PORTS_CONTENT \
        "%s,%u,%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",%" PRIu64 "\n"

#define ONVM_STATS_FOPEN_ARGS "w+"
#define ONVM_STATS_PATH_BASE "../onvm_web/"
#define ONVM_JSON_STATS_FILE ONVM_STATS_PATH_BASE "onvm_json_stats.json"
#define ONVM_JSON_EVENTS_FILE ONVM_STATS_PATH_BASE "onvm_json_events.json"
#define ONVM_STATS_FILE ONVM_STATS_PATH_BASE "onvm_stats.txt"

/* Handle types of web stats events */
#define ONVM_EVENT_WITH_CORE 0
#define ONVM_EVENT_PORT_INFO 1
#define ONVM_EVENT_NF_INFO 2
#define ONVM_EVENT_NF_STOP 3

#define ONVM_JSON_PORT_STATS_KEY "onvm_port_stats"
#define ONVM_JSON_NF_STATS_KEY "onvm_nf_stats"
#define ONVM_JSON_TIMESTAMP_KEY "last_updated"

#define ONVM_SNPRINTF(str_, sz_, fmt_, ...)                                                              \
        do {                                                                                             \
                (str_) = (char*)malloc(sizeof(char) * (sz_));                                            \
                if (!(str_))                                                                             \
                        rte_exit(-1, "ERROR! [%s,%d]: unable to malloc str.\n", __FUNCTION__, __LINE__); \
                snprintf((str_), (sz_), (fmt_), __VA_ARGS__);                                            \
        } while (0)

#define ONVM_RAW_STATS_DUMP 3

typedef enum { ONVM_STATS_NONE = 0, ONVM_STATS_STDOUT, ONVM_STATS_STDERR, ONVM_STATS_WEB } ONVM_STATS_OUTPUT;

struct onvm_event {
        uint8_t type;
        const char *msg;
        void *data;
};

cJSON* onvm_json_root;
cJSON* onvm_json_port_stats_obj;
cJSON* onvm_json_nf_stats_obj;
cJSON* onvm_json_port_stats[RTE_MAX_ETHPORTS];
cJSON* onvm_json_nf_stats[MAX_NFS];
cJSON* onvm_json_events_arr;

/*********************************Interfaces**********************************/

/*
 * Function for initializing stats
 *
 * Input : Verbosity level
 *
 */
void
onvm_stats_init(uint8_t verbosity_level);

/*
 * Interface called by the manager to tell the stats module where to print
 * You should only call this once
 *
 * Input: a STATS_OUTPUT enum value representing output destination.  If
 * STATS_NONE is specified, then stats will not be printed to the console or web
 * browser.  If STATS_STDOUT or STATS_STDOUT is specified, then stats will be
 * output the respective stream.
 */
void
onvm_stats_set_output(ONVM_STATS_OUTPUT output);

/*
 * Interface to close out file descriptions and clean up memory
 * To be called when the stats loop is done
 */
void
onvm_stats_cleanup(void);

/*
 * Interface called by the ONVM Manager to display all statistics
 * available.
 *
 * Input : time passed since last display (to compute packet rate)
 *
 */
void
onvm_stats_display_all(unsigned difftime, uint8_t verbosity_level);

/*
 * Interface called by the ONVM Manager to clear all NFs statistics
 * available.
 *
 * Note : this function doesn't use onvm_stats_clear_nf for each nf,
 * since with a huge number of NFs, the additional functions calls would
 * incur a visible slowdown.
 *
 */
void
onvm_stats_clear_all_nfs(void);

/*
 * Interface called by the ONVM Manager to clear one NF's statistics.
 *
 * Input : the NF id
 *
 */
void
onvm_stats_clear_nf(uint16_t id);

/*
 * Interface called by manager when a new event should be created.
 */
void
onvm_stats_gen_event_info(const char *msg, uint8_t type, void *data);

void
onvm_stats_gen_event_nf_info(const char *msg, struct onvm_nf *nf);

#endif  // _ONVM_STATS_H_
