/*********************************************************************
 *                     openNetVM
 *              https://sdnfv.github.io
 *
 *   BSD LICENSE
 *
 *   Copyright(c)
 *            2015-2021 George Washington University
 *            2015-2021 University of California Riverside
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
 * skeleton.c - Template NF for development.
 ********************************************************************/

#include <errno.h>
#include <getopt.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/queue.h>
#include <unistd.h>

#include <rte_common.h>
#include <rte_ip.h>
#include <rte_mbuf.h>

#include "onvm_nflib.h"
#include "onvm_pkt_helper.h"

// Define NF Tag Appropriately
#define NF_TAG "skeleton"

/* number of package between each print */
static uint32_t print_delay = 1000000;

// State structs hold any variables which are universal to all files within the NF
// The following struct is an example from test_messaging NF:
struct skeleton_state {
        // Insert stored data which is not specific to any function (similar to global data)
        int tests_passed;
        int test_phase;
        int test_msg_count;
        int ring_count;
        int mempool_count;
        uint16_t address;
        struct rte_mempool* msg_pool;
        struct rte_ring *msg_q;
        int test_status[3];
        uint64_t start_time;
};

/*
 * Print a usage message
 */
static void
usage(const char *progname) {
        printf("Usage:\n");
        printf("%s [EAL args] -- [NF_LIB args] -- -p <print_delay>\n", progname);
        printf("%s -F <CONFIG_FILE.json> [EAL args] -- [NF_LIB args] -- [NF args]\n\n", progname);
        printf("Flags:\n");
        printf(" - `-p <print_delay>`: number of packets between each print, e.g. `-p 1` prints every packets.\n");
}

/*
 * Parse the application arguments.
 */
static int
parse_app_args(int argc, char *argv[], const char *progname) {
        int c;

        // Handles command line arguments
        while ((c = getopt(argc, argv, "p:")) != -1) {
                switch (c) {
                        case 'p':
                                print_delay = strtoul(optarg, NULL, 10);
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

// Function for NF with displays - Example from bridge.c
static void
do_stats_display(struct rte_mbuf *pkt) {
        
        // [EDIT CODE IF NF USES DISPLAY]
        
        const char clr[] = {27, '[', '2', 'J', '\0'};
        const char topLeft[] = {27, '[', '1', ';', '1', 'H', '\0'};
        static uint64_t pkt_process = 0;

        struct rte_ipv4_hdr *ip;

        pkt_process += print_delay;

        /* Clear screen and move to top left */
        printf("%s%s", clr, topLeft);

        printf("PACKETS\n");
        printf("-----\n");
        printf("Port : %d\n", pkt->port);
        printf("Size : %d\n", pkt->pkt_len);
        printf("Type : %d\n", pkt->packet_type);
        printf("Number of packet processed : %" PRIu64 "\n", pkt_process);

        ip = onvm_pkt_ipv4_hdr(pkt);
        if (ip != NULL) {
                onvm_pkt_print(pkt);
        } else {
                printf("Not IP4\n");
        }

        printf("\n\n");
}


static int
packet_handler(struct rte_mbuf *pkt, struct onvm_pkt_meta *meta,
               __attribute__((unused)) struct onvm_nf_local_ctx *nf_local_ctx) {
        
        // Timed display counter - keep/edit if NF uses display
        static uint32_t counter = 0;
        if (counter++ == print_delay) {
                do_stats_display(pkt);
                counter = 0;
        }

        // [EDIT PACKET HANDLER CODE BELOW]
        
        /* Important Variables
          meta->action
                Defines what to do with the packet. Actions defined in onvm_common.h
          meta->destination 
                Defines the service ID of the NF which will receive the packet.           
        */
        if (pkt->port == 0) {
                meta->destination = 1;
        } else {
                meta->destination = 0;
        }
        
        // Send the packet out of the NIC port
        meta->action = ONVM_NF_ACTION_OUT;

        // Return 0 on success, -1 on failure
        return 0;
}

static int 
action(__attribute__((unused)) struct onvm_nf_local_ctx *nf_local_ctx){
        
        // Gets called every run loop even if there weren't any packets
        
        // [ADD ACTION CODE] - Remove __attribute__((unused))
        return 0;
}

static void 
setup(__attribute__((unused))struct onvm_nf_local_ctx *nf_local_ctx){

        // Gets called once before NF will get any packets/messages

        // Excerpt from test_messaging setup; note nf_local_ctx->nf->data with corresponding struct
        struct rte_mempool *pktmbuf_pool;
        struct test_msg_data *test_state;
        static struct rte_mempool *nf_msg_pool;
        pktmbuf_pool = rte_mempool_lookup(PKTMBUF_POOL_NAME);
        if (pktmbuf_pool == NULL) {
                onvm_nflib_stop(nf_local_ctx);
                rte_exit(EXIT_FAILURE, "Cannot find mbuf pool!\n");
        }
        nf_msg_pool = rte_mempool_lookup(_NF_MSG_POOL_NAME);
        if (nf_msg_pool == NULL)
                rte_exit(EXIT_FAILURE, "No NF Message mempool - bye\n");

        nf_local_ctx->nf->data = (void *)test_state;

        // [REMOVE EXCERPT, ADD SETUP CODE]
}

static void 
handle_msg(__attribute__((unused))void *msg_data, __attribute__((unused))struct onvm_nf_local_ctx *nf_local_ctx) {

        // Gets called if there is a message sent to the NF
        
        // [ADD MESSAGE HANDLER CODE] - Remove __attribute__((unused))
}

int
main(int argc, char *argv[]) {
        
        // Handles command line arguments
        int arg_offset;
        // Local context holds NF and status
        struct onvm_nf_local_ctx *nf_local_ctx;
        // Function table holds pointers to NF setup, msg_handler, user_actions, and pkt_handler
        struct onvm_nf_function_table *nf_function_table;
        // Handles command line arguments
        const char *progname = argv[0];                         

        // Initializing local context and start default signal handler
        nf_local_ctx = onvm_nflib_init_nf_local_ctx();
        onvm_nflib_start_signal_handler(nf_local_ctx, NULL);

        // [EDIT FUNCTION TABLE AS NEEDED]
        // Address declarations of NF functions for function table:
        nf_function_table = onvm_nflib_init_nf_function_table();
        nf_function_table->pkt_handler = &packet_handler;
        nf_function_table->setup = &setup;
        nf_function_table->user_actions = &action;
        nf_function_table->msg_handler = &handle_msg;

        // If a termination signal is received or initiation is interrupted, exit
        if ((arg_offset = onvm_nflib_init(argc, argv, NF_TAG, nf_local_ctx, nf_function_table)) < 0) {
                onvm_nflib_stop(nf_local_ctx);
                if (arg_offset == ONVM_SIGNAL_TERMINATION) {
                        printf("Exiting due to user termination\n");
                        return 0;
                } else {
                        rte_exit(EXIT_FAILURE, "Failed ONVM init\n");
                }
        }

        // Command line arguments
        argc -= arg_offset;
        argv += arg_offset;

        // Invalid command-line argument handling
        if (parse_app_args(argc, argv, progname) < 0) {
                onvm_nflib_stop(nf_local_ctx);
                rte_exit(EXIT_FAILURE, "Invalid command-line arguments\n");
        }

        // Begin running NF
        onvm_nflib_run(nf_local_ctx);

        // Once the NF has stopped running, free and stop
        onvm_nflib_stop(nf_local_ctx);
        printf("If we reach here, program is ending\n");
        return 0;
}
