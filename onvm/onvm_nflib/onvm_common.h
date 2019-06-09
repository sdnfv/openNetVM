/*********************************************************************
 *                     openNetVM
 *              https://sdnfv.github.io
 *
 *   BSD LICENSE
 *
 *   Copyright(c)
 *            2015-2019 George Washington University
 *            2015-2019 University of California Riverside
 *            2010-2019 Intel Corporation
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
 * onvm_common.h - shared data between host and NFs
 ********************************************************************/

#ifndef _ONVM_COMMON_H_
#define _ONVM_COMMON_H_

#include <stdint.h>

/* Std C library includes for shared core */
#include <sys/shm.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <semaphore.h>
#include <fcntl.h>
#include <signal.h>
#include <rte_ether.h>
#include <rte_mbuf.h>

#include "onvm_config_common.h"
#include "onvm_msg_common.h"

#define ONVM_MAX_CHAIN_LENGTH 4  // the maximum chain length
#define MAX_NFS 128              // total number of concurrent NFs allowed (-1 because ID 0 is reserved)
#define MAX_SERVICES 32          // total number of unique services allowed
#define MAX_NFS_PER_SERVICE 32   // max number of NFs per service.

#define NUM_MBUFS 32767          // total number of mbufs (2^15 - 1)
#define NF_QUEUE_RINGSIZE 16384  // size of queue for NFs

#define PACKET_READ_SIZE ((uint16_t)32)

#define ONVM_NF_SHARE_CORES_DEFAULT 0  // default value for shared core logic, if true NFs sleep while waiting for packets

#define ONVM_NF_ACTION_DROP 0  // drop packet
#define ONVM_NF_ACTION_NEXT 1  // to whatever the next action is configured by the SDN controller in the flow table
#define ONVM_NF_ACTION_TONF 2  // send to the NF specified in the argument field (assume it is on the same host)
#define ONVM_NF_ACTION_OUT  3  // send the packet out the NIC port set in the argument field

#define PKT_WAKEUP_THRESHOLD 1 // for shared core mode, how many packets are required to wake up the NF

/* Used in setting bit flags for core options */
#define MANUAL_CORE_ASSIGNMENT_BIT 0
#define SHARE_CORE_BIT 1

#define ONVM_SIGNAL_TERMINATION -999

/* Maximum length of NF_TAG including the \0 */
#define TAG_SIZE 15

// flag operations that should be used on onvm_pkt_meta
#define ONVM_CHECK_BIT(flags, n) !!((flags) & (1 << (n)))
#define ONVM_SET_BIT(flags, n) ((flags) | (1 << (n)))
#define ONVM_CLEAR_BIT(flags, n) ((flags) & (0 << (n)))

/* Measured in millions of packets */
#define PKT_TTL_MULTIPLIER 1000000
/* Measured in seconds */
#define TIME_TTL_MULTIPLIER 1

/* For NF termination handling */
#define NF_TERM_WAIT_TIME 1
#define NF_TERM_INIT_ITER_TIMES 3
/* If a lot of children spawned this might need to be increased */
#define NF_TERM_STOP_ITER_TIMES 10

struct onvm_pkt_meta {
        uint8_t action;       /* Action to be performed */
        uint16_t destination; /* where to go next */
        uint16_t src;         /* who processed the packet last */
        uint8_t chain_index;  /*index of the current step in the service chain*/
        uint8_t flags;        /* bits for custom NF data. Use with caution to prevent collisions from different NFs. */
};

static inline struct onvm_pkt_meta *
onvm_get_pkt_meta(struct rte_mbuf *pkt) {
        return (struct onvm_pkt_meta *)&pkt->udata64;
}

static inline uint8_t
onvm_get_pkt_chain_index(struct rte_mbuf *pkt) {
        struct onvm_pkt_meta* pkt_meta = (struct onvm_pkt_meta*) &pkt->udata64;
        return pkt_meta->chain_index;
}

/*
 * Shared port info, including statistics information for display by server.
 * Structure will be put in a memzone.
 * - All port id values share one cache line as this data will be read-only
 * during operation.
 * - All rx statistic values share cache lines, as this data is written only
 * by the server process. (rare reads by stats display)
 * - The tx statistics have values for all ports per cache line, but the stats
 * themselves are written by the NFs, so we have a distinct set, on different
 * cache lines for each NF to use.
 */
/*******************************Data Structures*******************************/

/*
 * Packets may be transported by a tx thread or by an NF.
 * This data structure encapsulates data specific to
 * tx threads.
 */
struct tx_thread_info {
        unsigned first_nf;
        unsigned last_nf;
        struct packet_buf *port_tx_bufs;
};

/*
 * Local buffers to put packets in, used to send packets in bursts to the
 * NFs or to the NIC
 */
struct packet_buf {
        struct rte_mbuf *buffer[PACKET_READ_SIZE];
        uint16_t count;
};

/*
 * Generic data struct that tx threads and nfs both use.
 * Allows pkt functions to be shared
 * */
struct queue_mgr {
        unsigned id;
        enum { NF, MGR } mgr_type_t;
        union {
                struct tx_thread_info *tx_thread_info;
                struct packet_buf *to_tx_buf;
        };
        struct packet_buf *nf_rx_bufs;
};

/* NFs wakeup Info: used by manager to update NFs pool and wakeup stats */
struct wakeup_thread_context {
        unsigned first_nf;
        unsigned last_nf;
};

struct nf_wakeup_info {
        const char *sem_name;
        sem_t *mutex;
        key_t shm_key;
        rte_atomic16_t *shm_server;
        uint64_t num_wakeups;
        uint64_t prev_num_wakeups;
};

struct rx_stats {
        uint64_t rx[RTE_MAX_ETHPORTS];
};

struct tx_stats {
        uint64_t tx[RTE_MAX_ETHPORTS];
        uint64_t tx_drop[RTE_MAX_ETHPORTS];
};

struct port_info {
        uint8_t num_ports;
        uint8_t id[RTE_MAX_ETHPORTS];
        struct ether_addr mac[RTE_MAX_ETHPORTS];
        volatile struct rx_stats rx_stats;
        volatile struct tx_stats tx_stats;
};

struct onvm_configuration {
        struct {
                uint8_t ONVM_NF_SHARE_CORES;
        } flags;
};

struct core_status {
        uint8_t enabled;
        uint8_t is_dedicated_core;
        uint16_t nf_count;
};

struct onvm_nf_local_ctx;
struct onvm_nf;
/* Function prototype for NF packet handlers */
typedef int (*nf_pkt_handler_fn)(struct rte_mbuf *pkt, struct onvm_pkt_meta *meta,
                                 __attribute__((unused)) struct onvm_nf_local_ctx *nf_local_ctx);
/* Function prototype for NF the callback */
typedef int (*nf_user_actions_fn)(__attribute__((unused)) struct onvm_nf_local_ctx *nf_local_ctx);
/* Function prototype for NFs that want extra initalization/setup before running */
typedef void (*nf_setup_fn)(struct onvm_nf_local_ctx *nf_local_ctx);
/* Function prototype for NFs to handle custom messages */
typedef void (*nf_msg_handler_fn)(void *msg_data, struct onvm_nf_local_ctx *nf_local_ctx);
/* Function prototype for NFs to signal handling */
typedef void (*handle_signal_func)(int);

/* Contains all functions the NF might use */
struct onvm_nf_function_table {
        nf_setup_fn  setup;
        nf_msg_handler_fn  msg_handler;
        nf_user_actions_fn user_actions;
        nf_pkt_handler_fn  pkt_handler;
};

/* Information needed to initialize a new NF child thread */
struct onvm_nf_scale_info {
        struct onvm_nf_init_cfg *nf_init_cfg;
        struct onvm_nf *parent;
        void * data;
        struct onvm_nf_function_table *function_table;
};

struct onvm_nf_local_ctx {
        struct onvm_nf *nf;
        rte_atomic16_t nf_init_finished;
        rte_atomic16_t keep_running;
        rte_atomic16_t nf_stopped;
};

/*
 * Define a NF structure with all needed info, including:
 *      thread information, function callbacks, flags, stats and shared core info.
 *
 * This structure is available in the NF when processing packets or executing the callback.
 */
struct onvm_nf {
        struct rte_ring *rx_q;
        struct rte_ring *tx_q;
        struct rte_ring *msg_q;
        /* Struct for NF to NF communication (NF tx) */
        struct queue_mgr *nf_tx_mgr;
        uint16_t instance_id;
        uint16_t service_id;
        uint8_t status;
        char *tag;
        /* Pointer to NF defined state data */
        void *data;

        struct {
                uint16_t core;
                /* Instance ID of parent NF or 0 */
                uint16_t parent;
                rte_atomic16_t children_cnt;
        } thread_info;

        struct {
                uint16_t init_options;
                /* If set NF will stop after time reaches time_to_live */
                uint16_t time_to_live;
                /* If set NF will stop after pkts TX reach pkt_limit */
                uint16_t pkt_limit;
        } flags;

        /* NF specific functions */
        struct onvm_nf_function_table *function_table;

        /*
         * Define a structure with stats from the NFs.
         *
         * These stats hold how many packets the NF will actually receive, send,
         * and how many packets were dropped because the NF's queue was full.
         * The port-info stats, in contrast, record how many packets were received
         * or transmitted on an actual NIC port.
         */
        struct {
                volatile uint64_t rx;
                volatile uint64_t rx_drop;
                volatile uint64_t tx;
                volatile uint64_t tx_drop;
                volatile uint64_t tx_buffer;
                volatile uint64_t tx_returned;
                volatile uint64_t act_out;
                volatile uint64_t act_tonf;
                volatile uint64_t act_drop;
                volatile uint64_t act_next;
                volatile uint64_t act_buffer;
        } stats;

        struct {
                 /* 
                  * Sleep state (shared mem variable) to track state of NF and trigger wakeups 
                  *     sleep_state = 1 => NF sleeping (waiting on semaphore)
                  *     sleep_state = 0 => NF running (not waiting on semaphore)
                  */
                rte_atomic16_t *sleep_state;
                /* Mutex for NF sem_wait */
                sem_t *nf_mutex;
        } shared_core;
};

/*
 * The config structure to inialize the NF with onvm_mgr
 */
struct onvm_nf_init_cfg {
        uint16_t instance_id;
        uint16_t service_id;
        uint16_t core;
        uint16_t init_options;
        uint8_t status;
        char *tag;
        /* If set NF will stop after time reaches time_to_live */
        uint16_t time_to_live;
        /* If set NF will stop after pkts TX reach pkt_limit */
        uint16_t pkt_limit;
};

/*
 * Define a structure to describe a service chain entry
 */
struct onvm_service_chain_entry {
        uint16_t destination;
        uint8_t action;
};

struct onvm_service_chain {
        struct onvm_service_chain_entry sc[ONVM_MAX_CHAIN_LENGTH];
        uint8_t chain_length;
        int ref_cnt;
};

struct lpm_request {
        char name[64];
        uint32_t max_num_rules;
        uint32_t num_tbl8s;
        int socket_id;
        int status;
};

/* define common names for structures shared between server and NF */
#define MP_NF_RXQ_NAME "MProc_Client_%u_RX"
#define MP_NF_TXQ_NAME "MProc_Client_%u_TX"
#define MP_CLIENT_SEM_NAME "MProc_Client_%u_SEM"
#define PKTMBUF_POOL_NAME "MProc_pktmbuf_pool"
#define MZ_PORT_INFO "MProc_port_info"
#define MZ_CORES_STATUS "MProc_cores_info"
#define MZ_NF_INFO "MProc_nf_init_cfg"
#define MZ_SERVICES_INFO "MProc_services_info"
#define MZ_NF_PER_SERVICE_INFO "MProc_nf_per_service_info"
#define MZ_ONVM_CONFIG "MProc_onvm_config"
#define MZ_SCP_INFO "MProc_scp_info"
#define MZ_FTP_INFO "MProc_ftp_info"

#define _MGR_MSG_QUEUE_NAME "MSG_MSG_QUEUE"
#define _NF_MSG_QUEUE_NAME "NF_%u_MSG_QUEUE"
#define _NF_MEMPOOL_NAME "NF_INFO_MEMPOOL"
#define _NF_MSG_POOL_NAME "NF_MSG_MEMPOOL"

/* interrupt semaphore specific updates */
#define SHMSZ 4                         // size of shared memory segement (page_size)
#define KEY_PREFIX 123                  // prefix len for key

/* common names for NF states */
#define NF_WAITING_FOR_ID 0       // First step in startup process, doesn't have ID confirmed by manager yet
#define NF_STARTING 1             // When a NF is in the startup process and already has an id
#define NF_RUNNING 2              // Running normally
#define NF_PAUSED 3               // NF is not receiving packets, but may in the future
#define NF_STOPPED 4              // NF has stopped and in the shutdown process
#define NF_ID_CONFLICT 5          // NF is trying to declare an ID already in use
#define NF_NO_IDS 6               // There are no available IDs for this NF
#define NF_SERVICE_MAX 7          // Service ID has exceeded the maximum amount
#define NF_SERVICE_COUNT_MAX 8    // Maximum amount of NF's per service spawned
#define NF_NO_CORES 9             // There are no cores available or specified core can't be used
#define NF_NO_DEDICATED_CORES 10  // There is no space for a dedicated core
#define NF_CORE_OUT_OF_RANGE 11   // The manually selected core is out of range
#define NF_CORE_BUSY 12           // The manually selected core is busy
#define NF_WAITING_FOR_LPM 13     // NF is waiting for a LPM request to be fulfilled

#define NF_NO_ID -1
#define ONVM_NF_HANDLE_TX 1  // should be true if NFs primarily pass packets to each other

/*
 * Given the rx queue name template above, get the queue name
 */
static inline const char *
get_rx_queue_name(unsigned id) {
        /* buffer for return value. Size calculated by %u being replaced
         * by maximum 3 digits (plus an extra byte for safety) */
        static char buffer[sizeof(MP_NF_RXQ_NAME) + 2];

        snprintf(buffer, sizeof(buffer) - 1, MP_NF_RXQ_NAME, id);
        return buffer;
}

/*
 * Given the tx queue name template above, get the queue name
 */
static inline const char *
get_tx_queue_name(unsigned id) {
        /* buffer for return value. Size calculated by %u being replaced
         * by maximum 3 digits (plus an extra byte for safety) */
        static char buffer[sizeof(MP_NF_TXQ_NAME) + 2];

        snprintf(buffer, sizeof(buffer) - 1, MP_NF_TXQ_NAME, id);
        return buffer;
}

/*
 * Given the name template above, get the mgr -> NF msg queue name
 */
static inline const char *
get_msg_queue_name(unsigned id) {
        /* buffer for return value. Size calculated by %u being replaced
         * by maximum 3 digits (plus an extra byte for safety) */
        static char buffer[sizeof(_NF_MSG_QUEUE_NAME) + 2];

        snprintf(buffer, sizeof(buffer) - 1, _NF_MSG_QUEUE_NAME, id);
        return buffer;
}

/*
 * Interface checking if a given NF is "valid", meaning if it's running.
 */
static inline int
onvm_nf_is_valid(struct onvm_nf *nf) {
        return nf && nf->status == NF_RUNNING;
}

/*
 * Given the rx queue name template above, get the key of the shared memory
 */
static inline key_t
get_rx_shmkey(unsigned id) {
        return KEY_PREFIX * 10 + id;
}

/*
 * Given the sem name template above, get the sem name
 */
static inline const char *
get_sem_name(unsigned id) {
        /* buffer for return value. Size calculated by %u being replaced
         * by maximum 3 digits (plus an extra byte for safety) */
        static char buffer[sizeof(MP_CLIENT_SEM_NAME) + 2];

        snprintf(buffer, sizeof(buffer) - 1, MP_CLIENT_SEM_NAME, id);
        return buffer;
}

static inline int
whether_wakeup_client(struct onvm_nf *nf, struct nf_wakeup_info *nf_wakeup_info) {
        if (rte_ring_count(nf->rx_q) < PKT_WAKEUP_THRESHOLD)
                return 0;

        /* Check if its already woken up */
        if (rte_atomic16_read(nf_wakeup_info->shm_server) == 0)
                return 0;

        return 1;
}

#define RTE_LOGTYPE_APP RTE_LOGTYPE_USER1

#endif  // _ONVM_COMMON_H_
