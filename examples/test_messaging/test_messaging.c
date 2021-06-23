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
 * test_messaging.c - unit test to ensure NFs can send messages to itself
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
#include <rte_mempool.h>
#include <rte_ring.h>
#include <rte_malloc.h>

#include "onvm_nflib.h"
#include "onvm_pkt_helper.h"
#include "onvm_common.h"

#define NF_TAG "test_messaging"

/* number of package between each print */
static uint32_t print_delay = 1000000;

static uint16_t destination;

struct test_msg_data{
        int tests_passed;
        int tests_failed;
        struct rte_mempool* msg_pool;
        int test_phase;
};

void
nf_setup(struct onvm_nf_local_ctx *nf_local_ctx);

void
nf_msg_handler(void *msg_data, struct onvm_nf_local_ctx *nf_local_ctx);

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

        while ((c = getopt(argc, argv, "p:")) != -1) {
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

void
nf_setup(__attribute__((unused)) struct onvm_nf_local_ctx *nf_local_ctx) {
        struct rte_mempool *pktmbuf_pool;

        pktmbuf_pool = rte_mempool_lookup(PKTMBUF_POOL_NAME);
        if (pktmbuf_pool == NULL) {
                onvm_nflib_stop(nf_local_ctx);
                rte_exit(EXIT_FAILURE, "Cannot find mbuf pool!\n");
        }

        uint16_t address = nf_local_ctx->nf->service_id;

        // lookup NF message pool and store pointer to it somewhere
        /* Lookup mempool for NF messages */
        // nf_msg_pool = rte_mempool_lookup(_NF_MSG_POOL_NAME);
        // if (nf_msg_pool == NULL)
        //         rte_exit(EXIT_FAILURE, "No NF Message mempool - bye\n");

        // send the numbers 1...10 and make sure we receive that correctly
        int msg_ints[130];

        for(int i = 0; i < 130; i++){
                msg_ints[i] = i;
                int ret = onvm_nflib_send_msg_to_nf(address, &msg_ints[i]);
                
                printf("Sending message %d\n", i);
                printf("Return: %d\n", ret);
                printf("Iteration: %d\n", i);
                printf("SID: %i\n", address);
                printf("Ring Count: %u\n", rte_ring_count(nf_local_ctx->nf->msg_q));
                FILE *fp = fopen("/users/noahchin/openNetVM/examples/test_messaging/status.txt", "a+");
                rte_ring_dump(fp, nf_local_ctx->nf->msg_q);
                fclose(fp);
                printf("---------------------------\n");

        }


}

void
nf_msg_handler(void *msg_data, __attribute__((unused)) struct onvm_nf_local_ctx *nf_local_ctx){

        // uint16_t address = nf_local_ctx->nf->service_id;
        printf("Receive message\n");
        printf("msg_data: %d\n", *((int*) msg_data));
        printf("Ring Count: %u\n", rte_ring_count(nf_local_ctx->nf->msg_q));
        FILE *fp = fopen("/users/noahchin/openNetVM/examples/test_messaging/status.txt", "a+");
        rte_ring_dump(fp, nf_local_ctx->nf->msg_q);
        fclose(fp);
        int empty = rte_ring_empty(nf_local_ctx->nf->msg_q);
        if(empty != 0){
                printf("Message Ring Empty\n");
        }
        else{
                printf("Message Ring Not Empty\n");
        }
        printf("---------------------------\n");
        
        
        
}

static int
packet_handler(__attribute__((unused)) struct rte_mbuf *pkt, struct onvm_pkt_meta *meta,
               __attribute__((unused)) struct onvm_nf_local_ctx *nf_local_ctx) {
        
        meta->destination = destination;
        meta->action = ONVM_NF_ACTION_DROP;

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
        nf_function_table->setup = &nf_setup;
        nf_function_table->msg_handler = &nf_msg_handler;

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

        if (parse_app_args(argc, argv, progname) < 0) {
                onvm_nflib_stop(nf_local_ctx);
                rte_exit(EXIT_FAILURE, "Invalid command-line arguments\n");
        }

        onvm_nflib_run(nf_local_ctx);

        onvm_nflib_stop(nf_local_ctx);
        
        printf("If we reach here, program is ending\n");
        return 0;
}