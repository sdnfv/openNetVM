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


#include "onvm_nflib.h"
#include "onvm_includes.h"
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
static struct rte_mempool *nf_info_mp;

// Shared pool for mgr <--> NF messages
static struct rte_mempool *nf_msg_pool;

// True as long as the NF should keep processing packets
static uint8_t keep_running = 1;

// Shared data for default service chain
struct onvm_service_chain *default_chain;

/***********************Internal Functions Prototypes*************************/


/*
 * Function that initialize a NF info data structure.
 *
 * Input  : the tag to name the NF
 * Output : the data structure initialized
 *
 */
static struct onvm_nf_info *
onvm_nflib_info_init(const char *tag);

/*
 * Function that initialize a nf tx info data structure.
 *
 * Input  : onvm_nf_info struct pointer  
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
onvm_nflib_parse_args(int argc, char *argv[], struct onvm_nf_info *nf_info);


/*
* Signal handler to catch SIGINT.
*
* Input : int corresponding to the signal catched
*
*/
static void
onvm_nflib_handle_signal(int sig);

/*
 * Check if there are packets in this NF's RX Queue and process them
 */
static inline uint16_t
onvm_nflib_dequeue_packets(void **pkts, struct onvm_nf *nf, pkt_handler_func handler) __attribute__((always_inline));

/*
 * Check if there is a message available for this NF and process it
 */
static inline void
onvm_nflib_dequeue_messages(struct onvm_nf *nf) __attribute__((always_inline));

/*
 * Set this NF's status to not running and release memory
 *
 * Input: Info struct corresponding to this NF
 */
static void
onvm_nflib_cleanup(struct onvm_nf_info *nf_info);

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
 * Start the NF by signaling manager that its ready to recieve packets
 *
 * Input: Info struct corresponding to this NF
 */
static int
onvm_nflib_start_nf(struct onvm_nf_info *nf_info);

/*
 * Entry point of the NF main loop
 *
 * Input: void pointer, points to the onvm_nf struct
 */
void *
onvm_nflib_thread_main_loop(void *arg);

/************************************API**************************************/


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
        struct rte_mempool *mp;
        struct onvm_service_chain **scp;

        /* Lookup mempool for nf_info struct */
        nf_info_mp = rte_mempool_lookup(_NF_MEMPOOL_NAME);
        if (nf_info_mp == NULL)
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

        mz_scp = rte_memzone_lookup(MZ_SCP_INFO);
        if (mz_scp == NULL)
                rte_exit(EXIT_FAILURE, "Cannot get service chain info structre\n");
        scp = mz_scp->addr;
        default_chain = *scp;
        onvm_sc_print(default_chain);

        mgr_msg_queue = rte_ring_lookup(_MGR_MSG_QUEUE_NAME);
        if (mgr_msg_queue == NULL)
                rte_exit(EXIT_FAILURE, "Cannot get nf_info ring");

        return 0;
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

        lpm_req->status = NF_WAITING_FOR_MGR;
        for (; lpm_req->status == (uint16_t) NF_WAITING_FOR_MGR;) {
                sleep(1);
        }

        rte_mempool_put(nf_msg_pool, request_message);
        return lpm_req->status;

}

static int
onvm_nflib_start_nf(struct onvm_nf_info *nf_info) {
        struct onvm_nf_msg *startup_msg;

        /* Put this NF's info struct onto queue for manager to process startup */
        if (rte_mempool_get(nf_msg_pool, (void**)(&startup_msg)) != 0) {
                rte_mempool_put(nf_info_mp, nf_info); // give back memory
                rte_exit(EXIT_FAILURE, "Cannot create startup msg");
        }

        /* Tell the manager we're ready to recieve packets */
        startup_msg->msg_type = MSG_NF_STARTING;
        startup_msg->msg_data = nf_info;
        if (rte_ring_enqueue(mgr_msg_queue, startup_msg) < 0) {
                rte_mempool_put(nf_info_mp, nf_info); // give back mermory
                rte_mempool_put(nf_msg_pool, startup_msg);
                rte_exit(EXIT_FAILURE, "Cannot send nf_info to manager");
        }

        /* Wait for a NF id to be assigned by the manager */
        RTE_LOG(INFO, APP, "Waiting for manager to assign an ID...\n");
        for (; nf_info->status == (uint16_t)NF_WAITING_FOR_ID ;) {
                sleep(1);
        }

        /* This NF is trying to declare an ID already in use. */
        if (nf_info->status == NF_ID_CONFLICT) {
                rte_mempool_put(nf_info_mp, nf_info);
                rte_exit(NF_ID_CONFLICT, "Selected ID already in use. Exiting...\n");
        } else if (nf_info->status == NF_SERVICE_MAX) {
                rte_mempool_put(nf_info_mp, nf_info);
                rte_exit(NF_SERVICE_MAX, "Service ID must be less than %d\n", MAX_SERVICES);
        } else if (nf_info->status == NF_SERVICE_COUNT_MAX) {
                rte_mempool_put(nf_info_mp, nf_info);
                rte_exit(NF_SERVICE_COUNT_MAX, "Maximum amount of NF's per service spawned, must be less than %d", MAX_NFS_PER_SERVICE);
        } else if(nf_info->status == NF_NO_IDS) {
                rte_mempool_put(nf_info_mp, nf_info);
                rte_exit(NF_NO_IDS, "There are no ids available for this NF\n");
        } else if(nf_info->status == NF_NO_CORES) {
                rte_mempool_put(nf_info_mp, nf_info);
                rte_exit(NF_NO_IDS, "There are no cores available for this NF\n");
        } else if(nf_info->status == NF_NO_DEDICATED_CORES) {
                rte_mempool_put(nf_info_mp, nf_info);
                rte_exit(NF_NO_IDS, "There is no space to assign a dedicated core, "
                                    "or manually selected core has NFs running\n");
        } else if(nf_info->status == NF_CORE_OUT_OF_RANGE) {
                rte_mempool_put(nf_info_mp, nf_info);
                rte_exit(NF_NO_IDS, "Requested core is not enabled or not in range\n");
        } else if(nf_info->status == NF_CORE_BUSY) {
                rte_mempool_put(nf_info_mp, nf_info);
                rte_exit(NF_NO_IDS, "Requested core is busy\n");
        } else if(nf_info->status != NF_STARTING) {
                rte_mempool_put(nf_info_mp, nf_info);
                rte_exit(EXIT_FAILURE, "Error occurred during manager initialization\n");
        }

        /* Set mode to UNKNOWN, to be determined later */
        nfs[nf_info->instance_id].nf_mode = NF_MODE_UNKNOWN;

        /* Initialize empty NF's tx manager */
        onvm_nflib_nf_tx_mgr_init(&nfs[nf_info->instance_id]);

        /* Set the parent id to none */
        nfs[nf_info->instance_id].parent = 0;

        RTE_LOG(INFO, APP, "Using Instance ID %d\n", nf_info->instance_id);
        RTE_LOG(INFO, APP, "Using Service ID %d\n", nf_info->service_id);
        RTE_LOG(INFO, APP, "Running on core %d\n", nf_info->core);

        RTE_LOG(INFO, APP, "Finished Process Init.\n");

        return 0;
}

int
onvm_nflib_init(int argc, char *argv[], const char *nf_tag, struct onvm_nf_info **nf_info_p) {
        int retval_parse, retval_final;
        struct onvm_nf_info *nf_info;
        int retval_eal = 0;
        int use_config = 0;

        /* Check to see if a config file should be used */
        if (strcmp(argv[1], "-F") == 0) {
                use_config = 1;
                cJSON* config = onvm_config_parse_file(argv[2]);
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
        argc -= retval_eal; argv += retval_eal;

        /* Reset getopt global variables opterr and optind to their default values */
        opterr = 0; optind = 1;

        /* Lookup the info shared or created by the manager */ 
        onvm_nflib_lookup_shared_structs();

        /* Initialize the info struct */
        nf_info = onvm_nflib_info_init(nf_tag);
        *nf_info_p = nf_info;

        if ((retval_parse = onvm_nflib_parse_args(argc, argv, nf_info)) < 0)
                rte_exit(EXIT_FAILURE, "Invalid command-line arguments\n");

        /* Reset getopt global variables opterr and optind to their default values */
        opterr = 0; optind = 1;

        /*
         * Calculate the offset that the nf will use to modify argc and argv for its
         * getopt call. This is the sum of the number of arguments parsed by
         * rte_eal_init and parse_nflib_args. This will be decremented by 1 to assure
         * getopt is looking at the correct index since optind is incremented by 1 each
         * time "--" is parsed.
         * This is the value that will be returned if initialization succeeds.
         */
        retval_final = (retval_eal + retval_parse) - 1;

        onvm_nflib_start_nf(nf_info);

        keep_running = 1;

        //Set to 3 because that is the bare minimum number of arguments, the config file will increase this number
        if (use_config) {
                return 3;
        }

        return retval_final;
}


int
onvm_nflib_run_callback(
        struct onvm_nf_info* info,
        pkt_handler_func handler,
        callback_handler_func callback)
{
        struct onvm_nf * nf;

        /* Listen for ^C and docker stop so we can exit gracefully */
        signal(SIGINT, onvm_nflib_handle_signal);
        signal(SIGTERM, onvm_nflib_handle_signal);

        nf = &nfs[info->instance_id];

        /* Don't allow conflicting NF modes */
        if (nf->nf_mode == NF_MODE_RING) {
                return -1;
        }
        nf->nf_mode = NF_MODE_SINGLE;

        /* Save the nf specifc functions, can be used if NFs spawn new threads */
        nf->nf_pkt_function = handler;
        nf->nf_callback_function = callback;

        pthread_t main_loop_thread;
        pthread_create(&main_loop_thread, NULL, onvm_nflib_thread_main_loop, (void *)nf);
        pthread_join(main_loop_thread, NULL);

        return 0;
}

void *
onvm_nflib_thread_main_loop(void *arg) {
        struct rte_mbuf *pkts[PACKET_READ_SIZE];
        struct onvm_nf * nf;
        uint16_t nb_pkts_added, i;
        struct onvm_nf_info* info;
        pkt_handler_func handler;
        callback_handler_func callback;
        int ret;

        nf = (struct onvm_nf *)arg;
        onvm_threading_core_affinitize(nf->info->core);

        info = nf->info;
        handler = nf->nf_pkt_function;
        callback = nf->nf_callback_function;

        /* Runs the NF setup function */
        if (nf->nf_setup_function != NULL)
                nf->nf_setup_function(info);

        printf("Sending NF_READY message to manager...\n");
        ret = onvm_nflib_nf_ready(info);
        if (ret != 0) rte_exit(EXIT_FAILURE, "Unable to message manager\n");

        printf("[Press Ctrl-C to quit ...]\n");
        for (; keep_running;) {
                nb_pkts_added = onvm_nflib_dequeue_packets((void **) pkts, nf, handler);

                if (likely(nb_pkts_added > 0)) {
                        onvm_pkt_process_tx_batch(nf->nf_tx_mgr, pkts, nb_pkts_added, nf);
                }

                /* Flush the packet buffers */
                onvm_pkt_enqueue_tx_thread(nf->nf_tx_mgr->to_tx_buf, nf);
                onvm_pkt_flush_all_nfs(nf->nf_tx_mgr, nf);

                onvm_nflib_dequeue_messages(nf);
                if (callback != ONVM_NO_CALLBACK) {
                        keep_running = !(*callback)(nf->info) && keep_running;
                }
        }

        /* Wait for children to quit */
        for (i = 0; i < MAX_NFS; i++)
                while (nfs[i].parent == nf->instance_id && nfs[i].info != NULL) {
                        sleep(1);
                }

        /* Stop and free */
        onvm_nflib_cleanup(info);

        return NULL;
}

int
onvm_nflib_run(struct onvm_nf_info* info, pkt_handler_func handler) {
        return onvm_nflib_run_callback(info, handler, ONVM_NO_CALLBACK);
}

int
onvm_nflib_return_pkt(struct onvm_nf_info* nf_info, struct rte_mbuf* pkt) {
        return onvm_nflib_return_pkt_bulk(nf_info, &pkt, 1);
}

int
onvm_nflib_return_pkt_bulk(struct onvm_nf_info *nf_info, struct rte_mbuf** pkts, uint16_t count)  {
        unsigned int i;
        if (pkts == NULL || count == 0) return -1;
        if (unlikely(rte_ring_enqueue_bulk(nfs[nf_info->instance_id].tx_q, (void **)pkts, count, NULL) == 0)) {
                nfs[nf_info->instance_id].stats.tx_drop += count;
                for (i = 0; i < count; i++) {
                        rte_pktmbuf_free(pkts[i]);
                }
                return -ENOBUFS;
        }
        else nfs[nf_info->instance_id].stats.tx_returned += count;
        return 0;
}

int
onvm_nflib_nf_ready(struct onvm_nf_info *info) {
        struct onvm_nf_msg *startup_msg;
        int ret;

        /* Put this NF's info struct onto queue for manager to process startup */
        ret = rte_mempool_get(nf_msg_pool, (void**)(&startup_msg));
        if (ret != 0) return ret;

        startup_msg->msg_type = MSG_NF_READY;
        startup_msg->msg_data = info;
        ret = rte_ring_enqueue(mgr_msg_queue, startup_msg);
        if (ret < 0) {
                rte_mempool_put(nf_msg_pool, startup_msg);
                return ret;
        }
        return 0;
}

int
onvm_nflib_handle_msg(struct onvm_nf_msg *msg, __attribute__((unused)) struct onvm_nf_info *info) {
        switch(msg->msg_type) {
        case MSG_STOP:
                RTE_LOG(INFO, APP, "Shutting down...\n");
                keep_running = 0;
                break;
        case MSG_SCALE:
                RTE_LOG(INFO, APP, "Received scale message...\n");
                onvm_nflib_scale((struct onvm_nf_scale_info*)msg->msg_data);
                break;
        case MSG_NOOP:
        default:
                break;
        }

        return 0;
}

void
onvm_nflib_stop(struct onvm_nf_info *nf_info) {
        onvm_nflib_cleanup(nf_info);
}


struct rte_ring *
onvm_nflib_get_tx_ring(struct onvm_nf_info* info) {
        if (info == NULL) {
                return NULL;
        }

        /* Don't allow conflicting NF modes */
        if (nfs[info->instance_id].nf_mode == NF_MODE_SINGLE) {
                return NULL;
        }

        /* We should return the tx_ring associated with the info struct */
        nfs[info->instance_id].nf_mode = NF_MODE_RING;
        return (struct rte_ring *)(&(nfs[info->instance_id].tx_q));
}


struct rte_ring *
onvm_nflib_get_rx_ring(struct onvm_nf_info* info) {
        if (info == NULL) {
                return NULL;
        }

        /* Don't allow conflicting NF modes */
        if (nfs[info->instance_id].nf_mode == NF_MODE_SINGLE) {
                return NULL;
        }

        /* We should return the rx_ring associated with the info struct */
        nfs[info->instance_id].nf_mode = NF_MODE_RING;
        return (struct rte_ring *)(&(nfs[info->instance_id].rx_q));
}


struct onvm_nf *
onvm_nflib_get_nf(uint16_t id) {
        /* Don't allow conflicting NF modes */
        if (nfs[id].nf_mode == NF_MODE_SINGLE) {
                return NULL;
        }

        /* We should return the NF struct referenced by instance id */
        nfs[id].nf_mode = NF_MODE_RING;
        return &nfs[id];
}

void
onvm_nflib_set_setup_function(struct onvm_nf_info *info, setup_func setup) {
        nfs[info->instance_id].nf_setup_function = setup;
}

int
onvm_nflib_scale(struct onvm_nf_scale_info *scale_info) {
        int ret;
        pthread_t app_thread;

        if (onvm_nflib_is_scale_info_valid(scale_info) < 0) {
                RTE_LOG(INFO, APP, "Scale info invalid\n");
                return -1;
        }

        ret = pthread_create(&app_thread, NULL, &onvm_nflib_start_child, scale_info);
        if (ret < 0) {
                RTE_LOG(INFO, APP, "Failed to create thread\n");
                return -1;
        }

        ret = pthread_detach(app_thread);
        if (ret < 0) {
                RTE_LOG(INFO, APP, "Failed to detach thread\n");
                return -1;
        }

        return 0;
}

struct onvm_nf_scale_info *
onvm_nflib_get_empty_scaling_config(struct onvm_nf_info *parent_info) {
        struct onvm_nf_scale_info *scale_info;

        scale_info = rte_calloc("nf_scale_info", 1, sizeof(struct onvm_nf_scale_info), 0);
        scale_info->parent = parent_info;
        scale_info->instance_id = NF_NO_ID;
        scale_info->flags = 0;

        return scale_info;
}

struct onvm_nf_scale_info *
onvm_nflib_inherit_parent_config(struct onvm_nf_info *parent_info, void *data) {
        struct onvm_nf_scale_info *scale_info;
        struct onvm_nf *parent_nf;

        parent_nf = &nfs[parent_info->instance_id];
        scale_info = rte_calloc("nf_scale_info", 1, sizeof(struct onvm_nf_scale_info), 0);
        scale_info->parent = parent_info;
        scale_info->instance_id = NF_NO_ID;
        scale_info->service_id = parent_info->service_id;
        scale_info->tag = parent_info->tag;
        scale_info->core = parent_info->core;
        scale_info->flags = parent_info->flags;
        scale_info->data = data;
        if (parent_nf->nf_mode == NF_MODE_SINGLE) {
                scale_info->pkt_func = parent_nf->nf_pkt_function;
                scale_info->setup_func = parent_nf->nf_setup_function;
                scale_info->callback_func = parent_nf->nf_callback_function;
        } else if (parent_nf->nf_mode == NF_MODE_RING) {
                scale_info->setup_func = parent_nf->nf_setup_function;
                scale_info->adv_rings_func = parent_nf->nf_advanced_rings_function;
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


static inline uint16_t
onvm_nflib_dequeue_packets(void **pkts, struct onvm_nf *nf, pkt_handler_func handler) {
        struct onvm_pkt_meta* meta;
        uint16_t i, nb_pkts;
        struct packet_buf tx_buf;
        int ret_act;

        /* Dequeue all packets in ring up to max possible. */
        nb_pkts = rte_ring_dequeue_burst(nf->rx_q, pkts, PACKET_READ_SIZE, NULL);

        /* Probably want to comment this out */
        if(unlikely(nb_pkts == 0)) {
                return 0;
        }

        tx_buf.count = 0;

        /* Give each packet to the user proccessing function */
        for (i = 0; i < nb_pkts; i++) {
                meta = onvm_get_pkt_meta((struct rte_mbuf*)pkts[i]);
                ret_act = (*handler)((struct rte_mbuf*)pkts[i], meta, nf->info);
                /* NF returns 0 to return packets or 1 to buffer */
                if(likely(ret_act == 0)) {
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
onvm_nflib_dequeue_messages(struct onvm_nf *nf) {
        struct onvm_nf_msg *msg;
        struct rte_ring *msg_q;

        msg_q = nf->msg_q;

        // Check and see if this NF has any messages from the manager
        if (likely(rte_ring_count(msg_q) == 0)) {
                return;
        }
        msg = NULL;
        rte_ring_dequeue(msg_q, (void**)(&msg));
        onvm_nflib_handle_msg(msg, nf->info);
        rte_mempool_put(nf_msg_pool, (void*)msg);
}

static void *
onvm_nflib_start_child(void *arg) {
        struct onvm_nf *parent;
        struct onvm_nf *child;
        struct onvm_nf_info *child_info;
        struct onvm_nf_scale_info *scale_info;

        scale_info = (struct onvm_nf_scale_info *) arg;

        parent = &nfs[scale_info->parent->instance_id];

        /* Initialize the info struct */
        child_info = onvm_nflib_info_init(parent->info->tag);

        /* Set child NF service and instance id */
        child_info->service_id = scale_info->service_id;
        child_info->instance_id = scale_info->instance_id;

        /* Set child NF core options */
        child_info->core = scale_info->flags;
        child_info->flags = scale_info->flags;

        RTE_LOG(INFO, APP, "Starting child NF with service %u, instance id %u\n", child_info->service_id, child_info->instance_id);
        onvm_nflib_start_nf(child_info);

        child = &nfs[child_info->instance_id];
        /* Save the parent id for future clean up */        
        child->parent = parent->instance_id;
        /* Save nf specifc functions for possible future use */
        child->nf_setup_function = scale_info->setup_func;
        child->nf_pkt_function = scale_info->pkt_func;
        child->nf_callback_function = scale_info->callback_func;
        child->nf_advanced_rings_function = scale_info->adv_rings_func;
        /* Set nf state data */
        child_info->data = scale_info->data;

        if (child->nf_pkt_function){
                onvm_nflib_run_callback(child_info, child->nf_pkt_function, child->nf_callback_function);
        } else if (child->nf_advanced_rings_function) {
                if (scale_info->setup_func != NULL)
                        scale_info->setup_func(child_info);
                onvm_nflib_nf_ready(child_info);
                scale_info->adv_rings_func(child_info);
        } else {
                /* Sanity check */
                rte_exit(EXIT_FAILURE, "Spawned NF doesn't have a pkt_handler or an advanced rings function\n");
        }

        if (scale_info != NULL) {
                rte_free(scale_info);
                scale_info = NULL;
        }

        return NULL;
}

static int
onvm_nflib_is_scale_info_valid(struct onvm_nf_scale_info *scale_info) {
        if (scale_info->service_id == 0 ||
            (scale_info->pkt_func == NULL && scale_info->adv_rings_func == NULL) ||
            (scale_info->pkt_func != NULL && scale_info->adv_rings_func != NULL))
                return -1;

        return 0;
}

static struct onvm_nf_info *
onvm_nflib_info_init(const char *tag)
{
        void *mempool_data;
        struct onvm_nf_info *info;

        if (rte_mempool_get(nf_info_mp, &mempool_data) < 0) {
                rte_exit(EXIT_FAILURE, "Failed to get nf info memory\n");
        }

        if (mempool_data == NULL) {
                rte_exit(EXIT_FAILURE, "Client Info struct not allocated\n");
        }

        info = (struct onvm_nf_info*) mempool_data;
        info->instance_id = NF_NO_ID;
        info->core = rte_lcore_id();
        info->flags = 0;
        info->status = NF_WAITING_FOR_ID;
        info->tag = tag;

        return info;
}

static void 
onvm_nflib_nf_tx_mgr_init(struct onvm_nf *nf)
{
        nf->nf_tx_mgr = calloc(1, sizeof(struct queue_mgr));
        nf->nf_tx_mgr->mgr_type_t = NF; 
        nf->nf_tx_mgr->to_tx_buf = calloc(1, sizeof(struct packet_buf));
        nf->nf_tx_mgr->id = nf->info->instance_id;
        nf->nf_tx_mgr->nf_rx_bufs = calloc(MAX_NFS, sizeof(struct packet_buf));
}


static void
onvm_nflib_usage(const char *progname) {
        printf("Usage: %s [EAL args] -- "
               "[-n <instance_id>]"
               "[-r <service_id>]"
               "[-m (manual core assignment flag)]"
               "[-s (share core flag)]\n\n", progname);
}


static int
onvm_nflib_parse_args(int argc, char *argv[], struct onvm_nf_info *nf_info) {
        const char *progname = argv[0];
        int c, initial_instance_id;
        int service_id = -1;

        opterr = 0;
        while ((c = getopt (argc, argv, "n:r:ms")) != -1)
                switch (c) {
                case 'n':
                        initial_instance_id = (uint16_t) strtoul(optarg, NULL, 10);
                        nf_info->instance_id = initial_instance_id;
                        break;
                case 'r':
                        service_id = (uint16_t) strtoul(optarg, NULL, 10);
                        // Service id 0 is reserved
                        if (service_id == 0) service_id = -1;
                        break;
                case 'm':
                        nf_info->flags = ONVM_SET_BIT(nf_info->flags, MANUAL_CORE_ASSIGNMENT_BIT);
                        break;
                case 's':
                        nf_info->flags = ONVM_SET_BIT(nf_info->flags, SHARE_CORE_BIT);
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
        nf_info->service_id = service_id;

        return optind;
}


static void
onvm_nflib_handle_signal(int sig)
{
        if (sig == SIGINT || sig == SIGTERM)
                keep_running = 0;
}

static void
onvm_nflib_cleanup(struct onvm_nf_info *nf_info)
{
        if (nf_info == NULL) {
                return;
        }

        if (nf_info->data != NULL) {
                rte_free(nf_info->data);
                nf_info->data = NULL;
        }

        struct onvm_nf_msg *shutdown_msg;

        /* Put this NF's info struct back into queue for manager to ack shutdown */
        if (mgr_msg_queue == NULL) {
                rte_mempool_put(nf_info_mp, nf_info); // give back mermory
                rte_exit(EXIT_FAILURE, "Cannot get nf_info ring for shutdown");
        }
        if (rte_mempool_get(nf_msg_pool, (void**)(&shutdown_msg)) != 0) {
                rte_mempool_put(nf_info_mp, nf_info); // give back mermory
                rte_exit(EXIT_FAILURE, "Cannot create shutdown msg");
        }

        shutdown_msg->msg_type = MSG_NF_STOPPING;
        shutdown_msg->msg_data = nf_info;

        if (rte_ring_enqueue(mgr_msg_queue, shutdown_msg) < 0) {
                rte_mempool_put(nf_info_mp, nf_info); // give back mermory
                rte_mempool_put(nf_msg_pool, shutdown_msg);
                rte_exit(EXIT_FAILURE, "Cannot send nf_info to manager for shutdown");
        }

}
