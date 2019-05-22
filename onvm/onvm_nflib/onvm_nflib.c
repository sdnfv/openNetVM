/*********************************************************************
 *                     openNetVM
 *              https://sdnfv.github.io
 *
 *   BSD LICENSE
 *
 *   Copyright(c)
 *            2015-2017 George Washington University
 *            2015-2017 University of California Riverside
 *            2016-2017 Hewlett Packard Enterprise Development LP
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
static struct rte_mempool *nf_init_data_mp;

// Shared pool for mgr <--> NF messages
static struct rte_mempool *nf_msg_pool;

// Global NF context to manage signal termination
static struct onvm_nf_context *global_termination_context;

// Global NF specific signal handler
static handle_signal_func global_nf_signal_handler = NULL;

// Shared data for default service chain
struct onvm_service_chain *default_chain;

/* Shared data for onvm config */
struct onvm_configuration *onvm_config;

/* Flag to check if shared cpu mutex sleep/wakeup is enabled */
uint8_t ONVM_ENABLE_SHARED_CPU;

/***********************Internal Functions Prototypes*************************/

/*
 * Function that initialize a NF info data structure.
 *
 * Input  : the tag to name the NF
 * Output : the data structure initialized
 *
 */
static struct onvm_nf_init_data *
onvm_nflib_info_init(const char *tag);

/*
 * Function that initialize a nf tx info data structure.
 *
 * Input  : onvm_nf_init_data struct pointer
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
onvm_nflib_parse_args(int argc, char *argv[], struct onvm_nf_init_data *nf_init_data);

/*
 * Check if there are packets in this NF's RX Queue and process them
 */
static inline uint16_t
onvm_nflib_dequeue_packets(void **pkts, struct onvm_nf *nf, pkt_handler_func handler) __attribute__((always_inline));

/*
 * Check if there is a message available for this NF and process it
 */
static inline void
onvm_nflib_dequeue_messages(struct onvm_nf_context *nf_context) __attribute__((always_inline));

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
onvm_nflib_cleanup(struct onvm_nf_context *nf_context);

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
 * Start the NF by signaling manager that its ready to recieve packets
 *
 * Input: Pointer to context struct of this NF
 */
static int
onvm_nflib_start_nf(struct onvm_nf_context *nf_context);

/*
 * Entry point of the NF main loop
 *
 * Input: void pointer, points to the onvm_nf_context struct
 */
void *
onvm_nflib_thread_main_loop(void *arg);

/*
 * Function to initalize the shared cpu support
 *
 * Input  : Number of NF instances
 */
static void
init_shared_cpu_info(uint16_t instance_id);

/*
 * Signal handler to catch SIGINT/SIGTERM.
 *
 * Input : int corresponding to the signal catched
 *
 */
void
onvm_nflib_handle_signal(int signal);

/************************************API**************************************/

struct onvm_nf_context *
onvm_nflib_init_nf_context(void) {
        struct onvm_nf_context *nf_context;

        nf_context = (struct onvm_nf_context*)calloc(1, sizeof(struct onvm_nf_context));
        if (nf_context == NULL)
                rte_exit(EXIT_FAILURE, "Failed to allocate memory for NF context\n");

        nf_context->keep_running = 1;
        rte_atomic16_init(&nf_context->nf_init_finished);
        rte_atomic16_set(&nf_context->nf_init_finished, 0);

        return nf_context;
}

int
onvm_nflib_start_signal_handler(struct onvm_nf_context *nf_context, handle_signal_func nf_signal_handler) {
        /* Signal handling is global thus save global context */
        global_termination_context = nf_context;
        global_nf_signal_handler = nf_signal_handler;

        printf("[Press Ctrl-C to quit ...]\n");
        signal(SIGINT, onvm_nflib_handle_signal);
        signal(SIGTERM, onvm_nflib_handle_signal);

        return 0;
}

int
onvm_nflib_init(int argc, char *argv[], const char *nf_tag, struct onvm_nf_context *nf_context) {
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
        nf_context->nf_init_data = onvm_nflib_info_init(nf_tag);

        if ((retval_parse = onvm_nflib_parse_args(argc, argv, nf_context->nf_init_data)) < 0)
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

        if ((ret = onvm_nflib_start_nf(nf_context)) < 0)
                return ret;

        // Set to 3 because that is the bare minimum number of arguments, the config file will increase this number
        if (use_config) {
                return 3;
        }

        return retval_final;
}

int
onvm_nflib_run_callback(struct onvm_nf_context *nf_context, pkt_handler_func handler, callback_handler_func callback) {
        struct onvm_nf *nf;
        int ret;

        nf = nf_context->nf;
        nf->nf_mode = NF_MODE_SINGLE;

        /* Save the nf specifc functions, can be used if NFs spawn new threads */
        nf->nf_pkt_function = handler;
        nf->nf_callback_function = callback;

        pthread_t main_loop_thread;
        if ((ret = pthread_create(&main_loop_thread, NULL, onvm_nflib_thread_main_loop, (void *)nf_context)) < 0) {
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
        struct onvm_nf_context *nf_context;
        struct onvm_nf *nf;
        uint16_t nb_pkts_added;
        pkt_handler_func handler;
        callback_handler_func callback;
        uint64_t start_time;
        int ret;

        nf_context = (struct onvm_nf_context *)arg;
        nf = nf_context->nf;
        onvm_threading_core_affinitize(nf->info->core);

        handler = nf->nf_pkt_function;
        callback = nf->nf_callback_function;

        printf("Sending NF_READY message to manager...\n");
        ret = onvm_nflib_nf_ready(nf);
        if (ret != 0)
                rte_exit(EXIT_FAILURE, "Unable to message manager\n");

        /* Run the setup function (this might send pkts so done after the state change) */
        if (nf->nf_setup_function != NULL)
                nf->nf_setup_function(nf_context);

        start_time = rte_get_tsc_cycles();
        for (; nf_context->keep_running;) {
                nb_pkts_added = onvm_nflib_dequeue_packets((void **)pkts, nf, handler);

                if (likely(nb_pkts_added > 0)) {
                        onvm_pkt_process_tx_batch(nf->nf_tx_mgr, pkts, nb_pkts_added, nf);
                }

                /* Flush the packet buffers */
                onvm_pkt_enqueue_tx_thread(nf->nf_tx_mgr->to_tx_buf, nf);
                onvm_pkt_flush_all_nfs(nf->nf_tx_mgr, nf);

                onvm_nflib_dequeue_messages(nf_context);
                if (callback != ONVM_NO_CALLBACK) {
                        nf_context->keep_running = !(*callback)(nf) && nf_context->keep_running;
                }

                if (nf->time_to_live && unlikely((rte_get_tsc_cycles() - start_time) *
                                          TIME_TTL_MULTIPLIER / rte_get_timer_hz() >= nf->time_to_live)) {
                        printf("Time to live exceeded, shutting down\n");
                        nf_context->keep_running = 0;
                }
                if (nf->pkt_limit && unlikely(nf->stats.rx >= (uint64_t) nf->pkt_limit * PKT_TTL_MULTIPLIER)) {
                        printf("Packet limit exceeded, shutting down\n");
                        nf_context->keep_running = 0;
                }
        }
        return NULL;
}

int
onvm_nflib_run(struct onvm_nf_context *nf_context, pkt_handler_func handler) {
        return onvm_nflib_run_callback(nf_context, handler, ONVM_NO_CALLBACK);
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
        startup_msg->msg_data = nf->info;
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
onvm_nflib_handle_msg(struct onvm_nf_msg *msg, struct onvm_nf_context *nf_context) {
        switch (msg->msg_type) {
                case MSG_STOP:
                        RTE_LOG(INFO, APP, "Shutting down...\n");
                        nf_context->keep_running = 0;
                        break;
                case MSG_SCALE:
                        RTE_LOG(INFO, APP, "Received scale message...\n");
                        onvm_nflib_scale((struct onvm_nf_scale_info*)msg->msg_data);
                        break;
                case MSG_FROM_NF:
                        RTE_LOG(INFO, APP, "Recieved MSG from other NF");
                        if (nf_context->nf->nf_handle_msg_function != NULL) {
                                nf_context->nf->nf_handle_msg_function(msg->msg_data, nf_context->nf);
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
onvm_nflib_stop(struct onvm_nf_context *nf_context) {
        /* Terminate children */
        onvm_nflib_terminate_children(nf_context->nf);
        /* Stop and free */
        onvm_nflib_cleanup(nf_context);
}

struct onvm_configuration *
onvm_nflib_get_onvm_config(void) {
        return onvm_config;
}

void
onvm_nflib_set_setup_function(struct onvm_nf *nf, setup_func setup) {
        nf->nf_setup_function = setup;
}

void
onvm_nflib_set_msg_handling_function(struct onvm_nf *nf, handle_msg_func nf_handle_msg) {
        nf->nf_handle_msg_function = nf_handle_msg;
}

int
onvm_nflib_scale(struct onvm_nf_scale_info *scale_info) {
        int ret;
        pthread_t app_thread;

        if (onvm_nflib_is_scale_info_valid(scale_info) < 0) {
                RTE_LOG(INFO, APP, "Scale info invalid\n");
                return -1;
        }

        rte_atomic16_inc(&nfs[scale_info->parent->instance_id].children_cnt);

        /* Careful, this is required for shared cpu scaling TODO: resolve */
        if (ONVM_ENABLE_SHARED_CPU)
                sleep(1);

        ret = pthread_create(&app_thread, NULL, &onvm_nflib_start_child, scale_info);
        if (ret < 0) {
                rte_atomic16_dec(&nfs[scale_info->parent->instance_id].children_cnt);
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

struct onvm_nf_scale_info *
onvm_nflib_get_empty_scaling_config(struct onvm_nf *parent) {
        struct onvm_nf_scale_info *scale_info;

        scale_info = rte_calloc("nf_scale_info", 1, sizeof(struct onvm_nf_scale_info), 0);
        scale_info->parent = parent;
        scale_info->instance_id = NF_NO_ID;
        scale_info->flags = 0;

        return scale_info;
}

struct onvm_nf_scale_info *
onvm_nflib_inherit_parent_config(struct onvm_nf *parent, void *data) {
        struct onvm_nf_scale_info *scale_info;

        scale_info = rte_calloc("nf_scale_info", 1, sizeof(struct onvm_nf_scale_info), 0);
        scale_info->parent = parent;
        scale_info->instance_id = NF_NO_ID;
        scale_info->service_id = parent->service_id;
        scale_info->tag = parent->tag;
        scale_info->core = parent->core;
        scale_info->flags = parent->info->flags;
        scale_info->data = data;
        scale_info->setup_func = parent->nf_setup_function;
        scale_info->handle_msg_function = parent->nf_handle_msg_function;
        if (parent->nf_mode == NF_MODE_SINGLE) {
                scale_info->pkt_func = parent->nf_pkt_function;
                scale_info->callback_func = parent->nf_callback_function;
        } else if (parent->nf_mode == NF_MODE_RING) {
                scale_info->adv_rings_func = parent->nf_advanced_rings_function;
        } else {
                RTE_LOG(INFO, APP, "Unknown NF mode detected\n");
                return NULL;
        }

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

        /* Lookup mempool for nf_init_data struct */
        nf_init_data_mp = rte_mempool_lookup(_NF_MEMPOOL_NAME);
        if (nf_init_data_mp == NULL)
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
                rte_exit(EXIT_FAILURE, "Cannot get nf_init_data ring");

        return 0;
}

static void
onvm_nflib_parse_config(struct onvm_configuration *config) {
        ONVM_ENABLE_SHARED_CPU = config->flags.ONVM_ENABLE_SHARED_CPU;
}

static int
onvm_nflib_start_nf(struct onvm_nf_context *nf_context) {
        struct onvm_nf_msg *startup_msg;
        struct onvm_nf_init_data *nf_init_data;
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

        if (!nf_context->keep_running) {
                return ONVM_SIGNAL_TERMINATION;
        }

        nf_init_data = nf_context->nf_init_data;

        /* Put this NF's info struct onto queue for manager to process startup */
        if (rte_mempool_get(nf_msg_pool, (void **)(&startup_msg)) != 0) {
                rte_mempool_put(nf_init_data_mp, nf_init_data);  // give back memory
                rte_exit(EXIT_FAILURE, "Cannot create startup msg");
        }

        /* Tell the manager we're ready to recieve packets */
        startup_msg->msg_type = MSG_NF_STARTING;
        startup_msg->msg_data = nf_init_data;
        if (rte_ring_enqueue(mgr_msg_queue, startup_msg) < 0) {
                rte_mempool_put(nf_init_data_mp, nf_init_data);  // give back mermory
                rte_mempool_put(nf_msg_pool, startup_msg);
                rte_exit(EXIT_FAILURE, "Cannot send nf_init_data to manager");
        }

        /* Wait for a NF id to be assigned by the manager */
        RTE_LOG(INFO, APP, "Waiting for manager to assign an ID...\n");
        for (; nf_init_data->status == (uint16_t)NF_WAITING_FOR_ID;) {
                sleep(1);
                if (!nf_context->keep_running) {
                        /* Wait because we sent a message to the onvm_mgr */
                        for (i = 0; i < NF_TERM_INIT_ITER_TIMES && nf_init_data->status != NF_STARTING; i++)
                                sleep(NF_TERM_WAIT_TIME);
                        return ONVM_SIGNAL_TERMINATION;
                }
        }

        /* This NF is trying to declare an ID already in use. */
        if (nf_init_data->status == NF_ID_CONFLICT) {
                rte_mempool_put(nf_init_data_mp, nf_init_data);
                rte_exit(NF_ID_CONFLICT, "Selected ID already in use. Exiting...\n");
        } else if (nf_init_data->status == NF_SERVICE_MAX) {
                rte_mempool_put(nf_init_data_mp, nf_init_data);
                rte_exit(NF_SERVICE_MAX, "Service ID must be less than %d\n", MAX_SERVICES);
        } else if (nf_init_data->status == NF_SERVICE_COUNT_MAX) {
                rte_mempool_put(nf_init_data_mp, nf_init_data);
                rte_exit(NF_SERVICE_COUNT_MAX, "Maximum amount of NF's per service spawned, must be less than %d",
                         MAX_NFS_PER_SERVICE);
        } else if (nf_init_data->status == NF_NO_IDS) {
                rte_mempool_put(nf_init_data_mp, nf_init_data);
                rte_exit(NF_NO_IDS, "There are no ids available for this NF\n");
        } else if (nf_init_data->status == NF_NO_CORES) {
                rte_mempool_put(nf_init_data_mp, nf_init_data);
                rte_exit(NF_NO_IDS, "There are no cores available for this NF\n");
        } else if (nf_init_data->status == NF_NO_DEDICATED_CORES) {
                rte_mempool_put(nf_init_data_mp, nf_init_data);
                rte_exit(NF_NO_IDS,
                         "There is no space to assign a dedicated core, "
                         "or manually selected core has NFs running\n");
        } else if (nf_init_data->status == NF_CORE_OUT_OF_RANGE) {
                rte_mempool_put(nf_init_data_mp, nf_init_data);
                rte_exit(NF_NO_IDS, "Requested core is not enabled or not in range\n");
        } else if (nf_init_data->status == NF_CORE_BUSY) {
                rte_mempool_put(nf_init_data_mp, nf_init_data);
                rte_exit(NF_NO_IDS, "Requested core is busy\n");
        } else if (nf_init_data->status != NF_STARTING) {
                rte_mempool_put(nf_init_data_mp, nf_init_data);
                rte_exit(EXIT_FAILURE, "Error occurred during manager initialization\n");
        }

        nf = &nfs[nf_init_data->instance_id];
        nf->context = nf_context;

        /* Signal handler will now do proper cleanup, only called from main thread */
        if (rte_atomic16_read(&nf_context->nf_init_finished) == 0) {
                nf_context->nf = nf;
                rte_atomic16_set(&nf_context->nf_init_finished, 1);
        }

        /* Set mode to RING, if normal mode used will be determined later */
        nf->nf_mode = NF_MODE_RING;

        /* Initialize empty NF's tx manager */
        onvm_nflib_nf_tx_mgr_init(nf);

        /* Set the parent id to none */
        nf->parent = 0;
        rte_atomic16_init(&nf->children_cnt);
        rte_atomic16_set(&nf->children_cnt, 0);

        /* Default to advanced rings, if normal nflib run is called change the mode later */
        nf->nf_mode = NF_MODE_RING;

        /* In case this instance_id is reused, clear all function pointers */
        nf->nf_pkt_function = NULL;
        nf->nf_callback_function = NULL;
        nf->nf_advanced_rings_function = NULL;
        nf->nf_setup_function = NULL;
        nf->nf_handle_msg_function = NULL;

        if (ONVM_ENABLE_SHARED_CPU) {
                RTE_LOG(INFO, APP, "Shared CPU support enabled\n");
                init_shared_cpu_info(nf_init_data->instance_id);
        }

        RTE_LOG(INFO, APP, "Using Instance ID %d\n", nf_init_data->instance_id);
        RTE_LOG(INFO, APP, "Using Service ID %d\n", nf_init_data->service_id);
        RTE_LOG(INFO, APP, "Running on core %d\n", nf_init_data->core);

        if (nf_init_data->time_to_live)
                RTE_LOG(INFO, APP, "Time to live set to %u\n", nf_init_data->time_to_live);
        if (nf_init_data->pkt_limit)
                RTE_LOG(INFO, APP, "Packet limit (rx) set to %u\n", nf_init_data->pkt_limit);

        /*
         * Allow this for cases when there is not enough cores and using 
         * the shared cpu mode is not an option
         */
        if (ONVM_CHECK_BIT(nf_init_data->flags, SHARE_CORE_BIT) && !ONVM_ENABLE_SHARED_CPU)
                RTE_LOG(WARNING, APP, "Requested shared cpu core allocation but shared cpu mode is NOT "
                                      "enabled, this will hurt performance, proceed with caution\n");

        RTE_LOG(INFO, APP, "Finished Process Init.\n");

        return 0;
}

static inline uint16_t
onvm_nflib_dequeue_packets(void **pkts, struct onvm_nf *nf, pkt_handler_func handler) {
        struct onvm_pkt_meta *meta;
        uint16_t i, nb_pkts;
        struct packet_buf tx_buf;
        int ret_act;

        /* Dequeue all packets in ring up to max possible. */
        nb_pkts = rte_ring_dequeue_burst(nf->rx_q, pkts, PACKET_READ_SIZE, NULL);

        /* Possibly sleep if in shared cpu mode, otherwise return */
        if (unlikely(nb_pkts == 0)) {
                if (ONVM_ENABLE_SHARED_CPU) {
                        rte_atomic16_set(nf->sleep_state, 1);
                        sem_wait(nf->nf_mutex);
                }
                return 0;
        }

        tx_buf.count = 0;

        /* Give each packet to the user proccessing function */
        for (i = 0; i < nb_pkts; i++) {
                meta = onvm_get_pkt_meta((struct rte_mbuf *)pkts[i]);
                ret_act = (*handler)((struct rte_mbuf *)pkts[i], meta, nf);
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
onvm_nflib_dequeue_messages(struct onvm_nf_context *nf_context) {
        struct onvm_nf_msg *msg;
        struct rte_ring *msg_q;

        msg_q = nf_context->nf->msg_q;

        // Check and see if this NF has any messages from the manager
        if (likely(rte_ring_count(msg_q) == 0)) {
                return;
        }
        msg = NULL;
        rte_ring_dequeue(msg_q, (void **)(&msg));
        onvm_nflib_handle_msg(msg, nf_context);
        rte_mempool_put(nf_msg_pool, (void *)msg);
}

static void *
onvm_nflib_start_child(void *arg) {
        struct onvm_nf *parent;
        struct onvm_nf *child;
        struct onvm_nf_init_data *child_info;
        struct onvm_nf_scale_info *scale_info;
        struct onvm_nf_context *child_context;

        scale_info = (struct onvm_nf_scale_info *)arg;

        child_context = onvm_nflib_init_nf_context();
        parent = &nfs[scale_info->parent->instance_id];

        /* Initialize the info struct */
        child_info = onvm_nflib_info_init(parent->info->tag);

        /* Set child NF service and instance id */
        child_info->service_id = scale_info->service_id;
        child_info->instance_id = scale_info->instance_id;

        /* Set child NF core options */
        child_info->core = scale_info->flags;
        child_info->flags = scale_info->flags;

        RTE_LOG(INFO, APP, "Starting child NF with service %u, instance id %u\n", child_info->service_id,
                child_info->instance_id);
        child_context->nf_init_data = child_info;
        if (onvm_nflib_start_nf(child_context) < 0) {
                onvm_nflib_stop(child_context);
                return NULL;
        }

        child = &nfs[child_info->instance_id];
        child_context->nf = child;
        /* Save the parent id for future clean up */
        child->parent = parent->instance_id;
        /* Save nf specifc functions for possible future use */
        child->nf_setup_function = scale_info->setup_func;
        child->nf_pkt_function = scale_info->pkt_func;
        child->nf_callback_function = scale_info->callback_func;
        child->nf_advanced_rings_function = scale_info->adv_rings_func;
        child->nf_handle_msg_function = scale_info->handle_msg_function;
        /* Set nf state data */
        child->data = scale_info->data;

        if (child->nf_pkt_function) {
                onvm_nflib_run_callback(child_context, child->nf_pkt_function, child->nf_callback_function);
        } else if (child->nf_advanced_rings_function) {
                onvm_nflib_nf_ready(child);
                if (scale_info->setup_func != NULL)
                        scale_info->setup_func(child_context);
                scale_info->adv_rings_func(child_context);
        } else {
                /* Sanity check */
                rte_exit(EXIT_FAILURE, "Spawned NF doesn't have a pkt_handler or an advanced rings function\n");
        }

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
        global_termination_context->keep_running = 0;

        /* If NF didn't start yet no cleanup is necessary */
        if (rte_atomic16_read(&global_termination_context->nf_init_finished) == 0) {
                return;
        }

        /* If NF is asleep, wake it up */
        nf = global_termination_context->nf;
        if (ONVM_ENABLE_SHARED_CPU && rte_atomic16_read(nf->sleep_state) == 1) {
                rte_atomic16_set(nf->sleep_state, 0);
                sem_post(nf->nf_mutex);
        }

        if (global_nf_signal_handler != NULL)
                global_nf_signal_handler(sig);

        /* All the child termination will be done later in onvm_nflib_stop */
}

static int
onvm_nflib_is_scale_info_valid(struct onvm_nf_scale_info *scale_info) {
        if (scale_info->service_id == 0 || (scale_info->pkt_func == NULL && scale_info->adv_rings_func == NULL) ||
            (scale_info->pkt_func != NULL && scale_info->adv_rings_func != NULL))
                return -1;

        return 0;
}

static struct onvm_nf_init_data *
onvm_nflib_info_init(const char *tag) {
        void *mempool_data;
        struct onvm_nf_init_data *info;

        if (rte_mempool_get(nf_init_data_mp, &mempool_data) < 0) {
                rte_exit(EXIT_FAILURE, "Failed to get nf info memory\n");
        }

        if (mempool_data == NULL) {
                rte_exit(EXIT_FAILURE, "Client Info struct not allocated\n");
        }

        info = (struct onvm_nf_init_data *)mempool_data;
        info->instance_id = NF_NO_ID;
        info->core = rte_lcore_id();
        info->flags = 0;
        info->status = NF_WAITING_FOR_ID;

        /* Allocate memory for the tag so that onvm_mgr can access it */
        info->tag = rte_malloc("nf_tag", TAG_SIZE, 0);
        strncpy(info->tag, tag, TAG_SIZE);

        /* TTL and packet limit disabled by default */
        info->time_to_live = 0;
        info->pkt_limit = 0;

        return info;
}

static void
onvm_nflib_nf_tx_mgr_init(struct onvm_nf *nf) {
        nf->nf_tx_mgr = calloc(1, sizeof(struct queue_mgr));
        nf->nf_tx_mgr->mgr_type_t = NF;
        nf->nf_tx_mgr->to_tx_buf = calloc(1, sizeof(struct packet_buf));
        nf->nf_tx_mgr->id = nf->info->instance_id;
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
onvm_nflib_parse_args(int argc, char *argv[], struct onvm_nf_init_data *nf_init_data) {
        const char *progname = argv[0];
        int c, initial_instance_id;
        int service_id = -1;

        opterr = 0;
        while ((c = getopt (argc, argv, "n:r:t:l:ms")) != -1)
                switch (c) {
                        case 'n':
                                initial_instance_id = (uint16_t)strtoul(optarg, NULL, 10);
                                nf_init_data->instance_id = initial_instance_id;
                                break;
                        case 'r':
                                service_id = (uint16_t)strtoul(optarg, NULL, 10);
                                // Service id 0 is reserved
                                if (service_id == 0)
                                        service_id = -1;
                                break;
                        case 't':
                                nf_init_data->time_to_live = (uint16_t) strtoul(optarg, NULL, 10);
                                if (nf_init_data->time_to_live == 0) {
                                        fprintf(stderr, "Time to live argument can't be 0\n");
                                        return -1;
                                }
                                break;
                        case 'l':
                                nf_init_data->pkt_limit = (uint16_t) strtoul(optarg, NULL, 10);
                                if (nf_init_data->pkt_limit == 0) {
                                        fprintf(stderr, "Packet time to live argument can't be 0\n");
                                        return -1;
                                }
                                break;
                        case 'm':
                                nf_init_data->flags = ONVM_SET_BIT(nf_init_data->flags, MANUAL_CORE_ASSIGNMENT_BIT);
                                break;
                        case 's':
                                nf_init_data->flags = ONVM_SET_BIT(nf_init_data->flags, SHARE_CORE_BIT);
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
        nf_init_data->service_id = service_id;

        return optind;
}

static void
onvm_nflib_terminate_children(struct onvm_nf *nf) {
        uint16_t i, iter_cnt;

        iter_cnt = 0;
        /* Keep trying to shutdown children until there are none left */
        while (rte_atomic16_read(&nf->children_cnt) > 0 && iter_cnt < NF_TERM_STOP_ITER_TIMES) {
                for (i = 0; i < MAX_NFS; i++) {
                        if (nfs[i].context == NULL)
                               continue;
                        if (nfs[i].parent != nf->instance_id)
                                continue;

                        /* First stop child from running */
                        nfs[i].context->keep_running = 0;

                        if (!onvm_nf_is_valid(&nfs[i]))
                               continue;

                        /* Wake up the child if its sleeping */
                        if (ONVM_ENABLE_SHARED_CPU && rte_atomic16_read(nfs[i].sleep_state) == 1) {
                                rte_atomic16_set(nfs[i].sleep_state, 0);
                                sem_post(nfs[i].nf_mutex);
                        }
                }
                RTE_LOG(INFO, APP, "NF %d: Waiting for %d children to exit\n",
                        nf->instance_id, rte_atomic16_read(&nf->children_cnt));
                sleep(NF_TERM_WAIT_TIME);
                iter_cnt++;
        }

        if (rte_atomic16_read(&nf->children_cnt) > 0) {
                RTE_LOG(INFO, APP, "NF %d: Up to %d children may still be running and must be killed manually\n",
                        nf->instance_id, rte_atomic16_read(&nf->children_cnt));
        }
}

static void
onvm_nflib_cleanup(struct onvm_nf_context *nf_context) {
        struct onvm_nf *nf;
        struct onvm_nf_init_data *nf_init_data;

        if (nf_context == NULL || nf_context->nf_init_data == NULL) {
                return;
        }

        nf_init_data = nf_context->nf_init_data;
        nf = &nfs[nf_init_data->instance_id];

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
        }
        if (nf->nf_tx_mgr != NULL) {
                free(nf->nf_tx_mgr);
                nf->nf_tx_mgr = NULL;
        }

        struct onvm_nf_msg *shutdown_msg;

        /* Put this NF's info struct back into queue for manager to ack shutdown */
        if (mgr_msg_queue == NULL) {
                rte_mempool_put(nf_init_data_mp, nf_init_data);  // give back mermory
                rte_exit(EXIT_FAILURE, "Cannot get nf_init_data ring for shutdown");
        }
        if (rte_mempool_get(nf_msg_pool, (void **)(&shutdown_msg)) != 0) {
                rte_mempool_put(nf_init_data_mp, nf_init_data);  // give back mermory
                rte_exit(EXIT_FAILURE, "Cannot create shutdown msg");
        }

        shutdown_msg->msg_type = MSG_NF_STOPPING;
        shutdown_msg->msg_data = nf_init_data;

        if (rte_ring_enqueue(mgr_msg_queue, shutdown_msg) < 0) {
                rte_mempool_put(nf_init_data_mp, nf_init_data);  // give back mermory
                rte_mempool_put(nf_msg_pool, shutdown_msg);
                rte_exit(EXIT_FAILURE, "Cannot send nf_init_data to manager for shutdown");
        }

        /* Cleanup context */
        if (nf->context != NULL) {
                free(nf->context);
                nf->context = NULL;
        }
}

static void
init_shared_cpu_info(uint16_t instance_id) {
        key_t key;
        int shmid;
        char *shm;
        struct onvm_nf *nf;
        const char *sem_name;

        nf = &nfs[instance_id];
        sem_name = get_sem_name(instance_id);

        nf->nf_mutex = sem_open(sem_name, 0, 0666, 0);
        if (nf->nf_mutex == SEM_FAILED)
                rte_exit(EXIT_FAILURE, "Unable to execute semphore for NF %d\n", instance_id);

        /* Get flag which is shared by server */
        key = get_rx_shmkey(instance_id);
        if ((shmid = shmget(key, SHMSZ, 0666)) < 0)
                rte_exit(EXIT_FAILURE, "Unable to locate the segment for NF %d\n", instance_id);

        if ((shm = shmat(shmid, NULL, 0)) == (char *) -1)
                rte_exit(EXIT_FAILURE, "Can not attach the shared segment to the NF space for NF %d\n", instance_id);

        nf->sleep_state = (rte_atomic16_t *)shm;
}
