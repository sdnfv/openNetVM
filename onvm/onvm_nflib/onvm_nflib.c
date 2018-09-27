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

// User-given NF Client ID (defaults to manager assigned)
static uint16_t initial_instance_id = NF_NO_ID;

// User supplied service ID
static uint16_t service_id = -1;

// True as long as the NF should keep processing packets
static uint8_t keep_running = 1;

// Shared data for default service chain
struct onvm_service_chain *default_chain;

// Keeping track of the inital args (but only once), so we can use them again
static int first_init_flag = 1;
static int first_argc;
static char **first_argv;
static const char *first_nf_tag;

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
onvm_nflib_parse_args(int argc, char *argv[]);


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
static int
onvm_nflib_start_child(void *arg);

/************************************API**************************************/


int
onvm_nflib_init(int argc, char *argv[], const char *nf_tag, struct onvm_nf_info **nf_info_p) {
        const struct rte_memzone *mz_nf;
        const struct rte_memzone *mz_port;
        const struct rte_memzone *mz_scp;
        const struct rte_memzone *mz_services;
        const struct rte_memzone *mz_nf_per_service;
        struct rte_mempool *mp;
        struct onvm_service_chain **scp;
        struct onvm_nf_msg *startup_msg;
        struct onvm_nf_info *nf_info;
        int retval_parse, retval_final;
        int retval_eal = 0;

        if (first_init_flag && (retval_eal = rte_eal_init(argc, argv)) < 0) 
                return -1;

        /* Modify argc and argv to conform to getopt rules for parse_nflib_args */
        argc -= retval_eal; argv += retval_eal;

        if (first_init_flag) {
                /* Keep these for if we need to start another copy */
                first_argc = argc;
                first_argv = argv;
                first_nf_tag = nf_tag;
                first_init_flag = 0;
        }

        /* Reset getopt global variables opterr and optind to their default values */
        opterr = 0; optind = 1;

        if ((retval_parse = onvm_nflib_parse_args(argc, argv)) < 0)
                rte_exit(EXIT_FAILURE, "Invalid command-line arguments\n");

        /*
         * Calculate the offset that the nf will use to modify argc and argv for its
         * getopt call. This is the sum of the number of arguments parsed by
         * rte_eal_init and parse_nflib_args. This will be decremented by 1 to assure
         * getopt is looking at the correct index since optind is incremented by 1 each
         * time "--" is parsed.
         * This is the value that will be returned if initialization succeeds.
         */
        retval_final = (retval_eal + retval_parse) - 1;

        /* Reset getopt global variables opterr and optind to their default values */
        opterr = 0; optind = 1;

        /* Lookup mempool for nf_info struct */
        nf_info_mp = rte_mempool_lookup(_NF_MEMPOOL_NAME);
        if (nf_info_mp == NULL)
                rte_exit(EXIT_FAILURE, "No NF Info mempool - bye\n");

        /* Lookup mempool for NF messages */
        nf_msg_pool = rte_mempool_lookup(_NF_MSG_POOL_NAME);
        if (nf_msg_pool == NULL)
                rte_exit(EXIT_FAILURE, "No NF Message mempool - bye\n");

        /* Initialize the info struct */
        nf_info = onvm_nflib_info_init(nf_tag);
        *nf_info_p = nf_info;
        
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

        mz_scp = rte_memzone_lookup(MZ_SCP_INFO);
        if (mz_scp == NULL)
                rte_exit(EXIT_FAILURE, "Cannot get service chain info structre\n");
        scp = mz_scp->addr;
        default_chain = *scp;

        onvm_sc_print(default_chain);

        mgr_msg_queue = rte_ring_lookup(_MGR_MSG_QUEUE_NAME);
        if (mgr_msg_queue == NULL)
                rte_exit(EXIT_FAILURE, "Cannot get nf_info ring");

        /* Put this NF's info struct onto queue for manager to process startup */
        if (rte_mempool_get(nf_msg_pool, (void**)(&startup_msg)) != 0) {
                rte_mempool_put(nf_info_mp, nf_info); // give back memory
                rte_exit(EXIT_FAILURE, "Cannot create startup msg");
        }

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
        } else if(nf_info->status == NF_NO_IDS) {
                rte_mempool_put(nf_info_mp, nf_info);
                rte_exit(NF_NO_IDS, "There are no ids available for this NF\n");
        } else if(nf_info->status != NF_STARTING) {
                rte_mempool_put(nf_info_mp, nf_info);
                rte_exit(EXIT_FAILURE, "Error occurred during manager initialization\n");
        }

        /* Set mode to UNKNOWN, to be determined later */
        nfs[nf_info->instance_id].nf_mode = NF_MODE_UNKNOWN;
        
        /* Set core headroom = available cores for scaling or 0, if this is not the master core */
        nfs[nf_info->instance_id].headroom = rte_get_master_lcore() == rte_lcore_id()
                ? rte_lcore_count() - 1
                : 0;

        /* Initialize empty NF's tx manager */
        onvm_nflib_nf_tx_mgr_init(&nfs[nf_info->instance_id]);

        RTE_LOG(INFO, APP, "Using Instance ID %d\n", nf_info->instance_id);
        RTE_LOG(INFO, APP, "Using Service ID %d\n", nf_info->service_id);

        /* Tell the manager we're ready to recieve packets */
        keep_running = 1;

        RTE_LOG(INFO, APP, "Finished Process Init.\n");
        return retval_final;
}


int
onvm_nflib_run_callback(
        struct onvm_nf_info* info,
        pkt_handler_func handler,
        callback_handler_func callback)
{
        struct rte_mbuf *pkts[PACKET_READ_SIZE];
        struct onvm_nf * nf;
        uint16_t nb_pkts_added;
        int ret;

        nf = &nfs[info->instance_id];

        /* Don't allow conflicting NF modes */
        if (nf->nf_mode == NF_MODE_RING) {
                return -1;
        }
        nf->nf_mode = NF_MODE_SINGLE;

        if (nf->nf_setup_function != NULL)
                nf->nf_setup_function(info);

        printf("\nClient process %d handling packets\n", nf->instance_id);

        /* Listen for ^C and docker stop so we can exit gracefully */
        signal(SIGINT, onvm_nflib_handle_signal);
        signal(SIGTERM, onvm_nflib_handle_signal);

        if (nf->nf_pkt_function == NULL) {
                nf->nf_pkt_function = handler;
        }

        if (nf->nf_callback_function == NULL) {
                nf->nf_callback_function = callback;
        }

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
                onvm_pkt_enqueue_tx_thread(nf->nf_tx_mgr->to_tx_buf, nf->instance_id);
                onvm_pkt_flush_all_nfs(nf->nf_tx_mgr);

                onvm_nflib_dequeue_messages(nf);
                if (callback != ONVM_NO_CALLBACK) {
                        keep_running = !(*callback)() && keep_running;
                }
        }

        // Stop and free
        onvm_nflib_cleanup(info);

        return 0;
}

int
onvm_nflib_run(struct onvm_nf_info* info, pkt_handler_func handler) {
        return onvm_nflib_run_callback(info, handler, ONVM_NO_CALLBACK);
}

int
onvm_nflib_return_pkt(struct onvm_nf_info* nf_info, struct rte_mbuf* pkt) {
        /* FIXME: should we get a batch of buffered packets and then enqueue? Can we keep stats? */
        if(unlikely(rte_ring_enqueue(nfs[nf_info->instance_id].tx_q, pkt) == -ENOBUFS)) {
                rte_pktmbuf_free(pkt);
                nfs[nf_info->instance_id].stats.tx_drop++;
                return -ENOBUFS;
        }
        else nfs[nf_info->instance_id].stats.tx_returned++;
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
onvm_nflib_handle_msg(struct onvm_nf_info *info, struct onvm_nf_msg *msg) {
        switch(msg->msg_type) {
        case MSG_STOP:
                RTE_LOG(INFO, APP, "Shutting down...\n");
                keep_running = 0;
                break;
        case MSG_SCALE:
                RTE_LOG(INFO, APP, "Received scale message...\n");
                onvm_nflib_scale(info);
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
                ret_act = (*handler)((struct rte_mbuf*)pkts[i], meta);
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

        onvm_pkt_enqueue_tx_thread(&tx_buf, nf->instance_id);
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
        onvm_nflib_handle_msg(nf->info, msg);
        rte_mempool_put(nf_msg_pool, (void*)msg);
}

int
onvm_nflib_scale(struct onvm_nf_info *info) {
        struct onvm_nf *nf;
        unsigned current;
        unsigned core;
        enum rte_lcore_state_t state;
        int ret;

        nf = &nfs[info->instance_id];

        if (nf->headroom == 0) {
                RTE_LOG(INFO, APP, "No cores available to scale\n");
                return -1;
        }

        current = rte_lcore_id();
        if (current != rte_get_master_lcore()) {
                RTE_LOG(INFO, APP, "Can only scale from the master lcore\n");
                return -1;
        }

        /* Find the next available lcore to use */
        RTE_LOG(INFO, APP, "Currently running on core %u\n", current);
        for (core = rte_get_next_lcore(current, 1, 0); core != RTE_MAX_LCORE; core = rte_get_next_lcore(core, 1, 0)) {
                state = rte_eal_get_lcore_state(core);
                if (state != RUNNING) {
                        RTE_LOG(INFO, APP, "Able to scale to core %u\n", core);
                        ret = rte_eal_remote_launch(&onvm_nflib_start_child, nf, core);
                        if (ret == -EBUSY) {
                                RTE_LOG(INFO, APP, "Core is %u busy, skipping...\n", core);
                                continue;
                        }
                        return 0;
                }
        }

        RTE_LOG(INFO, APP, "No cores available to scale\n");
        return -1;
}

static int
onvm_nflib_start_child(void *arg) {
        struct onvm_nf *parent;
        struct onvm_nf_info *child_info;
        int ret;

        parent = (struct onvm_nf *)arg;
        parent->headroom--;
        RTE_LOG(INFO, APP, "Starting another copy of service %u, new headroom: %u\n", parent->info->service_id, parent->headroom);
        ret = onvm_nflib_init(first_argc, first_argv, first_nf_tag, &child_info);
        if (ret < 0) {
                RTE_LOG(INFO, APP, "Unable to init new NF, exiting...\n");
                return -1;
        }
        
        nfs[child_info->instance_id].nf_setup_function = parent->nf_setup_function;

        if (parent->nf_mode == NF_MODE_SINGLE)
                onvm_nflib_run_callback(child_info, parent->nf_pkt_function, parent->nf_callback_function);
        else if (parent->nf_mode == NF_MODE_RING) {
                if (parent->nf_setup_function != NULL)
                        parent->nf_setup_function(child_info);
                onvm_nflib_nf_ready(child_info);
                parent->nf_advanced_rings_function(child_info);
        }
        return 0;
}

static struct onvm_nf_info *
onvm_nflib_info_init(const char *tag)
{
        void *mempool_data;
        struct onvm_nf_info *info;

        if (rte_mempool_get(nf_info_mp, &mempool_data) < 0) {
                rte_exit(EXIT_FAILURE, "Failed to get nf info memory");
        }

        if (mempool_data == NULL) {
                rte_exit(EXIT_FAILURE, "Client Info struct not allocated");
        }

        info = (struct onvm_nf_info*) mempool_data;
        info->instance_id = initial_instance_id;
        info->service_id = service_id;
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
        nf->nf_tx_mgr->id = initial_instance_id;
        nf->nf_tx_mgr->nf_rx_bufs = calloc(MAX_NFS, sizeof(struct packet_buf));
}


static void
onvm_nflib_usage(const char *progname) {
        printf("Usage: %s [EAL args] -- "
               "[-n <instance_id>]"
               "[-r <service_id>]\n\n", progname);
}


static int
onvm_nflib_parse_args(int argc, char *argv[]) {
        const char *progname = argv[0];
        int c;

        opterr = 0;
        while ((c = getopt (argc, argv, "n:r:")) != -1)
                switch (c) {
                case 'n':
                        initial_instance_id = (uint16_t) strtoul(optarg, NULL, 10);
                        break;
                case 'r':
                        service_id = (uint16_t) strtoul(optarg, NULL, 10);
                        // Service id 0 is reserved
                        if (service_id == 0) service_id = -1;
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

	struct onvm_nf_msg *shutdown_msg;
        nf_info->status = NF_STOPPED;

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
