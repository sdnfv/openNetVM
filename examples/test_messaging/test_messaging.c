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

#define TEST_FAILED -1
#define TEST_INCOMPLETE 0
#define TEST_PASSED 1

#define TEST_TIME_LIMIT 5

struct test_msg_data{
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

void
nf_setup(struct onvm_nf_local_ctx *nf_local_ctx);

void
nf_msg_handler(void *msg_data, struct onvm_nf_local_ctx *nf_local_ctx);

/*
 * Frees memory allocated to the test_msg_data struct
 */ 
static void
destroy_test_msg_data(struct test_msg_data **test_msg_data) {
        if (test_msg_data == NULL) {
                return;
        }
        rte_free(*test_msg_data);
        (*test_msg_data) = NULL;
        return;
}

/*
 * Clears the message queues after each test
 */
static void
clear_msg_queue(struct test_msg_data *test_state) {
        void* msg = NULL;
        while (rte_ring_count(test_state->msg_q) != 0) {
                rte_ring_dequeue(test_state->msg_q, &msg);
                rte_mempool_put(test_state->msg_pool, (void *)msg);
        }
        rte_free(msg);
}

/*
 * Print a usage message
 */
static void
usage(const char *progname) {
        printf("Usage:\n");
        printf("%s [EAL args] -- [NF_LIB args]\n", progname);
}

/*
 * Parse the application arguments.
 */
static int
parse_app_args(int argc, char *argv[], const char *progname, __attribute__((unused)) struct onvm_nf *nf) {
        int c;
        while ((c = getopt(argc, argv, "p:")) != -1) {
                switch (c) {
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
 * Sets up the NF and state data
 */
void
nf_setup(struct onvm_nf_local_ctx *nf_local_ctx) {
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

        test_state = (struct test_msg_data *)rte_malloc(NULL, sizeof(struct test_msg_data), 0);
        test_state->tests_passed = 0;
        test_state->test_phase = 0;
        test_state->address = nf_local_ctx->nf->service_id;
        test_state->msg_pool = nf_msg_pool;
        test_state->msg_q = nf_local_ctx->nf->msg_q;
        test_state->ring_count = 0;
        test_state->mempool_count = 0;
        test_state->test_msg_count = 0;
        test_state->start_time = rte_get_tsc_cycles();
        memset(test_state->test_status, TEST_INCOMPLETE, sizeof(int)*3);
        nf_local_ctx->nf->data = (void *)test_state;
}

/*
 * Initiates each test phase
 */
static int
test_handler(struct onvm_nf_local_ctx *nf_local_ctx) {
        struct test_msg_data *test_state;
        test_state = (struct test_msg_data *)nf_local_ctx->nf->data;

        if (( (rte_get_tsc_cycles() - test_state->start_time) / rte_get_timer_hz()) > TEST_TIME_LIMIT) {
                // it took too long
                // we failed this test
                test_state->test_status[test_state->test_phase - 1] = TEST_FAILED;
                // reset start time clock
                test_state->start_time = rte_get_tsc_cycles();
                // clear ring and move on to next test
                clear_msg_queue(test_state);
                test_state->test_phase++;
        }

        // If we haven't started any tests yet, start phase 1
        if (0 == test_state->test_phase) {
                printf("\nTEST MESSAGING STARTED\n");
                printf("---------------------------\n");
                clear_msg_queue(test_state);
                test_state->mempool_count = rte_mempool_avail_count(test_state->msg_pool);
                int *msg_int;
                msg_int = (int*)rte_malloc(NULL, sizeof(int), 0);
                if (msg_int == NULL) {
                        printf("Message was not able to be malloc'd\n");
                }
                *msg_int = MAGIC_NUMBER;
                int ret = onvm_nflib_send_msg_to_nf(test_state->address, msg_int);
                if (ret != 0) {
                        printf("Message was unable to be sent\n");
                }
                test_state->test_phase++;
        } else if (1 == test_state->test_phase) {
                printf("TEST 1: Send/Receive One Message with correct data...\n");
                printf("---------------------------\n");
                if (TEST_PASSED == test_state->test_status[0]) {
                        printf("PASSED TEST 1\n");
                        printf("---------------------------\n");
                } else if (TEST_FAILED == test_state->test_status[0]) {
                        printf("FAILED TEST 1\n");
                        printf("---------------------------\n");
                }
                if (TEST_INCOMPLETE != test_state->test_status[0]) {
                        printf("TEST 2: Send/Receive Multiple Messages...\n");
                        printf("---------------------------\n");
                        clear_msg_queue(test_state);
                        for (int i = 0; i < 10; i++) {
                                int* msg_int = (int*)rte_malloc(NULL, sizeof(int), 0);
                                *msg_int = i;
                                onvm_nflib_send_msg_to_nf(test_state->address, msg_int);
                        }
                        test_state->test_phase++;
                }
        } else if (2 == test_state->test_phase) {
                if (TEST_PASSED == test_state->test_status[1]) {
                        printf("PASSED TEST 2\n");
                        printf("---------------------------\n");

                } else if (TEST_FAILED == test_state->test_status[1]) {
                        printf("FAILED TEST 2\n");
                        printf("---------------------------\n");
                }
                if (TEST_INCOMPLETE != test_state->test_status[1]) {
                        printf("TEST 3: Message Ring Overflow...\n");
                        printf("---------------------------\n");
                        clear_msg_queue(test_state);
                        for (int i = 0; i < (int)(rte_ring_get_size(test_state->msg_q) + 2); i++) {
                                int* msg_int = (int*)rte_malloc(NULL, sizeof(int), 0);
                                *msg_int = i;
                                onvm_nflib_send_msg_to_nf(test_state->address, msg_int);
                        }
                        test_state->test_phase++;
                }
        } else if (3 == test_state->test_phase) {
                if (TEST_PASSED == test_state->test_status[2]) {
                        printf("PASSED TEST 3\n");
                        printf("---------------------------\n");
                } else if (TEST_FAILED == test_state->test_status[2]) {
                        printf("FAILED TEST 3\n");
                        printf("---------------------------\n");
                }
                if (TEST_INCOMPLETE != test_state->test_status[2]) {
                        clear_msg_queue(test_state);
                        test_state->test_phase++;
                }
        } else if (4 == test_state->test_phase) {
                printf("Passed %d/3 Tests\n", test_state->tests_passed);
                return 1;
        }
        return 0;
}

/*
 * Checks for test correctness
 */
void
nf_msg_handler(void *msg_data, struct onvm_nf_local_ctx *nf_local_ctx) {
        struct test_msg_data *test_state;
        test_state = (struct test_msg_data *)nf_local_ctx->nf->data;
        // Tests if one message can be sent to itself
        if (1 == test_state->test_phase) {
                test_state->test_msg_count++;
                if (1 == test_state->test_msg_count) {
                        if (rte_ring_count(test_state->msg_q) != 0) {
                                printf("FAILURE: Shouldn't be any messages left, but there are %d in the ring. This may cause future tests to fail.\n", rte_ring_count(test_state->msg_q));
                                printf("---------------------------\n");
                                test_state->test_status[0] = TEST_FAILED;
                        } else {
                                if (*((int *)msg_data) == MAGIC_NUMBER) {
                                        test_state->tests_passed++;
                                        test_state->test_status[0] = TEST_PASSED;
                                        test_state->test_msg_count = 0;
                                } else {
                                        printf("FAILURE: Received %d instead of %d\n", *((int *)msg_data), MAGIC_NUMBER);
                                        printf("---------------------------\n");
                                        test_state->test_status[0] = TEST_FAILED;
                                }
                        }
                }
        } else if (2 == test_state->test_phase) { // Tests if multiple messages can be sent to itself
                if (*((int *)msg_data) != test_state->test_msg_count) {
                        printf("FAILURE: Received %d instead of %d\n", *((int *)msg_data), test_state->test_msg_count);
                        printf("---------------------------\n");
                        test_state->test_status[1] = TEST_FAILED;
                } else {
                        test_state->test_msg_count++;
                }
                if (10 == test_state->test_msg_count) {
                        if (rte_ring_count(test_state->msg_q) != 0) {
                                printf("FAILURE: Shouldn't be any messages left, but there are %d in the ring. This may cause future tests to fail.\n", rte_ring_count(test_state->msg_q));
                                printf("---------------------------\n");
                                test_state->test_status[1] = TEST_FAILED;
                        } else {
                                test_state->tests_passed++;
                                test_state->test_status[1] = TEST_PASSED;
                                test_state->test_msg_count = 0;
                        }
                }
        } else if (3 == test_state->test_phase) { // Tests to see if even with a ring overflow it can still handle the messages
                if (*((int *)msg_data) != test_state->test_msg_count) {
                        printf("FAILURE: Received %d instead of %d\n", *((int *)msg_data), test_state->test_msg_count);
                        printf("---------------------------\n");
                        test_state->test_status[2] = TEST_FAILED;
                } else {
                        test_state->test_msg_count++;
                }
                if ((int)(rte_ring_get_size(test_state->msg_q) - 1) == test_state->test_msg_count) {
                        if (rte_ring_count(test_state->msg_q) != 0) {
                                printf("FAILURE: Shouldn't be any messages left, but there are %d in the ring. This may cause future tests to fail.\n", rte_ring_count(test_state->msg_q));
                                printf("---------------------------\n");
                                test_state->test_status[2] = TEST_FAILED;
                        } else if ((int)rte_mempool_avail_count(test_state->msg_pool) == test_state->mempool_count - 1) {
                                // only pass if there wasn't a memory leak
                                test_state->tests_passed++;
                                test_state->test_status[2] = TEST_PASSED;
                                test_state->test_msg_count = 0;
                        } else {
                                printf("FAILURE: %d messages have not been deallocated back to the memory pool.\n", test_state->mempool_count - rte_mempool_avail_count(test_state->msg_pool));
                                printf("---------------------------\n");
                                test_state->test_status[2] = TEST_FAILED;
                        }
                }
        }
        printf("---------------------------\n");
        rte_free(msg_data);
}

/*
 * Not concerned with packets, so they are dropped.
 */
static int
packet_handler(__attribute__((unused)) struct rte_mbuf *pkt, struct onvm_pkt_meta *meta,
               __attribute__((unused)) struct onvm_nf_local_ctx *nf_local_ctx) {
        meta->action = ONVM_NF_ACTION_DROP;
        return 0;
}

/*
 * Creates function table and local context. Runs NF.
 */
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
        nf_function_table->user_actions = &test_handler;

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

        return 0;
}
