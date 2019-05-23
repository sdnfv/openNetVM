/*********************************************************************
 *                     openNetVM
 *              https://sdnfv.github.io
 *
 *   BSD LICENSE
 *
 *   Copyright(c)
 *            2015-2017 George Washington University
 *            2015-2017 University of California Riverside
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
 * monitor.c - an example using onvm. Print a message each p package received
 ********************************************************************/

#include <unistd.h>
#include <stdint.h>
#include <stdio.h>
#include <inttypes.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/queue.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>

#include <rte_common.h>
#include <rte_mbuf.h>
#include <rte_ip.h>

#include "onvm_nflib.h"
#include "onvm_pkt_helper.h"
#include "onvm_flow_dir.h"



#define NF_TAG "basic_monitor"

/* Struct that contains information about this NF */
struct onvm_nf_info *nf_info;

/* number of package between each print */
static uint32_t print_delay = 1000000;

typedef struct monitor_state_info_table {
        uint16_t ft_index;
        uint16_t tag_counter;
        uint32_t pkt_counter;
}monitor_state_info_table_t;
monitor_state_info_table_t *mon_state_tbl = NULL;

#if 0
typedef struct dirty_mon_state_map_tbl {
        uint64_t dirty_index;   //Bit index to every 1K LSB=0-1K, MSB=63-64K
}dirty_mon_state_map_tbl_t;
dirty_mon_state_map_tbl_t *dirty_state_map = NULL;
#endif

#ifdef ENABLE_NFV_RESL
#define MAX_STATE_ELEMENTS  ((_NF_STATE_SIZE-sizeof(dirty_mon_state_map_tbl_t))/sizeof(monitor_state_info_table_t))
#endif

#ifdef MIMIC_FTMB
extern uint8_t SV_ACCES_PER_PACKET;
#endif
/*
 * Print a usage message
 */
static void
usage(const char *progname) {
        printf("Usage: %s [EAL args] -- [NF_LIB args] -- -p <print_delay>\n\n", progname);
}

/*
 * Parse the application arguments.
 */
static int
parse_app_args(int argc, char *argv[], const char *progname) {
        int c;

        while ((c = getopt (argc, argv, "p:")) != -1) {
                switch (c) {
                case 'p':
                        print_delay = strtoul(optarg, NULL, 10);
                        RTE_LOG(INFO, APP, "print_delay = %d\n", print_delay);
                        break;
                case '?':
                        usage(progname);
                        if (optopt == 'p')
                                RTE_LOG(INFO, APP, "Option -%c requires an argument.\n", optopt);
                        else if (isprint(optopt))
                                RTE_LOG(INFO, APP, "Unknown option `-%c'.\n", optopt);
                        else
                                RTE_LOG(INFO, APP, "Unknown option character `\\x%x'.\n", optopt);
                        return -1;
                default:
                        usage(progname);
                        return -1;
                }
        }
        return optind;
}

/*
 * This function displays stats. It uses ANSI terminal codes to clear
 * screen when called. It is called from a single non-master
 * thread in the server process, when the process is run with more
 * than one lcore enabled.
 */
static void
do_stats_display(struct rte_mbuf* pkt) {
//	return ;
        const char clr[] = { 27, '[', '2', 'J', '\0' };
        const char topLeft[] = { 27, '[', '1', ';', '1', 'H', '\0' };
        static uint64_t pkt_process = 0;

        struct ipv4_hdr* ip;

        pkt_process += print_delay;

        /* Clear screen and move to top left */
        printf("%s%s", clr, topLeft);

        printf("PACKETS\n");
        printf("-----\n");
        printf("Port : %d\n", pkt->port);
        printf("Size : %d\n", pkt->pkt_len);
        printf("Hash : %u\n", pkt->hash.rss);
        printf("N°   : %"PRIu64"\n", pkt_process);
#ifdef ENABLE_NFV_RESL
        printf("MAX State: %lu\n", MAX_STATE_ELEMENTS);
        if(mon_state_tbl) {
                printf("Share Counter: %d\n", mon_state_tbl[0].tag_counter);
                printf("Pkt Counter: %d\n", mon_state_tbl[0].pkt_counter);
                printf("Dirty Bits: 0x%lx\n", dirty_state_map->dirty_index);
        }
#endif
        printf("\n\n");

        ip = onvm_pkt_ipv4_hdr(pkt);
        if (ip != NULL) {
                onvm_pkt_print(pkt);
        } else {
                printf("No IP4 header found [%"PRIu64"]\n", pkt_process);
        }
}
#ifdef ENABLE_NFV_RESL
//#define ENABLE_LOCAL_LATENCY_PROFILER
static inline uint64_t map_tag_index_to_dirty_chunk_bit_index(uint16_t vlan_tbl_index) {
        uint32_t start_offset = sizeof(dirty_mon_state_map_tbl_t) + vlan_tbl_index*sizeof(monitor_state_info_table_t);
        uint32_t end_offset = start_offset + sizeof(monitor_state_info_table_t);
        uint64_t dirty_map_bitmask = 0;
        dirty_map_bitmask |= (1<< (start_offset/DIRTY_MAP_PER_CHUNK_SIZE));
        dirty_map_bitmask |= (1<< (end_offset/DIRTY_MAP_PER_CHUNK_SIZE));
        //printf("\n For %d, 0x%lx\n",(int)vlan_tbl_index, dirty_map_bitmask);
        return dirty_map_bitmask;
}
static inline int update_dirty_state_index(uint16_t flow_index) {
        if(dirty_state_map) {
                dirty_state_map->dirty_index |= map_tag_index_to_dirty_chunk_bit_index(flow_index);
                ////dirty_state_map->dirty_index |= (1L<<(rand() % 60));
                ////dirty_state_map->dirty_index |= (-1);
                dirty_state_map->dirty_index |= (0xFFFFFFFF);
        }
        return flow_index;
}

static int save_packet_state(struct rte_mbuf* pkt, struct onvm_pkt_meta* meta, __attribute__((unused)) struct onvm_nf_info *nf_info) {
//return 0;
#ifdef ENABLE_LOCAL_LATENCY_PROFILER
        static int countm = 0;uint64_t start_cycle=0;onvm_interval_timer_t ts_p;
        countm++;
        if(countm == 1000*1000*20) {
                onvm_util_get_start_time(&ts_p);
                start_cycle = onvm_util_get_current_cpu_cycles();
        }
#endif
        if(nf_info->nf_state_mempool) {
                uint16_t ft_index = 0;
                if(mon_state_tbl  == NULL) {
                        dirty_state_map = (dirty_mon_state_map_tbl_t*)nf_info->nf_state_mempool;
                        mon_state_tbl = (monitor_state_info_table_t*)(dirty_state_map+1);
                        //mon_state_tbl[0].ft_index = 0;
                        mon_state_tbl[0].tag_counter+=1;
                }
                if(mon_state_tbl) {
                        if(meta && pkt) {
                                struct onvm_flow_entry *flow_entry = NULL;
                                onvm_flow_dir_get_pkt(pkt, &flow_entry);
                                ft_index = (uint16_t)flow_entry->entry_index;
                                if(flow_entry) {
                                        mon_state_tbl[flow_entry->entry_index].ft_index = meta->src;
                                        mon_state_tbl[flow_entry->entry_index].pkt_counter +=1;
                                }
                        }
                        mon_state_tbl[0].pkt_counter+=1;
                }
                update_dirty_state_index(ft_index);
        }
#ifdef ENABLE_LOCAL_LATENCY_PROFILER
        if(countm == 1000*1000*20) {
                fprintf(stdout, "STATE REPLICATION TIME (Marking): %li(ns) and %li (cycles) \n", onvm_util_get_elapsed_time(&ts_p), onvm_util_get_elapsed_cpu_cycles(start_cycle));
                countm=0;
        }
#endif
        return 0;
}

static int save_packet_state_new(__attribute__((unused)) struct rte_mbuf* pkt, struct onvm_pkt_meta* meta, __attribute__((unused)) struct onvm_nf_info *nf_info) {
//return 0;
        if(mon_state_tbl) {
                int16_t ft_index = -1;
#ifdef ENABLE_FT_INDEX_IN_META
                ft_index = (uint16_t) (meta->ft_index);
#else
                {
                        struct onvm_flow_entry *flow_entry = NULL;
                        onvm_flow_dir_get_pkt(pkt, &flow_entry);
                        if(flow_entry)
                                ft_index = flow_entry->entry_index;
                }
#endif
                if(ft_index>=0) {
                        mon_state_tbl[ft_index].ft_index = meta->src;
                } else ft_index=0;
                mon_state_tbl[ft_index].pkt_counter+=1;
                update_dirty_state_index(ft_index);
        }
        return 0;
}
#endif //#ifdef ENABLE_NFV_RESL
static int
packet_handler(struct rte_mbuf* pkt, struct onvm_pkt_meta* meta, __attribute__((unused)) struct onvm_nf_info *nf_info) {
        static uint32_t counter = 0;
        //if (++counter == 0) {
        if (++counter == print_delay) {
                do_stats_display(pkt);
                counter = 0;
        }

        meta->action = ONVM_NF_ACTION_OUT;
        meta->destination = pkt->port;


        if (onvm_pkt_mac_addr_swap(pkt, 0) != 0) {
                printf("ERROR: MAC failed to swap!\n");
        }

#ifdef ENABLE_NFV_RESL
        return save_packet_state_new(pkt,meta,nf_info);
        save_packet_state(pkt,meta,nf_info);
#endif //#ifdef ENABLE_NFV_RESL
        return 0;
}


int main(int argc, char *argv[]) {
        int arg_offset;

        const char *progname = argv[0];

        if ((arg_offset = onvm_nflib_init(argc, argv, NF_TAG, &nf_info)) < 0)
                return -1;
        argc -= arg_offset;
        argv += arg_offset;

        if (parse_app_args(argc, argv, progname) < 0)
                rte_exit(EXIT_FAILURE, "Invalid command-line arguments\n");
#ifdef ENABLE_NFV_RESL
        if(nf_info->nf_state_mempool) {
                dirty_state_map = (dirty_mon_state_map_tbl_t*)nf_info->nf_state_mempool;
                mon_state_tbl = (monitor_state_info_table_t*)(dirty_state_map+1);
        }
#endif
#ifdef MIMIC_FTMB
SV_ACCES_PER_PACKET = 5;
#endif
        onvm_nflib_run(nf_info, &packet_handler);
        printf("If we reach here, program is ending\n");
        return 0;
}
