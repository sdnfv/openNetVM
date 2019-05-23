/*********************************************************************
 *                     openNetVM
 *              https://sdnfv.github.io
 *
 *   BSD LICENSE
 *
 *   Copyright(c)
 *            2015-2017 George Washington University
 *            2015-2017 University of California Riverside
 *            2010-2014 Intel Corporation. All rights reserved.
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

                                  onvm_init.c

                  File containing initialization functions.


******************************************************************************/


#include "onvm_mgr/onvm_init.h"


/********************************Global variables*****************************/


struct onvm_nf *nfs = NULL;
struct port_info *ports = NULL;

struct rte_mempool *pktmbuf_pool;
struct rte_mempool *nf_info_pool;
struct rte_mempool *nf_msg_pool;
struct rte_ring *mgr_msg_queue;

#ifdef ENABLE_SYNC_MGR_TO_NF_MSG
struct rte_ring *mgr_rsp_queue;
#endif

#ifdef ENABLE_NFV_RESL
struct rte_mempool *nf_state_pool;
#endif //#ifdef ENABLE_NFV_RESL

#ifdef ENABLE_PER_SERVICE_MEMPOOL
struct rte_mempool *service_state_pool;
void **services_state_pool;
#endif

#ifdef ENABLE_PER_FLOW_TS_STORE
//Allocated Mempool to store the NFs processed and Tx Sent Packets TS info
struct rte_mempool *per_flow_ts_pool;
//Allocated Mempool that can be used to store the Tx Packets TS info
void *onvm_mgr_tx_per_flow_ts_info;
#ifdef ENABLE_RSYNC_WITH_DOUBLE_BUFFERING_MODE
#ifdef ENABLE_RSYNC_MULTI_BUFFERING
void *onvm_mgr_tx_per_flow_ts_info_db[ENABLE_RSYNC_MULTI_BUFFERING];
#else
void *onvm_mgr_tx_per_flow_ts_info_db;
#endif
#endif
#endif

#ifdef ENABLE_REMOTE_SYNC_WITH_TX_LATCH
struct rte_ring *tx_port_ring[RTE_MAX_ETHPORTS];           //ring used by NFs and Other Tx threads to transmit out port packets
struct rte_ring *tx_tx_state_latch_ring[RTE_MAX_ETHPORTS]; //ring used by TX_RSYNC to store packets till 2 Phase commit of TS STAT Update
struct rte_ring *tx_nf_state_latch_ring[RTE_MAX_ETHPORTS]; //ring used by TX_RSYNC to store packets till 2 phase commit of NFs in the chain resulting in non-determinism.
#ifdef ENABLE_RSYNC_WITH_DOUBLE_BUFFERING_MODE
#ifdef ENABLE_RSYNC_MULTI_BUFFERING
struct rte_ring *tx_tx_state_latch_db_ring[ENABLE_RSYNC_MULTI_BUFFERING][RTE_MAX_ETHPORTS]; //additional Double buffer ring used by TX_RSYNC to store packets till 2 Phase commit of TS STAT Update
struct rte_ring *tx_nf_state_latch_db_ring[ENABLE_RSYNC_MULTI_BUFFERING][RTE_MAX_ETHPORTS]; //additional Double buffer ring used by TX_RSYNC to store packets till 2 phase commit of NFs in the chain resulting in non-determinism.
#else
struct rte_ring *tx_tx_state_latch_db_ring[RTE_MAX_ETHPORTS]; //ring used by TX_RSYNC to store packets till 2 Phase commit of TS STAT Update
struct rte_ring *tx_nf_state_latch_db_ring[RTE_MAX_ETHPORTS]; //ring used by TX_RSYNC to store packets till 2 phase commit of NFs in the chain resulting in non-determinism.
#endif
#endif
#endif

uint16_t **services;
uint16_t *nf_per_service_count;

struct onvm_service_chain *default_chain;
struct onvm_service_chain **default_sc_p;

#if defined (INTERRUPT_SEM) && defined (USE_SOCKET)
int onvm_socket_id;
#endif

#if defined (INTERRUPT_SEM) && defined (USE_ZMQ)
void *zmq_ctx;
void *onvm_socket_id;
void *onvm_socket_ctx;
#endif

#ifdef ENABLE_VXLAN
uint8_t remote_eth_addr[6] = {0x68,0x05,0xCA,0x30,0x5A,0x51};
struct ether_addr remote_eth_addr_struct;
#endif
/*********************************Prototypes**********************************/


static int init_mbuf_pools(void);
static int init_client_info_pool(void);
static int init_nf_msg_pool(void);
static int init_port(uint8_t port_num);
static int init_shm_rings(void);
static int init_mgr_queues(void);
static void check_all_ports_link_status(uint8_t port_num, uint32_t port_mask);


#ifdef ENABLE_NFV_RESL
static int init_nf_state_pool(void);
#endif // ENABLE_NFV_RESL

#ifdef ENABLE_NF_MGR_IDENTIFIER
#include <unistd.h>
uint32_t nf_mgr_id;
static uint32_t read_onvm_mgr_id_from_system(void);
#endif // ENABLE_NF_MGR_IDENTIFIER

#ifdef ENABLE_PER_SERVICE_MEMPOOL
static int init_service_state_pool(void);
#endif

#ifdef ENABLE_PER_FLOW_TS_STORE
static int init_per_flow_ts_pool(void);
#endif

#ifdef ENABLE_REMOTE_SYNC_WITH_TX_LATCH
static int init_rsync_tx_rings(void);
#endif
/*****************Internal Configuration Structs and Constants*****************/

/*
 * RX and TX Prefetch, Host, and Write-back threshold values should be
 * carefully set for optimal performance. Consult the network
 * controller's datasheet and supporting DPDK documentation for guidance
 * on how these parameters should be set.
 */
#define RX_PTHRESH 8 /* Default values of RX prefetch threshold reg. */
#define RX_HTHRESH 8 /* Default values of RX host threshold reg. */
#define RX_WTHRESH 4 /* Default values of RX write-back threshold reg. */

/*
 * These default values are optimized for use with the Intel(R) 82599 10 GbE
 * Controller and the DPDK ixgbe PMD. Consider using other values for other
 * network controllers and/or network drivers.
 */
#define TX_PTHRESH 36 /* Default values of TX prefetch threshold reg. */
#define TX_HTHRESH 0  /* Default values of TX host threshold reg. */
#define TX_WTHRESH 0  /* Default values of TX write-back threshold reg. */

static const struct rte_eth_conf port_conf = {
        .rxmode = {
                .mq_mode        = ETH_MQ_RX_RSS,
                .max_rx_pkt_len = ETHER_MAX_LEN,
                .split_hdr_size = 0,
                .offloads       = DEV_RX_OFFLOAD_CHECKSUM,
        },
        .rx_adv_conf = {
                .rss_conf = {
                        .rss_key = rss_symmetric_key,
                        .rss_hf  = ETH_RSS_IP | ETH_RSS_UDP | ETH_RSS_TCP | ETH_RSS_L2_PAYLOAD,
                },
        },
        .txmode = {
                .mq_mode = ETH_MQ_TX_NONE,
                .offloads = (DEV_TX_OFFLOAD_IPV4_CKSUM |
		             DEV_TX_OFFLOAD_UDP_CKSUM  |
                             DEV_TX_OFFLOAD_TCP_CKSUM)
        },
};

static const struct rte_eth_rxconf rx_conf = {
        .rx_thresh = {
                .pthresh = RX_PTHRESH,
                .hthresh = RX_HTHRESH,
                .wthresh = RX_WTHRESH,
        },
        .rx_free_thresh = 32,
};

static const struct rte_eth_txconf tx_conf = {
        .tx_thresh = {
                .pthresh = TX_PTHRESH,
                .hthresh = TX_HTHRESH,
                .wthresh = TX_WTHRESH,
        },
        .tx_free_thresh = 0,
        .tx_rs_thresh   = 0,
        //.txq_flags      = 0,
};


/*********************************Interfaces**********************************/


int
init(int argc, char *argv[]) {
        int retval;
        const struct rte_memzone *mz_port;
        const struct rte_memzone *mz_scp;
        uint8_t i, total_ports, port_id;

        /* init EAL, parsing EAL args */
        retval = rte_eal_init(argc, argv);
        if (retval < 0)
                return -1;
        argc -= retval;
        argv += retval;

#ifdef RTE_LIBRTE_PDUMP
        rte_pdump_init(NULL);
#endif
        /* get total number of ports */
        total_ports = rte_eth_dev_count_avail();

        /* set up ports info */
        mz_port = rte_memzone_reserve(MZ_PORT_INFO, sizeof(*ports),
                                   rte_socket_id(), NO_FLAGS);
        if (mz_port == NULL)
               rte_exit(EXIT_FAILURE, "Cannot reserve memory zone for port information\n");
        ports = mz_port->addr;

        /* parse additional, application arguments */
        retval = parse_app_args(total_ports, argc, argv);
        if (retval != 0)
                return -1;

        /* initialise mbuf pools */
        retval = init_mbuf_pools();
        if (retval != 0)
                rte_exit(EXIT_FAILURE, "Cannot create needed mbuf pools\n");

        /* initialise client info pool */
        retval = init_client_info_pool();
        if (retval != 0) {
                rte_exit(EXIT_FAILURE, "Cannot create client info mbuf pool: %s\n", rte_strerror(rte_errno));
        }

        /* initialise pool for NF messages */
        retval = init_nf_msg_pool();
        if (retval != 0) {
                rte_exit(EXIT_FAILURE, "Cannot create nf message pool: %s\n", rte_strerror(rte_errno));
        }
        /* now initialise the ports we will use */
        for (i = 0; i < ports->num_ports; i++) {
                port_id = ports->id[i];
                rte_eth_macaddr_get(port_id, &ports->mac[port_id]);
                retval = init_port(port_id);
                if (retval != 0)
                        rte_exit(EXIT_FAILURE, "Cannot initialise port %u:: %s\n",
                                        (unsigned)i, rte_strerror(rte_errno));
        }

        check_all_ports_link_status(ports->num_ports, (~0x0));

        /* initialise the client queues/rings for inter-eu comms */
        init_shm_rings();

        /* initialise a queue for newly created NFs */
        init_mgr_queues();

        /*initialize a default service chain*/
        default_chain = onvm_sc_create();
#ifdef ONVM_ENABLE_SPEACILA_NF
        retval = onvm_sc_append_entry(default_chain, ONVM_NF_ACTION_TO_NF_INSTANCE, 0); //0= INSTANCE ID of SPECIAL_NF
        //retval = onvm_sc_append_entry(default_chain, ONVM_NF_ACTION_TONF, 0);   //0 = SERVICE ID of SPECIAL NF
        //retval = onvm_sc_append_entry(default_chain, ONVM_NF_ACTION_TONF, 1); //default: send to any NF with service ID=1
#else
        retval = onvm_sc_append_entry(default_chain, ONVM_NF_ACTION_TONF, 1);
#endif
        if (retval == ENOSPC) {
                printf("chain length can not be larger than the maximum chain length\n");
                exit(5);
        }
        printf("Default service chain: send to sdn NF\n");        
        
        /* set up service chain pointer shared to NFs*/
        mz_scp = rte_memzone_reserve(MZ_SCP_INFO, sizeof(struct onvm_service_chain *),
                                   rte_socket_id(), NO_FLAGS);
        if (mz_scp == NULL)
                rte_exit(EXIT_FAILURE, "Cannot reserve memory zone for service chain pointer\n");
        memset(mz_scp->addr, 0, sizeof(struct onvm_service_chain *));
        default_sc_p = mz_scp->addr;
        *default_sc_p = default_chain;
        onvm_sc_print(default_chain);

        onvm_flow_dir_init();

#if defined(ENABLE_USE_RTE_TIMER_MODE_FOR_MAIN_THREAD) || defined(ENABLE_USE_RTE_TIMER_MODE_FOR_WAKE_THREAD)
        rte_timer_subsystem_init();
#endif //ENABLE_USE_RTE_TIMER_MODE_FOR_MAIN_THREAD

#ifdef ENABLE_NF_MGR_IDENTIFIER
        uint32_t my_id = read_onvm_mgr_id_from_system();
        printf("Read the ONVM_MGR Identifier as: [%d (0x%x)] \n", my_id,my_id);
#endif // ENABLE_NF_MGR_IDENTIFIER

        return 0;
}

/*****************************Internal functions******************************/


/**
 * Initialise the mbuf pool for packet reception for the NIC, and any other
 * buffer pools needed by the app - currently none.
 */
static int
init_mbuf_pools(void) {
        const unsigned num_mbufs = (MAX_NFS * MBUFS_PER_NF) \
                        + (ports->num_ports * MBUFS_PER_PORT);

        /* don't pass single-producer/single-consumer flags to mbuf create as it
         * seems faster to use a cache instead */
        printf("Creating mbuf pool '%s' [%u mbufs] ...\n",
                        PKTMBUF_POOL_NAME, num_mbufs);
        pktmbuf_pool = rte_mempool_create(PKTMBUF_POOL_NAME, num_mbufs,
                        MBUF_SIZE, MBUF_CACHE_SIZE,
                        sizeof(struct rte_pktmbuf_pool_private), rte_pktmbuf_pool_init,
                        NULL, rte_pktmbuf_init, NULL, rte_socket_id(), NO_FLAGS);

        return (pktmbuf_pool == NULL); /* 0  on success */
}

#ifdef ENABLE_NFV_RESL
static int init_nf_state_pool(void) {
        //printf("Cache size:[%d,max:%d], Creating mbuf pool '%s' ...\n", _NF_STATE_CACHE, RTE_MEMPOOL_CACHE_MAX_SIZE, _NF_MEMPOOL_NAME);
        //setting Cache size parameter seems to have inconsistent behavior; it is better to allocate first with cached; and on failure change to 0 cache size;
        nf_state_pool = rte_mempool_create(_NF_STATE_MEMPOOL_NAME, MAX_NFS,
                        _NF_STATE_SIZE, _NF_STATE_CACHE,
                        0, NULL, NULL, NULL, NULL, rte_socket_id(), NO_FLAGS);

        if(NULL == nf_state_pool) {
                printf("Failed to Create mbuf NF state pool '%s' with cache size...\n", _NF_STATE_MEMPOOL_NAME);
                nf_state_pool = rte_mempool_create(_NF_STATE_MEMPOOL_NAME, MAX_NFS,
                        _NF_STATE_SIZE, 0,
                        0, NULL, NULL, NULL, NULL, rte_socket_id(), NO_FLAGS);
        }

        if(nf_state_pool == NULL) { printf("Failed to Create mbuf state pool '%s' without cache size!...\n", _NF_STATE_MEMPOOL_NAME);}
        return (nf_state_pool == NULL); /* 0 on success */
}
#endif

#ifdef ENABLE_PER_SERVICE_MEMPOOL
static int init_service_state_pool(void) {
        service_state_pool = rte_mempool_create(_SERVICE_STATE_MEMPOOL_NAME, MAX_SERVICES,
                        _SERVICE_STATE_SIZE, _SERVICE_STATE_CACHE,
                        0, NULL, NULL, NULL, NULL, rte_socket_id(), NO_FLAGS);

        if(NULL == service_state_pool) {
                printf("Failed to Create mbuf service state pool '%s' with cache size...\n", _SERVICE_STATE_MEMPOOL_NAME);
                service_state_pool = rte_mempool_create(_SERVICE_STATE_MEMPOOL_NAME, MAX_SERVICES,
                        _SERVICE_STATE_SIZE, 0,
                        0, NULL, NULL, NULL, NULL, rte_socket_id(), NO_FLAGS);
        }

        if(service_state_pool == NULL) { printf("Failed to Create mbuf service state pool '%s' without cache size!...\n", _SERVICE_STATE_MEMPOOL_NAME);}
        return (service_state_pool == NULL); /* 0 on success */
}
#endif

#ifdef ENABLE_PER_FLOW_TS_STORE
static int init_per_flow_ts_pool(void) {
#ifdef ENABLE_RSYNC_WITH_DOUBLE_BUFFERING_MODE
        #ifdef ENABLE_RSYNC_MULTI_BUFFERING
                #ifdef ENABLE_NFLIB_PER_FLOW_TS_STORE
                        #define PER_FLOW_TS_POOL_COUNT  (MAX_NFS + ENABLE_RSYNC_MULTI_BUFFERING+1)
                #else
                        #define PER_FLOW_TS_POOL_COUNT  (ENABLE_RSYNC_MULTI_BUFFERING+1) //(MAX_NFS + ENABLE_RSYNC_MULTI_BUFFERING+1) //(MAX_NFS +2) //currently NFs do not use it
                #endif
        #else
                #ifdef ENABLE_NFLIB_PER_FLOW_TS_STORE
                        #define PER_FLOW_TS_POOL_COUNT  (MAX_NFS +2)
                #else
                        #define PER_FLOW_TS_POOL_COUNT  (2) //(MAX_NFS +2)
                #endif
        #endif
#else
        #ifdef ENABLE_NFLIB_PER_FLOW_TS_STORE
                #define PER_FLOW_TS_POOL_COUNT  (MAX_NFS +1)
        #else
                #define PER_FLOW_TS_POOL_COUNT  (1) //(MAX_NFS +1)
#endif
#endif
        per_flow_ts_pool = rte_mempool_create(_PER_FLOW_TS_MEMPOOL_NAME, PER_FLOW_TS_POOL_COUNT,
                        _PER_FLOW_TS_SIZE, _PER_FLOW_TS_CACHE,
                                0, NULL, NULL, NULL, NULL, rte_socket_id(), NO_FLAGS);

                if(NULL == per_flow_ts_pool) {
                        printf("Failed to Create mbuf service state pool '%s' with cache size...\n", _PER_FLOW_TS_MEMPOOL_NAME);
                        per_flow_ts_pool = rte_mempool_create(_PER_FLOW_TS_MEMPOOL_NAME, PER_FLOW_TS_POOL_COUNT,
                                        _PER_FLOW_TS_SIZE, 0,
                                0, NULL, NULL, NULL, NULL, rte_socket_id(), NO_FLAGS);
                }

                if(per_flow_ts_pool == NULL) { printf("Failed to Create mbuf pkt ts pool '%s' without cache size!...\n", _PER_FLOW_TS_MEMPOOL_NAME);}
                return (per_flow_ts_pool == NULL); /* 0 on success */
}
#endif


#ifdef ENABLE_REMOTE_SYNC_WITH_TX_LATCH
static int init_rsync_tx_rings(void) {
        uint8_t i = 0;
        for(; i< ports->num_ports; i++) {
                const char * tx_p_name = get_rsync_tx_port_ring_name(i);
                const char * tx_s_name = get_rsync_tx_tx_state_latch_ring_name(i);
                const char * tx_n_name = get_rsync_tx_nf_state_latch_ring_name(i);
#ifdef ENABLE_RSYNC_WITH_DOUBLE_BUFFERING_MODE
#ifdef ENABLE_RSYNC_MULTI_BUFFERING
                uint8_t j = 0;
                for(; j < ENABLE_RSYNC_MULTI_BUFFERING; j++) {
                        const char * tx_s_name_db = get_rsync_tx_tx_state_latch_db_ring_name((i*ports->num_ports+j));
                        const char * tx_n_name_db = get_rsync_tx_nf_state_latch_db_ring_name((i*ports->num_ports+j));
                        //Double buffering: additional ring used by TX_RSYNC to store packets till 2 Phase commit of TS STAT Update
                        tx_tx_state_latch_db_ring[j][i] = rte_ring_create(tx_s_name_db, TX_RSYNC_TX_LATCH_DB_RING_SIZE,rte_socket_id(),RING_F_SP_ENQ|RING_F_SC_DEQ);
                        if (NULL == tx_tx_state_latch_db_ring[j][i])
                                rte_exit(EXIT_FAILURE, "Cannot create Tx_tx_state_latch_db_ring Ring\n");
                        //Double buffering: additional ring used by TX_RSYNC to store packets till 2 phase commit of NFs in the chain resulting in non-determinism.
                        tx_nf_state_latch_db_ring[j][i] = rte_ring_create(tx_n_name_db, TX_RSYNC_NF_LATCH_DB_RING_SIZE,rte_socket_id(),RING_F_SP_ENQ|RING_F_SC_DEQ);
                        if (NULL == tx_nf_state_latch_db_ring[j][i])
                                rte_exit(EXIT_FAILURE, "Cannot create tx_nf_state_latch_db_ring Ring\n");
                }
#else

                const char * tx_s_name_db = get_rsync_tx_tx_state_latch_db_ring_name(i);
                const char * tx_n_name_db = get_rsync_tx_nf_state_latch_db_ring_name(i);
                //Double buffering: additional ring used by TX_RSYNC to store packets till 2 Phase commit of TS STAT Update
                tx_tx_state_latch_db_ring[i] = rte_ring_create(tx_s_name_db, TX_RSYNC_TX_LATCH_DB_RING_SIZE,rte_socket_id(),RING_F_SP_ENQ|RING_F_SC_DEQ);
                if (NULL == tx_tx_state_latch_db_ring[i])
                        rte_exit(EXIT_FAILURE, "Cannot create Tx_tx_state_latch_db_ring Ring\n");
                //Double buffering: additional ring used by TX_RSYNC to store packets till 2 phase commit of NFs in the chain resulting in non-determinism.
                tx_nf_state_latch_db_ring[i] = rte_ring_create(tx_n_name_db, TX_RSYNC_NF_LATCH_DB_RING_SIZE,rte_socket_id(),RING_F_SP_ENQ|RING_F_SC_DEQ);
                if (NULL == tx_nf_state_latch_db_ring[i])
                        rte_exit(EXIT_FAILURE, "Cannot create tx_nf_state_latch_db_ring Ring\n");
#endif
#endif
                //ring used by NFs and Other Tx threads to transmit out port packets
                tx_port_ring[i] = rte_ring_create(tx_p_name, TX_RSYNC_TX_PORT_RING_SIZE,rte_socket_id(),RING_F_SC_DEQ);
                if (NULL == tx_port_ring[i])
                        rte_exit(EXIT_FAILURE, "Cannot create Tx Port Ring\n");
                //ring used by TX_RSYNC to store packets till 2 Phase commit of TS STAT Update
                tx_tx_state_latch_ring[i] = rte_ring_create(tx_s_name, TX_RSYNC_TX_LATCH_RING_SIZE,rte_socket_id(),RING_F_SP_ENQ|RING_F_SC_DEQ);
                if (NULL == tx_tx_state_latch_ring[i])
                        rte_exit(EXIT_FAILURE, "Cannot create Tx_tx_state_latch_ring Ring\n");
                //ring used by TX_RSYNC to store packets till 2 phase commit of NFs in the chain resulting in non-determinism.
                tx_nf_state_latch_ring[i] = rte_ring_create(tx_n_name, TX_RSYNC_NF_LATCH_RING_SIZE,rte_socket_id(),RING_F_SP_ENQ|RING_F_SC_DEQ);
                if (NULL == tx_nf_state_latch_ring[i])
                        rte_exit(EXIT_FAILURE, "Cannot create tx_nf_state_latch_ring Ring\n");
        }
#if 0
        //ring used by NFs and Other Tx threads to transmit out port packets
        tx_port_ring = rte_ring_create(_TX_RSYNC_TX_PORT_RING_NAME, TX_RSYNC_TX_PORT_RING_SIZE,rte_socket_id(),RING_F_SC_DEQ);
        if (NULL == tx_port_ring)
                rte_exit(EXIT_FAILURE, "Cannot create Tx Port Ring\n");
        //ring used by TX_RSYNC to store packets till 2 Phase commit of TS STAT Update
        tx_tx_state_latch_ring = rte_ring_create(_TX_RSYNC_TX_LATCH_RING_NAME, TX_RSYNC_TX_LATCH_RING_SIZE,rte_socket_id(),RING_F_SP_ENQ|RING_F_SC_DEQ);
        if (NULL == tx_tx_state_latch_ring)
                rte_exit(EXIT_FAILURE, "Cannot create Tx_tx_state_latch_ring Ring\n");
        //ring used by TX_RSYNC to store packets till 2 phase commit of NFs in the chain resulting in non-determinism.
        tx_nf_state_latch_ring = rte_ring_create(_TX_RSYNC_NF_LATCH_RING_NAME, TX_RSYNC_NF_LATCH_RING_SIZE,rte_socket_id(),RING_F_SP_ENQ|RING_F_SC_DEQ);
        if (NULL == tx_nf_state_latch_ring)
                rte_exit(EXIT_FAILURE, "Cannot create tx_nf_state_latch_ring Ring\n");
#endif
        return 0;
}
#endif
/**
 * Set up a mempool to store nf_msg structs
 */
static int
init_nf_msg_pool(void)
{
        /* don't pass single-producer/single-consumer flags to mbuf
         * create as it seems faster to use a cache instead */
        printf("Creating mbuf pool '%s' ...\n", _NF_MSG_POOL_NAME);
        nf_msg_pool = rte_mempool_create(_NF_MSG_POOL_NAME, MAX_NFS * NF_MSG_QUEUE_SIZE,
                        NF_INFO_SIZE, NF_MSG_CACHE_SIZE,
                        0, NULL, NULL, NULL, NULL, rte_socket_id(), NO_FLAGS);

        return (nf_msg_pool == NULL); /* 0 on success */
}
/**
 * Set up a mempool to store nf_info structs
 */
static int
init_client_info_pool(void)
{
        /* don't pass single-producer/single-consumer flags to mbuf
         * create as it seems faster to use a cache instead */
        printf("Creating mbuf pool '%s' ...\n", _NF_MEMPOOL_NAME);

        //setting Cache size parameter seems to have inconsistent behavior; it is better to allocate first with cached; and on failure change to 0 cache size;
        nf_info_pool = rte_mempool_create(_NF_MEMPOOL_NAME, MAX_NFS,
                        NF_INFO_SIZE, NF_INFO_CACHE,
                        0, NULL, NULL, NULL, NULL, rte_socket_id(), NO_FLAGS);

        if(NULL == nf_info_pool) {
                printf("Failed to Create mbuf state pool '%s' with cache size...\n", _NF_MEMPOOL_NAME);
                nf_info_pool = rte_mempool_create(_NF_MEMPOOL_NAME, MAX_NFS,
                                NF_INFO_SIZE, 0,
                                0, NULL, NULL, NULL, NULL, rte_socket_id(), NO_FLAGS);
        }
#ifdef ENABLE_NFV_RESL
        if(init_nf_state_pool()) {
               rte_exit(EXIT_FAILURE, "Cannot create client state mbuf pool: %s\n", rte_strerror(rte_errno));
        }
#ifdef ENABLE_PER_SERVICE_MEMPOOL
        if(init_service_state_pool()) {
               rte_exit(EXIT_FAILURE, "Cannot create service state mbuf pool: %s\n", rte_strerror(rte_errno));
        }
#endif
#ifdef ENABLE_PER_FLOW_TS_STORE
        if(init_per_flow_ts_pool()) {
                rte_exit(EXIT_FAILURE, "Cannot create per_flow_ts mbuf pool: %s\n", rte_strerror(rte_errno));
        }
#endif
#endif  //ENABLE_NFV_RESL

        return (nf_info_pool == NULL); /* 0 on success */
}

/**
 * Initialise an individual port:
 * - configure number of rx and tx rings
 * - set up each rx ring, to pull from the main mbuf pool
 * - set up each tx ring
 * - start the port and report its status to stdout
 */
static int
init_port(uint8_t port_num) {
#if 0
        /* for port configuration all features are off by default */
        const struct rte_eth_conf port_conf = {
                .rxmode = {
                        .mq_mode = ETH_MQ_RX_RSS
                },
                .rx_adv_conf = {
                        .rss_conf = {
                                .rss_key = rss_symmetric_key,
                                .rss_hf = ETH_RSS_IP | ETH_RSS_UDP | ETH_RSS_TCP,
                        }
                },
        };
#endif
        const uint16_t rx_rings = ONVM_NUM_RX_THREADS;
        const uint16_t tx_rings = (ONVM_NUM_TX_THREADS + ONVM_NF_MGR_TX_QUEUES); //MAX_NFS;  //Note Max queuues on FS-2/3/4 = 16; requesting and allocating for more queues doesnt fail; but results in runtime SIGSEGV in ixgbe_transmit()
        uint16_t rx_ring_size = RTE_MP_RX_DESC_DEFAULT;
        uint16_t tx_ring_size = RTE_MP_TX_DESC_DEFAULT;
        struct rte_eth_rxconf rxq_conf;
        struct rte_eth_txconf txq_conf;
        struct rte_eth_dev_info dev_info;
        struct rte_eth_conf local_port_conf = port_conf;
        uint16_t q;
        int retval;

        printf("Port %u init ... \n", (unsigned)port_num);
        printf("Port %u socket id %u ... \n", (unsigned)port_num, (unsigned)rte_eth_dev_socket_id(port_num));
        printf("Port %u Rx rings %u  Tx rings %u ... \n", (unsigned)port_num, (unsigned)rx_rings, (unsigned)tx_rings );
        fflush(stdout);

        /* Standard DPDK port initialisation - config port, then set up
         * rx and tx rings */
        rte_eth_dev_info_get(port_num, &dev_info);
        if (dev_info.tx_offload_capa & DEV_TX_OFFLOAD_MBUF_FAST_FREE)
                local_port_conf.txmode.offloads |= DEV_TX_OFFLOAD_MBUF_FAST_FREE;
        local_port_conf.rx_adv_conf.rss_conf.rss_hf &=
                dev_info.flow_type_rss_offloads;
        if (local_port_conf.rx_adv_conf.rss_conf.rss_hf !=
                        port_conf.rx_adv_conf.rss_conf.rss_hf) {
                printf("Port %u modified RSS hash function based on hardware support,"
                        "requested:%#"PRIx64" configured:%#"PRIx64"\n",
                        port_num,
                        port_conf.rx_adv_conf.rss_conf.rss_hf,
                        local_port_conf.rx_adv_conf.rss_conf.rss_hf);
        }

        if ((retval = rte_eth_dev_configure(port_num, rx_rings, tx_rings,
                &local_port_conf)) != 0)
                return retval;

        /* Adjust rx,tx ring sizes if not allowed by ethernet device 
         * TODO if this is ajusted store the new values for future reference */
        retval = rte_eth_dev_adjust_nb_rx_tx_desc(
                port_num, &rx_ring_size, &tx_ring_size);
        if (retval < 0) {
                rte_panic("Cannot adjust number of descriptors for port %u (%d)\n",
                          port_num, retval);
        }

        rxq_conf = dev_info.default_rxconf;
        rxq_conf.offloads = local_port_conf.rxmode.offloads;
        for (q = 0; q < rx_rings; q++) {
                retval = rte_eth_rx_queue_setup(port_num, q, rx_ring_size,
                                rte_eth_dev_socket_id(port_num),
                                &rxq_conf, pktmbuf_pool);
                if (retval < 0) return retval;
        }

        txq_conf = dev_info.default_txconf;
        txq_conf.offloads = port_conf.txmode.offloads;
        for (q = 0; q < tx_rings; q++) {
                retval = rte_eth_tx_queue_setup(port_num, q, tx_ring_size,
                                rte_eth_dev_socket_id(port_num),
                                &txq_conf);
                if (retval < 0) return retval;
        }

        rte_eth_promiscuous_enable(port_num);

        retval  = rte_eth_dev_start(port_num);
        if (retval < 0) return retval;

        printf("done: \n");

        return 0;
}

/**
 * Set up the DPDK rings which will be used to pass packets, via
 * pointers, between the multi-process server and client processes.
 * Each client needs one RX queue.
 */
static int
init_shm_rings(void) {
        unsigned i;
        unsigned socket_id;
        const char * rq_name;
        const char * tq_name;
        const char * msg_q_name;
        const unsigned ringsize = NF_QUEUE_RINGSIZE;
        const unsigned msgringsize = NF_MSG_QUEUE_SIZE;
        const struct rte_memzone *mz_nf;
        const struct rte_memzone *mz_services;
        const struct rte_memzone *mz_nf_per_service;

        #ifdef INTERRUPT_SEM
        const char * sem_name;
        key_t key;
        int shmid;
        char *shm;

        #ifdef USE_SEMAPHORE
        sem_t *mutex;
        #endif
        #endif
        
#if defined(ENABLE_SHADOW_RINGS)
        const char * rsq_name;
        const char * tsq_name;
        const unsigned sringsize = CLIENT_SHADOW_RING_SIZE;
#endif

        /* set up array for NF Global data 'struct onvm_nf' data */
        mz_nf = rte_memzone_reserve(MZ_NF_INFO, sizeof(*nfs) * MAX_NFS,
                        rte_socket_id(), NO_FLAGS);
        if (mz_nf == NULL)
        rte_exit(EXIT_FAILURE, "Cannot reserve memory zone for nf information\n");
        memset(mz_nf->addr, 0, sizeof(*nfs) * MAX_NFS);
        nfs = mz_nf->addr;

        /* set up array for NF Service Map Table data */
        mz_services = rte_memzone_reserve(MZ_SERVICES_INFO, sizeof(uint16_t) * num_services, rte_socket_id(), NO_FLAGS);
        if (mz_services == NULL)
                rte_exit(EXIT_FAILURE, "Cannot reserve memory zone for services information\n");
        services = mz_services->addr;
        for (i = 0; i < num_services; i++) {
                services[i] = rte_calloc("one service NFs",
                                MAX_NFS_PER_SERVICE, sizeof(uint16_t), 0);
        }

        /* set up array for NF count per Service Table data */
        mz_nf_per_service = rte_memzone_reserve(MZ_NF_PER_SERVICE_INFO, sizeof(uint16_t) * num_services, rte_socket_id(), NO_FLAGS);
        if (mz_nf_per_service == NULL) {
                rte_exit(EXIT_FAILURE, "Cannot reserve memory zone for NF per service information.\n");
        }
        nf_per_service_count = mz_nf_per_service->addr;

        /* Create the NF Resources: Rx, Tx, Message Rings,  shared Memory Pool, etc.. */
        for (i = 0; i < MAX_NFS; i++) {
                /* Create an RX queue for each client */
                socket_id = rte_socket_id();
                rq_name = get_rx_queue_name(i);
                tq_name = get_tx_queue_name(i);
                msg_q_name = get_msg_queue_name(i);
                nfs[i].instance_id = i;

#ifdef ENABLE_NFV_RESL
                if(i < MAX_ACTIVE_CLIENTS) { //if(is_primary_active_nf_id(i)) {
                        nfs[i].rx_q = rte_ring_create(rq_name,
                                        ringsize, socket_id,
                                        RING_F_SC_DEQ);                 /* multi prod, single cons (Enqueue can be by either Rx/Tx Threads, but dequeue only by NF thread)*/
                        nfs[i].tx_q = rte_ring_create(tq_name,
                                        ringsize, socket_id,
                                        RING_F_SP_ENQ|RING_F_SC_DEQ);      /* single prod, single cons (Enqueue only by NF Thread, and dequeue only by dedicated Tx thread) */
                        nfs[i].msg_q = rte_ring_create(msg_q_name,
                                        msgringsize, socket_id,
                                        RING_F_SC_DEQ);                 /* multi prod, single cons */
                        if(rte_mempool_get(nf_state_pool,&nfs[i].nf_state_mempool) < 0) {
                                rte_exit(EXIT_FAILURE, "Failed to get client state memory");;
                        }
#ifdef ENABLE_NFLIB_PER_FLOW_TS_STORE
                        if(rte_mempool_get(per_flow_ts_pool,&nfs[i].per_flow_ts_info) < 0) {
                                rte_exit(EXIT_FAILURE, "Failed to get client per_flow_ts_info memory");;
                        }
#endif

#ifdef ENABLE_SHADOW_RINGS
                        rsq_name = get_rx_squeue_name(i);
                        tsq_name = get_tx_squeue_name(i);
                        nfs[i].rx_sq = rte_ring_create(rsq_name,
                                        sringsize, socket_id,
                                RING_F_SP_ENQ|RING_F_SC_DEQ);                       /* single prod, single cons (Enqueue only by NF Thread, and dequeue only by NF thread)*/
                        nfs[i].tx_sq = rte_ring_create(tsq_name,
                                        sringsize, socket_id,
                                        RING_F_SP_ENQ|RING_F_SC_DEQ);               /* single prod, single cons (Enqueue only by NF Thread, and dequeue only by dedicated Tx thread) */
#endif
                } else {
                        nfs[i].rx_q = nfs[get_associated_active_or_standby_nf_id(i)].rx_q;
                        nfs[i].tx_q = nfs[get_associated_active_or_standby_nf_id(i)].tx_q;
                        nfs[i].nf_state_mempool = nfs[get_associated_active_or_standby_nf_id(i)].nf_state_mempool;
#ifdef ENABLE_NFLIB_PER_FLOW_TS_STORE
                        nfs[i].per_flow_ts_info =  nfs[get_associated_active_or_standby_nf_id(i)].per_flow_ts_info;
#endif
                        fprintf(stderr, "re-using rx and tx queue rings for client %d with %d\n", i, get_associated_active_or_standby_nf_id(i));
#ifdef ENABLE_SHADOW_RINGS
                        nfs[i].rx_sq = nfs[get_associated_active_or_standby_nf_id(i)].rx_sq;
                        nfs[i].tx_sq = nfs[get_associated_active_or_standby_nf_id(i)].tx_sq;
#endif
                        nfs[i].msg_q = rte_ring_create(msg_q_name,
                                        msgringsize, socket_id,
                                        RING_F_SC_DEQ);                             /* multi prod, single cons */
                }
#else
                nfs[i].rx_q = rte_ring_create(rq_name,
                                ringsize, socket_id,
                                //RING_F_SP_ENQ|RING_F_SC_DEQ);     /* single prod, single cons */
                                RING_F_SC_DEQ);                 /* multi prod, single cons (Enqueue can be by either Rx/Tx Threads, but dequeue only by NF thread)*/
                nfs[i].tx_q = rte_ring_create(tq_name,
                                ringsize, socket_id,
                                RING_F_SP_ENQ|RING_F_SC_DEQ);      /* single prod, single cons (Enqueue only by NF Thread, and dequeue only by dedicated Tx thread) */
                                //RING_F_SC_DEQ);                 /* multi prod, single cons */
                                //but it should be RING_F_SP_ENQ
                nfs[i].msg_q = rte_ring_create(msg_q_name,
                                msgringsize, socket_id,
                                RING_F_SC_DEQ);                 /* multi prod, single cons */

#endif
                if (nfs[i].rx_q == NULL)
                        rte_exit(EXIT_FAILURE, "Cannot create rx ring queue for client %u\n", i);

                if (nfs[i].tx_q == NULL)
                        rte_exit(EXIT_FAILURE, "Cannot create tx ring queue for client %u\n", i);


#ifdef ENABLE_RING_WATERMARK
                //functionality is deprectated from DPDK 18.02 onwards.. Logic on packet_enqueue is updated to handle the water-mark scenario by making ring_count/check with available_size in the ring.
                //rte_ring_set_water_mark(nfs[i].rx_q, CLIENT_QUEUE_RING_WATER_MARK_SIZE);

                //rte_ring_set_water_mark(nfs[i].tx_q, CLIENT_QUEUE_RING_WATER_MARK_SIZE);
#endif

#ifdef INTERRUPT_SEM
                sem_name = get_sem_name(i);
                nfs[i].sem_name = sem_name;
                //fprintf(stderr, "sem_name=%s for client %d\n", sem_name, i);

#ifdef USE_SEMAPHORE
                mutex = sem_open(sem_name, O_CREAT, 06666, 0);
                if(mutex == SEM_FAILED) {
                        fprintf(stderr, "can not create semaphore for client %d\n", i);
                        sem_unlink(sem_name);
                        exit(1);
                }
                nfs[i].mutex = mutex;
#endif

                key = get_rx_shmkey(i);       
                if ((shmid = shmget(key, SHMSZ, IPC_CREAT | 0666)) < 0) {
                        fprintf(stderr, "can not create the shared memory segment for client %d\n", i);
                        exit(1);
                }
                
                if ((shm = shmat(shmid, NULL, 0)) == (char *) -1) {
                        fprintf(stderr, "can not attach the shared segment to the server space for client %d\n", i);
                               exit(1);
                    }

                nfs[i].shm_server = (rte_atomic16_t *)shm;
#ifdef USE_POLL_MODE
                rte_atomic16_set(nfs[i].shm_server, 0);
#else
                rte_atomic16_set(nfs[i].shm_server, 0);//rte_atomic16_set(nfs[i].shm_server, 1);
#endif
#endif


#ifdef NF_BACKPRESSURE_APPROACH_1
                memset(&nfs[i].bft_list, 0, sizeof(nfs[i].bft_list));
                nfs[i].bft_list.max_len=NF_QUEUE_RINGSIZE*2;
#endif
        }
        /* This is a separate service state pool maintained in NF MGR for each of the services: NFs need to attach to it on the run time after the NFs service type is identified */
#if defined (ENABLE_PER_SERVICE_MEMPOOL)
        services_state_pool = rte_calloc("services_state_pool", num_services, sizeof(struct rte_mempool*), 0);
        if (services_state_pool == NULL)
                rte_exit(EXIT_FAILURE, "Cannot allocate memory for services_state_pool details\n");
        for(i=0; i< num_services; i++) {
                if(rte_mempool_get(service_state_pool,&services_state_pool[i]) < 0) {
                        rte_exit(EXIT_FAILURE, "Failed to get service state memory from service_state_pool");;
                }
        }
#endif

#ifdef ENABLE_PER_FLOW_TS_STORE
        if(rte_mempool_get(per_flow_ts_pool,&onvm_mgr_tx_per_flow_ts_info) < 0) {
                rte_exit(EXIT_FAILURE, "Failed to get onvm_mgr per_flow_ts_info memory");
        }
#ifdef ENABLE_RSYNC_WITH_DOUBLE_BUFFERING_MODE
#ifdef ENABLE_RSYNC_MULTI_BUFFERING
        for(i=0; i< ENABLE_RSYNC_MULTI_BUFFERING; ++i) {
                if(rte_mempool_get(per_flow_ts_pool, &onvm_mgr_tx_per_flow_ts_info_db[i]) < 0) {
                        rte_exit(EXIT_FAILURE, "Failed to get onvm_mgr_per_flow_Ts_info_db memory for index:%d",i);
                }
        }
#else
        if(rte_mempool_get(per_flow_ts_pool,&onvm_mgr_tx_per_flow_ts_info_db) < 0) {
                rte_exit(EXIT_FAILURE, "Failed to get onvm_mgr per_flow_ts_info_db memory");
        }
#endif
#endif
#endif
        return 0;
}

/**
 * Allocate a rte_ring for newly created NFs
 */
static int
init_mgr_queues(void)
{
        mgr_msg_queue = rte_ring_create(
                _MGR_MSG_QUEUE_NAME,
                MAX_NFS,
                rte_socket_id(),
                RING_F_SC_DEQ); // MP enqueue (default), SC dequeue

        if (mgr_msg_queue == NULL)
                rte_exit(EXIT_FAILURE, "Cannot create MGR Message Ring\n");

#ifdef ENABLE_SYNC_MGR_TO_NF_MSG
        mgr_rsp_queue = rte_ring_create(
                        _MGR_RSP_QUEUE_NAME,
                        MAX_NFS,
                        rte_socket_id(),
                        RING_F_SC_DEQ); // MP enqueue (default), SC dequeue

        if (mgr_rsp_queue == NULL)
                rte_exit(EXIT_FAILURE, "Cannot create MGR Response Ring\n");
#endif

#ifdef ENABLE_REMOTE_SYNC_WITH_TX_LATCH
        return init_rsync_tx_rings();
#endif
        return 0;
}

/* Check the link status of all ports in up to 9s, and print them finally */
static void
check_all_ports_link_status(uint8_t port_num, uint32_t port_mask) {
#define CHECK_INTERVAL 100 /* 100ms */
#define MAX_CHECK_TIME 90 /* 9s (90 * 100ms) in total */
        uint8_t portid, count, all_ports_up, print_flag = 0;
        struct rte_eth_link link;

        printf("\nChecking link status");
        fflush(stdout);
        for (count = 0; count <= MAX_CHECK_TIME; count++) {
                all_ports_up = 1;
                for (portid = 0; portid < port_num; portid++) {
                        if ((port_mask & (1 << ports->id[portid])) == 0)
                                continue;
                        memset(&link, 0, sizeof(link));
                        rte_eth_link_get_nowait(ports->id[portid], &link);
                        /* print link status if flag set */
                        if (print_flag == 1) {
                                if (link.link_status) {
                                        printf("Port %d Link Up - speed %u "
                                                "Mbps - %s\n", ports->id[portid],
                                                (unsigned)link.link_speed,
                                (link.link_duplex == ETH_LINK_FULL_DUPLEX) ?
                                        ("full-duplex") : ("half-duplex\n"));
                                        ports->down_status[portid]=0;
                                }
                                else {
                                        printf("Port %d Link Down\n",
                                                (uint8_t)ports->id[portid]);
                                        ports->down_status[portid]=1;
                                }
                                continue;
                        }
                        /* clear all_ports_up flag if any link down */
                        if (link.link_status == 0) {
                                all_ports_up = 0;
                                break;
                        }
                }
                /* after finally printing all link status, get out */
                if (print_flag == 1)
                        break;

                if (all_ports_up == 0) {
                        printf(".");
                        fflush(stdout);
                        rte_delay_ms(CHECK_INTERVAL);
                }

                /* set the print_flag if all ports up or timeout */
                if (all_ports_up == 1 || count == (MAX_CHECK_TIME - 1)) {
                        print_flag = 1;
                        printf("done\n");
                }
        }
}

/**
 * Main init function for the multi-process server app,
 * calls subfunctions to do each stage of the initialisation.
 */


#ifdef ENABLE_NF_MGR_IDENTIFIER
static uint32_t read_onvm_mgr_id_from_system(void) {
        nf_mgr_id = gethostid();
        return nf_mgr_id;
}
#endif // ENABLE_NF_MGR_IDENTIFIER
