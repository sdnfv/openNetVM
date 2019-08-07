/*********************************************************************
 *                     openNetVM
 *              https://sdnfv.github.io
 *
 *   BSD LICENSE
 *
 *   Copyright(c)
 *            2015-2019 George Washington University
 *            2015-2019 University of California Riverside
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
 * flow_tracker.c - an example using onvm. Stores incoming flows and prints info about them.
 ********************************************************************/

#include <errno.h>
#include <getopt.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <rte_common.h>
#include <rte_cycles.h>
#include <rte_hash.h>
#include <rte_ip.h>
#include <rte_malloc.h>
#include <rte_mbuf.h>

#include "onvm_flow_table.h"
#include "onvm_nflib.h"
#include "onvm_pkt_helper.h"

#define NF_TAG "flow_tracker"
#define TBL_SIZE 100
#define EXPIRE_TIME 5

/*Struct that holds all NF state information */
struct state_info {
        struct onvm_ft *ft;
        uint16_t destination;
        uint16_t print_delay;
        uint16_t num_stored;
        uint64_t elapsed_cycles;
        uint64_t last_cycles;
};

/*Struct that holds info about each flow, and is stored at each flow table entry */
struct flow_stats {
        int pkt_count;
        uint64_t last_pkt_cycles;
        int is_active;
};

struct state_info *state_info;

/*
 * Prints application arguments
 */
static void
usage(const char *progname) {
        printf("Usage:\n");
        printf("%s [EAL args] -- [NF_LIB args] --d <destination>\n", progname);
        printf("%s -F <CONFIG_FILE.json> [EAL args] -- [NF_LIB args] -- [NF args]\n\n", progname);
        printf("Flags:\n");
        printf(" - `-d <destination_id>`: Service ID to send packets to`\n");
        printf(" - `-p <print_delay>`:  Number of seconds between each print (default is 5)\n");
}

/*
 * Loops through inputted arguments and assigns values as necessary
 */
static int
parse_app_args(int argc, char *argv[], const char *progname) {
        int c, dst_flag = 0;

        while ((c = getopt(argc, argv, "d:p:")) != -1) {
                switch (c) {
                        case 'd':
                                state_info->destination = strtoul(optarg, NULL, 10);
                                dst_flag = 1;
                                RTE_LOG(INFO, APP, "Sending packets to service ID %d\n", state_info->destination);
                                break;
                        case 'p':
                                state_info->print_delay = strtoul(optarg, NULL, 10);
                                break;
                        case '?':
                                usage(progname);
                                if (optopt == 'd')
                                        RTE_LOG(INFO, APP, "Option -%c requires an argument\n", optopt);
                                else if (optopt == 'p')
                                        RTE_LOG(INFO, APP, "Option -%c requires an argument\n", optopt);
                                else
                                        RTE_LOG(INFO, APP, "Unknown option character\n");
                                return -1;
                        default:
                                usage(progname);
                                return -1;
                }
        }

        if (!dst_flag) {
                RTE_LOG(INFO, APP, "Flow Tracker NF requires a destination NF service ID with the -d flag \n");
                return -1;
        }

        return optind;
}

/*
 * Updates flow status to be "active" or "expired"
 */
static int
update_status(uint64_t elapsed_cycles, struct flow_stats *data) {
        if (unlikely(data == NULL)) {
                return -1;
        }
        if ((elapsed_cycles - data->last_pkt_cycles) / rte_get_timer_hz() >= EXPIRE_TIME) {
                data->is_active = 0;
        } else {
                data->is_active = 1;
        }

        return 0;
}

/*
 * Clears expired entries from the flow table
 */
static int
clear_entries(struct state_info *state_info) {
        if (unlikely(state_info == NULL)) {
                return -1;
        }

        printf("Clearing expired entries\n");
        struct flow_stats *data = NULL;
        struct onvm_ft_ipv4_5tuple *key = NULL;
        uint32_t next = 0;
        int ret = 0;

        while (onvm_ft_iterate(state_info->ft, (const void **)&key, (void **)&data, &next) > -1) {
                if (update_status(state_info->elapsed_cycles, data) < 0) {
                        return -1;
                }

                if (!data->is_active) {
                        ret = onvm_ft_remove_key(state_info->ft, key);
                        state_info->num_stored--;
                        if (ret < 0) {
                                printf("Key should have been removed, but was not\n");
                                state_info->num_stored++;
                        }
                }
        }

        return 0;
}

/*
 * Prints out information about flows stored in table
 */
static void
do_stats_display(struct state_info *state_info) {
        struct flow_stats *data = NULL;
        struct onvm_ft_ipv4_5tuple *key = NULL;
        uint32_t next = 0;
        int32_t index;

        printf("------------------------------\n");
        printf("     Flow Table Contents\n");
        printf("------------------------------\n");
        printf("Current capacity: %d / %d\n\n", state_info->num_stored, TBL_SIZE);
        while ((index = onvm_ft_iterate(state_info->ft, (const void **)&key, (void **)&data, &next)) > -1) {
                update_status(state_info->elapsed_cycles, data);
                printf("%d. Status: ", index);
                if (data->is_active) {
                        printf("Active\n");
                } else {
                        printf("Expired\n");
                }

                printf("Key information:\n");
                _onvm_ft_print_key(key);
                printf("Packet count: %d\n\n", data->pkt_count);
        }
}

/*
 * Adds an entry to the flow table. It first checks if the table is full, and
 * if so, it calls clear_entries() to free up space.
 */
static int
table_add_entry(struct onvm_ft_ipv4_5tuple *key, struct state_info *state_info) {
        struct flow_stats *data = NULL;

        if (unlikely(key == NULL || state_info == NULL)) {
                return -1;
        }

        if (TBL_SIZE - state_info->num_stored == 0) {
                int ret = clear_entries(state_info);
                if (ret < 0) {
                        return -1;
                }
        }

        int tbl_index = onvm_ft_add_key(state_info->ft, key, (char **)&data);
        if (tbl_index < 0) {
                return -1;
        }

        data->pkt_count = 0;
        data->last_pkt_cycles = state_info->elapsed_cycles;
        data->is_active = 1;
        state_info->num_stored += 1;
        return 0;
}

/*
 * Looks up a packet hash to see if there is a matching key in the table.
 * If it finds one, it updates the metadata associated with the key entry,
 * and if it doesn't, it calls table_add_entry() to add it to the table.
 */
static int
table_lookup_entry(struct rte_mbuf *pkt, struct state_info *state_info) {
        struct flow_stats *data = NULL;
        struct onvm_ft_ipv4_5tuple key;

        if (unlikely(pkt == NULL || state_info == NULL)) {
                return -1;
        }

        int ret = onvm_ft_fill_key_symmetric(&key, pkt);
        if (ret < 0)
                return -1;

        int tbl_index = onvm_ft_lookup_key(state_info->ft, &key, (char **)&data);
        if (tbl_index == -ENOENT) {
                return table_add_entry(&key, state_info);
        } else if (tbl_index < 0) {
                printf("Some other error occurred with the packet hashing\n");
                return -1;
        } else {
                data->pkt_count += 1;
                data->last_pkt_cycles = state_info->elapsed_cycles;
                return 0;
        }
}

static int
callback_handler(__attribute__((unused)) struct onvm_nf_local_ctx *nf_local_ctx) {
        state_info->elapsed_cycles = rte_get_tsc_cycles();

        if ((state_info->elapsed_cycles - state_info->last_cycles) / rte_get_timer_hz() > state_info->print_delay) {
                state_info->last_cycles = state_info->elapsed_cycles;
                do_stats_display(state_info);
        }

        return 0;
}

static int
packet_handler(struct rte_mbuf *pkt, struct onvm_pkt_meta *meta,
               __attribute__((unused)) struct onvm_nf_local_ctx *nf_local_ctx) {
        if (!onvm_pkt_is_ipv4(pkt)) {
                meta->destination = state_info->destination;
                meta->action = ONVM_NF_ACTION_TONF;
                return 0;
        }

        if (table_lookup_entry(pkt, state_info) < 0) {
                printf("Packet could not be identified or processed\n");
        }

        meta->destination = state_info->destination;
        meta->action = ONVM_NF_ACTION_TONF;

        return 0;
}

int
main(int argc, char *argv[]) {
        int arg_offset;
        struct onvm_nf_local_ctx *nf_local_ctx;
        struct onvm_nf_function_table *nf_function_table;
        const char *progname = argv[0];

        nf_local_ctx = onvm_nflib_init_nf_local_ctx();
        onvm_nflib_start_signal_handler(nf_local_ctx, NULL);

        nf_function_table = onvm_nflib_init_nf_function_table();
        nf_function_table->pkt_handler = &packet_handler;
        nf_function_table->user_actions = &callback_handler;

        if ((arg_offset = onvm_nflib_init(argc, argv, NF_TAG, nf_local_ctx, nf_function_table)) < 0) {
                onvm_nflib_stop(nf_local_ctx);
                if (arg_offset == ONVM_SIGNAL_TERMINATION) {
                        printf("Exiting due to user termination\n");
                        return 0;
                } else {
                        rte_exit(EXIT_FAILURE, "Failed ONVM init\n");
                }
        }

        argc -= arg_offset;
        argv += arg_offset;

        state_info = rte_calloc("state", 1, sizeof(struct state_info), 0);
        if (state_info == NULL) {
                onvm_nflib_stop(nf_local_ctx);
                rte_exit(EXIT_FAILURE, "Unable to initialize NF state");
        }

        state_info->print_delay = 5;
        state_info->num_stored = 0;

        if (parse_app_args(argc, argv, progname) < 0) {
                onvm_nflib_stop(nf_local_ctx);
                rte_exit(EXIT_FAILURE, "Invalid command-line arguments");
        }

        state_info->ft = onvm_ft_create(TBL_SIZE, sizeof(struct flow_stats));
        if (state_info->ft == NULL) {
                onvm_nflib_stop(nf_local_ctx);
                rte_exit(EXIT_FAILURE, "Unable to create flow table");
        }

        /*Initialize NF timer */
        state_info->elapsed_cycles = rte_get_tsc_cycles();

        onvm_nflib_run(nf_local_ctx);

        onvm_nflib_stop(nf_local_ctx);
        printf("If we reach here, program is ending!\n");
        return 0;
}
