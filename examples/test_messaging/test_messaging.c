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
 * test_messaging.c - unit test to ensure NFs can send messages to themselves
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
#define MAGIC_NUMBER 11

static uint32_t print_delay = 1000000;

static uint16_t destination;

struct test_msg_data{
        int tests_passed;
        int tests_failed;
        int test_phase;
        int test_msg_count;
        int ring_count;
        int mempool_count;
        uint16_t address;
        struct rte_mempool* msg_pool;
        struct rte_ring *msg_q;
};

void
nf_setup(struct onvm_nf_local_ctx *nf_local_ctx);

void
nf_msg_handler(void *msg_data, struct onvm_nf_local_ctx *nf_local_ctx);

/*
 * Frees memory allocated to the test_msg_data struct
 */ 
static int
destroy_test_msg_data(struct test_msg_data **test_msg_data){
        if(test_msg_data == NULL){
                return 0;
        }
        rte_free(*test_msg_data);
        (*test_msg_data) = NULL;
        return 0;
}

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
parse_app_args(int argc, char *argv[], const char *progname, struct onvm_nf *nf) {
        int c;
        struct test_msg_data *msg_params;
        static struct rte_mempool *nf_msg_pool;

        nf_msg_pool = rte_mempool_lookup(_NF_MSG_POOL_NAME);
        if (nf_msg_pool == NULL)
                rte_exit(EXIT_FAILURE, "No NF Message mempool - bye\n");

        msg_params = (struct test_msg_data *)rte_malloc(NULL, sizeof(struct test_msg_data), 0);
        msg_params->tests_passed = 0;
        msg_params->tests_failed = 0;
        msg_params->test_phase = 1;
        msg_params->address = nf->service_id;
        msg_params->msg_pool = nf_msg_pool;
        msg_params->msg_q = nf->msg_q;
        msg_params->ring_count = 0;
        msg_params->mempool_count = rte_mempool_avail_count(msg_params->msg_pool);
        msg_params->test_msg_count = 0;
        nf->data = (void *)msg_params;

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

/*
 * Sets up the NF. Initiates Unit Test
 */
void
nf_setup(__attribute__((unused)) struct onvm_nf_local_ctx *nf_local_ctx) {
        struct rte_mempool *pktmbuf_pool;

        pktmbuf_pool = rte_mempool_lookup(PKTMBUF_POOL_NAME);
        if (pktmbuf_pool == NULL) {
                onvm_nflib_stop(nf_local_ctx);
                rte_exit(EXIT_FAILURE, "Cannot find mbuf pool!\n");
        }

        printf("\nTEST MESSAGING STARTED\n");
        printf("---------------------------\n");

        struct test_msg_data *msg_params;

        msg_params = (struct test_msg_data *)nf_local_ctx->nf->data;            

        int *msg_int;
        msg_int = (int*)rte_malloc(NULL, sizeof(int), 0);

        if(msg_int == NULL){
                printf("Message was not able to be malloc'd\n");
        }

        *msg_int = MAGIC_NUMBER;
        int ret = onvm_nflib_send_msg_to_nf(msg_params->address, msg_int);
        
        if(ret != 0){
                printf("Message was unable to be sent\n");
        }
}

/*
 * Runs logic for tests
 */
void
nf_msg_handler(void *msg_data, __attribute__((unused)) struct onvm_nf_local_ctx *nf_local_ctx){

        struct test_msg_data *msg_params;

        msg_params = (struct test_msg_data *)nf_local_ctx->nf->data;

        // Tests if one message can be sent to itself
        if(1 == msg_params->test_phase){

                printf("TEST 1: Send/Receive One Message...\n");
                printf("---------------------------\n");

                msg_params->test_msg_count++;

                if(1 == msg_params->test_msg_count){
                        
                        if(rte_ring_count(msg_params->msg_q) != 0) {
                                printf("FAILED TEST 1: Shouldn't be any messages left, but there are %d in the ring. This may cause future tests to fail.\n", rte_ring_count(msg_params->msg_q));
                                printf("---------------------------\n");
                                while(rte_ring_count(msg_params->msg_q) != 0){
                                        rte_ring_dequeue(msg_params->msg_q, msg_data);
                                }
                                msg_params->tests_failed++;
                                msg_params->test_phase++;
                        }
                        if((int)rte_mempool_avail_count(msg_params->msg_pool) != msg_params->mempool_count - 1){
                                printf("FAILED TEST 1: %d messages have not been deallocated from the memory pool. There is a memory leak somewhere.\n", rte_mempool_avail_count(msg_params->msg_pool));
                                msg_params->tests_failed++;
                                msg_params->test_phase++;
                        }
                        else {
                                if(*((int *)msg_data) == MAGIC_NUMBER){
                                        printf("PASSED TEST 1\n");
                                        printf("---------------------------\n");
                                        msg_params->tests_passed++;
                                        msg_params->test_phase++;
                                }
                                else {
                                        printf("FAILED TEST 1: received %d instead of %d\n", *((int *)msg_data), MAGIC_NUMBER);
                                        printf("---------------------------\n");
                                        msg_params->tests_failed++;
                                        msg_params->test_phase++;
                                }
                        }

                }
                
                rte_free(msg_data);

                msg_params->test_msg_count = 0;

                printf("TEST 2: Send/Receive Multiple Messages...\n");
                printf("---------------------------\n");

                for(int i = 0; i < 10; i++){
                        int* msg_int = (int*)rte_malloc(NULL, sizeof(int), 0);
                        *msg_int = i;    
                        onvm_nflib_send_msg_to_nf(msg_params->address, msg_int);
                }

        }
        //Tests if multiple messages can be sent to itself
        else if(2 == msg_params->test_phase){
                
                if(*((int *)msg_data) != msg_params->test_msg_count) { 
                        printf("FAILED TEST 2: received %d instead of %d\n", *((int *)msg_data), msg_params->test_msg_count);
                        printf("---------------------------\n");
                        msg_params->tests_failed++;
                        msg_params->test_phase++;
                }
                else {
                        msg_params->test_msg_count++;
                }

                rte_free(msg_data);

                if(10 == msg_params->test_msg_count){

                        if(rte_ring_count(msg_params->msg_q) != 0) {
                                printf("FAILED TEST 2: Shouldn't be any messages left, but there are %d in the ring. This may cause future tests to fail.\n", rte_ring_count(msg_params->msg_q));
                                printf("---------------------------\n");
                                while(rte_ring_count(msg_params->msg_q) != 0){
                                        rte_ring_dequeue(msg_params->msg_q, msg_data);
                                }
                                msg_params->tests_failed++;
                                msg_params->test_phase++;
                        }
                        if((int)rte_mempool_avail_count(msg_params->msg_pool) != msg_params->mempool_count - 1){
                                printf("FAILED TEST 2: %d messages have not been deallocated from the memory pool. There is a memory leak somewhere.\n", rte_mempool_avail_count(msg_params->msg_pool));
                                msg_params->tests_failed++;
                                msg_params->test_phase++;
                        }
                        else {
                                printf("PASSED TEST 2\n");
                                printf("---------------------------\n");
                                msg_params->tests_passed++;
                                msg_params->test_phase++;
                                msg_params->test_msg_count = 0;
                        }

                }
                
                if(0 == msg_params->test_msg_count){
                        printf("TEST 3: Message Ring Overflow...\n");
                        printf("---------------------------\n");
                        for(int i = 0; i < (int)(rte_ring_get_size(msg_params->msg_q) + 2); i++){
                                int* msg_int = (int*)rte_malloc(NULL, sizeof(int), 0);
                                *msg_int = i;    
                                onvm_nflib_send_msg_to_nf(msg_params->address, msg_int);
                        }
                }

                // TODO: can we detect that we fail phase 2 from missing messages?

        }
        //Tests to see if even with a ring overflow it can still handle the messages
        else if(msg_params->test_phase == 3){
                
                if(*((int *)msg_data) != msg_params->test_msg_count) { 
                        printf("FAILED TEST 3: received %d instead of %d\n", *((int *)msg_data), msg_params->test_msg_count);
                        printf("---------------------------\n");
                        msg_params->tests_failed++;
                        msg_params->test_phase++;
                }
                else {
                        msg_params->test_msg_count++;
                }

                rte_free(msg_data);
                
                if((int)(rte_ring_get_size(msg_params->msg_q) - 1) == msg_params->test_msg_count){

                        if(rte_ring_count(msg_params->msg_q) != 0) {
                                printf("FAILED TEST 3: Shouldn't be any messages left, but there are %d in the ring. This may cause future tests to fail.\n", rte_ring_count(msg_params->msg_q));
                                printf("---------------------------\n");
                                while(rte_ring_count(msg_params->msg_q) != 0){
                                        rte_ring_dequeue(msg_params->msg_q, msg_data);
                                }
                                msg_params->tests_failed++;
                                msg_params->test_phase++;
                        }
                        if((int)rte_mempool_avail_count(msg_params->msg_pool) != msg_params->mempool_count - 1){
                                printf("FAILED TEST 3: %d messages have not been deallocated from the memory pool. There is a memory leak somewhere.\n", rte_mempool_avail_count(msg_params->msg_pool));
                                msg_params->tests_failed++;
                                msg_params->test_phase++;
                        }
                        else {
                                printf("PASSED TEST 3\n");
                                printf("---------------------------\n");
                                msg_params->tests_passed++;
                                msg_params->test_phase++;
                        }

                }

        }

        if(4 == msg_params->test_phase){
                printf("Passed %d/3 Tests\n", msg_params->tests_passed);
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

        if (parse_app_args(argc, argv, progname, nf_local_ctx->nf) < 0) {
                destroy_test_msg_data(nf_local_ctx->nf->data);
                onvm_nflib_stop(nf_local_ctx);
                rte_exit(EXIT_FAILURE, "Invalid command-line arguments\n");
        }

        onvm_nflib_run(nf_local_ctx);

        destroy_test_msg_data((struct test_msg_data**)&nf_local_ctx->nf->data);

        onvm_nflib_stop(nf_local_ctx);
        
        printf("If we reach here, program is ending\n");
        return 0;
}