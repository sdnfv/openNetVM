/*********************************************************************
 *                     openNetVM
 *              https://sdnfv.github.io
 *
 *   BSD LICENSE
 *
 *   Copyright(c)
 *            2015-2019 George Washington University
 *            2015-2019 University of California Riverside
 *            2016-2019 Hewlett Packard Enterprise Development LP
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

                                  onvm_nflib.c


                  File containing all functions of the NF API


******************************************************************************/

/***************************Standard C library********************************/

#include <getopt.h>
#include <signal.h>

/******************************DPDK libraries*********************************/
#include "rte_malloc.h"

/*****************************Internal headers********************************/

#include "onvm_includes.h"
#include "onvm_nflib.h"
#include "onvm_sc_common.h"

/**********************************Macros*************************************/

// Possible NF packet consuming modes
#define NF_MODE_UNKNOWN 0
#define NF_MODE_SINGLE 1
#define NF_MODE_RING 2

#define ONVM_NO_CALLBACK NULL

/******************************Global Variables*******************************/

// Shared data for host port information
struct port_info *ports;

// Shared data for core information
struct core_status *cores;

// ring used for NF -> mgr messages (like startup & shutdown)
static struct rte_ring *mgr_msg_queue;

// Shared data from server. We update statistics here
struct onvm_nf *nfs;

// Shared data from manager, has information used for nf_side tx
uint16_t **services;
uint16_t *nf_per_service_count;

// Shared pool for all NFs info
static struct rte_mempool *nf_init_cfg_mp;

// Shared pool for mgr <--> NF messages
static struct rte_mempool *nf_msg_pool;

// Global NF context to manage signal termination
static struct onvm_nf_local_ctx *main_nf_local_ctx;

// Global NF specific signal handler
static handle_signal_func global_nf_signal_handler = NULL;

// Shared data for default service chain
struct onvm_service_chain *default_chain;

/* Shared data for onvm config */
struct onvm_configuration *onvm_config;

/* Flag to check if shared core mutex sleep/wakeup is enabled */
uint8_t ONVM_NF_SHARE_CORES;

/***********************Internal Functions Prototypes*************************/

/*
 * Function that initialize a nf tx info data structure.
 *
 * Input  : onvm_nf_init_cfg struct pointer
 * Output : the data structure initialized
 *
 */
static void
onvm_nflib_nf_tx_mgr_init(struct onvm_nf *nf);

/*
 * Function printing an explanation of command line instruction for a NF.
 *
 * Input : name of the executable containing the NF
 *
 */
static void
onvm_nflib_usage(const char *progname);

/*
 * Function that parses the global arguments common to all NFs.
 *
 * Input  : the number of arguments (following C standard library convention)
 *          an array of strings representing these arguments
 * Output : an error code
 *
 */
static int
onvm_nflib_parse_args(int argc, char *argv[], struct onvm_nf_init_cfg *nf_init_cfg);

/*
 * Check if there are packets in this NF's RX Queue and process them
 */
static inline uint16_t
onvm_nflib_dequeue_packets(void **pkts, struct onvm_nf_local_ctx *nf_local_ctx,
                           nf_pkt_handler_fn handler) __attribute__((always_inline));

/*
 * Check if there is a message available for this NF and process it
 */
static inline void
onvm_nflib_dequeue_messages(struct onvm_nf_local_ctx *nf_local_ctx) __attribute__((always_inline));

/*
 * Terminate the children spawned by the NF
 *
 * Input: Info struct corresponding to this NF
 */
static void
onvm_nflib_terminate_children(struct onvm_nf *nf);

/*
 * Set this NF's status to not running and release memory
 *
 * Input: pointer to context struct for this NF
 */
static void
onvm_nflib_cleanup(struct onvm_nf_local_ctx *nf_local_ctx);

/*
 * Entry point of a spawned child NF
 */
static void *
onvm_nflib_start_child(void *arg);

/*
 * Check if the NF info struct is valid
 */
static int
onvm_nflib_is_scale_info_valid(struct onvm_nf_scale_info *scale_info);

/*
 * Initialize dpdk as a secondary proc
 *
 * Input: arc, argv args
 */
static int
onvm_nflib_dpdk_init(int argc, char *argv[]);

/*
 * Lookup the structs shared with manager
 *
 */
static int
onvm_nflib_lookup_shared_structs(void);

/*
 * Parse the custom onvm config shared with manager
 *
 */
static void
onvm_nflib_parse_config(struct onvm_configuration *onvm_config);

/*
 * Entry point of the NF main loop
 *
 * Input: void pointer, points to the onvm_nf_local_ctx struct
 */
void *
onvm_nflib_thread_main_loop(void *arg);

/*
 * Function to initalize the shared core support
 *
 * Input  : Number of NF instances
 */
static void
init_shared_core_mode_info(uint16_t instance_id);

/*
 * Signal handler to catch SIGINT/SIGTERM.
 *
 * Input : int corresponding to the signal catched
 *
 */
void
onvm_nflib_handle_signal(int signal);

/************************************API**************************************/

struct onvm_nf_local_ctx *
onvm_nflib_init_nf_local_ctx(void) {
        struct onvm_nf_local_ctx *nf_local_ctx;

        nf_local_ctx = (struct onvm_nf_local_ctx*)calloc(1, sizeof(struct onvm_nf_local_ctx));
        if (nf_local_ctx == NULL)
                rte_exit(EXIT_FAILURE, "Failed to allocate memory for NF context\n");

        rte_atomic16_init(&nf_local_ctx->keep_running);
        rte_atomic16_set(&nf_local_ctx->keep_running, 1);
        rte_atomic16_init(&nf_local_ctx->nf_init_finished);
        rte_atomic16_set(&nf_local_ctx->nf_init_finished, 0);
        rte_atomic16_init(&nf_local_ctx->nf_stopped);
        rte_atomic16_set(&nf_local_ctx->nf_stopped, 0);

        return nf_local_ctx;
}

struct onvm_nf_function_table *
onvm_nflib_init_nf_function_table(void) {
        struct onvm_nf_function_table *nf_function_table;

        nf_function_table = (struct onvm_nf_function_table*)calloc(1, sizeof(struct onvm_nf_function_table));
        if (nf_function_table == NULL)
                rte_exit(EXIT_FAILURE, "Failed to allocate memory for NF context\n");

        return nf_function_table;
}

int
onvm_nflib_request_lpm(struct lpm_request *lpm_req) {
        struct onvm_nf_msg *request_message;
        int ret;

        ret = rte_mempool_get(nf_msg_pool, (void **) (&request_message));
        if (ret != 0) return ret;

        request_message->msg_type = MSG_REQUEST_LPM_REGION;
        request_message->msg_data = lpm_req;

        ret = rte_ring_enqueue(mgr_msg_queue, request_message);
        if (ret < 0) {
                rte_mempool_put(nf_msg_pool, request_message);
                return ret;
        }

        lpm_req->status = NF_WAITING_FOR_LPM;
        for (; lpm_req->status == (uint16_t) NF_WAITING_FOR_LPM;) {
                sleep(1);
        }

        rte_mempool_put(nf_msg_pool, request_message);
        return lpm_req->status;
}

int
onvm_nflib_start_signal_handler(struct onvm_nf_local_ctx *nf_local_ctx, handle_signal_func nf_signal_handler) {
        /* Signal handling is global thus save global context */
        main_nf_local_ctx = nf_local_ctx;
        global_nf_signal_handler = nf_signal_handler;

        printf("[Press Ctrl-C to quit ...]\n");
        signal(SIGINT, onvm_nflib_handle_signal);
        signal(SIGTERM, onvm_nflib_handle_signal);

        return 0;
}

int
onvm_nflib_init(int argc, char *argv[], const char *nf_tag, struct onvm_nf_local_ctx *nf_local_ctx,
                struct onvm_nf_function_table *nf_function_table) {
        struct onvm_nf_init_cfg *nf_init_cfg;
        int ret, retval_eal, retval_parse, retval_final;
        int use_config = 0;

        /* Check to see if a config file should be used */
        if (strcmp(argv[1], "-F") == 0) {
                use_config = 1;
                cJSON *config = onvm_config_parse_file(argv[2]);
                if (config == NULL) {
                        printf("Could not parse config file\n");
                        return -1;
                }

                if (onvm_config_create_nf_arg_list(config, &argc, &argv) < 0) {
                        printf("Could not create arg list\n");
                        cJSON_Delete(config);
                        return -1;
                }

                cJSON_Delete(config);
                printf("LOADED CONFIG SUCCESFULLY\n");
        }

        retval_eal = onvm_nflib_dpdk_init(argc, argv);
        if (retval_eal < 0)
                return retval_eal;

        /* Modify argc and argv to conform to getopt rules for parse_nflib_args */
        argc -= retval_eal;
        argv += retval_eal;

        /* Reset getopt global variables opterr and optind to their default values */
        opterr = 0;
        optind = 1;

        /* Lookup the info shared or created by the manager */
        onvm_nflib_lookup_shared_structs();

        /* Initialize the info struct */
        nf_init_cfg = onvm_nflib_init_nf_init_cfg(nf_tag);

        if ((retval_parse = onvm_nflib_parse_args(argc, argv, nf_init_cfg)) < 0)
                rte_exit(EXIT_FAILURE, "Invalid command-line arguments\n");

        /* Reset getopt global variables opterr and optind to their default values */
        opterr = 0;
        optind = 1;

        /*
         * Calculate the offset that the nf will use to modify argc and argv for its
         * getopt call. This is the sum of the number of arguments parsed by
         * rte_eal_init and parse_nflib_args. This will be decremented by 1 to assure
         * getopt is looking at the correct index since optind is incremented by 1 each
         * time "--" is parsed.
         * This is the value that will be returned if initialization succeeds.
         */
        retval_final = (retval_eal + retval_parse) - 1;

        if ((ret = onvm_nflib_start_nf(nf_local_ctx, nf_init_cfg)) < 0)
                return ret;

        /* Save the nf specifc function table */
        nf_local_ctx->nf->function_table = nf_function_table;

        // Set to 3 because that is the bare minimum number of arguments, the config file will increase this number
        if (use_config) {
                return 3;
        }

        return retval_final;
}

int
onvm_nflib_start_nf(struct onvm_nf_local_ctx *nf_local_ctx, struct onvm_nf_init_cfg *nf_init_cfg) {
        struct onvm_nf_msg *startup_msg;
        struct onvm_nf *nf;
        int i;

        /* Block signals, ensure only the parent signal handler gets the signal */
        sigset_t mask;
        sigemptyset(&mask);
        sigaddset(&mask, SIGINT);
        sigaddset(&mask, SIGTERM);
        if (pthread_sigmask(SIG_BLOCK, &mask, NULL) != 0) {
                printf("Could not set pthread sigmask\n");
                return -1;
        }
        if (!rte_atomic16_read(&nf_local_ctx->keep_running)) {
                return ONVM_SIGNAL_TERMINATION;
        }

        /* Put this NF's info struct onto queue for manager to process startup */
        if (rte_mempool_get(nf_msg_pool, (void **)(&startup_msg)) != 0) {
                rte_mempool_put(nf_init_cfg_mp, nf_init_cfg);  // give back memory
                rte_exit(EXIT_FAILURE, "Cannot create startup msg");
        }

        /* Tell the manager we're ready to recieve packets */
        startup_msg->msg_type = MSG_NF_STARTING;
        startup_msg->msg_data = nf_init_cfg;
        if (rte_ring_enqueue(mgr_msg_queue, startup_msg) < 0) {
                rte_mempool_put(nf_init_cfg_mp, nf_init_cfg);  // give back mermory
                rte_mempool_put(nf_msg_pool, startup_msg);
                rte_exit(EXIT_FAILURE, "Cannot send nf_init_cfg to manager");
        }

        /* Wait for a NF id to be assigned by the manager */
        RTE_LOG(INFO, APP, "Waiting for manager to assign an ID...\n");
        for (; nf_init_cfg->status == (uint16_t)NF_WAITING_FOR_ID;) {
                sleep(1);
                if (!rte_atomic16_read(&nf_local_ctx->keep_running)) {
                        /* Wait because we sent a message to the onvm_mgr */
                        for (i = 0; i < NF_TERM_INIT_ITER_TIMES && nf_init_cfg->status != NF_STARTING; i++) {
                                printf("Waiting for onvm_mgr to recieve the message before shutting down\n");
                                sleep(NF_TERM_WAIT_TIME);
                        }
                        /* Mark init as finished, even though we're exiting onvm_nflib_stop will do proper cleanup */
                        if (nf_init_cfg->status == NF_STARTING) {
                                nf_local_ctx->nf = &nfs[nf_init_cfg->instance_id];
                                rte_atomic16_set(&nf_local_ctx->nf_init_finished, 1);
                        }
                        return ONVM_SIGNAL_TERMINATION;
                }
        }

        /* This NF is trying to declare an ID already in use. */
        if (nf_init_cfg->status == NF_ID_CONFLICT) {
                rte_mempool_put(nf_init_cfg_mp, nf_init_cfg);
                printf("%s", "Selected ID already in use. Exiting...\n");
                return -NF_ID_CONFLICT;
        } else if (nf_init_cfg->status == NF_SERVICE_MAX) {
                rte_mempool_put(nf_init_cfg_mp, nf_init_cfg);
                printf("Service ID must be less than %d\n", MAX_SERVICES);
                return -NF_SERVICE_MAX;
        } else if (nf_init_cfg->status == NF_SERVICE_COUNT_MAX) {
                rte_mempool_put(nf_init_cfg_mp, nf_init_cfg);
                printf("Maximum amount of NF's per service spawned, must be less than %d", MAX_NFS_PER_SERVICE);
                return -NF_SERVICE_COUNT_MAX;
        } else if (nf_init_cfg->status == NF_NO_IDS) {
                rte_mempool_put(nf_init_cfg_mp, nf_init_cfg);
                printf("There are no ids available for this NF\n");
                return -NF_NO_IDS;
        } else if (nf_init_cfg->status == NF_NO_CORES) {
                rte_mempool_put(nf_init_cfg_mp, nf_init_cfg);
                printf("There are no cores available for this NF\n");
                return -NF_NO_CORES;
        } else if (nf_init_cfg->status == NF_NO_DEDICATED_CORES) {
                rte_mempool_put(nf_init_cfg_mp, nf_init_cfg);
                printf("There is no space to assign a dedicated core, or the selected core has NFs running\n");
                return -NF_NO_DEDICATED_CORES;
        } else if (nf_init_cfg->status == NF_CORE_OUT_OF_RANGE) {
                rte_mempool_put(nf_init_cfg_mp, nf_init_cfg);
                printf("Requested core is not enabled or not in range\n");
                return -NF_CORE_OUT_OF_RANGE;
        } else if (nf_init_cfg->status == NF_CORE_BUSY) {
                rte_mempool_put(nf_init_cfg_mp, nf_init_cfg);
                printf("Requested core is busy\n");
                return -NF_CORE_BUSY;
        } else if (nf_init_cfg->status != NF_STARTING) {
                rte_mempool_put(nf_init_cfg_mp, nf_init_cfg);
                printf("Error occurred during manager initialization\n");
                return -1;
        }

        nf = &nfs[nf_init_cfg->instance_id];

        /* Mark init as finished, sig handler/onvm_nflib_stop will now do proper cleanup */
        if (rte_atomic16_read(&nf_local_ctx->nf_init_finished) == 0) {
                nf_local_ctx->nf = nf;
                rte_atomic16_set(&nf_local_ctx->nf_init_finished, 1);
        }

        /* Init finished free the bootstrap struct */
        rte_mempool_put(nf_init_cfg_mp, nf_init_cfg);

        /* Initialize empty NF's tx manager */
        onvm_nflib_nf_tx_mgr_init(nf);

        /* Set the parent id to none */
        nf->thread_info.parent = 0;
        rte_atomic16_init(&nf->thread_info.children_cnt);
        rte_atomic16_set(&nf->thread_info.children_cnt, 0);

        /* In case this instance_id is reused, clear all function pointers */
        nf->function_table = NULL;

        if (ONVM_NF_SHARE_CORES) {
                RTE_LOG(INFO, APP, "Shared CPU support enabled\n");
                init_shared_core_mode_info(nf->instance_id);
        }

        RTE_LOG(INFO, APP, "Using Instance ID %d\n", nf->instance_id);
        RTE_LOG(INFO, APP, "Using Service ID %d\n", nf->service_id);
        RTE_LOG(INFO, APP, "Running on core %d\n", nf->thread_info.core);

        if (nf->flags.time_to_live)
                RTE_LOG(INFO, APP, "Time to live set to %u\n", nf->flags.time_to_live);
        if (nf->flags.pkt_limit)
                RTE_LOG(INFO, APP, "Packet limit (rx) set to %u\n", nf->flags.pkt_limit);

        /*
         * Allow this for cases when there is not enough cores and using 
         * the shared core mode is not an option
         */
        if (ONVM_CHECK_BIT(nf->flags.init_options, SHARE_CORE_BIT) && !ONVM_NF_SHARE_CORES)
               RTE_LOG(WARNING, APP, "Requested shared core allocation but shared core mode is NOT "
                                     "enabled, this will hurt performance, proceed with caution\n");

        RTE_LOG(INFO, APP, "Finished Process Init.\n");

        return 0;
}

int
onvm_nflib_run(struct onvm_nf_local_ctx *nf_local_ctx) {
        int ret;

        pthread_t main_loop_thread;
        if ((ret = pthread_create(&main_loop_thread, NULL, onvm_nflib_thread_main_loop, (void *)nf_local_ctx)) < 0) {
                rte_exit(EXIT_FAILURE, "Failed to spawn main loop thread, error %d", ret);
        }
        if ((ret = pthread_join(main_loop_thread, NULL)) < 0) {
                rte_exit(EXIT_FAILURE, "Failed to join with main loop thread, error %d", ret);
        }

        return 0;
}

void *
onvm_nflib_thread_main_loop(void *arg) {
        struct rte_mbuf *pkts[PACKET_READ_SIZE];
        struct onvm_nf_local_ctx *nf_local_ctx;
        struct onvm_nf *nf;
        uint16_t nb_pkts_added;
        uint64_t start_time;
        int ret;

        nf_local_ctx = (struct onvm_nf_local_ctx *)arg;
        nf = nf_local_ctx->nf;
        onvm_threading_core_affinitize(nf->thread_info.core);

        printf("Sending NF_READY message to manager...\n");
        ret = onvm_nflib_nf_ready(nf);
        if (ret != 0)
                rte_exit(EXIT_FAILURE, "Unable to message manager\n");

        /* Run the setup function (this might send pkts so done after the state change) */
        if (nf->function_table->setup != NULL)
                nf->function_table->setup(nf_local_ctx);

        start_time = rte_get_tsc_cycles();
        for (;rte_atomic16_read(&nf_local_ctx->keep_running) && rte_atomic16_read(&main_nf_local_ctx->keep_running);) {
                nb_pkts_added =
                        onvm_nflib_dequeue_packets((void **)pkts, nf_local_ctx, nf->function_table->pkt_handler);

                if (likely(nb_pkts_added > 0)) {
                        onvm_pkt_process_tx_batch(nf->nf_tx_mgr, pkts, nb_pkts_added, nf);
                }

                /* Flush the packet buffers */
                onvm_pkt_enqueue_tx_thread(nf->nf_tx_mgr->to_tx_buf, nf);
                onvm_pkt_flush_all_nfs(nf->nf_tx_mgr, nf);

                onvm_nflib_dequeue_messages(nf_local_ctx);
                if (nf->function_table->user_actions != ONVM_NO_CALLBACK) {
                        rte_atomic16_set(&nf_local_ctx->keep_running,
                                         !(*nf->function_table->user_actions)(nf_local_ctx) &&
                                         rte_atomic16_read(&nf_local_ctx->keep_running));
                }

                if (nf->flags.time_to_live && unlikely((rte_get_tsc_cycles() - start_time) *
                                          TIME_TTL_MULTIPLIER / rte_get_timer_hz() >= nf->flags.time_to_live)) {
                        printf("Time to live exceeded, shutting down\n");
                        rte_atomic16_set(&nf_local_ctx->keep_running, 0);
                }
                if (nf->flags.pkt_limit && unlikely(nf->stats.rx >= (uint64_t)nf->flags.pkt_limit *
                                                                    PKT_TTL_MULTIPLIER)) {
                        printf("Packet limit exceeded, shutting down\n");
                        rte_atomic16_set(&nf_local_ctx->keep_running, 0);
                }
        }
        return NULL;
}

int
onvm_nflib_return_pkt(struct onvm_nf *nf, struct rte_mbuf *pkt) {
        return onvm_nflib_return_pkt_bulk(nf, &pkt, 1);
}

int
onvm_nflib_return_pkt_bulk(struct onvm_nf *nf, struct rte_mbuf **pkts, uint16_t count) {
        unsigned int i;
        if (pkts == NULL || count == 0)
                return -1;
        if (unlikely(rte_ring_enqueue_bulk(nf->tx_q, (void **)pkts, count, NULL) == 0)) {
                nf->stats.tx_drop += count;
                for (i = 0; i < count; i++) {
                        rte_pktmbuf_free(pkts[i]);
                }
                return -ENOBUFS;
        } else {
                nf->stats.tx_returned += count;
        }

        return 0;
}

int
onvm_nflib_nf_ready(struct onvm_nf *nf) {
        struct onvm_nf_msg *startup_msg;
        int ret;

        /* Put this NF's info struct onto queue for manager to process startup */
        ret = rte_mempool_get(nf_msg_pool, (void **)(&startup_msg));
        if (ret != 0)
                return ret;

        startup_msg->msg_type = MSG_NF_READY;
        startup_msg->msg_data = nf;
        ret = rte_ring_enqueue(mgr_msg_queue, startup_msg);
        if (ret < 0) {
                rte_mempool_put(nf_msg_pool, startup_msg);
                return ret;
        }

        /* Don't start running before the onvm_mgr handshake is finished */
        while (nf->status != NF_RUNNING) {
                sleep(1);
        }

        return 0;
}

int
onvm_nflib_handle_msg(struct onvm_nf_msg *msg, struct onvm_nf_local_ctx *nf_local_ctx) {
        switch (msg->msg_type) {
                case MSG_STOP:
                        RTE_LOG(INFO, APP, "Shutting down...\n");
                        rte_atomic16_set(&nf_local_ctx->keep_running, 0);
                        break;
                case MSG_SCALE:
                        RTE_LOG(INFO, APP, "Received scale message...\n");
                        onvm_nflib_scale((struct onvm_nf_scale_info*)msg->msg_data);
                        break;
                case MSG_FROM_NF:
                        RTE_LOG(INFO, APP, "Recieved MSG from other NF");
                        if (nf_local_ctx->nf->function_table->msg_handler != NULL) {
                                nf_local_ctx->nf->function_table->msg_handler(msg->msg_data, nf_local_ctx);
                        }
                        break;
                case MSG_NOOP:
                default:
                        break;
        }

        return 0;
}

int
onvm_nflib_send_msg_to_nf(uint16_t dest, void *msg_data) {
        int ret;
        struct onvm_nf_msg *msg;

        ret = rte_mempool_get(nf_msg_pool, (void**)(&msg));
        if (ret != 0) {
                RTE_LOG(INFO, APP, "Oh the huge manatee! Unable to allocate msg from pool :(\n");
                return ret;
        }

        msg->msg_type = MSG_FROM_NF;
        msg->msg_data = msg_data;

        return rte_ring_enqueue(nfs[dest].msg_q, (void*)msg);
}

void
onvm_nflib_stop(struct onvm_nf_local_ctx *nf_local_ctx) {
        if (nf_local_ctx == NULL || nf_local_ctx->nf == NULL || rte_atomic16_read(&nf_local_ctx->nf_stopped) != 0) {
                return;
        }

        /* Ensure we only call nflib_stop once */
        rte_atomic16_set(&nf_local_ctx->nf_stopped, 1);

        /* Terminate children */
        onvm_nflib_terminate_children(nf_local_ctx->nf);
        /* Stop and free */
        onvm_nflib_cleanup(nf_local_ctx);
}

struct onvm_configuration *
onvm_nflib_get_onvm_config(void) {
        return onvm_config;
}

int
onvm_nflib_scale(struct onvm_nf_scale_info *scale_info) {
        int ret;
        pthread_t app_thread;

        if (onvm_nflib_is_scale_info_valid(scale_info) < 0) {
                RTE_LOG(INFO, APP, "Scale info invalid\n");
                return -1;
        }

        rte_atomic16_inc(&nfs[scale_info->parent->instance_id].thread_info.children_cnt);

        /* Careful, this is required for shared core scaling TODO: resolve */
        if (ONVM_NF_SHARE_CORES)
                sleep(1);

        ret = pthread_create(&app_thread, NULL, &onvm_nflib_start_child, scale_info);
        if (ret < 0) {
                rte_atomic16_dec(&nfs[scale_info->parent->instance_id].thread_info.children_cnt);
                RTE_LOG(INFO, APP, "Failed to create child thread\n");
                return -1;
        }

        ret = pthread_detach(app_thread);
        if (ret < 0) {
                RTE_LOG(INFO, APP, "Failed to detach child thread\n");
                return -1;
        }

        return 0;
}

struct onvm_nf_init_cfg *
onvm_nflib_init_nf_init_cfg(const char *tag) {
        void *mempool_data;
        struct onvm_nf_init_cfg *nf_init_cfg;

        if (rte_mempool_get(nf_init_cfg_mp, &mempool_data) < 0) {
                rte_exit(EXIT_FAILURE, "Failed to get nf nf_init_cfg memory\n");
        }

        if (mempool_data == NULL) {
                rte_exit(EXIT_FAILURE, "Client Info struct not allocated\n");
        }

        nf_init_cfg = (struct onvm_nf_init_cfg *)mempool_data;
        nf_init_cfg->instance_id = NF_NO_ID;
        nf_init_cfg->core = rte_lcore_id();
        nf_init_cfg->init_options = 0;
        nf_init_cfg->status = NF_WAITING_FOR_ID;

        /* Allocate memory for the tag so that onvm_mgr can access it */
        nf_init_cfg->tag = rte_malloc("nf_tag", TAG_SIZE, 0);
        strncpy(nf_init_cfg->tag, tag, TAG_SIZE);
        /* In case provided tag was longer than TAG_SIZE */
        nf_init_cfg->tag[TAG_SIZE - 1] = '\0';

        /* TTL and packet limit disabled by default */
        nf_init_cfg->time_to_live = 0;
        nf_init_cfg->pkt_limit = 0;

        return nf_init_cfg;
}

struct onvm_nf_init_cfg *
onvm_nflib_inherit_parent_init_cfg(struct onvm_nf *parent) {
        struct onvm_nf_init_cfg *nf_init_cfg;

        nf_init_cfg = onvm_nflib_init_nf_init_cfg(parent->tag);

        nf_init_cfg->service_id = parent->service_id;
        nf_init_cfg->core = parent->thread_info.core;
        nf_init_cfg->init_options = parent->flags.init_options;
        nf_init_cfg->time_to_live = parent->flags.time_to_live;
        nf_init_cfg->pkt_limit = parent->flags.pkt_limit;

        return nf_init_cfg;
}

struct onvm_nf_scale_info *
onvm_nflib_get_empty_scaling_config(struct onvm_nf *parent) {
        struct onvm_nf_scale_info *scale_info;

        scale_info = rte_calloc("nf_scale_info", 1, sizeof(struct onvm_nf_scale_info), 0);
        scale_info->nf_init_cfg = onvm_nflib_init_nf_init_cfg(parent->tag);
        scale_info->parent = parent;

        return scale_info;
}

struct onvm_nf_scale_info *
onvm_nflib_inherit_parent_config(struct onvm_nf *parent, void *data) {
        struct onvm_nf_scale_info *scale_info;

        scale_info = rte_calloc("nf_scale_info", 1, sizeof(struct onvm_nf_scale_info), 0);
        scale_info->nf_init_cfg = onvm_nflib_inherit_parent_init_cfg(parent);
        scale_info->parent = parent;
        scale_info->data = data;
        scale_info->function_table = parent->function_table;

        return scale_info;
}

struct onvm_service_chain *
onvm_nflib_get_default_chain(void) {
        return default_chain;
}

/******************************Helper functions*******************************/

static int
onvm_nflib_dpdk_init(int argc, char *argv[]) {
        int retval_eal = 0;
        if ((retval_eal = rte_eal_init(argc, argv)) < 0)
                return -1;
        return retval_eal;
}

static int
onvm_nflib_lookup_shared_structs(void) {
        const struct rte_memzone *mz_nf;
        const struct rte_memzone *mz_port;
        const struct rte_memzone *mz_cores;
        const struct rte_memzone *mz_scp;
        const struct rte_memzone *mz_services;
        const struct rte_memzone *mz_nf_per_service;
        const struct rte_memzone *mz_onvm_config;
        struct rte_mempool *mp;
        struct onvm_service_chain **scp;

        /* Lookup mempool for nf_init_cfg struct */
        nf_init_cfg_mp = rte_mempool_lookup(_NF_MEMPOOL_NAME);
        if (nf_init_cfg_mp == NULL)
                rte_exit(EXIT_FAILURE, "No NF Info mempool - bye\n");

        /* Lookup mempool for NF messages */
        nf_msg_pool = rte_mempool_lookup(_NF_MSG_POOL_NAME);
        if (nf_msg_pool == NULL)
                rte_exit(EXIT_FAILURE, "No NF Message mempool - bye\n");

        mp = rte_mempool_lookup(PKTMBUF_POOL_NAME);
        if (mp == NULL)
                rte_exit(EXIT_FAILURE, "Cannot get mempool for mbufs\n");

        /* Lookup mempool for NF structs */
        mz_nf = rte_memzone_lookup(MZ_NF_INFO);
        if (mz_nf == NULL)
                rte_exit(EXIT_FAILURE, "Cannot get NF structure mempool\n");
        nfs = mz_nf->addr;

        mz_services = rte_memzone_lookup(MZ_SERVICES_INFO);
        if (mz_services == NULL) {
                rte_exit(EXIT_FAILURE, "Cannot get service information\n");
        }
        services = mz_services->addr;

        mz_nf_per_service = rte_memzone_lookup(MZ_NF_PER_SERVICE_INFO);
        if (mz_nf_per_service == NULL) {
                rte_exit(EXIT_FAILURE, "Cannot get NF per service information\n");
        }
        nf_per_service_count = mz_nf_per_service->addr;

        mz_port = rte_memzone_lookup(MZ_PORT_INFO);
        if (mz_port == NULL)
                rte_exit(EXIT_FAILURE, "Cannot get port info structure\n");
        ports = mz_port->addr;

        mz_cores = rte_memzone_lookup(MZ_CORES_STATUS);
        if (mz_cores == NULL)
                rte_exit(EXIT_FAILURE, "Cannot get core status structure\n");
        cores = mz_cores->addr;

        mz_onvm_config = rte_memzone_lookup(MZ_ONVM_CONFIG);
        if (mz_onvm_config == NULL)
                rte_exit(EXIT_FAILURE, "Cannot get onvm config\n");
        onvm_config = mz_onvm_config->addr;
        onvm_nflib_parse_config(onvm_config);

        mz_scp = rte_memzone_lookup(MZ_SCP_INFO);
        if (mz_scp == NULL)
                rte_exit(EXIT_FAILURE, "Cannot get service chain info structre\n");
        scp = mz_scp->addr;
        default_chain = *scp;
        onvm_sc_print(default_chain);

        mgr_msg_queue = rte_ring_lookup(_MGR_MSG_QUEUE_NAME);
        if (mgr_msg_queue == NULL)
                rte_exit(EXIT_FAILURE, "Cannot get mgr message ring");

        return 0;
}

static void
onvm_nflib_parse_config(struct onvm_configuration *config) {
        ONVM_NF_SHARE_CORES = config->flags.ONVM_NF_SHARE_CORES;
}

static inline uint16_t
onvm_nflib_dequeue_packets(void **pkts, struct onvm_nf_local_ctx *nf_local_ctx, nf_pkt_handler_fn  handler) {
        struct onvm_nf *nf;
        struct onvm_pkt_meta *meta;
        uint16_t i, nb_pkts;
        struct packet_buf tx_buf;
        int ret_act;

        nf = nf_local_ctx->nf;

        /* Dequeue all packets in ring up to max possible. */
        nb_pkts = rte_ring_dequeue_burst(nf->rx_q, pkts, PACKET_READ_SIZE, NULL);

        /* Possibly sleep if in shared core mode, otherwise return */
        if (unlikely(nb_pkts == 0)) {
                if (ONVM_NF_SHARE_CORES) {
                        rte_atomic16_set(nf->shared_core.sleep_state, 1);
                        sem_wait(nf->shared_core.nf_mutex);
                }
                return 0;
        }

        tx_buf.count = 0;

        /* Give each packet to the user proccessing function */
        for (i = 0; i < nb_pkts; i++) {
                meta = onvm_get_pkt_meta((struct rte_mbuf *)pkts[i]);
                ret_act = (*handler)((struct rte_mbuf *)pkts[i], meta, nf_local_ctx);
                /* NF returns 0 to return packets or 1 to buffer */
                if (likely(ret_act == 0)) {
                        tx_buf.buffer[tx_buf.count++] = pkts[i];
                } else {
                        nf->stats.tx_buffer++;
                }
        }
        if (ONVM_NF_HANDLE_TX) {
                return nb_pkts;
        }

        onvm_pkt_enqueue_tx_thread(&tx_buf, nf);
        return 0;
}

static inline void
onvm_nflib_dequeue_messages(struct onvm_nf_local_ctx *nf_local_ctx) {
        struct onvm_nf_msg *msg;
        struct rte_ring *msg_q;

        msg_q = nf_local_ctx->nf->msg_q;

        // Check and see if this NF has any messages from the manager
        if (likely(rte_ring_count(msg_q) == 0)) {
                return;
        }
        msg = NULL;
        rte_ring_dequeue(msg_q, (void **)(&msg));
        onvm_nflib_handle_msg(msg, nf_local_ctx);
        rte_mempool_put(nf_msg_pool, (void *)msg);
}

static void *
onvm_nflib_start_child(void *arg) {
        struct onvm_nf *parent;
        struct onvm_nf *child;
        struct onvm_nf_init_cfg *child_nf_init_cfg;
        struct onvm_nf_scale_info *scale_info;
        struct onvm_nf_local_ctx *child_context;

        scale_info = (struct onvm_nf_scale_info *)arg;

        child_context = onvm_nflib_init_nf_local_ctx();
        parent = &nfs[scale_info->parent->instance_id];

        /* Initialize the info struct */
        child_nf_init_cfg = scale_info->nf_init_cfg;

        RTE_LOG(INFO, APP, "Starting child NF with service %u, instance id %u\n", child_nf_init_cfg->service_id,
                child_nf_init_cfg->instance_id);
        if (onvm_nflib_start_nf(child_context, child_nf_init_cfg) < 0) {
                onvm_nflib_stop(child_context);
                return NULL;
        }

        child = &nfs[child_nf_init_cfg->instance_id];
        child_context->nf = child;
        /* Save the parent id for future clean up */
        child->thread_info.parent = parent->instance_id;
        /* Save the nf specifc function table */
        child->function_table = scale_info->function_table;
        /* Set nf state data */
        child->data = scale_info->data;

        onvm_nflib_run(child_context);

        /* Clean up after the child */
        onvm_nflib_stop(child_context);

        if (scale_info != NULL) {
                rte_free(scale_info);
                scale_info = NULL;
        }

        return NULL;
}

void
onvm_nflib_handle_signal(int sig) {
        struct onvm_nf *nf;

        if (sig != SIGINT && sig != SIGTERM)
                return;

        /* Stops both starting and running NFs */
        rte_atomic16_set(&main_nf_local_ctx->keep_running, 0);

        /* If NF didn't start yet no cleanup is necessary */
        if (rte_atomic16_read(&main_nf_local_ctx->nf_init_finished) == 0) {
                return;
        }

        /* If NF is asleep, wake it up */
        nf = main_nf_local_ctx->nf;
        if (ONVM_NF_SHARE_CORES && rte_atomic16_read(nf->shared_core.sleep_state) == 1) {
                rte_atomic16_set(nf->shared_core.sleep_state, 0);
                sem_post(nf->shared_core.nf_mutex);
        }

        if (global_nf_signal_handler != NULL)
                global_nf_signal_handler(sig);

        /* All the child termination will be done later in onvm_nflib_stop */
}

static int
onvm_nflib_is_scale_info_valid(struct onvm_nf_scale_info *scale_info) {
        return scale_info->nf_init_cfg->service_id != 0 && scale_info->function_table != NULL &&
               scale_info->function_table->pkt_handler != NULL;
}


static void
onvm_nflib_nf_tx_mgr_init(struct onvm_nf *nf) {
        nf->nf_tx_mgr = calloc(1, sizeof(struct queue_mgr));
        nf->nf_tx_mgr->mgr_type_t = NF;
        nf->nf_tx_mgr->to_tx_buf = calloc(1, sizeof(struct packet_buf));
        nf->nf_tx_mgr->id = nf->instance_id;
        nf->nf_tx_mgr->nf_rx_bufs = calloc(MAX_NFS, sizeof(struct packet_buf));
}

static void
onvm_nflib_usage(const char *progname) {
        printf(
            "Usage: %s [EAL args] -- "
            "[-n <instance_id>] "
            "[-r <service_id>] "
            "[-t <time_to_live>] "
            "[-l <pkt_limit>] "
            "[-m (manual core assignment flag)] "
            "[-s (share core flag)]\n\n",
            progname);
}

static int
onvm_nflib_parse_args(int argc, char *argv[], struct onvm_nf_init_cfg *nf_init_cfg) {
        const char *progname = argv[0];
        int c, initial_instance_id;
        int service_id = -1;

        opterr = 0;
        while ((c = getopt (argc, argv, "n:r:t:l:ms")) != -1)
                switch (c) {
                        case 'n':
                                initial_instance_id = (uint16_t)strtoul(optarg, NULL, 10);
                                nf_init_cfg->instance_id = initial_instance_id;
                                break;
                        case 'r':
                                service_id = (uint16_t)strtoul(optarg, NULL, 10);
                                // Service id 0 is reserved
                                if (service_id == 0)
                                        service_id = -1;
                                break;
                        case 't':
                                nf_init_cfg->time_to_live = (uint16_t) strtoul(optarg, NULL, 10);
                                if (nf_init_cfg->time_to_live == 0) {
                                        fprintf(stderr, "Time to live argument can't be 0\n");
                                        return -1;
                                }
                                break;
                        case 'l':
                                nf_init_cfg->pkt_limit = (uint16_t) strtoul(optarg, NULL, 10);
                                if (nf_init_cfg->pkt_limit == 0) {
                                        fprintf(stderr, "Packet time to live argument can't be 0\n");
                                        return -1;
                                }
                                break;
                        case 'm':
                                nf_init_cfg->init_options = ONVM_SET_BIT(nf_init_cfg->init_options,
                                                                         MANUAL_CORE_ASSIGNMENT_BIT);
                                break;
                        case 's':
                                nf_init_cfg->init_options = ONVM_SET_BIT(nf_init_cfg->init_options, SHARE_CORE_BIT);
                                break;
                        case '?':
                                onvm_nflib_usage(progname);
                                if (optopt == 'n')
                                        fprintf(stderr, "Option -%c requires an argument.\n", optopt);
                                else if (isprint(optopt))
                                        fprintf(stderr, "Unknown option `-%c'.\n", optopt);
                                else
                                        fprintf(stderr, "Unknown option character `\\x%x'.\n", optopt);
                                return -1;
                        default:
                                return -1;
                }

        if (service_id == (uint16_t)-1) {
                /* Service ID is required */
                fprintf(stderr, "You must provide a nonzero service ID with -r\n");
                return -1;
        }
        nf_init_cfg->service_id = service_id;

        return optind;
}

static void
onvm_nflib_terminate_children(struct onvm_nf *nf) {
        uint16_t i, iter_cnt;

        iter_cnt = 0;
        /* Keep trying to shutdown children until there are none left */
        while (rte_atomic16_read(&nf->thread_info.children_cnt) > 0 && iter_cnt < NF_TERM_STOP_ITER_TIMES) {
                for (i = 0; i < MAX_NFS; i++) {
                        if (nfs[i].thread_info.parent != nf->instance_id)
                                continue;

                        if (!onvm_nf_is_valid(&nfs[i]))
                               continue;

                        /* Wake up the child if its sleeping */
                        if (ONVM_NF_SHARE_CORES && rte_atomic16_read(nfs[i].shared_core.sleep_state) == 1) {
                                rte_atomic16_set(nfs[i].shared_core.sleep_state, 0);
                                sem_post(nfs[i].shared_core.nf_mutex);
                        }
                }
                RTE_LOG(INFO, APP, "NF %d: Waiting for %d children to exit\n",
                        nf->instance_id, rte_atomic16_read(&nf->thread_info.children_cnt));
                sleep(NF_TERM_WAIT_TIME);
                iter_cnt++;
        }

        if (rte_atomic16_read(&nf->thread_info.children_cnt) > 0) {
                RTE_LOG(INFO, APP, "NF %d: Up to %d children may still be running and must be killed manually\n",
                        nf->instance_id, rte_atomic16_read(&nf->thread_info.children_cnt));
        }
}

static void
onvm_nflib_cleanup(struct onvm_nf_local_ctx *nf_local_ctx) {
        struct onvm_nf_msg *shutdown_msg;
        struct onvm_nf *nf;

        if (nf_local_ctx == NULL) {
                return;
        }

        /* In case init wasn't finished */
        if (rte_atomic16_read(&nf_local_ctx->nf_init_finished) == 0) {
                free(nf_local_ctx);
                nf_local_ctx = NULL;
                return;
        }

        nf = nf_local_ctx->nf;

        /* Sanity check */
        if (nf == NULL) {
                rte_exit(EXIT_FAILURE, "NF init finished but context->nf is NULL");
        }

        /* Cleanup state data */
        if (nf->data != NULL) {
                rte_free(nf->data);
                nf->data = NULL;
        }

        /* Cleanup for the nf_tx_mgr pointers */
        if (nf->nf_tx_mgr) {
                if (nf->nf_tx_mgr->to_tx_buf != NULL) {
                        free(nf->nf_tx_mgr->to_tx_buf);
                        nf->nf_tx_mgr->to_tx_buf = NULL;
                }
                if (nf->nf_tx_mgr->nf_rx_bufs != NULL) {
                        free(nf->nf_tx_mgr->nf_rx_bufs);
                        nf->nf_tx_mgr->nf_rx_bufs = NULL;
                }
                free(nf->nf_tx_mgr);
                nf->nf_tx_mgr = NULL;
        }

        if (nf->function_table) {
                free(nf->function_table);
                nf->function_table = NULL;
        }

        if (mgr_msg_queue == NULL)
                rte_exit(EXIT_FAILURE, "Cannot get mgr message ring for shutdown");
        if (rte_mempool_get(nf_msg_pool, (void **)(&shutdown_msg)) != 0)
                rte_exit(EXIT_FAILURE, "Cannot create shutdown msg");

        shutdown_msg->msg_type = MSG_NF_STOPPING;
        shutdown_msg->msg_data = nf;

        if (rte_ring_enqueue(mgr_msg_queue, shutdown_msg) < 0) {
                rte_mempool_put(nf_msg_pool, shutdown_msg);
                rte_exit(EXIT_FAILURE, "Cannot send mgr message to manager for shutdown");
        }

        /* Cleanup context */
        nf_local_ctx->nf = NULL;
        free(nf_local_ctx);
}

static void
init_shared_core_mode_info(uint16_t instance_id) {
        key_t key;
        int shmid;
        char *shm;
        struct onvm_nf *nf;
        const char *sem_name;

        nf = &nfs[instance_id];
        sem_name = get_sem_name(instance_id);

        nf->shared_core.nf_mutex = sem_open(sem_name, 0, 0666, 0);
        if (nf->shared_core.nf_mutex == SEM_FAILED)
                rte_exit(EXIT_FAILURE, "Unable to execute semphore for NF %d\n", instance_id);

        /* Get flag which is shared by server */
        key = get_rx_shmkey(instance_id);
        if ((shmid = shmget(key, SHMSZ, 0666)) < 0)
                rte_exit(EXIT_FAILURE, "Unable to locate the segment for NF %d\n", instance_id);

        if ((shm = shmat(shmid, NULL, 0)) == (char *) -1)
                rte_exit(EXIT_FAILURE, "Can not attach the shared segment to the NF space for NF %d\n", instance_id);

        nf->shared_core.sleep_state = (rte_atomic16_t *)shm;
}
